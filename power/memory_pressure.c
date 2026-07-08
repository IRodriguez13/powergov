#include "memory_pressure.h"
#include <stdio.h>
#include <string.h>

static unsigned long g_pswpin_last;
static unsigned long g_pswpout_last;
static int g_vmstat_init;

static int read_psi_line(const char *kind, double *avg10_out)
{
    FILE *f;
    char line[128];
    char label[16];
    double avg10;
    double avg60;
    double avg300;
    unsigned long total;

    if (!kind || !avg10_out)
        return -1;

    f = fopen("/proc/pressure/memory", "r");
    if (!f)
        return -1;

    while (fgets(line, sizeof(line), f))
    {
        if (sscanf(line, "%15s avg10=%lf avg60=%lf avg300=%lf total=%lu",
                   label, &avg10, &avg60, &avg300, &total) >= 2 &&
            strcmp(label, kind) == 0)
        {
            *avg10_out = avg10;
            fclose(f);
            return 0;
        }
    }

    fclose(f);
    return -1;
}

static int read_vmstat_swap(unsigned long *pswpin, unsigned long *pswpout)
{
    FILE *f;
    char key[64];
    unsigned long val;

    if (!pswpin || !pswpout)
        return -1;

    *pswpin = 0;
    *pswpout = 0;

    f = fopen("/proc/vmstat", "r");
    if (!f)
        return -1;

    while (fscanf(f, " %63s %lu", key, &val) == 2)
    {
        if (strcmp(key, "pswpin") == 0)
            *pswpin = val;
        else if (strcmp(key, "pswpout") == 0)
            *pswpout = val;
    }

    fclose(f);
    return 0;
}

int powergov_memory_pressure_poll(powergov_memory_pressure_t *out)
{
    unsigned long pswpin;
    unsigned long pswpout;

    if (!out)
        return -1;

    memset(out, 0, sizeof(*out));

    if (read_psi_line("some", &out->psi_some_avg10) == 0)
        out->psi_available = 1;
    read_psi_line("full", &out->psi_full_avg10);

    if (read_vmstat_swap(&pswpin, &pswpout) == 0)
    {
        if (g_vmstat_init)
        {
            out->swap_pages_in = pswpin - g_pswpin_last;
            out->swap_pages_out = pswpout - g_pswpout_last;
        }
        g_pswpin_last = pswpin;
        g_pswpout_last = pswpout;
        g_vmstat_init = 1;
    }

    return 0;
}

void powergov_memory_pressure_classify(const powergov_config_t *cfg,
                                       const powergov_memory_pressure_t *mp,
                                       int *stressed_out,
                                       int *severe_out)
{
    unsigned long swap_total;
    int stressed = 0;
    int severe = 0;

    if (stressed_out)
        *stressed_out = 0;
    if (severe_out)
        *severe_out = 0;

    if (!cfg || !cfg->memory_aware || !mp)
        return;

    swap_total = mp->swap_pages_in + mp->swap_pages_out;

    if (mp->psi_available)
    {
        if (mp->psi_full_avg10 >= (double)cfg->memory_psi_full_pct)
            severe = 1;
        if (mp->psi_some_avg10 >= (double)cfg->memory_psi_some_pct)
            stressed = 1;
    }

    if (swap_total >= (unsigned long)cfg->memory_swap_pages_severe)
        severe = 1;
    else if (swap_total >= (unsigned long)cfg->memory_swap_pages_tick)
        stressed = 1;

    if (severe)
        stressed = 1;

    if (stressed_out)
        *stressed_out = stressed;
    if (severe_out)
        *severe_out = severe;
}
