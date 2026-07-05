#!/usr/bin/env bash
# Install a desktop shortcut for the current user (do not run as root).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=powergov-xdg-paths.sh
. "${SCRIPT_DIR}/powergov-xdg-paths.sh"

SOURCE="${1:-/usr/share/applications/powergov-ui.desktop}"

if [[ "${EUID}" -eq 0 ]]; then
    echo "install-desktop-shortcut: run as your user, not with sudo." >&2
    echo "Example: make install-ui-shortcut" >&2
    exit 1
fi

if [[ ! -f "${SOURCE}" ]]; then
    echo "install-desktop-shortcut: missing ${SOURCE}" >&2
    echo "Run: sudo make install-ui" >&2
    exit 1
fi

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
    echo "Desktop shortcut: ${shortcut}"
}

while IFS= read -r desktop_dir; do
    [[ -n "${desktop_dir}" ]] || continue
    install_desktop_shortcut "${SOURCE}" "${desktop_dir}"
done < <(resolve_desktop_dirs)
