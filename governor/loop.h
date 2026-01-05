#ifndef LOOP_H
#define LOOP_H

typedef struct
{
    int battery_safe_enabled;
    int battery_threshold;
} powergov_config_t;


void powergov_loop();

#endif