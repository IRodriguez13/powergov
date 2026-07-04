#include "core/loop.h"
#include "power/power_supply.h"
#include "config/config.h"
#include "cpu/cpu_load.h"
#include "cpu/governor.h"
#include "cpu/epp.h"
#include "cpu/turbo.h"
#include "cpu/freq_cap.h"
#include "platform/platform_profile.h"
#include "log/log.h"
#include "metrics/metrics.h"
#include "version.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

powergov_config_t config;

static int pidfile_fd = -1;

static int send_socket_msg(powergov_socket_msg_t *msg,
                           powergov_socket_status_t *reply)
{
    int sockfd;
    struct sockaddr_un addr;
    ssize_t n;

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0)
        return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, POWERGOV_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        close(sockfd);
        return -1;
    }

    n = write(sockfd, msg, sizeof(*msg));
    if (n != (ssize_t)sizeof(*msg))
    {
        close(sockfd);
        return -1;
    }

    if (reply)
    {
        n = read(sockfd, reply, sizeof(*reply));
        close(sockfd);
        return (n == (ssize_t)sizeof(*reply)) ? 0 : -1;
    }

    close(sockfd);
    return 0;
}

static int send_battery_config(int threshold)
{
    powergov_socket_msg_t msg;

    msg.magic = POWERGOV_SOCKET_MAGIC;
    msg.cmd = POWERGOV_SOCKET_CMD_SET_BATTERY_THRESHOLD;
    msg.value = threshold;
    msg.value2 = 0;
    return send_socket_msg(&msg, NULL);
}

static int send_user_mode(powergov_user_mode_t mode)
{
    powergov_socket_msg_t msg;

    msg.magic = POWERGOV_SOCKET_MAGIC;
    msg.cmd = POWERGOV_SOCKET_CMD_SET_USER_MODE;
    msg.value = (int)mode;
    msg.value2 = 0;
    return send_socket_msg(&msg, NULL);
}

static int send_feature_toggle(powergov_feature_id_t id, int on)
{
    powergov_socket_msg_t msg;

    msg.magic = POWERGOV_SOCKET_MAGIC;
    msg.cmd = POWERGOV_SOCKET_CMD_SET_FEATURE;
    msg.value = (int)id;
    msg.value2 = on ? 1 : 0;
    return send_socket_msg(&msg, NULL);
}

static int query_runtime_config(powergov_socket_status_t *st)
{
    powergov_socket_msg_t msg;

    msg.magic = POWERGOV_SOCKET_MAGIC;
    msg.cmd = POWERGOV_SOCKET_CMD_QUERY_FULL_CONFIG;
    msg.value = 0;
    msg.value2 = 0;
    return send_socket_msg(&msg, st);
}

static const char *state_from_governor(const char *gov)
{
    if (!gov)
        return "UNKNOWN";
    if (strcmp(gov, "powersave") == 0)
        return "POWERSAVE";
    if (strcmp(gov, "performance") == 0)
        return "PERFORMANCE";
    if (strcmp(gov, "schedutil") == 0)
        return "BALANCED";
    return "UNKNOWN";
}

static void on_shutdown_signal(int sig)
{
    (void)sig;
    powergov_request_shutdown();
}

static int acquire_pidfile(void)
{
    int fd;
    char pidbuf[32];
    struct flock lock = {
        .l_type = F_WRLCK,
        .l_whence = SEEK_SET,
        .l_start = 0,
        .l_len = 0
    };

    mkdir("/run/powergov", 0755);

    fd = open(POWERGOV_PIDFILE_PATH, O_RDWR | O_CREAT, 0644);
    if (fd < 0)
        return -1;

    if (fcntl(fd, F_SETLK, &lock) < 0)
    {
        close(fd);
        return -1;
    }

    if (ftruncate(fd, 0) < 0)
    {
        close(fd);
        return -1;
    }

    snprintf(pidbuf, sizeof(pidbuf), "%d\n", getpid());
    if (write(fd, pidbuf, strlen(pidbuf)) < 0)
    {
        close(fd);
        return -1;
    }

    return fd;
}

static void release_pidfile(void)
{
    if (pidfile_fd >= 0)
    {
        close(pidfile_fd);
        pidfile_fd = -1;
    }
    unlink(POWERGOV_PIDFILE_PATH);
}

static int stop_via_pidfile(void)
{
    FILE *f;
    pid_t pid;

    f = fopen(POWERGOV_PIDFILE_PATH, "r");
    if (!f)
        return -1;

    if (fscanf(f, "%d", &pid) != 1)
    {
        fclose(f);
        return -1;
    }
    fclose(f);

    if (pid <= 1)
        return -1;

    return kill(pid, SIGTERM) == 0 ? 0 : -1;
}

static void config_load_or_defaults(powergov_config_t *cfg)
{
    powergov_config_set_defaults(cfg);
    powergov_config_load(cfg);
}

static void stop_powergov(void)
{
    if (stop_via_pidfile() == 0)
    {
        printf("Powergov stopped.\n");
        return;
    }

    if (system("pkill -x powergov") == 0)
        printf("Powergov stopped.\n");
}

