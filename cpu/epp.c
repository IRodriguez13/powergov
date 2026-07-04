#include "epp.h"
#include "../core/sysfs.h"
#include "../log/log.h"
#include "../metrics/metrics.h"
#include <stdio.h>
#include <string.h>

int cpu_epp_available(void)
{
    char path[256];
    snprintf(path, sizeof(path),
             POWERGOV_CPU_BASE "/cpu0/cpufreq/energy_performance_preference");
    return sysfs_path_exists(path);
}

int cpu_epp_read(char *out, size_t out_sz)
{
    if (!cpu_epp_available())
        return -1;
    return sysfs_read_first_cpu_leaf("energy_performance_preference", out, out_sz);
}

int cpu_epp_apply(const char *epp)
{
    char cur[64];
    int skipped = 0;
    int ok;

    if (!epp || !cpu_epp_available())
        return -1;

    if (cpu_epp_read(cur, sizeof(cur)) == 0 && strcmp(cur, epp) == 0)
    {
        skipped = 1;
        ok = 1;
    }
    else
    {
        ok = (sysfs_write_all_cpu_leaf("energy_performance_preference", epp) == 0);
        PG_LOG_I("epp", "apply %s (%s)", epp, ok ? "ok" : "fail");
    }

    powergov_metrics_apply(POWERGOV_FEATURE_EPP, ok, skipped);
    if (!skipped && ok)
        (void)cpu_epp_verify(epp);
    return ok ? 0 : -1;
}

int cpu_epp_verify(const char *expected)
{
    char cur[64];
    int ok;

    if (!expected || cpu_epp_read(cur, sizeof(cur)) != 0)
        return -1;

    ok = (strcmp(cur, expected) == 0);
    if (!ok)
        PG_LOG_W("epp", "verify mismatch want=%s got=%s", expected, cur);
    powergov_metrics_verify(POWERGOV_FEATURE_EPP, ok);
    return ok ? 0 : -1;
}
