#include "log.h"
#include "../include/powergov/types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#include <signal.h>

static powergov_log_level_t g_level = POWERGOV_LOG_INFO;
static int g_enabled = 1;
static FILE *g_fp;
static volatile sig_atomic_t g_follow_stop = 0;

static void on_follow_signal(int sig)
{
    (void)sig;
    g_follow_stop = 1;
}

void powergov_log_shutdown(void)
{
    if (g_fp)
    {
        fclose(g_fp);
        g_fp = NULL;
    }
}

static const char *level_tag(powergov_log_level_t level)
{
    switch (level)
    {
    case POWERGOV_LOG_ERROR: return "ERROR";
    case POWERGOV_LOG_WARN:  return "WARN";
    case POWERGOV_LOG_INFO:  return "INFO";
    case POWERGOV_LOG_DEBUG: return "DEBUG";
    default:                 return "?";
    }
}

static void ensure_log_dir(void)
{
    mkdir("/var/log/powergov", 0755);
}

void powergov_log_init(const powergov_config_t *cfg)
{
    if (cfg)
    {
        g_level = cfg->log_level;
        g_enabled = cfg->dev_log_enabled;
    }

    if (!g_enabled)
        return;

    ensure_log_dir();
    if (g_fp)
        return;

    g_fp = fopen(POWERGOV_LOG_PATH, "a");
}

void powergov_log_set_level(powergov_log_level_t level)
{
    g_level = level;
}

void powergov_log_set_enabled(int enabled)
{
    g_enabled = enabled;
    if (!enabled && g_fp)
    {
        fclose(g_fp);
        g_fp = NULL;
    }
}

void powergov_log_write(powergov_log_level_t level, const char *module,
                        const char *fmt, ...)
{
    char msg[512];
    char line[640];
    time_t now;
    struct tm tm;
    va_list ap;

    if (!g_enabled || level > g_level || !fmt)
        return;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    now = time(NULL);
    localtime_r(&now, &tm);
    snprintf(line, sizeof(line),
             "%04d-%02d-%02d %02d:%02d:%02d [%s] %s: %s\n",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec,
             level_tag(level), module ? module : "-", msg);

    if (!g_fp)
    {
        ensure_log_dir();
        g_fp = fopen(POWERGOV_LOG_PATH, "a");
    }

    if (g_fp)
    {
        fputs(line, g_fp);
        fflush(g_fp);
    }
}

int powergov_log_tail(int lines, int to_stdout)
{
    FILE *f;
    char **ring;
    int cap;
    int count;
    int i;
    char buf[640];

    if (lines <= 0)
        lines = 50;

    f = fopen(POWERGOV_LOG_PATH, "r");
    if (!f)
    {
        fprintf(stderr, "powergov: no log at %s\n", POWERGOV_LOG_PATH);
        return -1;
    }

    cap = lines;
    ring = calloc((size_t)cap, sizeof(char *));
    if (!ring)
    {
        fclose(f);
        return -1;
    }

    count = 0;
    while (fgets(buf, sizeof(buf), f))
    {
        size_t idx = (size_t)(count % cap);
        free(ring[idx]);
        ring[idx] = strdup(buf);
        count++;
    }
    fclose(f);

    if (count == 0)
    {
        free(ring);
        printf("(empty log)\n");
        return 0;
    }

    i = (count > cap) ? (count - cap) : 0;
    for (; i < count; i++)
    {
        size_t idx = (size_t)(i % cap);
        if (ring[idx])
        {
            if (to_stdout)
                fputs(ring[idx], stdout);
        }
    }

    for (i = 0; i < cap; i++)
        free(ring[i]);
    free(ring);
    return 0;
}

int powergov_log_follow(int tail_lines)
{
    FILE *f;
    char buf[640];
    long pos;
    struct stat st;

    if (tail_lines > 0)
    {
        if (powergov_log_tail(tail_lines, 1) != 0)
            return -1;
    }

    f = fopen(POWERGOV_LOG_PATH, "r");
    if (!f)
    {
        fprintf(stderr, "powergov: no log at %s\n", POWERGOV_LOG_PATH);
        return -1;
    }

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return -1;
    }

    pos = ftell(f);

    signal(SIGINT, on_follow_signal);
    signal(SIGTERM, on_follow_signal);
    g_follow_stop = 0;

    while (!g_follow_stop)
    {
        if (stat(POWERGOV_LOG_PATH, &st) != 0)
        {
            usleep(250000);
            continue;
        }

        if (st.st_size < pos)
        {
            /* log rotated or truncated */
            fclose(f);
            f = fopen(POWERGOV_LOG_PATH, "r");
            if (!f)
                return -1;
            pos = 0;
        }

        if (fseek(f, pos, SEEK_SET) != 0)
        {
            usleep(250000);
            continue;
        }

        while (fgets(buf, sizeof(buf), f))
        {
            fputs(buf, stdout);
            fflush(stdout);
            pos = ftell(f);
        }

        clearerr(f);
        usleep(250000);
    }

    fclose(f);
    return 0;
}
