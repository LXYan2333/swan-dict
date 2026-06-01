# Swan Dict Plasma Applet Proposal

## Summary

Swan Dict is a KDE Plasma 6 digital clock applet derived from the official KDE Digital Clock. It keeps the normal clock behavior, calendar popup, timezone handling, and appearance configuration, but replaces the date label with a compact translation summary of the current primary selection when the selected text can be translated. The full dictionary content is shown on hover and in the click popup.

The intended interaction is simple:

1. The user selects an English word or short phrase in any application.
2. The applet reads the primary selection.
3. The date area of the digital clock shows a compact Chinese translation summary.
4. Hovering the date/clock shows the full dictionary content.
5. Clicking the applet opens a popup with the full dictionary content.
6. If there is no useful selection or no dictionary match, the date area and popup fall back to the normal Digital Clock behavior.

Translation is local and offline, using ECDICT data.

## Goals

- Preserve the official KDE Digital Clock user experience.
- Replace only the date display path, not the timekeeping implementation.
- Read the primary selection reliably on Plasma 6.
- Translate selected English words and short phrases without network access.
- Keep the panel layout stable in horizontal, vertical, and desktop applet form factors.
- Package the applet under a separate plugin id: `com.github.LXYan2333.swan-dict`.

## Non-Goals

- Reimplementing the digital clock from scratch.
- Providing a full dictionary application UI.
- Sending selected text to online translation services.
- Replacing KDE's clipboard manager.
- Supporting long paragraph translation in the panel label.

## Current State

The repository currently contains a minimal Plasma applet package under `applet/`:

- `applet/metadata.json`
- `applet/contents/ui/main.qml`
- `README.md`

The installed system digital clock is available at:

```text
/usr/share/plasma/plasmoids/org.kde.plasma.digitalclock
```

The official applet contains the needed Plasma 6 implementation files:

- `contents/ui/main.qml`
- `contents/ui/DigitalClock.qml`
- `contents/ui/Tooltip.qml`
- `contents/ui/CalendarView.qml`
- `contents/ui/configAppearance.qml`
- `contents/ui/configCalendar.qml`
- `contents/ui/configTimeZones.qml`
- `contents/config/config.qml`
- `contents/config/main.xml`
- `metadata.json`

Swan Dict should use these files as its base and keep changes small.

## Architecture

### Applet Layer

The applet package should remain a normal Plasma applet package:

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

`metadata.json` should keep Swan Dict's identity:

```json
"Id": "com.github.LXYan2333.swan-dict",
"Name": "Swan Dictionary",
"Category": "Language"
```

The inherited Digital Clock config keys from `contents/config/main.xml` must be included because `DigitalClock.qml` reads them directly through `Plasmoid.configuration`.

### Native QML Module

Primary-selection access and dictionary lookup should be implemented in a small native Qt/KF6 QML module instead of QML-only code.

Proposed import:

```qml
import com.github.LXYan2333.SwanDict
```

Proposed QML objects:

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

Responsibilities:

- `SelectionWatcher`
  - Uses `wl-paste --primary` on Wayland.
  - Uses `QClipboard::Selection` on X11.
  - Emits `textChanged` only when normalized selected text changes.
  - Handles empty or unavailable selections gracefully.
  - Hides backend details from QML so the implementation can later move from `wl-paste` to direct Wayland protocol code without changing `DigitalClock.qml`.

- `Translator`
  - Opens the local SQLite dictionary database.
  - Normalizes selected text.
  - Performs exact and simple fallback lookups.
  - Returns a short display string for the panel.
  - Returns full dictionary content for hover and popup display.

### Dictionary Storage

Use SQLite for ECDICT data.

Recommended table shape:

```sql
CREATE TABLE stardict (
    word TEXT PRIMARY KEY,
    translation TEXT,
    definition TEXT,
    phonetic TEXT
);
```

`word`, `translation`, `definition`, and `phonetic` should be available to support both compact and full dictionary views.

The database can be installed under one of these locations:

