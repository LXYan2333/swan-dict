#!/usr/bin/env bash
set -euo pipefail

distro="${1:-}"
if [[ -z "${distro}" ]]; then
    echo "Usage: $0 <debian|ubuntu|fedora|arch>" >&2
    exit 1
fi

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
version="$(sed -nE 's/^project\(swan-dict VERSION ([^ ]+).*/\1/p' "${repo_root}/CMakeLists.txt" | head -n 1)"
dist_dir="${repo_root}/dist/${distro}"
profile=""

if [[ -z "${version}" ]]; then
    echo "Unable to determine project version." >&2
    exit 1
fi

mkdir -p "${dist_dir}"

case "${distro}" in
    debian)
        profile="debian/13"
        ;;
    ubuntu)
        profile="ubuntu/26.04"
        ;;
    fedora)
        profile="fedora/44"
        ;;
    arch)
        profile="arch/latest"
        ;;
    *)
        echo "Unknown distro: ${distro}" >&2
        exit 1
        ;;
esac

prepare_digital_clock_from_source_package() {
    SWAN_DICT_PROFILE="${profile}" \
    SWAN_DICT_PREPARE_SOURCE_OVERWRITE=1 \
        python3 "${repo_root}/scripts/manage.py" prepare-source

    SWAN_DICT_PROFILE="${profile}" \
    SWAN_DICT_SYNC_DIGITAL_CLOCK_OVERWRITE=1 \
        python3 "${repo_root}/scripts/manage.py" sync-digital-clock
}

prepare_source_tree() {
    local parent="$1"
    local source_dir="${parent}/swan-dict-${version}"

    mkdir -p "${source_dir}"
    tar -C "${repo_root}" \
        --exclude='.git' \
        --exclude='.cache' \
        --exclude='build' \
        --exclude='dist' \
        --exclude='applet' \
        --exclude='applet/contents/data/ecdict.sqlite' \
        --exclude='applets/*/*/contents/data/ecdict.sqlite' \
        --exclude='third_party/ECDICT/stardict.7z' \
        --exclude='__pycache__' \
        -cf - . | tar -C "${source_dir}" -xf -

    echo "${source_dir}"
}

