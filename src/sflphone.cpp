/************************************** ************************************
 *   Copyright (C) 2009-2012 by Savoir-Faire Linux                         *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>         *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
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
#include "sflphone.h"

//System
#include <unistd.h>

//Qt
#include <QtCore/QString>
#include <QtGui/QActionGroup>
#include <QtGui/QLabel>
#include <QtGui/QCursor>

//KDE
#include <KDebug>
#include <KStandardAction>
#include <KAction>
#include <KStatusBar>
#include <KActionCollection>
#include <KNotification>
#include <KShortcutsDialog>
#include <KComboBox>

//sflphone library
#include "lib/sflphone_const.h"
#include "lib/instance_interface_singleton.h"
#include "lib/configurationmanager_interface_singleton.h"
#include "lib/contact.h"
#include "lib/accountlist.h"
#include "lib/instantmessagingmodel.h"

//sflphone
#include "klib/akonadibackend.h"
#include "accountwizard.h"
#include "sflphoneview.h"
#include "widgets/sflphonetray.h"
#include "widgets/contactdock.h"
#include "widgets/historydock.h"
#include "widgets/bookmarkdock.h"
#include "widgets/videodock.h"
#include "klib/configurationskeleton.h"
#include "sflphoneaccessibility.h"
#include "lib/videomodel.h"

SFLPhone* SFLPhone::m_sApp              = nullptr;
TreeWidgetCallModel* SFLPhone::m_pModel = nullptr;

///Constructor
SFLPhone::SFLPhone(QWidget *parent)
    : KXmlGuiWindow(parent), m_pInitialized(false), m_pView(new SFLPhoneView(this))
#ifdef ENABLE_VIDEO
      ,m_pVideoDW(nullptr)
#endif
{
    setupActions();
    m_sApp = this;
}

///Destructor
SFLPhone::~SFLPhone()
{
   if (!isHidden()) {
      ConfigurationSkeleton::setDisplayContactDock ( m_pContactCD->isVisible()  );
      ConfigurationSkeleton::setDisplayHistoryDock ( m_pHistoryDW->isVisible()  );
      ConfigurationSkeleton::setDisplayBookmarkDock( m_pBookmarkDW->isVisible() );
   }

   delete action_accept                ;
   delete action_refuse                ;
   delete action_hold                  ;
   delete action_transfer              ;
   delete action_record                ;
   delete action_mailBox               ;
   delete action_close                 ;
   delete action_quit                  ;
   delete action_displayVolumeControls ;
   delete action_displayDialpad        ;
   delete action_displayMessageBox     ;
   delete action_configureSflPhone     ;
   delete action_configureShortcut     ;
   delete action_accountCreationWizard ;
   delete action_pastenumber           ;
   delete action_showContactDock       ;
   delete action_showHistoryDock       ;
   delete action_showBookmarkDock      ;

   delete m_pView            ;
   delete m_pTrayIcon        ;
   delete m_pStatusBarWidget ;
   delete m_pContactCD       ;
   delete m_pCentralDW       ;
   delete m_pHistoryDW       ;
   delete m_pBookmarkDW      ;

   if (m_pModel) {
      delete m_pModel;
   }
   delete AkonadiBackend::getInstance();
   TreeWidgetCallModel::destroy();
   //saveState();
}

///Init everything
bool SFLPhone::initialize()
{
   if ( m_pInitialized ) {
      kDebug() << "Already initialized.";
      return false;
   }

   ConfigurationSkeleton::self();

   //Keep these template parameter or the static attribute wont be share between this and the call view, they need to be
//    CallModel<CallTreeItem*,QTreeWidgetItem*>* histoModel = new CallModel<CallTreeItem*,QTreeWidgetItem*>(CallModel<CallTreeItem*,QTreeWidgetItem*>::History);
//    histoModel->initHistory();

   // accept dnd
   setAcceptDrops(true);

   m_pContactCD = new ContactDock(this);
   addDockWidget(Qt::TopDockWidgetArea,m_pContactCD);
   m_pContactCD->show();
   m_pContactCD->setVisible(ConfigurationSkeleton::displayContactDock());

   // tell the KXmlGuiWindow that this is indeed the main widget
   m_pCentralDW = new QDockWidget(this);
   m_pCentralDW->setObjectName  ( "callDock"                                    );
   m_pCentralDW->setSizePolicy  ( QSizePolicy::Expanding,QSizePolicy::Expanding );
   m_pCentralDW->setWidget      ( m_pView                                       );
   m_pCentralDW->setWindowTitle ( i18n("Call")                                  );
   m_pCentralDW->setFeatures    ( QDockWidget::NoDockWidgetFeatures             );
   m_pView->setSizePolicy       ( QSizePolicy::Expanding,QSizePolicy::Expanding );
   m_pCentralDW->setStyleSheet  ( "\
      QDockWidget::title {\
         margin:0px;\
         padding:0px;\
         spacing:0px;\
         max-height:0px;\
      }\
      \
   ");

   m_pCentralDW->setTitleBarWidget(new QWidget());
   m_pCentralDW->setContentsMargins(0,0,0,0);
   m_pView->setContentsMargins     (0,0,0,0);

   addDockWidget(Qt::TopDockWidgetArea,m_pCentralDW);

   m_pHistoryDW       = new HistoryDock  ( this                     );
   m_pBookmarkDW      = new BookmarkDock ( this                     );
   m_pStatusBarWidget = new QLabel       (                          );

   //System tray
   m_pTrayIcon        = new SFLPhoneTray ( this->windowIcon(), this );
   m_pTrayIcon->addAction( action_accept   );
   m_pTrayIcon->addAction( action_mailBox  );
   m_pTrayIcon->addAction( action_refuse   );
   m_pTrayIcon->addAction( action_hold     );
   m_pTrayIcon->addAction( action_transfer );
   m_pTrayIcon->addAction( action_record   );
   m_pTrayIcon->addSeparator();
   m_pTrayIcon->addAction( action_quit     );

   addDockWidget( Qt::TopDockWidgetArea,m_pHistoryDW  );
   addDockWidget( Qt::TopDockWidgetArea,m_pBookmarkDW );
   tabifyDockWidget(m_pBookmarkDW,m_pHistoryDW);


   m_pHistoryDW->show();
   m_pHistoryDW->setVisible(ConfigurationSkeleton::displayHistoryDock());
   m_pBookmarkDW->show();
   m_pBookmarkDW->setVisible(ConfigurationSkeleton::displayBookmarkDock());


   setWindowIcon (QIcon(ICON_SFLPHONE) );
   setWindowTitle(i18n("SFLphone")     );

   setupActions();
   connect(action_showContactDock, SIGNAL(toggled(bool)),m_pContactCD, SLOT(setVisible(bool)));
   connect(action_showHistoryDock, SIGNAL(toggled(bool)),m_pHistoryDW, SLOT(setVisible(bool)));
   connect(action_showBookmarkDock,SIGNAL(toggled(bool)),m_pBookmarkDW,SLOT(setVisible(bool)));

   #ifdef ENABLE_VIDEO
   connect(VideoModel::getInstance(),SIGNAL(videoCallInitiated(VideoRenderer*)),this,SLOT(displayVideoDock(VideoRenderer*)));
   #endif

   statusBar()->addWidget(m_pStatusBarWidget);


   m_pTrayIcon->show();


   setObjectNames();
   QMetaObject::connectSlotsByName(this);
   m_pView->loadWindow();

   move(QCursor::pos().x() - geometry().width()/2, QCursor::pos().y() - geometry().height()/2);
   show();

   if (AccountList::getInstance()->size() <= 1) {
      (new AccountWizard())->show();
   }

   m_pIconChanged = false;
   m_pInitialized = true ;

   KStatusBar* bar = statusBar();
   
   QLabel* curAccL = new QLabel(i18n("Current account: "));
   bar->addPermanentWidget(curAccL);

   m_pAccountStatus = new KComboBox(bar);
   m_pAccountStatus->setModel(AccountList::getInstance());
   m_pAccountStatus->setMinimumSize(100,0);
   bar->addPermanentWidget(m_pAccountStatus);

   connect(m_pAccountStatus, SIGNAL(currentIndexChanged(int)), this, SLOT(currentAccountIndexChanged(int)) );
   connect(AccountList::getInstance(), SIGNAL(priorAccountChanged(Account*)),this,SLOT(currentPriorAccountChanged(Account*)));
   currentPriorAccountChanged(AccountList::getCurrentAccount());
   
   return true;
}

///Setup evry actions
void SFLPhone::setupActions()
{
   kDebug() << "setupActions";

   action_accept      = new KAction(this);
   action_refuse      = new KAction(this);
   action_hold        = new KAction(this);
   action_transfer    = new KAction(this);
   action_record      = new KAction(this);
   action_mailBox     = new KAction(this);

   action_accept->setShortcut      ( Qt::CTRL + Qt::Key_A );
   action_refuse->setShortcut      ( Qt::CTRL + Qt::Key_D );
   action_hold->setShortcut        ( Qt::CTRL + Qt::Key_H );
   action_transfer->setShortcut    ( Qt::CTRL + Qt::Key_T );
   action_record->setShortcut      ( Qt::CTRL + Qt::Key_R );
   action_mailBox->setShortcut     ( Qt::CTRL + Qt::Key_M );

   action_screen = new QActionGroup(this);
   action_screen->setExclusive(true);

   action_close = KStandardAction::close(this, SLOT(close()), this);
   action_quit  = KStandardAction::quit(this, SLOT(quitButton()), this);

   action_configureSflPhone = KStandardAction::preferences(m_pView, SLOT(configureSflPhone()), this);
   action_configureSflPhone->setText(i18n("Configure SFLphone"));

   action_displayDialpad        = new KAction(KIcon(QIcon(ICON_DISPLAY_DIALPAD)), i18n("Display dialpad")                 , this);
   action_displayMessageBox     = new KAction(KIcon("mail-message-new"), i18n("Display text message box")                 , this);
   action_displayVolumeControls = new KAction(KIcon(QIcon(ICON_DISPLAY_VOLUME_CONSTROLS)), i18n("Display volume controls"), this);
   action_pastenumber           = new KAction(KIcon("edit-paste"), i18n("Paste")                                          , this);
   action_showContactDock       = new KAction(KIcon("edit-find-user")   , i18n("Display Contact")                         , this);
   action_showHistoryDock       = new KAction(KIcon("view-history")     , i18n("Display history")                         , this);
   action_showBookmarkDock      = new KAction(KIcon("bookmark-new-list"), i18n("Display bookmark")                        , this);
   action_accountCreationWizard = new KAction(i18n("Account creation wizard")                                             , this);


   action_displayDialpad->setCheckable( true );
   action_displayDialpad->setChecked  ( ConfigurationSkeleton::displayDialpad() );
   action_configureSflPhone->setText(i18n("Configure SFLphone"));

   action_displayMessageBox->setCheckable( true );
   action_displayMessageBox->setChecked  ( ConfigurationSkeleton::displayMessageBox() );

   action_displayVolumeControls->setCheckable( true );
   action_displayVolumeControls->setChecked  ( ConfigurationSkeleton::displayVolume() );

   action_pastenumber->setShortcut ( Qt::CTRL + Qt::Key_V );

   action_showContactDock->setCheckable( true );
   action_showContactDock->setChecked(ConfigurationSkeleton::displayContactDock());

   action_showHistoryDock->setCheckable( true );
   action_showHistoryDock->setChecked(ConfigurationSkeleton::displayHistoryDock());

   action_showBookmarkDock->setCheckable( true );
   action_showBookmarkDock->setChecked(ConfigurationSkeleton::displayBookmarkDock());



   action_configureShortcut = new KAction(KIcon(KIcon("configure-shortcuts")), i18n("Configure Shortcut"), this);
   //                    SENDER                        SIGNAL               RECEIVER                 SLOT               /
   /**/connect(action_accept,                SIGNAL(triggered()),           m_pView , SLOT(accept())                    );
   /**/connect(action_refuse,                SIGNAL(triggered()),           m_pView , SLOT(refuse())                    );
   /**/connect(action_hold,                  SIGNAL(triggered()),           m_pView , SLOT(hold())                      );
   /**/connect(action_transfer,              SIGNAL(triggered()),           m_pView , SLOT(transfer())                  );
   /**/connect(action_record,                SIGNAL(triggered()),           m_pView , SLOT(record())                    );
   /**/connect(action_mailBox,               SIGNAL(triggered()),           m_pView , SLOT(mailBox())                   );
   /**/connect(action_displayVolumeControls, SIGNAL(toggled(bool)),         m_pView , SLOT(displayVolumeControls(bool)) );
   /**/connect(action_displayDialpad,        SIGNAL(toggled(bool)),         m_pView , SLOT(displayDialpad(bool))        );
   /**/connect(action_displayMessageBox,     SIGNAL(toggled(bool)),         m_pView , SLOT(displayMessageBox(bool))     );
   /**/connect(action_accountCreationWizard, SIGNAL(triggered()),           m_pView , SLOT(accountCreationWizard())     );
   /**/connect(action_pastenumber,           SIGNAL(triggered()),           m_pView , SLOT(paste())                     );
   /**/connect(action_configureShortcut,     SIGNAL(triggered()),           this    , SLOT(showShortCutEditor())        );
   /*                                                                                                                   */


   actionCollection()->addAction("action_accept"                , action_accept                );
   actionCollection()->addAction("action_refuse"                , action_refuse                );
   actionCollection()->addAction("action_hold"                  , action_hold                  );
   actionCollection()->addAction("action_transfer"              , action_transfer              );
   actionCollection()->addAction("action_record"                , action_record                );
   actionCollection()->addAction("action_mailBox"               , action_mailBox               );
   actionCollection()->addAction("action_close"                 , action_close                 );
   actionCollection()->addAction("action_quit"                  , action_quit                  );
   actionCollection()->addAction("action_displayVolumeControls" , action_displayVolumeControls );
   actionCollection()->addAction("action_displayDialpad"        , action_displayDialpad        );
   actionCollection()->addAction("action_displayMessageBox"     , action_displayMessageBox     );
   actionCollection()->addAction("action_configureSflPhone"     , action_configureSflPhone     );
   actionCollection()->addAction("action_accountCreationWizard" , action_accountCreationWizard );
   actionCollection()->addAction("action_configureShortcut"     , action_configureShortcut     );
   actionCollection()->addAction("action_pastenumber"           , action_pastenumber           );
   actionCollection()->addAction("action_showContactDock"       , action_showContactDock       );
   actionCollection()->addAction("action_showHistoryDock"       , action_showHistoryDock       );
   actionCollection()->addAction("action_showBookmarkDock"      , action_showBookmarkDock      );


   QList<KAction*> acList = *SFLPhoneAccessibility::getInstance();

   foreach(KAction* ac,acList) {
      actionCollection()->addAction(ac->objectName() , ac);
   }

   setAutoSaveSettings();
   createGUI();
}


