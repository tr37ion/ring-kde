/***************************************************************************
 *   Copyright (C) 2017 by Bluesystems                                     *
 *   Author : Emmanuel Lepage Vallee <elv1313@gmail.com>                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/
#include "treeview2.h"

#include <QtCore/QTimer>

#include <functional>

#define V_ITEM(i) static_cast<VisualTreeItem*>(i)

// Use some constant for readability
#define PREVIOUS 0
#define NEXT 1
#define FIRST 0
#define LAST 1

/*
 * Design:
 *
 * This class implement 3 embedded state machines.
 *
 * The first machine has a single instance and manage the whole view. It allows
 * different behavior (corner case) to be handled in a stateful way without "if".
 * It tracks a moving window of elements coarsely equivalent to the number of
 * elements on screen.
 *
 * The second layer reflects the model and corresponds to a QModelIndex currently
 * being tracked by the system. They are lazy-loaded and mostly unaware of the
 * model topology. While they are hierarchical, they are so compared to themselves,
 * not the actual model hierarchy. This allows the model to have unlimited depth
 * without any performance impact.
 *
 * The last layer of state machine represents the visual elements. Being separated
 * from the model view allows the QQuickItem elements to fail to load without
 * spreading havoc. This layer also implements an abstraction that allows the
 * tree to be browsed as a list. This greatly simplifies the widget code. The
 * abstract class has to be implemented by the view implementations. All the
 * heavy lifting is done in this class and the implementation can assume all is
 * valid and perform minimal validation.
 *
 */

class VisualTreeItemPrivate
{
public:

};

/*
 * The visual elements state changes.
 *
 * Note that the ::FAILED elements will always try to self-heal themselves and
 * go back into FAILED once the self-healing itself failed.
 */
#define S VisualTreeItem::State::
const VisualTreeItem::State VisualTreeItem::m_fStateMap[7][7] = {
/*              ATTACH ENTER_BUFFER ENTER_VIEW UPDATE    MOVE   LEAVE_BUFFER  DETACH  */
/*POOLING */ { S POOLING, S BUFFER, S ERROR , S ERROR , S ERROR , S ERROR  , S DANGLING },
/*POOLED  */ { S POOLED , S BUFFER, S ERROR , S ERROR , S ERROR , S ERROR  , S DANGLING },
/*BUFFER  */ { S ERROR  , S ERROR , S ACTIVE, S BUFFER, S ERROR , S POOLING, S DANGLING },
/*ACTIVE  */ { S ERROR  , S BUFFER, S ERROR , S ACTIVE, S ACTIVE, S POOLING, S POOLING  },
/*FAILED  */ { S ERROR  , S BUFFER, S ACTIVE, S ACTIVE, S ACTIVE, S POOLING, S DANGLING },
/*DANGLING*/ { S ERROR  , S ERROR , S ERROR , S ERROR , S ERROR , S ERROR  , S DANGLING },
/*ERROR   */ { S ERROR  , S ERROR , S ERROR , S ERROR , S ERROR , S ERROR  , S DANGLING },
};
#undef S

#define A &VisualTreeItem::
const VisualTreeItem::StateF VisualTreeItem::m_fStateMachine[7][7] = {
/*             ATTACH  ENTER_BUFFER  ENTER_VIEW   UPDATE     MOVE   LEAVE_BUFFER  DETACH  */
/*POOLING */ { A error  , A error  , A error  , A error  , A error  , A error  , A error   },
/*POOLED  */ { A nothing, A attach , A error  , A error  , A error  , A error  , A destroy },
/*BUFFER  */ { A error  , A error  , A move   , A refresh, A error  , A detach , A destroy },
/*ACTIVE  */ { A error  , A nothing, A error  , A refresh, A move   , A detach , A detach  },
/*FAILED  */ { A error  , A attach , A attach , A attach , A attach , A detach , A destroy },
/*DANGLING*/ { A error  , A error  , A error  , A error  , A error  , A error  , A destroy },
/*error   */ { A error  , A error  , A error  , A error  , A error  , A error  , A destroy },
};
#undef A

/**
 * Hold the QPersistentModelIndex and the metadata associated with them.
 */
struct TreeTraversalItems
{
    explicit TreeTraversalItems(TreeTraversalItems* parent, TreeView2Private* d):
        m_pParent(parent), d_ptr(d) {}

    enum class State {
        BUFFER    = 0, /*!< Not in use by any visible indexes, but pre-loaded */
        REMOVED   = 1, /*!< Currently in a removal transaction                */
        REACHABLE = 2, /*!< The [grand]parent of visible indexes              */
        VISIBLE   = 3, /*!< The element is visible on screen                  */
        ERROR     = 4, /*!< Something went wrong                              */
        DANGLING  = 5, /*!< Being destroyed                                   */
    };

    enum class Action {
        SHOW   = 0, /*!< Make visible on screen (or buffer) */
        HIDE   = 1, /*!< Remove from the screen (or buffer) */
        ATTACH = 2, /*!< Track, but do not show             */
        DETACH = 3, /*!< Stop tracking for changes          */
        UPDATE = 4, /*!< Update the element                 */
        MOVE   = 5, /*!< Update the depth and lookup        */
    };

    typedef bool(TreeTraversalItems::*StateF)();

    static const State  m_fStateMap    [6][6];
    static const StateF m_fStateMachine[6][6];

    bool performAction(Action);

    bool nothing();
    bool error  ();
    bool show   ();
    bool hide   ();
    bool attach ();
    bool detach ();
    bool refresh();
    bool index  ();
    bool destroy();

    //TODO use a btree, not an hash
    QHash<QPersistentModelIndex, TreeTraversalItems*> m_hLookup;

    uint m_Depth {0};
    State m_State {State::BUFFER};

    // Keep the parent to be able to get back to the root
    TreeTraversalItems* m_pParent;

    TreeTraversalItems* m_tSiblings[2] = {nullptr, nullptr};
    TreeTraversalItems* m_tChildren[2] = {nullptr, nullptr};

    QPersistentModelIndex m_Index;
    VisualTreeItem* m_pTreeItem {nullptr};

    TreeView2Private* d_ptr;
};

#define S TreeTraversalItems::State::
const TreeTraversalItems::State TreeTraversalItems::m_fStateMap[6][6] = {
/*                 SHOW         HIDE        ATTACH     DETACH      UPDATE       MOVE     */
/*BUFFER   */ { S VISIBLE, S BUFFER   , S REACHABLE, S DANGLING , S BUFFER , S BUFFER    },
/*REMOVED  */ { S ERROR  , S ERROR    , S ERROR    , S BUFFER   , S ERROR  , S ERROR     },
/*REACHABLE*/ { S VISIBLE, S REACHABLE, S ERROR    , S BUFFER   , S ERROR  , S REACHABLE },
/*VISIBLE  */ { S VISIBLE, S REACHABLE, S ERROR    , S BUFFER   , S VISIBLE, S VISIBLE   },
/*ERROR    */ { S ERROR  , S ERROR    , S ERROR    , S ERROR    , S ERROR  , S ERROR     },
/*DANGLING */ { S ERROR  , S ERROR    , S ERROR    , S ERROR    , S ERROR  , S ERROR     },
};
#undef S

#define A &TreeTraversalItems::
const TreeTraversalItems::StateF TreeTraversalItems::m_fStateMachine[6][6] = {
/*                 SHOW       HIDE      ATTACH    DETACH      UPDATE    MOVE   */
/*BUFFER   */ { A show   , A nothing, A attach, A destroy , A refresh, A index },
/*REMOVED  */ { A error  , A error  , A error , A detach  , A error  , A error },
/*REACHABLE*/ { A show   , A nothing, A error , A detach  , A error  , A index },
/*VISIBLE  */ { A nothing, A hide   , A error , A detach  , A refresh, A index },
/*ERROR    */ { A error  , A error  , A error , A error   , A error  , A error },
/*DANGLING */ { A error  , A error  , A error , A error   , A error  , A error },
};
#undef A

