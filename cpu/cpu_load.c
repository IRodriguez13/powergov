#include "cpu_load.h"
#include <stdio.h>
#include <unistd.h>

static double g_cpu_load_cache = 0.0;

double powergov_cpu_load_cached(void)
{
    return g_cpu_load_cache;
}

static int read_cpu_times(unsigned long long *idle,
                          unsigned long long *total)
{
    FILE *fp;
    unsigned long long user, nice, system, idle_t, io_wait, irq, softirq;

    if (!idle || !total)
        return -1;

    fp = fopen("/proc/stat", "r");
    if (!fp)
        return -1;

    if (fscanf(fp, "cpu %llu %llu %llu %llu %llu %llu %llu",
               &user, &nice, &system, &idle_t, &io_wait, &irq, &softirq) != 7)
    {
        fclose(fp);
        return -1;
    }

    *idle = idle_t;
    *total = user + nice + system + idle_t + io_wait + irq + softirq;
    fclose(fp);
    return 0;
}

double get_cpu_usage(void)
{
    unsigned long long idle1 = 0;
    unsigned long long total1 = 0;
    unsigned long long idle2 = 0;
    unsigned long long total2 = 0;
    unsigned long long idle_delta;
    unsigned long long total_delta;

    if (read_cpu_times(&idle1, &total1) != 0)
        return 0.0;

    usleep(200000);

    if (read_cpu_times(&idle2, &total2) != 0)
        return 0.0;

    idle_delta = idle2 - idle1;
    total_delta = total2 - total1;

    if (total_delta == 0)
        return g_cpu_load_cache;

    g_cpu_load_cache = 1.0 - ((double)idle_delta / (double)total_delta);
    return g_cpu_load_cache;
}
