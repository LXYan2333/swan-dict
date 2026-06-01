#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
upstream="${SWAN_DICT_DIGITAL_CLOCK_SOURCE:-/usr/share/plasma/plasmoids/org.kde.plasma.digitalclock/contents}"
current="${repo_root}/applet/contents"
patch_dir="${repo_root}/patches"
tmp_dir="$(mktemp -d)"

cleanup() {
    rm -rf "${tmp_dir}"
}
trap cleanup EXIT

if [[ ! -d "${upstream}" ]]; then
    echo "Digital Clock source not found: ${upstream}" >&2
    exit 1
fi

if [[ ! -d "${current}" ]]; then
    echo "Current applet contents not found: ${current}" >&2
    exit 1
fi

mkdir -p "${tmp_dir}/upstream/contents/config" "${tmp_dir}/upstream/contents/ui"
mkdir -p "${tmp_dir}/current/contents/config" "${tmp_dir}/current/contents/ui" "${patch_dir}"

files=(
    "DigitalClock.qml:0001-digital-clock-qml-date-label.patch"
    "Tooltip.qml:0002-tooltip-qml-dictionary-content.patch"
    "main.qml:0003-main-qml-swan-dict-wiring.patch"
    "../config/config.qml:0004-config-qml-translation-page.patch"
    "../config/main.xml:0005-main-xml-translation-settings.patch"
)

for entry in "${files[@]}"; do
    relative_name="${entry%%:*}"
    patch_name="${entry#*:}"
    upstream_file="${upstream}/ui/${relative_name}"
    current_file="${current}/ui/${relative_name}"
    patch_file="${patch_dir}/${patch_name}"
    normalized_name="${relative_name#../}"

    if [[ ! -f "${upstream_file}" ]]; then
        echo "Upstream file not found: ${upstream_file}" >&2
        exit 1
    fi
    if [[ ! -f "${current_file}" ]]; then
        echo "Current file not found: ${current_file}" >&2
        exit 1
    fi

    cp "${upstream_file}" "${tmp_dir}/upstream/contents/${normalized_name}"
    cp "${current_file}" "${tmp_dir}/current/contents/${normalized_name}"

    if diff -q "${tmp_dir}/upstream/contents/${normalized_name}" "${tmp_dir}/current/contents/${normalized_name}" >/dev/null; then
        rm -f "${patch_file}"
        echo "unchanged: ${normalized_name}"
    else
        (
            cd "${tmp_dir}"
            diff -u "upstream/contents/${normalized_name}" "current/contents/${normalized_name}" > "${patch_file}" || true
        )
        echo "updated: ${patch_file}"
    fi
done
