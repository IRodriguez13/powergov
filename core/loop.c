#include "loop.h"
#include "state_machine.h"
#include "../power/power_supply.h"
#include "../power/profile.h"
#include "../cpu/cpu_load.h"
#include "../cpu/policy.h"
#include "../platform/platform_profile.h"
#include "../devices/runtime_pm.h"
#include "../log/log.h"
#include "../metrics/metrics.h"
#include "../cpu/governor.h"
#include "../config/config.h"
#include "info.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

static volatile sig_atomic_t powergov_shutdown = 0;
static volatile sig_atomic_t g_force_reapply = 0;

void powergov_request_shutdown(void)
{
    powergov_shutdown = 1;
}

int setup_socket_server(void)
{
    int sockfd;
    struct sockaddr_un server_addr;

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0)
        return -1;

    unlink(POWERGOV_SOCKET_PATH);
    mkdir("/run/powergov", 0755);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, POWERGOV_SOCKET_PATH,
            sizeof(server_addr.sun_path) - 1);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        close(sockfd);
        return -1;
    }

    chmod(server_addr.sun_path, 0666);

    if (listen(sockfd, 5) < 0)
    {
        close(sockfd);
        return -1;
    }

    return sockfd;
}

static void apply_full_policy(const powergov_config_t *cfg,
                              const powergov_effective_policy_t *policy)
{
    cpu_policy_apply(cfg, policy);

    if (cfg->features.platform_profile && policy->platform_profile)
        platform_profile_apply(policy->platform_profile);

    if (cfg->features.runtime_pm)
        runtime_pm_apply_aggressive(policy->runtime_pm_aggressive);
    else
        runtime_pm_restore();
}

static int socket_write_full(int fd, const void *buf, size_t len)
{
    size_t off = 0;
    const unsigned char *p = buf;

    while (off < len)
    {
        ssize_t n = write(fd, p + off, len - off);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return (errno == EPIPE) ? -EPIPE : -1;
        }
        off += (size_t)n;
    }
    return 0;
}

static int policy_changed(const powergov_effective_policy_t *prev,
                          const powergov_effective_policy_t *cur,
                          int features_mask, int prev_features_mask)
{
    if (features_mask != prev_features_mask)
        return 1;
    if (!prev || !cur)
        return 1;
    if (strcmp(prev->governor, cur->governor) != 0)
        return 1;
    if (strcmp(prev->epp, cur->epp) != 0)
        return 1;
    if (prev->turbo_on != cur->turbo_on)
        return 1;
    if (prev->freq_cap_pct != cur->freq_cap_pct)
        return 1;
    if (prev->runtime_pm_aggressive != cur->runtime_pm_aggressive)
        return 1;
    if (prev->platform_profile != cur->platform_profile)
        return 1;
    if (prev->platform_profile && cur->platform_profile &&
        strcmp(prev->platform_profile, cur->platform_profile) != 0)
        return 1;
    return 0;
}

