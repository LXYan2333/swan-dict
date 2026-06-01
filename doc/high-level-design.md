# Swan Dict High-Level Design

## Overview

Swan Dict is a KDE Plasma 6 applet based on KDE's official Digital Clock. It preserves the clock, calendar popup, timezone support, and appearance settings, while replacing the date label with a compact translation summary of the user's current primary selection when a translation is available. The full dictionary entry is shown in the hover tooltip and in the click popup.

The applet is packaged separately from the official clock under:

```text
com.github.LXYan2333.swan-dict
```

The design keeps the Digital Clock behavior intact and adds translation as a secondary-label provider.

## Design Goals

- Keep the official Digital Clock implementation as the behavioral base.
- Limit applet-side changes to the date label path.
- Read primary selection on KDE Plasma 6 Wayland and X11.
- Translate locally with ECDICT data stored in SQLite.
- Fall back to the normal date whenever selection or translation is unavailable.
- Keep QML free of platform-specific selection and dictionary details.

## Out of Scope

- A full dictionary UI.
- Online translation.
- Long paragraph translation.
- Reimplementation of Digital Clock layout, timekeeping, calendar, or timezone behavior.
- Vendoring `wl-clipboard` source or converting it into a library.

## System Context

```text
User selection
    |
    v
SelectionWatcher
    |
    v
Translator
    |
    v
DigitalClock date label
```

Swan Dict runs inside Plasma as a normal applet. The visible applet remains the Digital Clock compact representation. The only user-visible change is that the date label may show a translation instead of the formatted date.

## Components

### Plasma Applet Package

The applet package contains the QML UI and configuration metadata:

```text
applet/
  metadata.json
  contents/
    config/
      config.qml
      main.xml
    ui/
      main.qml
      DigitalClock.qml
      CalendarView.qml
      Tooltip.qml
      NoTimezoneWarning.qml
      configAppearance.qml
      configCalendar.qml
      configTimeZones.qml
```

The package is derived from:

```text
/usr/share/plasma/plasmoids/org.kde.plasma.digitalclock
```

The inherited `contents/config/main.xml` is required because the official clock QML reads Digital Clock settings through `Plasmoid.configuration`.

### Digital Clock QML

`DigitalClock.qml` owns the panel label layout. The design changes only the source of `dateLabel.text`.

Official behavior:

```qml
dateLabel.text = dateFormatter(getCurrentTime());
```

Swan Dict behavior:

```qml
dateLabel.text = main.secondaryLabelText();
```

`secondaryLabelText()` returns:

- an empty string when `showDate` is disabled
- a translation when selected text has a dictionary hit
- the formatted date otherwise

### Native QML Module

A small native QML module exposes platform and dictionary functionality to QML:

```qml
import com.github.LXYan2333.SwanDict
```

Proposed QML types:

```qml
SelectionWatcher {
    id: selectionWatcher
    interval: 300
}

Translator {
    id: translator
    databasePath: root.dictionaryPath
}
```

The module hides C++ details and external-command details from `DigitalClock.qml`.

### SelectionWatcher

`SelectionWatcher` provides normalized primary-selection text.

Backend selection:

- Wayland: invoke `wl-paste --primary --no-newline --type text/plain`.
- X11: use `QClipboard::Selection`.

Responsibilities:

- Detect the current session backend.
- Read primary selection without blocking the UI.
- Ignore empty, unavailable, binary, or failed reads.
- Emit changes only when normalized text changes.
- Fall back silently when the backend is unavailable.

The first Wayland implementation uses polling. A future version may replace polling with a persistent `wl-paste --primary --watch` bridge or direct `zwlr_data_control_manager_v1` code.

### Translator

`Translator` maps normalized selected text to a structured dictionary result. The compact date label uses a short summary, while the tooltip and popup use the full entry content.

Responsibilities:

- Open the SQLite dictionary database.
- Validate and normalize selected text.
- Query exact words or short phrases.
- Apply simple fallback forms when exact lookup fails.
- Return a concise panel-safe summary string.
- Return full dictionary content for tooltip and popup display.
- Report lookup failure as "no translation" rather than an applet error.

Conceptual result shape:

```text
DictEntry
  query: normalized selected text
  matchedWord: dictionary headword
  summary: compact label text
  fullText: full dictionary content
  phonetic: optional phonetic text
  translation: optional translation text
  definition: optional definition text
```

### Dictionary Database

Dictionary data is stored locally in SQLite.

Minimum required table shape:

```sql
CREATE TABLE stardict (
    word TEXT PRIMARY KEY,
    translation TEXT,
    definition TEXT,
    phonetic TEXT
);
```

These fields are used by the first implementation:

```text
word
translation
definition
phonetic
```

Preferred initial location:

```text
contents/data/ecdict.sqlite
```

This keeps dictionary path resolution simple for the first working version.

The upstream ECDICT project is included as a Git submodule outside the applet package:

```text
third_party/ECDICT
```

This submodule is an import source only. It is not part of the installable Plasma applet directory. Importer scripts should convert ECDICT data into the runtime SQLite database used by the applet.

## Data Flow

### Normal Date Flow

```text
DigitalClock.qml
    -> getCurrentTime()
    -> dateFormatter(...)
    -> dateLabel.text
```

This path remains the fallback behavior.

### Translation Flow

```text
SelectionWatcher
    -> selectedTextChanged(normalizedText)
    -> Translator.lookup(normalizedText)
    -> currentDictEntry updated
    -> DigitalClock.secondaryLabelText()
    -> dateLabel.text
```

If any step fails or returns no result, `secondaryLabelText()` returns the normal date.

### Tooltip Flow

```text
currentDictEntry
    -> Tooltip.qml
    -> full dictionary content on hover
```

When no dictionary entry is active, the tooltip keeps the official Digital Clock behavior.

