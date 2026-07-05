#include "bluetooth_pm.h"
#include "../core/sysfs.h"
#include "../log/log.h"
#include "../metrics/metrics.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAX_HCI 4

typedef struct
{
    char path[512];
    char saved[16];
} hci_entry_t;

static int g_aggression;
static hci_entry_t g_hci[MAX_HCI];
static int g_hci_count;

static void hci_apply_power_save(void)
{
    DIR *d;
    struct dirent *ent;

    d = opendir("/sys/class/bluetooth");
    if (!d)
        return;

    while ((ent = readdir(d)) != NULL)
    {
        char path[512];

        if (ent->d_name[0] == '.')
            continue;
        if (g_hci_count >= MAX_HCI)
            break;

        snprintf(path, sizeof(path),
                 "/sys/class/bluetooth/%s/device/power/control", ent->d_name);
        if (!sysfs_path_exists(path))
            continue;

        if (sysfs_read_file(path, g_hci[g_hci_count].saved,
                            sizeof(g_hci[g_hci_count].saved)) != 0)
            continue;

        snprintf(g_hci[g_hci_count].path,
                 sizeof(g_hci[g_hci_count].path), "%s", path);
        g_hci_count++;

        sysfs_write_file(path, "auto");
    }

    closedir(d);
}

static void hci_restore(void)
{
    int i;

    for (i = 0; i < g_hci_count; i++)
        sysfs_write_file(g_hci[i].path, g_hci[i].saved);
    g_hci_count = 0;
}

int bluetooth_pm_available(void)
{
    return sysfs_path_exists("/sys/class/bluetooth");
}

int bluetooth_pm_apply(int aggression)
{
    if (aggression < 2)
        return bluetooth_pm_restore();

    if (g_aggression == aggression && g_hci_count > 0)
    {
        powergov_metrics_apply(POWERGOV_FEATURE_BLUETOOTH_PM, 1, 1);
        return 0;
    }

    bluetooth_pm_restore();
    hci_apply_power_save();

    g_aggression = aggression;
    PG_LOG_I("bluetooth_pm", "aggression=%d hci=%d", aggression, g_hci_count);
    powergov_metrics_apply(POWERGOV_FEATURE_BLUETOOTH_PM, g_hci_count > 0, 0);
    return 0;
}

int bluetooth_pm_restore(void)
{
    hci_restore();
    g_aggression = 0;
    powergov_metrics_apply(POWERGOV_FEATURE_BLUETOOTH_PM, 1, 0);
    return 0;
}
