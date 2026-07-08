#include "session_idle.h"
#include <stdio.h>
#include <string.h>

static int parse_idle_line(const char *line)
{
    if (!line)
        return -1;

    if (strstr(line, "true") || strcmp(line, "yes") == 0)
        return 1;
    if (strstr(line, "false") || strcmp(line, "no") == 0)
        return 0;

    return -1;
}

int powergov_session_idle_poll(int *idle_out)
{
    FILE *f;
    char line[128];
    int idle;

    if (!idle_out)
        return -1;

    *idle_out = -1;

    f = popen("busctl get-property org.freedesktop.login1 "
              "/org/freedesktop/login1 org.freedesktop.login1.Manager "
              "IdleHint --value 2>/dev/null", "r");
    if (f)
    {
        if (fgets(line, sizeof(line), f))
        {
            idle = parse_idle_line(line);
            if (idle >= 0)
            {
                pclose(f);
                *idle_out = idle;
                return 0;
            }
        }
        pclose(f);
    }

    f = popen("loginctl show-session self -p IdleHint --value 2>/dev/null", "r");
    if (f)
    {
        if (fgets(line, sizeof(line), f))
        {
            idle = parse_idle_line(line);
            if (idle >= 0)
            {
                pclose(f);
                *idle_out = idle;
                return 0;
            }
        }
        pclose(f);
    }

    return -1;
}
