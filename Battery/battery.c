#include "battery.h"
#include <stdio.h>
#include <stdlib.h>

int get_battery_level(void)
{
    FILE *f = fopen("/sys/class/power_supply/BAT0/capacity", "r");

    if (!f)
        return -1;

    int capacity;

    fscanf(f, "%d", &capacity);
    
    if (fscanf(f, "%d", &capacity) != 1)
    {
        fclose(f);
        return -1;
    }

    fclose(f);

    printf("Battery capacity: %d%%\n", capacity);
    return capacity;
}
