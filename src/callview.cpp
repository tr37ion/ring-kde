/***************************************************************************
 *   Copyright (C) 2009-2012 by Savoir-Faire Linux                         *
 *   Author : Emmanuel Lepage Valle <emmanuel.lepage@savoirfairelinux.com >*
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

//Parent
#include "callview.h"

//Qt
#include <QtGui/QTreeWidget>
#include <QtGui/QTreeWidgetItem>
#include <QtGui/QPushButton>
#include <QtGui/QSpacerItem>
#include <QtGui/QGridLayout>
#include <QtGui/QLabel>

//KDE
#include <KDebug>
#include <KLineEdit>
#include <KStandardDirs>

//SFLPhone library
#include "lib/contact.h"
#include "lib/sflphone_const.h"
#include "lib/callmanager_interface_singleton.h"
#include "klib/akonadibackend.h"
#include "klib/configurationskeleton.h"
#include "klib/helperfunctions.h"

//sflphone
#include "sflphoneview.h"
#include "widgets/calltreeitem.h"
#include "sflphone.h"
#include "sflphoneaccessibility.h"
#include "widgets/conferencebox.h"

///CallTreeItemDelegate: Delegates for CallTreeItem
class CallTreeItemDelegate : public QStyledItemDelegate
{
public:
CallTreeItemDelegate(CallView* widget)
      : QStyledItemDelegate(widget)
      , m_tree(widget)
      , m_ConferenceDrawer()
   {
   }

   QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
   QSize sh = QStyledItemDelegate::sizeHint(option, index);
   QTreeWidgetItem* item = (m_tree)->itemFromIndex(index);
   if (item) {
      CallTreeItem* widget = (CallTreeItem*)m_tree->itemWidget(item,0);
      if (widget)
         sh.rheight() = widget->sizeHint().height()+11; //Equal top and bottom padding

      if (index.parent().isValid() && !index.parent().child(index.row()+1,0).isValid())
         sh.rheight() += 15;
   }
   return sh;
   }

   QRect fullCategoryRect(const QStyleOptionViewItem& option, const QModelIndex& index) const {
      QModelIndex i(index),old(index);
      //BEGIN real sizeHint()
      //Otherwise it would be called too often (thanks to valgrind)
      ((CallTreeItemDelegate*)this)->m_SH          = QStyledItemDelegate::sizeHint(option, index);
      ((CallTreeItemDelegate*)this)->m_LeftMargin  = m_ConferenceDrawer.leftMargin();
      ((CallTreeItemDelegate*)this)->m_RightMargin = m_ConferenceDrawer.rightMargin();
      if (!index.parent().isValid() && index.child(0,0).isValid()) {
         ((QSize)m_SH).rheight() += 2 * m_ConferenceDrawer.leftMargin();
      } else {
         ((QSize)m_SH).rheight() += m_ConferenceDrawer.leftMargin();
      }
      ((QSize)m_SH).rwidth() += m_ConferenceDrawer.leftMargin();
      //END real sizeHint()

      if (i.parent().isValid()) {
         i = i.parent();
      }

      //Avoid repainting the category over and over (optimization)
      //note: 0,0,0,0 is actually wrong, but it wont change anything for this use case
      if (i != old && old.row()>2)
         return QRect(0,0,0,0);

      QTreeWidgetItem* item = m_tree->itemFromIndex(i);
      QRect r = m_tree->visualItemRect(item);

      // adapt width
      r.setLeft(m_ConferenceDrawer.leftMargin());
      r.setWidth(m_tree->viewport()->width() - m_ConferenceDrawer.leftMargin() - m_ConferenceDrawer.rightMargin());

      // adapt height
      if (item->isExpanded() && item->childCount() > 0) {
         const int childCount = item->childCount();
         //There is a massive implact on CPU usage to have massive rect
         for (int i =0;i < childCount;i++) {
            r.setHeight(r.height() + sizeHint(option,index).height());
         }
      }

      r.setTop(r.top() + m_ConferenceDrawer.leftMargin());

      return r;
    }

   virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
   {
      Q_ASSERT(index.isValid());

      QStyleOptionViewItem opt(option);
      //BEGIN: draw toplevel items
      if (!index.parent().isValid() && index.child(0,0).isValid()) {
         const QRegion cl = painter->clipRegion();
         painter->setClipRect(opt.rect);
         opt.rect = fullCategoryRect(option, index);
         m_ConferenceDrawer.drawCategory(index, 0, opt, painter);
         painter->setClipRegion(cl);
         return;
      }

      if (!index.parent().parent().isValid()) {
         opt.rect = fullCategoryRect(option, index);
         const QRegion cl = painter->clipRegion();
         QRect cr = option.rect;
         if (index.column() == 0) {
            if (m_tree->layoutDirection() == Qt::LeftToRight) {
               cr.setLeft(5);
            } else {
               cr.setRight(opt.rect.right());
            }
         }
         painter->setClipRect(cr);
         if (index.parent().isValid())
            m_ConferenceDrawer.drawCategory(index, 0, opt, painter);
         painter->setClipRegion(cl);
         painter->setRenderHint(QPainter::Antialiasing, false);
      }

      //END: draw background of category for all other items

      int max = 9999;
      painter->setClipRect(option.rect);
      if (option.state & QStyle::State_Selected) {
         QStyleOptionViewItem opt2(option);
         opt2.rect.setWidth(opt2.rect.width()-15);
         //Check if it is the last item
         if (index.parent().isValid() && !index.parent().child(index.row()+1,0).isValid()) {
            opt2.rect.setHeight(opt2.rect.height()-15);
            QStyledItemDelegate::paint(painter,opt2,index);
            max = opt2.rect.height();
         }
         else {
            QStyledItemDelegate::paint(painter,index.parent().isValid()?opt2:option,index);
         }
      }

      QTreeWidgetItem* item = m_tree->itemFromIndex(index);
      if (item) {
         QWidget* widget = m_tree->itemWidget(item,0);
         if (widget) {
            widget->setMinimumSize((m_tree->viewport()->width() - m_ConferenceDrawer.leftMargin() - m_ConferenceDrawer.rightMargin())-m_ConferenceDrawer.leftMargin(),10);
            widget->setMaximumSize((m_tree->viewport()->width() - m_ConferenceDrawer.leftMargin() - m_ConferenceDrawer.rightMargin())-m_ConferenceDrawer.leftMargin(),max);
         }
      }

      if (index.parent().isValid() && !index.parent().child(index.row()+1,0).isValid()) {
         m_ConferenceDrawer.drawBoxBottom(index, 0, option, painter);
      }
   }


private:
   CallView*      m_tree;
   ConferenceBox  m_ConferenceDrawer;
   QSize m_SH;
   int m_LeftMargin;
   int m_RightMargin;
};


///Retrieve current and older calls from the daemon, fill history and the calls TreeView and enable drag n' drop
CallView::CallView(QWidget* parent) : QTreeWidget(parent),m_pActiveOverlay(0),m_pCallPendingTransfer(0)
{
   //Widget part
   setAcceptDrops      (true );
   setDragEnabled      (true );
   setAnimated         (false);
   setUniformRowHeights(false);
   setRootIsDecorated  (false);
   setIndentation(15);

   CallTreeItemDelegate *delegate = new CallTreeItemDelegate(this);
   setItemDelegate(delegate);
   setSizePolicy(QSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding));

   QString image = "<img width=100 height=100  src='"+KStandardDirs::locate("data","sflphone-client-kde/transferarraw.png")+"' />";

   m_pTransferOverlay = new CallViewOverlay ( this               );
   m_pTransferB       = new QPushButton     ( m_pTransferOverlay );
   m_pTransferLE      = new KLineEdit       ( m_pTransferOverlay );
   QGridLayout* gl    = new QGridLayout     ( m_pTransferOverlay );
   QLabel* lblImg     = new QLabel          ( image              );

   m_pTransferOverlay->setVisible(false);
   m_pTransferOverlay->resize(size());
   m_pTransferOverlay->setCornerWidget(lblImg);
   m_pTransferOverlay->setAccessMessage(i18n("Please enter a transfer number and press Enter, press Escape to cancel"));

   m_pTransferB->setText(i18n("Transfer"));
   m_pTransferB->setMaximumSize(70,9000);

   gl->addItem  (new QSpacerItem(0,0,QSizePolicy::Expanding,QSizePolicy::Minimum), 0 , 0 , 1 , 3 );
   gl->addWidget(m_pTransferLE                                                   , 1 , 1 , 1 , 2 );
   gl->addWidget(m_pTransferB                                                    , 1 , 4 , 1 , 2 );
   gl->addItem  (new QSpacerItem(0,0,QSizePolicy::Expanding,QSizePolicy::Minimum), 2 , 0 , 1 , 3 );

   foreach(Call* active, SFLPhone::model()->getCallList()) {
      addCall(active);
   }

   foreach(Call* active, SFLPhone::model()->getConferenceList()) {
      if (qobject_cast<Call*>(active)) //As of May 2012, the daemon still produce fake conferences
         addConference(active);
   }

   //User Interface even
   //              SENDER                                   SIGNAL                              RECEIVER                     SLOT                       /
   /**/connect(this              , SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int))               , this, SLOT(itemDoubleClicked(QTreeWidgetItem*,int)) );
   /**/connect(this              , SIGNAL(itemClicked(QTreeWidgetItem*,int))                     , this, SLOT(itemClicked(QTreeWidgetItem*,int))       );
   /**/connect(this              , SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)) , this, SLOT(itemClicked(QTreeWidgetItem*))           );
   /**/connect(SFLPhone::model() , SIGNAL(conferenceCreated(Call*))                              , this, SLOT(addConference(Call*))                    );
   /**/connect(SFLPhone::model() , SIGNAL(conferenceChanged(Call*))                              , this, SLOT(conferenceChanged(Call*))                );
   /**/connect(SFLPhone::model() , SIGNAL(aboutToRemoveConference(Call*))                        , this, SLOT(conferenceRemoved(Call*))                );
   /**/connect(SFLPhone::model() , SIGNAL(callAdded(Call*,Call*))                                , this, SLOT(addCall(Call*,Call*))                    );
   /**/connect(m_pTransferB      , SIGNAL(clicked())                                             , this, SLOT(transfer())                              );
   /**/connect(m_pTransferLE     , SIGNAL(returnPressed())                                       , this, SLOT(transfer())                              );
   /*                                                                                                                                                  */

} //CallView

