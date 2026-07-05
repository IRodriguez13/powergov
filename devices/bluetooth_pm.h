#ifndef POWERGOV_BLUETOOTH_PM_H
#define POWERGOV_BLUETOOTH_PM_H

int bluetooth_pm_apply(int aggression);
int bluetooth_pm_restore(void);
int bluetooth_pm_available(void);

#endif
