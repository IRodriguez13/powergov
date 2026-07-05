#ifndef POWERGOV_PERIPHERAL_PM_H
#define POWERGOV_PERIPHERAL_PM_H

#include "../include/powergov/types.h"

/* 0=restore, 1=WiFi+SATA med+audio on battery, 2=level 1 + SATA min_power */
int peripheral_pm_apply(int level);
int peripheral_pm_apply_cfg(int level, const powergov_peripheral_opts_t *opts);
int peripheral_pm_restore(void);
int peripheral_pm_available(void);

#endif
