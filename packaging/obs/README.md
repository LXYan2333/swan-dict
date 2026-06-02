# OBS Packaging Notes

This project is intended to be built against each distribution's own Plasma
Digital Clock files instead of vendoring one fixed upstream copy.

## Source Policy

The source package should include:

- `applet/metadata.json`
- owned QML files, especially `applet/contents/ui/DictionaryPopup.qml` and
  `applet/contents/ui/configTranslation.qml`
- `patches/*.patch`
- `scripts/sync-digital-clock.sh`
- `src/`
- `po/`
- `tools/import-ecdict/import_ecdict.py`
- `third_party/ECDICT/ecdict.csv`

The source package should not include:

- `build/`
- `applet/contents/data/ecdict.sqlite`
- copied Digital Clock files from `applet/contents/config/`
- copied upstream-owned Digital Clock QML files under `applet/contents/ui/`

During CMake configure, `scripts/sync-digital-clock.sh` copies the Digital Clock
from the build root's installed Plasma package when generated files are absent.
This keeps `org.kde.plasma.private.digitalclock` and the copied QML in sync with
the target distribution.

During CMake build, `tools/import-ecdict/import_ecdict.py` generates
`applet/contents/data/ecdict.sqlite` from `third_party/ECDICT/ecdict.csv`.

## Required Build Dependencies

Common requirements:

- CMake
- Ninja or Make
- pkg-config / pkgconf
- C++17 compiler
- Python 3
- Extra CMake Modules
- Qt 6 Core, Gui, Network, Qml, Sql
- KF6 I18n
- KF6 Package
- Wayland client development files
- wayland-protocols
- Plasma Workspace package that owns
  `/usr/share/plasma/plasmoids/org.kde.plasma.digitalclock`

Runtime requirements:

- Plasma 6
- Qt 6 SQLite plugin

## CMake

Use normal distro paths:

```sh
%cmake_kf6 -DSWAN_DICT_GENERATE_DICTIONARY=ON
%cmake_build
%cmake_install
```

or, without distro macros:

```sh
cmake -B build -S . -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DSWAN_DICT_GENERATE_DICTIONARY=ON
cmake --build build
DESTDIR="$pkgdir" cmake --install build
```

## Distribution Package Name Hints

These names need verification in each OBS target, but they are the expected
starting point.

Debian/Ubuntu build dependencies:

- `cmake`
- `extra-cmake-modules`
- `ninja-build`
- `pkgconf`
- `python3`
- `qt6-base-dev`
- `qt6-declarative-dev`
- `libkf6i18n-dev`
- `libkf6package-dev`
- `plasma-workspace`
- `kwin-dev`
- `libwayland-dev`
- `wayland-protocols`

Debian/Ubuntu runtime dependencies:

- `plasma-workspace`
- `qml6-module-qtquick`
- `qml6-module-qtquick-controls`
- `qml6-module-qtquick-layouts`
- `libqt6sql6-sqlite`

Fedora build dependencies:

- `cmake`
- `extra-cmake-modules`
- `ninja-build`
- `pkgconf-pkg-config`
- `python3`
- `qt6-qtbase-devel`
- `qt6-qtdeclarative-devel`
- `kf6-ki18n-devel`
- `kf6-kpackage-devel`
- `plasma-workspace`
- KWin development package, distro name varies
- `wayland-devel`
- `wayland-protocols-devel`

Fedora runtime dependencies:

- `plasma-workspace`
- `qt6-qtbase`
- `qt6-qtdeclarative`
- `qt6-qtbase-sqlite`

Arch build dependencies:

- `cmake`
- `extra-cmake-modules`
- `ninja`
- `pkgconf`
- `python`
- `qt6-base`
- `qt6-declarative`
- `ki18n`
- `kpackage`
- `plasma-workspace`
- `wayland`
- `wayland-protocols`

Arch runtime dependencies:

- `plasma-workspace`
- `qt6-base`
- `qt6-declarative`

## OBS Workflow

1. Create one OBS project with repositories for Debian, Ubuntu, Fedora, and
   Arch.
2. Add distro-specific package recipes in the OBS package:
   - RPM `.spec` for Fedora.
   - Debian packaging for Debian/Ubuntu.
   - `PKGBUILD` for Arch.
3. Ensure each recipe runs the normal CMake configure/build/install steps.
4. Do not call `scripts/regenerate-patches.sh` in OBS. Patch regeneration is a
   maintainer/dev action, not a package build action.
