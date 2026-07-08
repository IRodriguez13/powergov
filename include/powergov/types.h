#ifndef POWERGOV_TYPES_H
#define POWERGOV_TYPES_H

#include <stdint.h>

#define POWERGOV_SOCKET_MAGIC       0x50474F56
#define POWERGOV_CONF_PATH          "/etc/powergov.conf"
#define POWERGOV_LOG_PATH           "/var/log/powergov/powergov.log"
#define POWERGOV_METRICS_PATH       "/run/powergov/metrics"
#define POWERGOV_PIDFILE_PATH       "/run/powergov/powergov.pid"
#define POWERGOV_SOCKET_PATH        "/run/powergov/powergov.sock"

#define POWERGOV_TICKS_PER_SEC      2
#define POWERGOV_HYSTERESIS_SAMPLES 3
#define POWERGOV_BATTERY_REFRESH    5
#define POWERGOV_BATTERY_REFRESH_AC 30

#define POWERGOV_THRESHOLD_LOW_DEF    0.25
#define POWERGOV_THRESHOLD_MID_DEF  0.60
#define POWERGOV_THRESHOLD_HIGH_DEF 0.75
#define POWERGOV_FREQ_CAP_BATTERY_DEF 80
#define POWERGOV_LOW_BATTERY_DEF    15
#define POWERGOV_CONSERVE_BATTERY_PCT 30
#define POWERGOV_LOW_BATTERY_FREQ_CAP 70

typedef enum
{
    POWERGOV_USER_MAX_BATTERY = 0,
    POWERGOV_USER_BALANCED    = 1,
    POWERGOV_USER_PERFORMANCE = 2,
    POWERGOV_USER_CUSTOM      = 3
} powergov_user_mode_t;

typedef enum
{
    POWERGOV_POWER_UNKNOWN = 0,
    POWERGOV_POWER_AC,
    POWERGOV_POWER_BATTERY
} powergov_power_source_t;

typedef enum
{
    POWERGOV_GOV_POWERSAVE = 0,
    POWERGOV_GOV_BALANCED,
    POWERGOV_GOV_PERFORMANCE
} powergov_gov_state_t;

typedef enum
{
    POWERGOV_LOG_OFF   = 0,
    POWERGOV_LOG_ERROR = 1,
    POWERGOV_LOG_WARN  = 2,
    POWERGOV_LOG_INFO  = 3,
    POWERGOV_LOG_DEBUG = 4
} powergov_log_level_t;

typedef struct
{
    unsigned cpu_governor    : 1;
    unsigned cpu_epp         : 1;
    unsigned cpu_freq_cap    : 1;
    unsigned cpu_turbo       : 1;
    unsigned platform_profile : 1;
    unsigned runtime_pm      : 1;
    unsigned peripheral_pm   : 1;
    unsigned disk_pm         : 1;
    unsigned pcie_aspm       : 1;
    unsigned bluetooth_pm    : 1;
} powergov_features_t;

typedef struct
{
    unsigned wifi  : 1;
    unsigned sata  : 1;
    unsigned audio : 1;
} powergov_peripheral_opts_t;

typedef struct
{
    int battery_safe_enabled;
    int battery_threshold;
    powergov_user_mode_t user_mode;
    powergov_features_t features;
    powergov_log_level_t log_level;
    int dev_log_enabled;
    double threshold_low;
    double threshold_mid;
    double threshold_high;
    int freq_cap_battery_pct;
    int low_battery_pct;
    powergov_peripheral_opts_t peripheral;
    int custom_allow_performance;
    int custom_runtime_aggressive;
    int lid_aggressive;
    int display_aggressive;
    int context_require_low_load;
    int context_low_load_pct;
} powergov_config_t;