class TreeView2Private : public QObject
{
    Q_OBJECT
public:

    enum class State {
        UNFILLED, /*!< There is less items that the space available          */
        ANCHORED, /*!< Some items are out of view, but it's at the beginning */
        SCROLLED, /*!< It's scrolled to a random point                       */
        AT_END  , /*!< It's at the end of the items                          */
        ERROR   , /*!< Something went wrong                                  */
    };

    enum class Action {
        INSERTION    = 0,
        REMOVAL      = 1,
        MOVE         = 2,
        RESET_SCROLL = 3,
        SCROLL       = 4,
    };

    typedef bool(TreeView2Private::*StateF)();

    static const State  m_fStateMap    [5][5];
    static const StateF m_fStateMachine[5][5];

    bool m_UniformRowHeight   {false};
    bool m_UniformColumnWidth {false};
    bool m_Collapsable        {true };
    bool m_AutoExpand         {false};
    int  m_MaxDepth           { -1  };
    int  m_CacheBuffer        { 10  };
    int  m_PoolSize           { 10  };
    int  m_FailedCount        {  0  };
    TreeView2::RecyclingMode m_RecyclingMode {
        TreeView2::RecyclingMode::NoRecycling
    };
    State m_State {State::UNFILLED};

    TreeTraversalItems* m_pRoot {new TreeTraversalItems(nullptr, this)};

    /// All elements with loaded children
    QHash<QPersistentModelIndex, TreeTraversalItems*> m_hMapper;

    // Helpers
    bool isActive(const QModelIndex& parent, int first, int last);
    void initTree(const QModelIndex& parent);
    TreeTraversalItems* addChildren(TreeTraversalItems* parent, const QModelIndex& index);
    void bridgeGap(TreeTraversalItems* first, TreeTraversalItems* second, bool insert = false);
    void bridgeGap(VisualTreeItem* first, VisualTreeItem* second, bool insert = false);
    void createGap(VisualTreeItem* first, VisualTreeItem* last  );

    void setTemporaryIndices(const QModelIndex &parent, int start, int end,
                             const QModelIndex &destination, int row);
    void resetTemporaryIndices(const QModelIndex &parent, int start, int end,
                               const QModelIndex &destination, int row);

    // Tests
    void _test_validateTree(TreeTraversalItems* p);

    TreeView2* q_ptr;

private:
    bool nothing     ();
    bool resetScoll  ();
    bool refresh     ();
    bool refreshFront();
    bool refreshBack ();
    bool error       () __attribute__ ((noreturn));

public Q_SLOTS:
    void cleanup();
    void slotRowsInserted  (const QModelIndex& parent, int first, int last);
    void slotRowsRemoved   (const QModelIndex& parent, int first, int last);
    void slotLayoutChanged (                                              );
    void slotDataChanged   (const QModelIndex& tl, const QModelIndex& br  );
    void slotRowsMoved     (const QModelIndex &p, int start, int end,
                            const QModelIndex &dest, int row);

    void slotRowsMoved2    (const QModelIndex &p, int start, int end,
                            const QModelIndex &dest, int row);
};

#define S TreeView2Private::State::
const TreeView2Private::State TreeView2Private::m_fStateMap[5][5] = {
/*              INSERTION    REMOVAL       MOVE     RESET_SCROLL    SCROLL  */
/*UNFILLED */ { S ANCHORED, S UNFILLED , S UNFILLED , S UNFILLED, S UNFILLED },
/*ANCHORED */ { S ANCHORED, S ANCHORED , S ANCHORED , S ANCHORED, S SCROLLED },
/*SCROLLED */ { S SCROLLED, S SCROLLED , S SCROLLED , S ANCHORED, S SCROLLED },
/*AT_END   */ { S AT_END  , S AT_END   , S AT_END   , S ANCHORED, S SCROLLED },
/*ERROR    */ { S ERROR   , S ERROR    , S ERROR    , S ERROR   , S ERROR    },
};
#undef S

#define A &TreeView2Private::
const TreeView2Private::StateF TreeView2Private::m_fStateMachine[5][5] = {
/*              INSERTION           REMOVAL          MOVE        RESET_SCROLL     SCROLL  */
/*UNFILLED*/ { A refreshFront, A refreshFront , A refreshFront , A nothing   , A nothing  },
/*ANCHORED*/ { A refreshFront, A refreshFront , A refreshFront , A nothing   , A refresh  },
/*SCROLLED*/ { A refresh     , A refresh      , A refresh      , A resetScoll, A refresh  },
/*AT_END  */ { A refreshBack , A refreshBack  , A refreshBack  , A resetScoll, A refresh  },
/*ERROR   */ { A error       , A error        , A error        , A error     , A error    },
};
#undef A

TreeView2::TreeView2(QQuickItem* parent) : FlickableView(parent),
    d_ptr(new TreeView2Private())
{
    d_ptr->q_ptr = this;
}

TreeView2::~TreeView2()
{
    delete d_ptr->m_pRoot;
    delete d_ptr;
}

void TreeView2::setModel(QSharedPointer<QAbstractItemModel> m)
{
    if (m == model())
        return;

    if (auto oldM = model()) {
        disconnect(oldM.data(), &QAbstractItemModel::rowsInserted, d_ptr,
            &TreeView2Private::slotRowsInserted);
        disconnect(oldM.data(), &QAbstractItemModel::rowsAboutToBeRemoved, d_ptr,
            &TreeView2Private::slotRowsRemoved);
        disconnect(oldM.data(), &QAbstractItemModel::layoutAboutToBeChanged, d_ptr,
            &TreeView2Private::cleanup);
        disconnect(oldM.data(), &QAbstractItemModel::layoutChanged, d_ptr,
            &TreeView2Private::slotLayoutChanged);
        disconnect(oldM.data(), &QAbstractItemModel::modelAboutToBeReset, d_ptr,
            &TreeView2Private::cleanup);
        disconnect(oldM.data(), &QAbstractItemModel::modelReset, d_ptr,
            &TreeView2Private::slotLayoutChanged);
        disconnect(oldM.data(), &QAbstractItemModel::rowsAboutToBeMoved, d_ptr,
            &TreeView2Private::slotRowsMoved);
        disconnect(oldM.data(), &QAbstractItemModel::rowsMoved, d_ptr,
            &TreeView2Private::slotRowsMoved2);
        disconnect(oldM.data(), &QAbstractItemModel::dataChanged, d_ptr,
            &TreeView2Private::slotDataChanged);

        d_ptr->slotRowsRemoved({}, 0, oldM->rowCount()-1);
    }


    d_ptr->m_hMapper.clear();
    delete d_ptr->m_pRoot;
    d_ptr->m_pRoot = new TreeTraversalItems(nullptr, d_ptr);

    FlickableView::setModel(m);

    if (!m)
        return;

    connect(model().data(), &QAbstractItemModel::rowsInserted, d_ptr,
        &TreeView2Private::slotRowsInserted );
    connect(model().data(), &QAbstractItemModel::rowsAboutToBeRemoved, d_ptr,
        &TreeView2Private::slotRowsRemoved  );
    connect(model().data(), &QAbstractItemModel::layoutAboutToBeChanged, d_ptr,
        &TreeView2Private::cleanup);
    connect(model().data(), &QAbstractItemModel::layoutChanged, d_ptr,
        &TreeView2Private::slotLayoutChanged);
    connect(model().data(), &QAbstractItemModel::modelAboutToBeReset, d_ptr,
        &TreeView2Private::cleanup);
    connect(model().data(), &QAbstractItemModel::modelReset, d_ptr,
        &TreeView2Private::slotLayoutChanged);
    connect(model().data(), &QAbstractItemModel::rowsAboutToBeMoved, d_ptr,
        &TreeView2Private::slotRowsMoved);
    connect(model().data(), &QAbstractItemModel::rowsMoved, d_ptr,
        &TreeView2Private::slotRowsMoved2);
    connect(model().data(), &QAbstractItemModel::dataChanged, d_ptr,
        &TreeView2Private::slotDataChanged  );

    if (auto rc = m->rowCount())
        d_ptr->slotRowsInserted({}, 0, rc - 1);
}

