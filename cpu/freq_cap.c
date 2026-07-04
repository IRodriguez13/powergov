#include "freq_cap.h"
#include "../core/sysfs.h"
#include "../log/log.h"
#include "../metrics/metrics.h"
#include <stdio.h>
#include <string.h>

static long g_saved_max_freq;
static int g_saved_valid;

int cpu_freq_cap_available(void)
{
    char path[256];
    snprintf(path, sizeof(path),
             POWERGOV_CPU_BASE "/cpu0/cpufreq/scaling_max_freq");
    return sysfs_path_exists(path);
}

static int read_hw_max(long *hw_max)
{
    char path[256];
    snprintf(path, sizeof(path),
             POWERGOV_CPU_BASE "/cpu0/cpufreq/cpuinfo_max_freq");
    return sysfs_read_int(path, hw_max);
}

int cpu_freq_cap_apply_pct(int pct)
{
    long hw_max;
    long target;
    char buf[32];
    char cur[32];
    int skipped = 0;
    int ok;

    if (pct <= 0 || pct > 100)
        return cpu_freq_cap_restore();

    if (!cpu_freq_cap_available() || read_hw_max(&hw_max) != 0)
        return -1;

    if (!g_saved_valid)
    {
        char path[256];
        snprintf(path, sizeof(path),
                 POWERGOV_CPU_BASE "/cpu0/cpufreq/scaling_max_freq");
        if (sysfs_read_int(path, &g_saved_max_freq) == 0)
            g_saved_valid = 1;
    }

    target = (hw_max * pct) / 100;
    snprintf(buf, sizeof(buf), "%ld", target);

    if (sysfs_read_first_cpu_leaf("scaling_max_freq", cur, sizeof(cur)) == 0 &&
        strcmp(cur, buf) == 0)
    {
        skipped = 1;
        ok = 1;
    }
    else
    {
        ok = (sysfs_write_all_cpu_leaf("scaling_max_freq", buf) == 0);
        PG_LOG_I("freq_cap", "apply %d%% -> %ld kHz (%s)", pct, target,
                 ok ? "ok" : "fail");
    }

    powergov_metrics_apply(POWERGOV_FEATURE_FREQ_CAP, ok, skipped);
    if (!skipped && ok)
        (void)cpu_freq_cap_verify_pct(pct);
    return ok ? 0 : -1;
}

int cpu_freq_cap_restore(void)
{
    char buf[32];
    int ok;

    if (!g_saved_valid)
    {
        powergov_metrics_apply(POWERGOV_FEATURE_FREQ_CAP, 1, 1);
        return 0;
    }

    snprintf(buf, sizeof(buf), "%ld", g_saved_max_freq);
    ok = (sysfs_write_all_cpu_leaf("scaling_max_freq", buf) == 0);
    PG_LOG_I("freq_cap", "restore %ld kHz (%s)", g_saved_max_freq, ok ? "ok" : "fail");
    powergov_metrics_apply(POWERGOV_FEATURE_FREQ_CAP, ok, 0);
    g_saved_valid = 0;
    return ok ? 0 : -1;
}

int cpu_freq_cap_verify_pct(int pct)
{
    long hw_max;
    long target;
    long cur;
    char path[256];
    int ok;

    if (pct <= 0)
        return 0;

    if (read_hw_max(&hw_max) != 0)
        return -1;

    target = (hw_max * pct) / 100;
    snprintf(path, sizeof(path),
             POWERGOV_CPU_BASE "/cpu0/cpufreq/scaling_max_freq");
    if (sysfs_read_int(path, &cur) != 0)
        return -1;

    /* Driver may enforce a minimum above target (e.g. intel_pstate floor). */
    ok = (cur <= target + (hw_max / 50)) ||
         (cur < hw_max && cur <= target + (hw_max / 10));
    if (!ok)
        PG_LOG_W("freq_cap", "verify mismatch want~=%ld got=%ld", target, cur);
    powergov_metrics_verify(POWERGOV_FEATURE_FREQ_CAP, ok);
    return ok ? 0 : -1;
}
