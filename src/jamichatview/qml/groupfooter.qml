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
import org.kde.kirigami 2.2 as Kirigami

RowLayout {
    anchors.horizontalCenter: parent.horizontalCenter
    clip: true
    height: implicitHeight
    opacity: chatView.displayExtraTime ? 1 : 0
    Behavior on opacity {
            NumberAnimation {duration: 200}
    }

    property real lineWidth: Math.min(50, (width - label.implicitWidth)/2)

    Rectangle {
        height: 1
        Layout.preferredWidth: 50
        color: Kirigami.Theme.disabledTextColor
    }
    Text {
        id: label
        text: endAt
        color: Kirigami.Theme.disabledTextColor
    }
    Rectangle {
        height: 1
        Layout.preferredWidth: 50
        color: Kirigami.Theme.disabledTextColor
    }
}
