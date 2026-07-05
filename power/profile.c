#include "profile.h"
#include "power_supply.h"
#include <string.h>

static void apply_gov_state_defaults(powergov_gov_state_t gov_state,
                                     int on_battery,
                                     powergov_effective_policy_t *out)
{
    switch (gov_state)
    {
    case POWERGOV_GOV_POWERSAVE:
        out->governor = "powersave";
        out->epp = "power";
        out->turbo_on = 0;
        break;
    case POWERGOV_GOV_BALANCED:
        out->governor = "schedutil";
        out->epp = on_battery ? "balance_power" : "balance_performance";
        out->turbo_on = on_battery ? 0 : 1;
        break;
    case POWERGOV_GOV_PERFORMANCE:
        out->governor = "performance";
        out->epp = "performance";
        out->turbo_on = 1;
        break;
    default:
        out->governor = "schedutil";
        out->epp = "default";
        out->turbo_on = 1;
        break;
    }
}

static int peripheral_level_from_opts(const powergov_config_t *cfg,
                                      int on_battery, int low_battery)
{
    int level = 0;

    if (!on_battery || !cfg)
        return 0;

    if (cfg->peripheral.wifi || cfg->peripheral.sata || cfg->peripheral.audio)
        level = 1;

    if (low_battery && cfg->peripheral.sata)
        level = 2;

    return level;
}

void powergov_profile_compute(const powergov_config_t *cfg,
                              const powergov_power_info_t *power,
                              powergov_gov_state_t gov_state,
                              int battery_limited,
                              powergov_effective_policy_t *out)
{
    int on_battery;
    int low_battery;
    powergov_user_mode_t mode;

    if (!cfg || !out)
        return;

    memset(out, 0, sizeof(*out));
    on_battery = power && power->present &&
                 power->source == POWERGOV_POWER_BATTERY;
    low_battery = on_battery && power->capacity_pct >= 0 &&
                  power->capacity_pct <= cfg->low_battery_pct;
    mode = cfg->user_mode;

    apply_gov_state_defaults(gov_state, on_battery, out);

    if (mode == POWERGOV_USER_CUSTOM)
    {
        out->allow_performance = cfg->custom_allow_performance;
        if (on_battery)
        {
            out->freq_cap_pct = cfg->freq_cap_battery_pct;
            out->platform_profile = "balanced";
            out->runtime_pm_aggressive = cfg->custom_runtime_aggressive;
            out->peripheral_pm_level =
                peripheral_level_from_opts(cfg, on_battery, low_battery);
        }
        else
        {
            out->freq_cap_pct = 0;
            out->platform_profile =
                cfg->custom_allow_performance ? "performance" : "balanced";
            out->runtime_pm_aggressive = 0;
            out->peripheral_pm_level = 0;
        }

        if (!out->allow_performance && gov_state == POWERGOV_GOV_PERFORMANCE)
        {
            out->governor = "schedutil";
            out->epp = on_battery ? "balance_power" : "balance_performance";
            out->turbo_on = on_battery ? 0 : 1;
        }
        return;
    }

    if (mode == POWERGOV_USER_PERFORMANCE)
        out->allow_performance = 1;
    else if (battery_limited || low_battery)
        out->allow_performance = 0;
    else if (on_battery && mode == POWERGOV_USER_MAX_BATTERY)
        out->allow_performance = 0;
    else if (on_battery)
        out->allow_performance = 0;
    else
        out->allow_performance = 1;

    if (on_battery)
    {
        if (mode == POWERGOV_USER_MAX_BATTERY || low_battery)
        {
            if (low_battery)
            {
                out->freq_cap_pct = POWERGOV_LOW_BATTERY_FREQ_CAP;
                out->platform_profile = "low-power";
                out->runtime_pm_aggressive = 1;
            }
            else if (power->capacity_pct >= 0 &&
                     power->capacity_pct <= POWERGOV_CONSERVE_BATTERY_PCT)
            {
                out->freq_cap_pct = 0;
                out->platform_profile = "balanced";
                out->runtime_pm_aggressive = 0;
            }
            else if (mode == POWERGOV_USER_MAX_BATTERY)
            {
                out->freq_cap_pct = 0;
                out->platform_profile = "balanced";
                out->runtime_pm_aggressive = 0;
            }
            if (gov_state == POWERGOV_GOV_PERFORMANCE && !out->allow_performance)
            {
                out->governor = "schedutil";
                out->epp = "balance_power";
                out->turbo_on = 0;
            }
        }
        else if (mode == POWERGOV_USER_BALANCED)
        {
            out->freq_cap_pct = cfg->freq_cap_battery_pct;
            out->platform_profile = "balanced";
            out->runtime_pm_aggressive = 1;
            if (gov_state == POWERGOV_GOV_PERFORMANCE && !out->allow_performance)
            {
                out->governor = "schedutil";
                out->epp = "balance_performance";
            }
        }
        else
        {
            out->freq_cap_pct = 0;
            out->platform_profile = "balanced";
            out->runtime_pm_aggressive = 0;
        }

        if (low_battery)
            out->peripheral_pm_level = 2;
        else if (mode == POWERGOV_USER_MAX_BATTERY ||
                 mode == POWERGOV_USER_BALANCED)
            out->peripheral_pm_level = 1;
        else
            out->peripheral_pm_level = 0;
    }
    else
    {
        out->freq_cap_pct = 0;
        out->platform_profile =
            (gov_state == POWERGOV_GOV_PERFORMANCE) ? "performance" : "balanced";
        out->runtime_pm_aggressive = 0;
        out->peripheral_pm_level = 0;
    }

    if (!out->allow_performance && gov_state == POWERGOV_GOV_PERFORMANCE)
    {
        out->governor = "schedutil";
        out->epp = on_battery ? "balance_power" : "balance_performance";
        out->turbo_on = on_battery ? 0 : 1;
    }
}
