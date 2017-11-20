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
#include "quicktreeview.h"

// Qt
#include <QQmlContext>

/**
 * Polymorphic tree item for the TreeView2.
 *
 * Classes implementing TreeView2 need to provide an implementation of the pure
 * virtual functions. It is useful, for example, to manage both a raster and
 * QQuickItem based version of a view.
 *
 * The state is managed by the TreeView2 and it's own protected virtual methods.
 */
class QuickTreeViewItem : public VolatileTreeItem
{
public:
    explicit QuickTreeViewItem();
    virtual ~QuickTreeViewItem();

    // Actions
    virtual bool attach () override;
    virtual bool refresh() override;
    virtual bool move   () override;
    virtual bool flush  () override;
    virtual bool detach () override;

    virtual void setSelected(bool s) final override;
    virtual QRectF geometry() const final override;

private:
    QQuickItem* m_pItem     {nullptr};
    QQmlContext* m_pContent {nullptr};
    TreeViewPage* m_pPage   {nullptr};

    QuickTreeViewPrivate* d() const;
};

class QuickTreeViewPrivate
{
public:

    // When all elements are assumed to have the same height, life is easy
    QVector<qreal> m_DepthChart {0};
};

QuickTreeView::QuickTreeView(QQuickItem* parent) : TreeView2(parent),
    d_ptr(new QuickTreeViewPrivate)
{

}

QuickTreeView::~QuickTreeView()
{
    delete d_ptr;
}

FlickableView::ModelIndexItem* QuickTreeView::createItem() const
{
    return new QuickTreeViewItem();
}

QuickTreeViewItem::QuickTreeViewItem() : VolatileTreeItem()
{
}

QuickTreeViewItem::~QuickTreeViewItem()
{
    delete m_pItem;
}

QuickTreeViewPrivate* QuickTreeViewItem::d() const
{
    return static_cast<QuickTreeView*>(view())->QuickTreeView::d_ptr;
}

bool QuickTreeViewItem::attach()
{
    if (!view()->delegate()) {
        qDebug() << "Cannot attach, there is no delegate";
        return false;
    }

    auto pair = static_cast<QuickTreeView*>(view())->loadDelegate(
        view()->contentItem(),
        view()->rootContext(),
        index()
    );

    if (!pair.first->z())
        pair.first->setZ(1);

    d()->m_DepthChart[depth()] = std::max(
        d()->m_DepthChart[depth()],
        pair.first->height()
    );

    m_pContent = pair.second;
    m_pItem    = pair.first;

    // Add some useful metadata
    m_pContent->setContextProperty("rowCount", index().model()->rowCount(index()));
    m_pContent->setContextProperty("index", index().row());
    m_pContent->setContextProperty("modelIndex", index());

    Q_ASSERT(m_pItem && m_pContent);

    return move();
}

bool QuickTreeViewItem::refresh()
{
    return true;
}

bool QuickTreeViewItem::move()
{
    // Will happen when trying to move a FAILED, but buffered item
    if (!m_pItem) {
        qDebug() << "NO ITEM" << index().data();
        return false;
    }

    m_pItem->setWidth(view()->contentItem()->width());

    // So other items can be GCed without always resetting to 0x0, note that it
    // might be a good idea to extend SimpleFlickable to support a virtual
    // origin point.
    if (!previous()) {
        m_pItem->setY(0);
    }
    else if (auto otheri = static_cast<QuickTreeViewItem*>(previous())->m_pItem) {
        auto anchors = qvariant_cast<QObject*>(m_pItem->property("anchors"));
        anchors->setProperty("top", otheri->property("bottom"));
    }

    // Now, update the next anchors
    if (auto n = static_cast<QuickTreeViewItem*>(next())) {
        auto anchors = qvariant_cast<QObject*>(n->m_pItem->property("anchors"));
        anchors->setProperty("top", m_pItem->property("bottom"));
    }

    view()->contentItem()->setHeight(10000); //FIXME

    return true;
}

bool QuickTreeViewItem::flush()
{
    return true;
}

bool QuickTreeViewItem::detach()
{
    m_pItem->setParent(nullptr);
    m_pItem->setParentItem(nullptr);
    m_pItem->setVisible(false);
    return true;
}

void QuickTreeViewItem::setSelected(bool s)
{
    m_pContent->setContextProperty("isCurrentItem", s);
}

QRectF QuickTreeViewItem::geometry() const
{
    const QPointF p = m_pItem->mapFromItem(view()->contentItem(), {0,0});
    return {
        -p.x(),
        -p.y(),
        m_pItem->width(),
        m_pItem->height()
    };
}