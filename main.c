#include "governor/loop.h"
#include "Battery/battery.h"
#include "config/config.h"
#include "cpu/cpu_load.h"
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

#define SOCKET_PATH "/run/powergov/powergov.sock"
#define PIDFILE_PATH "/run/powergov/powergov.pid"

powergov_config_t config = {0};

static int pidfile_fd = -1;

static const char *socket_paths[] = {SOCKET_PATH, "/tmp/powergov.sock", NULL};

int send_battery_config(int threshold)
{
    int sockfd;
    struct sockaddr_un server_addr;
    ssize_t n;
    int i;

    for (i = 0; socket_paths[i] != NULL; i++)
    {
        sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            continue;
        }

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sun_family = AF_UNIX;
        strncpy(server_addr.sun_path, socket_paths[i], sizeof(server_addr.sun_path) - 1);

        /* Connect to server */
        if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0)
        {
            /* Connection successful, send threshold (legacy protocol) */
            n = write(sockfd, &threshold, sizeof(int));
            close(sockfd);

            if (n == sizeof(int))
            {
                return 0;
            }
            return -1;
        }

        close(sockfd);
    }

    return -1;
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

static int read_current_governor(char *out, size_t out_sz)
{
    if (!out || out_sz == 0)
        return -1;

    FILE *f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "r");
    if (!f)
        return -1;

    if (!fgets(out, (int)out_sz, f))
    {
        fclose(f);
        return -1;
    }

    fclose(f);

    /* Strip newline */
    size_t len = strlen(out);
    if (len > 0 && out[len - 1] == '\n')
        out[len - 1] = '\0';

    return 0;
}

static int query_battery_config(powergov_socket_status_t *out_status)
{
    int sockfd;
    struct sockaddr_un server_addr;
    ssize_t n;
    int i;

    if (!out_status)
        return -1;

    for (i = 0; socket_paths[i] != NULL; i++)
    {
        sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sockfd < 0)
            continue;

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sun_family = AF_UNIX;
        strncpy(server_addr.sun_path, socket_paths[i], sizeof(server_addr.sun_path) - 1);

        if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0)
        {
            powergov_socket_msg_t msg;
            msg.magic = POWERGOV_SOCKET_MAGIC;
            msg.cmd = POWERGOV_SOCKET_CMD_QUERY_BATTERY_CONFIG;
            msg.value = 0;

            n = write(sockfd, &msg, sizeof(msg));
            if (n != (ssize_t)sizeof(msg))
            {
                close(sockfd);
                return -1;
            }

            n = read(sockfd, out_status, sizeof(*out_status));
            close(sockfd);

            if (n == (ssize_t)sizeof(*out_status))
                return 0;
            return -1;
        }

        close(sockfd);
    }

    return -1;
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

    fd = open(PIDFILE_PATH, O_RDWR | O_CREAT, 0644);
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
    unlink(PIDFILE_PATH);
}

static int stop_via_pidfile(void)
{
    FILE *f;
    pid_t pid;

    f = fopen(PIDFILE_PATH, "r");
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

    if (kill(pid, SIGTERM) != 0)
        return -1;

    return 0;
}

void stop_powergov(void)
{
    if (stop_via_pidfile() == 0)
    {
        printf("Powergov stopped.\n");
        return;
    }

    if (system("pkill -x powergov") == 0)
        printf("Powergov stopped.\n");
}

void start_powergov(powergov_config_t *config)
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
    powergov_config_load(config);
    powergov_loop(config);
    release_pidfile();
}


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: powergov [on/off/status]\n");
        return -1;
    }

    if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)
    {
        printf("powergov %s\n", POWERGOV_VERSION);
        return 0;
    }

    if (strcmp(argv[1], "on") == 0)
    {
        start_powergov(&config);
    }

    else if (strcmp(argv[1], "off") == 0)
    {
        stop_powergov();
    }

    else if (strcmp(argv[1], "--battery-safe") == 0)
    {
        if (argc != 3)
        {
            fprintf(stderr, "Usage: --battery-safe <percent>\n");
            return 1;
        }

        int threshold = atoi(argv[2]);
        if (threshold < 0 || threshold > 100)
        {
            fprintf(stderr, "Invalid battery threshold (must be 0-100)\n");
            return 1;
        }

        /* Check if battery is available (skip check if disabling with 0) */
        if (threshold > 0 && get_battery_level() < 0)
        {
            fprintf(stderr, "powergov: warning: no battery detected, battery-safe mode not useful on this hardware\n");
            return 1;
        }

        /* Try to send configuration to running process */
        if (send_battery_config(threshold) == 0)
        {
            if (threshold != 0)
            {
                printf("Battery-safe configuration updated successfully.\n");
                return 0;
            }
                printf("battery-safe off\n");
            
            return 0;
        }

        /* Persist for next boot when daemon is not running */
        config.battery_safe_enabled = (threshold > 0);
        config.battery_threshold = threshold;
        if (powergov_config_save(&config) == 0)
        {
            printf("Battery-safe configuration saved for next service start.\n");
            return 0;
        }

        fprintf(stderr, "Error: No running powergov process found.\n");
        fprintf(stderr, "Please start the service with: sudo systemctl start powergov\n");
        fprintf(stderr, "Or run manually with: sudo powergov on\n");
        return 1;
    }

    else if (strcmp(argv[1], "status") == 0)
    {
        char gov[64];
        const char *state = "UNKNOWN";
        double load = get_cpu_usage();
        int battery = get_battery_level();

        if (read_current_governor(gov, sizeof(gov)) == 0)
            state = state_from_governor(gov);

        printf("Current state: %s\n", state);
        printf("CPU load: %.0f%%\n", load * 100.0);

        if (battery >= 0)
            printf("Battery: %d%%\n", battery);
        else
            printf("Battery: N/A\n");

        powergov_socket_status_t st;
        if (query_battery_config(&st) == 0)
        {
            printf("Battery-safe: %s\n", st.battery_safe_enabled ? "on" : "off");
            if (st.battery_safe_enabled)
                printf("Battery-safe threshold: %d%%\n", st.battery_threshold);
            else
                printf("Battery-safe threshold: 0%%\n");
        }
        else
        {
            powergov_config_load(&config);
            if (config.battery_safe_enabled)
            {
                printf("Battery-safe: on (saved, service not running)\n");
                printf("Battery-safe threshold: %d%%\n", config.battery_threshold);
            }
            else
            {
                printf("Battery-safe: off\n");
                printf("Battery-safe threshold: 0%%\n");
            }
        }
    }

    else if (strcmp(argv[1], "--help") == 0)
    {
        printf(
            "Usage:\n"
            "  powergov on\n"
            "  powergov off\n"
            "  powergov status\n"
            "  powergov --battery-safe <percent>\n"
            "  powergov -v\n"
            "\n"
            "Options:\n"
            "  -v, --version      Print version and exit\n"
            "  --battery-safe N   Disable performance governor when battery <= N%%\n"
            "  Use 0 to disable battery safe mode\n"
            "\n"
            "Boot service:\n"
            "  sudo make install-service   Install and enable systemd unit\n"
            "  sudo systemctl status powergov\n");
    }

    else
    {
        printf("Invalid argument. Use --help.\n");
        return 1;
    }

    return 0;
}