///Destructor
CallView::~CallView()
{
   delete m_pTransferB;
   delete m_pTransferLE;
   if (m_pTransferOverlay) delete m_pTransferOverlay;
   if (m_pActiveOverlay)   delete m_pActiveOverlay;
}

///A tree is not a good representation, remove branch and skin everything
void CallView::drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const
{
   Q_UNUSED(painter)
   Q_UNUSED(rect)
   Q_UNUSED(index)
}



/*****************************************************************************
 *                                                                           *
 *                        Drag and drop related code                         *
 *                                                                           *
 ****************************************************************************/

///Called when someone try to drop something on the tree
void CallView::dragEnterEvent ( QDragEnterEvent *e )
{
   kDebug() << "Potential drag event enter";
   e->accept();
}

///When a drag position change
void CallView::dragMoveEvent  ( QDragMoveEvent  *e )
{
   e->accept();
}

///When a drag event is leaving the widget
void CallView::dragLeaveEvent ( QDragLeaveEvent *e )
{
   kDebug() << "Potential drag event leave";
   e->accept();
}

///Proxy to handle transfer mime data
void CallView::transferDropEvent(Call* call,QMimeData* data)
{
   QByteArray encodedCallId = data->data( MIME_CALLID );
   SFLPhone::model()->attendedTransfer(SFLPhone::model()->getCall(encodedCallId),call);
}

