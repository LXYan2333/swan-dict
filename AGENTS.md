# AGENTS.md

## Project Overview

Swan Dict is a KDE Plasma 6 widget derived from the upstream Digital Clock.
It replaces the date label with a translation of the primary selection and
opens a dictionary popup on click.

The widget has three main parts:

- Common project-owned applet files under `applets/common/`
- Generated distro applet packages under `applets/<distro>/<version>/`
- Native QML plugin under `src/`
- ECDICT importer under `tools/import-ecdict/`
- Optional KWin mouse-click helper effect under `kwin-helper/`

## Generated Digital Clock Files

Most Digital Clock files under `applets/<distro>/<version>/contents/` are
generated from the upstream Digital Clock selected for that distro profile:

- `applets/<distro>/<version>/contents/config/`
- `applets/<distro>/<version>/contents/ui/CalendarView.qml`
- `applets/<distro>/<version>/contents/ui/DigitalClock.qml`
- `applets/<distro>/<version>/contents/ui/NoTimezoneWarning.qml`
- `applets/<distro>/<version>/contents/ui/Tooltip.qml`
- `applets/<distro>/<version>/contents/ui/configAppearance.qml`
- `applets/<distro>/<version>/contents/ui/configCalendar.qml`
- `applets/<distro>/<version>/contents/ui/configTimeZones.qml`
- `applets/<distro>/<version>/contents/ui/main.qml`

These generated files are ignored by git. Do not treat them as canonical source.

Project-owned QML files are kept in `applets/common/` and copied into each
generated profile during sync:

- `applets/common/metadata.json`
- `applets/common/contents/config/config.qml`
- `applets/common/contents/ui/DictionaryTooltip.qml`
- `applets/common/contents/ui/DictionaryPopup.qml`
- `applets/common/contents/ui/SwanDictController.qml`
- `applets/common/contents/ui/configTranslation.qml`

## Patch Workflow

Upstream-owned Digital Clock changes are stored as split patch files under the
matching distro profile:

- `patches/digital-clock/debian/13/`
- `patches/digital-clock/ubuntu/26.04/`
- `patches/digital-clock/fedora/44/`
- `patches/digital-clock/arch/latest/`

Use:

```console
python3 scripts/manage.py regenerate-patches
```

to regenerate patch files from the current generated applet files for the
default `debian/13` profile. Set `SWAN_DICT_PROFILE`, for example:

```console
SWAN_DICT_PROFILE=fedora/44 python3 scripts/manage.py regenerate-patches
```

## Syncing Digital Clock Files

Use the two-step source workflow:

```console
SWAN_DICT_PROFILE=debian/13 python3 scripts/manage.py prepare-source
SWAN_DICT_PROFILE=debian/13 SWAN_DICT_SYNC_DIGITAL_CLOCK_OVERWRITE=1 python3 scripts/manage.py sync-digital-clock
```

`prepare-source` must obtain the target distro's `plasma-workspace` source
package. Do not copy Digital Clock QML from the system-installed plasmoid
directory. If a distro has no supported source package workflow, the script
must fail and tell the maintainer.

Prepared source cache layout is profile-specific:

```text
.cache/plasma-workspace-source/<distro>/<version>/source
.cache/plasma-workspace-source/<distro>/<version>/download
```

Use `SWAN_DICT_PROFILE` to sync another profile, for example:

```console
SWAN_DICT_PROFILE=fedora/44 SWAN_DICT_SYNC_DIGITAL_CLOCK_OVERWRITE=1 python3 scripts/manage.py sync-digital-clock
```

After applying patches, the sync script copies:

```text
applets/common/contents/config/config.qml
```

to:

```text
applets/<distro>/<version>/contents/config/config.qml
```

This avoids cross-distro breakage from upstream Digital Clock's dynamic calendar
event plugin config model, whose roles differ between Plasma versions.

Without `SWAN_DICT_SYNC_DIGITAL_CLOCK_OVERWRITE=1`, the sync script refuses to
overwrite existing generated files. This is intentional to protect local dev
edits.

OBS/package source archives should include prepared generated applet files for
each supported profile. Generate them from distro source packages before
creating the source archive.

## Dictionary Database

The SQLite dictionary database is generated from:

```text
third_party/ECDICT/ecdict.csv
```

by:

```text
tools/import-ecdict/import_ecdict.py
```

The generated database is:

```text
applets/<distro>/<version>/contents/data/ecdict.sqlite
```

It is ignored by git and should not be committed.

CMake generates the database during build by default:

```console
cmake --build build
```

The CMake option is:

```console
-DSWAN_DICT_GENERATE_DICTIONARY=ON
```

Only fields used by the runtime are imported:

- `word`
- `sw`
- `phonetic`
- `definition`
- `translation`
- `bnc`
- `frq`
- `exchange`

Do not re-add unused ECDICT fields such as `collins`, `oxford`, `tag`, or `pos`
unless runtime code starts using them.

## Native QML Plugin

