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

    readonly property string swanDictTranslationDomain: "plasma_applet_com.github.LXYan2333.swan-dict"

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

    function escapeHtml(value): string {
        return `${value ?? ""}`
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/"/g, "&quot;")
            .replace(/'/g, "&#39;");
    }

    function htmlParagraphs(text): string {
        return `${text ?? ""}`
            .split(/\n+/)
            .filter(value => value.length > 0)
            .map(value => `<p>${rootItem.escapeHtml(value)}</p>`)
            .join("");
    }

    function rowsHtml(rows): string {
        if (rows === undefined || rows.length === 0) {
            return "";
        }

        const rowHtml = [];
        for (let i = 0; i < rows.length; i++) {
            const row = rows[i];
            if (row.isWarning === true) {
                rowHtml.push(`<tr><td colspan="2" style="color:#b00020;font-weight:700">${rootItem.escapeHtml(row.text)}</td></tr>`);
                continue;
            }

            rowHtml.push("<tr>"
                + `<td style="vertical-align:top;white-space:nowrap;color:#777;padding-right:0.6em">${rootItem.escapeHtml(row.pos)}</td>`
                + `<td style="vertical-align:top">${rootItem.escapeHtml(row.text)}</td>`
                + "</tr>");
        }

        return `<table style="border-collapse:collapse;margin:0.35em 0">${rowHtml.join("")}</table>`;
    }

    function exchangeHtml(rows): string {
        if (rows === undefined || rows.length === 0) {
            return "";
        }

        const rowHtml = [];
        for (let i = 0; i < rows.length; i++) {
            const row = rows[i];
            rowHtml.push("<tr>"
                + `<td style="vertical-align:top;white-space:nowrap;color:#777;padding-right:0.6em">${rootItem.escapeHtml(row.label)}</td>`
                + `<td style="vertical-align:top">${rootItem.escapeHtml(row.value)}</td>`
                + "</tr>");
        }

        return `<table style="border-collapse:collapse;margin:0.55em 0 0">${rowHtml.join("")}</table>`;
    }

    function copyTextForEntry(entry): string {
        const parts = [];
        const word = entry.matchedWord ?? "";
        const phonetic = entry.phonetic ?? "";
        if (word.length > 0) {
            parts.push(word);
        }
        if (phonetic.length > 0) {
            parts.push(`/${phonetic}/`);
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

    function copyHtmlForEntry(entry): string {
        const parts = [];
        const word = entry.matchedWord ?? "";
        const phonetic = entry.phonetic ?? "";
        if (word.length > 0) {
            parts.push(`<h3 style="margin:0 0 0.25em">${rootItem.escapeHtml(word)}</h3>`);
        }
        if (phonetic.length > 0) {
            parts.push(`<p style="margin:0.15em 0 0.55em;color:#777">/${rootItem.escapeHtml(phonetic)}/</p>`);
        }

        const translationRows = entry.translationRows ?? [];
        const definitionRows = entry.definitionRows ?? [];
        if (translationRows.length > 0) {
            parts.push(rootItem.rowsHtml(translationRows));
        } else if ((entry.translation ?? "").length > 0) {
            parts.push(rootItem.htmlParagraphs(entry.translation));
        }

        if (definitionRows.length > 0) {
            parts.push(rootItem.rowsHtml(definitionRows));
        } else if ((entry.definition ?? "").length > 0) {
            parts.push(rootItem.htmlParagraphs(entry.definition));
        }

        parts.push(rootItem.exchangeHtml(entry.exchangeRows ?? []));
        return `<div>${parts.filter(value => value.length > 0).join("")}</div>`;
    }

    function copyEntry(entry) {
        clipboardWriter.setRichText(
            rootItem.copyTextForEntry(entry),
            rootItem.copyHtmlForEntry(entry)
        );
    }

    function tr(message) {
        return i18nd(swanDictTranslationDomain, message);
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
                        && modelData.isNote !== true
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

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    visible: modelData.isNote === true
                    text: modelData.text
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
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
                QQC2.ToolTip.text: rootItem.tr("Copy dictionary entry")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                onClicked: rootItem.copyEntry(entry)
            }
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            visible: showHeading && entry.phonetic !== undefined && entry.phonetic.length > 0
            text: entry.phonetic !== undefined ? `/${entry.phonetic}/` : ""
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
            Layout.topMargin: entry.translationRows !== undefined
                && entry.translationRows.length > 0
                ? Kirigami.Units.smallSpacing
                : 0
        }

        ExchangeRows {
            rows: entry.exchangeRows ?? []
        }
    }

    readonly property bool hasPopupEntries: dictEntry.popupEntries !== undefined
        && dictEntry.popupEntries.length > 0

    Layout.minimumWidth: Kirigami.Units.gridUnit * 22
    Layout.minimumHeight: Kirigami.Units.gridUnit * 12
    Layout.preferredWidth: Kirigami.Units.gridUnit * 22
    Layout.preferredHeight: Kirigami.Units.gridUnit * 18

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
                QQC2.ToolTip.text: rootItem.tr("Copy dictionary entry")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                onClicked: rootItem.copyEntry(rootItem.dictEntry)
            }
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            visible: rootItem.dictEntry.phonetic !== undefined && rootItem.dictEntry.phonetic.length > 0
            text: rootItem.dictEntry.phonetic !== undefined ? `/${rootItem.dictEntry.phonetic}/` : ""
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
                            ? rootItem.tr("Translate again with DeepSeek")
                            : rootItem.tr("Translate with DeepSeek")
                        onClicked: rootItem.requestSentenceTranslation()
                    }

                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        visible: rootItem.sentenceTranslationBusy
                        text: rootItem.tr("Translating selected text...")
                        opacity: 0.7
                        textFormat: Text.PlainText
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: !rootItem.sentenceTranslationBusy
                            && rootItem.sentenceTranslation.length > 0

                        QQC2.Button {
                            Layout.alignment: Qt.AlignTop
                            icon.name: "edit-paste"
                            display: QQC2.AbstractButton.IconOnly
                            QQC2.ToolTip.text: rootItem.tr("Copy DeepSeek translation")
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                            onClicked: clipboardWriter.setText(rootItem.sentenceTranslation)
                        }

                        PlasmaComponents.Label {
                            Layout.fillWidth: true
                            text: rootItem.sentenceTranslation
                            wrapMode: Text.WordWrap
                            textFormat: Text.PlainText
                        }
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
