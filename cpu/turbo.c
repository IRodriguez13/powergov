#include "turbo.h"
#include "../core/sysfs.h"
#include "../log/log.h"
#include "../metrics/metrics.h"
#include <stdio.h>
#include <string.h>

static const char *TURBO_INTEL =
    "/sys/devices/system/cpu/intel_pstate/no_turbo";
static const char *TURBO_GENERIC =
    "/sys/devices/system/cpu/cpufreq/boost";

static const char *turbo_path(void)
{
    if (sysfs_path_exists(TURBO_GENERIC))
        return TURBO_GENERIC;
    if (sysfs_path_exists(TURBO_INTEL))
        return TURBO_INTEL;
    return NULL;
}

static int turbo_is_intel_path(const char *path)
{
    return path && strstr(path, "no_turbo") != NULL;
}

int cpu_turbo_available(void)
{
    return turbo_path() != NULL;
}

int cpu_turbo_read(int *enabled)
{
    const char *path = turbo_path();
    long val;

    if (!enabled || !path)
        return -1;

    if (sysfs_read_int(path, &val) != 0)
        return -1;

    if (turbo_is_intel_path(path))
        *enabled = (val == 0);
    else
        *enabled = (val != 0);

    return 0;
}

int cpu_turbo_apply(int enabled)
{
    const char *path = turbo_path();
    char want_buf[8];
    long cur_val;
    long want_val;
    int skipped = 0;
    int ok;

    if (!path)
        return -1;

    if (turbo_is_intel_path(path))
        want_val = enabled ? 0 : 1;
    else
        want_val = enabled ? 1 : 0;

    snprintf(want_buf, sizeof(want_buf), "%ld", want_val);

    if (sysfs_read_int(path, &cur_val) == 0 && cur_val == want_val)
    {
        skipped = 1;
        ok = 1;
    }
    else
    {
        ok = (sysfs_write_file(path, want_buf) == 0);
        PG_LOG_I("turbo", "apply %s (%s)", enabled ? "on" : "off",
                 ok ? "ok" : "fail");
    }

    powergov_metrics_apply(POWERGOV_FEATURE_TURBO, ok, skipped);
    if (!skipped && ok)
        (void)cpu_turbo_verify(enabled);
    return ok ? 0 : -1;
}

int cpu_turbo_verify(int enabled)
{
    int cur;
    int ok;

    if (cpu_turbo_read(&cur) != 0)
        return -1;

    ok = (cur == (enabled ? 1 : 0));
    if (!ok)
        PG_LOG_W("turbo", "verify mismatch want=%d got=%d", enabled, cur);
    powergov_metrics_verify(POWERGOV_FEATURE_TURBO, ok);
    return ok ? 0 : -1;
}
