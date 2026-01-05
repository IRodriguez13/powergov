#include "loop.h"
#include "../Battery/battery.h"
#include "../cpu/cpu_load.h"
#include "governor.h"
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

#define CPU_LOW 0.25
#define CPU_MEDIUM 0.60
#define CPU_HIGH 0.75

typedef enum
{
    GOV_POWERSAVE,
    GOV_BALANCED,
    GOV_PERFORMANCE

} state_t;

void powergov_loop(powergov_config_t *config)
{
    if (!config)
    {
        fprintf(stderr, "powergov_loop: NULL config\n");
        return;
    }

    state_t state = GOV_POWERSAVE;

    static int battery_available = -1;
    static int last_battery = -1;
    static int battery_tick = 0;

    for (;;)
    {
        double load = get_cpu_usage();
        int battery_limited = 0;

        if (config->battery_safe_enabled)
        {
            /* Battery? */
            if (battery_available == -1)
            {
                int b = get_battery_level();
                battery_available = (b >= 0) ? 1 : 0;
                last_battery = b;
            }

            /* If battery, refresh in N secs */
            if (battery_available)
            {
                if (battery_tick++ >= 5) // ~10 secs
                {
                    last_battery = get_battery_level();
                    battery_tick = 0;
                }

                if (last_battery >= 0 &&
                    last_battery <= config->battery_threshold)
                {
                    battery_limited = 1;
                }
            }
        }

        /* State machine */
        if (state == GOV_POWERSAVE && load > CPU_LOW)
        {
            set_governor("schedutil");
            state = GOV_BALANCED;
        }

        if (!battery_limited &&
            state == GOV_BALANCED &&
            load > CPU_HIGH)
        {
            set_governor("performance");
            state = GOV_PERFORMANCE;
        }

        if (battery_limited && state == GOV_PERFORMANCE)
        {
            set_governor("schedutil");
            state = GOV_BALANCED;
        }
        else if (state == GOV_PERFORMANCE && load < CPU_MEDIUM)
        {
            set_governor("schedutil");
            state = GOV_BALANCED;
        }
        else if (state == GOV_BALANCED && load < CPU_LOW)
        {
            set_governor("powersave");
            state = GOV_POWERSAVE;
        }

        sleep(2);
    }
}