The native QML plugin is `com.github.LXYan2333.SwanDict`.

Important classes:

- `SelectionWatcher`: reads the primary selection.
- `Translator`: local ECDICT lookup and DeepSeek sentence translation.
- `ClipboardWriter`: writes copied dictionary entries to the clipboard.

Wayland primary selection is read through the in-process
`WaylandPrimarySelection` helper, which uses the `zwp_primary_selection_v1`
protocol generated by CMake from `wayland-protocols`.

X11 uses `QClipboard::Selection`.

## Optional KWin Helper

The optional helper under `kwin-helper/` is a KWin C++ effect that listens for
global mouse button presses inside KWin and emits a session D-Bus signal:

```text
/com/github/LXYan2333/SwanDict/KWinHelper
com.github.LXYan2333.SwanDict.KWinHelper.mouseClicked
```

`SelectionWatcher` listens for this signal if it exists. The widget must still
build and run without the helper installed or loaded.

Config key `startKWinMouseHelper` controls whether the widget asks KWin to load
the helper on startup. It defaults to `true`, but loading must remain
best-effort and silent when KWin or the helper is unavailable.

The top-level CMake default builds the helper:

```console
cmake -B build -S . -DSWAN_DICT_BUILD_KWIN_HELPER=ON
```

OBS distro packages should build the helper by default. The helper requires
KWin development files, for example Debian's `kwin-dev`.

## Translation Behavior

Local dictionary lookup:

- Single-word selection shows a full dictionary entry.
- Multi-word selection splits words and shows per-word dictionary entries.
- Date replacement remains compact and uses the configured maximum length.
- Hover for multi-word selections shows expanded per-word local translations.
- Click popup for multi-word selections shows expanded entries per word.

Selection limit:

- Config key: `maximumSelectionLength`
- Default: `128`
- If the selection exceeds the limit, lookup extends to the end of the current
  word so a word is not split.
- The UI displays a red bold warning heading when truncation occurs.

Date replacement length:

- Config key: `dateReplacementTranslationMaximumLength`
- Default: `10`
- Truncation uses `…`.

DeepSeek sentence translation:

- Config key: `enableDeepSeekSentenceTranslation`
- Config key: `deepSeekApiKey`
- It is manually triggered from the popup button.
- It must not run on hover.
- It must not run automatically when the popup opens.
- It should be cancelled when selection changes or popup closes.

The API key is stored in KDE applet config, not KWallet.

## UI Conventions

Use KDE/Kirigami/Plasma components where possible.

Copy buttons use themed icons:

```qml
icon.name: "edit-paste"
```

Do not hardcode Breeze icon file paths for themed UI icons.

Exchange rows use their own aligned two-column layout and should not align with
translation/definition POS columns.

Warning text should be visually obvious:

- red / `Kirigami.Theme.negativeTextColor`
- bold
- heading style

## i18n

The translation catalog is:

```text
plasma_applet_com.github.LXYan2333.swan-dict
```

Translation files live under:

```text
po/
```

Chinese translation currently lives at:

```text
po/zh_CN/plasma_applet_com.github.LXYan2333.swan-dict.po
```

Use `i18n()` / `i18np()` in QML and `KLocalizedString` in C++ for user-facing
strings.

## OBS Packaging

OBS notes are in:

```text
packaging/obs/README.md
```

The OBS source package should include:

- owned source files
- patch files
- `third_party/ECDICT/ecdict.csv`

It should not include:

- `build/`
- generated `ecdict.sqlite`
- stale legacy `applet/` files

Package builds should use the target distro's Plasma Workspace package so the
copied Digital Clock QML matches the target distro's
`org.kde.plasma.private.digitalclock`.

Use `SWAN_DICT_PROFILE` for package-specific builds:

- Debian 13: `debian/13`
- Ubuntu 26.04: `ubuntu/26.04`
- Fedora 44: `fedora/44`
- Arch rolling: `arch/latest`

## Build And Test

Normal local configure/build:

```console
cmake -B build -S .
cmake --build build
```

The user usually launches from the source tree with `QML_IMPORT_PATH` pointing
at the build module directory. Do not assume install is required for testing.

Typical source-tree launch:

```console
QML_IMPORT_PATH=build/src plasmoidviewer -a applets/debian/13
```

When changing C++:

```console
cmake --build build
```

When changing generated upstream-owned QML/config files and the change should
persist:

```console
python3 scripts/manage.py regenerate-patches
```

When changing only project-owned files such as `DictionaryTooltip.qml`,
`DictionaryPopup.qml`, `SwanDictController.qml`, `configTranslation.qml`, or
files under `src/`, patch regeneration is not needed.

## Safety Notes

Do not run destructive git commands.

Do not overwrite generated Digital Clock files unless explicitly syncing with:

```sh
SWAN_DICT_SYNC_DIGITAL_CLOCK_OVERWRITE=1
```

Before finishing work that affects generated upstream-owned files, ensure the
patch files are regenerated.
