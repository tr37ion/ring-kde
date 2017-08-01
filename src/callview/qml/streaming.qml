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
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.0
import Ring 1.0

ColumnLayout {
    id: streaming
    property QtObject call: null
    property string currentFile: ""

    Component {
        id: fileDelegate
        Item {
            height: 30
            Text {
                text: display
                color: "white"
                anchors.fill: parent
                anchors.margins: 5
            }
        }
    }

    ListView {
        model: RecentFileModel
        Layout.fillWidth: true
        Layout.fillHeight: true
        delegate: fileDelegate
    }
    Button {
        Layout.fillWidth: true
        text: "Select file"
        onClicked: {
            streaming.currentFile = RecentFileModel.addFile()
        }
    }
    Item {
        Layout.fillWidth: true
        height:streamButton.height
        Button {
            id: streamButton
            text: "Stream"
            anchors.right: parent.right
            onClicked: {
                if (!streaming.call)
                    return

                if (!streaming.call.sourceModel)
                    return

                streaming.call.sourceModel.setFile(streaming.currentFile)
            }
        }
    }
}