int handle_socket_config(int sockfd, powergov_config_t *config)
{
    fd_set readfds;
    struct timeval timeout;
    int client_fd;
    struct sockaddr_un client_addr;
    socklen_t client_len = sizeof(client_addr);
    ssize_t n;
    powergov_socket_msg_t msg;
    powergov_socket_status_t status;

    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    if (select(sockfd + 1, &readfds, NULL, NULL, &timeout) <= 0)
        return 0;

    if (!FD_ISSET(sockfd, &readfds))
        return 0;

    client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0)
        return -1;

    memset(&msg, 0, sizeof(msg));
    n = read(client_fd, &msg, sizeof(msg));

    if (n == (ssize_t)sizeof(int))
    {
        int threshold;
        memcpy(&threshold, &msg, sizeof(int));
        if (threshold == 0)
        {
            config->battery_safe_enabled = 0;
            config->battery_threshold = 0;
        }
        else if (threshold > 0 && threshold <= 100)
        {
            config->battery_safe_enabled = 1;
            config->battery_threshold = threshold;
        }
        powergov_config_save(config);
        g_force_reapply = 1;
    }
    else if (n == (ssize_t)sizeof(msg) && msg.magic == POWERGOV_SOCKET_MAGIC)
    {
        switch (msg.cmd)
        {
        case POWERGOV_SOCKET_CMD_SET_BATTERY_THRESHOLD:
            if (msg.value == 0)
            {
                config->battery_safe_enabled = 0;
                config->battery_threshold = 0;
            }
            else if (msg.value > 0 && msg.value <= 100)
            {
                config->battery_safe_enabled = 1;
                config->battery_threshold = msg.value;
            }
            powergov_config_save(config);
            g_force_reapply = 1;
            break;

        case POWERGOV_SOCKET_CMD_SET_USER_MODE:
            if (msg.value >= POWERGOV_USER_MAX_BATTERY &&
                msg.value <= POWERGOV_USER_PERFORMANCE)
            {
                config->user_mode = (powergov_user_mode_t)msg.value;
                powergov_config_save(config);
                g_force_reapply = 1;
            }
            break;

        case POWERGOV_SOCKET_CMD_SET_FEATURE:
            if (msg.value >= 0 && msg.value < POWERGOV_FEATURE_COUNT)
            {
                int mask = powergov_features_to_mask(&config->features);
                if (msg.value2)
                    mask |= (1 << msg.value);
                else
                    mask &= ~(1 << msg.value);
                powergov_features_from_mask(&config->features, mask);
                powergov_config_save(config);
                g_force_reapply = 1;
            }
            break;

        case POWERGOV_SOCKET_CMD_QUERY_BATTERY_CONFIG:
            status.battery_safe_enabled = config->battery_safe_enabled;
            status.battery_threshold = config->battery_threshold;
            status.user_mode = (int)config->user_mode;
            status.features_mask = powergov_features_to_mask(&config->features);
            status.log_level = (int)config->log_level;
            if (socket_write_full(client_fd, &status, sizeof(status)) != 0)
                PG_LOG_D("loop", "socket reply failed");
            break;

        case POWERGOV_SOCKET_CMD_QUERY_FULL_CONFIG:
            status.battery_safe_enabled = config->battery_safe_enabled;
            status.battery_threshold = config->battery_threshold;
            status.user_mode = (int)config->user_mode;
            status.features_mask = powergov_features_to_mask(&config->features);
            status.log_level = (int)config->log_level;
            if (socket_write_full(client_fd, &status, sizeof(status)) != 0)
                PG_LOG_D("loop", "socket reply failed");
            break;

        case POWERGOV_SOCKET_CMD_QUERY_STATUS:
        {
            powergov_reply_status_t rs;
            powergov_info_fill_status(config, &rs);
            if (socket_write_full(client_fd, &rs, sizeof(rs)) != 0)
                PG_LOG_D("loop", "socket reply failed");
            break;
        }

        case POWERGOV_SOCKET_CMD_QUERY_SYSTEM:
        {
            powergov_reply_system_t rs;
            powergov_info_fill_system(&rs);
            if (socket_write_full(client_fd, &rs, sizeof(rs)) != 0)
                PG_LOG_D("loop", "socket reply failed");
            break;
        }

        case POWERGOV_SOCKET_CMD_QUERY_CPU:
        {
            powergov_reply_cpu_t rs;
            powergov_info_fill_cpu(&rs);
            if (socket_write_full(client_fd, &rs, sizeof(rs)) != 0)
                PG_LOG_D("loop", "socket reply failed");
            break;
        }

        case POWERGOV_SOCKET_CMD_QUERY_COMPAT:
        {
            powergov_reply_compat_t rs;
            powergov_info_fill_compat(config, &rs);
            if (socket_write_full(client_fd, &rs, sizeof(rs)) != 0)
                PG_LOG_D("loop", "socket reply failed");
            break;
        }

        case POWERGOV_SOCKET_CMD_QUERY_METRICS:
        {
            powergov_reply_metrics_t rs;
            powergov_info_fill_metrics(&rs);
            if (socket_write_full(client_fd, &rs, sizeof(rs)) != 0)
                PG_LOG_D("loop", "socket reply failed");
            break;
        }

        case POWERGOV_SOCKET_CMD_QUERY_LOG:
        {
            powergov_reply_log_t rs;
            powergov_info_fill_log(msg.value > 0 ? msg.value : 80, &rs);
            if (socket_write_full(client_fd, &rs, sizeof(rs)) != 0)
                PG_LOG_D("loop", "socket reply failed");
            break;
        }

        case POWERGOV_SOCKET_CMD_QUERY_BUNDLE:
        {
            powergov_reply_bundle_t rs;
            unsigned int mask = (unsigned int)msg.value;
            int log_lines = msg.value2;

            if (mask == 0)
                mask = POWERGOV_BUNDLE_STATUS;
            powergov_info_fill_bundle(config, mask, log_lines, &rs);
            if (socket_write_full(client_fd, &rs, sizeof(rs)) != 0)
                PG_LOG_D("loop", "socket reply failed");
            break;
        }

        default:
            break;
        }
    }
    else if (n > 0 && n < (ssize_t)sizeof(msg))
    {
        PG_LOG_W("loop", "partial socket read (%zd bytes)", n);
    }

    close(client_fd);
    return 1;
}

