#ifndef POWERGOV_STATE_MACHINE_H
#define POWERGOV_STATE_MACHINE_H

#include "../include/powergov/types.h"

typedef struct
{
    powergov_gov_state_t state;
    int up_streak;
    int down_streak;
} powergov_state_machine_t;

void powergov_state_machine_init(powergov_state_machine_t *sm,
                                 powergov_gov_state_t initial);
powergov_gov_state_t powergov_state_machine_step(
    powergov_state_machine_t *sm,
    const powergov_config_t *cfg,
    double load,
    int battery_limited,
    int allow_performance,
    int memory_stressed);

#endif
