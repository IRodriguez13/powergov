#!/usr/bin/env bash
# Install powergov systemd service from a staging directory (run as root via pkexec).
set -euo pipefail

STAGING="${1:?staging directory with powergov binaries}"
LIBEXEC="/usr/local/libexec/powergov"
SYSTEMD_UNIT="/etc/systemd/system/powergov.service"
CONF="/etc/powergov.conf"

if [[ "${STAGING}" != /* ]]; then
    echo "install-service-resident: staging path must be absolute (got: ${STAGING})" >&2
    exit 1
fi

STAGING="$(readlink -f "${STAGING}")"

if [[ "$(id -u)" -ne 0 ]]; then
    echo "install-service-resident: must run as root" >&2
    exit 1
fi

for f in powergov libpowergov.so powergov.conf powergov.service org.powergov.policy dev-auth powergov-uninstall.sh remove-appimage-user-files.sh; do
    if [[ ! -f "${STAGING}/${f}" ]]; then
        echo "install-service-resident: missing ${STAGING}/${f}" >&2
        exit 1
    fi
done

install -D -m 755 "${STAGING}/powergov" /usr/local/bin/powergov
install -D -m 755 "${STAGING}/libpowergov.so" /usr/local/lib/libpowergov.so
install -D -m 644 "${STAGING}/powergov.conf" "${CONF}"
install -D -m 644 "${STAGING}/powergov.service" "${SYSTEMD_UNIT}"
install -D -m 644 "${STAGING}/org.powergov.policy" \
    /usr/share/polkit-1/actions/org.powergov.policy
install -D -m 755 "${STAGING}/dev-auth" "${LIBEXEC}/dev-auth"
install -D -m 755 "${STAGING}/powergov-uninstall.sh" \
    "${LIBEXEC}/powergov-uninstall.sh"
install -D -m 755 "${STAGING}/remove-appimage-user-files.sh" \
    "${LIBEXEC}/remove-appimage-user-files.sh"

SELF="$(readlink -f "$0")"
if [[ "${SELF}" != "${LIBEXEC}/install-service-resident.sh" ]]; then
    install -D -m 755 "${SELF}" "${LIBEXEC}/install-service-resident.sh"
fi

mkdir -p "${LIBEXEC}/staging"
cp -a "${STAGING}/." "${LIBEXEC}/staging/"

if command -v ldconfig >/dev/null 2>&1; then
    ldconfig
fi

if ! command -v systemctl >/dev/null 2>&1; then
    echo "install-service-resident: systemctl not found" >&2
    exit 1
fi

systemctl daemon-reload
systemctl enable powergov.service
systemctl restart powergov.service

exit 0
