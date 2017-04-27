/***************************************************************************
 *   Copyright (C) 2009-2015 by Savoir-Faire Linux                         *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>         *
 *            Emmanuel Lepage Vallee <elv1313@gmail.com>                   *
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
#include "dialpad.h"

//Qt
#include <QtWidgets/QLabel>
#include <QtWidgets/QGridLayout>
#include <QtGui/QFontMetrics>

const char* Dialpad::m_pNumbers[] =
       {"1", "2", "3",
        "4", "5", "6",
        "7", "8", "9",
        "*", "0", "#"};

const char* Dialpad::m_pTexts[12] =
        { ""  ,  "abc",  "def" ,
        "ghi" ,  "jkl",  "mno" ,
        "pqrs",  "tuv",  "wxyz",
          ""  ,   ""  ,   ""   };

///Constructor
Dialpad::Dialpad(QWidget *parent)
 : QWidget(parent),gridLayout(new QGridLayout(this)),m_pButtons(new DialpadButton*[12])
{
   for (uint i=0; i < 12;i++) {
      m_pButtons[i]       = new DialpadButton( this,m_pNumbers[i] );
      QHBoxLayout* layout = new QHBoxLayout  ( m_pButtons     [i]  );
      QLabel *number(new QLabel(m_pNumbers[i])),*text(new QLabel(m_pTexts[i]));
      static QFontMetrics metric(m_pButtons[i]->font());
      m_pButtons[i]->setMinimumHeight((30 > metric.height()+6)?30:metric.height()+6);
      gridLayout->addWidget( m_pButtons[i],i/3,i%3              );
      QFont font = number->font();
      font.setPointSize(m_NumberSize);
      number->setFont      ( font );
      number->setAlignment ( Qt::AlignRight | Qt::AlignVCenter  );
      font = text->font();
      font.setPointSize(m_TextSize);
      text->setFont        ( font );
      layout->setSpacing        ( m_Spacing );
      layout->addWidget         ( number    );
      layout->addWidget         ( text      );
      layout->setContentsMargins( 0,0,0,0   );
      connect(m_pButtons[i],&DialpadButton::typed,this,&Dialpad::clicked);
   }
} //Dialpad

///Destructor
Dialpad::~Dialpad()
{
   delete[] m_pButtons;
   delete gridLayout;
}

///Proxy to make the view more convinient to use
void Dialpad::clicked(QString& text)
{
   emit typed(text);
}

// kate: space-indent on; indent-width 3; replace-tabs on;