/*****************************************************************************
 *                                                                           *
 *                                  Getters                                  *
 *                                                                           *
 ****************************************************************************/

///Singleton
SFLPhone* SFLPhone::app()
{
   return m_sApp;
}

///Get the view (to be used with the singleton)
SFLPhoneView* SFLPhone::view()
{
   return m_pView;
}

///Singleton
TreeWidgetCallModel* SFLPhone::model()
{
   if (!m_pModel) {
      m_pModel = new TreeWidgetCallModel();
      m_pModel->initCall();
      Call::setContactBackend(AkonadiBackend::getInstance());
      InstantMessagingModelManager::init(m_pModel);
      AccountList::getInstance()->setDefaultAccount(AccountList::getInstance()->getAccountById(ConfigurationSkeleton::defaultAccountId()));
      #ifdef ENABLE_VIDEO
      VideoModel::getInstance();
      #endif
    }
   return m_pModel;
}

///Return the contact dock
ContactDock*  SFLPhone::contactDock()
{
   return m_pContactCD;
}

///Return the history dock
HistoryDock*  SFLPhone::historyDock()
{
   return m_pHistoryDW;
}

///Return the bookmark dock
BookmarkDock* SFLPhone::bookmarkDock()
{
   return m_pBookmarkDW;
}

void SFLPhone::showShortCutEditor() {
   KShortcutsDialog::configure( actionCollection() );
}