```text
contents/data/ecdict.sqlite
~/.local/share/swan-dict/ecdict.sqlite
```

For the first version, placing it in `contents/data/` is easiest because the applet can resolve the path relative to its package.

The upstream ECDICT project is kept as a Git submodule outside the applet package:

```text
third_party/ECDICT
```

Importer scripts should read from this submodule and generate a runtime SQLite database. Raw ECDICT files must not be installed as part of the Plasma applet package.

## Display Behavior

The official digital clock sets the date text in `DigitalClock.qml`:

```qml
dateLabel.text = dateFormatter(getCurrentTime());
```

Swan Dict should replace this assignment with a helper function:

```qml
dateLabel.text = main.secondaryLabelText();
```

Expected behavior:

- If `showDate` is disabled, keep the date label empty.
- If selected text is empty, show the normal date.
- If selected text is too long, show the normal date.
- If selected text has no dictionary match, show the normal date.
- If selected text has a dictionary match, show a compact translation summary.
- If selected text has a dictionary match, show the full dictionary content in the hover tooltip.
- If selected text has a dictionary match, show the full dictionary content in the click popup.
- If selected text has no dictionary match, keep the official calendar popup behavior.

Suggested maximum selected text length:

```text
64 characters
```

Suggested compact label rules:

- Use the first translation line.
- Remove excessive whitespace.
- Keep the result short enough for a panel label.
- Elide through the existing Plasma label behavior if still too long.

Suggested full-content rules:

- Include the selected text or matched dictionary headword.
- Include phonetic text when available.
- Include full translation content.
- Include definition content when available.
- Preserve readable line breaks where useful.
- Use the hover tooltip and click popup for content that is too long for the panel.

## Lookup Rules

Initial lookup should be conservative:

1. Trim leading and trailing whitespace.
2. Collapse internal whitespace.
3. Reject multi-line selections.
4. Strip surrounding punctuation.
5. Lowercase ASCII English text.
6. Query exact word or phrase.
7. If no result, try simple fallback forms:
   - plural `words` to `word`
   - past tense `worked` to `work`
   - gerund `working` to `work`

More advanced morphology can be added later if needed.

## Configuration

The first version can work without Swan Dict-specific configuration.

Later optional settings:

- Enable translation replacement.
- Maximum selected text length.
- Show original word before translation.
- Fallback behavior:
  - show date
  - show selected text
  - show nothing

These settings should be added to `contents/config/main.xml` and surfaced in a small config page only after the core behavior works.

## Build Dependencies

On Debian 13, the installed development/runtime packages are expected to include:

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

`wl-clipboard` provides `wl-paste`, which is the planned Wayland backend for reading primary selection in the first implementation.

## Build System

Add a CMake project for the native QML module.

Expected top-level files:

```text
CMakeLists.txt
src/
  CMakeLists.txt
  selectionwatcher.h
  selectionwatcher.cpp
  translator.h
  translator.cpp
```

The CMake project should:

- Require Qt 6.
- Require ECM.
- Build a QML module named `com.github.LXYan2333.SwanDict`.
- Link Qt Quick, Qt Gui, Qt Sql, and required KF6 libraries.
- Install the applet package.
- Install the QML module where Plasma can import it.

## Implementation Phases

### Phase 1: Vendor the Official Clock

Copy the official Plasma 6 digital clock files into this repository.

Keep the official logic intact except for metadata identity. Verify the applet installs and behaves exactly like the KDE clock.

Success criteria:

- Applet installs with `kpackagetool6`.
- Plasma can add `Swan Dictionary` as a widget.
- Time, date, tooltip, popup calendar, and settings work.

### Phase 2: Replace Date With Static Test Text

Modify `DigitalClock.qml` so the date label can be replaced by a controlled string.

Use a temporary hardcoded translation string to validate layout.

Success criteria:

- Time display still updates.
- The date area displays the test string.
- Horizontal and vertical panel layouts remain usable.

### Phase 3: Add Primary Selection Watcher

Implement the native `SelectionWatcher` QML type with platform-specific backends:

