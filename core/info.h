#ifndef POWERGOV_INFO_H
#define POWERGOV_INFO_H

#include "../include/powergov/client.h"
#include "../include/powergov/types.h"

void powergov_info_fill_status(const powergov_config_t *cfg,
                               powergov_reply_status_t *out);
void powergov_info_fill_system(const powergov_config_t *cfg,
                               powergov_reply_system_t *out);
void powergov_info_fill_cpu(powergov_reply_cpu_t *out);
void powergov_info_fill_compat(const powergov_config_t *cfg,
                               powergov_reply_compat_t *out);
void powergov_info_fill_metrics(powergov_reply_metrics_t *out);
void powergov_info_fill_log(int lines, powergov_reply_log_t *out);
void powergov_info_fill_bundle(const powergov_config_t *cfg, unsigned int mask,
                               int log_lines, powergov_reply_bundle_t *out);

#endif
