#ifndef POWERGOV_DISK_PM_H
#define POWERGOV_DISK_PM_H

#include "../include/powergov/types.h"

int disk_pm_apply(int aggression, const powergov_peripheral_opts_t *opts);
int disk_pm_restore(void);
int disk_pm_available(void);

#endif