///Proxy to handle conversation mime data
void CallView::conversationDropEvent(Call* call,QMimeData* data)
{
   kDebug() << "Calling real drag and drop function";
   dropMimeData(SFLPhone::model()->getIndex(call), 0, data, (Qt::DropAction)0);
}

///A call is dropped on another call
bool CallView::callToCall(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action)
{
   Q_UNUSED(index)
   Q_UNUSED(action)
   QByteArray encodedCallId      = data->data( MIME_CALLID      );
   if (!QString(encodedCallId).isEmpty()) {
      if (SFLPhone::model()->getIndex(encodedCallId) && dynamic_cast<Call*>(SFLPhone::model()->getCall(encodedCallId))) //Prevent a race
        clearArtefact(SFLPhone::model()->getIndex(encodedCallId));

      if (!parent) {
         kDebug() << "Call dropped on empty space";
         if (SFLPhone::model()->getIndex(encodedCallId)->parent()) {
            kDebug() << "Detaching participant";
            SFLPhone::model()->detachParticipant(SFLPhone::model()->getCall(encodedCallId));
         }
         else
            kDebug() << "The call is not in a conversation (doing nothing)";
         return true;
      }

      if (SFLPhone::model()->getCall(parent)->getCallId() == QString(encodedCallId)) {
         kDebug() << "Call dropped on itself (doing nothing)";
         return true;
      }
      else if (SFLPhone::model()->getIndex(encodedCallId) == parent) {
         kDebug() << "Dropping conference on itself (doing nothing)";
         return true;
      }

      if ((parent->childCount()) && (SFLPhone::model()->getIndex(encodedCallId)->childCount())) {
         kDebug() << "Merging two conferences";
         SFLPhone::model()->mergeConferences(SFLPhone::model()->getCall(parent),SFLPhone::model()->getCall(encodedCallId));
         return true;
      }
      else if ((parent->parent()) || (parent->childCount())) {
         kDebug() << "Call dropped on a conference";

         if (SFLPhone::model()->getIndex(encodedCallId)->childCount() && (SFLPhone::model()->getIndex(encodedCallId)->childCount()) && (!parent->childCount())) {
            kDebug() << "Conference dropped on a call (doing nothing)";
            return true;
         }

         QTreeWidgetItem* call1 = SFLPhone::model()->getIndex(encodedCallId);
         QTreeWidgetItem* call2 = (parent->parent())?parent->parent():parent;

         if (call1->parent()) {
            kDebug() << "Call 1 is part of a conference";
            if (call1->parent() == call2) {
               kDebug() << "Call dropped on it's own conference (doing nothing)";
               return true;
            }
            else if (SFLPhone::model()->getIndex(call1)->childCount()) {
               kDebug() << "Merging two conferences";
               SFLPhone::model()->mergeConferences(SFLPhone::model()->getCall(call1),SFLPhone::model()->getCall(call2));
            }
            else if (call1->parent()) {
               kDebug() << "Moving call from a conference to an other";
               SFLPhone::model()->detachParticipant(SFLPhone::model()->getCall(encodedCallId));
            }
         }
         kDebug() << "Adding participant";
         int state = SFLPhone::model()->getCall(call1)->getState();
         if(state == CALL_STATE_INCOMING || state == CALL_STATE_DIALING || state == CALL_STATE_TRANSFERRED || state == CALL_STATE_TRANSF_HOLD) {
            SFLPhone::model()->getCall(call1)->actionPerformed(CALL_ACTION_ACCEPT);
         }
         state = SFLPhone::model()->getCall(call2)->getState();
         if(state == CALL_STATE_INCOMING || state == CALL_STATE_DIALING || state == CALL_STATE_TRANSFERRED || state == CALL_STATE_TRANSF_HOLD) {
            SFLPhone::model()->getCall(call2)->actionPerformed(CALL_ACTION_ACCEPT);
         }
         SFLPhone::model()->addParticipant(SFLPhone::model()->getCall(call1),SFLPhone::model()->getCall(call2));
         return true;
      }
      else if (SFLPhone::model()->getIndex(encodedCallId) && (SFLPhone::model()->getIndex(encodedCallId)->childCount()) && (!parent->childCount())) {
         kDebug() << "Call dropped on it's own conference (doing nothing)";
         return true;
      }

      kDebug() << "Call dropped on another call";
      SFLPhone::model()->createConferenceFromCall(SFLPhone::model()->getCall(encodedCallId),SFLPhone::model()->getCall(parent));
      return true;
   }
   return false;
} //callToCall

