#ifndef CONFIG_H
#define CONFIG_H

#include "../governor/loop.h"

#define POWERGOV_CONF_PATH "/etc/powergov.conf"

int powergov_config_load(powergov_config_t *config);
int powergov_config_save(const powergov_config_t *config);

#endif
