#include "lid_state.h"
#include "../core/sysfs.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>

static int parse_lid_buf(const char *buf)
{
    if (!buf || !buf[0])
        return -1;

    if (strstr(buf, "closed"))
        return 1;
    if (strstr(buf, "open"))
        return 0;

    if (buf[0] == '0')
        return 1;
    if (buf[0] == '1')
        return 0;

    return -1;
}

int powergov_lid_poll(int *closed_out)
{
    DIR *dir;
    struct dirent *ent;
    char path[256];
    char buf[64];
    int state;

    if (!closed_out)
        return -1;

    *closed_out = -1;

    dir = opendir("/proc/acpi/button/lid");
    if (dir)
    {
        while ((ent = readdir(dir)) != NULL)
        {
            if (ent->d_name[0] == '.')
                continue;

            snprintf(path, sizeof(path),
                     "/proc/acpi/button/lid/%s/state", ent->d_name);
            if (sysfs_read_file(path, buf, sizeof(buf)) != 0)
                continue;

            state = parse_lid_buf(buf);
            if (state >= 0)
            {
                *closed_out = state;
                closedir(dir);
                return 0;
            }
        }
        closedir(dir);
    }

    dir = opendir("/sys/bus/acpi/devices");
    if (dir)
    {
        while ((ent = readdir(dir)) != NULL)
        {
            if (strncmp(ent->d_name, "LID", 3) != 0)
                continue;

            snprintf(path, sizeof(path),
                     "/sys/bus/acpi/devices/%s/state", ent->d_name);
            if (sysfs_read_file(path, buf, sizeof(buf)) != 0)
                continue;

            state = parse_lid_buf(buf);
            if (state >= 0)
            {
                *closed_out = state;
                closedir(dir);
                return 0;
            }
        }
        closedir(dir);
    }

    return -1;
}
