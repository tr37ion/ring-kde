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
import Ring 1.0
import RingQmlWidgets 1.0
import QtGraphicalEffects 1.0
import Style 1.0
import org.kde.kirigami 2.2 as Kirigami

Item {
    id: chatMessage
    width: parent.width

    property color background
    property color foreground
    signal clicked()

    height: bubble.height + 10

    RowLayout {
        anchors.fill: parent

        PixmapWrapper {
            width: 50
            height: 50
            visible: direction == 1
            pixmap: decoration
            themeFallback: "im-user"
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 20
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Bubble {
                id: bubble
                anchors.margins: 5
                anchors.right: direction == 0 ? parent.right : undefined
                anchors.left : direction == 0 ? undefined : parent.left
                font.pointSize: Kirigami.Theme.defaultFont.pointSize*1.2
                font.family: "Noto Color Emoji"
                z: 1

                alignment: direction == 0 ? Text.AlignRight : Text.AlignLeft
                color: background

                text: display != undefined ? display : "N/A"
                maximumWidth: parent.width*((height > (width*0.5)) ? 0.9 : 0.7)
                height: Math.max(50, label.implicitHeight + dateLabel.implicitHeight + 5)

                Text {
                    id: label
                    anchors.fill: parent
                    anchors.leftMargin: 30
                    anchors.rightMargin: 30
                    anchors.topMargin: 5
                    anchors.bottomMargin: 5
                    horizontalAlignment: direction == 0 ? Text.AlignRight : Text.AlignLeft
                    font: bubble.font
                    text: display != undefined ? display : "N/A"
                    color: foreground
                    wrapMode: Text.WordWrap
                }

                Text {
                    id: dateLabel
                    anchors.bottom: parent.bottom
                    anchors.left: direction == 0 ? parent.left : undefined
                    anchors.right: direction == 1 ? parent.right : undefined
                    anchors.bottomMargin: 4
                    anchors.leftMargin: direction == 0 ? 4 : undefined
                    anchors.rightMargin: direction == 1 ? 4 : undefined
                    text: formattedDate != undefined ? formattedDate : "N/A"
                    color: "gray"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        chatMessage.clicked()
                    }
                }
            }
        }

        PixmapWrapper {
            width: 50
            height: 50
            visible: direction == 0
            pixmap: decoration
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 20
        }
    }
}