///A string containing a call number is dropped on a call
bool CallView::phoneNumberToCall(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action)
{
   Q_UNUSED(index)
   Q_UNUSED(action)
   QByteArray encodedPhoneNumber = data->data( MIME_PHONENUMBER );
   if (!QString(encodedPhoneNumber).isEmpty()) {
      Contact* contact = AkonadiBackend::getInstance()->getContactByPhone(encodedPhoneNumber,true);
      QString name;
      name = (contact)?contact->getFormattedName():i18nc("Unknown peer","Unknown");
      Call* call2 = SFLPhone::model()->addDialingCall(name, AccountList::getCurrentAccount());
      if (call2) {
         call2->appendText(QString(encodedPhoneNumber));
         if (!parent) {
            //Dropped on free space
            kDebug() << "Adding new dialing call";
         }
         else if (parent->childCount() || parent->parent()) {
            //Dropped on a conversation
            QTreeWidgetItem* call = (parent->parent())?parent->parent():parent;
            SFLPhone::model()->addParticipant(SFLPhone::model()->getCall(call),call2);
         }
         else {
            //Dropped on call
            call2->actionPerformed(CALL_ACTION_ACCEPT);
            int state = SFLPhone::model()->getCall(parent)->getState();
            if(state == CALL_STATE_INCOMING || state == CALL_STATE_DIALING || state == CALL_STATE_TRANSFERRED || state == CALL_STATE_TRANSF_HOLD) {
               SFLPhone::model()->getCall(parent)->actionPerformed(CALL_ACTION_ACCEPT);
            }
            SFLPhone::model()->createConferenceFromCall(call2,SFLPhone::model()->getCall(parent));
         }
      }
      else {
         HelperFunctions::displayNoAccountMessageBox(this);
      }
   }
   return false;
} //phoneNumberToCall