- Wayland: call `wl-paste --primary --no-newline --type text/plain`.
- X11: read `QClipboard::Selection`.

Connect it to `DigitalClock.qml` and show the normalized selected text temporarily.

Success criteria:

- Selecting text in another application updates the date area.
- Empty selection restores the normal date.
- Re-selecting the same text does not cause unnecessary updates.
- Missing `wl-paste` on Wayland does not break the clock.

### Phase 4: Add SQLite Translation

Implement the native `Translator` QML type.

Connect selection changes to dictionary lookup and show translations instead of raw selected text.

Success criteria:

- Known ECDICT words show Chinese translations.
- Unknown words fall back to the date.
- Database failures do not break the clock.

### Phase 5: Polish and Package

Add install instructions, optional config, and layout refinements.

Success criteria:

- Clean build from a fresh checkout.
- Applet can be installed, upgraded, and removed.
- README explains dictionary placement and test workflow.

## Test Plan

Manual tests:

1. Install the applet.
2. Add it to a horizontal panel.
3. Confirm normal clock/date behavior with no selection.
4. Select an English word in a browser or editor.
5. Confirm the date area changes to a translation.
6. Select an unknown word.
7. Confirm the date area falls back to date.
8. Select a long sentence.
9. Confirm the applet ignores it or falls back.
10. Open the calendar popup.
11. Confirm the popup still works.
12. Change clock appearance settings.
13. Confirm inherited settings still work.

Useful commands:

```bash
kpackagetool6 --type Plasma/Applet --install applet
kpackagetool6 --type Plasma/Applet --upgrade applet
kpackagetool6 --type Plasma/Applet --remove com.github.LXYan2333.swan-dict
journalctl --user -f | grep plasmashell
```

Build verification:

```bash
cmake -B build
cmake --build build
```

## Risks

### Primary Selection on Wayland

Qt's `QClipboard::Selection` reports primary-selection support on the tested KDE Wayland session, but it did not read KWrite selections in the POC. `wl-paste --primary` did read primary-selection text because it uses the Wayland data-control path exposed by KWin.

Mitigation:

- Use `wl-paste --primary --no-newline --type text/plain` as the first Wayland backend.
- Keep each `wl-paste` call bounded with a process timeout.
- Ignore unavailable selections, command failures, and non-text data.
- Keep fallback date behavior.
- Keep `QClipboard::Selection` for X11.
- Consider replacing `wl-paste` later with direct `zwlr_data_control_manager_v1` code if the external process dependency becomes a real problem.

### External Command Dependency

Using `wl-paste` makes the first Wayland implementation much smaller, but it adds a runtime dependency on the `wl-clipboard` package and process-spawning overhead.

Mitigation:

- Declare `wl-clipboard` as a runtime dependency.
- Check for `wl-paste` availability during startup.
- Poll at a modest interval, such as 300-500 ms.
- Avoid overlapping `wl-paste` processes.
- Use `wl-paste --primary --watch` only in a later bridge design if polling overhead becomes visible.

### Plasma Private Imports

The official clock imports `org.kde.plasma.private.digitalclock`. This is a private API and can change with Plasma updates.

Mitigation:

- Keep the applet closely aligned with the installed Plasma version.
- Avoid unnecessary changes to official clock internals.

### Layout Overflow

Translations may be longer than dates.

Mitigation:

- Limit displayed translation length.
- Reuse existing `Text.Fit`, `Text.ElideRight`, and panel-specific layout behavior.
- Prefer concise translation strings.

### Large Dictionary Data

ECDICT is too large to load into QML memory.

Mitigation:

- Use SQLite.
- Add an index on `word`.
- Keep lookup synchronous only if fast; otherwise move lookup to a worker later.

## Open Questions

- Should translation show only Chinese, or `word: translation`?
- Should phrase lookup be supported in the first version?
- Should the dictionary database be bundled in the applet package or installed separately?
- Should unknown selected words fall back to the date or show the selected word?
- Should the first Wayland backend poll `wl-paste`, or should it use a persistent `wl-paste --primary --watch` bridge?
