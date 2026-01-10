#include "governor/loop.h"
#include "Battery/battery.h"
#include "cpu/cpu_load.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define SOCKET_PATH "/run/powergov.sock"

powergov_config_t config = {0};

int send_battery_config(int threshold)
{
    int sockfd;
    struct sockaddr_un server_addr;
    ssize_t n;
    const char *socket_paths[] = {SOCKET_PATH, "/tmp/powergov.sock", NULL};
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
    const char *socket_paths[] = {SOCKET_PATH, "/tmp/powergov.sock", NULL};
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

/* Main CLI entrypoint with the on/off logic. No more, no less*/

void stop_powergov()
{
    int kill = system("pkill powergov");

    if(kill)
    {
        printf("Powergov stopped.\n");
    }
}

void start_powergov(powergov_config_t *config)
{
    setsid();
    powergov_loop(config);
    stop_powergov();
}


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: powergov [on/off/status]\n");
        return -1;
    }

    if (strcmp(argv[1], "on") == 0)
    {
        /* Call to start_powergov(); */
        start_powergov(&config);
    }

    else if (strcmp(argv[1], "off") == 0)
    {
        /* Call to stop_powergov(); */
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

        /* If no process is running, inform user to use systemd */
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
            printf("Battery-safe: unknown (powergov not running)\n");
            printf("Battery-safe threshold: unknown\n");
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
            "\n"
            "Options:\n"
            "  --battery-safe N   Disable performance governor when battery <= N%%\n"
            "  Use 0 to disable battery safe mode\n");
    }

    else
    {
        printf("Invalid argument. Use --help.\n");
        return 1;
    }

    return 0;
}