#ifndef POWERGOV_LOG_H
#define POWERGOV_LOG_H

#include "../include/powergov/types.h"

void powergov_log_init(const powergov_config_t *cfg);
void powergov_log_set_level(powergov_log_level_t level);
void powergov_log_set_enabled(int enabled);

void powergov_log_shutdown(void);

void powergov_log_write(powergov_log_level_t level, const char *module,
                        const char *fmt, ...);

#define PG_LOG_E(mod, ...) powergov_log_write(POWERGOV_LOG_ERROR, mod, __VA_ARGS__)
#define PG_LOG_W(mod, ...) powergov_log_write(POWERGOV_LOG_WARN,  mod, __VA_ARGS__)
#define PG_LOG_I(mod, ...) powergov_log_write(POWERGOV_LOG_INFO,  mod, __VA_ARGS__)
#define PG_LOG_D(mod, ...) powergov_log_write(POWERGOV_LOG_DEBUG, mod, __VA_ARGS__)

int powergov_log_tail(int lines, int to_stdout);
int powergov_log_follow(int tail_lines);

#endif
