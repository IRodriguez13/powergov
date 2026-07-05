#ifndef POWERGOV_VERSION_H
#define POWERGOV_VERSION_H

#include <stddef.h>

#ifndef POWERGOV_VERSION
#define POWERGOV_VERSION "unknown"
#endif

#define POWERGOV_SOURCE_URL "https://github.com/IRodriguez13/powergov"

void powergov_print_version(void);

/* Returns 1 on success; writes major/minor/patch (patch 0 if omitted). */
int powergov_version_parse(const char *s, int *maj, int *min, int *pat);

/* -1 if a<b, 0 if equal, 1 if a>b (unparseable strings compare equal). */
int powergov_version_compare(const char *a, const char *b);

int powergov_version_newer(const char *a, const char *b);

/* Reads "x.y.z" from /usr/local/bin/powergov --version when socket has no version. */
int powergov_probe_installed_daemon_version(char *out, size_t outsz);

#endif
