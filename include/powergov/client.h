#ifndef POWERGOV_CLIENT_H
#define POWERGOV_CLIENT_H

#include "types.h"

#define POWERGOV_SOCK_STR       64
#define POWERGOV_SOCK_STR_LONG  128
#define POWERGOV_SOCK_LOG_SZ   8192
#define POWERGOV_SOCK_METRICS_SZ 4096

typedef struct
{
    int daemon_running;
    int user_mode;
    int battery_pct;
    int battery_safe_enabled;
    int battery_threshold;
    int features_mask;
    int power_source;
    int turbo_on;
    double cpu_load;
    char governor[POWERGOV_SOCK_STR];
    char epp[POWERGOV_SOCK_STR];
    char battery_status[POWERGOV_SOCK_STR];
} powergov_reply_status_t;

typedef struct
{
    char pretty_name[POWERGOV_SOCK_STR_LONG];
    char kernel[POWERGOV_SOCK_STR];
    char powergov_version[POWERGOV_SOCK_STR];
    int systemd_active;
    int ppd_detected;
    int tlp_detected;
    int lid_state;
    int session_idle;
} powergov_reply_system_t;

typedef struct
{
    char model[POWERGOV_SOCK_STR_LONG];
    int cpu_count;
    char scaling_driver[POWERGOV_SOCK_STR];
    char governor[POWERGOV_SOCK_STR];
    char governors_avail[POWERGOV_SOCK_STR_LONG];
    char epp[POWERGOV_SOCK_STR];
    int epp_available;
    int turbo_on;
    char freq_hw_max[POWERGOV_SOCK_STR];
    char freq_scaling_max[POWERGOV_SOCK_STR];
    char platform_profile[POWERGOV_SOCK_STR];
    int rapl_available;
} powergov_reply_cpu_t;

typedef enum
{
    POWERGOV_COMPAT_UNSUPPORTED = 0,
    POWERGOV_COMPAT_SUPPORTED,
    POWERGOV_COMPAT_PARTIAL,
    POWERGOV_COMPAT_CONFLICT
} powergov_compat_state_t;

typedef struct
{
    char name[POWERGOV_SOCK_STR];
    int state;
    int hw_available;
    int enabled;
    char detail[POWERGOV_SOCK_STR_LONG];
} powergov_reply_compat_row_t;

typedef struct
{
    int adaptability_score;
    char summary[POWERGOV_SOCK_STR_LONG];
    char scaling_driver[POWERGOV_SOCK_STR];
    powergov_reply_compat_row_t rows[POWERGOV_FEATURE_COUNT];
} powergov_reply_compat_t;

typedef struct
{
    char text[POWERGOV_SOCK_METRICS_SZ];
} powergov_reply_metrics_t;

typedef struct
{
    int ok;
    char text[POWERGOV_SOCK_LOG_SZ];
} powergov_reply_log_t;

#define POWERGOV_BUNDLE_STATUS   (1u << 0)
#define POWERGOV_BUNDLE_SYSTEM   (1u << 1)
#define POWERGOV_BUNDLE_CPU      (1u << 2)
#define POWERGOV_BUNDLE_COMPAT   (1u << 3)
#define POWERGOV_BUNDLE_METRICS  (1u << 4)
#define POWERGOV_BUNDLE_LOG      (1u << 5)
#define POWERGOV_BUNDLE_DEV_ALL  (POWERGOV_BUNDLE_STATUS | POWERGOV_BUNDLE_SYSTEM | \
                                  POWERGOV_BUNDLE_CPU | POWERGOV_BUNDLE_COMPAT | \
                                  POWERGOV_BUNDLE_METRICS | POWERGOV_BUNDLE_LOG)

typedef struct
{
    unsigned int mask;
    powergov_reply_status_t status;
    powergov_reply_system_t system;
    powergov_reply_cpu_t cpu;
    powergov_reply_compat_t compat;
    powergov_reply_metrics_t metrics;
    powergov_reply_log_t log;
} powergov_reply_bundle_t;

int powergov_client_ping(void);

int powergov_client_set_user_mode(powergov_user_mode_t mode);
int powergov_client_set_battery_threshold(int threshold);
int powergov_client_set_feature(powergov_feature_id_t id, int enabled);
int powergov_client_set_tuning(powergov_tuning_id_t id, int value);
int powergov_client_query_tuning(powergov_reply_tuning_t *out);

int powergov_client_query_status(powergov_reply_status_t *out);
int powergov_client_query_system(powergov_reply_system_t *out);
int powergov_client_query_cpu(powergov_reply_cpu_t *out);
int powergov_client_query_compat(powergov_reply_compat_t *out);
int powergov_client_query_metrics(powergov_reply_metrics_t *out);
int powergov_client_query_log(int lines, powergov_reply_log_t *out);
int powergov_client_query_bundle(unsigned int mask, int log_lines,
                                 powergov_reply_bundle_t *out);

const char *powergov_user_mode_title(powergov_user_mode_t mode);
const char *powergov_user_mode_subtitle(powergov_user_mode_t mode);
const char *powergov_compat_state_str(int state);

#endif
