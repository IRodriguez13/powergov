#ifndef POWERGOV_PLATFORM_PROFILE_H
#define POWERGOV_PLATFORM_PROFILE_H

#include <stddef.h>

int platform_ppd_active(void);
int platform_profile_available(void);
int platform_profile_read(char *out, size_t out_sz);
int platform_profile_apply(const char *profile);
int platform_profile_verify(const char *expected);

#endif