///Produce an actionList for auto CallBack
QList<QAction*> SFLPhone::getCallActions()
{
   QList<QAction*> callActions = QList<QAction *>();
   callActions.insert((int) Accept   , action_accept   );
   callActions.insert((int) Refuse   , action_refuse   );
   callActions.insert((int) Hold     , action_hold     );
   callActions.insert((int) Transfer , action_transfer );
   callActions.insert((int) Record   , action_record   );
   callActions.insert((int) Mailbox  , action_mailBox  );
   return callActions;
}


/*****************************************************************************
 *                                                                           *
 *                                  Setters                                  *
 *                                                                           *
 ****************************************************************************/

///Set widgets object name
void SFLPhone::setObjectNames()
{
   m_pView->setObjectName      ( "m_pView"       );
   statusBar()->setObjectName  ( "statusBar"     );
   m_pTrayIcon->setObjectName  ( "m_pTrayIcon"   );
}


/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///[Action]Hide sflphone
bool SFLPhone::queryClose()
{
   if (!isHidden()) {
      ConfigurationSkeleton::setDisplayContactDock ( m_pContactCD->isVisible()  );
      ConfigurationSkeleton::setDisplayHistoryDock ( m_pHistoryDW->isVisible()  );
      ConfigurationSkeleton::setDisplayBookmarkDock( m_pBookmarkDW->isVisible() );
   }
   hide();
   return false;
}

