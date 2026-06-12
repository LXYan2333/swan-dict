Name:           swan-dict
Version:        1.2.0
Release:        0
Summary:        KDE Plasma dictionary clock widget
License:        GPL-2.0-or-later
URL:            https://github.com/LXYan2333/swan-dict
Source0:        %{name}-%{version}.tar.xz

BuildRequires:  cmake
BuildRequires:  extra-cmake-modules
BuildRequires:  gcc-c++
BuildRequires:  ninja-build
BuildRequires:  pkgconfig
BuildRequires:  python3
BuildRequires:  wayland-protocols-devel
BuildRequires:  cmake(Qt6Core)
BuildRequires:  cmake(Qt6DBus)
BuildRequires:  cmake(Qt6Gui)
BuildRequires:  cmake(Qt6Network)
BuildRequires:  cmake(Qt6Qml)
BuildRequires:  cmake(Qt6Quick)
BuildRequires:  cmake(Qt6Sql)
BuildRequires:  cmake(Qt6Widgets)
BuildRequires:  cmake(KF6Config)
BuildRequires:  cmake(KF6I18n)
BuildRequires:  cmake(KF6CoreAddons)
BuildRequires:  cmake(KF6Package)
BuildRequires:  cmake(KF6WindowSystem)
BuildRequires:  cmake(KWin)
BuildRequires:  cmake(Plasma)
BuildRequires:  pkgconfig(epoxy)
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  plasma-desktop
BuildRequires:  plasma-workspace
Requires:       kwin
Requires:       plasma-desktop
Requires:       plasma-workspace
Requires:       qt6-qtbase

%description
Swan Dict is a KDE Plasma 6 widget derived from the Digital Clock. It replaces
the date label with a compact local dictionary translation of the primary
selection and opens a full dictionary popup on click.

%prep
%autosetup -p1

%build
SWAN_DICT_SYNC_DIGITAL_CLOCK_OVERWRITE=1 %cmake -G Ninja \
    -DBUILD_TESTING=OFF \
    -DSWAN_DICT_GENERATE_DICTIONARY=ON \
    -DSWAN_DICT_BUILD_KWIN_HELPER=ON
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%doc README.md
%{_datadir}/plasma/plasmoids/com.github.LXYan2333.swan-dict/
%{_libdir}/qt6/qml/com/github/LXYan2333/SwanDict/
%{_libdir}/qt6/plugins/kwin/effects/plugins/swandictmousehelper.so
%{_datadir}/locale/*/LC_MESSAGES/plasma_applet_com.github.LXYan2333.swan-dict.mo

%changelog