enable_deb_source_repositories() {
    if compgen -G "/etc/apt/sources.list.d/*.sources" >/dev/null; then
        sed -i -E '/^Types:/ { /(^|[[:space:]])deb-src($|[[:space:]])/! s/$/ deb-src/ }' /etc/apt/sources.list.d/*.sources
    fi
    if [[ -f /etc/apt/sources.list ]]; then
        sed -i -E 's/^# deb-src /deb-src /' /etc/apt/sources.list
        if ! grep -q -E '^deb-src[[:space:]]+' /etc/apt/sources.list; then
            local tmp_sources
            tmp_sources="$(mktemp)"
            awk '
                /^deb / {
                    source = $0
                    sub(/^deb /, "deb-src ", source)
                    if (!seen[source]++) {
                        print source
                    }
                }
            ' /etc/apt/sources.list > "${tmp_sources}"
            cat "${tmp_sources}" >> /etc/apt/sources.list
            rm -f "${tmp_sources}"
        fi
    fi
}

create_source_tarball() {
    local output="$1"
    local tmp_dir
    tmp_dir="$(mktemp -d /tmp/swan-dict-source.XXXXXX)"
    prepare_source_tree "${tmp_dir}" >/dev/null
    tar -C "${tmp_dir}" -cJf "${output}" "swan-dict-${version}"
    rm -rf "${tmp_dir}"
}

install_deb_dependencies() {
    export DEBIAN_FRONTEND=noninteractive
    enable_deb_source_repositories
    apt-get update
    apt-get install -y --no-install-recommends ca-certificates curl git tar xz-utils

    if [[ "${distro}" == "ubuntu" ]]; then
        apt-get install -y --no-install-recommends software-properties-common
        add-apt-repository -y universe || true
        apt-get update
    fi

    apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        debhelper \
        devscripts \
        dpkg-dev \
        extra-cmake-modules \
        libdrm-dev \
        libepoxy-dev \
        libkf6config-dev \
        libkf6coreaddons-dev \
        libkf6i18n-dev \
        libkf6package-dev \
        libkf6windowsystem-dev \
        libplasma-dev \
        libwayland-dev \
        ninja-build \
        pkgconf \
        plasma-workspace \
        python3 \
        qt6-base-dev \
        qt6-declarative-dev \
        wayland-protocols

    apt-get install -y --no-install-recommends kwin-dev || true
}

build_deb_package() {
    install_deb_dependencies
    prepare_digital_clock_from_source_package

    local tmp_dir source_dir
    tmp_dir="$(mktemp -d /tmp/swan-dict-deb.XXXXXX)"
    source_dir="$(prepare_source_tree "${tmp_dir}")"
    cp -a "${repo_root}/packaging/obs/debian" "${source_dir}/debian"

    (
        cd "${source_dir}"
        SWAN_DICT_PROFILE="${profile}" dpkg-buildpackage -us -uc -b
    )

    cp "${tmp_dir}"/*.deb "${dist_dir}/"
    cp "${tmp_dir}"/*.buildinfo "${tmp_dir}"/*.changes "${dist_dir}/" 2>/dev/null || true
}

build_fedora_package() {
    dnf -y install dnf-plugins-core rpm-build rpmdevtools ca-certificates cpio curl git tar xz
    dnf -y builddep "${repo_root}/packaging/obs/swan-dict.spec"
    prepare_digital_clock_from_source_package

    local rpmbuild_dir="${repo_root}/build/rpmbuild"
    mkdir -p "${rpmbuild_dir}/BUILD" "${rpmbuild_dir}/RPMS" "${rpmbuild_dir}/SOURCES" "${rpmbuild_dir}/SPECS" "${rpmbuild_dir}/SRPMS"
    create_source_tarball "${rpmbuild_dir}/SOURCES/swan-dict-${version}.tar.xz"
    cp "${repo_root}/packaging/obs/swan-dict.spec" "${rpmbuild_dir}/SPECS/"

    rpmbuild -ba \
        --define "_topdir ${rpmbuild_dir}" \
        --define "swan_dict_profile ${profile}" \
        "${rpmbuild_dir}/SPECS/swan-dict.spec"

    find "${rpmbuild_dir}/RPMS" "${rpmbuild_dir}/SRPMS" -type f \( -name '*.rpm' -o -name '*.src.rpm' \) -exec cp {} "${dist_dir}/" \;
}

build_arch_package() {
    pacman -Syu --noconfirm
    pacman -S --needed --noconfirm \
        base-devel \
        ca-certificates \
        cmake \
        curl \
        devtools \
        extra-cmake-modules \
        git \
        kconfig \
        ki18n \
        kpackage \
        kwin \
        kwindowsystem \
        libdrm \
        libepoxy \
        libplasma \
        ninja \
        pkgconf \
        plasma-desktop \
        plasma-workspace \
        python \
        qt6-base \
        qt6-declarative \
        sudo \
        tar \
        vulkan-headers \
        wayland \
        wayland-protocols \
        xz

    id -u builder >/dev/null 2>&1 || useradd -m builder
    chown -R builder:builder "${repo_root}"

    runuser -u builder -- env \
        SWAN_DICT_PROFILE="${profile}" \
        SWAN_DICT_PREPARE_SOURCE_OVERWRITE=1 \
        python3 "${repo_root}/scripts/manage.py" prepare-source

    runuser -u builder -- env \
        SWAN_DICT_PROFILE="${profile}" \
        SWAN_DICT_SYNC_DIGITAL_CLOCK_OVERWRITE=1 \
        python3 "${repo_root}/scripts/manage.py" sync-digital-clock

    local build_dir="/tmp/swan-dict-arch"
    rm -rf "${build_dir}"
    mkdir -p "${build_dir}"
    create_source_tarball "${build_dir}/swan-dict-${version}.tar.xz"
    cp "${repo_root}/packaging/obs/PKGBUILD" "${build_dir}/"

    chown -R builder:builder "${build_dir}"
    (
        cd "${build_dir}"
        runuser -u builder -- makepkg --noconfirm --needed --skippgpcheck
    )

    cp "${build_dir}"/*.pkg.tar.* "${dist_dir}/"
}

case "${distro}" in
    debian|ubuntu)
        build_deb_package
        ;;
    fedora)
        build_fedora_package
        ;;
    arch)
        build_arch_package
        ;;
esac

find "${dist_dir}" -maxdepth 1 -type f -print
