#ifndef POWERGOV_CPU_EPP_H
#define POWERGOV_CPU_EPP_H

#include <stddef.h>

int cpu_epp_available(void);
int cpu_epp_read(char *out, size_t out_sz);
int cpu_epp_apply(const char *epp);
int cpu_epp_verify(const char *expected);

#endif
