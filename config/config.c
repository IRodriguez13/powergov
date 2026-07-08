#include "config.h"
#include "../include/powergov/types.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define POWERGOV_CONF_TMP "/etc/powergov.conf.tmp"

static void config_normalize_thresholds(powergov_config_t *config)
{
    if (!config)
        return;

    if (config->threshold_low < 0.0)
        config->threshold_low = POWERGOV_THRESHOLD_LOW_DEF;
    if (config->threshold_mid <= config->threshold_low)
        config->threshold_mid = config->threshold_low + 0.10;
    if (config->threshold_high <= config->threshold_mid)
        config->threshold_high = config->threshold_mid + 0.10;
    if (config->threshold_high > 1.0)
        config->threshold_high = 1.0;
}

int powergov_feature_parse_name(const char *name, powergov_feature_id_t *out)
{
    if (!name || !out)
        return -1;

    if (strcmp(name, "governor") == 0 || strcmp(name, "cpu_governor") == 0)
        *out = POWERGOV_FEATURE_GOVERNOR;
    else if (strcmp(name, "epp") == 0)
        *out = POWERGOV_FEATURE_EPP;
    else if (strcmp(name, "freq-cap") == 0 || strcmp(name, "freq_cap") == 0)
        *out = POWERGOV_FEATURE_FREQ_CAP;
    else if (strcmp(name, "turbo") == 0)
        *out = POWERGOV_FEATURE_TURBO;
    else if (strcmp(name, "platform") == 0 || strcmp(name, "platform_profile") == 0)
        *out = POWERGOV_FEATURE_PLATFORM;
    else if (strcmp(name, "runtime-pm") == 0 || strcmp(name, "runtime_pm") == 0)
        *out = POWERGOV_FEATURE_RUNTIME_PM;
    else if (strcmp(name, "peripheral-pm") == 0 ||
             strcmp(name, "peripheral_pm") == 0)
        *out = POWERGOV_FEATURE_PERIPHERAL_PM;
    else if (strcmp(name, "disk-pm") == 0 || strcmp(name, "disk_pm") == 0)
        *out = POWERGOV_FEATURE_DISK_PM;
    else if (strcmp(name, "pcie-aspm") == 0 || strcmp(name, "pcie_aspm") == 0)
        *out = POWERGOV_FEATURE_PCIE_ASPM;
    else if (strcmp(name, "bluetooth-pm") == 0 ||
             strcmp(name, "bluetooth_pm") == 0)
        *out = POWERGOV_FEATURE_BLUETOOTH_PM;
    else
        return -1;

    return 0;
}

static void parse_features_string(powergov_features_t *f, const char *s)
{
    char buf[128];
    char *tok;
    char *save;

    if (!f || !s)
        return;

    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    f->cpu_governor = 0;
    f->cpu_epp = 0;
    f->cpu_freq_cap = 0;
    f->cpu_turbo = 0;
    f->platform_profile = 0;
    f->runtime_pm = 0;
    f->peripheral_pm = 0;
    f->disk_pm = 0;
    f->pcie_aspm = 0;
    f->bluetooth_pm = 0;

    tok = strtok_r(buf, ",", &save);
    while (tok)
    {
        powergov_feature_id_t id;
        if (powergov_feature_parse_name(tok, &id) == 0)
        {
            int mask = powergov_features_to_mask(f);
            mask |= (1 << id);
            powergov_features_from_mask(f, mask);
        }
        tok = strtok_r(NULL, ",", &save);
    }
}

