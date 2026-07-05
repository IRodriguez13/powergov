#include "info.h"
#include "../cpu/cpu_load.h"
#include "../cpu/governor.h"
#include "../cpu/epp.h"
#include "../cpu/turbo.h"
#include "../power/power_supply.h"
#include "../platform/platform_profile.h"
#include "../core/sysfs.h"
#include "../include/version.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <glob.h>

static void safe_copy(char *dst, size_t dst_sz, const char *src)
{
    if (!dst || dst_sz == 0)
        return;
    if (!src)
    {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_sz, "%s", src);
}

static int read_os_pretty(char *out, size_t out_sz)
{
    FILE *f;
    char line[256];

    f = fopen("/etc/os-release", "r");
    if (!f)
        return -1;

    while (fgets(line, sizeof(line), f))
    {
        if (strncmp(line, "PRETTY_NAME=", 12) == 0)
        {
            char *v = line + 12;
            v[strcspn(v, "\n")] = '\0';
            if (v[0] == '"')
            {
                v++;
                v[strcspn(v, "\"")] = '\0';
            }
            safe_copy(out, out_sz, v);
            fclose(f);
            return 0;
        }
    }

    fclose(f);
    return -1;
}

static int cpu_model(char *out, size_t out_sz)
{
    FILE *f;
    char line[256];

    f = fopen("/proc/cpuinfo", "r");
    if (!f)
        return -1;

    while (fgets(line, sizeof(line), f))
    {
        if (strncmp(line, "model name", 10) == 0)
        {
            char *c = strchr(line, ':');
            if (c)
            {
                c += 2;
                c[strcspn(c, "\n")] = '\0';
                safe_copy(out, out_sz, c);
                fclose(f);
                return 0;
            }
        }
    }

    fclose(f);
    return -1;
}

static int cpu_count(void)
{
    glob_t g;
    int n = 0;

    if (glob("/sys/devices/system/cpu/cpu[0-9]*", 0, NULL, &g) == 0)
    {
        n = (int)g.gl_pathc;
        globfree(&g);
    }
    return n;
}

void powergov_info_fill_status(const powergov_config_t *cfg,
                               powergov_reply_status_t *out)
{
    powergov_power_info_t power;
    char gov[64];
    char epp[64];
    int turbo;

    if (!out)
        return;

    memset(out, 0, sizeof(*out));
    out->daemon_running = 1;
    out->battery_pct = -1;
    out->turbo_on = -1;

    if (cfg)
    {
        out->user_mode = (int)cfg->user_mode;
        out->battery_safe_enabled = cfg->battery_safe_enabled;
        out->battery_threshold = cfg->battery_threshold;
        out->features_mask = powergov_features_to_mask(&cfg->features);
    }

    out->cpu_load = powergov_cpu_load_cached();

    if (powergov_power_supply_poll(&power) == 0 && power.present)
    {
        out->power_source = (int)power.source;
        out->battery_pct = power.capacity_pct;
        safe_copy(out->battery_status, sizeof(out->battery_status), power.name);
    }

    if (cpu_governor_read(gov, sizeof(gov)) == 0)
        safe_copy(out->governor, sizeof(out->governor), gov);

    if (cpu_epp_read(epp, sizeof(epp)) == 0)
        safe_copy(out->epp, sizeof(out->epp), epp);

    if (cpu_turbo_read(&turbo) == 0)
        out->turbo_on = turbo;
}

void powergov_info_fill_system(powergov_reply_system_t *out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));
    read_os_pretty(out->pretty_name, sizeof(out->pretty_name));

    {
        struct utsname u;
        if (uname(&u) == 0)
            safe_copy(out->kernel, sizeof(out->kernel), u.release);
    }

    snprintf(out->powergov_version, sizeof(out->powergov_version),
             "%s", POWERGOV_VERSION);

    /* Answering on the socket implies systemd started us. */
    out->systemd_active = 1;
    out->ppd_detected = platform_ppd_active();
}