bool TreeView2::hasUniformRowHeight() const
{
    return d_ptr->m_UniformRowHeight;
}

void TreeView2::setUniformRowHeight(bool value)
{
    d_ptr->m_UniformRowHeight = value;
}

bool TreeView2::hasUniformColumnWidth() const
{
    return d_ptr->m_UniformColumnWidth;
}

void TreeView2::setUniformColumnColumnWidth(bool value)
{
    d_ptr->m_UniformColumnWidth = value;
}

bool TreeView2::isCollapsable() const
{
    return d_ptr->m_Collapsable;
}

void TreeView2::setCollapsable(bool value)
{
    d_ptr->m_Collapsable = value;
}

bool TreeView2::isAutoExpand() const
{
    return d_ptr->m_AutoExpand;
}

void TreeView2::setAutoExpand(bool value)
{
    d_ptr->m_AutoExpand = value;
}

int TreeView2::maxDepth() const
{
    return d_ptr->m_MaxDepth;
}

void TreeView2::setMaxDepth(int depth)
{
    d_ptr->m_MaxDepth = depth;
}

int TreeView2::cacheBuffer() const
{
    return d_ptr->m_CacheBuffer;
}

void TreeView2::setCacheBuffer(int value)
{
    d_ptr->m_CacheBuffer = value;
}

int TreeView2::poolSize() const
{
    return d_ptr->m_PoolSize;
}

void TreeView2::setPoolSize(int value)
{
    d_ptr->m_PoolSize = value;
}

TreeView2::RecyclingMode TreeView2::recyclingMode() const
{
    return d_ptr->m_RecyclingMode;
}

void TreeView2::setRecyclingMode(TreeView2::RecyclingMode mode)
{
    d_ptr->m_RecyclingMode = mode;
}

void TreeView2::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    FlickableView::geometryChanged(newGeometry, oldGeometry);
    d_ptr->m_pRoot->performAction(TreeTraversalItems::Action::MOVE);
    contentItem()->setWidth(newGeometry.width());
}

void TreeView2::reloadChildren(const QModelIndex& index) const
{
    if (auto i = static_cast<VisualTreeItem*>(itemForIndex(index))) {
        const auto p = i->m_pTTI;

        if (!p)
            return;

        auto c = p->m_tChildren[FIRST];

        while (c && c != p->m_tChildren[LAST]) {
            if (c->m_pTreeItem) {
                c->m_pTreeItem->performAction( VisualTreeItem::Action::UPDATE );
                c->m_pTreeItem->performAction( VisualTreeItem::Action::MOVE   );
            }
            c = c->m_tSiblings[NEXT];
        }
    }
}

QQuickItem* TreeView2::parentTreeItem(const QModelIndex& index) const
{
    if (auto i = static_cast<VisualTreeItem*>(itemForIndex(index))) {
        if (i->m_pTTI && i->m_pTTI->m_pParent && i->m_pTTI->m_pParent->m_pTreeItem)
            return i->m_pTTI->m_pParent->m_pTreeItem->item();
    }

    return nullptr;
}

/// Return true if the indices affect the current view
bool TreeView2Private::isActive(const QModelIndex& parent, int first, int last)
{
    return true; //FIXME

    if (m_State == State::UNFILLED)
        return true;

    //FIXME only insert elements with loaded children into m_hMapper
    auto pitem = parent.isValid() ? m_hMapper.value(parent) : m_pRoot;

    if (parent.isValid() && pitem == m_pRoot)
        return false;

    if ((!pitem->m_tChildren[LAST]) || (!pitem->m_tChildren[FIRST]))
        return true;

    if (pitem->m_tChildren[LAST]->m_Index.row() >= first)
        return true;

    if (pitem->m_tChildren[FIRST]->m_Index.row() <= last)
        return true;

    return false;
}

/// Add new entries to the mapping
TreeTraversalItems* TreeView2Private::addChildren(TreeTraversalItems* parent, const QModelIndex& index)
{
    Q_ASSERT(index.isValid());
    Q_ASSERT(index.parent() != index);

    auto e = new TreeTraversalItems(parent, this);
    e->m_Index = index;

    const int oldSize(m_hMapper.size()), oldSize2(parent->m_hLookup.size());

    m_hMapper        [index] = e;
    parent->m_hLookup[index] = e;

    // If the size did not grow, something leaked
    Q_ASSERT(m_hMapper.size() == oldSize+1);
    Q_ASSERT(parent->m_hLookup.size() == oldSize2+1);

    return e;
}

/// Make sure all elements exists all the way to the root
void TreeView2Private::initTree(const QModelIndex& parent)
{
    Q_UNUSED(parent);
    //
}

void TreeView2Private::cleanup()
{
    m_pRoot->performAction(TreeTraversalItems::Action::DETACH);

    m_hMapper.clear();
    m_pRoot = new TreeTraversalItems(nullptr, this);
    m_FailedCount = 0;
}

FlickableView::ModelIndexItem* TreeView2::itemForIndex(const QModelIndex& idx) const
{
    if (!idx.isValid())
        return nullptr;

    if (!idx.parent().isValid()) {
        const auto tti = d_ptr->m_pRoot->m_hLookup.value(idx);
        return tti ? tti->m_pTreeItem : nullptr;
    }

    if (auto parent = d_ptr->m_hMapper.value(idx.parent())) {
        const auto tti = parent->m_hLookup.value(idx);
        return tti ? tti->m_pTreeItem : nullptr;
    }

    return nullptr;
}

void TreeView2::reload()
{
    if (d_ptr->m_hMapper.isEmpty())
        return;


}

