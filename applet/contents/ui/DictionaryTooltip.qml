/*
    SPDX-FileCopyrightText: 2026 LXYan <z00823823@outlook.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

ColumnLayout {
    id: root

    required property var dictEntry
    property int preferredTextWidth: Kirigami.Units.gridUnit * 20

    Layout.fillWidth: true
    Layout.minimumWidth: Math.min(implicitWidth, preferredTextWidth)
    Layout.preferredWidth: preferredTextWidth
    Layout.maximumWidth: preferredTextWidth
    spacing: Kirigami.Units.smallSpacing

    Kirigami.Heading {
        Layout.minimumWidth: Math.min(implicitWidth, root.preferredTextWidth)
        Layout.maximumWidth: root.preferredTextWidth

        level: 3
        elide: Text.ElideRight
        text: root.dictEntry.matchedWord ?? ""
        textFormat: Text.PlainText
        visible: root.dictEntry.isSplitEntry !== true
    }

    PlasmaComponents.Label {
        Layout.minimumWidth: Math.min(implicitWidth, root.preferredTextWidth)
        Layout.maximumWidth: root.preferredTextWidth
        maximumLineCount: 16
        wrapMode: Text.Wrap

        text: root.dictEntry.phonetic !== undefined
            && root.dictEntry.phonetic.length > 0
            ? `/${root.dictEntry.phonetic}/`
            : ""
        textFormat: Text.PlainText
        visible: text.length > 0
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.minimumWidth: Math.min(implicitWidth, root.preferredTextWidth)
        Layout.preferredWidth: root.preferredTextWidth
        Layout.maximumWidth: root.preferredTextWidth
        spacing: Kirigami.Units.smallSpacing
        visible: root.dictEntry.translationRows !== undefined
            && root.dictEntry.translationRows.length > 0

        Repeater {
            model: root.dictEntry.translationRows ?? []

            RowLayout {
                required property var modelData

                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        Layout.topMargin: Kirigami.Units.smallSpacing
                        visible: modelData.word !== undefined
                            && modelData.word.length > 0
                            && modelData.isFirstWord !== true
                        color: Kirigami.Theme.disabledTextColor
                        opacity: 0.35
                    }

                    Kirigami.Heading {
                        Layout.fillWidth: true
                        visible: modelData.word !== undefined && modelData.word.length > 0
                        level: 4
                        text: modelData.word ?? ""
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: modelData.isWarning !== true
                            && modelData.isNote !== true
                        spacing: Kirigami.Units.smallSpacing

                        PlasmaComponents.Label {
                            Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 1.6
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
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }
                    }

                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        visible: modelData.isNote === true
                        text: modelData.text
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }

                    Kirigami.Heading {
                        Layout.fillWidth: true
                        level: 4
                        visible: modelData.isWarning === true
                        text: modelData.text
                        color: Kirigami.Theme.negativeTextColor
                        font.bold: true
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.minimumWidth: Math.min(implicitWidth, root.preferredTextWidth)
        Layout.preferredWidth: root.preferredTextWidth
        Layout.maximumWidth: root.preferredTextWidth
        Layout.topMargin: root.dictEntry.translationRows !== undefined
            && root.dictEntry.translationRows.length > 0
            ? Kirigami.Units.smallSpacing
            : 0
        spacing: Kirigami.Units.smallSpacing
        visible: root.dictEntry.definitionRows !== undefined
            && root.dictEntry.definitionRows.length > 0

        Repeater {
            model: root.dictEntry.definitionRows ?? []

            ColumnLayout {
                required property var modelData

                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    visible: modelData.isNote === true
                    text: modelData.text
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: modelData.isNote !== true
                    spacing: Kirigami.Units.smallSpacing

                    PlasmaComponents.Label {
                        Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 1.6
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
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }
                }
            }
        }
    }

    PlasmaComponents.Label {
        Layout.minimumWidth: Math.min(implicitWidth, root.preferredTextWidth)
        Layout.maximumWidth: root.preferredTextWidth
        maximumLineCount: 8
        wrapMode: Text.Wrap

        text: root.dictEntry.definition ?? ""
        textFormat: Text.PlainText
        visible: text.length > 0
            && (root.dictEntry.definitionRows === undefined
                || root.dictEntry.definitionRows.length === 0)
    }

    ColumnLayout {
        id: exchangeRowsLayout

        property real labelWidth: {
            let width = 0;
            const rows = root.dictEntry.exchangeRows ?? [];
            for (let i = 0; i < rows.length; i++) {
                width = Math.max(width, labelMetrics.advanceWidth(rows[i].label ?? ""));
            }
            return Math.ceil(width);
        }

        Layout.fillWidth: true
        Layout.minimumWidth: Math.min(implicitWidth, root.preferredTextWidth)
        Layout.preferredWidth: root.preferredTextWidth
        Layout.maximumWidth: root.preferredTextWidth
        Layout.topMargin: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing
        visible: root.dictEntry.exchangeRows !== undefined
            && root.dictEntry.exchangeRows.length > 0

        FontMetrics {
            id: labelMetrics
        }

        Repeater {
            model: root.dictEntry.exchangeRows ?? []

            RowLayout {
                required property var modelData

                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents.Label {
                    Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                    Layout.preferredWidth: exchangeRowsLayout.labelWidth
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
}
