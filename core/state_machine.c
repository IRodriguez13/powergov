#include "state_machine.h"
#include "../metrics/metrics.h"

void powergov_state_machine_init(powergov_state_machine_t *sm,
                                 powergov_gov_state_t initial)
{
    if (!sm)
        return;
    sm->state = initial;
    sm->up_streak = 0;
    sm->down_streak = 0;
}

static powergov_gov_state_t transition(powergov_state_machine_t *sm,
                                       powergov_gov_state_t next)
{
    if (sm->state != next)
    {
        powergov_metrics_state_change();
        sm->state = next;
    }
    return sm->state;
}

powergov_gov_state_t powergov_state_machine_step(
    powergov_state_machine_t *sm,
    const powergov_config_t *cfg,
    double load,
    int battery_limited,
    int allow_performance)
{
    double low;
    double mid;
    double high;

    if (!sm || !cfg)
        return POWERGOV_GOV_POWERSAVE;

    low = cfg->threshold_low;
    mid = cfg->threshold_mid;
    high = cfg->threshold_high;

    switch (sm->state)
    {
    case POWERGOV_GOV_POWERSAVE:
        if (load > low)
        {
            sm->up_streak++;
            sm->down_streak = 0;
            if (sm->up_streak >= POWERGOV_HYSTERESIS_SAMPLES)
            {
                sm->up_streak = 0;
                return transition(sm, POWERGOV_GOV_BALANCED);
            }
        }
        else
        {
            sm->up_streak = 0;
        }
        break;

    case POWERGOV_GOV_BALANCED:
        if (!battery_limited && allow_performance && load > high)
        {
            sm->up_streak++;
            sm->down_streak = 0;
            if (sm->up_streak >= POWERGOV_HYSTERESIS_SAMPLES)
            {
                sm->up_streak = 0;
                return transition(sm, POWERGOV_GOV_PERFORMANCE);
            }
        }
        else if (load < low)
        {
            sm->down_streak++;
            sm->up_streak = 0;
            if (sm->down_streak >= POWERGOV_HYSTERESIS_SAMPLES)
            {
                sm->down_streak = 0;
                return transition(sm, POWERGOV_GOV_POWERSAVE);
            }
        }
        else
        {
            sm->up_streak = 0;
            sm->down_streak = 0;
        }
        break;

    case POWERGOV_GOV_PERFORMANCE:
        if (battery_limited || !allow_performance)
        {
            sm->down_streak = POWERGOV_HYSTERESIS_SAMPLES;
        }

        if (load < mid || battery_limited || !allow_performance)
        {
            sm->down_streak++;
            sm->up_streak = 0;
            if (sm->down_streak >= POWERGOV_HYSTERESIS_SAMPLES)
            {
                sm->down_streak = 0;
                return transition(sm, POWERGOV_GOV_BALANCED);
            }
        }
        else
        {
            sm->down_streak = 0;
        }
        break;

    default:
        sm->state = POWERGOV_GOV_POWERSAVE;
        break;
    }

    return sm->state;
}