void TreeView2Private::_test_validateTree(TreeTraversalItems* p)
{
#ifdef QT_NO_DEBUG_OUTPUT
    return;
#endif

    // The asserts below only work on valid models with valid delegates.
    // If those conditions are not met, it *could* work anyway, but cannot be
    // validated.
    Q_ASSERT(m_FailedCount >= 0);
    if (m_FailedCount) {
        qWarning() << "The tree is fragmented and failed to self heal: disable validation";
        return;
    }

    if (p->m_pParent == m_pRoot && m_pRoot->m_tChildren[FIRST] == p) {
        Q_ASSERT(!p->m_pTreeItem->up());
    }

    // First, let's check the linked list to avoid running more test on really
    // corrupted data
    if (auto i = p->m_tChildren[FIRST]) {
        auto idx = i->m_Index;
        int count = 1;
        auto oldI = i;

        while ((oldI = i) && (i = i->m_tSiblings[NEXT])) {
            // If this is a next, then there has to be a previous
            Q_ASSERT(i->m_pParent == p);
            Q_ASSERT(i->m_tSiblings[PREVIOUS]);
            Q_ASSERT(i->m_tSiblings[PREVIOUS]->m_Index == idx);
            //Q_ASSERT(i->m_Index.row() == idx.row()+1); //FIXME
            Q_ASSERT(i->m_tSiblings[PREVIOUS]->m_tSiblings[NEXT] == i);
            Q_ASSERT(i->m_tSiblings[PREVIOUS] == oldI);
            idx = i->m_Index;
            count++;
        }

        Q_ASSERT(p == p->m_tChildren[FIRST]->m_pParent);
        Q_ASSERT(p == p->m_tChildren[LAST]->m_pParent);
        Q_ASSERT(p->m_hLookup.size() == count);
    }

    // Do that again in the other direction
    if (auto i = p->m_tChildren[LAST]) {
        auto idx = i->m_Index;
        auto oldI = i;
        int count = 1;

        while ((oldI = i) && (i = i->m_tSiblings[PREVIOUS])) {
            Q_ASSERT(i->m_tSiblings[NEXT]);
            Q_ASSERT(i->m_tSiblings[NEXT]->m_Index == idx);
            Q_ASSERT(i->m_pParent == p);
            //Q_ASSERT(i->m_Index.row() == idx.row()-1); //FIXME
            Q_ASSERT(i->m_tSiblings[NEXT]->m_tSiblings[PREVIOUS] == i);
            Q_ASSERT(i->m_tSiblings[NEXT] == oldI);
            idx = i->m_Index;
            count++;
        }

        Q_ASSERT(p->m_hLookup.size() == count);
    }

    //TODO remove once stable
    // Brute force recursive validations
    TreeTraversalItems *old(nullptr), *newest(nullptr);
    for (auto i = p->m_hLookup.constBegin(); i != p->m_hLookup.constEnd(); i++) {
        if ((!newest) || i.key().row() < newest->m_Index.row())
            newest = i.value();

        if ((!old) || i.key().row() > old->m_Index.row())
            old = i.value();

        // Check that m_FailedCount is valid
        Q_ASSERT(i.value()->m_pTreeItem->m_State != VisualTreeItem::State::FAILED);

        // Test the indices
        Q_ASSERT(p == m_pRoot || i.key().internalPointer() == i.value()->m_Index.internalPointer());
        Q_ASSERT(p == m_pRoot || (p->m_Index.isValid()) || p->m_Index.internalPointer() != i.key().internalPointer());
        //Q_ASSERT(old == i.value() || old->m_Index.row() > i.key().row()); //FIXME
        //Q_ASSERT(newest == i.value() || newest->m_Index.row() < i.key().row()); //FIXME

        // Test that there is no trivial duplicate TreeTraversalItems for the same index
        if(i.value()->m_tSiblings[PREVIOUS] && i.value()->m_tSiblings[PREVIOUS]->m_hLookup.isEmpty()) {
            const auto prev = V_ITEM(i.value()->m_pTreeItem->up());
            Q_ASSERT(prev->m_pTTI == i.value()->m_tSiblings[PREVIOUS]);

            const auto next = V_ITEM(prev->down());
            Q_ASSERT(next->m_pTTI == i.value());
        }

        // Test the virtual linked list between the leafs and branches
        if(auto next = V_ITEM(i.value()->m_pTreeItem->down())) {
            Q_ASSERT(next->up() == i.value()->m_pTreeItem);
            Q_ASSERT(next->m_pTTI != i.value());
        }
        else {
            // There is always a next is those conditions are not met unless there
            // is failed elements creating (auto-corrected) holes in the chains.
            Q_ASSERT(!i.value()->m_tSiblings[NEXT]);
            Q_ASSERT(i.value()->m_hLookup.isEmpty());
        }

        if(auto prev = V_ITEM(i.value()->m_pTreeItem->up())) {
            Q_ASSERT(V_ITEM(prev->down()) == i.value()->m_pTreeItem);
            Q_ASSERT(prev->m_pTTI != i.value());
        }
        else {
            // There is always a previous if those conditions are not met unless there
            // is failed elements creating (auto-corrected) holes in the chains.
            Q_ASSERT(!i.value()->m_tSiblings[PREVIOUS]);
            Q_ASSERT(!i.value()->m_pParent->m_pTreeItem);
        }

        _test_validateTree(i.value());
    }

    // Traverse as a list
    if (p == m_pRoot) {
        VisualTreeItem* oldVI(nullptr);

        int count(0), count2(0);
        for (auto i = m_pRoot->m_tChildren[FIRST]->m_pTreeItem; i; i = V_ITEM(i->down())) {
            Q_ASSERT((!oldVI) || i->up());
            Q_ASSERT(i->up() == oldVI);
            oldVI = i;
            count++;
        }

        // Backward too
        oldVI = nullptr;
        auto last = m_pRoot->m_tChildren[LAST];
        while (last && last->m_tChildren[LAST])
            last = last->m_tChildren[LAST];

        for (auto i = last->m_pTreeItem; i; i = V_ITEM(i->up())) {
            Q_ASSERT((!oldVI) || i->down());
            Q_ASSERT(i->down() == oldVI);
            oldVI = i;
            count2++;
        }

        Q_ASSERT(count == count2);
    }

    // Test that the list edges are valid
    Q_ASSERT(!(!!p->m_tChildren[LAST] ^ !!p->m_tChildren[FIRST]));
    Q_ASSERT(p->m_tChildren[LAST]  == old);
    Q_ASSERT(p->m_tChildren[FIRST] == newest);
    Q_ASSERT((!old) || !old->m_tSiblings[NEXT]);
    Q_ASSERT((!newest) || !newest->m_tSiblings[PREVIOUS]);
}

void TreeView2Private::slotRowsInserted(const QModelIndex& parent, int first, int last)
{
    Q_ASSERT((!parent.isValid()) || parent.model() == q_ptr->model());
//     qDebug() << "\n\nADD" << first << last;

    if (!isActive(parent, first, last))
        return;

//     qDebug() << "\n\nADD2" << q_ptr->width() << q_ptr->height();

    auto pitem = parent.isValid() ? m_hMapper.value(parent) : m_pRoot;

    TreeTraversalItems *prev(nullptr);

    //FIXME use up()
    if (first && pitem)
        prev = pitem->m_hLookup.value(q_ptr->model()->index(first-1, 0, parent));

    //FIXME support smaller ranges
    for (int i = first; i <= last; i++) {
        auto idx = q_ptr->model()->index(i, 0, parent);
        Q_ASSERT(idx.isValid());
        Q_ASSERT(idx.parent() != idx);
        Q_ASSERT(idx.model() == q_ptr->model());

        auto e = addChildren(pitem, idx);

        // Keep a dual chained linked list between the visual elements
        e->m_tSiblings[PREVIOUS] = prev ? prev : nullptr; //FIXME incorrect

        //FIXME It can happen if the previous is out of the visible range
        Q_ASSERT( e->m_tSiblings[PREVIOUS] || e->m_Index.row() == 0);

        //TODO merge with bridgeGap
        if (prev)
            bridgeGap(prev, e, true);

        // This is required before ::ATTACH because otherwise ::down() wont work
        if ((!pitem->m_tChildren[FIRST]) || e->m_Index.row() <= pitem->m_tChildren[FIRST]->m_Index.row()) {
            e->m_tSiblings[NEXT] = pitem->m_tChildren[FIRST];
            pitem->m_tChildren[FIRST] = e;
        }

        e->performAction(TreeTraversalItems::Action::ATTACH);

        if ((!pitem->m_tChildren[FIRST]) || e->m_Index.row() <= pitem->m_tChildren[FIRST]->m_Index.row()) {
            Q_ASSERT(pitem != e);
            if (auto pe = V_ITEM(e->m_pTreeItem->up()))
                pe->m_pTTI->performAction(TreeTraversalItems::Action::MOVE);
        }

        if ((!pitem->m_tChildren[LAST]) || e->m_Index.row() > pitem->m_tChildren[LAST]->m_Index.row()) {
            Q_ASSERT(pitem != e);
            pitem->m_tChildren[LAST] = e;
            if (auto ne = V_ITEM(e->m_pTreeItem->down()))
                ne->m_pTTI->performAction(TreeTraversalItems::Action::MOVE);
        }

        if (auto rc = q_ptr->model()->rowCount(idx))
            slotRowsInserted(idx, 0, rc-1);

        prev = e;
    }

    if ((!pitem->m_tChildren[LAST]) || last > pitem->m_tChildren[LAST]->m_Index.row())
        pitem->m_tChildren[LAST] = prev;

    Q_ASSERT(pitem->m_tChildren[LAST]);

    //FIXME use down()
    if (q_ptr->model()->rowCount(parent) > last) {
        if (auto i = pitem->m_hLookup.value(q_ptr->model()->index(last+1, 0, parent))) {
            i->m_tSiblings[PREVIOUS] = prev;
            prev->m_tSiblings[NEXT] = i;
        }
//         else //FIXME it happens
//             Q_ASSERT(false);
    }

    //FIXME EVIL and useless
    m_pRoot->performAction(TreeTraversalItems::Action::MOVE);

    _test_validateTree(m_pRoot);

    Q_EMIT q_ptr->contentChanged();

    if (!parent.isValid())
        Q_EMIT q_ptr->countChanged();
}