int powergov_config_load(powergov_config_t *config)
{
    FILE *f;
    char line[256];
    char key[64];
    char val[128];
    int ival;

    if (!config)
        return -1;

    powergov_config_set_defaults(config);

    f = fopen(POWERGOV_CONF_PATH, "r");
    if (!f)
        return 0;

    while (fgets(line, sizeof(line), f))
    {
        if (line[0] == '#' || line[0] == '\n')
            continue;

        if (sscanf(line, "BATTERY_SAFE_THRESHOLD=%d", &ival) == 1)
        {
            if (ival <= 0)
            {
                config->battery_safe_enabled = 0;
                config->battery_threshold = 0;
            }
            else if (ival <= 100)
            {
                config->battery_safe_enabled = 1;
                config->battery_threshold = ival;
            }
        }
        else if (sscanf(line, "USER_MODE=%63s", val) == 1)
        {
            config->user_mode = powergov_user_mode_parse(val);
        }
        else if (sscanf(line, "FEATURES=%127s", val) == 1)
        {
            parse_features_string(&config->features, val);
        }
        else if (sscanf(line, "LOG_LEVEL=%d", &ival) == 1)
        {
            if (ival >= POWERGOV_LOG_OFF && ival <= POWERGOV_LOG_DEBUG)
                config->log_level = (powergov_log_level_t)ival;
        }
        else if (sscanf(line, "DEV_LOG=%d", &ival) == 1)
        {
            config->dev_log_enabled = ival ? 1 : 0;
        }
        else if (sscanf(line, "THRESHOLD_LOW=%lf", &config->threshold_low) == 1)
            ;
        else if (sscanf(line, "THRESHOLD_MID=%lf", &config->threshold_mid) == 1)
            ;
        else if (sscanf(line, "THRESHOLD_HIGH=%lf", &config->threshold_high) == 1)
            ;
        else if (sscanf(line, "FREQ_CAP_BATTERY=%d", &ival) == 1)
        {
            if (ival >= 50 && ival <= 100)
                config->freq_cap_battery_pct = ival;
        }
        else if (sscanf(line, "LOW_BATTERY=%d", &ival) == 1)
        {
            if (ival >= 5 && ival <= 50)
                config->low_battery_pct = ival;
        }
        else if (sscanf(line, "PERIPHERAL_WIFI=%d", &ival) == 1)
            config->peripheral.wifi = ival ? 1 : 0;
        else if (sscanf(line, "PERIPHERAL_SATA=%d", &ival) == 1)
            config->peripheral.sata = ival ? 1 : 0;
        else if (sscanf(line, "PERIPHERAL_AUDIO=%d", &ival) == 1)
            config->peripheral.audio = ival ? 1 : 0;
        else if (sscanf(line, "CUSTOM_ALLOW_PERFORMANCE=%d", &ival) == 1)
            config->custom_allow_performance = ival ? 1 : 0;
        else if (sscanf(line, "CUSTOM_RUNTIME_AGGRESSIVE=%d", &ival) == 1)
            config->custom_runtime_aggressive = ival ? 1 : 0;
        else if (sscanf(line, "LID_AGGRESSIVE=%d", &ival) == 1)
            config->lid_aggressive = ival ? 1 : 0;
        else if (sscanf(line, "DISPLAY_AGGRESSIVE=%d", &ival) == 1)
            config->display_aggressive = ival ? 1 : 0;
        else if (sscanf(line, "CONTEXT_REQUIRE_LOW_LOAD=%d", &ival) == 1)
            config->context_require_low_load = ival ? 1 : 0;
        else if (sscanf(line, "CONTEXT_LOW_LOAD_PCT=%d", &ival) == 1)
        {
            if (ival >= 1 && ival <= 80)
                config->context_low_load_pct = ival;
        }
        else if (sscanf(line, "MEMORY_AWARE=%d", &ival) == 1)
            config->memory_aware = ival ? 1 : 0;
        else if (sscanf(line, "MEMORY_PSI_SOME_PCT=%d", &ival) == 1)
        {
            if (ival >= 1 && ival <= 100)
                config->memory_psi_some_pct = ival;
        }
        else if (sscanf(line, "MEMORY_PSI_FULL_PCT=%d", &ival) == 1)
        {
            if (ival >= 1 && ival <= 100)
                config->memory_psi_full_pct = ival;
        }
        else if (sscanf(line, "MEMORY_SWAP_PAGES_TICK=%d", &ival) == 1)
        {
            if (ival >= 1 && ival <= 65535)
                config->memory_swap_pages_tick = ival;
        }
        else if (sscanf(line, "MEMORY_SWAP_PAGES_SEVERE=%d", &ival) == 1)
        {
            if (ival >= 1 && ival <= 65535)
                config->memory_swap_pages_severe = ival;
        }
        else if (sscanf(line, "%63[^=]=%127s", key, val) == 2)
        {
            if (strcmp(key, "FEATURES_OFF") == 0)
            {
                powergov_features_t off = {0};
                parse_features_string(&off, val);
                {
                    int m = powergov_features_to_mask(&config->features);
                    int offm = powergov_features_to_mask(&off);
                    m &= ~offm;
                    powergov_features_from_mask(&config->features, m);
                }
            }
        }
    }

    fclose(f);
    config_normalize_thresholds(config);
    return 0;
}

