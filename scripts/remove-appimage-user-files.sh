#!/usr/bin/env bash
# Remove user-scope AppImage shortcuts (menu, desktop, icons, marker). No root.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=powergov-xdg-paths.sh
. "${SCRIPT_DIR}/powergov-xdg-paths.sh"

if [[ "${EUID}" -eq 0 ]]; then
    exit 0
fi

DATA_HOME="${XDG_DATA_HOME:-${HOME}/.local/share}"
APPL_DIR="${DATA_HOME}/applications"
ICON_ROOT="${DATA_HOME}/icons/hicolor"
CONFIG_DIR="${XDG_CONFIG_HOME:-${HOME}/.config}/powergov"

rm -f "${APPL_DIR}/powergov-ui.desktop"
rm -f "${CONFIG_DIR}/appimage-desktop"

while IFS= read -r desktop_dir; do
    [[ -n "${desktop_dir}" ]] || continue
    rm -f "${desktop_dir}/powergov-ui.desktop"
done < <(pg_desktop_dirs)

shopt -s nullglob
for icon in "${ICON_ROOT}"/*/apps/powergov.png; do
    rm -f "${icon}"
done
rm -f "${ICON_ROOT}/scalable/apps/powergov.svg"
shopt -u nullglob

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "${ICON_ROOT}" 2>/dev/null || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${APPL_DIR}" 2>/dev/null || true
fi
