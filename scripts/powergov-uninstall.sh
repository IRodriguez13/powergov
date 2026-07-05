#!/usr/bin/env bash
# Remove powergov service and user shortcuts (AppImage or install-powergov.sh).
set -euo pipefail

LIBEXEC="/usr/local/libexec/powergov"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

cleanup_user_scope() {
    local user="${1:-}"
    local home remove_script

    [[ -z "${user}" ]] && return 0
    home="$(getent passwd "${user}" | cut -d: -f6)"
    [[ -z "${home}" || ! -d "${home}" ]] && return 0

    remove_script="${SCRIPT_DIR}/remove-appimage-user-files.sh"
    if [[ ! -x "${remove_script}" && -x "${LIBEXEC}/remove-appimage-user-files.sh" ]]; then
        remove_script="${LIBEXEC}/remove-appimage-user-files.sh"
    fi

    if [[ -x "${remove_script}" ]]; then
        if command -v runuser >/dev/null 2>&1; then
            runuser -u "${user}" -- env HOME="${home}" \
                USER="${user}" LOGNAME="${user}" \
                bash "${remove_script}" 2>/dev/null || true
        else
            su - "${user}" -c "HOME='${home}' bash '${remove_script}'" \
                2>/dev/null || true
        fi
        return 0
    fi

    # Fallback when helper script missing: XDG paths only (no hardcoded locale names).
    local xdg_script="${SCRIPT_DIR}/powergov-xdg-paths.sh"
    if [[ -f "${xdg_script}" ]]; then
        # shellcheck source=powergov-xdg-paths.sh
        . "${xdg_script}"
        rm -f "${home}/.local/share/applications/powergov-ui.desktop" \
            "${home}/.config/powergov/appimage-desktop" 2>/dev/null || true
        while IFS= read -r d; do
            [[ -n "${d}" ]] || continue
            rm -f "${d}/powergov-ui.desktop" 2>/dev/null || true
        done < <(HOME="${home}" pg_desktop_dirs)
        return 0
    fi

    rm -f "${home}/.local/share/applications/powergov-ui.desktop" \
        "${home}/.config/powergov/appimage-desktop" 2>/dev/null || true
}

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
rm -f /usr/share/applications/powergov-ui.desktop

if [[ -d "${LIBEXEC}" ]]; then
    rm -rf "${LIBEXEC}"
fi

systemctl daemon-reload
command -v ldconfig >/dev/null 2>&1 && ldconfig || true
command -v update-desktop-database >/dev/null 2>&1 && \
    update-desktop-database /usr/share/applications || true

if [[ -n "${SUDO_USER:-}" ]]; then
    cleanup_user_scope "${SUDO_USER}"
fi

echo "PowerGov uninstalled."
