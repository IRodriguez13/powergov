#ifndef POWERGOV_CPU_GOVERNOR_H
#define POWERGOV_CPU_GOVERNOR_H

#include <stddef.h>

int cpu_governor_read(char *out, size_t out_sz);
int cpu_governor_apply(const char *gov);
int cpu_governor_verify(const char *expected);

#endif
