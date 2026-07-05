#!/usr/bin/env bash
# Download a PowerGov AppImage release and refresh user shortcuts (no root).
set -euo pipefail

TAG="${1:?release tag, e.g. v1.9.2}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CONFIG_DIR="${XDG_CONFIG_HOME:-${HOME}/.config}/powergov"
LOG="${CONFIG_DIR}/update-install.log"
VER="${TAG#v}"
VER="${VER#V}"

if [[ -z "${VER}" ]]; then
    echo "install-appimage-update: invalid tag ${TAG}" >&2
    exit 1
fi

if command -v xdg-user-dir >/dev/null 2>&1; then
    DOWN="$(xdg-user-dir DOWNLOAD 2>/dev/null || true)"
fi
if [[ -z "${DOWN:-}" || ! -d "${DOWN}" ]]; then
    for candidate in "${HOME}/Descargas" "${HOME}/Downloads"; do
        if [[ -d "${candidate}" ]]; then
            DOWN="${candidate}"
            break
        fi
    done
fi
DOWN="${DOWN:-${HOME}}"

FILE="${DOWN}/PowerGov-${VER}-x86_64.AppImage"
URL="https://github.com/IRodriguez13/powergov/releases/download/${TAG}/PowerGov-${VER}-x86_64.AppImage"
DESKTOP_HELPER="${SCRIPT_DIR}/install-appimage-desktop.sh"

mkdir -p "${CONFIG_DIR}" "${DOWN}"
exec >>"${LOG}" 2>&1
echo "== $(date -Iseconds) install ${TAG} -> ${FILE} =="

download() {
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL --retry 3 --retry-delay 2 -o "${FILE}.part" \
            -H "User-Agent: PowerGov-update/${VER}" \
            "${URL}"
        mv -f "${FILE}.part" "${FILE}"
        return 0
    fi
    if command -v wget >/dev/null 2>&1; then
        wget -qO "${FILE}.part" "${URL}"
        mv -f "${FILE}.part" "${FILE}"
        return 0
    fi
    echo "install-appimage-update: need curl or wget" >&2
    return 1
}

download
chmod +x "${FILE}"

if [[ -x "${DESKTOP_HELPER}" ]]; then
    TMP="$(mktemp -d)"
    (cd "${TMP}" && "${FILE}" --appimage-extract)
    "${DESKTOP_HELPER}" "${FILE}" "${TMP}/squashfs-root"
    rm -rf "${TMP}"
fi

if command -v notify-send >/dev/null 2>&1; then
    notify-send "PowerGov" "Actualización ${VER} instalada en ${FILE}" 2>/dev/null || true
fi

echo "== done ${FILE} =="
