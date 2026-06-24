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

    readonly property string swanDictTranslationDomain: "plasma_applet_com.github.LXYan2333.swan-dict"
    function tr(message, arg1) {
        if (arguments.length === 2) {
            return i18nd(swanDictTranslationDomain, message, arg1);
        }
        return i18nd(swanDictTranslationDomain, message);
    }
    function trp(singular, plural, value) {
        return i18ndp(swanDictTranslationDomain, singular, plural, value);
    }

    property alias cfg_enableDeepSeekSentenceTranslation: enableDeepSeekSentenceTranslation.checked
    property alias cfg_deepSeekApiKey: deepSeekApiKey.text
    property alias cfg_dateReplacementTranslationMaximumLength: dateReplacementTranslationMaximumLength.value
    property alias cfg_dateReplacementFixedWidth: dateReplacementFixedWidth.value
    property alias cfg_maximumSelectionLength: maximumSelectionLength.value
    property alias cfg_startKWinMouseHelper: startKWinMouseHelper.checked

    Kirigami.FormLayout {
        Component {
            id: helpTextComponent

            QQC2.Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.disabledTextColor
                textFormat: Text.PlainText
            }
        }

        QQC2.SpinBox {
            id: maximumSelectionLength
            Kirigami.FormData.label: translationPage.tr("Dictionary selection limit:")
            from: 4
            to: 512
            stepSize: 1
            textFromValue: value => translationPage.trp("%1 character", "%1 characters", value)
            valueFromText: text => parseInt(text)
        }

        Loader {
            Layout.fillWidth: true
            sourceComponent: helpTextComponent
            property string helpText: translationPage.tr("Maximum selected-text length used for local dictionary lookup. If the selection is longer, Swan Dict translates up to this limit but extends to the end of the current word.")
            onLoaded: item.text = helpText
        }

        QQC2.SpinBox {
            id: dateReplacementTranslationMaximumLength
            Kirigami.FormData.label: translationPage.tr("Date replacement length:")
            from: 4
            to: 128
            stepSize: 1
            textFromValue: value => translationPage.trp("%1 character", "%1 characters", value)
            valueFromText: text => parseInt(text)
        }

        Loader {
            Layout.fillWidth: true
            sourceComponent: helpTextComponent
            property string helpText: translationPage.tr("Maximum number of characters shown in the panel date area. Longer summaries are truncated with an ellipsis.")
            onLoaded: item.text = helpText
        }

        QQC2.SpinBox {
            id: dateReplacementFixedWidth
            Kirigami.FormData.label: translationPage.tr("Date replacement width:")
            from: 0
            to: 512
            stepSize: 1
            textFromValue: value => value === 0 ? translationPage.tr("Automatic") : translationPage.tr("%1 px", value)
            valueFromText: text => parseInt(text) || 0
        }

        Loader {
            Layout.fillWidth: true
            sourceComponent: helpTextComponent
            property string helpText: translationPage.tr("Fixed pixel width reserved for the date or translation in the panel. Use Automatic to let the widget size itself like the original Digital Clock.")
            onLoaded: item.text = helpText
        }

        QQC2.CheckBox {
            id: startKWinMouseHelper
            Kirigami.FormData.label: translationPage.tr("KWin mouse helper:")
            text: translationPage.tr("Start the optional KWin helper when this widget starts")
        }

        Loader {
            Layout.fillWidth: true
            sourceComponent: helpTextComponent
            property string helpText: translationPage.tr("The helper lets the widget notice mouse clicks outside itself on Wayland, so stale selected-text translations can return to the normal date sooner. The widget still works without it.")
            onLoaded: item.text = helpText
        }

        Item {
            Kirigami.FormData.isSection: true
        }

        QQC2.CheckBox {
            id: enableDeepSeekSentenceTranslation
            Kirigami.FormData.label: translationPage.tr("Sentence translation:")
            text: translationPage.tr("Use DeepSeek when opening the popup for multi-word selections")
        }

        Loader {
            Layout.fillWidth: true
            sourceComponent: helpTextComponent
            property string helpText: translationPage.tr("Allows manual AI translation for multi-word selections in the click popup. It is not triggered on hover or automatically when the popup opens.")
            onLoaded: item.text = helpText
        }

        QQC2.TextField {
            id: deepSeekApiKey
            Layout.fillWidth: true
            Kirigami.FormData.label: translationPage.tr("DeepSeek API key:")
            enabled: enableDeepSeekSentenceTranslation.checked
            echoMode: TextInput.Password
            placeholderText: translationPage.tr("sk-...")
        }

        Loader {
            Layout.fillWidth: true
            sourceComponent: helpTextComponent
            property string helpText: translationPage.tr("API key used only when you press the DeepSeek translation button. It is stored in the widget configuration, not in KWallet.")
            onLoaded: item.text = helpText
        }
    }
}
