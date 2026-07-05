#include "peripheral_pm.h"
#include "../core/sysfs.h"
#include "../log/log.h"
#include "../metrics/metrics.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_WIFI   8
#define MAX_AUDIO  8

typedef struct
{
    char iface[32];
    char saved[16];
    int have_saved;
} wifi_entry_t;

typedef struct
{
    char path[512];
    char saved[16];
} audio_entry_t;

static int g_level;
static unsigned g_last_wifi = 1;
static unsigned g_last_audio = 1;
static wifi_entry_t g_wifi[MAX_WIFI];
static int g_wifi_count;
static audio_entry_t g_audio[MAX_AUDIO];
static int g_audio_count;

static const char *iw_bin(void)
{
    if (access("/usr/sbin/iw", X_OK) == 0)
        return "/usr/sbin/iw";
    if (access("/usr/bin/iw", X_OK) == 0)
        return "/usr/bin/iw";
    return NULL;
}

static int is_wireless_iface(const char *name)
{
    char path[256];

    snprintf(path, sizeof(path), "/sys/class/net/%s/wireless", name);
    return sysfs_path_exists(path);
}

static int run_iw_power_save(const char *iface, int on)
{
    const char *iw = iw_bin();
    char cmd[160];
    int rc;

    if (!iw || !iface)
        return -1;

    snprintf(cmd, sizeof(cmd), "%s dev %s set power_save %s",
             iw, iface, on ? "on" : "off");
    rc = system(cmd);
    return (rc == 0) ? 0 : -1;
}

static int read_iw_power_save(const char *iface, char *out, size_t outsz)
{
    const char *iw = iw_bin();
    FILE *f;
    char cmd[160];
    char line[64];

    if (!iw || !iface || !out || outsz == 0)
        return -1;

    snprintf(cmd, sizeof(cmd), "%s dev %s get power_save 2>/dev/null", iw, iface);
    f = popen(cmd, "r");
    if (!f)
        return -1;

    if (!fgets(line, sizeof(line), f))
    {
        pclose(f);
        return -1;
    }
    pclose(f);

    if (strstr(line, "on"))
        strncpy(out, "on", outsz - 1);
    else if (strstr(line, "off"))
        strncpy(out, "off", outsz - 1);
    else
        return -1;

    out[outsz - 1] = '\0';
    return 0;
}

static void wifi_apply(int on)
{
    DIR *d;
    struct dirent *ent;

    d = opendir("/sys/class/net");
    if (!d)
        return;

    while ((ent = readdir(d)) != NULL)
    {
        int idx;
        char saved[16];

        if (ent->d_name[0] == '.')
            continue;
        if (!is_wireless_iface(ent->d_name))
            continue;
        if (g_wifi_count >= MAX_WIFI)
            break;

        idx = g_wifi_count++;
        strncpy(g_wifi[idx].iface, ent->d_name, sizeof(g_wifi[idx].iface) - 1);
        g_wifi[idx].iface[sizeof(g_wifi[idx].iface) - 1] = '\0';

        if (read_iw_power_save(g_wifi[idx].iface, saved, sizeof(saved)) == 0)
        {
            strncpy(g_wifi[idx].saved, saved, sizeof(g_wifi[idx].saved) - 1);
            g_wifi[idx].have_saved = 1;
        }

        if (on)
            run_iw_power_save(g_wifi[idx].iface, 1);
    }

    closedir(d);
}

static void wifi_restore(void)
{
    int i;

    for (i = 0; i < g_wifi_count; i++)
    {
        if (g_wifi[i].have_saved)
        {
            run_iw_power_save(g_wifi[i].iface,
                              strcmp(g_wifi[i].saved, "on") == 0);
        }
    }
    g_wifi_count = 0;
}

static void audio_apply(int on)
{
    static const char *const paths[] = {
        "/sys/module/snd_hda_intel/parameters/power_save",
        "/sys/module/snd_hda_codec_generic/parameters/power_save",
        "/sys/module/snd_hda_codec_hdmi/parameters/power_save",
        NULL
    };
    int i;

    for (i = 0; paths[i]; i++)
    {
        char val[16];

        if (g_audio_count >= MAX_AUDIO)
            break;
        if (!sysfs_path_exists(paths[i]))
            continue;
        if (sysfs_read_file(paths[i], val, sizeof(val)) != 0)
            continue;

        snprintf(g_audio[g_audio_count].path,
                 sizeof(g_audio[g_audio_count].path), "%s", paths[i]);
        strncpy(g_audio[g_audio_count].saved, val,
                sizeof(g_audio[g_audio_count].saved) - 1);
        g_audio_count++;

        snprintf(val, sizeof(val), "%d\n", on ? 1 : 0);
        sysfs_write_file(paths[i], val);
    }
}

static void audio_restore(void)
{
    int i;
    char val[16];

    for (i = 0; i < g_audio_count; i++)
    {
        snprintf(val, sizeof(val), "%s", g_audio[i].saved);
        sysfs_write_file(g_audio[i].path, val);
    }
    g_audio_count = 0;
}

int peripheral_pm_available(void)
{
    if (iw_bin())
        return 1;
    return sysfs_path_exists(
        "/sys/module/snd_hda_intel/parameters/power_save");
}

int peripheral_pm_apply(int level)
{
    powergov_peripheral_opts_t opts = {1, 1, 1};

    return peripheral_pm_apply_cfg(level, &opts);
}

int peripheral_pm_apply_cfg(int level, const powergov_peripheral_opts_t *opts)
{
    int wifi = 1;
    int audio = 1;

    if (level <= 0)
    {
        g_last_wifi = 1;
        g_last_audio = 1;
        return peripheral_pm_restore();
    }

    if (opts)
    {
        wifi = opts->wifi;
        audio = opts->audio;
    }

    if (!wifi && !audio)
    {
        g_last_wifi = 1;
        g_last_audio = 1;
        return peripheral_pm_restore();
    }

    if (g_level == level && g_wifi_count + g_audio_count > 0 &&
        wifi == (int)g_last_wifi && audio == (int)g_last_audio)
    {
        powergov_metrics_apply(POWERGOV_FEATURE_PERIPHERAL_PM, 1, 1);
        return 0;
    }

    peripheral_pm_restore();

    if (wifi)
        wifi_apply(1);
    if (audio)
        audio_apply(1);
    g_level = level;
    g_last_wifi = (unsigned)wifi;
    g_last_audio = (unsigned)audio;

    PG_LOG_I("peripheral_pm", "level=%d wifi=%d audio=%d",
             level, g_wifi_count, g_audio_count);
    powergov_metrics_apply(POWERGOV_FEATURE_PERIPHERAL_PM,
                           g_wifi_count + g_audio_count > 0,
                           0);
    return 0;
}

int peripheral_pm_restore(void)
{
    wifi_restore();
    audio_restore();
    g_level = 0;
    g_last_wifi = 1;
    g_last_audio = 1;
    powergov_metrics_apply(POWERGOV_FEATURE_PERIPHERAL_PM, 1, 0);
    return 0;
}
