#!/usr/bin/env bash
# User-scope menu + desktop shortcut for the PowerGov AppImage (no root).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=powergov-xdg-paths.sh
. "${SCRIPT_DIR}/powergov-xdg-paths.sh"

APPIMAGE="${1:-${APPIMAGE:-}}"
APPDIR="${2:-${APPDIR:-}}"

if [[ -z "${APPIMAGE}" || ! -f "${APPIMAGE}" ]]; then
    exit 0
fi

if [[ "${EUID}" -eq 0 ]]; then
    exit 0
fi

desktop_escape() {
    local s=$1
    s=${s//\\/\\\\}
    s=${s//\"/\\\"}
    printf '%s' "$s"
}

# Resolve desktop dirs via XDG (locale-safe); create default if missing.
resolve_desktop_dirs() {
    local -a existing=()
    local d

    while IFS= read -r d; do
        [[ -n "${d}" ]] && existing+=("${d}")
    done < <(pg_desktop_dirs)

    if [[ ${#existing[@]} -eq 0 ]]; then
        d="$(pg_default_desktop_dir)" && existing=("${d}")
    fi

    printf '%s\n' "${existing[@]}"
}

install_desktop_shortcut() {
    local source=$1
    local dir=$2
    local shortcut="${dir}/powergov-ui.desktop"

    install -m 644 "${source}" "${shortcut}"
    if command -v gio >/dev/null 2>&1; then
        gio set "${shortcut}" metadata::trusted true 2>/dev/null || true
    fi
}

appimage_version() {
    local bin=$1
    local line ver

    [[ -x "${bin}" ]] || return 1
    line="$("${bin}" --version 2>/dev/null | head -1 || true)"
    ver="$(printf '%s' "${line}" | sed -n 's/.*) \([0-9][0-9.]*\).*/\1/p')"
    [[ -n "${ver}" ]] || return 1
    printf '%s' "${ver}"
}

version_newer() {
    local a=$1
    local b=$2
    local IFS=.
    local -a av bv
    local i

    IFS=. read -r -a av <<< "${a}"
    IFS=. read -r -a bv <<< "${b}"
    for i in 0 1 2; do
        local ai=${av[$i]:-0}
        local bi=${bv[$i]:-0}
        (( 10#${ai} > 10#${bi} )) && return 0
        (( 10#${ai} < 10#${bi} )) && return 1
    done
    return 1
}

should_replace_canonical() {
    local src=$1

    [[ ! -f "${CANONICAL}" ]] && return 0
    local sv dv
    sv="$(appimage_version "${src}" || true)"
    dv="$(appimage_version "${CANONICAL}" || true)"
    [[ -z "${sv}" || -z "${dv}" ]] && return 0
    version_newer "${sv}" "${dv}"
}

DATA_HOME="${XDG_DATA_HOME:-${HOME}/.local/share}"
APPL_DIR="${DATA_HOME}/applications"
ICON_ROOT="${DATA_HOME}/icons/hicolor"
CANONICAL_DIR="${DATA_HOME}/powergov"
CANONICAL="${CANONICAL_DIR}/PowerGov.AppImage"
DESKTOP_FILE="${APPL_DIR}/powergov-ui.desktop"
CONFIG_DIR="${XDG_CONFIG_HOME:-${HOME}/.config}/powergov"
MARKER="${CONFIG_DIR}/appimage-desktop"

mkdir -p "${CONFIG_DIR}"

# Prefer a single install location; migrate shortcuts from a download-folder copy.
if [[ -f "${APPIMAGE}" && "${APPIMAGE}" != "${CANONICAL}" ]]; then
    mkdir -p "${CANONICAL_DIR}"
    if should_replace_canonical "${APPIMAGE}"; then
        cp -f "${APPIMAGE}" "${CANONICAL}.part" && mv -f "${CANONICAL}.part" "${CANONICAL}"
        chmod +x "${CANONICAL}"
    fi
    APPIMAGE="${CANONICAL}"
fi

if [[ -f "${MARKER}" && -f "${DESKTOP_FILE}" ]]; then
    if [[ "$(tr -d '[:space:]' < "${MARKER}")" == "${APPIMAGE}" ]]; then
        exit 0
    fi
fi

if [[ -n "${APPDIR}" && -d "${APPDIR}/usr/share/icons/hicolor" ]]; then
    shopt -s nullglob
    for icon in "${APPDIR}/usr/share/icons/hicolor/"*/*/apps/powergov.png; do
        sz="$(basename "$(dirname "$(dirname "${icon}")")")"
        install -D -m 644 "${icon}" "${ICON_ROOT}/${sz}/apps/powergov.png"
    done
    if [[ -f "${APPDIR}/usr/share/icons/hicolor/scalable/apps/powergov.svg" ]]; then
        install -D -m 644 \
            "${APPDIR}/usr/share/icons/hicolor/scalable/apps/powergov.svg" \
            "${ICON_ROOT}/scalable/apps/powergov.svg"
    fi
    shopt -u nullglob
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -f -t "${ICON_ROOT}" 2>/dev/null || true
    fi
fi

exec_path="$(desktop_escape "${APPIMAGE}")"
if [[ "${APPIMAGE}" == *" "* ]]; then
    exec_line="Exec=\"${exec_path}\""
else
    exec_line="Exec=${exec_path}"
fi

mkdir -p "${APPL_DIR}"
cat > "${DESKTOP_FILE}" <<EOF
[Desktop Entry]
Type=Application
Name=PowerGov
Name[es]=PowerGov
Comment=Linux laptop power management
Comment[es]=Gestión de energía para portátiles Linux
${exec_line}
Icon=powergov
Categories=System;Settings;
Terminal=false
StartupNotify=true
EOF
chmod 644 "${DESKTOP_FILE}"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${APPL_DIR}" 2>/dev/null || true
fi

while IFS= read -r desktop_dir; do
    [[ -n "${desktop_dir}" ]] || continue
    install_desktop_shortcut "${DESKTOP_FILE}" "${desktop_dir}" || true
done < <(resolve_desktop_dirs)

printf '%s\n' "${APPIMAGE}" > "${MARKER}"
