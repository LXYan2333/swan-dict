/*
    SPDX-FileCopyrightText: 2026 LXYan <z00823823@outlook.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import com.github.LXYan2333.SwanDict

Item {
    id: rootItem

    property var dictEntry: ({})
    property string sentenceTranslation: ""
    property bool sentenceTranslationBusy: false
    property string sentenceTranslationError: ""
    property bool sentenceTranslationAvailable: false
    property bool sentenceTranslationRequested: false

    signal requestSentenceTranslation()

    ClipboardWriter {
        id: clipboardWriter
    }

    function copyTextForEntry(entry): string {
        const parts = [];
        const word = entry.matchedWord ?? "";
        const phonetic = entry.phonetic ?? "";
        if (word.length > 0) {
            parts.push(word);
        }
        if (phonetic.length > 0) {
            parts.push(`[${phonetic}]`);
        }
        if ((entry.translation ?? "").length > 0) {
            parts.push(entry.translation);
        }
        if ((entry.definition ?? "").length > 0) {
            parts.push(entry.definition);
        }

        const exchangeRows = entry.exchangeRows ?? [];
        if (exchangeRows.length > 0) {
            const exchangeLines = [];
            for (let i = 0; i < exchangeRows.length; i++) {
                exchangeLines.push(`${exchangeRows[i].label}: ${exchangeRows[i].value}`);
            }
            parts.push(exchangeLines.join("\n"));
        }

        return parts.filter(value => value.length > 0).join("\n\n");
    }

    function copyEntry(entry) {
        clipboardWriter.setText(rootItem.copyTextForEntry(entry));
    }

    component PartOfSpeechRows: ColumnLayout {
        required property var rows
        property real posWidth: Kirigami.Units.gridUnit * 1.6
        property bool showWarnings: true

        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing
        visible: rows !== undefined
            && rows.some(row => showWarnings || row.isWarning !== true)

        Repeater {
            model: rows ?? []

            ColumnLayout {
                required property var modelData

                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                visible: showWarnings || modelData.isWarning !== true

                Kirigami.Heading {
                    Layout.fillWidth: true
                    level: 4
                    visible: modelData.isWarning === true
                    text: modelData.text
                    color: Kirigami.Theme.negativeTextColor
                    font.bold: true
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: modelData.isWarning !== true
                    spacing: Kirigami.Units.smallSpacing

                    PlasmaComponents.Label {
                        Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                        Layout.preferredWidth: posWidth
                        text: modelData.pos
                        opacity: 0.5
                        textFormat: Text.PlainText
                    }

                    Item {
                        Layout.preferredWidth: Kirigami.Units.smallSpacing
                    }

                    PlasmaComponents.Label {
                        Layout.alignment: Qt.AlignTop
                        Layout.fillWidth: true
                        text: modelData.text
                        wrapMode: Text.WordWrap
                        textFormat: Text.PlainText
                    }
                }
            }
        }
    }

    component ExchangeRows: ColumnLayout {
        required property var rows
        property real labelWidth: {
            let width = 0;
            for (let i = 0; rows !== undefined && i < rows.length; i++) {
                width = Math.max(width, labelMetrics.advanceWidth(rows[i].label ?? ""));
            }
            return Math.ceil(width);
        }

        Layout.fillWidth: true
        Layout.topMargin: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing
        visible: rows !== undefined && rows.length > 0

        FontMetrics {
            id: labelMetrics
        }

        Repeater {
            model: rows ?? []

            RowLayout {
                required property var modelData

                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents.Label {
                    Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                    Layout.preferredWidth: labelWidth
                    text: modelData.label
                    opacity: 0.7
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                PlasmaComponents.Label {
                    Layout.alignment: Qt.AlignTop
                    Layout.fillWidth: true
                    text: modelData.value
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
            }
        }
    }

    component DictionaryEntryContent: ColumnLayout {
        required property var entry
        property bool showHeading: false

        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true
            visible: showHeading

            Kirigami.Heading {
                Layout.fillWidth: true
                level: 3
                elide: Text.ElideRight
                text: entry.matchedWord ?? ""
                textFormat: Text.PlainText
            }

            QQC2.Button {
                icon.name: "edit-paste"
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: i18n("Copy dictionary entry")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                onClicked: rootItem.copyEntry(entry)
            }
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            visible: showHeading && entry.phonetic !== undefined && entry.phonetic.length > 0
            text: entry.phonetic !== undefined ? `[${entry.phonetic}]` : ""
            opacity: 0.7
            textFormat: Text.PlainText
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            visible: entry.translationRows === undefined
                || entry.translationRows.length === 0
            text: [entry.translation, entry.definition]
                .filter(value => value !== undefined && value.length > 0)
                .join("\n\n")
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
        }

        PartOfSpeechRows {
            rows: entry.translationRows ?? []
            showWarnings: false
        }

        PartOfSpeechRows {
            rows: entry.definitionRows ?? []
            showWarnings: false
        }

        ExchangeRows {
            rows: entry.exchangeRows ?? []
        }
    }

    readonly property bool hasPopupEntries: dictEntry.popupEntries !== undefined
        && dictEntry.popupEntries.length > 0

    Layout.minimumWidth: Kirigami.Units.gridUnit * 20
    Layout.minimumHeight: Kirigami.Units.gridUnit * 12
    Layout.preferredWidth: Kirigami.Units.gridUnit * 28
    Layout.preferredHeight: Kirigami.Units.gridUnit * 20

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true
            visible: !rootItem.hasPopupEntries

            Kirigami.Heading {
                Layout.fillWidth: true
                level: 2
                elide: Text.ElideRight
                text: rootItem.dictEntry.matchedWord ?? ""
                textFormat: Text.PlainText
            }

            QQC2.Button {
                visible: !rootItem.hasPopupEntries
                icon.name: "edit-paste"
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: i18n("Copy dictionary entry")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                onClicked: rootItem.copyEntry(rootItem.dictEntry)
            }
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            visible: rootItem.dictEntry.phonetic !== undefined && rootItem.dictEntry.phonetic.length > 0
            text: rootItem.dictEntry.phonetic !== undefined ? `[${rootItem.dictEntry.phonetic}]` : ""
            opacity: 0.7
            textFormat: Text.PlainText
        }

        QQC2.ScrollView {
            id: contentScrollView

            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: contentScrollView.availableWidth
                spacing: Kirigami.Units.smallSpacing

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing
                    visible: rootItem.hasPopupEntries
                        && (rootItem.sentenceTranslationAvailable
                            || rootItem.sentenceTranslationBusy
                            || rootItem.sentenceTranslation.length > 0
                            || rootItem.sentenceTranslationError.length > 0)

                    QQC2.Button {
                        Layout.alignment: Qt.AlignLeft
                        visible: rootItem.sentenceTranslationAvailable
                        enabled: !rootItem.sentenceTranslationBusy
                        text: rootItem.sentenceTranslation.length > 0
                            || rootItem.sentenceTranslationError.length > 0
                            ? i18n("Translate again with DeepSeek")
                            : i18n("Translate with DeepSeek")
                        onClicked: rootItem.requestSentenceTranslation()
                    }

                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        visible: rootItem.sentenceTranslationBusy
                        text: i18n("Translating selected text...")
                        opacity: 0.7
                        textFormat: Text.PlainText
                    }

                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        visible: !rootItem.sentenceTranslationBusy
                            && rootItem.sentenceTranslation.length > 0
                        text: rootItem.sentenceTranslation
                        wrapMode: Text.WordWrap
                        textFormat: Text.PlainText
                    }

                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        visible: !rootItem.sentenceTranslationBusy
                            && rootItem.sentenceTranslationError.length > 0
                        text: rootItem.sentenceTranslationError
                        opacity: 0.7
                        wrapMode: Text.WordWrap
                        textFormat: Text.PlainText
                    }

                    Kirigami.Separator {
                        Layout.fillWidth: true
                    }
                }

                DictionaryEntryContent {
                    visible: !rootItem.hasPopupEntries
                    entry: rootItem.dictEntry
                    showHeading: false
                }

                Repeater {
                    model: rootItem.hasPopupEntries ? rootItem.dictEntry.popupEntries : []

                    ColumnLayout {
                        required property var modelData
                        required property int index

                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        DictionaryEntryContent {
                            entry: modelData
                            showHeading: true
                        }

                        Kirigami.Separator {
                            Layout.fillWidth: true
                            visible: index < rootItem.dictEntry.popupEntries.length - 1
                        }
                    }
                }

                Kirigami.Heading {
                    Layout.fillWidth: true
                    level: 4
                    visible: rootItem.dictEntry.truncationNote !== undefined
                        && rootItem.dictEntry.truncationNote.length > 0
                    text: rootItem.dictEntry.truncationNote ?? ""
                    color: Kirigami.Theme.negativeTextColor
                    font.bold: true
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                }
            }
        }
    }
}
