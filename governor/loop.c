#include "loop.h"
#include "../cpu/cpu_load.h"
#include "governor.h"
#include <stdio.h>
#include <unistd.h>

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

        if(state == GOV_POWERSAVE && load > 0.35)
        {
            set_governor("schedutil");
            state = GOV_BALANCED;
        }

        if(state == GOV_BALANCED && load > 0.75)
        {
            set_governor("performance");
            state = GOV_PERFORMANCE;
        }

        else if(state == GOV_PERFORMANCE && load < 0.60)
        {
            set_governor("schedutil");
            state = GOV_BALANCED;
        }

        else if(state == GOV_BALANCED && load< 0.25)
        {
            set_governor("powersave");
            state = GOV_POWERSAVE;
        }

        sleep(2);
    }
}