#include "config.h"
#include <stdio.h>
#include <string.h>

int powergov_config_load(powergov_config_t *config)
{
    FILE *f;
    char line[256];
    int threshold = 0;
    int found = 0;

    if (!config)
        return -1;

    f = fopen(POWERGOV_CONF_PATH, "r");
    if (!f)
        return 0;

    while (fgets(line, sizeof(line), f))
    {
        if (line[0] == '#' || line[0] == '\n')
            continue;

        if (sscanf(line, "BATTERY_SAFE_THRESHOLD=%d", &threshold) == 1)
        {
            found = 1;
        }
    }

    fclose(f);

    if (found)
    {
        if (threshold <= 0)
        {
            config->battery_safe_enabled = 0;
            config->battery_threshold = 0;
        }
        else if (threshold <= 100)
        {
            config->battery_safe_enabled = 1;
            config->battery_threshold = threshold;
        }
    }

    return 0;
}

int powergov_config_save(const powergov_config_t *config)
{
    FILE *f;
    int threshold;

    if (!config)
        return -1;

    threshold = config->battery_safe_enabled ? config->battery_threshold : 0;

    f = fopen(POWERGOV_CONF_PATH, "w");
    if (!f)
        return -1;

    fprintf(f, "# powergov persistent configuration\n");
    fprintf(f, "BATTERY_SAFE_THRESHOLD=%d\n", threshold);

    if (fclose(f) != 0)
        return -1;

    return 0;
}
