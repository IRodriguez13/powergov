#include "governor.h"
#include "../core/sysfs.h"
#include "../log/log.h"
#include "../metrics/metrics.h"
#include <string.h>

int cpu_governor_read(char *out, size_t out_sz)
{
    return sysfs_read_first_cpu_leaf("scaling_governor", out, out_sz);
}

static int governor_in_list(const char *gov, const char *list)
{
    const char *p;
    size_t len;

    if (!gov || !list || !*gov)
        return 0;

    len = strlen(gov);
    for (p = list; *p;)
    {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;
        if (strncmp(p, gov, len) == 0 &&
            (p[len] == '\0' || p[len] == ' ' || p[len] == '\t'))
            return 1;
        while (*p && *p != ' ' && *p != '\t')
            p++;
    }
    return 0;
}

static int governor_available(const char *gov)
{
    char avail[256];

    if (!gov)
        return 0;

    if (sysfs_read_first_cpu_leaf("scaling_available_governors", avail,
                                   sizeof(avail)) != 0)
        return 1;

    return governor_in_list(gov, avail);
}

static void governor_resolve(const char *want, char *resolved, size_t resolved_sz)
{
    if (!want || !resolved || resolved_sz == 0)
        return;

    if (governor_available(want))
    {
        strncpy(resolved, want, resolved_sz - 1);
        resolved[resolved_sz - 1] = '\0';
        return;
    }

    /*
     * amd-pstate-epp (active mode) exposes only performance/powersave.
     * powersave there scales dynamically; EPP carries the balance intent.
     */
    if (strcmp(want, "schedutil") == 0 && governor_available("powersave"))
    {
        PG_LOG_I("governor", "schedutil unavailable; using powersave (EPP handles balance)");
        strncpy(resolved, "powersave", resolved_sz - 1);
        resolved[resolved_sz - 1] = '\0';
        return;
    }

    strncpy(resolved, want, resolved_sz - 1);
    resolved[resolved_sz - 1] = '\0';
}

int cpu_governor_apply(const char *gov)
{
    char resolved[64];
    char cur[64];
    int skipped = 0;
    int ok;

    if (!gov)
        return -1;

    governor_resolve(gov, resolved, sizeof(resolved));

    memset(cur, 0, sizeof(cur));
    (void)cpu_governor_read(cur, sizeof(cur));

    if (cur[0] && strcmp(cur, resolved) == 0)
    {
        skipped = 1;
        ok = 1;
    }
    else
    {
        ok = (sysfs_write_all_cpu_leaf("scaling_governor", resolved) == 0);
        PG_LOG_I("governor", "apply %s -> %s (%s)",
                 cur[0] ? cur : "?", resolved, ok ? "ok" : "fail");
    }

    powergov_metrics_apply(POWERGOV_FEATURE_GOVERNOR, ok, skipped);
    if (!skipped && ok)
        (void)cpu_governor_verify(resolved);
    return ok ? 0 : -1;
}

int cpu_governor_verify(const char *expected)
{
    char resolved[64];
    char cur[64];
    int ok;

    if (!expected)
        return -1;

    governor_resolve(expected, resolved, sizeof(resolved));

    if (cpu_governor_read(cur, sizeof(cur)) != 0)
        return -1;

    ok = (strcmp(cur, resolved) == 0);
    if (!ok)
        PG_LOG_W("governor", "verify mismatch want=%s got=%s", resolved, cur);
    powergov_metrics_verify(POWERGOV_FEATURE_GOVERNOR, ok);
    return ok ? 0 : -1;
}
