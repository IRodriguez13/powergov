#include "metrics.h"
#include "../core/sysfs.h"
#include "../log/log.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static powergov_metrics_t g_metrics;
static uint64_t g_rapl_prev_uj;
static int g_rapl_prev_valid;

static const char *RAPL_PATH =
    "/sys/class/powercap/intel-rapl:0/energy_uj";

void powergov_metrics_init(void)
{
    memset(&g_metrics, 0, sizeof(g_metrics));
    g_metrics.rapl_available = sysfs_path_exists(RAPL_PATH);
    g_rapl_prev_valid = 0;
}

powergov_metrics_t *powergov_metrics_get(void)
{
    return &g_metrics;
}

void powergov_metrics_tick(void)
{
    g_metrics.loop_ticks++;
}

void powergov_metrics_state_change(void)
{
    g_metrics.state_transitions++;
}

void powergov_metrics_apply(powergov_feature_id_t id, int ok, int skipped)
{
    if (id < 0 || id >= POWERGOV_FEATURE_COUNT)
        return;
    if (skipped)
        g_metrics.apply_skip[id]++;
    else if (ok)
        g_metrics.apply_ok[id]++;
    else
        g_metrics.apply_fail[id]++;
}

void powergov_metrics_verify(powergov_feature_id_t id, int ok)
{
    if (id < 0 || id >= POWERGOV_FEATURE_COUNT)
        return;
    if (ok)
        g_metrics.verify_ok[id]++;
    else
        g_metrics.verify_fail[id]++;
}

void powergov_metrics_sample_rapl(void)
{
    long uj;

    if (!g_metrics.rapl_available)
        return;

    if (sysfs_read_int(RAPL_PATH, &uj) != 0)
        return;

    g_metrics.rapl_energy_uj = (uint64_t)uj;

    if (g_rapl_prev_valid && uj >= (long)g_rapl_prev_uj)
    {
        uint64_t delta = (uint64_t)uj - g_rapl_prev_uj;
        /* Each loop tick ~2s */
        g_metrics.rapl_watts = (double)delta / 2000000.0;
    }

    g_rapl_prev_uj = (uint64_t)uj;
    g_rapl_prev_valid = 1;
}

int powergov_metrics_write_file(void)
{
    FILE *f;
    int i;

    f = fopen(POWERGOV_METRICS_PATH, "w");
    if (!f)
        return -1;

    fprintf(f, "loop_ticks=%llu\n", (unsigned long long)g_metrics.loop_ticks);
    fprintf(f, "state_transitions=%llu\n",
            (unsigned long long)g_metrics.state_transitions);
    for (i = 0; i < POWERGOV_FEATURE_COUNT; i++)
    {
        fprintf(f, "%s_apply_ok=%llu\n", powergov_feature_name((powergov_feature_id_t)i),
                (unsigned long long)g_metrics.apply_ok[i]);
        fprintf(f, "%s_apply_fail=%llu\n", powergov_feature_name((powergov_feature_id_t)i),
                (unsigned long long)g_metrics.apply_fail[i]);
        fprintf(f, "%s_apply_skip=%llu\n", powergov_feature_name((powergov_feature_id_t)i),
                (unsigned long long)g_metrics.apply_skip[i]);
        fprintf(f, "%s_verify_ok=%llu\n", powergov_feature_name((powergov_feature_id_t)i),
                (unsigned long long)g_metrics.verify_ok[i]);
        fprintf(f, "%s_verify_fail=%llu\n", powergov_feature_name((powergov_feature_id_t)i),
                (unsigned long long)g_metrics.verify_fail[i]);
    }
    fprintf(f, "rapl_available=%d\n", g_metrics.rapl_available);
    fprintf(f, "rapl_energy_uj=%llu\n", (unsigned long long)g_metrics.rapl_energy_uj);
    fprintf(f, "rapl_watts_est=%.3f\n", g_metrics.rapl_watts);

    fclose(f);
    return 0;
}

int powergov_metrics_load_file(void)
{
    FILE *f;
    char line[128];
    char key[64];
    unsigned long long val;
    double dval;
    int i;

    f = fopen(POWERGOV_METRICS_PATH, "r");
    if (!f)
        return -1;

    while (fgets(line, sizeof(line), f))
    {
        if (sscanf(line, "loop_ticks=%llu", &val) == 1)
            g_metrics.loop_ticks = val;
        else if (sscanf(line, "state_transitions=%llu", &val) == 1)
            g_metrics.state_transitions = val;
        else if (sscanf(line, "rapl_available=%d", &g_metrics.rapl_available) == 1)
            ;
        else if (sscanf(line, "rapl_energy_uj=%llu", &val) == 1)
            g_metrics.rapl_energy_uj = val;
        else if (sscanf(line, "rapl_watts_est=%lf", &dval) == 1)
            g_metrics.rapl_watts = dval;
        else
        {
            for (i = 0; i < POWERGOV_FEATURE_COUNT; i++)
            {
                const char *name = powergov_feature_name((powergov_feature_id_t)i);
                if (sscanf(line, "%63[^=]=%llu", key, &val) == 2)
                {
                    char expect[80];
                    snprintf(expect, sizeof(expect), "%s_apply_ok", name);
                    if (strcmp(key, expect) == 0)
                        g_metrics.apply_ok[i] = val;
                    snprintf(expect, sizeof(expect), "%s_apply_fail", name);
                    if (strcmp(key, expect) == 0)
                        g_metrics.apply_fail[i] = val;
                    snprintf(expect, sizeof(expect), "%s_apply_skip", name);
                    if (strcmp(key, expect) == 0)
                        g_metrics.apply_skip[i] = val;
                    snprintf(expect, sizeof(expect), "%s_verify_ok", name);
                    if (strcmp(key, expect) == 0)
                        g_metrics.verify_ok[i] = val;
                    snprintf(expect, sizeof(expect), "%s_verify_fail", name);
                    if (strcmp(key, expect) == 0)
                        g_metrics.verify_fail[i] = val;
                }
            }
        }
    }

    fclose(f);
    return 0;
}

int powergov_metrics_print_human(void)
{
    int i;

    printf("=== powergov metrics (dev) ===\n");
    printf("loop_ticks:         %llu\n", (unsigned long long)g_metrics.loop_ticks);
    printf("state_transitions:  %llu\n", (unsigned long long)g_metrics.state_transitions);

    for (i = 0; i < POWERGOV_FEATURE_COUNT; i++)
    {
        printf("[%s] ok=%llu fail=%llu skip=%llu verify_ok=%llu verify_fail=%llu\n",
               powergov_feature_name((powergov_feature_id_t)i),
               (unsigned long long)g_metrics.apply_ok[i],
               (unsigned long long)g_metrics.apply_fail[i],
               (unsigned long long)g_metrics.apply_skip[i],
               (unsigned long long)g_metrics.verify_ok[i],
               (unsigned long long)g_metrics.verify_fail[i]);
    }

    if (g_metrics.rapl_available)
    {
        printf("RAPL energy_uj:     %llu\n", (unsigned long long)g_metrics.rapl_energy_uj);
        printf("RAPL watts (est):   %.3f W\n", g_metrics.rapl_watts);
    }
    else
    {
        printf("RAPL:               not available\n");
    }

    if (access(POWERGOV_METRICS_PATH, R_OK) == 0)
        printf("metrics file:       %s\n", POWERGOV_METRICS_PATH);
    else
        printf("metrics file:       (daemon not running or not yet written)\n");

    return 0;
}