void TreeView2Private::slotRowsRemoved(const QModelIndex& parent, int first, int last)
{
    Q_ASSERT((!parent.isValid()) || parent.model() == q_ptr->model());
    Q_EMIT q_ptr->contentChanged();

    if (!isActive(parent, first, last))
        return;

    auto pitem = parent.isValid() ? m_hMapper.value(parent) : m_pRoot;

    //TODO make sure the state machine support them
    //TreeTraversalItems *prev(nullptr), *next(nullptr);

    //FIXME use up()
    //if (first && pitem)
    //    prev = pitem->m_hLookup.value(q_ptr->model()->index(first-1, 0, parent));

    //next = pitem->m_hLookup.value(q_ptr->model()->index(last+1, 0, parent));

    //FIXME support smaller ranges
    for (int i = first; i <= last; i++) {
        auto idx = q_ptr->model()->index(i, 0, parent);

        auto elem = pitem->m_hLookup.value(idx);
        Q_ASSERT(elem);

        elem->performAction(TreeTraversalItems::Action::DETACH);
    }

    if (!parent.isValid())
        Q_EMIT q_ptr->countChanged();
}

void TreeView2Private::slotLayoutChanged()
{

    if (auto rc = q_ptr->model()->rowCount())
        slotRowsInserted({}, 0, rc - 1);

    Q_EMIT q_ptr->contentChanged();

    Q_EMIT q_ptr->countChanged();
}

void TreeView2Private::createGap(VisualTreeItem* first, VisualTreeItem* last)
{
    Q_ASSERT(first->m_pTTI->m_pParent == last->m_pTTI->m_pParent);

    if (first->m_pTTI->m_tSiblings[PREVIOUS]) {
        first->m_pTTI->m_tSiblings[PREVIOUS]->m_tSiblings[NEXT] = last->m_pTTI->m_tSiblings[NEXT];
    }

    if (last->m_pTTI->m_tSiblings[NEXT]) {
        last->m_pTTI->m_tSiblings[NEXT]->m_tSiblings[PREVIOUS] = first->m_pTTI->m_tSiblings[PREVIOUS];
    }

    if (first->m_pTTI->m_pParent->m_tChildren[FIRST] == first->m_pTTI)
        first->m_pTTI->m_pParent->m_tChildren[FIRST] = last->m_pTTI->m_tSiblings[NEXT];

    if (last->m_pTTI->m_pParent->m_tChildren[LAST] == last->m_pTTI)
        last->m_pTTI->m_pParent->m_tChildren[LAST] = first->m_pTTI->m_tSiblings[PREVIOUS];

    Q_ASSERT((!first->m_pTTI->m_tSiblings[PREVIOUS]) ||
        first->m_pTTI->m_tSiblings[PREVIOUS]->m_pTreeItem->down() != first);
    Q_ASSERT((!last->m_pTTI->m_tSiblings[NEXT]) ||
        last->m_pTTI->m_tSiblings[NEXT]->m_pTreeItem->up() != last);

    Q_ASSERT((!first) || first->m_pTTI->m_tChildren[FIRST] || first->m_pTTI->m_hLookup.isEmpty());
    Q_ASSERT((!last) || last->m_pTTI->m_tChildren[FIRST] || last->m_pTTI->m_hLookup.isEmpty());

    // Do not leave invalid pointers for easier debugging
    last->m_pTTI->m_tSiblings[NEXT]      = nullptr;
    first->m_pTTI->m_tSiblings[PREVIOUS] = nullptr;
}

// Convenience wrapper
void TreeView2Private::bridgeGap(VisualTreeItem* first, VisualTreeItem* second, bool insert)
{
    bridgeGap(
        first  ? first->m_pTTI  : nullptr,
        second ? second->m_pTTI : nullptr,
        insert
    );
}

/// Fix the issues introduced by createGap (does not update m_pParent and m_hLookup)
void TreeView2Private::bridgeGap(TreeTraversalItems* first, TreeTraversalItems* second, bool insert)
{
    // 3 possible case: siblings, first child or last child

    if (first && second && first->m_pParent == second->m_pParent) {
        // first and second are siblings

        // Assume the second item is new
        if (insert && first->m_tSiblings[NEXT]) {
            second->m_tSiblings[NEXT] = first->m_tSiblings[NEXT];
            first->m_tSiblings[NEXT]->m_tSiblings[PREVIOUS] = second;
        }

        first->m_tSiblings[NEXT] = second;
        second->m_tSiblings[PREVIOUS] = first;
    }
    else if (second && ((!first) || first == second->m_pParent)) {
        // The `second` is `first` first child or it's the new root
        second->m_tSiblings[PREVIOUS] = nullptr;

        if (!second->m_pParent->m_tChildren[LAST])
            second->m_pParent->m_tChildren[LAST] = second;

        second->m_tSiblings[NEXT] = second->m_pParent->m_tChildren[FIRST];

        if (second->m_pParent->m_tChildren[FIRST]) {
            second->m_pParent->m_tChildren[FIRST]->m_tSiblings[PREVIOUS] = second;
        }

        second->m_pParent->m_tChildren[FIRST] = second;

        //BEGIN test
        /*int count =0;
        for (auto c = second->m_pParent->m_tChildren[FIRST]; c; c = c->m_tSiblings[NEXT])
            count++;
        Q_ASSERT(count == second->m_pParent->m_hLookup.size());*/
        //END test
    }
    else if (first) {
        // It's the last element or the second is a last leaf and first is unrelated
        first->m_tSiblings[NEXT] = nullptr;

        if (!first->m_pParent->m_tChildren[FIRST])
            first->m_pParent->m_tChildren[FIRST] = first;

        if (first->m_pParent->m_tChildren[LAST] && first->m_pParent->m_tChildren[LAST] != first) {
            first->m_pParent->m_tChildren[LAST]->m_tSiblings[NEXT] = first;
            first->m_tSiblings[PREVIOUS] = first->m_pParent->m_tChildren[LAST];
        }

        first->m_pParent->m_tChildren[LAST] = first;

        //BEGIN test
        int count =0;
        for (auto c = first->m_pParent->m_tChildren[LAST]; c; c = c->m_tSiblings[PREVIOUS])
            count++;

        Q_ASSERT(first->m_pParent->m_tChildren[FIRST]);

        Q_ASSERT(count == first->m_pParent->m_hLookup.size());
        //END test
    }
    else {
        Q_ASSERT(false); //Something went really wrong elsewhere
    }

    if (first)
        Q_ASSERT(first->m_pParent->m_tChildren[FIRST]);
    if (second)
        Q_ASSERT(second->m_pParent->m_tChildren[FIRST]);

    if (first)
        Q_ASSERT(first->m_pParent->m_tChildren[LAST]);
    if (second)
        Q_ASSERT(second->m_pParent->m_tChildren[LAST]);

//     if (first && second) { //Need to disable other asserts in down()
//         Q_ASSERT(first->down() == second);
//         Q_ASSERT(second->up() == first);
//     }
}

