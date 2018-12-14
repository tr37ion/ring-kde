/***************************************************************************
*   Copyright (C) 2018 by Bluesystems                                     *
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
import QtQuick 2.8
import org.kde.kirigami 2.5 as Kirigami
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.0
import RingQmlWidgets 1.0
import TimeLine 1.0
import org.kde.playground.kquickitemviews 1.0 as KQuickItemViews

Page {
    function save() {
        if (contactInfo.isChanged) {
            contactInfo.save()
        }
    }

    ContactInfo {
        id: contactInfo
        anchors.fill: parent
        showStat: false
        showImage: true
        showSave: true
        individual: profile ? profile.individual : null
        forcedState: "profile"
    }
}
