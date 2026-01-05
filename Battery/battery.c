#include "battery.h"
#include <stdio.h>
#include <stdlib.h>

int get_battery_level(void)
{
    FILE *f = fopen("/sys/class/power_supply/BAT0/capacity", "r");

    if(!f)
    {
        perror("fopen");
        return -1;
    }

    int capacity;

    fscanf(f, "%d", &capacity);
    fclose(f);

    printf("Battery capacity: %d%%\n", capacity);
    return 0;
}