void TreeView2Private::setTemporaryIndices(const QModelIndex &parent, int start, int end,
                                     const QModelIndex &destination, int row)
{
    //FIXME list only
    // Before moving them, set a temporary now/col value because it wont be set
    // on the index until before slotRowsMoved2 is called (but after this
    // method returns //TODO do not use the hashmap, it is already known
    if (parent == destination) {
        const auto pitem = parent.isValid() ? m_hMapper.value(parent) : m_pRoot;
        for (int i = start; i <= end; i++) {
            auto idx = q_ptr->model()->index(i, 0, parent);

            auto elem = pitem->m_hLookup.value(idx);
            Q_ASSERT(elem);

            elem->m_pTreeItem->m_MoveToRow = row + (i - start);
        }

        for (int i = row; i <= row + (end - start); i++) {
            auto idx = q_ptr->model()->index(i, 0, parent);

            auto elem = pitem->m_hLookup.value(idx);
            Q_ASSERT(elem);

            elem->m_pTreeItem->m_MoveToRow = row + (end - start) + 1;
        }
    }
}

void TreeView2Private::resetTemporaryIndices(const QModelIndex &parent, int start, int end,
                                     const QModelIndex &destination, int row)
{
    //FIXME list only
    // Before moving them, set a temporary now/col value because it wont be set
    // on the index until before slotRowsMoved2 is called (but after this
    // method returns //TODO do not use the hashmap, it is already known
    if (parent == destination) {
        const auto pitem = parent.isValid() ? m_hMapper.value(parent) : m_pRoot;
        for (int i = start; i <= end; i++) {
            auto idx = q_ptr->model()->index(i, 0, parent);
            auto elem = pitem->m_hLookup.value(idx);
            Q_ASSERT(elem);
            elem->m_pTreeItem->m_MoveToRow = -1;
        }

        for (int i = row; i <= row + (end - start); i++) {
            auto idx = q_ptr->model()->index(i, 0, parent);
            auto elem = pitem->m_hLookup.value(idx);
            Q_ASSERT(elem);
            elem->m_pTreeItem->m_MoveToRow = -1;
        }
    }
}

void TreeView2Private::slotRowsMoved(const QModelIndex &parent, int start, int end,
                                     const QModelIndex &destination, int row)
{
    Q_ASSERT((!parent.isValid()) || parent.model() == q_ptr->model());
    Q_ASSERT((!destination.isValid()) || destination.model() == q_ptr->model());

    // There is literally nothing to do
    if (parent == destination && start == row)
        return;

    // Whatever has to be done only affect a part that's not currently tracked.
    if ((!isActive(parent, start, end)) && !isActive(destination, row, row+(end-start)))
        return;

    setTemporaryIndices(parent, start, end, destination, row);

    //TODO also support trees

    // As the actual view is implemented as a daisy chained list, only moving
    // the edges is necessary for the TreeTraversalItems. Each VisualTreeItem
    // need to be moved.

    const auto idxStart = q_ptr->model()->index(start, 0, parent);
    const auto idxEnd   = q_ptr->model()->index(end  , 0, parent);
    Q_ASSERT(idxStart.isValid() && idxEnd.isValid());

    //FIXME once partial ranges are supported, this is no longer always valid
    auto startVI = V_ITEM(q_ptr->itemForIndex(idxStart));
    auto endVI   = V_ITEM(q_ptr->itemForIndex(idxEnd));

    if (end - start == 1)
        Q_ASSERT(startVI->m_pTTI->m_tSiblings[NEXT] == endVI->m_pTTI);

    //FIXME so I don't forget, it will mess things up if silently ignored
    Q_ASSERT(startVI && endVI);
    Q_ASSERT(startVI->m_pTTI->m_pParent == endVI->m_pTTI->m_pParent);

    auto oldPreviousVI = V_ITEM(startVI->up());
    auto oldNextVI     = V_ITEM(endVI->down());

    Q_ASSERT((!oldPreviousVI) || oldPreviousVI->down() == startVI);
    Q_ASSERT((!oldNextVI) || oldNextVI->up() == endVI);

    auto newNextIdx = q_ptr->model()->index(row, 0, destination);

    // You cannot move things into an empty model
    Q_ASSERT((!row) || newNextIdx.isValid());

    VisualTreeItem *newNextVI(nullptr), *newPrevVI(nullptr);

    // Rewind until a next element is found, this happens when destination is empty
    if (!newNextIdx.isValid() && destination.parent().isValid()) {
        Q_ASSERT(q_ptr->model()->rowCount(destination) == row);
        auto par = destination.parent();
        do {
            if (q_ptr->model()->rowCount(par.parent()) > par.row()) {
                newNextIdx = q_ptr->model()->index(par.row(), 0, par.parent());
                break;
            }

            par = par.parent();
        } while (par.isValid());

        newNextVI = V_ITEM(q_ptr->itemForIndex(newNextIdx));
    }
    else {
        newNextVI = V_ITEM(q_ptr->itemForIndex(newNextIdx));
        newPrevVI = newNextVI ? V_ITEM(newNextVI->up()) : nullptr;
    }

    if (!row) {
        auto otherI = V_ITEM(q_ptr->itemForIndex(destination));
        Q_ASSERT((!newPrevVI) || otherI == newPrevVI);
    }

    // When there is no next element, then the parent has to be extracted manually
    if (!(newNextVI || newPrevVI)) {
        if (!row)
            newPrevVI = V_ITEM(q_ptr->itemForIndex(destination));
        else {
            newPrevVI = V_ITEM(q_ptr->itemForIndex(
                destination.model()->index(row-1, 0, destination)
            ));
        }
    }

    Q_ASSERT((newPrevVI || startVI) && newPrevVI != startVI);
    Q_ASSERT((newNextVI || endVI  ) && newNextVI != endVI  );

    TreeTraversalItems* newParentTTI = nullptr;
    auto oldParentTTI = startVI->m_pTTI->m_pParent;

    if (auto parentVI = V_ITEM(q_ptr->itemForIndex(destination)))
        newParentTTI = parentVI->m_pTTI;
    else
        newParentTTI = m_pRoot;

    // Make sure not to leave invalid pointers while the steps below are being performed
    createGap(startVI, endVI);

    // Update the tree parent (if necessary)
    if (oldParentTTI != newParentTTI) {
        for (auto i = startVI; i; i = i->m_pTTI->m_tSiblings[NEXT] ? i->m_pTTI->m_tSiblings[NEXT]->m_pTreeItem : nullptr) {
            auto idx = i->index();

            const int size = oldParentTTI->m_hLookup.size();
            oldParentTTI->m_hLookup.remove(idx);
            Q_ASSERT(oldParentTTI->m_hLookup.size() == size-1);

            newParentTTI->m_hLookup[idx] = i->m_pTTI;
            i->m_pTTI->m_pParent = newParentTTI;
            if (i == endVI)
                break;
        }
    }

    Q_ASSERT(startVI->m_pTTI->m_pParent == newParentTTI);
    Q_ASSERT(endVI->m_pTTI->m_pParent   == newParentTTI);

    bridgeGap(newPrevVI, startVI  );
    bridgeGap(endVI    , newNextVI);

    // Close the gap between the old previous and next elements
    Q_ASSERT(startVI->m_pTTI->m_tSiblings[NEXT]     != startVI->m_pTTI);
    Q_ASSERT(startVI->m_pTTI->m_tSiblings[PREVIOUS] != startVI->m_pTTI);
    Q_ASSERT(endVI->m_pTTI->m_tSiblings[NEXT]       != endVI->m_pTTI  );
    Q_ASSERT(endVI->m_pTTI->m_tSiblings[PREVIOUS]   != endVI->m_pTTI  );

    //BEGIN debug
    if (newPrevVI) {
    int count = 0;
    for (auto c = newPrevVI->m_pTTI->m_pParent->m_tChildren[FIRST]; c; c = c->m_tSiblings[NEXT])
        count++;
    Q_ASSERT(count == newPrevVI->m_pTTI->m_pParent->m_hLookup.size());

    count = 0;
    for (auto c = newPrevVI->m_pTTI->m_pParent->m_tChildren[LAST]; c; c = c->m_tSiblings[PREVIOUS])
        count++;
    Q_ASSERT(count == newPrevVI->m_pTTI->m_pParent->m_hLookup.size());
    }
    //END

    bridgeGap(oldPreviousVI, oldNextVI);


    if (endVI->m_pTTI->m_tSiblings[NEXT]) {
        Q_ASSERT(endVI->m_pTTI->m_tSiblings[NEXT]->m_tSiblings[PREVIOUS] == endVI->m_pTTI);
    }

    if (startVI->m_pTTI->m_tSiblings[PREVIOUS]) {
        Q_ASSERT(startVI->m_pTTI->m_tSiblings[PREVIOUS]->m_pParent == startVI->m_pTTI->m_pParent);
        Q_ASSERT(startVI->m_pTTI->m_tSiblings[PREVIOUS]->m_tSiblings[NEXT] == startVI->m_pTTI);
    }


//     Q_ASSERT((!newNextVI) || newNextVI->m_pParent->m_tSiblings[PREVIOUS] == endVI->m_pParent);
//     Q_ASSERT((!newPrevVI) ||
// //         newPrevVI->m_pParent->m_tSiblings[NEXT] == startVI->m_pParent ||
//         (newPrevVI->m_pParent->m_tChildren[FIRST] == startVI->m_pParent && !row)
//     );

//     Q_ASSERT((!oldPreviousVI) || (!oldPreviousVI->m_pParent->m_tSiblings[NEXT]) ||
//         oldPreviousVI->m_pParent->m_tSiblings[NEXT] == (oldNextVI ? oldNextVI->m_pParent : nullptr));


    // Move everything
    //TODO move it more efficient
    m_pRoot->performAction(TreeTraversalItems::Action::MOVE);

    resetTemporaryIndices(parent, start, end, destination, row);
}