///A contact ID is dropped on a call
bool CallView::contactToCall(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action)
{
   kDebug() << "contactToCall";
   Q_UNUSED( index  )
   Q_UNUSED( action )
   QByteArray encodedContact = data->data( MIME_CONTACT );
   if (!QString(encodedContact).isEmpty()) {
      Contact* contact = AkonadiBackend::getInstance()->getContactByUid(encodedContact);
      if (contact) {
         Call* call2 = nullptr;
         if (!SFLPhone::app()->view()->selectCallPhoneNumber(&call2,contact))
            return false;
         if (!parent) {
            //Dropped on free space
            kDebug() << "Adding new dialing call";
         }
         else if (parent->childCount() || parent->parent()) {
            //Dropped on a conversation
            QTreeWidgetItem* call = (parent->parent())?parent->parent():parent;
            SFLPhone::model()->addParticipant(SFLPhone::model()->getCall(call),call2);
         }
         else {
            //Dropped on call
//             if (!call2) {
//                call2 = SFLPhone::model()->addDialingCall(contact->getFormattedName());
//             }
//             QByteArray encodedPhoneNumber = data->data( MIME_PHONENUMBER );
//             if (!encodedPhoneNumber.isEmpty()) {
//                call2->setCallNumber(encodedPhoneNumber);
//             }

            call2->actionPerformed(CALL_ACTION_ACCEPT);
            int state = SFLPhone::model()->getCall(parent)->getState();
            if(state == CALL_STATE_INCOMING || state == CALL_STATE_DIALING || state == CALL_STATE_TRANSFERRED || state == CALL_STATE_TRANSF_HOLD) {
               SFLPhone::model()->getCall(parent)->actionPerformed(CALL_ACTION_ACCEPT);
            }
            SFLPhone::model()->createConferenceFromCall(call2,SFLPhone::model()->getCall(parent));
         }
      }
   }
   return false;
} //contactToCall

///Action performed when an item is dropped on the TreeView
bool CallView::dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action)
{
   Q_UNUSED(index)
   Q_UNUSED(action)

   QByteArray encodedCallId      = data->data( MIME_CALLID      );
   QByteArray encodedPhoneNumber = data->data( MIME_PHONENUMBER );
   QByteArray encodedContact     = data->data( MIME_CONTACT     );

   if (!QString(encodedCallId).isEmpty()) {
      kDebug() << "CallId dropped"<< QString(encodedCallId);
      callToCall(parent, index, data, action);
   }
   else if (!QString(encodedPhoneNumber).isEmpty()) {
      kDebug() << "PhoneNumber dropped"<< QString(encodedPhoneNumber);
      phoneNumberToCall(parent, index, data, action);
   }
   else if (!QString(encodedContact).isEmpty()) {
      kDebug() << "Contact dropped"<< QString(encodedContact);
      contactToCall(parent, index, data, action);
   }
   return false;
} //dropMimeData

///Encode data to be tranported during the drag n' drop operation
QMimeData* CallView::mimeData( const QList<QTreeWidgetItem *> items) const
{
   kDebug() << "A call is being dragged";
   if (items.size() < 1) {
      return nullptr;
   }

   QMimeData *mimeData = new QMimeData();

   //Call ID for internal call merging and spliting
   if (SFLPhone::model()->getCall(items[0])->isConference()) {
      mimeData->setData(MIME_CALLID, SFLPhone::model()->getCall(items[0])->getConfId().toAscii());
   }
   else {
      mimeData->setData(MIME_CALLID, SFLPhone::model()->getCall(items[0])->getCallId().toAscii());
   }

   //Plain text for other applications
   mimeData->setData(MIME_PLAIN_TEXT, QString(SFLPhone::model()->getCall(items[0])->getPeerName()+'\n'+SFLPhone::model()->getCall(items[0])->getPeerPhoneNumber()).toAscii());

   //TODO Comment this line if you don't want to see ugly artefact, but the caller details will not be visible while dragged
   items[0]->setText(0, SFLPhone::model()->getCall(items[0])->getPeerName() + '\n' + SFLPhone::model()->getCall(items[0])->getPeerPhoneNumber());
   return mimeData;
} //mimeData


/*****************************************************************************
 *                                                                           *
 *                            Call related code                              *
 *                                                                           *
 ****************************************************************************/

///Add a call in the model structure, the call must exist before being added to the model
Call* CallView::addCall(Call* call, Call* parent)
{
   QTreeWidgetItem* callItem = new QTreeWidgetItem();
   SFLPhone::model()->updateIndex(call,callItem);
   insertItem(callItem,parent);

   setCurrentItem(callItem);

   connect(call, SIGNAL(isOver(Call*)), this, SLOT(destroyCall(Call*)));
   return call;
}

///Transfer a call
void CallView::transfer()
{
   if (m_pCallPendingTransfer && !m_pTransferLE->text().isEmpty()) {
      SFLPhone::model()->transfer(m_pCallPendingTransfer,m_pTransferLE->text());
      if (ConfigurationSkeleton::enableVoiceFeedback()) {
         SFLPhoneAccessibility::getInstance()->say(i18n("Your call have been transferred to %1", m_pTransferLE->text()));
      }
   }

   m_pCallPendingTransfer = 0;
   m_pTransferLE->clear();

   m_pTransferOverlay->setVisible(false);
}

