#!/usr/bin/env bash
# Maintainer-only: build a precompiled AppImage for end users.
# Users download the .AppImage from GitHub Releases — they do not run this script.
# First launch: UI prompts to install the background service (pkexec, one-time).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

VERSION="$(tr -d '[:space:]' < VERSION)"
APPDIR="${ROOT}/AppDir"
OUT="${ROOT}/dist/PowerGov-${VERSION}-x86_64.AppImage"
CACHE="${ROOT}/.build/appimage"
LINUXDEPLOY="${CACHE}/linuxdeploy-x86_64.AppImage"
GTK_PLUGIN="${CACHE}/linuxdeploy-plugin-gtk.sh"
GTK_PLUGIN_REF="3b67a1d1c1b0c8268f57f2bce40fe2d33d409cea"
DESKTOP="${APPDIR}/powergov-ui.desktop"
ICON="${ROOT}/data/icons/hicolor/256x256/apps/powergov.png"
APPRUN="${ROOT}/scripts/AppRun.appimage"

fetch_tool() {
    local url="$1"
    local dest="$2"
    if [[ -x "${dest}" ]]; then
        return 0
    fi
    mkdir -p "$(dirname "${dest}")"
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL "${url}" -o "${dest}"
    elif command -v wget >/dev/null 2>&1; then
        wget -q -O "${dest}" "${url}"
    else
        echo "error: need curl or wget to fetch linuxdeploy" >&2
        exit 1
    fi
    chmod +x "${dest}"
}

echo "==> build powergov-ui + libpowergov.so + install staging"
make -s libpowergov.so powergov-ui
STAGING="${ROOT}/.staging/appimage"
chmod +x scripts/prepare-install-staging.sh scripts/install-service-resident.sh
./scripts/prepare-install-staging.sh "${STAGING}"

echo "==> fetch linuxdeploy (cached in .build/appimage/)"
fetch_tool \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" \
    "${LINUXDEPLOY}"
fetch_tool \
    "https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/${GTK_PLUGIN_REF}/linuxdeploy-plugin-gtk.sh" \
    "${GTK_PLUGIN}"

export APPIMAGE_EXTRACT_AND_RUN=1
export PATH="${CACHE}:${PATH}"

echo "==> prepare AppDir"
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin" "${APPDIR}/usr/lib" \
    "${APPDIR}/usr/lib/powergov/staging" \
    "${APPDIR}/usr/libexec/powergov"
cp "${ROOT}/powergov-ui" "${APPDIR}/usr/bin/"
cp "${ROOT}/libpowergov.so" "${APPDIR}/usr/lib/"
cp "${ROOT}/scripts/install-service-resident.sh" \
    "${APPDIR}/usr/libexec/powergov/install-service-resident.sh"
chmod +x "${APPDIR}/usr/libexec/powergov/install-service-resident.sh"

cat > "${DESKTOP}" <<EOF
[Desktop Entry]
Type=Application
Name=PowerGov
Name[es]=PowerGov
Comment=Linux laptop power management
Comment[es]=Gestión de energía para portátiles Linux
Exec=powergov-ui
Icon=powergov
Categories=System;Settings;
Terminal=false
StartupNotify=true
EOF

if [[ ! -f "${ICON}" ]]; then
    make -s icons
fi

export ARCH=x86_64
export VERSION="${VERSION}"
export LD_LIBRARY_PATH="${APPDIR}/usr/lib:${LD_LIBRARY_PATH:-}"
export DEPLOY_GTK_VERSION=3

echo "==> linuxdeploy (GTK3)"
"${LINUXDEPLOY}" --appdir "${APPDIR}" \
    --executable "${APPDIR}/usr/bin/powergov-ui" \
    --library "${APPDIR}/usr/lib/libpowergov.so" \
    --desktop-file "${DESKTOP}" \
    --icon-file "${ICON}" \
    --plugin gtk

echo "==> embed service staging (after gtk plugin)"
cp -a "${STAGING}/." "${APPDIR}/usr/lib/powergov/staging/"

echo "==> pack AppImage"
"${LINUXDEPLOY}" --appdir "${APPDIR}" \
    --output appimage \
    --custom-apprun "${APPRUN}"

mkdir -p "${ROOT}/dist"
mv -f PowerGov-"${VERSION}"-x86_64.AppImage "${OUT}" 2>/dev/null || \
    mv -f ./*.AppImage "${OUT}"
chmod +x "${OUT}"

echo "==> ${OUT}"
echo "note: ship this file in GitHub Releases; end users run it directly (no compile wait)"