void TreeView2Private::slotRowsMoved2(const QModelIndex &parent, int start, int end,
                                     const QModelIndex &destination, int row)
{
    Q_UNUSED(parent)
    Q_UNUSED(start)
    Q_UNUSED(end)
    Q_UNUSED(destination)
    Q_UNUSED(row)

    // The test would fail if it was in aboutToBeMoved
    _test_validateTree(m_pRoot);
}

void TreeView2Private::slotDataChanged(const QModelIndex& tl, const QModelIndex& br)
{
    if (tl.model() && tl.model() != q_ptr->model()) {
        Q_ASSERT(false);
        return;
    }

    if (br.model() && br.model() != q_ptr->model()) {
        Q_ASSERT(false);
        return;
    }

    if ((!tl.isValid()) || (!br.isValid()))
        return;

    if (!isActive(tl.parent(), tl.row(), br.row()))
        return;

    //FIXME tolerate other cases
    Q_ASSERT(q_ptr->model());
    Q_ASSERT(tl.model() == q_ptr->model() && br.model() == q_ptr->model());
    Q_ASSERT(tl.parent() == br.parent());

    //TODO Use a smaller range when possible

    //itemForIndex(const QModelIndex& idx) const final override;
    for (int i = tl.row(); i <= br.row(); i++) {
        const auto idx = q_ptr->model()->index(i, tl.column(), tl.parent());
        if (auto item = V_ITEM(q_ptr->itemForIndex(idx)))
            item->performAction(VisualTreeItem::Action::UPDATE);
    }
}

bool VisualTreeItem::performAction(Action a)
{
    if (m_State == VisualTreeItem::State::FAILED)
        m_pTTI->d_ptr->m_FailedCount--;

    const int s = (int)m_State;
    m_State     = m_fStateMap [s][(int)a];
    Q_ASSERT(m_State != VisualTreeItem::State::ERROR);

    const bool ret = (this->*m_fStateMachine[s][(int)a])();

    if (m_State == VisualTreeItem::State::FAILED || !ret) {
        m_State = VisualTreeItem::State::FAILED;
        m_pTTI->d_ptr->m_FailedCount++;
    }

    return ret;
}

QWeakPointer<FlickableView::ModelIndexItem> VisualTreeItem::reference() const
{
    if (!m_pSelf)
        m_pSelf = QSharedPointer<VisualTreeItem>(
            const_cast<VisualTreeItem*>(this)
        );

    return m_pSelf;
}

int VisualTreeItem::depth() const
{
    return m_pTTI->m_Depth;
}

bool VisualTreeItem::isVisible() const
{
    return true;//m_pTTI->TODO
}

QPersistentModelIndex VisualTreeItem::index() const
{
    return m_pTTI->m_Index;
}

/**
 * Flatten the tree as a linked list.
 *
 * Returns the previous non-failed item.
 */
FlickableView::ModelIndexItem* VisualTreeItem::up() const
{
    Q_ASSERT(m_State == State::ACTIVE
        || m_State == State::BUFFER
        || m_State == State::FAILED
        || m_State == State::POOLING
        || m_State == State::DANGLING //FIXME add a new state for
        // "deletion in progress" or swap the f call and set state
    );

    TreeTraversalItems* ret = nullptr;

    // Another simple case, there is no parent
    if (!m_pTTI->m_pParent) {
        Q_ASSERT(!index().parent().isValid()); //TODO remove, no longer true when partial loading is implemented

        return nullptr;
    }

    // The parent has no previous siblings, therefor it is directly above the item
    if (!m_pTTI->m_tSiblings[PREVIOUS]) {
//         if (m_pParent->m_pParent && m_pParent->m_pParent->m_tChildren[FIRST])
//             Q_ASSERT(m_pParent->m_pParent->m_tChildren[FIRST] == m_pParent);
        //FIXME Q_ASSERT(index().row() == 0);

        // This is the root, there is no previous element
        if (!m_pTTI->m_pParent->m_pTreeItem) {
            //Q_ASSERT(!index().parent().isValid()); //Happens when reseting models
            return nullptr;
        }

        ret = m_pTTI->m_pParent;

        // Avoids useless unreadable indentation
        goto sanitize;
    }

    ret = m_pTTI->m_tSiblings[PREVIOUS];

    while (ret->m_tChildren[LAST])
        ret = ret->m_tChildren[LAST];

sanitize:

    Q_ASSERT((!ret) || ret->m_pTreeItem);

    // Recursively look for a valid element. Doing this here allows the views
    // that implement this (abstract) class to work without having to always
    // check if some of their item failed to load. This is non-fatal in the
    // other Qt views, so it isn't fatal here either.
    if (ret && ret->m_pTreeItem->m_State == VisualTreeItem::State::FAILED)
        return ret->m_pTreeItem->up();

    return ret ? ret->m_pTreeItem : nullptr;
}

/**
 * Flatten the tree as a linked list.
 *
 * Returns the next non-failed item.
 */
