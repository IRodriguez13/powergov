#include "governor/loop.h"
#include "Battery/battery.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

powergov_config_t config = {0};

/* Main CLI entrypoint with the on/off logic. No more, no less*/

void stop_powergov()
{
    int kill = system("pkill powergov");

    if(kill)
    {
        printf("Powergov stopped.\n");
    }
}

void start_powergov(powergov_config_t *config)
{
    setsid();
    powergov_loop(config);
    stop_powergov();
}


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: powergov [on/off]\n");
        return -1;
    }

    if (strcmp(argv[1], "on") == 0)
    {
        /* Call to start_powergov(); */
        start_powergov(&config);
    }

    else if (strcmp(argv[1], "off") == 0)
    {
        /* Call to stop_powergov(); */
        stop_powergov();
    }

    else if (strcmp(argv[1], "--battery-safe") == 0)
    {
        if (argc != 3)
        {
            fprintf(stderr, "Usage: --battery-safe <percent>\n");
            return 1;
        }

        int threshold = atoi(argv[2]);
        if (threshold <= 0 || threshold > 100)
        {
            fprintf(stderr, "Invalid battery threshold\n");
            return 1;
        }

        config.battery_safe_enabled = 1;
        config.battery_threshold = threshold;

        start_powergov(&config);
    }

    else if (strcmp(argv[1], "getbattery") == 0)
    {
        int b = get_battery_level();

        if (b >= 0)
            printf("%d\n", b);
    }

    else if (strcmp(argv[1], "--help") == 0)
    {
        printf(
            "Usage:\n"
            "  powergov on\n"
            "  powergov off\n"
            "  powergov --battery-safe <percent>\n"
            "  powergov getbattery\n\n"
            "Options:\n"
            "  --battery-safe N   Disable performance governor when battery <= N%%\n"
            "  Use 0 to disable battery safe mode\n");
    }

    else
    {
        printf("Invalid argument. Use --help.\n");
        return 1;
    }

    return 0;
}