void cleanup_socket_server(int sockfd)
{
    if (sockfd >= 0)
    {
        close(sockfd);
        unlink(POWERGOV_SOCKET_PATH);
    }
}

static powergov_gov_state_t detect_initial_state(void)
{
    char gov[64];

    if (cpu_governor_read(gov, sizeof(gov)) != 0)
        return POWERGOV_GOV_POWERSAVE;

    if (strcmp(gov, "performance") == 0)
        return POWERGOV_GOV_PERFORMANCE;
    if (strcmp(gov, "schedutil") == 0)
        return POWERGOV_GOV_BALANCED;

    return POWERGOV_GOV_POWERSAVE;
}

void powergov_loop(powergov_config_t *config)
{
    powergov_state_machine_t sm;
    powergov_power_info_t power;
    powergov_effective_policy_t policy;
    powergov_effective_policy_t prev_policy;
    powergov_power_info_t cached_power;
    int socket_fd;
    int battery_tick = 0;
    int battery_limited = 0;
    int allow_performance = 0;
    int have_prev = 0;
    int have_cached_power = 0;
    int prev_features_mask = -1;

    if (!config)
        return;

    powergov_log_init(config);
    powergov_metrics_init();
    powergov_power_supply_detect();

    if (platform_ppd_active() && config->features.platform_profile)
    {
        config->features.platform_profile = 0;
        PG_LOG_W("loop", "power-profiles-daemon detected; platform_profile disabled");
    }

    socket_fd = setup_socket_server();
    if (socket_fd < 0)
        PG_LOG_W("loop", "socket server unavailable");

    powergov_state_machine_init(&sm, detect_initial_state());
    memset(&prev_policy, 0, sizeof(prev_policy));

    PG_LOG_I("loop", "started mode=%s features=0x%x",
             powergov_user_mode_str(config->user_mode),
             powergov_features_to_mask(&config->features));

    for (;;)
    {
        powergov_gov_state_t gov_state;
        double load;

        if (powergov_shutdown)
            break;

        load = get_cpu_usage();

        if (socket_fd >= 0)
        {
            while (handle_socket_config(socket_fd, config) > 0)
                ;
        }

        if (!have_cached_power || battery_tick >= POWERGOV_BATTERY_REFRESH)
        {
            if (powergov_power_supply_poll(&cached_power) == 0)
                have_cached_power = 1;
            battery_tick = 0;
        }
        battery_tick++;

        if (have_cached_power)
            power = cached_power;
        else
            memset(&power, 0, sizeof(power));

        if (config->battery_safe_enabled && power.present && power.capacity_pct >= 0)
            battery_limited = (power.capacity_pct <= config->battery_threshold);
        else
            battery_limited = 0;

        powergov_profile_compute(config, &power, sm.state, battery_limited, &policy);
        allow_performance = policy.allow_performance;

        gov_state = powergov_state_machine_step(&sm, config, load,
                                                battery_limited, allow_performance);
        powergov_profile_compute(config, &power, gov_state, battery_limited, &policy);

        {
            int features_mask = powergov_features_to_mask(&config->features);
            if (g_force_reapply ||
                !have_prev ||
                policy_changed(&prev_policy, &policy, features_mask,
                               prev_features_mask))
            {
                apply_full_policy(config, &policy);
                prev_policy = policy;
                prev_features_mask = features_mask;
                have_prev = 1;
                g_force_reapply = 0;
                PG_LOG_D("loop", "policy gov=%s epp=%s turbo=%d cap=%d src=%s load=%.0f%%",
                         policy.governor, policy.epp, policy.turbo_on,
                         policy.freq_cap_pct,
                         powergov_power_source_str(power.source), load * 100.0);
            }
        }

        powergov_metrics_tick();
        powergov_metrics_sample_rapl();
        powergov_metrics_write_file();

        for (int i = 0; i < 20 && !powergov_shutdown; i++)
            usleep(100000);
    }

    cpu_policy_restore(config);
    runtime_pm_restore();
    cleanup_socket_server(socket_fd);
    PG_LOG_I("loop", "shutdown complete");
    powergov_log_shutdown();
}