static void start_powergov(powergov_config_t *cfg)
{
    struct sigaction sa;

    pidfile_fd = acquire_pidfile();
    if (pidfile_fd < 0)
    {
        fprintf(stderr, "powergov: already running or unable to create pidfile\n");
        return;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_shutdown_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    setsid();
    powergov_config_set_defaults(cfg);
    powergov_config_load(cfg);
    powergov_loop(cfg);
    release_pidfile();
}

static void print_features(const powergov_features_t *f)
{
    int i;
    int mask = powergov_features_to_mask(f);

    for (i = 0; i < POWERGOV_FEATURE_COUNT; i++)
    {
        printf("  %-12s %s\n", powergov_feature_name((powergov_feature_id_t)i),
               (mask & (1 << i)) ? "on" : "off");
    }
}

static void cmd_status(void)
{
    char gov[64];
    char epp[64];
    int turbo = -1;
    powergov_power_info_t power;
    powergov_socket_status_t st;
    double load = get_cpu_usage();

    if (cpu_governor_read(gov, sizeof(gov)) != 0)
        strcpy(gov, "unknown");

    printf("State:        %s\n", state_from_governor(gov));
    printf("CPU load:     %.0f%%\n", load * 100.0);

    if (powergov_power_supply_poll(&power) == 0 && power.present)
    {
        printf("Power:        %s\n", powergov_power_source_str(power.source));
        if (power.capacity_pct >= 0)
            printf("Battery:      %d%%\n", power.capacity_pct);
    }
    else
    {
        printf("Battery:      N/A\n");
    }

    if (cpu_epp_available() && cpu_epp_read(epp, sizeof(epp)) == 0)
        printf("EPP:          %s\n", epp);

    if (cpu_turbo_available() && cpu_turbo_read(&turbo) == 0)
        printf("Turbo:        %s\n", turbo ? "on" : "off");

    if (platform_profile_available())
    {
        char plat[64];
        if (platform_profile_read(plat, sizeof(plat)) == 0)
            printf("Platform:     %s\n", plat);
    }

    if (query_runtime_config(&st) == 0)
    {
        printf("User mode:    %s\n", powergov_user_mode_str((powergov_user_mode_t)st.user_mode));
        printf("Battery-safe: %s", st.battery_safe_enabled ? "on" : "off");
        if (st.battery_safe_enabled)
            printf(" (threshold %d%%)", st.battery_threshold);
        printf("\n");
        powergov_features_t f;
        powergov_features_from_mask(&f, st.features_mask);
        printf("Features:\n");
        print_features(&f);
    }
    else
    {
        powergov_config_load(&config);
        printf("User mode:    %s (saved)\n", powergov_user_mode_str(config.user_mode));
        printf("Battery-safe: %s\n", config.battery_safe_enabled ? "on" : "off");
        printf("Features:\n");
        print_features(&config.features);
    }
}

static int cmd_mode(int argc, char *argv[])
{
    powergov_user_mode_t mode;

    if (argc != 3)
    {
        fprintf(stderr, "Usage: powergov mode <max-battery|balanced|performance>\n");
        return 1;
    }

    mode = powergov_user_mode_parse(argv[2]);
    if (strcmp(argv[2], "max-battery") != 0 && strcmp(argv[2], "max") != 0 &&
        strcmp(argv[2], "balanced") != 0 &&
        strcmp(argv[2], "performance") != 0 && strcmp(argv[2], "perf") != 0)
    {
        fprintf(stderr, "Unknown mode: %s\n", argv[2]);
        return 1;
    }

    if (send_user_mode(mode) == 0)
    {
        printf("User mode set to %s.\n", powergov_user_mode_str(mode));
        return 0;
    }

    config_load_or_defaults(&config);
    config.user_mode = mode;
    if (powergov_config_save(&config) == 0)
    {
        printf("Mode saved for next start: %s\n", powergov_user_mode_str(mode));
        return 0;
    }

    fprintf(stderr, "Error: daemon not running and config not writable.\n");
    return 1;
}

static int cmd_feature(int argc, char *argv[])
{
    powergov_feature_id_t id;
    int on;

    if (argc == 3 && strcmp(argv[2], "list") == 0)
    {
        int i;
        printf("Available features:\n");
        for (i = 0; i < POWERGOV_FEATURE_COUNT; i++)
            printf("  %s\n", powergov_feature_name((powergov_feature_id_t)i));
        return 0;
    }

    if (argc != 4)
    {
        fprintf(stderr, "Usage: powergov feature <name> on|off\n");
        fprintf(stderr, "       powergov features list\n");
        return 1;
    }

    if (powergov_feature_parse_name(argv[2], &id) != 0)
    {
        fprintf(stderr, "Unknown feature: %s\n", argv[2]);
        return 1;
    }

    if (strcmp(argv[3], "on") == 0)
        on = 1;
    else if (strcmp(argv[3], "off") == 0)
        on = 0;
    else
    {
        fprintf(stderr, "Use on or off\n");
        return 1;
    }

    if (send_feature_toggle(id, on) == 0)
    {
        printf("Feature %s turned %s.\n", powergov_feature_name(id), on ? "on" : "off");
        return 0;
    }

    powergov_config_load(&config);
    {
        int mask = powergov_features_to_mask(&config.features);
        if (on)
            mask |= (1 << id);
        else
            mask &= ~(1 << id);
        powergov_features_from_mask(&config.features, mask);
    }
    if (powergov_config_save(&config) == 0)
    {
        printf("Feature saved for next start.\n");
        return 0;
    }

    return 1;
}

static int cmd_battery_safe(int argc, char *argv[])
{
    int threshold;

    if (argc != 3)
    {
        fprintf(stderr, "Usage: powergov --battery-safe <percent>\n");
        return 1;
    }

    threshold = atoi(argv[2]);
    if (threshold < 0 || threshold > 100)
    {
        fprintf(stderr, "Invalid threshold (0-100)\n");
        return 1;
    }

    if (threshold > 0 && get_battery_level() < 0)
    {
        fprintf(stderr, "powergov: no battery detected\n");
        return 1;
    }

    if (send_battery_config(threshold) == 0)
    {
        printf(threshold ? "Battery-safe updated.\n" : "Battery-safe off.\n");
        return 0;
    }

    config_load_or_defaults(&config);
    config.battery_safe_enabled = (threshold > 0);
    config.battery_threshold = threshold;
    if (powergov_config_save(&config) == 0)
    {
        printf("Battery-safe saved for next start.\n");
        return 0;
    }

    fprintf(stderr, "Error: no running daemon.\n");
    return 1;
}

static int cmd_dev_log(int argc, char *argv[])
{
    int lines = 80;
    int follow = 0;
    int i;

    for (i = 2; i < argc; i++)
    {
        if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--follow") == 0)
            follow = 1;
        else if (strcmp(argv[i], "--tail") == 0 || strcmp(argv[i], "-n") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "powergov dev-log: missing value after %s\n", argv[i]);
                return 1;
            }
            lines = atoi(argv[++i]);
            if (lines <= 0)
                lines = 80;
        }
        else
        {
            fprintf(stderr, "Usage: powergov dev-log [--tail N|-n N] [-f|--follow]\n");
            return 1;
        }
    }

    if (follow)
        return powergov_log_follow(lines) == 0 ? 0 : 1;

    return powergov_log_tail(lines, 1) == 0 ? 0 : 1;
}

