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

DESKTOP_DIR="${XDG_DESKTOP_DIR:-${HOME}/Desktop}"
mkdir -p "${DESKTOP_DIR}"
TARGET="${DESKTOP_DIR}/powergov-ui.desktop"

if command -v xdg-desktop-icon >/dev/null 2>&1; then
    xdg-desktop-icon install "${SOURCE}"
    TARGET="${DESKTOP_DIR}/powergov-ui.desktop"
else
    install -m 644 "${SOURCE}" "${TARGET}"
fi

if command -v gio >/dev/null 2>&1 && [[ -f "${TARGET}" ]]; then
    gio set "${TARGET}" metadata::trusted true 2>/dev/null || true
fi

echo "Desktop shortcut: ${TARGET}"
