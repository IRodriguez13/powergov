#ifndef POWERGOV_CPU_TURBO_H
#define POWERGOV_CPU_TURBO_H

int cpu_turbo_available(void);
int cpu_turbo_read(int *enabled);
int cpu_turbo_apply(int enabled);
int cpu_turbo_verify(int enabled);

#endif