void powergov_info_fill_cpu(powergov_reply_cpu_t *out)
{
    char gov[128];
    char path[256];

    if (!out)
        return;

    memset(out, 0, sizeof(*out));
    cpu_model(out->model, sizeof(out->model));
    out->cpu_count = cpu_count();

    if (sysfs_read_first_cpu_leaf("scaling_driver", out->scaling_driver,
                                  sizeof(out->scaling_driver)) != 0)
        out->scaling_driver[0] = '\0';

    if (cpu_governor_read(gov, sizeof(gov)) == 0)
        safe_copy(out->governor, sizeof(out->governor), gov);

    if (sysfs_read_first_cpu_leaf("scaling_available_governors", gov,
                                  sizeof(gov)) == 0)
        safe_copy(out->governors_avail, sizeof(out->governors_avail), gov);

    out->epp_available = cpu_epp_available();
    if (cpu_epp_read(out->epp, sizeof(out->epp)) != 0)
        out->epp[0] = '\0';

    {
        int turbo;
        if (cpu_turbo_read(&turbo) == 0)
            out->turbo_on = turbo;
        else
            out->turbo_on = -1;
    }

    snprintf(path, sizeof(path),
             POWERGOV_CPU_BASE "/cpu0/cpufreq/cpuinfo_max_freq");
    if (sysfs_read_file(path, out->freq_hw_max, sizeof(out->freq_hw_max)) != 0)
        out->freq_hw_max[0] = '\0';

    if (sysfs_read_first_cpu_leaf("scaling_max_freq", out->freq_scaling_max,
                                  sizeof(out->freq_scaling_max)) != 0)
        out->freq_scaling_max[0] = '\0';

    if (platform_profile_read(out->platform_profile,
                              sizeof(out->platform_profile)) != 0)
        out->platform_profile[0] = '\0';

    out->rapl_available = sysfs_path_exists(
        "/sys/class/powercap/intel-rapl:0/energy_uj");
}

static int feature_hw(int id)
{
    switch (id)
    {
    case POWERGOV_FEATURE_GOVERNOR:
        return sysfs_path_exists(POWERGOV_CPU_BASE "/cpu0/cpufreq/scaling_governor");
    case POWERGOV_FEATURE_EPP:
        return cpu_epp_available();
    case POWERGOV_FEATURE_FREQ_CAP:
        return sysfs_path_exists(POWERGOV_CPU_BASE "/cpu0/cpufreq/scaling_max_freq");
    case POWERGOV_FEATURE_TURBO:
        return cpu_turbo_available();
    case POWERGOV_FEATURE_PLATFORM:
        return platform_profile_available();
    case POWERGOV_FEATURE_RUNTIME_PM:
        return sysfs_path_exists("/sys/bus/pci/devices");
    default:
        return 0;
    }
}

void powergov_info_fill_compat(const powergov_config_t *cfg,
                               powergov_reply_compat_t *out)
{
    int mask;
    int supported = 0;
    int partial = 0;
    int i;
    char driver[64];

    if (!out)
        return;

    memset(out, 0, sizeof(*out));
    mask = cfg ? powergov_features_to_mask(&cfg->features) : 0;

    if (sysfs_read_first_cpu_leaf("scaling_driver", driver, sizeof(driver)) == 0)
        safe_copy(out->scaling_driver, sizeof(out->scaling_driver), driver);

    for (i = 0; i < POWERGOV_FEATURE_COUNT; i++)
    {
        int hw = feature_hw(i);
        int st = POWERGOV_COMPAT_UNSUPPORTED;
        const char *detail = "Not available on this hardware/kernel.";

        out->rows[i].hw_available = hw;
        out->rows[i].enabled = (mask >> i) & 1;
        safe_copy(out->rows[i].name, sizeof(out->rows[i].name),
                  powergov_feature_name((powergov_feature_id_t)i));

        if (i == POWERGOV_FEATURE_PLATFORM && platform_ppd_active())
        {
            st = POWERGOV_COMPAT_CONFLICT;
            detail = "power-profiles-daemon active; powergov skips platform_profile.";
        }
        else if (i == POWERGOV_FEATURE_FREQ_CAP && hw)
        {
            st = POWERGOV_COMPAT_PARTIAL;
            detail = "Available; driver may impose a frequency floor.";
            partial++;
        }
        else if (i == POWERGOV_FEATURE_RUNTIME_PM && hw)
        {
            st = POWERGOV_COMPAT_PARTIAL;
            detail = "PCI/USB; effect depends on each device.";
            partial++;
        }
        else if (hw)
        {
            st = POWERGOV_COMPAT_SUPPORTED;
            detail = "Sysfs present.";
            supported++;
        }

        out->rows[i].state = st;
        safe_copy(out->rows[i].detail, sizeof(out->rows[i].detail), detail);
    }

    out->adaptability_score = (supported * 100 + partial * 50) / POWERGOV_FEATURE_COUNT;
    snprintf(out->summary, sizeof(out->summary),
             "%d subsystems supported, %d partial, of %d total.",
             supported, partial, POWERGOV_FEATURE_COUNT);
}

