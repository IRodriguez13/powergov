#ifndef POWERGOV_POWER_SUPPLY_H
#define POWERGOV_POWER_SUPPLY_H

#include "../include/powergov/types.h"

typedef struct
{
    int present;
    int capacity_pct;
    powergov_power_source_t source;
    char name[64];
} powergov_power_info_t;

int powergov_power_supply_detect(void);
int powergov_power_supply_poll(powergov_power_info_t *info);

/* Legacy helper used by CLI */
int get_battery_level(void);

#endif
