#ifndef CONFIG_H
#define CONFIG_H

#include "../include/powergov/types.h"

int powergov_config_load(powergov_config_t *config);
int powergov_config_save(const powergov_config_t *config);
int powergov_feature_parse_name(const char *name, powergov_feature_id_t *out);

#endif
