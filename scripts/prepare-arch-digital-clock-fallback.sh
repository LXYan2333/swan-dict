#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cache_dir="${SWAN_DICT_ARCH_PLASMA_WORKSPACE_CACHE:-${repo_root}/.cache/arch-plasma-workspace}"
package_dir="${SWAN_DICT_ARCH_PLASMA_WORKSPACE_PACKAGE_DIR:-${cache_dir}/plasma-workspace}"
source_dir="${SWAN_DICT_ARCH_PLASMA_WORKSPACE_SOURCE_DIR:-}"
target="${repo_root}/applet/contents"
patch_dir="${SWAN_DICT_ARCH_DIGITAL_CLOCK_PATCH_DIR:-${repo_root}/patches/arch-plasma-6.6.5}"
allow_overwrite="${SWAN_DICT_PREPARE_ARCH_DIGITAL_CLOCK_FALLBACK_OVERWRITE:-0}"

has_existing_generated_files=false
if [[ -d "${target}/config" ]]; then
    has_existing_generated_files=true
elif [[ -d "${target}/ui" ]] && compgen -G "${target}/ui/*.qml" > /dev/null; then
    has_existing_generated_files=true
fi

if [[ "${has_existing_generated_files}" == true && "${allow_overwrite}" != "1" ]]; then
    echo "Digital Clock generated files already exist; not overwriting." >&2
    echo "Set SWAN_DICT_PREPARE_ARCH_DIGITAL_CLOCK_FALLBACK_OVERWRITE=1 to refresh the Arch fallback." >&2
    exit 1
fi

if [[ -z "${source_dir}" ]]; then
    if ! command -v pkgctl >/dev/null 2>&1; then
        echo "pkgctl not found. Install Arch devtools or set SWAN_DICT_ARCH_PLASMA_WORKSPACE_SOURCE_DIR." >&2
        exit 1
    fi

    if ! command -v makepkg >/dev/null 2>&1; then
        echo "makepkg not found. Install Arch pacman tools or set SWAN_DICT_ARCH_PLASMA_WORKSPACE_SOURCE_DIR." >&2
        exit 1
    fi

    mkdir -p "${cache_dir}"

    if [[ ! -d "${package_dir}/.git" ]]; then
        (
            cd "${cache_dir}"
            pkgctl repo clone plasma-workspace
        )
    else
        (
            cd "${package_dir}"
            git pull --ff-only
        )
    fi

    (
        cd "${package_dir}"
        makepkg --nobuild --nodeps
    )

    source_dir="${package_dir}/src"
fi

digital_clock_contents="$(
    find "${source_dir}" -type d \( \
        -path '*/applets/digital-clock/package/contents' \
        -o -path '*/applets/digital-clock' \
    \) -print -quit
)"

if [[ -z "${digital_clock_contents}" || ! -d "${digital_clock_contents}" ]]; then
    echo "Digital Clock package contents not found in Arch plasma-workspace source." >&2
    exit 1
fi

if [[ ! -f "${digital_clock_contents}/DigitalClock.qml" && ! -f "${digital_clock_contents}/ui/DigitalClock.qml" ]]; then
    echo "Digital Clock QML not found in: ${digital_clock_contents}" >&2
    exit 1
fi

SWAN_DICT_DIGITAL_CLOCK_SOURCE="${digital_clock_contents}" \
SWAN_DICT_DIGITAL_CLOCK_PATCH_DIR="${patch_dir}" \
SWAN_DICT_SYNC_DIGITAL_CLOCK_OVERWRITE=1 \
    "${repo_root}/scripts/sync-digital-clock.sh"

echo "Prepared Arch Digital Clock fallback from: ${digital_clock_contents}"
