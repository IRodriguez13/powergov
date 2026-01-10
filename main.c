#include "governor/loop.h"
#include "Battery/battery.h"
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

    /* Try both /run and /tmp locations */
    for (i = 0; socket_paths[i] != NULL; i++)
    {
        /* Create socket */
        sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            continue;
        }

        /* Setup address structure */
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sun_family = AF_UNIX;
        strncpy(server_addr.sun_path, socket_paths[i], sizeof(server_addr.sun_path) - 1);

        /* Connect to server */
        if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0)
        {
            /* Connection successful, send threshold */
            n = write(sockfd, &threshold, sizeof(int));
            close(sockfd);

            if (n == sizeof(int))
            {
                return 0; /* Success */
            }
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
        printf("Usage: powergov [on/off]\n");
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
            printf("Battery-safe configuration updated successfully.\n");
            return 0;
        }

        /* If no process is running, inform user to use systemd */
        fprintf(stderr, "Error: No running powergov process found.\n");
        fprintf(stderr, "Please start the service with: sudo systemctl start powergov\n");
        fprintf(stderr, "Or run manually with: sudo powergov on\n");
        return 1;
    }

    else if (strcmp(argv[1], "getbattery") == 0)
    {
        int b = get_battery_level();

        if (b >= 0)
            printf("%d\n", b);
    }

    else if (strcmp(argv[1], "--help") == 0)
    {
        printf(
            "Usage:\n"
            "  powergov on\n"
            "  powergov off\n"
            "  powergov --battery-safe <percent>\n"
            "  powergov getbattery\n\n"
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