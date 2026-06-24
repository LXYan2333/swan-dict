/*
    SPDX-FileCopyrightText: 2026 LXYan <z00823823@outlook.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

pragma ComponentBehavior: Bound

import QtQuick

import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: i18nd("plasma_applet_org.kde.plasma.digitalclock", "Appearance")
        icon: "preferences-desktop-color"
        source: "configAppearance.qml"
    }

    ConfigCategory {
        name: i18nd("plasma_applet_org.kde.plasma.digitalclock", "Calendar")
        icon: "office-calendar"
        source: "configCalendar.qml"
    }

    ConfigCategory {
        name: i18nd("plasma_applet_org.kde.plasma.digitalclock", "Time Zones")
        icon: "preferences-system-time"
        source: "configTimeZones.qml"
    }

    ConfigCategory {
        name: i18nd("plasma_applet_com.github.LXYan2333.swan-dict", "Translation")
        icon: "preferences-desktop-locale"
        source: "configTranslation.qml"
    }
}
