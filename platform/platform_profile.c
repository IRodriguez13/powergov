#include "platform_profile.h"
#include "../core/sysfs.h"
#include "../log/log.h"
#include "../metrics/metrics.h"
#include <string.h>
#include <unistd.h>

#define PLATFORM_PROFILE_PATH "/sys/firmware/acpi/platform_profile"

int platform_ppd_active(void)
{
    return access("/run/power-profiles-daemon", F_OK) == 0 ||
           access("/var/run/power-profiles-daemon", F_OK) == 0;
}

int platform_profile_available(void)
{
    return sysfs_path_exists(PLATFORM_PROFILE_PATH);
}

int platform_profile_read(char *out, size_t out_sz)
{
    return sysfs_read_file(PLATFORM_PROFILE_PATH, out, out_sz);
}

int platform_profile_apply(const char *profile)
{
    char cur[64];
    int skipped = 0;
    int ok;

    if (!profile || !platform_profile_available())
        return -1;

    if (platform_ppd_active())
    {
        PG_LOG_D("platform", "skip: power-profiles-daemon active");
        powergov_metrics_apply(POWERGOV_FEATURE_PLATFORM, 1, 1);
        return 0;
    }

    if (platform_profile_read(cur, sizeof(cur)) == 0 && strcmp(cur, profile) == 0)
    {
        skipped = 1;
        ok = 1;
    }
    else
    {
        ok = (sysfs_write_file(PLATFORM_PROFILE_PATH, profile) == 0);
        PG_LOG_I("platform", "apply %s (%s)", profile, ok ? "ok" : "fail");
    }

    powergov_metrics_apply(POWERGOV_FEATURE_PLATFORM, ok, skipped);
    if (!skipped && ok)
        (void)platform_profile_verify(profile);
    return ok ? 0 : -1;
}

int platform_profile_verify(const char *expected)
{
    char cur[64];
    int ok;

    if (!expected || platform_profile_read(cur, sizeof(cur)) != 0)
        return -1;

    ok = (strcmp(cur, expected) == 0);
    if (!ok)
        PG_LOG_W("platform", "verify mismatch want=%s got=%s", expected, cur);
    powergov_metrics_verify(POWERGOV_FEATURE_PLATFORM, ok);
    return ok ? 0 : -1;
}
