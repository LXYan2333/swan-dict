#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
upstream="${SWAN_DICT_DIGITAL_CLOCK_SOURCE:-/usr/share/plasma/plasmoids/org.kde.plasma.digitalclock/contents}"
target="${repo_root}/applet/contents"
patch_dir="${repo_root}/patches"
allow_overwrite="${SWAN_DICT_SYNC_DIGITAL_CLOCK_OVERWRITE:-0}"

if [[ ! -d "${upstream}" ]]; then
    echo "Digital Clock source not found: ${upstream}" >&2
    exit 1
fi

if ! compgen -G "${patch_dir}/*.patch" > /dev/null; then
    echo "No patch files found in: ${patch_dir}" >&2
    exit 1
fi

has_existing_generated_files=false
if [[ -d "${target}/config" ]]; then
    has_existing_generated_files=true
fi
for upstream_file in "${upstream}/ui/"*.qml; do
    target_file="${target}/ui/$(basename -- "${upstream_file}")"
    if [[ -e "${target_file}" ]]; then
        has_existing_generated_files=true
        break
    fi
done

if [[ "${has_existing_generated_files}" == true && "${allow_overwrite}" != "1" ]]; then
    echo "Digital Clock generated files already exist; not overwriting." >&2
    echo "Set SWAN_DICT_SYNC_DIGITAL_CLOCK_OVERWRITE=1 to refresh them from upstream and reapply patches." >&2
    exit 0
fi

rm -rf "${target}/config"
mkdir -p "${target}/ui"
cp -a "${upstream}/config" "${target}/"
for upstream_file in "${upstream}/ui/"*.qml; do
    cp -a "${upstream_file}" "${target}/ui/"
done

for patch_file in "${patch_dir}"/*.patch; do
    patch -d "${repo_root}/applet" -p1 < "${patch_file}"
done
