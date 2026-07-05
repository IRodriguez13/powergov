#!/usr/bin/env bash
# User-scope menu + desktop shortcut for the PowerGov AppImage (no root).
set -euo pipefail

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

canonical_dir() {
    local d=$1
    if [[ -d "${d}" ]] && command -v realpath >/dev/null 2>&1; then
        realpath "${d}"
    else
        printf '%s' "${d}"
    fi
}

# Resolve every existing user desktop dir (Escritorio/Desktop/locale via xdg).
# Prints one path per line; creates a sensible default if none exist.
resolve_desktop_dirs() {
    local -a candidates=()
    local primary="" d canon

    if command -v xdg-user-dir >/dev/null 2>&1; then
        primary="$(xdg-user-dir DESKTOP 2>/dev/null || true)"
        [[ -n "${primary}" ]] && candidates+=("${primary}")
    fi

    if [[ -n "${XDG_DESKTOP_DIR:-}" ]]; then
        candidates+=("${XDG_DESKTOP_DIR}")
    fi
    candidates+=("${HOME}/Desktop" "${HOME}/Escritorio")

    declare -A seen_dirs=()
    local -a existing=()

    for d in "${candidates[@]}"; do
        [[ -z "${d}" || "${d}" == "/" ]] && continue
        canon="$(canonical_dir "${d}")"
        [[ -n "${canon}" && -z "${seen_dirs[${canon}]+x}" ]] || continue
        seen_dirs["${canon}"]=1
        if [[ -d "${d}" ]]; then
            existing+=("${d}")
        fi
    done

    if [[ ${#existing[@]} -eq 0 ]]; then
        if [[ -z "${primary}" ]] && command -v xdg-user-dir >/dev/null 2>&1; then
            primary="$(xdg-user-dir DESKTOP 2>/dev/null || true)"
        fi
        if [[ -z "${primary}" ]]; then
            primary="${XDG_DESKTOP_DIR:-${HOME}/Desktop}"
        fi
        mkdir -p "${primary}"
        existing=("${primary}")
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

DATA_HOME="${XDG_DATA_HOME:-${HOME}/.local/share}"
APPL_DIR="${DATA_HOME}/applications"
ICON_ROOT="${DATA_HOME}/icons/hicolor"
DESKTOP_FILE="${APPL_DIR}/powergov-ui.desktop"
CONFIG_DIR="${XDG_CONFIG_HOME:-${HOME}/.config}/powergov"
MARKER="${CONFIG_DIR}/appimage-desktop"

mkdir -p "${CONFIG_DIR}"

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
