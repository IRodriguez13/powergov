#include "sysfs.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int sysfs_path_exists(const char *path)
{
    return access(path, F_OK) == 0;
}

int sysfs_read_file(const char *path, char *out, size_t out_sz)
{
    FILE *f;

    if (!path || !out || out_sz == 0)
        return -1;

    f = fopen(path, "r");
    if (!f)
        return -1;

    if (!fgets(out, (int)out_sz, f))
    {
        fclose(f);
        return -1;
    }

    fclose(f);
    out[strcspn(out, "\n")] = '\0';
    return 0;
}

int sysfs_write_file(const char *path, const char *value)
{
    FILE *f;
    int ret = 0;

    if (!path || !value)
        return -1;

    f = fopen(path, "w");
    if (!f)
        return -1;

    if (fprintf(f, "%s", value) < 0)
        ret = -1;

    if (fclose(f) != 0)
        ret = -1;

    return ret;
}

int sysfs_read_int(const char *path, long *out)
{
    char buf[64];

    if (!out || sysfs_read_file(path, buf, sizeof(buf)) != 0)
        return -1;

    if (sscanf(buf, "%ld", out) != 1)
        return -1;

    return 0;
}

int sysfs_read_first_cpu_leaf(const char *leaf, char *out, size_t out_sz)
{
    char path[512];

    if (!leaf || !out)
        return -1;

    snprintf(path, sizeof(path), POWERGOV_CPU_BASE "/cpu0/cpufreq/%s", leaf);
    return sysfs_read_file(path, out, out_sz);
}

int sysfs_write_all_cpu_leaf(const char *leaf, const char *value)
{
    DIR *d;
    struct dirent *entry;
    char path[512];
    int ok = 0;
    int fail = 0;

    if (!leaf || !value)
        return -1;

    d = opendir(POWERGOV_CPU_BASE);
    if (!d)
        return -1;

    while ((entry = readdir(d)) != NULL)
    {
        if (strncmp(entry->d_name, "cpu", 3) != 0)
            continue;
        if (entry->d_name[3] < '0' || entry->d_name[3] > '9')
            continue;

        snprintf(path, sizeof(path),
                 POWERGOV_CPU_BASE "/%s/cpufreq/%s",
                 entry->d_name, leaf);

        if (sysfs_write_file(path, value) == 0)
            ok++;
        else
            fail++;
    }

    closedir(d);
    return (ok > 0 && fail == 0) ? 0 : -1;
}
