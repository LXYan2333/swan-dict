# OBS Packaging Notes

This project should use each distribution's own Plasma Digital Clock files when
the distribution ships copyable QML applet sources.

Arch currently ships the Digital Clock as a Plasma applet plugin and does not
provide `/usr/share/plasma/plasmoids/org.kde.plasma.digitalclock/contents`.
For that target, prepare the fallback files from Arch's `plasma-workspace`
source package recipe and include those generated Digital Clock QML/config
files in the OBS source archive.

## Source Policy

The source package should include:

- `applet/metadata.json`
- `applet-owned/config/config.qml`
- owned QML files, especially `applet/contents/ui/DictionaryPopup.qml` and
  `applet/contents/ui/configTranslation.qml`
- copied Digital Clock QML/config files when building targets that do not ship
  copyable Digital Clock applet sources, currently Arch. Generate these files
  with `scripts/prepare-arch-digital-clock-fallback.sh`.
- `patches/*.patch`
- `scripts/sync-digital-clock.sh`
- `src/`
- `po/`
- `tools/import-ecdict/import_ecdict.py`
- `third_party/ECDICT/ecdict.csv`

The source package should not include:

- `build/`
- `applet/contents/data/ecdict.sqlite`

During CMake configure, `scripts/sync-digital-clock.sh` copies the Digital Clock
from the build root's installed Plasma package when generated files are absent.
This keeps `org.kde.plasma.private.digitalclock` and the copied QML in sync with
the target distribution.

For Debian/Fedora builds where the distro does ship the Digital Clock source,
set this environment variable during configure so the distro source replaces
the fallback files:

```console
SWAN_DICT_SYNC_DIGITAL_CLOCK_OVERWRITE=1
```

Do not set it for Arch unless Arch starts shipping the copyable Digital Clock
source path.

To refresh the Arch fallback from Arch's current `plasma-workspace` source
package:

```console
SWAN_DICT_PREPARE_ARCH_DIGITAL_CLOCK_FALLBACK_OVERWRITE=1 scripts/prepare-arch-digital-clock-fallback.sh
```

The script uses `pkgctl repo clone plasma-workspace` and `makepkg --nobuild
--nodeps` under `.cache/arch-plasma-workspace/`, finds
`applets/digital-clock/package/contents`, then applies this project's split
patches through `scripts/sync-digital-clock.sh`.

On non-Arch systems, you can provide an already unpacked matching
`plasma-workspace` source tree instead:

```console
SWAN_DICT_ARCH_PLASMA_WORKSPACE_SOURCE_DIR=/path/to/plasma-workspace \
SWAN_DICT_PREPARE_ARCH_DIGITAL_CLOCK_FALLBACK_OVERWRITE=1 \
    scripts/prepare-arch-digital-clock-fallback.sh
```

Required local tools for that refresh:

- `pkgctl` from Arch `devtools`
- `makepkg`
- normal source download access for the Arch `plasma-workspace` PKGBUILD

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

```console
%cmake_kf6 -DSWAN_DICT_GENERATE_DICTIONARY=ON
%cmake_build
%cmake_install
```

or, without distro macros:

```console
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
- `libkf6config-dev`
- `libkf6coreaddons-dev`
- `libkf6i18n-dev`
- `libkf6package-dev`
- `libkf6windowsystem-dev`
- `plasma-workspace`
- `kwin-dev`
- `libwayland-dev`
- `wayland-protocols`

Debian/Ubuntu runtime dependencies:

- `plasma-workspace`
- `kwin-wayland | kwin-x11`
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
- `kf6-kconfig-devel`
- `kf6-kcoreaddons-devel`
- `kf6-kpackage-devel`
- `kf6-kwindowsystem-devel`
- `kwin-devel` or `cmake(KWin)` provider
- `plasma-workspace`
- `wayland-devel`
- `wayland-protocols-devel`

Fedora runtime dependencies:

- `plasma-workspace`
- `kwin`
- `qt6-qtbase`
- `qt6-qtdeclarative`

Fedora does not split Qt 6's SQLite driver into a separate
`qt6-qtbase-sqlite` package. The `libqsqlite.so` driver is provided by
`qt6-qtbase`.

Arch build dependencies:

- `cmake`
- `extra-cmake-modules`
- `ninja`
- `pkgconf`
- `python`
- `qt6-base`
- `qt6-declarative`
- `ki18n`
- `kconfig`
- `kpackage`
- `kwin`
- `kwindowsystem`
- `plasma-workspace`
- `wayland`
- `wayland-protocols`

Arch runtime dependencies:

- `plasma-workspace`
- `kwin`
- `qt6-base`
- `qt6-declarative`

## OBS Workflow

1. Create one OBS project with repositories for Debian, Ubuntu, Fedora, and
   Arch.
2. Use the packaging templates in this directory:
   - `swan-dict.spec` is the initial RPM recipe for Fedora/openSUSE-style
     repositories.
   - `debian/` is the initial Debian/Ubuntu packaging directory.
   - `PKGBUILD` is the initial Arch recipe.
3. Generate and upload source archives with
   `packaging/obs/update-obs-sources.sh`, or let the GitHub Actions workflow do
   it after a release tag.
4. Do not upload an OBS `_service` file for this package. The source archive is
   already generated before `osc commit`; uploading `_service` makes OBS try to
   install source-service packages such as `obs-service-tar` in target
   repositories, which breaks Debian/Arch/Fedora builds.
5. Ensure each recipe runs the normal CMake configure/build/install steps.
6. Do not call `scripts/regenerate-patches.sh` in OBS. Patch regeneration is a
   maintainer/dev action, not a package build action.

The recipes build and install the KWin helper by default:

```console
-DSWAN_DICT_BUILD_KWIN_HELPER=ON
```

This means every OBS target needs the distro's KWin development package and
runtime KWin package available.

For Debian/Ubuntu, OBS may require a `.dsc` source package in addition to the
`debian/` directory, depending on the chosen OBS workflow. One practical local
workflow is to generate the Debian source package from a release tarball with:

```console
dpkg-buildpackage -S -us -uc
```

and upload the resulting `.dsc`, `.debian.tar.*`, and `.orig.tar.*` files to
the OBS package.