/*****************************************************************************
 *                                                                           *
 *                            View related code                              *
 *                                                                           *
 ****************************************************************************/

///Show the transfer overlay
void CallView::showTransferOverlay(Call* call)
{
   if (!m_pTransferOverlay) {
      kDebug() << "Creating overlay";
   }
   m_pTransferOverlay->setVisible(true);
   m_pCallPendingTransfer = call;
   m_pActiveOverlay = m_pTransferOverlay;
   m_pTransferLE->setFocus();
   connect(call,SIGNAL(changed()),this,SLOT(hideOverlay()));
}

///Is there an active overlay
bool CallView::haveOverlay()
{
   return (m_pActiveOverlay && m_pActiveOverlay->isVisible());
}

///Remove the active overlay
void CallView::hideOverlay()
{
   if (m_pActiveOverlay){
      disconnect(m_pCallPendingTransfer,SIGNAL(changed()),this,SLOT(hideOverlay()));
      m_pActiveOverlay->setVisible(false);
   }

   m_pActiveOverlay = 0;

   m_pCallPendingTransfer = 0;
} //hideOverlay

///Be sure the size of the overlay stay the same
void CallView::resizeEvent (QResizeEvent *e)
{
   if (m_pTransferOverlay)
      m_pTransferOverlay->resize(size());
   QTreeWidget::resizeEvent(e);
}

///Set the TreeView header text
void CallView::setTitle(const QString& title)
{
   headerItem()->setText(0,title);
}

///Return the current item
Call* CallView::getCurrentItem()
{
   if (currentItem() && SFLPhone::model()->getCall(QTreeWidget::currentItem()))
      return SFLPhone::model()->getCall(QTreeWidget::currentItem());
   else
      return false;
}

///Remove a TreeView item and delete it
bool CallView::removeItem(Call* item)
{
   if (indexOfTopLevelItem(SFLPhone::model()->getIndex(item)) != -1) {
      QTreeWidgetItem* parent = itemAt(indexOfTopLevelItem(SFLPhone::model()->getIndex(item)),0);
      removeItemWidget(SFLPhone::model()->getIndex(item),0);
      if (parent->childCount() == 0) //TODO this have to be done in the daemon, not here, but oops still happen too often to ignore
         removeItemWidget(parent,0);
      return true;
   }
   else
      return false;
}

///Return the TreeView, this
QWidget* CallView::getWidget()
{
   return this;
}

///Convenience wrapper around extractItem(QTreeWidgetItem*)
QTreeWidgetItem* CallView::extractItem(const QString& callId)
{
   QTreeWidgetItem* currentItem = SFLPhone::model()->getIndex(callId);
   return extractItem(currentItem);
}

///Extract an item from the TreeView and return it, the item is -not- deleted
QTreeWidgetItem* CallView::extractItem(QTreeWidgetItem* item)
{
   if (!item)
      return nullptr;
   QTreeWidgetItem* parentItem = item->parent();

   if (parentItem) {
      if ((indexOfTopLevelItem(parentItem) == -1 ) || (parentItem->indexOfChild(item) == -1)) {
         kDebug() << "The conversation does not exist";
         return 0;
      }

      QTreeWidgetItem* toReturn = parentItem->takeChild(parentItem->indexOfChild(item));

      return toReturn;
   }
   else
      return takeTopLevelItem(indexOfTopLevelItem(item));
} //extractItem

///Convenience wrapper around insertItem(QTreeWidgetItem*, QTreeWidgetItem*)
CallTreeItem* CallView::insertItem(QTreeWidgetItem* item, Call* parent)
{
   return insertItem(item,(parent)?SFLPhone::model()->getIndex(parent):0);
}

///Insert a TreeView item in the TreeView as child of parent or as a top level item, also restore the item Widget
CallTreeItem* CallView::insertItem(QTreeWidgetItem* item, QTreeWidgetItem* parent)
{
   if (!dynamic_cast<QTreeWidgetItem*>(item) && SFLPhone::model()->getCall(item) && !dynamic_cast<QTreeWidgetItem*>(parent))
      return nullptr;

   if (!item) {
      kDebug() << "This is not a valid call";
      return 0;
   }

   if (!parent)
      insertTopLevelItem(0,item);
   else
      parent->addChild(item);

   CallTreeItem* callItem = new CallTreeItem();
   connect(callItem, SIGNAL(askTransfer(Call*))                     , this, SLOT(showTransferOverlay(Call*))             );
   connect(callItem, SIGNAL(transferDropEvent(Call*,QMimeData*))    , this, SLOT(transferDropEvent(Call*,QMimeData*))    );
   connect(callItem, SIGNAL(conversationDropEvent(Call*,QMimeData*)), this, SLOT(conversationDropEvent(Call*,QMimeData*)));

   SFLPhone::model()->updateWidget(SFLPhone::model()->getCall(item), callItem);
   callItem->setCall(SFLPhone::model()->getCall(item));

   setItemWidget(item,0,callItem);

   expandAll();
   return callItem;
} //insertItem