FlickableView::ModelIndexItem* VisualTreeItem::down() const
{
    Q_ASSERT(m_State == State::ACTIVE
        || m_State == State::BUFFER
        || m_State == State::FAILED
        || m_State == State::POOLING
        || m_State == State::DANGLING //FIXME add a new state for
        // "deletion in progress" or swap the f call and set state
    );

    VisualTreeItem* ret = nullptr;
    auto i = m_pTTI;

    if (m_pTTI->m_tChildren[FIRST]) {
        //Q_ASSERT(m_pParent->m_tChildren[FIRST]->m_Index.row() == 0); //racy
        ret = m_pTTI->m_tChildren[FIRST]->m_pTreeItem;
        goto sanitizeNext;
    }


    // Recursively unwrap the tree until an element is found
    while(i) {
        if (i->m_tSiblings[NEXT]) {
            ret = i->m_tSiblings[NEXT]->m_pTreeItem;
            goto sanitizeNext;
        }

        i = i->m_pParent;
    }

    // Can't happen, exists to detect corrupted code
    if (m_pTTI->m_Index.parent().isValid()) {
        Q_ASSERT(m_pTTI->m_pParent);
//         Q_ASSERT(m_pParent->m_pParent->m_pParent->m_hLookup.size()
//             == m_pParent->m_Index.parent().row()+1);
    }

sanitizeNext:

    // Recursively look for a valid element. Doing this here allows the views
    // that implement this (abstract) class to work without having to always
    // check if some of their item failed to load. This is non-fatal in the
    // other Qt views, so it isn't fatal here either.
    if (ret && ret->m_State == VisualTreeItem::State::FAILED)
        return ret->down();

    return ret;
}


int VisualTreeItem::row() const
{
    return m_MoveToRow == -1 ? index().row() : m_MoveToRow;
}

int VisualTreeItem::column() const
{
    return m_MoveToColumn == -1 ? index().column() : m_MoveToColumn;
}

bool VisualTreeItem::nothing()
{
    return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
bool VisualTreeItem::error()
{
    Q_ASSERT(false);
    return true;
}
#pragma GCC diagnostic pop

bool VisualTreeItem::destroy()
{
    auto ptrCopy = m_pSelf;

    QTimer::singleShot(0,[this, ptrCopy]() {
        if (!ptrCopy)
            delete this;
        // else the reference will be dropped and the destructor called
    });

    m_pSelf.clear();
    return true;
}

bool TreeTraversalItems::performAction(Action a)
{
    const int s = (int)m_State;
    m_State     = m_fStateMap            [s][(int)a];
    bool ret    = (this->*m_fStateMachine[s][(int)a])();

    return ret;
}

bool TreeTraversalItems::nothing()
{ return true; }


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
bool TreeTraversalItems::error()
{
    Q_ASSERT(false);
    return false;
}
#pragma GCC diagnostic pop

bool TreeTraversalItems::show()
{
//     qDebug() << "SHOW";

    if (!m_pTreeItem) {
        m_pTreeItem = V_ITEM(d_ptr->q_ptr->createItem());
        m_pTreeItem->m_pTTI = this;
    }

    m_pTreeItem->performAction(VisualTreeItem::Action::ENTER_BUFFER);
    m_pTreeItem->performAction(VisualTreeItem::Action::ENTER_VIEW  );

    return true;
}

bool TreeTraversalItems::hide()
{
//     qDebug() << "HIDE";
    return true;
}

bool TreeTraversalItems::attach()
{
    if (m_pTreeItem)
        m_pTreeItem->performAction(VisualTreeItem::Action::ATTACH);

//     qDebug() << "ATTACH" << (int)m_State;
    performAction(Action::MOVE); //FIXME don't
    return performAction(Action::SHOW); //FIXME don't
}

// This methodwrap the removal of the element from the view
bool VisualTreeItem::detach()
{
    remove();
    m_State = VisualTreeItem::State::POOLED;

    return true;
}

bool TreeTraversalItems::detach()
{
    // First, detach any remaining children
    auto i = m_hLookup.begin();
    while ((i = m_hLookup.begin()) != m_hLookup.end())
        i.value()->performAction(TreeTraversalItems::Action::DETACH);
    Q_ASSERT(m_hLookup.isEmpty());

    if (m_pTreeItem) {

        // If the item was active (due, for example, of a full reset), then
        // it has to be removed from view then deleted.
        if (m_pTreeItem->m_State == VisualTreeItem::State::ACTIVE) {
            m_pTreeItem->performAction(VisualTreeItem::Action::DETACH);

            // It should still exists, it may crash otherwise, so make sure early
            Q_ASSERT(m_pTreeItem->m_State == VisualTreeItem::State::POOLED);

            m_pTreeItem->m_State = VisualTreeItem::State::POOLED;
            //FIXME ^^ add a new action for finish pooling or call
            // VisualTreeItem::detach from a new action method (instead of directly)
        }

        m_pTreeItem->performAction(VisualTreeItem::Action::DETACH);
        m_pTreeItem = nullptr;
    }

    if (m_pParent) {
        const int size = m_pParent->m_hLookup.size();
        m_pParent->m_hLookup.remove(m_Index);
        Q_ASSERT(size == m_pParent->m_hLookup.size()+1);
    }

    if (m_tSiblings[PREVIOUS] || m_tSiblings[NEXT]) {
        if (m_tSiblings[PREVIOUS])
            m_tSiblings[PREVIOUS]->m_tSiblings[NEXT] = m_tSiblings[NEXT];
        if (m_tSiblings[NEXT])
            m_tSiblings[NEXT]->m_tSiblings[PREVIOUS] = m_tSiblings[PREVIOUS];
    }
    else if (m_pParent) { //FIXME very wrong
        Q_ASSERT(m_pParent->m_hLookup.isEmpty());
        m_pParent->m_tChildren[FIRST] = nullptr;
        m_pParent->m_tChildren[LAST] = nullptr;
    }

    //FIXME set the parent m_tChildren[FIRST] correctly and add an insert()/move() method
    // then drop bridgeGap

    return true;
}

bool TreeTraversalItems::refresh()
{
    //
//     qDebug() << "REFRESH";

    for (auto i = m_tChildren[FIRST]; i; i = i->m_tSiblings[NEXT]) {
        Q_ASSERT(i);
        Q_ASSERT(i != this);
        i->performAction(TreeTraversalItems::Action::UPDATE);

        if (i == m_tChildren[LAST])
            break;
    }

    return true;
}

bool TreeTraversalItems::index()
{
    //TODO remove this implementation and use the one below

    // Propagate
    for (auto i = m_tChildren[FIRST]; i; i = i->m_tSiblings[NEXT]) {
        Q_ASSERT(i);
        Q_ASSERT(i != this);
        i->performAction(TreeTraversalItems::Action::MOVE);

        if (i == m_tChildren[LAST])
            break;
    }

    if (m_pTreeItem)
        m_pTreeItem->performAction(VisualTreeItem::Action::MOVE); //FIXME don't

    return true;

//     if (!m_pTreeItem)
//         return true;

//FIXME this is better, but require having createItem() called earlier
//     if (m_pTreeItem)
//         m_pTreeItem->performAction(VisualTreeItem::Action::MOVE); //FIXME don't
//
//     if (oldGeo != m_pTreeItem->geometry())
//         if (auto n = m_pTreeItem->down())
//             n->m_pParent->performAction(TreeTraversalItems::Action::MOVE);

//     return true;
}

bool TreeTraversalItems::destroy()
{
    detach();

    m_pTreeItem = nullptr;

    Q_ASSERT(m_hLookup.isEmpty());

    delete this;
    return true;
}

bool TreeView2Private::nothing()
{ return true; }

bool TreeView2Private::resetScoll()
{
    return true;
}

bool TreeView2Private::refresh()
{
    // Propagate
    for (auto i = m_pRoot->m_hLookup.constBegin(); i != m_pRoot->m_hLookup.constEnd(); i++)
        i.value()->performAction(TreeTraversalItems::Action::UPDATE);

    return true;
}

bool TreeView2Private::refreshFront()
{
    return true;
}

bool TreeView2Private::refreshBack()
{
    return true;
}

bool TreeView2Private::error()
{
    Q_ASSERT(false);
    return true;
}

#include <treeview2.moc>
#undef V_ITEM
#undef PREVIOUS
#undef NEXT
#undef FIRST
#undef LAST
