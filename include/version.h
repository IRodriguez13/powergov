#ifndef POWERGOV_VERSION_H
#define POWERGOV_VERSION_H

#include <stddef.h>
#include <stdio.h>

#ifndef POWERGOV_VERSION
#define POWERGOV_VERSION "unknown"
#endif

#define POWERGOV_PACKAGE_NAME     "powergov"
#define POWERGOV_SOURCE_URL       "https://github.com/IRodriguez13/powergov"
#define POWERGOV_COPYRIGHT_YEAR   "2026"
#define POWERGOV_AUTHOR_NAME      "Iván Ezequiel Rodriguez"

/* CLI: same layout as pack/extract -v (program (package) version + GPL block). */
void powergov_print_version(void);
void powergov_print_version_for(const char *program);

/* "program (powergov) X.Y.Z" into out; returns bytes written (excluding NUL). */
int powergov_version_title_line(char *out, size_t outsz, const char *program);

/* Returns 1 on success; writes major/minor/patch (patch 0 if omitted). */
int powergov_version_parse(const char *s, int *maj, int *min, int *pat);

/* -1 if a<b, 0 if equal, 1 if a>b (unparseable strings compare equal). */
int powergov_version_compare(const char *a, const char *b);

int powergov_version_newer(const char *a, const char *b);

/* Reads "x.y.z" from /usr/local/bin/powergov --version when socket has no version. */
int powergov_probe_installed_daemon_version(char *out, size_t outsz);

#endif
