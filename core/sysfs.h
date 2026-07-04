#ifndef POWERGOV_SYSFS_H
#define POWERGOV_SYSFS_H

#include <stddef.h>

#define POWERGOV_CPU_BASE "/sys/devices/system/cpu"

int sysfs_read_file(const char *path, char *out, size_t out_sz);
int sysfs_write_file(const char *path, const char *value);
int sysfs_read_int(const char *path, long *out);
int sysfs_read_first_cpu_leaf(const char *leaf, char *out, size_t out_sz);
int sysfs_write_all_cpu_leaf(const char *leaf, const char *value);
int sysfs_path_exists(const char *path);

#endif
