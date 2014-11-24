/****************************************************************************
 *   Copyright (C) 2009-2014 by Savoir-Faire Linux                          *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>          *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/
#include "dlgaudio.h"

//Qt
#include <QtGui/QHeaderView>

//KDE
#include <KStandardDirs>
#include <KLineEdit>

//SFLPhone
#include "klib/kcfg_settings.h"
#include "conf/configurationdialog.h"
#include "lib/sflphone_const.h"
#include "lib/audio/settings.h"
#include "lib/audio/inputdevicemodel.h"
#include "lib/audio/outputdevicemodel.h"
#include "lib/audio/ringtonedevicemodel.h"
#include "lib/audio/managermodel.h"
#include "lib/audio/alsapluginmodel.h"

///Constructor
DlgAudio::DlgAudio(KConfigDialog *parent)
 : QWidget(parent),m_Changed(false),m_IsLoading(false)
{
   setupUi(this);

   m_pAlwaysRecordCK->setChecked(Audio::Settings::instance()->isAlwaysRecording());

   KUrlRequester_destinationFolder->setMode(KFile::Directory|KFile::ExistingOnly|KFile::LocalOnly);
   KUrlRequester_destinationFolder->setUrl(KUrl(Audio::Settings::instance()->recordPath()));
   KUrlRequester_destinationFolder->lineEdit()->setReadOnly(true);

   m_pSuppressNoise->setChecked(Audio::Settings::instance()->isNoiseSuppressEnabled());
   m_pCPlayDTMFCk->setChecked(Audio::Settings::instance()->areDTMFMuted());

   alsaInputDevice->setModel   (Audio::Settings::instance()->inputDeviceModel   () );
   alsaOutputDevice->setModel  (Audio::Settings::instance()->outputDeviceModel  () );
   alsaRingtoneDevice->setModel(Audio::Settings::instance()->ringtoneDeviceModel() );
   m_pManager->setModel        (Audio::Settings::instance()->managerModel       () );
   box_alsaPlugin->setModel    (Audio::Settings::instance()->alsaPluginModel    () );
   loadAlsaSettings();

   m_pManager->setCurrentIndex (Audio::Settings::instance()->managerModel()->currentManagerIndex().row());

   connect( box_alsaPlugin   , SIGNAL(activated(int)) , parent, SLOT(updateButtons()));
   connect( this             , SIGNAL(updateButtons()), parent, SLOT(updateButtons()));
   connect( m_pAlwaysRecordCK, SIGNAL(clicked(bool))  , this  , SLOT(changed())      );

   connect( box_alsaPlugin                  , SIGNAL(currentIndexChanged(int)) , SLOT(changed()));
   connect( m_pSuppressNoise                , SIGNAL(toggled(bool))            , SLOT(changed()));
   connect( m_pCPlayDTMFCk                  , SIGNAL(toggled(bool))            , SLOT(changed()));
   connect( alsaInputDevice                 , SIGNAL(currentIndexChanged(int)) , SLOT(changed()));
   connect( alsaOutputDevice                , SIGNAL(currentIndexChanged(int)) , SLOT(changed()));
   connect( alsaRingtoneDevice              , SIGNAL(currentIndexChanged(int)) , SLOT(changed()));
   connect( m_pManager                      , SIGNAL(currentIndexChanged(int)) , SLOT(changed()));
   connect( KUrlRequester_destinationFolder , SIGNAL(textChanged(QString))     , SLOT(changed()));
   connect( m_pManager                      , SIGNAL(currentIndexChanged(int)) , 
            Audio::Settings::instance()->managerModel(),SLOT(setCurrentManager(int)));
   connect(Audio::Settings::instance()->managerModel(),SIGNAL(currentManagerChanged(int)),m_pManager,
           SLOT(setCurrentIndex(int)));
   connect( m_pManager                      , SIGNAL(currentIndexChanged(int)) , SLOT(loadAlsaSettings()));
}

///Destructor
DlgAudio::~DlgAudio()
{
}

///Update the widgets
void DlgAudio::updateWidgets()
{
   loadAlsaSettings();
}

///Save the settings
void DlgAudio::updateSettings()
{
   if (m_Changed) {
      m_IsLoading = true;

      Audio::Settings::instance()->setRecordPath(KUrlRequester_destinationFolder->lineEdit()->text());
      Audio::Settings::instance()->setAlwaysRecording(m_pAlwaysRecordCK->isChecked());

      Audio::Settings::instance()->inputDeviceModel   ()->setCurrentDevice(alsaInputDevice->currentIndex   ());
      Audio::Settings::instance()->outputDeviceModel  ()->setCurrentDevice(alsaOutputDevice->currentIndex  ());
      Audio::Settings::instance()->ringtoneDeviceModel()->setCurrentDevice(alsaRingtoneDevice->currentIndex());
      Audio::Settings::instance()->alsaPluginModel    ()->setCurrentPlugin(box_alsaPlugin->currentIndex());
      Audio::Settings::instance()->setNoiseSuppressState(m_pSuppressNoise->isChecked());
      Audio::Settings::instance()->setDTMFMuted         (m_pCPlayDTMFCk  ->isChecked());

      m_Changed   = false;
      m_IsLoading = false;
   }
}

///Have this dialog changed
bool DlgAudio::hasChanged()
{
   return m_Changed;
}

///Tag the dialog as needing saving
void DlgAudio::changed()
{
   switch (Audio::Settings::instance()->managerModel()->currentManager()) {
      case Audio::ManagerModel::Manager::PULSE:
         box_alsaPlugin->setDisabled(true);
         stackedWidget_interfaceSpecificSettings->setVisible(true);
         break;
      case Audio::ManagerModel::Manager::ALSA:
         box_alsaPlugin->setDisabled(false);
         stackedWidget_interfaceSpecificSettings->setVisible(true);
         break;
      case Audio::ManagerModel::Manager::JACK:
         box_alsaPlugin->setDisabled(true);
         stackedWidget_interfaceSpecificSettings->setVisible(false);
         break;
   };
   if (!m_IsLoading) {
      m_Changed = true;
      emit updateButtons();
   }
}

///Load alsa settings
void DlgAudio::loadAlsaSettings()
{
   m_IsLoading = true;
   Audio::Settings::instance()->reload();
   alsaInputDevice->setCurrentIndex    ( Audio::Settings::instance()->inputDeviceModel()->currentDevice().row()   );
   alsaOutputDevice->setCurrentIndex   ( Audio::Settings::instance()->outputDeviceModel()->currentDevice().row()  );
   alsaRingtoneDevice->setCurrentIndex ( Audio::Settings::instance()->ringtoneDeviceModel()->currentDevice().row());
   box_alsaPlugin->setCurrentIndex     ( Audio::Settings::instance()->alsaPluginModel()->currentPlugin().row()    );
   m_IsLoading = false;
}
