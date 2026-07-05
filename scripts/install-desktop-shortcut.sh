#!/usr/bin/env bash
# Install a desktop shortcut for the current user (do not run as root).
set -euo pipefail

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

canonical_dir() {
    local d=$1
    if [[ -d "${d}" ]] && command -v realpath >/dev/null 2>&1; then
        realpath "${d}"
    else
        printf '%s' "${d}"
    fi
}

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
    echo "Desktop shortcut: ${shortcut}"
}

while IFS= read -r desktop_dir; do
    [[ -n "${desktop_dir}" ]] || continue
    install_desktop_shortcut "${SOURCE}" "${desktop_dir}"
done < <(resolve_desktop_dirs)
