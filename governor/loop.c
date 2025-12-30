#include "loop.h"
#include "../cpu/cpu_load.h"
#include "governor.h"
#include <stdio.h>
#include <unistd.h>

#define CPU_LOW 0.25
#define CPU_MEDIUM 0.60
#define CPU_HIGH 0.75

typedef enum
{
    GOV_POWERSAVE,
    GOV_BALANCED,
    GOV_PERFORMANCE

} state_t;

void powergov_loop(void)
{
    state_t state = GOV_POWERSAVE;

    for(;;)
    {
        double load = get_cpu_usage();

        if(state == GOV_POWERSAVE && load > CPU_LOW)
        {
            set_governor("schedutil");
            state = GOV_BALANCED;
        }

        if(state == GOV_BALANCED && load > CPU_HIGH)
        {
            set_governor("performance");
            state = GOV_PERFORMANCE;
        }

        else if(state == GOV_PERFORMANCE && load < CPU_MEDIUM)
        {
            set_governor("schedutil");
            state = GOV_BALANCED;
        }

        else if(state == GOV_BALANCED && load < CPU_LOW)
        {
            set_governor("powersave");
            state = GOV_POWERSAVE;
        }

        sleep(2);
    }
}