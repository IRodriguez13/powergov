/*
 * version.c - Version output for powergov
 * Copyright (C) 2026 Iván Ezequiel Rodriguez
 * License: GPLv3+
 */
#include "version.h"
#include <stdio.h>

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