///[Action] Quit action
void SFLPhone::quitButton()
{
   qApp->quit();
}

///Called when something happen
void SFLPhone::changeEvent(QEvent* event)
{
   if (event->type() == QEvent::ActivationChange && m_pIconChanged && isActiveWindow()) {
      m_pIconChanged = false;
   }
}

///Change status message
void SFLPhone::on_m_pView_statusMessageChangeAsked(const QString& message)
{
   m_pStatusBarWidget->setText(message);
}

///Change windowtitle
void SFLPhone::on_m_pView_windowTitleChangeAsked(const QString& message)
{
   setWindowTitle(message);
}

///Enable or disable toolbar items
void SFLPhone::on_m_pView_enabledActionsChangeAsked(const bool* enabledActions)
{
   action_accept->setVisible   ( enabledActions[SFLPhone::Accept   ]);
   action_refuse->setVisible   ( enabledActions[SFLPhone::Refuse   ]);
   action_hold->setVisible     ( enabledActions[SFLPhone::Hold     ]);
   action_transfer->setVisible ( enabledActions[SFLPhone::Transfer ]);
   action_record->setVisible   ( enabledActions[SFLPhone::Record   ]);
   action_mailBox->setVisible  ( enabledActions[SFLPhone::Mailbox  ]);
}

