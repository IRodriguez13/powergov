#!/usr/bin/env bash
# Remove powergov service and files installed via AppImage or install-powergov.sh.
set -euo pipefail

if [[ "$(id -u)" -ne 0 ]]; then
    exec sudo -E "$0" "$@"
fi

systemctl stop powergov.service 2>/dev/null || true
systemctl disable powergov.service 2>/dev/null || true

rm -f /etc/systemd/system/powergov.service
rm -f /etc/powergov.conf
rm -f /usr/local/bin/powergov /usr/local/bin/powergov-ui
rm -f /usr/local/lib/libpowergov.so
rm -f /usr/share/polkit-1/actions/org.powergov.policy
rm -rf /usr/local/libexec/powergov
rm -f /usr/share/applications/powergov-ui.desktop

systemctl daemon-reload
command -v ldconfig >/dev/null 2>&1 && ldconfig || true
command -v update-desktop-database >/dev/null 2>&1 && \
    update-desktop-database /usr/share/applications || true

if [[ -n "${SUDO_USER:-}" ]]; then
    home="$(getent passwd "$SUDO_USER" | cut -d: -f6)"
    rm -f "$home/Desktop/powergov-ui.desktop" \
        "$home/.local/share/applications/powergov-ui.desktop" \
        "${XDG_DESKTOP_DIR:-$home/Desktop}/powergov-ui.desktop" 2>/dev/null || true
fi

echo "PowerGov desinstalado."
