/*
    SPDX-FileCopyrightText: 2026 LXYan <z00823823@outlook.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

pragma ComponentBehavior: Bound

import QtQuick

import com.github.LXYan2333.SwanDict

Item {
    id: root

    required property var configuration
    property bool expanded: false
    readonly property alias dictEntry: state.dictEntry
    readonly property alias sentenceTranslation: state.sentenceTranslation
    readonly property alias sentenceTranslationError: state.sentenceTranslationError
    readonly property alias sentenceTranslationBusy: state.sentenceTranslationBusy
    readonly property alias sentenceTranslationRequested: state.sentenceTranslationRequested
    readonly property bool hasDictEntry: state.dictEntry
        && state.dictEntry.summary !== undefined
        && state.dictEntry.summary.length > 0
        && state.dictEntry.fullText !== undefined
        && state.dictEntry.fullText.length > 0
    readonly property bool canTranslateSelectedSentence: hasDictEntry
        && state.dictEntry.isSplitEntry === true
        && configuration.enableDeepSeekSentenceTranslation
        && (configuration.deepSeekApiKey ?? "").trim().length > 0

    QtObject {
        id: state
        property var dictEntry: ({})
        property string sentenceTranslation: ""
        property string sentenceTranslationError: ""
        property string sentenceTranslationQuery: ""
        property bool sentenceTranslationBusy: false
        property bool sentenceTranslationRequested: false
    }

    SelectionWatcher {
        id: selectionWatcher
        interval: 500
        kWinMouseClearingSuspended: root.expanded
    }

    Translator {
        id: translator
        databaseUrl: Qt.resolvedUrl("../data/ecdict.sqlite")
        maximumSelectionLength: root.configuration.maximumSelectionLength
        dateReplacementMaximumLength: root.configuration.dateReplacementTranslationMaximumLength
    }

    function ignoreNextKWinMouseClick(timeoutMs: int) {
        selectionWatcher.ignoreNextKWinMouseClick(timeoutMs);
    }

    function clearCurrentSelection() {
        selectionWatcher.clearCurrentSelection();
    }

    function startKWinMouseHelper() {
        selectionWatcher.startKWinMouseHelper();
    }

    function clearSentenceTranslation() {
        state.sentenceTranslation = "";
        state.sentenceTranslationError = "";
        state.sentenceTranslationQuery = "";
        state.sentenceTranslationBusy = false;
        state.sentenceTranslationRequested = false;
        translator.cancelSentenceTranslation();
    }

    function requestSentenceTranslation() {
        if (!root.canTranslateSelectedSentence) {
            return;
        }

        const watcherText = selectionWatcher.text ?? "";
        const query = watcherText.length > 0
            ? watcherText
            : (state.dictEntry.query ?? state.dictEntry.matchedWord ?? "");
        if (query.length === 0) {
            return;
        }

        state.sentenceTranslationRequested = true;
        state.sentenceTranslation = "";
        state.sentenceTranslationError = "";
        state.sentenceTranslationQuery = query;
        state.sentenceTranslationBusy = true;
        translator.translateSentenceWithDeepSeek(query, root.configuration.deepSeekApiKey ?? "");
    }

    function updateDictEntry() {
        state.dictEntry = translator.lookup(selectionWatcher.text);
        root.clearSentenceTranslation();
    }

    Connections {
        target: root.configuration
        function onStartKWinMouseHelperChanged() {
            if (root.configuration.startKWinMouseHelper) {
                root.startKWinMouseHelper();
            }
        }
    }

    Connections {
        target: selectionWatcher
        function onTextChanged() {
            if (root.expanded) {
                return;
            }
            root.updateDictEntry();
        }
    }

    Connections {
        target: translator
        function onSentenceTranslationReady(selectedText, translation) {
            if (selectedText !== state.sentenceTranslationQuery) {
                return;
            }

            state.sentenceTranslation = translation;
            state.sentenceTranslationError = "";
            state.sentenceTranslationBusy = false;
            state.sentenceTranslationRequested = false;
        }

        function onSentenceTranslationFailed(selectedText, message) {
            if (selectedText !== state.sentenceTranslationQuery) {
                return;
            }

            state.sentenceTranslation = "";
            state.sentenceTranslationError = message;
            state.sentenceTranslationBusy = false;
            state.sentenceTranslationRequested = false;
        }
    }

    onExpandedChanged: {
        if (!expanded) {
            root.clearSentenceTranslation();
            root.clearCurrentSelection();
        }
    }

    Component.onCompleted: {
        if (root.configuration.startKWinMouseHelper) {
            root.startKWinMouseHelper();
        }
        root.updateDictEntry();
    }
}