int powergov_config_save(const powergov_config_t *config)
{
    FILE *f;
    char feat_buf[128];
    int pos;
    int i;

    if (!config)
        return -1;

    f = fopen(POWERGOV_CONF_TMP, "w");
    if (!f)
        return -1;

    fprintf(f, "# powergov persistent configuration\n");
    fprintf(f, "BATTERY_SAFE_THRESHOLD=%d\n",
            config->battery_safe_enabled ? config->battery_threshold : 0);
    fprintf(f, "USER_MODE=%s\n", powergov_user_mode_str(config->user_mode));

    pos = 0;
    feat_buf[0] = '\0';
    for (i = 0; i < POWERGOV_FEATURE_COUNT; i++)
    {
        int mask = powergov_features_to_mask(&config->features);
        int n;
        if (mask & (1 << i))
        {
            if (pos > 0 && pos < (int)sizeof(feat_buf) - 1)
                pos += snprintf(feat_buf + pos, sizeof(feat_buf) - (size_t)pos, ",");
            if (pos >= (int)sizeof(feat_buf) - 1)
                break;
            n = snprintf(feat_buf + pos, sizeof(feat_buf) - (size_t)pos, "%s",
                         powergov_feature_name((powergov_feature_id_t)i));
            if (n > 0)
                pos += n;
            if (pos >= (int)sizeof(feat_buf))
                pos = (int)sizeof(feat_buf) - 1;
        }
    }
    feat_buf[sizeof(feat_buf) - 1] = '\0';
    fprintf(f, "FEATURES=%s\n", feat_buf);
    fprintf(f, "LOG_LEVEL=%d\n", (int)config->log_level);
    fprintf(f, "DEV_LOG=%d\n", config->dev_log_enabled);
    fprintf(f, "THRESHOLD_LOW=%.2f\n", config->threshold_low);
    fprintf(f, "THRESHOLD_MID=%.2f\n", config->threshold_mid);
    fprintf(f, "THRESHOLD_HIGH=%.2f\n", config->threshold_high);
    fprintf(f, "FREQ_CAP_BATTERY=%d\n", config->freq_cap_battery_pct);
    fprintf(f, "LOW_BATTERY=%d\n", config->low_battery_pct);
    fprintf(f, "PERIPHERAL_WIFI=%d\n", config->peripheral.wifi);
    fprintf(f, "PERIPHERAL_SATA=%d\n", config->peripheral.sata);
    fprintf(f, "PERIPHERAL_AUDIO=%d\n", config->peripheral.audio);
    fprintf(f, "CUSTOM_ALLOW_PERFORMANCE=%d\n", config->custom_allow_performance);
    fprintf(f, "CUSTOM_RUNTIME_AGGRESSIVE=%d\n", config->custom_runtime_aggressive);
    fprintf(f, "LID_AGGRESSIVE=%d\n", config->lid_aggressive);
    fprintf(f, "DISPLAY_AGGRESSIVE=%d\n", config->display_aggressive);
    fprintf(f, "CONTEXT_REQUIRE_LOW_LOAD=%d\n", config->context_require_low_load);
    fprintf(f, "CONTEXT_LOW_LOAD_PCT=%d\n", config->context_low_load_pct);
    fprintf(f, "MEMORY_AWARE=%d\n", config->memory_aware);
    fprintf(f, "MEMORY_PSI_SOME_PCT=%d\n", config->memory_psi_some_pct);
    fprintf(f, "MEMORY_PSI_FULL_PCT=%d\n", config->memory_psi_full_pct);
    fprintf(f, "MEMORY_SWAP_PAGES_TICK=%d\n", config->memory_swap_pages_tick);
    fprintf(f, "MEMORY_SWAP_PAGES_SEVERE=%d\n", config->memory_swap_pages_severe);

    if (fclose(f) != 0)
        return -1;

    if (rename(POWERGOV_CONF_TMP, POWERGOV_CONF_PATH) != 0)
        return -1;

    return 0;
}
