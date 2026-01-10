#include "battery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

int detect_battery(void)
{
    DIR *dir = opendir("/sys/class/power_supply");
    if (!dir)
        return 0;

    struct dirent *ent;
    char path[256];
    char type[32];

    while ((ent = readdir(dir)) != NULL)
    {
        if (ent->d_name[0] == '.')
            continue;

        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/type", ent->d_name);

        FILE *f = fopen(path, "r");
        if (!f)
            continue;

        if (fgets(type, sizeof(type), f))
        {
            type[strcspn(type, "\n")] = 0;

            if (strcmp(type, "Battery") == 0)
            {
                fclose(f);
                closedir(dir);
                return 1;
            }
        }

        fclose(f);
    }

    closedir(dir);
    return 0;
}

int get_battery_level(void)
{
    DIR *dir = opendir("/sys/class/power_supply");
    if (!dir)
        return -1;

    struct dirent *ent;
    char path[256];

    while ((ent = readdir(dir)) != NULL)
    {
        if (ent->d_name[0] == '.')
            continue;

        snprintf(path, sizeof(path),
                 "/sys/class/power_supply/%s/type",
                 ent->d_name);

        FILE *ft = fopen(path, "r");
        if (!ft)
            continue;

        char type[32];
        fgets(type, sizeof(type), ft);
        fclose(ft);

        if (strncmp(type, "Battery", 7) != 0)
            continue;

        snprintf(path, sizeof(path),
                 "/sys/class/power_supply/%s/capacity",
                 ent->d_name);

        FILE *fc = fopen(path, "r");
        if (!fc)
            continue;

        int cap;
        if (fscanf(fc, "%d", &cap) == 1)
        {
            fclose(fc);
            closedir(dir);
            return cap;
        }

        fclose(fc);
    }

    closedir(dir);
    return -1;
}

