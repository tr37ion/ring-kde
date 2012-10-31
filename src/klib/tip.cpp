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
#include "tip.h"

//Qt
#include <QtSvg/QSvgRenderer>
#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>
#include <QtCore/QFile>

//KDE
#include <KDebug>

//SFLPhone
#include "tipmanager.h"

///Constructor
Tip::Tip(QWidget* parent,const QString& path, const QString& text, int maxLine) : QObject(parent),m_OriginalText(text),m_MaxLine(maxLine),m_Position(TipPosition::Bottom),m_IsMaxSize(false),m_pR(nullptr),
m_OriginalPalette(parent->palette()),m_AnimationIn(TipAnimation::TranslationTop),m_AnimationOut(TipAnimation::TranslationTop),m_pFont(nullptr)
{
   loadSvg(path);
}

///Destructor
Tip::~Tip()
{
   if (m_pFont) delete m_pFont;
}

/**
 * Reload the tip for new dimensions
 * @return The size required for the tip
 */
QSize Tip::reload(const QRect& availableSize)
{
   if (m_CurrentRect != availableSize && !(m_IsMaxSize && m_CurrentSize.width()*1.25 < availableSize.width())) {
      m_CurrentRect = availableSize;
      m_CurrentRect.setHeight(PADDING);

      //One 1000px wide line is not so useful, this may change later (variable)
      if (m_CurrentRect.width() > MAX_WIDTH) {
         m_CurrentRect.setWidth( MAX_WIDTH );
      }
      m_CurrentRect.setWidth(m_CurrentRect.width());

      //Get area required to display the text
      QRect textRect = getTextRect(m_OriginalText);
      m_CurrentRect.setHeight(m_CurrentRect.height() + textRect.height() + PADDING + getDecorationRect().height());

      //Create the background image
      m_CurrentImage = QImage(QSize(m_CurrentRect.width(),m_CurrentRect.height()),QImage::Format_RGB888);
      m_CurrentImage.fill(m_OriginalPalette.base().color() );
      QPainter p(&m_CurrentImage);
      p.setRenderHint(QPainter::Antialiasing, true);
      p.setFont(font());


      //Draw the tip rectangle
      p.setPen(QPen(m_OriginalPalette.base().color()));
      p.setBrush(QBrush(brightOrDarkBase()?Qt::black:Qt::white));
      p.drawRoundedRect(QRect(0,0,m_CurrentRect.width(),m_CurrentRect.height()),10,10);

      //Draw the wrapped text in textRectS
      p.drawText(textRect,Qt::TextWordWrap|Qt::AlignJustify,m_OriginalText);


      //If the widget is subclassed, this would allow decorations to be added like images
      paintDecorations(p,textRect);
      
      //Set the size from the RECT //TODO redundant
      m_CurrentSize = QSize(m_CurrentRect.width(),m_CurrentRect.height());
   }
   return m_CurrentSize;
}

QRect Tip::getTextRect(const QString& text)
{
   QFontMetrics metric(font());
   QRect rect = metric.boundingRect(QRect(PADDING,PADDING,m_CurrentRect.width()-2*PADDING,999999),Qt::AlignJustify|Qt::TextWordWrap,text);
   return rect;
}

///Check if the thene color scheme is darker than #888888
///@return true = bright, false = dark
bool Tip::brightOrDarkBase()
{
   QColor color = m_OriginalPalette.base().color();
   return (color.red() > 128 && color.green() > 128 && color.blue() > 128);
}


QRect Tip::getDecorationRect()
{
   return QRect(0,0,m_CurrentSize.width()-2*PADDING,60);
}

void Tip::paintDecorations(QPainter& p, const QRect& textRect)
{
   if (!m_pR)
      m_pR = new QSvgRenderer(m_OriginalFile);
   m_pR->render(&p,QRect(m_CurrentRect.width() - PADDING - 50*2.59143327842 - 10 ,textRect.y()+textRect.height() + 10,50*2.59143327842,50));
}

const QFont& Tip::font()
{
   if (!m_pFont) {
      m_pFont = new QFont();
      m_pFont->setBold(true);
   }
   return (const QFont&) *m_pFont;
}

QString Tip::loadSvg(const QString& path)
{
   QFile file(path);
   if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      kDebug() << "The tip" << path << "failed to load: No such file";
   }
   else {
      m_OriginalFile = file.readAll();
      m_OriginalFile.replace("BACKGROUD_COLOR_ROLE",brightOrDarkBase()?"#000000":"#ffffff");
      m_OriginalFile.replace("BASE_ROLE_COLOR",m_OriginalPalette.base().color().name().toAscii());
   }
   return m_OriginalFile;
}