void powergov_info_fill_metrics(powergov_reply_metrics_t *out)
{
    FILE *f;

    if (!out)
        return;

    memset(out, 0, sizeof(*out));
    f = fopen(POWERGOV_METRICS_PATH, "r");
    if (!f)
    {
        safe_copy(out->text, sizeof(out->text), "(no metrics)");
        return;
    }

    if (fread(out->text, 1, sizeof(out->text) - 1, f) == 0)
        out->text[0] = '\0';
    out->text[sizeof(out->text) - 1] = '\0';
    fclose(f);
}

void powergov_info_fill_log(int lines, powergov_reply_log_t *out)
{
    FILE *f;
    char **ring;
    int cap;
    int count;
    int i;
    char buf[640];
    size_t pos;

    if (!out)
        return;

    memset(out, 0, sizeof(*out));

    if (lines <= 0)
        lines = 80;

    f = fopen(POWERGOV_LOG_PATH, "r");
    if (!f)
    {
        out->ok = 0;
        snprintf(out->text, sizeof(out->text),
                 "Could not read %s", POWERGOV_LOG_PATH);
        return;
    }

    cap = lines;
    ring = calloc((size_t)cap, sizeof(char *));
    if (!ring)
    {
        fclose(f);
        out->ok = 0;
        return;
    }

    count = 0;
    while (fgets(buf, sizeof(buf), f))
    {
        size_t idx = (size_t)(count % cap);
        free(ring[idx]);
        ring[idx] = strdup(buf);
        count++;
    }
    fclose(f);

    out->ok = 1;
    pos = 0;
    i = (count > cap) ? (count - cap) : 0;
    for (; i < count; i++)
    {
        size_t idx = (size_t)(i % cap);
        if (!ring[idx])
            continue;
        strncat(out->text, ring[idx], sizeof(out->text) - pos - 1);
        pos = strlen(out->text);
    }

    for (i = 0; i < cap; i++)
        free(ring[i]);
    free(ring);
}

void powergov_info_fill_bundle(const powergov_config_t *cfg, unsigned int mask,
                               int log_lines, powergov_reply_bundle_t *out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));
    out->mask = mask;

    if (mask & POWERGOV_BUNDLE_STATUS)
        powergov_info_fill_status(cfg, &out->status);
    if (mask & POWERGOV_BUNDLE_SYSTEM)
        powergov_info_fill_system(&out->system);
    if (mask & POWERGOV_BUNDLE_CPU)
        powergov_info_fill_cpu(&out->cpu);
    if (mask & POWERGOV_BUNDLE_COMPAT)
        powergov_info_fill_compat(cfg, &out->compat);
    if (mask & POWERGOV_BUNDLE_METRICS)
        powergov_info_fill_metrics(&out->metrics);
    if (mask & POWERGOV_BUNDLE_LOG)
        powergov_info_fill_log(log_lines > 0 ? log_lines : 80, &out->log);
}
