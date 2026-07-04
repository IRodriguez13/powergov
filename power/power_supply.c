#include "power_supply.h"
#include "../core/sysfs.h"
#include "../log/log.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>

static char g_battery_name[64];
static int g_battery_found;

static powergov_power_source_t parse_status(const char *status)
{
    if (!status)
        return POWERGOV_POWER_UNKNOWN;

    if (strcmp(status, "Discharging") == 0)
        return POWERGOV_POWER_BATTERY;
    if (strcmp(status, "Charging") == 0 || strcmp(status, "Full") == 0 ||
        strcmp(status, "Not charging") == 0)
        return POWERGOV_POWER_AC;

    return POWERGOV_POWER_UNKNOWN;
}

int powergov_power_supply_detect(void)
{
    DIR *dir;
    struct dirent *ent;
    char path[256];
    char type[32];

    g_battery_found = 0;
    g_battery_name[0] = '\0';

    dir = opendir("/sys/class/power_supply");
    if (!dir)
        return 0;

    while ((ent = readdir(dir)) != NULL)
    {
        if (ent->d_name[0] == '.')
            continue;

        snprintf(path, sizeof(path),
                 "/sys/class/power_supply/%s/type", ent->d_name);

        if (sysfs_read_file(path, type, sizeof(type)) != 0)
            continue;

        if (strcmp(type, "Battery") == 0)
        {
            snprintf(g_battery_name, sizeof(g_battery_name), "%s", ent->d_name);
            g_battery_found = 1;
            break;
        }
    }

    closedir(dir);
    PG_LOG_I("power", "battery detect: %s (%s)",
             g_battery_found ? "found" : "none",
             g_battery_found ? g_battery_name : "-");
    return g_battery_found;
}

int powergov_power_supply_poll(powergov_power_info_t *info)
{
    char path[256];
    char status[32];
    long cap;

    if (!info)
        return -1;

    memset(info, 0, sizeof(*info));
    info->source = POWERGOV_POWER_UNKNOWN;
    info->capacity_pct = -1;

    if (!g_battery_found && !powergov_power_supply_detect())
        return -1;

    info->present = 1;
    snprintf(info->name, sizeof(info->name), "%s", g_battery_name);

    snprintf(path, sizeof(path),
             "/sys/class/power_supply/%s/capacity", g_battery_name);
    if (sysfs_read_int(path, &cap) == 0)
        info->capacity_pct = (int)cap;

    snprintf(path, sizeof(path),
             "/sys/class/power_supply/%s/status", g_battery_name);
    if (sysfs_read_file(path, status, sizeof(status)) == 0)
        info->source = parse_status(status);

    return 0;
}

int get_battery_level(void)
{
    powergov_power_info_t info;

    if (powergov_power_supply_poll(&info) != 0)
        return -1;
    return info.capacity_pct;
}
