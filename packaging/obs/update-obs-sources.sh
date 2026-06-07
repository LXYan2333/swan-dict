#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"

project="${SWAN_DICT_OBS_PROJECT:-home:lxyan3}"
package="${SWAN_DICT_OBS_PACKAGE:-swan-dict}"
checkout_root="${SWAN_DICT_OBS_CHECKOUT_DIR:-${RUNNER_TEMP:-/tmp}/swan-dict-obs}"
arch_plasma_version="${SWAN_DICT_ARCH_PLASMA_WORKSPACE_VERSION:-6.6.5}"

read_project_version() {
    sed -nE 's/^project\(swan-dict VERSION ([^ ]+).*/\1/p' "${repo_root}/CMakeLists.txt" | head -n 1
}

version="${SWAN_DICT_VERSION:-$(read_project_version)}"
if [[ -z "${version}" ]]; then
    echo "Unable to determine Swan Dict version." >&2
    exit 1
fi

debian_revision="${SWAN_DICT_DEBIAN_REVISION:-1}"
generic_tarball="swan-dict-${version}.tar.xz"
debian_orig_tarball="swan-dict_${version}.orig.tar.xz"
debian_source_tarball="swan-dict_${version}-${debian_revision}.debian.tar.xz"
debian_dsc="swan-dict_${version}-${debian_revision}.dsc"

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Required command not found: $1" >&2
        exit 1
    fi
}

require_command curl
require_command dpkg-source
require_command osc
require_command tar

prepare_arch_plasma_source() {
    if [[ -n "${SWAN_DICT_ARCH_PLASMA_WORKSPACE_SOURCE_DIR:-}" ]]; then
        return
    fi

    local download_root="${SWAN_DICT_ARCH_PLASMA_WORKSPACE_DOWNLOAD_DIR:-${RUNNER_TEMP:-/tmp}/swan-dict-arch-plasma-workspace}"
    local archive="${download_root}/plasma-workspace-${arch_plasma_version}.tar.xz"
    local extracted="${download_root}/plasma-workspace-${arch_plasma_version}"
    local url="https://download.kde.org/stable/plasma/${arch_plasma_version}/plasma-workspace-${arch_plasma_version}.tar.xz"

    mkdir -p "${download_root}"

    if [[ ! -f "${archive}" ]]; then
        curl -L --fail --retry 3 -o "${archive}" "${url}"
    fi

    if [[ ! -d "${extracted}" ]]; then
        tar -C "${download_root}" -xf "${archive}"
    fi

    export SWAN_DICT_ARCH_PLASMA_WORKSPACE_SOURCE_DIR="${extracted}"
}

prepare_source_tree() {
    local source_parent="$1"
    local source_dir="${source_parent}/swan-dict-${version}"

    mkdir -p "${source_dir}"

    tar -C "${repo_root}" \
        --exclude='.git' \
        --exclude='.cache' \
        --exclude='build' \
        --exclude='applet/contents/data/ecdict.sqlite' \
        --exclude='third_party/ECDICT/stardict.7z' \
        --exclude='__pycache__' \
        -cf - . | tar -C "${source_dir}" -xf -

    echo "${source_dir}"
}

checkout_obs_package() {
    mkdir -p "${checkout_root}"

    if [[ -d "${checkout_root}/${project}/${package}/.osc" ]]; then
        (
            cd "${checkout_root}/${project}/${package}"
            osc update >&2
        )
    else
        (
            cd "${checkout_root}"
            osc checkout "${project}" "${package}" >&2
        )
    fi

    echo "${checkout_root}/${project}/${package}"
}

remove_old_versioned_sources() {
    local package_dir="$1"
    shift
    local keep=" $* "
    local file base

    for file in \
        "${package_dir}"/swan-dict-*.tar.xz \
        "${package_dir}"/swan-dict_*.orig.tar.xz \
        "${package_dir}"/swan-dict_*-*.debian.tar.xz \
        "${package_dir}"/swan-dict_*-*.dsc; do
        [[ -e "${file}" ]] || continue
        base="$(basename -- "${file}")"
        if [[ "${keep}" == *" ${base} "* ]]; then
            continue
        fi

        (
            cd "${package_dir}"
            osc remove --force "${base}" >/dev/null 2>&1 || rm -f -- "${base}"
        )
    done
}

stage_obs_files() {
    local package_dir="$1"
    shift

    (
        cd "${package_dir}"
        osc add "$@" >/dev/null 2>&1 || true
        osc status
    )
}

tmp_dir="$(mktemp -d "${SWAN_DICT_OBS_TMP_DIR:-${RUNNER_TEMP:-/tmp}/swan-dict-obs-src.XXXXXX}")"
trap 'rm -rf "${tmp_dir}"' EXIT

prepare_arch_plasma_source

SWAN_DICT_PREPARE_ARCH_DIGITAL_CLOCK_FALLBACK_OVERWRITE=1 \
    "${repo_root}/scripts/prepare-arch-digital-clock-fallback.sh"

package_dir="$(checkout_obs_package)"
source_dir="$(prepare_source_tree "${tmp_dir}")"

tar -C "${tmp_dir}" -cJf "${package_dir}/${generic_tarball}" "swan-dict-${version}"

cp -a "${repo_root}/packaging/obs/debian" "${source_dir}/debian"
tar -C "${tmp_dir}" --exclude="swan-dict-${version}/debian" \
    -cJf "${tmp_dir}/${debian_orig_tarball}" "swan-dict-${version}"

(
    cd "${tmp_dir}"
    dpkg-source -b "swan-dict-${version}"
)

cp "${tmp_dir}/${debian_orig_tarball}" "${package_dir}/"
cp "${tmp_dir}/${debian_source_tarball}" "${package_dir}/"
cp "${tmp_dir}/${debian_dsc}" "${package_dir}/"
cp "${repo_root}/packaging/obs/PKGBUILD" "${package_dir}/"
cp "${repo_root}/packaging/obs/swan-dict.spec" "${package_dir}/"
cp "${repo_root}/packaging/obs/_service" "${package_dir}/"

remove_old_versioned_sources \
    "${package_dir}" \
    "${generic_tarball}" \
    "${debian_orig_tarball}" \
    "${debian_source_tarball}" \
    "${debian_dsc}"

stage_obs_files \
    "${package_dir}" \
    "${generic_tarball}" \
    "${debian_orig_tarball}" \
    "${debian_source_tarball}" \
    "${debian_dsc}" \
    PKGBUILD \
    swan-dict.spec \
    _service

if [[ "${SWAN_DICT_OBS_COMMIT:-0}" == "1" ]]; then
    (
        cd "${package_dir}"
        osc commit -m "${SWAN_DICT_OBS_COMMIT_MESSAGE:-Release ${version}}"
    )
fi

echo "OBS package checkout: ${package_dir}"
