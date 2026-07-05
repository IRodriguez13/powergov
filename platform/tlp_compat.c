#include "tlp_compat.h"
#include <unistd.h>
#include <stdlib.h>

static int tlp_service_active(void)
{
    int rc;

    rc = system("systemctl is-active --quiet tlp.service 2>/dev/null");
    return (rc == 0);
}

static int tlp_binary_present(void)
{
    return access("/usr/sbin/tlp", X_OK) == 0 ||
           access("/usr/bin/tlp", X_OK) == 0;
}

static int tlp_config_present(void)
{
    return access("/etc/tlp.conf", F_OK) == 0 ||
           access("/etc/default/tlp", F_OK) == 0;
}

int tlp_active(void)
{
    if (tlp_service_active())
        return 1;

    if (access("/run/tlp", F_OK) == 0 && (tlp_config_present() || tlp_binary_present()))
        return 1;

    if (tlp_binary_present() && tlp_config_present())
    {
        /* tlp stat exits 0 when the stack is usable even if service name differs */
        if (system("tlp stat >/dev/null 2>&1") == 0)
            return 1;
    }

    return 0;
}

int tlp_defers_feature(powergov_feature_id_t id)
{
    if (!tlp_active())
        return 0;

    switch (id)
    {
    case POWERGOV_FEATURE_RUNTIME_PM:
    case POWERGOV_FEATURE_PERIPHERAL_PM:
    case POWERGOV_FEATURE_DISK_PM:
    case POWERGOV_FEATURE_PCIE_ASPM:
    case POWERGOV_FEATURE_BLUETOOTH_PM:
        return 1;
    default:
        return 0;
    }
}
