#include "runtime_pm.h"
#include "../core/sysfs.h"
#include "../log/log.h"
#include "../metrics/metrics.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>

#define PCI_POWER "/sys/bus/pci/devices"
#define USB_POWER "/sys/bus/usb/devices"
#define MAX_SAVED 128

typedef struct
{
    char path[512];
    char value[16];
} pm_entry_t;

static pm_entry_t g_saved[MAX_SAVED];
static int g_saved_count;
static int g_aggressive;

static void scan_bus(const char *base, const char *target)
{
    DIR *d;
    struct dirent *ent;
    char path[512];

    d = opendir(base);
    if (!d)
        return;

    while ((ent = readdir(d)) != NULL)
    {
        if (ent->d_name[0] == '.')
            continue;
        if (g_saved_count >= MAX_SAVED)
            break;

        snprintf(path, sizeof(path), "%s/%s/power/control", base, ent->d_name);
        if (!sysfs_path_exists(path))
            continue;

        if (sysfs_read_file(path, g_saved[g_saved_count].value,
                            sizeof(g_saved[0].value)) != 0)
            continue;

        if (strcmp(g_saved[g_saved_count].value, target) == 0)
            continue;

        if (sysfs_write_file(path, target) != 0)
            continue;

        snprintf(g_saved[g_saved_count].path, sizeof(g_saved[0].path), "%s", path);
        g_saved_count++;
    }

    closedir(d);
}

int runtime_pm_apply_aggressive(int aggressive)
{
    int ok = 1;

    if (!aggressive)
        return runtime_pm_restore();

    if (g_aggressive)
    {
        powergov_metrics_apply(POWERGOV_FEATURE_RUNTIME_PM, 1, 1);
        return 0;
    }

    g_saved_count = 0;
    scan_bus(PCI_POWER, "auto");
    scan_bus(USB_POWER, "auto");
    g_aggressive = 1;

    PG_LOG_I("runtime_pm", "aggressive ON changed=%d", g_saved_count);
    powergov_metrics_apply(POWERGOV_FEATURE_RUNTIME_PM, ok, g_saved_count == 0);
    powergov_metrics_verify(POWERGOV_FEATURE_RUNTIME_PM, ok);
    return 0;
}

int runtime_pm_restore(void)
{
    int i;
    int ok = 1;

    if (!g_aggressive && g_saved_count == 0)
    {
        powergov_metrics_apply(POWERGOV_FEATURE_RUNTIME_PM, 1, 1);
        return 0;
    }

    for (i = 0; i < g_saved_count; i++)
    {
        if (sysfs_write_file(g_saved[i].path, g_saved[i].value) != 0)
            ok = 0;
    }

    PG_LOG_I("runtime_pm", "restore count=%d (%s)", g_saved_count, ok ? "ok" : "fail");
    g_aggressive = 0;
    g_saved_count = 0;
    powergov_metrics_apply(POWERGOV_FEATURE_RUNTIME_PM, ok, 0);
    return ok ? 0 : -1;
}
