#include "types.h"
#include <string.h>

const char *powergov_user_mode_str(powergov_user_mode_t mode)
{
    switch (mode)
    {
    case POWERGOV_USER_MAX_BATTERY: return "max-battery";
    case POWERGOV_USER_BALANCED:    return "balanced";
    case POWERGOV_USER_PERFORMANCE: return "performance";
    default:                        return "unknown";
    }
}

const char *powergov_gov_state_str(powergov_gov_state_t state)
{
    switch (state)
    {
    case POWERGOV_GOV_POWERSAVE:   return "POWERSAVE";
    case POWERGOV_GOV_BALANCED:    return "BALANCED";
    case POWERGOV_GOV_PERFORMANCE: return "PERFORMANCE";
    default:                       return "UNKNOWN";
    }
}

const char *powergov_power_source_str(powergov_power_source_t src)
{
    switch (src)
    {
    case POWERGOV_POWER_AC:      return "AC";
    case POWERGOV_POWER_BATTERY: return "battery";
    default:                     return "unknown";
    }
}

powergov_user_mode_t powergov_user_mode_parse(const char *s)
{
    if (!s)
        return POWERGOV_USER_MAX_BATTERY;
    if (strcmp(s, "max-battery") == 0 || strcmp(s, "max") == 0)
        return POWERGOV_USER_MAX_BATTERY;
    if (strcmp(s, "balanced") == 0)
        return POWERGOV_USER_BALANCED;
    if (strcmp(s, "performance") == 0 || strcmp(s, "perf") == 0)
        return POWERGOV_USER_PERFORMANCE;
    return POWERGOV_USER_MAX_BATTERY;
}

const char *powergov_feature_name(powergov_feature_id_t id)
{
    switch (id)
    {
    case POWERGOV_FEATURE_GOVERNOR:   return "governor";
    case POWERGOV_FEATURE_EPP:        return "epp";
    case POWERGOV_FEATURE_FREQ_CAP:   return "freq_cap";
    case POWERGOV_FEATURE_TURBO:      return "turbo";
    case POWERGOV_FEATURE_PLATFORM:   return "platform";
    case POWERGOV_FEATURE_RUNTIME_PM: return "runtime_pm";
    default:                          return "unknown";
    }
}

void powergov_config_set_defaults(powergov_config_t *cfg)
{
    if (!cfg)
        return;

    memset(cfg, 0, sizeof(*cfg));
    cfg->user_mode = POWERGOV_USER_MAX_BATTERY;
    cfg->features.cpu_governor = 1;
    cfg->features.cpu_epp = 1;
    cfg->features.cpu_freq_cap = 1;
    cfg->features.cpu_turbo = 1;
    cfg->features.platform_profile = 1;
    cfg->features.runtime_pm = 1;
    cfg->log_level = POWERGOV_LOG_INFO;
    cfg->dev_log_enabled = 1;
    cfg->threshold_low = POWERGOV_THRESHOLD_LOW_DEF;
    cfg->threshold_mid = POWERGOV_THRESHOLD_MID_DEF;
    cfg->threshold_high = POWERGOV_THRESHOLD_HIGH_DEF;
    cfg->freq_cap_battery_pct = POWERGOV_FREQ_CAP_BATTERY_DEF;
    cfg->low_battery_pct = POWERGOV_LOW_BATTERY_DEF;
}

int powergov_features_to_mask(const powergov_features_t *f)
{
    int m = 0;
    if (!f)
        return 0;
    if (f->cpu_governor)     m |= (1 << POWERGOV_FEATURE_GOVERNOR);
    if (f->cpu_epp)          m |= (1 << POWERGOV_FEATURE_EPP);
    if (f->cpu_freq_cap)     m |= (1 << POWERGOV_FEATURE_FREQ_CAP);
    if (f->cpu_turbo)        m |= (1 << POWERGOV_FEATURE_TURBO);
    if (f->platform_profile) m |= (1 << POWERGOV_FEATURE_PLATFORM);
    if (f->runtime_pm)       m |= (1 << POWERGOV_FEATURE_RUNTIME_PM);
    return m;
}

void powergov_features_from_mask(powergov_features_t *f, int mask)
{
    if (!f)
        return;
    f->cpu_governor = (mask >> POWERGOV_FEATURE_GOVERNOR) & 1;
    f->cpu_epp = (mask >> POWERGOV_FEATURE_EPP) & 1;
    f->cpu_freq_cap = (mask >> POWERGOV_FEATURE_FREQ_CAP) & 1;
    f->cpu_turbo = (mask >> POWERGOV_FEATURE_TURBO) & 1;
    f->platform_profile = (mask >> POWERGOV_FEATURE_PLATFORM) & 1;
    f->runtime_pm = (mask >> POWERGOV_FEATURE_RUNTIME_PM) & 1;
}
