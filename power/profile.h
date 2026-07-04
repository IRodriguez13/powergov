#ifndef POWERGOV_PROFILE_H
#define POWERGOV_PROFILE_H

#include "../include/powergov/types.h"
#include "power_supply.h"

void powergov_profile_compute(const powergov_config_t *cfg,
                              const powergov_power_info_t *power,
                              powergov_gov_state_t gov_state,
                              int battery_limited,
                              powergov_effective_policy_t *out);

#endif
