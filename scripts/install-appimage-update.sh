#!/usr/bin/env bash
# Download a PowerGov AppImage release and refresh user shortcuts (no root).
# Always installs to a single canonical path (overwrite) to avoid duplicates.
set -euo pipefail

TAG="${1:?release tag, e.g. v1.9.2}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=powergov-xdg-paths.sh
. "${SCRIPT_DIR}/powergov-xdg-paths.sh"
DATA_HOME="${XDG_DATA_HOME:-${HOME}/.local/share}"
CONFIG_DIR="${XDG_CONFIG_HOME:-${HOME}/.config}/powergov"
CANONICAL_DIR="${DATA_HOME}/powergov"
CANONICAL="${CANONICAL_DIR}/PowerGov.AppImage"
NEW="${CANONICAL}.new"
PENDING="${CONFIG_DIR}/appimage-pending"
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

running_path() {
    local exe target

    if [[ -n "${APPIMAGE:-}" && -f "${APPIMAGE}" ]]; then
        printf '%s' "${APPIMAGE}"
        return 0
    fi

    exe="/proc/self/exe"
    if [[ -L "${exe}" ]]; then
        target="$(readlink -f "${exe}" 2>/dev/null || true)"
        if [[ -n "${target}" ]]; then
            printf '%s' "${target}"
            return 0
        fi
    fi
    return 1
}

remove_if_safe() {
    local path=$1
    local running

    running="$(running_path 2>/dev/null || true)"

    [[ -z "${path}" || ! -f "${path}" ]] && return 0
    [[ "${path}" == "${CANONICAL}" ]] && return 0
    [[ -n "${running}" && "${path}" == "${running}" ]] && return 0

    rm -f "${path}" && echo "removed old ${path}" || echo "keep old (in use?) ${path}"
}

download() {
    rm -f "${NEW}"
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL --retry 3 --retry-delay 2 -o "${NEW}" \
            -H "User-Agent: PowerGov-update/${VER}" \
            "${URL}"
        return 0
    fi
    if command -v wget >/dev/null 2>&1; then
        wget -qO "${NEW}" "${URL}"
        return 0
    fi
    echo "install-appimage-update: need curl or wget" >&2
    return 1
}

apply_download() {
    local running

    running="$(running_path 2>/dev/null || true)"
    chmod +x "${NEW}"

    if [[ -n "${running}" && "${running}" == "${CANONICAL}" ]]; then
        printf '%s\n' "${NEW}" > "${PENDING}"
        echo "pending replace (restart PowerGov to finish): ${NEW}"
        return 0
    fi

    mv -f "${NEW}" "${CANONICAL}"
    rm -f "${PENDING}"
    echo "installed ${CANONICAL}"
}

OLD="$(read_old_marker || true)"
download
apply_download

if [[ -f "${CANONICAL}" ]]; then
    chmod +x "${CANONICAL}"
fi

if [[ -x "${DESKTOP_HELPER}" && -f "${CANONICAL}" ]]; then
    TMP="$(mktemp -d)"
    (cd "${TMP}" && "${CANONICAL}" --appimage-extract)
    "${DESKTOP_HELPER}" "${CANONICAL}" "${TMP}/squashfs-root"
    rm -rf "${TMP}"
fi

if [[ -n "${OLD}" && "${OLD}" != "${CANONICAL}" ]]; then
    remove_if_safe "${OLD}"
fi

while IFS= read -r dir; do
    [[ -n "${dir}" ]] || continue
    shopt -s nullglob
    for old in "${dir}"/PowerGov-*-x86_64.AppImage; do
        remove_if_safe "${old}"
    done
    shopt -u nullglob
done < <(pg_download_dirs)

if command -v notify-send >/dev/null 2>&1; then
    if [[ -f "${PENDING}" ]]; then
        notify-send "PowerGov" \
            "Descarga lista (${VER}). Cerrá y volvé a abrir PowerGov para terminar." \
            2>/dev/null || true
    else
        notify-send "PowerGov" "Actualizado a ${VER} (${CANONICAL})" \
            2>/dev/null || true
    fi
fi

echo "== done ${CANONICAL} =="
