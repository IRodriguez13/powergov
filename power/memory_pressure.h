#ifndef POWERGOV_MEMORY_PRESSURE_H
#define POWERGOV_MEMORY_PRESSURE_H

#include "../include/powergov/types.h"

typedef struct
{
    int psi_available;
    double psi_some_avg10;
    double psi_full_avg10;
    unsigned long swap_pages_in;
    unsigned long swap_pages_out;
} powergov_memory_pressure_t;

int powergov_memory_pressure_poll(powergov_memory_pressure_t *out);

void powergov_memory_pressure_classify(const powergov_config_t *cfg,
                                       const powergov_memory_pressure_t *mp,
                                       int *stressed_out,
                                       int *severe_out);

#endif
