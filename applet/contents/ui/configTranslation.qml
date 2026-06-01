/*
    SPDX-FileCopyrightText: 2026 LXYan <z00823823@outlook.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kcmutils as KCMUtils
import org.kde.kirigami as Kirigami

KCMUtils.SimpleKCM {
    id: translationPage

    property alias cfg_enableDeepSeekSentenceTranslation: enableDeepSeekSentenceTranslation.checked
    property alias cfg_deepSeekApiKey: deepSeekApiKey.text
    property alias cfg_dateReplacementTranslationMaximumLength: dateReplacementTranslationMaximumLength.value
    property alias cfg_maximumSelectionLength: maximumSelectionLength.value

    Kirigami.FormLayout {
        QQC2.SpinBox {
            id: maximumSelectionLength
            Kirigami.FormData.label: i18n("Dictionary selection limit:")
            from: 4
            to: 512
            stepSize: 1
            textFromValue: value => i18np("%1 character", "%1 characters", value)
            valueFromText: text => parseInt(text)
        }

        QQC2.SpinBox {
            id: dateReplacementTranslationMaximumLength
            Kirigami.FormData.label: i18n("Date replacement length:")
            from: 4
            to: 128
            stepSize: 1
            textFromValue: value => i18np("%1 character", "%1 characters", value)
            valueFromText: text => parseInt(text)
        }

        Item {
            Kirigami.FormData.isSection: true
        }

        QQC2.CheckBox {
            id: enableDeepSeekSentenceTranslation
            Kirigami.FormData.label: i18n("Sentence translation:")
            text: i18n("Use DeepSeek when opening the popup for multi-word selections")
        }

        QQC2.TextField {
            id: deepSeekApiKey
            Layout.fillWidth: true
            Kirigami.FormData.label: i18n("DeepSeek API key:")
            enabled: enableDeepSeekSentenceTranslation.checked
            echoMode: TextInput.Password
            placeholderText: i18n("sk-...")
        }
    }
}
