#include "policy.h"
#include "governor.h"
#include "epp.h"
#include "freq_cap.h"
#include "turbo.h"
#include "../log/log.h"

int cpu_policy_apply(const powergov_config_t *cfg,
                     const powergov_effective_policy_t *policy)
{
    if (!cfg || !policy)
        return -1;

    if (cfg->features.cpu_governor && policy->governor)
        cpu_governor_apply(policy->governor);

    if (cfg->features.cpu_epp && policy->epp && cpu_epp_available())
        cpu_epp_apply(policy->epp);

    if (cfg->features.cpu_freq_cap)
    {
        if (policy->freq_cap_pct > 0)
            cpu_freq_cap_apply_pct(policy->freq_cap_pct);
        else
            cpu_freq_cap_restore();
    }

    if (cfg->features.cpu_turbo && cpu_turbo_available())
        cpu_turbo_apply(policy->turbo_on);

    return 0;
}

int cpu_policy_restore(const powergov_config_t *cfg)
{
    if (!cfg)
        return -1;

    if (cfg->features.cpu_freq_cap)
        cpu_freq_cap_restore();

    PG_LOG_I("cpu_policy", "restore complete");
    return 0;
}
