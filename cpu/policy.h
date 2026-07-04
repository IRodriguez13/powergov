#ifndef POWERGOV_CPU_POLICY_H
#define POWERGOV_CPU_POLICY_H

#include "../include/powergov/types.h"

int cpu_policy_apply(const powergov_config_t *cfg,
                     const powergov_effective_policy_t *policy);
int cpu_policy_restore(const powergov_config_t *cfg);

#endif
