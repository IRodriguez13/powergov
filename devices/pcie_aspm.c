#include "pcie_aspm.h"
#include "../core/sysfs.h"
#include "../log/log.h"
#include "../metrics/metrics.h"
#include <string.h>

#define PCIE_ASPM_POLICY "/sys/module/pcie_aspm/parameters/policy"

static int g_aggression;
static char g_saved[32];
static int g_have_saved;

int pcie_aspm_available(void)
{
    return sysfs_path_exists(PCIE_ASPM_POLICY);
}

int pcie_aspm_apply(int aggression)
{
    const char *target;
    char cur[32];

    if (aggression <= 0)
        return pcie_aspm_restore();

    if (!pcie_aspm_available())
    {
        powergov_metrics_apply(POWERGOV_FEATURE_PCIE_ASPM, 0, 1);
        return -1;
    }

    target = (aggression >= 2) ? "powersave\n" : "default\n";

    if (g_aggression == aggression && g_have_saved)
    {
        powergov_metrics_apply(POWERGOV_FEATURE_PCIE_ASPM, 1, 1);
        return 0;
    }

    if (!g_have_saved &&
        sysfs_read_file(PCIE_ASPM_POLICY, g_saved, sizeof(g_saved)) == 0)
        g_have_saved = 1;

    if (sysfs_read_file(PCIE_ASPM_POLICY, cur, sizeof(cur)) == 0 &&
        strncmp(cur, target, strlen(target)) == 0)
    {
        g_aggression = aggression;
        powergov_metrics_apply(POWERGOV_FEATURE_PCIE_ASPM, 1, 1);
        return 0;
    }

    if (sysfs_write_file(PCIE_ASPM_POLICY, target) != 0)
    {
        powergov_metrics_apply(POWERGOV_FEATURE_PCIE_ASPM, 0, 0);
        return -1;
    }

    g_aggression = aggression;
    PG_LOG_I("pcie_aspm", "policy=%s", target);
    powergov_metrics_apply(POWERGOV_FEATURE_PCIE_ASPM, 1, 0);
    return 0;
}

int pcie_aspm_restore(void)
{
    if (g_have_saved)
        sysfs_write_file(PCIE_ASPM_POLICY, g_saved);

    g_aggression = 0;
    g_have_saved = 0;
    g_saved[0] = '\0';
    powergov_metrics_apply(POWERGOV_FEATURE_PCIE_ASPM, 1, 0);
    return 0;
}