///Remove a call from the interface
void CallView::destroyCall(Call* toDestroy)
{
   if (SFLPhone::model()->getIndex(toDestroy) == currentItem())
      setCurrentItem(0);

   if (!SFLPhone::model()->getIndex(toDestroy))
       kDebug() << "Call not found";
   else if (indexOfTopLevelItem(SFLPhone::model()->getIndex(toDestroy)) != -1)
      takeTopLevelItem(indexOfTopLevelItem(SFLPhone::model()->getIndex(toDestroy)));
   else if (SFLPhone::model()->getIndex(toDestroy)->parent()) {
      QTreeWidgetItem* callIndex = SFLPhone::model()->getIndex(toDestroy);
      QTreeWidgetItem* parent = callIndex->parent();
      if (indexOfTopLevelItem(parent) != -1) {
         parent->removeChild(callIndex);
         if (dynamic_cast<QTreeWidgetItem*>(parent) && parent->childCount() == 0) /*This should never happen, but it does*/
            takeTopLevelItem(indexOfTopLevelItem(parent));
      }
   }
   else
      kDebug() << "Call not found";
} //destroyCall

/// @todo Remove the text partially covering the TreeView item widget when it is being dragged, a beter implementation is needed
void CallView::clearArtefact(QTreeWidgetItem* item)
{
   if (item)
      item->setText(0,"");
}


/*****************************************************************************
 *                                                                           *
 *                           Event related code                              *
 *                                                                           *
 ****************************************************************************/

void CallView::itemDoubleClicked(QTreeWidgetItem* item, int column) {
   Q_UNUSED(column)
   kDebug() << "Item doubleclicked" << SFLPhone::model()->getCall(item)->getState();
   switch(SFLPhone::model()->getCall(item)->getState()) {
      case CALL_STATE_INCOMING:
         SFLPhone::model()->getCall(item)->actionPerformed(CALL_ACTION_ACCEPT);
         break;
      case CALL_STATE_HOLD:
         SFLPhone::model()->getCall(item)->actionPerformed(CALL_ACTION_HOLD);
         break;
      case CALL_STATE_DIALING:
         SFLPhone::model()->getCall(item)->actionPerformed(CALL_ACTION_ACCEPT);
         break;
      default:
         kDebug() << "Double clicked an item with no action on double click.";
    }
} //itemDoubleClicked

void CallView::itemClicked(QTreeWidgetItem* item, int column) {
   Q_UNUSED(column)
   Call* call = SFLPhone::model()->getCall(item);
   call->setSelected(true);

   if (ConfigurationSkeleton::enableReadDetails()) {
      SFLPhoneAccessibility::getInstance()->currentCallDetails();
   }

   emit itemChanged(call);
   kDebug() << "Item clicked";
}


/*****************************************************************************
 *                                                                           *
 *                         Conference related code                           *
 *                                                                           *
 ****************************************************************************/

///Add a new conference, get the call list and update the interface as needed
Call* CallView::addConference(Call* conf)
{
   kDebug() << "Conference created";
   Call* newConf =  conf;

   QTreeWidgetItem* confItem = new QTreeWidgetItem();
   SFLPhone::model()->updateIndex(conf,confItem);

   insertItem(confItem,(QTreeWidgetItem*)0);


   setCurrentItem(confItem);

   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   const QStringList callList = callManager.getParticipantList(conf->getConfId());

   foreach (const QString& callId, callList) {
      kDebug() << "Adding " << callId << "to the conversation";
      insertItem(extractItem(SFLPhone::model()->getIndex(callId)),confItem);
   }
   
   return newConf;
} //addConference

///Executed when the daemon signal a modification in an existing conference. Update the call list and update the TreeView
bool CallView::conferenceChanged(Call* conf)
{
   if (!dynamic_cast<Call*>(conf)) return false;
   kDebug() << "Conference changed";

   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   const QStringList callList = callManager.getParticipantList(conf->getConfId());

   QList<QTreeWidgetItem*> buffer;
   foreach (const QString& callId, callList) {
      if (SFLPhone::model()->getCall(callId)) {
         QTreeWidgetItem* item3 = extractItem(SFLPhone::model()->getIndex(callId));
         insertItem(item3, SFLPhone::model()->getIndex(conf));
         buffer << SFLPhone::model()->getIndex(callId);
      }
      else
         kDebug() << "Call " << callId << " does not exist";
   }

   QTreeWidgetItem* item = SFLPhone::model()->getIndex(conf);
   if (item) /*Can happen if the daemon crashed*/
      for (int j =0; j < item->childCount();j++) {
         if (buffer.indexOf(item->child(j)) == -1)
            insertItem(extractItem(item->child(j)));
      }

   Q_ASSERT_X(SFLPhone::model()->getIndex(conf)->childCount() == 0,"changing conference","A conference can't have no participants");

   return true;
} //conferenceChanged

