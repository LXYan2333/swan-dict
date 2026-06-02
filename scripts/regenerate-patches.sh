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
    "configAppearance.qml:0006-config-appearance-digital-clock-i18n.patch"
    "configCalendar.qml:0007-config-calendar-digital-clock-i18n.patch"
    "configTimeZones.qml:0008-config-time-zones-digital-clock-i18n.patch"
    "CalendarView.qml:0009-calendar-view-digital-clock-i18n.patch"
    "NoTimezoneWarning.qml:0010-no-timezone-warning-digital-clock-i18n.patch"
)

for entry in "${files[@]}"; do
    relative_name="${entry%%:*}"
    patch_name="${entry#*:}"
    upstream_file="${upstream}/ui/${relative_name}"
    current_file="${current}/ui/${relative_name}"
    patch_file="${patch_dir}/${patch_name}"
    if [[ "${relative_name}" == ../* ]]; then
        normalized_name="${relative_name#../}"
    else
        normalized_name="ui/${relative_name}"
    fi

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
        if [[ -f "${patch_file}" ]]; then
            rm -f "${patch_file}"
            echo "removed: ${patch_file}"
        else
            echo "unchanged: ${normalized_name}"
        fi
    else
        generated_patch="${tmp_dir}/${patch_name}"
        (
            cd "${tmp_dir}"
            diff -u \
                --label "upstream/contents/${normalized_name}" \
                --label "current/contents/${normalized_name}" \
                "upstream/contents/${normalized_name}" \
                "current/contents/${normalized_name}" > "${generated_patch}" || true
        )

        if [[ -f "${patch_file}" ]] && cmp -s "${generated_patch}" "${patch_file}"; then
            echo "unchanged: ${patch_file}"
        else
            cp "${generated_patch}" "${patch_file}"
            echo "updated: ${patch_file}"
        fi
    fi
done
