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
import Ring 1.0
import QtQuick.Layouts 1.0
import QtGraphicalEffects 1.0
import org.kde.kirigami 2.2 as Kirigami

import RingQmlWidgets 1.0

Rectangle {
    SystemPalette {
        id: activePalette
        colorGroup: SystemPalette.Active
    }

    SystemPalette {
        id: inactivePalette
        colorGroup: SystemPalette.Disabled
    }

    color: activePalette.base
    id: recentDock
    visible: true

    property alias overlayVisible: scrollBar.overlayVisible
    signal contactMethodSelected(var cm)
    signal contextMenuRequested(var cm, int index)

    FontMetrics {
        id: fontMetrics
    }

    Component {
        id: contactDelegate
        ContactMethodDelegate {}
    }

    Component {
        id: sectionDelegate
        PeersTimelineCategories {}
    }

    // To allow multiple places to set the contact method without knowing
    // it's index, use a selectionModel
    PeersTimelineSelectionModel {
        id: selectionMapper
        objectName: "selectionMapper"
        onCurrentIndexChanged: {
            recentView.currentIndex = current.row
        }
    }

    // Give time to all the code to run before invalidating the temporary
    // contact method. By then the code will have created a real one.
    Timer {
        id: clearTimer
        running: false
        repeat: false
        interval: 0 // Run idle
        onTriggered: {
            search.text = ""
        }
    }
    onContactMethodSelected: {
        clearTimer.running = true
    }

    ColumnLayout {
        anchors.fill: parent

        TextField {
            id: search
            Layout.fillWidth: true
            placeholderText: i18n("Find someone")
            text: CallModel.hasDialingCall ?
                CallModel.dialingCall().dialNumber : ""

            onTextChanged: {
                // Cache the text to avoid a binding loop when the dialing call
                // is created for the first time
                var text = search.text
                var call = CallModel.dialingCall()
                call.dialNumber = text
            }
            Keys.onDownPressed: {
                searchView.currentIndex = (searchView.currentIndex == searchView.count - 1) ?
                    0 : searchView.currentIndex + 1
            }
            Keys.onUpPressed: {
                searchView.currentIndex = (searchView.currentIndex == 0) ?
                    searchView.count - 1 : searchView.currentIndex - 1
            }
            Keys.onReturnPressed: {
                if (searchStateGroup.state != "searchActive")
                    return

                var cm = searchView.currentItem.contactMethod

                if (!cm)
                    return

                contactMethodSelected(cm)
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Item {
                id: newUserHelper
                width: parent.width - 20
                height: 200
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: 10

                Rectangle {
                    anchors.fill: parent
                    color: activePalette.text
                    opacity: 0.1
                    radius: 10
                }

                Text {
                    anchors.fill: parent
                    anchors.margins: 10
                    font.bold: true
                    wrapMode: Text.WordWrap
                    color: activePalette.base
                    text: i18n("To begin using Ring-KDE, enter an username in the box above and press enter or add new contacts")
                }
            }

            QuickListView {
                id: recentView
                clip: true
                anchors.fill: parent
//                 highlightMoveVelocity: Infinity //HACK
                delegate: contactDelegate
                section.delegate: sectionDelegate
                section.property: "formattedLastUsed" // indexedLastUsed
                section.model: PeersTimelineModel.timelineSummaryModel
                highlight: Item {

                    anchors.topMargin: 5
                    anchors.bottomMargin: 5
                    anchors.leftMargin: 30
                    anchors.rightMargin: 40
                    Rectangle {
                        anchors.fill: parent
                        anchors.topMargin: 5
                        anchors.bottomMargin: 5
                        anchors.leftMargin: 30
                        anchors.rightMargin: 40
                        radius: 10
                        color: activePalette.highlight
                    }
                }
                model: PeersTimelineModel.deduplicatedTimelineModel

                onCountChanged: {
                    if (count > 0)
                        newUserHelper.visible = false

                    scrollBar.visible = count*50 > recentDock.height
                    scrollBar.handleHeight = recentDock.height * (recentDock.height/(count*50))
                }

                onHeightChanged: {
                    scrollBar.handleHeight = recentDock.height * (recentDock.height/(count*50))
                }
            }

            TimelineScrollbar {
                id: scrollBar
                height: parent.height
                anchors.top: parent.top
                anchors.right: parent.right
                width: 10
                model: PeersTimelineModel.timelineSummaryModel
                z: 100
                onWidthChanged: {
                    burryOverlay.width = scrollBar.fullWidth + 15
                }
                visible: PeersTimelineModel.deduplicatedTimelineModel.count*50 > recentDock.height

                onPositionChanged: {
                    recentView.contentY = (recentView.contentHeight-recentView.height)*scrollBar.position
                }
            }

            // Add a blurry background
            ShaderEffectSource {
                id: effectSource
                visible: false

                sourceItem: recentView
                anchors.right: recentView.right
                anchors.top: recentView.top
                width: scrollBar.fullWidth + 15
                height: recentView.height

                sourceRect: Qt.rect(
                    parent.width - scrollBar.fullWidth - 15,
                    0,
                    scrollBar.fullWidth + 15,
                    recentView.height
                )
            }

            Item {
                id: burryOverlay
                visible: false
                opacity: 0
                anchors.right: recentView.right
                anchors.top: recentView.top
                width: scrollBar.fullWidth + 15
                height: recentView.height
                clip: true

                Repeater {
                    anchors.fill: parent
                    model: 5
                    FastBlur {
                        anchors.fill: parent
                        source: effectSource
                        radius: 30

                    }
                }

                Rectangle {
                    anchors.fill: parent
                    color: inactivePalette.highlight
                    opacity: 0.75
                }
            }

            FindPeers {
                id: searchView
                visible: false
                anchors.fill: parent
                z: 99999999
            }
        }
    }

    /*Rectangle {
        color: "red"
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 200

        Component {
            id: contactRequestDelegate
            Text {
                height: 40
                text: display
            }
        }

        ListView {
            anchors.fill: parent
            model: AccountModel.incomingContactRequestModel
            delegate: contactRequestDelegate
            Component.onCompleted: {
                console.log("\n\n\nCOMPLETED!!!!", AccountModel.incomingContactRequestModel
                    , AccountModel.incomingContactRequestModel.rowCount()
                )
            }
        }
    }*/

    onHeightChanged: {
        scrollBar.handleHeight = recentDock.height * (recentDock.height/(recentView.count*50))
    }

    Connections {
        target: CallModel
        onDialNumberChanged: {
            search.text = CallModel.hasDialingCall ?
                CallModel.dialingCall().dialNumber : ""
        }
    }

    // Timeline scrollbar
    StateGroup {
        states: [
            State {
                name: "overlay"
                when: scrollBar.overlayVisible
                PropertyChanges {
                    target:  burryOverlay
                    visible: true
                }
                PropertyChanges {
                    target:  burryOverlay
                    opacity: 1
                }
                PropertyChanges {
                    target:  effectSource
                    visible: true
                }
            }
        ]

        transitions: [
            Transition {
                to: "overlay"
                NumberAnimation {
                    properties: "opacity"
                    easing.type: Easing.InQuad
                    duration: 400
                    loops: 1
                }
            }
        ]
    }

    // Search
    StateGroup {
        id: searchStateGroup
        states: [
            State {
                name: "searchActive"
                when: search.text != ""
                PropertyChanges {
                    target:  scrollBar
                    visible: false
                }
                PropertyChanges {
                    target:  searchView
                    visible: true
                }
                PropertyChanges {
                    target:  burryOverlay
                    visible: true
                    opacity: 1
                    width: recentView.width
                    height: recentView.height
                }
                PropertyChanges {
                    target:  effectSource
                    sourceRect: Qt.rect(0, 0, parent.width, parent.height)
                }
            }
        ]

        transitions: [
            Transition {
                to: "searchActive"
                NumberAnimation {
                    properties: "opacity"
                    easing.type: Easing.InQuad
                    duration: 200
                    loops: 1
                }
                NumberAnimation {
                    properties: "width,height"
                    easing.type: Easing.InQuad
                    duration: 0
                    loops: 1
                }
            }
        ]
    }

}
