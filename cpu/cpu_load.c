#include "cpu_load.h"
#include <stdio.h>
#include <unistd.h>

static void read_cpu_times(unsigned long long *idle,
                           unsigned long long *total)
{
    FILE *fp  = fopen("/proc/stat","r");

    if(!fp) return;

    unsigned long long user, nice, system, idle_t, io_wait, irq, softirq;
    
    fscanf(fp, "cpu %llu %llu %llu %llu %llu %llu %llu",
           &user, &nice, &system, &idle_t, &io_wait, &irq, &softirq);

    *idle = idle_t + io_wait;
    *total = user + nice + system + idle_t + irq + softirq;

    fclose(fp);

}

/* 
    This is simple, I know but it's a deterministic way to get an aprox cpu usage number
    
    The business is: read, wait a bit, read again & calculate the deltas whit 
    the total and idle times.
*/
double get_cpu_usage(void)
{
    unsigned long long idle1,total1;
    unsigned long long idle2,total2;

    read_cpu_times(&idle1, &total1);
 
    usleep(200000); // 200 ms window

    read_cpu_times(&idle2, &total2);

    unsigned long long idle_delta = idle2 - idle1;
    unsigned long long total_delta = total2 - total1;

    if(total_delta == 0) return 0.0;

    return 1.0 - ((double)idle_delta / (double)total_delta);
}