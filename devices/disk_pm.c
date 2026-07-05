#include "disk_pm.h"
#include "../core/sysfs.h"
#include "../log/log.h"
#include "../metrics/metrics.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_SATA   16
#define MAX_DISK   8
#define MAX_NVME   4

typedef struct
{
    char path[512];
    char saved[32];
} sata_entry_t;

typedef struct
{
    char dev[32];
    int saved_apm;
    int have_apm;
} disk_apm_t;

typedef struct
{
    char path[512];
    char saved[32];
} nvme_entry_t;

static int g_aggression;
static sata_entry_t g_sata[MAX_SATA];
static int g_sata_count;
static disk_apm_t g_apm[MAX_DISK];
static int g_apm_count;
static nvme_entry_t g_nvme[MAX_NVME];
static int g_nvme_count;

static int block_is_usb(const char *block_name)
{
    char path[256];
    char link[512];
    ssize_t n;

    snprintf(path, sizeof(path), "/sys/block/%s/device", block_name);
    n = readlink(path, link, sizeof(link) - 1);
    if (n <= 0)
        return 0;
    link[n] = '\0';
    return strstr(link, "usb") != NULL;
}

static const char *hdparm_bin(void)
{
    if (access("/usr/sbin/hdparm", X_OK) == 0)
        return "/usr/sbin/hdparm";
    if (access("/sbin/hdparm", X_OK) == 0)
        return "/sbin/hdparm";
    if (access("/usr/bin/hdparm", X_OK) == 0)
        return "/usr/bin/hdparm";
    return NULL;
}

static int run_hdparm_apm(const char *dev, int level)
{
    const char *hp = hdparm_bin();
    char cmd[128];
    int rc;

    if (!hp || !dev || level < 1 || level > 254)
        return -1;

    snprintf(cmd, sizeof(cmd), "%s -B %d %s >/dev/null 2>&1", hp, level, dev);
    rc = system(cmd);
    return (rc == 0) ? 0 : -1;
}

static int read_hdparm_apm(const char *dev, int *out)
{
    const char *hp = hdparm_bin();
    char cmd[128];
    FILE *f;
    char line[128];

    if (!hp || !dev || !out)
        return -1;

    snprintf(cmd, sizeof(cmd), "%s -B %s 2>/dev/null", hp, dev);
    f = popen(cmd, "r");
    if (!f)
        return -1;

    while (fgets(line, sizeof(line), f))
    {
        if (sscanf(line, " APM_level = %d", out) == 1 ||
            sscanf(line, " APM level = %d", out) == 1)
        {
            pclose(f);
            return 0;
        }
    }

    pclose(f);
    return -1;
}

static void apm_apply(int level)
{
    DIR *d;
    struct dirent *ent;

    if (level <= 0)
        return;

    d = opendir("/sys/block");
    if (!d)
        return;

    while ((ent = readdir(d)) != NULL)
    {
        char dev[64];
        int saved;
        int idx;

        if (ent->d_name[0] == '.')
            continue;
        if (strncmp(ent->d_name, "sd", 2) != 0)
            continue;
        if (block_is_usb(ent->d_name))
            continue;
        if (g_apm_count >= MAX_DISK)
            break;

        snprintf(dev, sizeof(dev), "/dev/%s", ent->d_name);
        idx = g_apm_count++;
        snprintf(g_apm[idx].dev, sizeof(g_apm[idx].dev), "%s", dev);
        if (read_hdparm_apm(dev, &saved) == 0)
        {
            g_apm[idx].saved_apm = saved;
            g_apm[idx].have_apm = 1;
        }
        run_hdparm_apm(dev, level);
    }

    closedir(d);

    d = opendir("/sys/block");
    if (!d)
        return;

    while ((ent = readdir(d)) != NULL)
    {
        char path[512];
        char val[32];

        if (ent->d_name[0] == '.')
            continue;
        if (strncmp(ent->d_name, "nvme", 4) != 0)
            continue;
        if (strchr(ent->d_name + 4, 'p') != NULL)
            continue;
        if (g_nvme_count >= MAX_NVME)
            break;

        snprintf(path, sizeof(path),
                 "/sys/block/%s/device/power/control", ent->d_name);
        if (!sysfs_path_exists(path))
            continue;
        if (sysfs_read_file(path, val, sizeof(val)) != 0)
            continue;

        snprintf(g_nvme[g_nvme_count].path,
                 sizeof(g_nvme[g_nvme_count].path), "%s", path);
        strncpy(g_nvme[g_nvme_count].saved, val,
                sizeof(g_nvme[g_nvme_count].saved) - 1);
        g_nvme_count++;

        if (level >= 2)
            sysfs_write_file(path, "auto");
    }

    closedir(d);
}

