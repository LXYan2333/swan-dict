# OBS Packaging Notes

This project prepares Digital Clock QML/config from each distribution's own
`plasma-workspace` source package. It must not copy Digital Clock files from the
system-installed plasmoid directory.

## Source Policy

The source package should include:

- `applets/common/metadata.json`
- `applet-owned/config/config.qml`
- owned QML files under `applets/common/contents/ui/`
- copied Digital Clock QML/config files generated from distro source packages
- `patches/digital-clock/<profile>/*.patch`
- `scripts/manage.py`
- `src/`
- `po/`
- `tools/import-ecdict/import_ecdict.py`
- `third_party/ECDICT/ecdict.csv`

The source package should not include:

- `build/`
- `applets/*/*/contents/data/ecdict.sqlite`

Before creating an OBS source archive, refresh every supported profile:

```console
SWAN_DICT_PROFILE=debian/13 python3 scripts/manage.py prepare-source
SWAN_DICT_PROFILE=debian/13 SWAN_DICT_SYNC_DIGITAL_CLOCK_OVERWRITE=1 python3 scripts/manage.py sync-digital-clock
```

Repeat for `ubuntu/26.04`, `fedora/44`, and `arch/latest`. If a distro source
package cannot be obtained, the manager stops and reports the unsupported path.

During CMake build, `tools/import-ecdict/import_ecdict.py` generates
`applets/<profile>/contents/data/ecdict.sqlite` from `third_party/ECDICT/ecdict.csv`.

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
- Plasma Workspace source package access for the target profile

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
6. Do not call `python3 scripts/manage.py regenerate-patches` in OBS. Patch regeneration is a
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