typedef enum
{
    POWERGOV_SOCKET_CMD_SET_BATTERY_THRESHOLD = 1,
    POWERGOV_SOCKET_CMD_QUERY_BATTERY_CONFIG  = 2,
    POWERGOV_SOCKET_CMD_SET_USER_MODE         = 3,
    POWERGOV_SOCKET_CMD_SET_FEATURE           = 4,
    POWERGOV_SOCKET_CMD_QUERY_FULL_CONFIG     = 5,
    POWERGOV_SOCKET_CMD_QUERY_STATUS          = 6,
    POWERGOV_SOCKET_CMD_QUERY_SYSTEM          = 7,
    POWERGOV_SOCKET_CMD_QUERY_CPU             = 8,
    POWERGOV_SOCKET_CMD_QUERY_COMPAT          = 9,
    POWERGOV_SOCKET_CMD_QUERY_METRICS          = 10,
    POWERGOV_SOCKET_CMD_QUERY_LOG             = 11,
    POWERGOV_SOCKET_CMD_QUERY_BUNDLE          = 12,
    POWERGOV_SOCKET_CMD_SET_TUNING            = 13,
    POWERGOV_SOCKET_CMD_QUERY_TUNING          = 14
} powergov_socket_cmd_t;

typedef enum
{
    POWERGOV_TUNING_THRESHOLD_LOW = 0,
    POWERGOV_TUNING_THRESHOLD_MID,
    POWERGOV_TUNING_THRESHOLD_HIGH,
    POWERGOV_TUNING_FREQ_CAP_BATTERY,
    POWERGOV_TUNING_LOW_BATTERY,
    POWERGOV_TUNING_PERIPHERAL_WIFI,
    POWERGOV_TUNING_PERIPHERAL_SATA,
    POWERGOV_TUNING_PERIPHERAL_AUDIO,
    POWERGOV_TUNING_CUSTOM_ALLOW_PERF,
    POWERGOV_TUNING_CUSTOM_RUNTIME_PM,
    POWERGOV_TUNING_LID_AGGRESSIVE,
    POWERGOV_TUNING_DISPLAY_AGGRESSIVE
} powergov_tuning_id_t;

typedef enum
{
    POWERGOV_FEATURE_GOVERNOR = 0,
    POWERGOV_FEATURE_EPP,
    POWERGOV_FEATURE_FREQ_CAP,
    POWERGOV_FEATURE_TURBO,
    POWERGOV_FEATURE_PLATFORM,
    POWERGOV_FEATURE_RUNTIME_PM,
    POWERGOV_FEATURE_PERIPHERAL_PM,
    POWERGOV_FEATURE_DISK_PM,
    POWERGOV_FEATURE_PCIE_ASPM,
    POWERGOV_FEATURE_BLUETOOTH_PM,
    POWERGOV_FEATURE_COUNT
} powergov_feature_id_t;

typedef struct
{
    int magic;
    int cmd;
    int value;
    int value2;
} powergov_socket_msg_t;

typedef struct
{
    int battery_safe_enabled;
    int battery_threshold;
    int user_mode;
    int features_mask;
    int log_level;
} powergov_socket_status_t;

typedef struct
{
    int threshold_low_pct;
    int threshold_mid_pct;
    int threshold_high_pct;
    int freq_cap_battery_pct;
    int low_battery_pct;
    int peripheral_wifi;
    int peripheral_sata;
    int peripheral_audio;
    int custom_allow_performance;
    int custom_runtime_aggressive;
    int lid_aggressive;
    int display_aggressive;
} powergov_reply_tuning_t;

typedef struct
{
    const char *governor;
    const char *epp;
    int turbo_on;
    int freq_cap_pct;
    const char *platform_profile;
    int runtime_pm_aggressive;
    int peripheral_pm_level;
    int device_aggression;
    int allow_performance;
} powergov_effective_policy_t;

const char *powergov_user_mode_str(powergov_user_mode_t mode);
const char *powergov_gov_state_str(powergov_gov_state_t state);
const char *powergov_power_source_str(powergov_power_source_t src);
powergov_user_mode_t powergov_user_mode_parse(const char *s);
void powergov_config_set_defaults(powergov_config_t *cfg);
int powergov_features_to_mask(const powergov_features_t *f);
void powergov_features_from_mask(powergov_features_t *f, int mask);

const char *powergov_feature_name(powergov_feature_id_t id);

#endif