///Remove a conference from the model and the TreeView
void CallView::conferenceRemoved(Call* conf)
{
   kDebug() << "Attempting to remove conference";
   QTreeWidgetItem* idx = SFLPhone::model()->getIndex(conf);
   if (idx) {
   while (idx->childCount()) {
      insertItem(extractItem(SFLPhone::model()->getIndex(conf)->child(0)));
   }
   takeTopLevelItem(indexOfTopLevelItem(SFLPhone::model()->getIndex(conf)));
   kDebug() << "Conference removed";
   }
   else {
      kDebug() << "Conference not found";
   }
} //conferenceRemoved

///Clear the list of old calls //TODO Clear them from the daemon
void CallView::clearHistory()
{
   //SFLPhone::model()->getHistory().clear();
}

///Redirect keypresses to parent
void CallView::keyPressEvent(QKeyEvent* event) {
   SFLPhone::app()->view()->keyPressEvent(event);
}

///Move selection using arrow keys
void CallView::moveSelectedItem( Qt::Key direction )
{
   if (direction == Qt::Key_Left) {
      setCurrentIndex(moveCursor(QAbstractItemView::MoveLeft ,Qt::NoModifier));
   }
   else if (direction == Qt::Key_Right) {
      setCurrentIndex(moveCursor(QAbstractItemView::MoveRight,Qt::NoModifier));
   }
   else if (direction == Qt::Key_Up) {
      qDebug() << "Move up";
      setCurrentIndex(moveCursor(QAbstractItemView::MoveUp   ,Qt::NoModifier));
   }
   else if (direction == Qt::Key_Down) {
      setCurrentIndex(moveCursor(QAbstractItemView::MoveDown ,Qt::NoModifier));
   }
}

/*****************************************************************************
 *                                                                           *
 *                                 Overlay                                   *
 *                                                                           *
 ****************************************************************************/

///Constructor
CallViewOverlay::CallViewOverlay(QWidget* parent) : QWidget(parent),m_pIcon(0),m_pTimer(0),m_enabled(true),m_black("black")
{
   m_black.setAlpha(75);
}

///Destructor
CallViewOverlay::~CallViewOverlay()
{

}

///Add a widget (usually an icon) in the corner
void CallViewOverlay::setCornerWidget(QWidget* wdg) {
   wdg->setParent      ( this                       );
   wdg->setMinimumSize ( 100         , 100          );
   wdg->resize         ( 100         , 100          );
   wdg->move           ( width()-100 , height()-100 );
   m_pIcon = wdg;
}

///Overload the setVisible method to trigger the timer
void CallViewOverlay::setVisible(bool enabled) {
   if (m_enabled != enabled) {
      if (m_pTimer) {
         m_pTimer->stop();
         disconnect(m_pTimer);
         delete m_pTimer;
      }
      m_pTimer = new QTimer(this);
      connect(m_pTimer, SIGNAL(timeout()), this, SLOT(changeVisibility()));
      m_step = 0;
      m_black.setAlpha(0);
      repaint();
      m_pTimer->start(10);
   }
   m_enabled = enabled;
   QWidget::setVisible(enabled);
   if (!m_accessMessage.isEmpty() && enabled == true && ConfigurationSkeleton::enableReadLabel()) {
      SFLPhoneAccessibility::getInstance()->say(m_accessMessage);
   }
} //setVisible

///How to paint the overlay
void CallViewOverlay::paintEvent(QPaintEvent* event) {
   Q_UNUSED(event)
   QPainter customPainter(this);
   customPainter.fillRect(rect(),m_black);
}

///Be sure the event is always the right size
void CallViewOverlay::resizeEvent(QResizeEvent *e) {
   Q_UNUSED(e)
   if (m_pIcon) {
      m_pIcon->setMinimumSize(100,100);
      m_pIcon->move(width()-100,height()-100);
   }
}

///Step by step animation to fade in/out
void CallViewOverlay::changeVisibility() {
   m_step++;
   m_black.setAlpha(0.1*m_step*m_step);
   repaint();
   if (m_step >= 35)
      m_pTimer->stop();
}

///Set accessibility message
void CallViewOverlay::setAccessMessage(QString message)
{
   m_accessMessage = message;
}