### Popup Flow

```text
click compact representation
    -> fullRepresentation
    -> dictionary entry view when currentDictEntry exists
    -> calendar view otherwise
```

When a dictionary entry is active, the popup shows the full dictionary content. When no dictionary entry is active, the popup remains the normal calendar.

## Selection Handling

Selected text is accepted only when it is suitable for a compact panel label.

Initial rules:

1. Trim leading and trailing whitespace.
2. Collapse repeated internal whitespace.
3. Reject multi-line selections.
4. Reject selections longer than 64 characters.
5. Strip surrounding punctuation.
6. Lowercase ASCII English text for lookup.

Selections that fail validation are treated as unavailable and the applet shows the date.

## Lookup Behavior

Lookup order:

1. Exact word or phrase.
2. Simple plural fallback, such as `words` to `word`.
3. Simple past-tense fallback, such as `worked` to `work`.
4. Simple gerund fallback, such as `working` to `work`.

The first implementation does not require advanced morphology.

## Display Behavior

The date label follows this priority:

1. If `Plasmoid.configuration.showDate` is false, show nothing.
2. If a valid dictionary entry exists, show its compact summary.
3. Otherwise, show the normal formatted date.

Compact label rules:

- Use the first useful translation line.
- Normalize whitespace.
- Keep the text concise.
- Rely on existing Plasma label fitting and eliding for final layout protection.

Tooltip behavior:

- If a dictionary entry exists, show the full dictionary content.
- If no dictionary entry exists, use the official Digital Clock tooltip.

Popup behavior:

- If a dictionary entry exists, show a dictionary detail view.
- If no dictionary entry exists, show the official calendar popup.

The exact formatting of the dictionary detail view is an open design decision.

## Error Handling

Swan Dict should fail soft.

Expected fallback cases:

- `wl-paste` is not installed.
- `wl-paste` times out or exits non-zero.
- `QClipboard::Selection` is unsupported.
- Selection is empty or too long.
- Dictionary database is missing.
- SQLite lookup fails.
- No dictionary entry exists.

In all cases, the applet continues to show the normal clock and date.

## Build and Runtime Dependencies

Development and runtime packages on Debian 13:

```bash
sudo apt --mark-auto install \
  build-essential cmake extra-cmake-modules \
  qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools qt6-tools-dev \
  libplasma-dev libplasma5support-dev plasma-workspace-dev \
  libkf6config-dev libkf6coreaddons-dev libkf6i18n-dev libkf6package-dev \
  kpackagetool6 libqt6sql6-sqlite wl-clipboard \
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-org-kde-kirigami \
  qml6-module-org-kde-plasma-plasma5support
```

`wl-clipboard` is a runtime dependency for the initial Wayland backend.

## Packaging

The applet package is installed with `kpackagetool6`.

Useful commands:

```bash
kpackagetool6 --type Plasma/Applet --install applet
kpackagetool6 --type Plasma/Applet --upgrade applet
kpackagetool6 --type Plasma/Applet --remove com.github.LXYan2333.swan-dict
```

The native QML module is built with CMake and installed to a location where Plasma can import:

```text
com.github.LXYan2333.SwanDict
```

## Implementation Sequence

1. Vendor the official Digital Clock applet files.
2. Preserve official clock behavior under Swan Dict metadata.
3. Replace the date label source with `secondaryLabelText()`.
4. Add `SelectionWatcher`.
5. Add SQLite-backed `Translator`.
6. Wire translation result into the date label fallback flow.
7. Add packaging and README instructions.

## Verification

Manual verification should cover:

- Applet installation and upgrade.
- Horizontal panel layout.
- Vertical panel layout.
- Normal date fallback.
- Primary selection from KDE apps on Wayland.
- Primary selection on X11 if available.
- Missing `wl-paste`.
- Missing dictionary database.
- Unknown selected word.
- Long selected text.
- Calendar popup behavior.
- Inherited clock configuration pages.

Useful log command:

```bash
journalctl --user -f | grep plasmashell
```

## Known Risks

### Wayland Selection Backend

The first Wayland backend depends on `wl-paste`. This is simpler than direct Wayland protocol code, but it adds an external runtime command and polling overhead.

Mitigation:

- Require `wl-clipboard`.
- Bound every `wl-paste` invocation with a timeout.
- Avoid overlapping reads.
- Keep fallback-to-date behavior.

### Plasma Private APIs

The official clock imports `org.kde.plasma.private.digitalclock`. This is a private Plasma API and may change across Plasma releases.

Mitigation:

- Keep the applet aligned with the installed Plasma Digital Clock source.
- Avoid unnecessary changes to official clock internals.

### Translation Length

Dictionary translations can be longer than the original date.

Mitigation:

- Prefer concise translation output.
- Limit accepted selected text.
- Reuse existing clock label fitting and eliding behavior.

### Dictionary Size

ECDICT is too large to load directly into QML memory.

Mitigation:

- Use SQLite.
- Query by indexed `word`.
- Move lookup off the UI path later if synchronous lookup becomes visible.

## Decisions Already Made

- The applet is based on KDE's official Digital Clock.
- The date label is the replacement target.
- Translation is local and offline.
- ECDICT data is stored in SQLite.
- Wayland primary selection is read through `wl-paste --primary` in the first implementation.
- X11 primary selection uses `QClipboard::Selection`.

## Open Decisions

- Whether translation display should be only Chinese or `word: translation`.
- Whether phrase lookup is required in the first version.
- Whether the dictionary database should be bundled in the applet package or installed separately.
- Whether unknown selected words should show the date or the selected word.
- Whether the Wayland backend should remain polling-based or move to a `wl-paste --primary --watch` bridge.
- Exact formatting for full dictionary content in hover tooltip and click popup.