///Change icons
void SFLPhone::on_m_pView_actionIconsChangeAsked(const QString* actionIcons)
{
   action_accept->setIcon   ( QIcon(actionIcons[SFLPhone::Accept   ]));
   action_refuse->setIcon   ( QIcon(actionIcons[SFLPhone::Refuse   ]));
   action_hold->setIcon     ( QIcon(actionIcons[SFLPhone::Hold     ]));
   action_transfer->setIcon ( QIcon(actionIcons[SFLPhone::Transfer ]));
   action_record->setIcon   ( QIcon(actionIcons[SFLPhone::Record   ]));
   action_mailBox->setIcon  ( QIcon(actionIcons[SFLPhone::Mailbox  ]));
}

///Change text
void SFLPhone::on_m_pView_actionTextsChangeAsked(const QString* actionTexts)
{
   action_accept->setText   ( actionTexts[SFLPhone::Accept   ]);
   action_refuse->setText   ( actionTexts[SFLPhone::Refuse   ]);
   action_hold->setText     ( actionTexts[SFLPhone::Hold     ]);
   action_transfer->setText ( actionTexts[SFLPhone::Transfer ]);
   action_record->setText   ( actionTexts[SFLPhone::Record   ]);
   action_mailBox->setText  ( actionTexts[SFLPhone::Mailbox  ]);
}

///Change transfer state
void SFLPhone::on_m_pView_transferCheckStateChangeAsked(bool transferCheckState)
{
   action_transfer->setChecked(transferCheckState);
}

///Change record state
void SFLPhone::on_m_pView_recordCheckStateChangeAsked(bool recordCheckState)
{
   action_record->setChecked(recordCheckState);
}

///Called when a call is coming
void SFLPhone::on_m_pView_incomingCall(const Call* call)
{
   Contact* contact = AkonadiBackend::getInstance()->getContactByPhone(call->getPeerPhoneNumber());
   if (contact && call) {
      KNotification::event(KNotification::Notification, i18n("New incoming call"), i18n("New call from: \n") + (call->getPeerName().isEmpty() ? call->getPeerPhoneNumber() : call->getPeerName()),((contact->getPhoto())?*contact->getPhoto():nullptr));
   }
   KNotification::event(KNotification::Notification, i18n("New incoming call"), i18n("New call from: \n") + (call->getPeerName().isEmpty() ? call->getPeerPhoneNumber() : call->getPeerName()));
}

///Change current account
void SFLPhone::currentAccountIndexChanged(int newIndex)
{
   Account* acc = AccountList::getInstance()->getAccountByModelIndex(AccountList::getInstance()->index(newIndex,0));
   AccountList::getInstance()->setPriorAccount(acc);
}

///Update the combobox index
void SFLPhone::currentPriorAccountChanged(Account* newPrior)
{
   m_pAccountStatus->setCurrentIndex(newPrior->getIndex().row());
}

#ifdef ENABLE_VIDEO
///Display the video dock
void SFLPhone::displayVideoDock(VideoRenderer* r)
{
   if (!m_pVideoDW) {
      m_pVideoDW = new VideoDock(this);
   }
   m_pVideoDW->setRenderer(r);
   m_pVideoDW->show();
}
#endif