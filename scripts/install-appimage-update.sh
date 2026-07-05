#!/usr/bin/env bash
# Download a PowerGov AppImage release and refresh user shortcuts (no root).
# Always installs to a single canonical path (overwrite) to avoid duplicates.
set -euo pipefail

TAG="${1:?release tag, e.g. v1.9.2}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DATA_HOME="${XDG_DATA_HOME:-${HOME}/.local/share}"
CONFIG_DIR="${XDG_CONFIG_HOME:-${HOME}/.config}/powergov"
CANONICAL_DIR="${DATA_HOME}/powergov"
CANONICAL="${CANONICAL_DIR}/PowerGov.AppImage"
MARKER="${CONFIG_DIR}/appimage-desktop"
LOG="${CONFIG_DIR}/update-install.log"
VER="${TAG#v}"
VER="${VER#V}"

if [[ -z "${VER}" ]]; then
    echo "install-appimage-update: invalid tag ${TAG}" >&2
    exit 1
fi

URL="https://github.com/IRodriguez13/powergov/releases/download/${TAG}/PowerGov-${VER}-x86_64.AppImage"
DESKTOP_HELPER="${SCRIPT_DIR}/install-appimage-desktop.sh"

mkdir -p "${CONFIG_DIR}" "${CANONICAL_DIR}"
exec >>"${LOG}" 2>&1
echo "== $(date -Iseconds) install ${TAG} -> ${CANONICAL} =="

read_old_marker() {
    if [[ -f "${MARKER}" ]]; then
        tr -d '[:space:]' < "${MARKER}"
    fi
}

remove_if_safe() {
    local path=$1
    local running="${APPIMAGE:-}"

    [[ -z "${path}" || ! -f "${path}" ]] && return 0
    [[ "${path}" == "${CANONICAL}" ]] && return 0
    [[ -n "${running}" && "${path}" == "${running}" ]] && return 0

    rm -f "${path}" && echo "removed old ${path}" || echo "keep old (in use?) ${path}"
}

download() {
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL --retry 3 --retry-delay 2 -o "${CANONICAL}.part" \
            -H "User-Agent: PowerGov-update/${VER}" \
            "${URL}"
        mv -f "${CANONICAL}.part" "${CANONICAL}"
        return 0
    fi
    if command -v wget >/dev/null 2>&1; then
        wget -qO "${CANONICAL}.part" "${URL}"
        mv -f "${CANONICAL}.part" "${CANONICAL}"
        return 0
    fi
    echo "install-appimage-update: need curl or wget" >&2
    return 1
}

OLD="$(read_old_marker || true)"
download
chmod +x "${CANONICAL}"

if [[ -x "${DESKTOP_HELPER}" ]]; then
    TMP="$(mktemp -d)"
    (cd "${TMP}" && "${CANONICAL}" --appimage-extract)
    "${DESKTOP_HELPER}" "${CANONICAL}" "${TMP}/squashfs-root"
    rm -rf "${TMP}"
fi

if [[ -n "${OLD}" && "${OLD}" != "${CANONICAL}" ]]; then
    remove_if_safe "${OLD}"
fi

# Drop versioned duplicates left in common download folders from older updaters.
if command -v xdg-user-dir >/dev/null 2>&1; then
    DOWN="$(xdg-user-dir DOWNLOAD 2>/dev/null || true)"
fi
for dir in "${DOWN:-}" "${HOME}/Descargas" "${HOME}/Downloads"; do
    [[ -z "${dir}" || ! -d "${dir}" ]] && continue
    shopt -s nullglob
    for old in "${dir}"/PowerGov-*-x86_64.AppImage; do
        remove_if_safe "${old}"
    done
    shopt -u nullglob
done

if command -v notify-send >/dev/null 2>&1; then
    notify-send "PowerGov" "Actualizado a ${VER} (${CANONICAL})" 2>/dev/null || true
fi

echo "== done ${CANONICAL} =="
