#include "governor.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#define CPU_GOV_PATH "/sys/devices/system/cpu"

int set_governor(const char *gov)
{
    DIR *d = opendir(CPU_GOV_PATH);
    if (!d) return -1;

    struct dirent *entry;
    char path[256];

    while ((entry = readdir(d)) != NULL)
    {
        if (strncmp(entry->d_name, "cpu", 3) == 0 &&
            entry->d_name[3] >= '0' && entry->d_name[3] <= '9')
        {
            snprintf(path, sizeof(path),
                     CPU_GOV_PATH "/%s/cpufreq/scaling_governor",
                     entry->d_name);

            FILE *fp = fopen(path, "w");
            if (fp)
            {
                fprintf(fp, "%s\n", gov);
                fclose(fp);
            }
        }
    }

    closedir(d);
}