/***************************************************************************
 *   Copyright (C) 2013-2015 by Savoir-Faire Linux                         *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
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
#ifndef CMD_H
#define CMD_H

#include <QObject>

//KDE
class KAboutData;

//Ring
class Call;

class Cmd : public QObject {
   Q_OBJECT

public:
   virtual ~Cmd(){}

   //Static mutators
   static void parseCmd(int argc, char **argv, KAboutData& about);
   static void placeCall(const QString& number);
   static void sendText(const QString& number, const QString& text);

private:
   //Private constructor
   explicit Cmd(QObject* parent=nullptr);

   static Cmd* instance();

   //Attributes
   static Cmd* m_spSelf;

private Q_SLOTS:
   void textMessagePickup(Call* call);
};

#endif