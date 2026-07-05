/*
 * version.c - Version output for powergov
 * Copyright (C) 2026 Iván Ezequiel Rodriguez
 * License: GPLv3+
 */
#include "version.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int powergov_version_parse(const char *s, int *maj, int *min, int *pat)
{
    const char *p = s;
    int a = 0;
    int b = 0;
    int c = 0;

    if (!s || !maj || !min || !pat)
        return 0;

    while (*p == 'v' || *p == 'V' || *p == ' ')
        p++;

    if (sscanf(p, "%d.%d.%d", &a, &b, &c) < 2)
        return 0;

    *maj = a;
    *min = b;
    *pat = c;
    return 1;
}

int powergov_version_compare(const char *a, const char *b)
{
    int ama, ami, apa;
    int bma, bmi, bpa;

    if (!powergov_version_parse(a, &ama, &ami, &apa))
        return 0;
    if (!powergov_version_parse(b, &bma, &bmi, &bpa))
        return 0;

    if (ama != bma)
        return (ama > bma) ? 1 : -1;
    if (ami != bmi)
        return (ami > bmi) ? 1 : -1;
    if (apa != bpa)
        return (apa > bpa) ? 1 : -1;
    return 0;
}

int powergov_version_newer(const char *a, const char *b)
{
    return powergov_version_compare(a, b) > 0;
}

int powergov_probe_installed_daemon_version(char *out, size_t outsz)
{
    FILE *f;
    char line[128];
    const char *bin = "/usr/local/bin/powergov";
    char cmd[160];

    if (!out || outsz == 0)
        return 0;

    out[0] = '\0';
    snprintf(cmd, sizeof(cmd), "%s --version 2>/dev/null", bin);
    f = popen(cmd, "r");
    if (!f)
        return 0;

    if (!fgets(line, sizeof(line), f))
    {
        pclose(f);
        return 0;
    }
    pclose(f);

    {
        const char *p = strstr(line, ") ");
        if (p)
            p += 2;
        else
            p = line;

        while (*p == 'v' || *p == 'V' || *p == ' ')
            p++;

        snprintf(out, outsz, "%s", p);
        {
            size_t n = strlen(out);
            while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r'))
                out[--n] = '\0';
        }
    }

    return out[0] != '\0';
}

void powergov_print_version(void)
{
    printf(
        "powergov (powergov) %s\n"
        "Copyright (C) 2026 Iván Ezequiel Rodriguez\n"
        "License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>.\n"
        "This is free software: you are free to change and redistribute it.\n"
        "There is NO WARRANTY, to the extent permitted by law.\n"
        "\n"
        "Source: %s\n"
        "\n"
        "Escrito por Iván Ezequiel Rodriguez.\n",
        POWERGOV_VERSION,
        POWERGOV_SOURCE_URL);
}
