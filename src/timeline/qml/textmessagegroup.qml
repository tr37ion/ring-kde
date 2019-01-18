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
import QtQuick 2.7
import QtQuick.Layouts 1.0


import RingQmlWidgets 1.0

Item {
    id: textGroupDelegate
    width: parent.width
    height: 50

    GroupHeader {
        type: "text"
        anchors.margins: 4
        height: 38
    }

    GroupFooter {
        anchors.horizontalCenter: parent.horizontalCenter
        y: (parent.parent && parent.parent.parent && parent.parent.parent.height) ?
            parent.parent.parent.height - height : 0
    }
}