static void print_help(void)
{
    printf(
        "Usage:\n"
        "  powergov on\n"
        "  powergov off\n"
        "  powergov status\n"
        "  powergov mode <max-battery|balanced|performance>\n"
        "  powergov feature <name> on|off\n"
        "  powergov features list\n"
        "  powergov --battery-safe <percent>\n"
        "  powergov dev-log [--tail N|-n N] [-f|--follow]\n"
        "  powergov dev-metrics\n"
        "  powergov -h | --help\n"
        "  powergov -v | --version\n"
        "\n"
        "User modes:\n"
        "  max-battery  Maximum battery protection (default)\n"
        "  balanced     Moderate savings on battery\n"
        "  performance  Allow high performance even on battery\n"
        "\n"
        "Developer commands (not for end users):\n"
        "  dev-log      Show log; -f/--follow streams new lines (Ctrl+C to stop)\n"
        "  dev-metrics  Show apply/verify counters and RAPL estimate\n");
}

static int is_help_arg(const char *arg)
{
    return arg && (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0 ||
                   strcmp(arg, "-?") == 0);
}

int main(int argc, char *argv[])
{
    if (argc < 2 || is_help_arg(argv[1]))
    {
        print_help();
        return argc < 2 ? 1 : 0;
    }

    if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)
    {
        powergov_print_version();
        return 0;
    }

    if (strcmp(argv[1], "on") == 0)
    {
        start_powergov(&config);
        return 0;
    }

    if (strcmp(argv[1], "off") == 0)
    {
        stop_powergov();
        return 0;
    }

    if (strcmp(argv[1], "status") == 0)
    {
        cmd_status();
        return 0;
    }

    if (strcmp(argv[1], "mode") == 0)
        return cmd_mode(argc, argv);

    if (strcmp(argv[1], "feature") == 0)
        return cmd_feature(argc, argv);

    if (strcmp(argv[1], "features") == 0)
    {
        char *fake[] = {"powergov", "features", "list"};
        return cmd_feature(3, fake);
    }

    if (strcmp(argv[1], "--battery-safe") == 0)
        return cmd_battery_safe(argc, argv);

    if (strcmp(argv[1], "dev-log") == 0)
        return cmd_dev_log(argc, argv);

    if (strcmp(argv[1], "dev-metrics") == 0)
    {
        powergov_metrics_init();
        if (powergov_metrics_load_file() != 0)
            printf("(no live metrics file; showing local counters)\n\n");
        return powergov_metrics_print_human();
    }

    printf("Invalid argument. Use --help or -h.\n");
    return 1;
}
