#include "governor.h"
#include "../core/sysfs.h"
#include "../log/log.h"
#include "../metrics/metrics.h"
#include <string.h>

int cpu_governor_read(char *out, size_t out_sz)
{
    return sysfs_read_first_cpu_leaf("scaling_governor", out, out_sz);
}

int cpu_governor_apply(const char *gov)
{
    char cur[64];
    int skipped = 0;
    int ok;

    if (!gov)
        return -1;

    if (cpu_governor_read(cur, sizeof(cur)) == 0 && strcmp(cur, gov) == 0)
    {
        skipped = 1;
        ok = 1;
    }
    else
    {
        ok = (sysfs_write_all_cpu_leaf("scaling_governor", gov) == 0);
        PG_LOG_I("governor", "apply %s -> %s (%s)",
                 skipped ? cur : "?", gov, ok ? "ok" : "fail");
    }

    powergov_metrics_apply(POWERGOV_FEATURE_GOVERNOR, ok, skipped);
    if (!skipped && ok)
        (void)cpu_governor_verify(gov);
    return ok ? 0 : -1;
}

int cpu_governor_verify(const char *expected)
{
    char cur[64];
    int ok;

    if (!expected || cpu_governor_read(cur, sizeof(cur)) != 0)
        return -1;

    ok = (strcmp(cur, expected) == 0);
    if (!ok)
        PG_LOG_W("governor", "verify mismatch want=%s got=%s", expected, cur);
    powergov_metrics_verify(POWERGOV_FEATURE_GOVERNOR, ok);
    return ok ? 0 : -1;
}
