/*
 *   Copyright 2019 Emmanuel Lepage <emmanuel.lepage@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 3, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
import QtQuick 2.2
import QtQuick.Layouts 1.4
import QtQuick.Controls 2.2 as Controls
import org.kde.kirigami 2.6 as Kirigami
import org.kde.ringkde.basicview 1.0 as BasicView
import org.kde.ringkde.jamichatview 1.0 as JamiChatView
import org.kde.ringkde.jamicallview 1.0 as JamiCallView
import net.lvindustries.ringqtquick 1.0 as RingQtQuick
import net.lvindustries.ringqtquick.media 1.0 as RingQtMedia

Kirigami.Page {
    spacing: 0
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    padding: 0
    globalToolBarStyle: Kirigami.ApplicationHeaderStyle.ToolBar
    Kirigami.Theme.colorSet: Kirigami.Theme.View

    titleDelegate: BasicView.DesktopHeader {
        id: dheader
        visible: fits
        Layout.fillWidth: true
        Component.onCompleted: _fits = fits
        onFitsChanged: _fits = fits
    }

    function getCall(cm) {
        return mainPage.call && mainPage.call.lifeCycleState != RingQtQuick.Call.FINISHED ?
            mainPage.call : RingSession.callModel.dialingCall(cm)
    }

    function getDefaultCm() {
        if (mainPage.currentContactMethod)
            return mainPage.currentContactMethod

        if (mainPage.currentIndividual)
            return mainPage.currentIndividual.mainContactMethod

        return null
    }

    function audioCall() {
        var cm = getDefaultCm()

        if (cm.hasInitCall) {
            mainPage.showCall(cm.firstActiveCall)
            return
        }

        var call = getCall(cm)

        call.performAction(RingQtQuick.Call.ACCEPT)
    }

    function videoCall() {
        var cm = getDefaultCm()

        if (cm.hasInitCall) {
            mainPage.showCall(cm.firstActiveCall)
            return
        }

        var call = getCall(cm)

        call.performAction(RingQtQuick.Call.ACCEPT)
    }

    function screencast() {
        var cm = getDefaultCm()

        if (cm.hasInitCall) {
            mainPage.showCall(cm.firstActiveCall)
            return
        }

        var call = getCall(cm)

        call.performAction(RingQtQuick.Call.ACCEPT)
    }

    JamiCallView.CallView {
        id: callview
        anchors.fill: parent
        mode: "CONVERSATION"
        call: mainPage.call

        Connections {
            target: mainPage
            onCallChanged: {
                callview.call = mainPage.call
            }
        }

        onCallWithAudio: {
            var cm = getDefaultCm()

            if (!cm)
                return

            audioCall()
        }
        onCallWithVideo: {
            var cm = getDefaultCm()

            if (!cm)
                return

            videoCall()
        }
        onCallWithScreen: {
            var cm = getDefaultCm()

            if (!cm)
                return

            screencast()
        }
    }

    actions {
        main : actionCollection.chatAction
    }

    // Not worth it on mobile
    contextualActions: Kirigami.Settings.isMobile ? [] : [
        ActionCollection.holdAction        ,
        ActionCollection.recordAction      ,
        ActionCollection.muteCaptureAction ,
        ActionCollection.mutePlaybackAction,
        ActionCollection.hangupAction      ,
        ActionCollection.transferAction    ,
        ActionCollection.acceptAction      ,
        ActionCollection.newCallAction
    ]
}