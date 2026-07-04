#ifndef POWERGOV_METRICS_H
#define POWERGOV_METRICS_H

#include "../include/powergov/types.h"
#include <stdint.h>

typedef struct
{
    uint64_t loop_ticks;
    uint64_t state_transitions;
    uint64_t apply_ok[POWERGOV_FEATURE_COUNT];
    uint64_t apply_fail[POWERGOV_FEATURE_COUNT];
    uint64_t apply_skip[POWERGOV_FEATURE_COUNT];
    uint64_t verify_ok[POWERGOV_FEATURE_COUNT];
    uint64_t verify_fail[POWERGOV_FEATURE_COUNT];
    uint64_t rapl_energy_uj;
    double rapl_watts;
    int rapl_available;
} powergov_metrics_t;

void powergov_metrics_init(void);
powergov_metrics_t *powergov_metrics_get(void);

void powergov_metrics_tick(void);
void powergov_metrics_state_change(void);
void powergov_metrics_apply(powergov_feature_id_t id, int ok, int skipped);
void powergov_metrics_verify(powergov_feature_id_t id, int ok);

void powergov_metrics_sample_rapl(void);
int powergov_metrics_write_file(void);
int powergov_metrics_load_file(void);
int powergov_metrics_print_human(void);

#endif
