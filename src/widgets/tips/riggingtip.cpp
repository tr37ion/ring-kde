/****************************************************************************
 *   Copyright (C) 2012 by Savoir-Faire Linux                               *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
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
#include "riggingtip.h"

//Qt
#include <QtSvg/QSvgRenderer>
#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>
#include <QtGui/QApplication>
#include <QtCore/QFile>
#include <QtCore/QTimer>

//KDE
#include <KDebug>
#include <KLocale>
#include <KStandardDirs>

//STD
#include <cmath>

///Constructor
RiggingTip::RiggingTip(QWidget* parent) : Tip(QString(),parent),m_pTimer(nullptr),m_Counter(0)

{
   setHasBackground(false);
   setHasText(false);
   m_Padding = 0;
   m_phoneOriginalSVG = loadSvg(KStandardDirs::locate("data", "sflphone-client-kde/tips/phoneDown.svg"));
   m_ring1OriginalSVG = loadSvg(KStandardDirs::locate("data", "sflphone-client-kde/tips/ring1.svg"));
   m_ring2OriginalSVG = loadSvg(KStandardDirs::locate("data", "sflphone-client-kde/tips/ring2.svg"));
   m_ring3OriginalSVG = loadSvg(KStandardDirs::locate("data", "sflphone-client-kde/tips/ring3.svg"));
   loadSvg(KStandardDirs::locate("data", "sflphone-client-kde/tips/rigging.svg"));
   connect(this,SIGNAL(visibilityChanged(bool)),this,SLOT(startAnimation(bool)));
}

///Destructor
RiggingTip::~RiggingTip()
{
   if (m_pTimer) delete m_pTimer;
}

QRect RiggingTip::getDecorationRect()
{
   return QRect(0,0,135,120);
}

void RiggingTip::paintDecorations(QPainter& p, const QRect& textRect)
{
   Q_UNUSED(textRect);
   if (!m_pR) {
      m_pR = new QSvgRenderer(m_OriginalFile); //TODO delete
      m_pPhoneR = new QSvgRenderer(m_phoneOriginalSVG);
      m_pRing1R = new QSvgRenderer(m_ring1OriginalSVG);
      m_pRing2R = new QSvgRenderer(m_ring2OriginalSVG);
      m_pRing3R = new QSvgRenderer(m_ring3OriginalSVG);
      m_pPhonePix = new QPixmap(135         , 135*0.346975929367);
      m_pRing3Pix = new QPixmap(19.319489*6 , 4.73458*7);
      m_pRing2Pix = new QPixmap(13.757887*6 , 3.7651761*7);
      m_pRing1Pix = new QPixmap(9.3203869*6 , 2.9834957*7);
      
      m_pPhonePix->fill(QApplication::palette().base().color() );
      m_pRing1Pix->fill(QApplication::palette().base().color() );
      m_pRing2Pix->fill(QApplication::palette().base().color() );
      m_pRing3Pix->fill(QApplication::palette().base().color() );
      
      QPainter p2;
      p2.begin(m_pRing3Pix);
      m_pRing3R->render(&p2,QRect(0 , 0  , 19.319489*6 , 4.73458*7   ));
      p2.end();
      p2.begin(m_pRing2Pix);
      m_pRing2R->render(&p2,QRect(0 , 0  , 13.757887*6 , 3.7651761*7 ));
      p2.end();
      p2.begin(m_pRing1Pix);
      m_pRing1R->render(&p2,QRect(0 , 0  , 9.3203869*6 , 2.9834957*7 ));
      p2.end();
      p2.begin(m_pPhonePix);
      m_pPhoneR->render(&p2,QRect(0 , 0 , 135 , 135*0.346975929367 ));
      p2.end();
   }

      p.setOpacity(1);
      p.drawPixmap(0                   , 75 , *m_pPhonePix);
      p.setOpacity(fabs((float)(char)(m_Counter-160)/256.0));
      p.drawPixmap((135-19.319489*6)/2 , 0  , *m_pRing3Pix);
      p.setOpacity(fabs((float)(char)(m_Counter-80)/256.0));
      p.drawPixmap((135-13.757887*6)/2 , 25 , *m_pRing2Pix);
      p.setOpacity(fabs((float)(char)(m_Counter)/256.0));
      p.drawPixmap((135-9.3203869*6)/2 , 50 , *m_pRing1Pix);
}

void RiggingTip::startAnimation(bool visibility)
{
   if (!m_pTimer && visibility) {
      m_pTimer = new QTimer(this);
      connect(m_pTimer,SIGNAL(timeout()),this,SLOT(timeout()));
   }

   if (visibility)
      m_pTimer->start(1000/30);
   else if (m_pTimer) {
      m_pTimer->stop();
      m_Counter = 0;
   }
}

void RiggingTip::timeout()
{
   m_Counter += 8; //Animation speed
   reload(m_CurrentRect,true);
}