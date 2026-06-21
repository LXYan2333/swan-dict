#!/usr/bin/env bash
set -euo pipefail

distro="${1:-}"
if [[ -z "${distro}" ]]; then
    echo "Usage: $0 <debian|ubuntu|fedora|arch>" >&2
    exit 1
fi

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
version="$(sed -nE 's/^project\(swan-dict VERSION ([^ ]+).*/\1/p' "${repo_root}/CMakeLists.txt" | head -n 1)"
plasma_workspace_version="${SWAN_DICT_ARCH_PLASMA_WORKSPACE_VERSION:-6.6.5}"
dist_dir="${repo_root}/dist/${distro}"

if [[ -z "${version}" ]]; then
    echo "Unable to determine project version." >&2
    exit 1
fi

mkdir -p "${dist_dir}"

prepare_digital_clock_fallback() {
    local download_root="/tmp/swan-dict-plasma-workspace"
    local archive="${download_root}/plasma-workspace-${plasma_workspace_version}.tar.xz"
    local extracted="${download_root}/plasma-workspace-${plasma_workspace_version}"
    local url="https://download.kde.org/stable/plasma/${plasma_workspace_version}/plasma-workspace-${plasma_workspace_version}.tar.xz"

    mkdir -p "${download_root}"
    if [[ ! -f "${archive}" ]]; then
        curl -L --fail --retry 3 -o "${archive}" "${url}"
    fi

    if [[ ! -d "${extracted}" ]]; then
        tar -C "${download_root}" -xf "${archive}"
    fi

    SWAN_DICT_ARCH_PLASMA_WORKSPACE_SOURCE_DIR="${extracted}" \
    SWAN_DICT_PREPARE_ARCH_DIGITAL_CLOCK_FALLBACK_OVERWRITE=1 \
        "${repo_root}/scripts/prepare-arch-digital-clock-fallback.sh"
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
        --exclude='applet/contents/data/ecdict.sqlite' \
        --exclude='third_party/ECDICT/stardict.7z' \
        --exclude='__pycache__' \
        -cf - . | tar -C "${source_dir}" -xf -

    echo "${source_dir}"
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
    prepare_digital_clock_fallback

    local tmp_dir source_dir
    tmp_dir="$(mktemp -d /tmp/swan-dict-deb.XXXXXX)"
    source_dir="$(prepare_source_tree "${tmp_dir}")"
    cp -a "${repo_root}/packaging/obs/debian" "${source_dir}/debian"

    (
        cd "${source_dir}"
        dpkg-buildpackage -us -uc -b
    )

    cp "${tmp_dir}"/*.deb "${dist_dir}/"
    cp "${tmp_dir}"/*.buildinfo "${tmp_dir}"/*.changes "${dist_dir}/" 2>/dev/null || true
}

build_fedora_package() {
    dnf -y install dnf-plugins-core rpm-build rpmdevtools ca-certificates curl git tar xz
    dnf -y builddep "${repo_root}/packaging/obs/swan-dict.spec"
    prepare_digital_clock_fallback

    local rpmbuild_dir="${repo_root}/build/rpmbuild"
    mkdir -p "${rpmbuild_dir}/BUILD" "${rpmbuild_dir}/RPMS" "${rpmbuild_dir}/SOURCES" "${rpmbuild_dir}/SPECS" "${rpmbuild_dir}/SRPMS"
    create_source_tarball "${rpmbuild_dir}/SOURCES/swan-dict-${version}.tar.xz"
    cp "${repo_root}/packaging/obs/swan-dict.spec" "${rpmbuild_dir}/SPECS/"

    rpmbuild -ba \
        --define "_topdir ${rpmbuild_dir}" \
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

    prepare_digital_clock_fallback

    local build_dir="/tmp/swan-dict-arch"
    rm -rf "${build_dir}"
    mkdir -p "${build_dir}"
    create_source_tarball "${build_dir}/swan-dict-${version}.tar.xz"
    cp "${repo_root}/packaging/obs/PKGBUILD" "${build_dir}/"

    useradd -m builder
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
    *)
        echo "Unknown distro: ${distro}" >&2
        exit 1
        ;;
esac

find "${dist_dir}" -maxdepth 1 -type f -print