static void apm_restore(void)
{
    int i;

    for (i = 0; i < g_apm_count; i++)
    {
        if (g_apm[i].have_apm)
            run_hdparm_apm(g_apm[i].dev, g_apm[i].saved_apm);
    }
    g_apm_count = 0;

    for (i = 0; i < g_nvme_count; i++)
        sysfs_write_file(g_nvme[i].path, g_nvme[i].saved);
    g_nvme_count = 0;
}

static void sata_apply(const char *policy)
{
    DIR *d;
    struct dirent *ent;
    char path[512];

    d = opendir("/sys/class/scsi_host");
    if (!d || !policy)
        return;

    while ((ent = readdir(d)) != NULL)
    {
        if (ent->d_name[0] == '.')
            continue;
        if (g_sata_count >= MAX_SATA)
            break;

        snprintf(path, sizeof(path),
                 "/sys/class/scsi_host/%s/link_power_management_policy",
                 ent->d_name);
        if (!sysfs_path_exists(path))
            continue;

        if (sysfs_read_file(path, g_sata[g_sata_count].saved,
                            sizeof(g_sata[g_sata_count].saved)) != 0)
            continue;

        snprintf(g_sata[g_sata_count].path,
                 sizeof(g_sata[g_sata_count].path), "%s", path);
        if (sysfs_write_file(path, policy) == 0)
            g_sata_count++;
    }

    closedir(d);
}

static void sata_restore(void)
{
    int i;

    for (i = 0; i < g_sata_count; i++)
        sysfs_write_file(g_sata[i].path, g_sata[i].saved);
    g_sata_count = 0;
}

int disk_pm_available(void)
{
    DIR *d;

    if (hdparm_bin())
        return 1;
    d = opendir("/sys/class/scsi_host");
    if (d)
    {
        closedir(d);
        return 1;
    }
    return sysfs_path_exists("/sys/block/nvme0n1");
}

int disk_pm_apply(int aggression, const powergov_peripheral_opts_t *opts)
{
    int apm_level = 0;
    const char *sata_policy = NULL;
    int sata_on = 1;
    int changed;

    if (aggression <= 0)
        return disk_pm_restore();

    if (opts)
        sata_on = opts->sata;

    switch (aggression)
    {
    case 1:
        apm_level = 192;
        sata_policy = sata_on ? "med_power_withapm" : NULL;
        break;
    case 2:
        apm_level = 128;
        sata_policy = sata_on ? "med_power_withapm" : NULL;
        break;
    default:
        apm_level = 127;
        sata_policy = sata_on ? "min_power" : NULL;
        break;
    }

    if (g_aggression == aggression && g_sata_count + g_apm_count + g_nvme_count > 0)
    {
        powergov_metrics_apply(POWERGOV_FEATURE_DISK_PM, 1, 1);
        return 0;
    }

    disk_pm_restore();

    if (apm_level > 0)
        apm_apply(apm_level);
    if (sata_policy)
        sata_apply(sata_policy);

    g_aggression = aggression;
    changed = g_sata_count + g_apm_count + g_nvme_count;

    PG_LOG_I("disk_pm", "aggression=%d apm=%d sata=%d nvme=%d",
             aggression, g_apm_count, g_sata_count, g_nvme_count);
    powergov_metrics_apply(POWERGOV_FEATURE_DISK_PM, changed > 0, changed == 0);
    return 0;
}

int disk_pm_restore(void)
{
    sata_restore();
    apm_restore();
    g_aggression = 0;
    powergov_metrics_apply(POWERGOV_FEATURE_DISK_PM, 1, 0);
    return 0;
}
