#ifndef POWERGOV_CPU_FREQ_CAP_H
#define POWERGOV_CPU_FREQ_CAP_H

int cpu_freq_cap_available(void);
int cpu_freq_cap_apply_pct(int pct);
int cpu_freq_cap_restore(void);
int cpu_freq_cap_verify_pct(int pct);

#endif
