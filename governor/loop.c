#include "loop.h"
#include "../Battery/battery.h"
#include "../cpu/cpu_load.h"
#include "governor.h"
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define SOCKET_PATH "/run/powergov.sock"

#define CPU_LOW 0.25
#define CPU_MEDIUM 0.60
#define CPU_HIGH 0.75

typedef enum
{
    GOV_POWERSAVE,
    GOV_BALANCED,
    GOV_PERFORMANCE

} state_t;

int setup_socket_server(void)
{
    int sockfd;
    struct sockaddr_un server_addr;
    const char *socket_path = SOCKET_PATH;

    /* Create socket */
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return -1;
    }

    /* Remove existing socket file if it exists */
    unlink(SOCKET_PATH);

    /* Setup address structure */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    /* Bind socket */
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        /* If /run doesn't exist or we don't have permission, try /tmp as fallback */
        if (errno == ENOENT || errno == EACCES)
        {
            socket_path = "/tmp/powergov.sock";
            unlink(socket_path);
            strncpy(server_addr.sun_path, socket_path, sizeof(server_addr.sun_path) - 1);
            if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
            {
                perror("bind");
                close(sockfd);
                return -1;
            }
        }
        else
        {
            perror("bind");
            close(sockfd);
            return -1;
        }
    }

    /* Set socket permissions to allow connections */
    chmod(server_addr.sun_path, 0666);

    /* Listen for connections */
    if (listen(sockfd, 5) < 0)
    {
        perror("listen");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int handle_socket_config(int sockfd, powergov_config_t *config)
{
    fd_set readfds;
    struct timeval timeout;
    int activity;
    int client_fd;
    struct sockaddr_un client_addr;
    socklen_t client_len = sizeof(client_addr);
    ssize_t n;
    int threshold;
    powergov_socket_msg_t msg;

    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    activity = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

    if (activity < 0 && errno != EINTR)
    {
        perror("select");
        return -1;
    }

    if (activity > 0 && FD_ISSET(sockfd, &readfds))
    {
        client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            perror("accept");
            return -1;
        }

        /* Accept both legacy (int threshold) and v1 protocol messages */
        memset(&msg, 0, sizeof(msg));
        n = read(client_fd, &msg, sizeof(msg));

        if (n == (ssize_t)sizeof(int))
        {
            /* Legacy client: it sent only an int threshold */
            memcpy(&threshold, &msg, sizeof(int));
            if (threshold == 0)
            {
                config->battery_safe_enabled = 0;
            }
            else if (threshold > 0 && threshold <= 100)
            {
                config->battery_safe_enabled = 1;
                config->battery_threshold = threshold;
            }
        }
        else if (n == (ssize_t)sizeof(msg) && msg.magic == POWERGOV_SOCKET_MAGIC)
        {
            if (msg.cmd == POWERGOV_SOCKET_CMD_SET_BATTERY_THRESHOLD)
            {
                threshold = msg.value;
                if (threshold == 0)
                {
                    config->battery_safe_enabled = 0;
                }
                else if (threshold > 0 && threshold <= 100)
                {
                    config->battery_safe_enabled = 1;
                    config->battery_threshold = threshold;
                }
            }
            else if (msg.cmd == POWERGOV_SOCKET_CMD_QUERY_BATTERY_CONFIG)
            {
                powergov_socket_status_t status;
                status.battery_safe_enabled = config->battery_safe_enabled;
                status.battery_threshold = config->battery_threshold;
                (void)write(client_fd, &status, sizeof(status));
            }
        }

        close(client_fd);
        return 1; /* Configuration updated */
    }

    return 0; /* No new configuration */
}

void cleanup_socket_server(int sockfd)
{
    if (sockfd >= 0)
    {
        close(sockfd);
        /* Cleanup socket files */
        unlink(SOCKET_PATH);
        unlink("/tmp/powergov.sock");
    }
}

void powergov_loop(powergov_config_t *config)
{
    if (!config)
    {
        fprintf(stderr, "powergov_loop: NULL config\n");
        return;
    }

    /* Setup socket server for dynamic configuration */
    int socket_fd = setup_socket_server();
    if (socket_fd < 0)
    {
        fprintf(stderr, "Warning: Failed to setup socket server. Dynamic reconfiguration disabled.\n");
    }

    state_t state = GOV_POWERSAVE;

    static int battery_available = -1;
    static int last_battery = -1;
    static int battery_tick = 0;

    for (;;)
    {
        /* Check for new configuration from socket */
        if (socket_fd >= 0)
        {
            handle_socket_config(socket_fd, config);
        }

        double load = get_cpu_usage();
        int battery_limited = 0;

        if (config->battery_safe_enabled)
        {
            /* Battery? */
            if (battery_available == -1)
            {
                int b = get_battery_level();
                battery_available = (b >= 0) ? 1 : 0;
                last_battery = b;
            }

            /* If battery, refresh in N secs */
            if (battery_available)
            {
                if (battery_tick++ >= 5) // ~10 secs
                {
                    last_battery = get_battery_level();
                    battery_tick = 0;
                }

                if (last_battery >= 0 &&
                    last_battery <= config->battery_threshold)
                {
                    battery_limited = 1;
                }
            }
        }

        /* State machine */
        if (state == GOV_POWERSAVE && load > CPU_LOW)
        {
            set_governor("schedutil");
            state = GOV_BALANCED;
        }

        if (!battery_limited &&
            state == GOV_BALANCED &&
            load > CPU_HIGH)
        {
            set_governor("performance");
            state = GOV_PERFORMANCE;
        }

        if (battery_limited && state == GOV_PERFORMANCE)
        {
            set_governor("schedutil");
            state = GOV_BALANCED;
        }
        else if (state == GOV_PERFORMANCE && load < CPU_MEDIUM)
        {
            set_governor("schedutil");
            state = GOV_BALANCED;
        }
        else if (state == GOV_BALANCED && load < CPU_LOW)
        {
            set_governor("powersave");
            state = GOV_POWERSAVE;
        }

        sleep(2);
    }

    /* Cleanup on exit */
    cleanup_socket_server(socket_fd);
}

