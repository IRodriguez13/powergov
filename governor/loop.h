#ifndef LOOP_H
#define LOOP_H

typedef struct
{
    int battery_safe_enabled;
    int battery_threshold;
} powergov_config_t;


void powergov_loop(powergov_config_t *config);
int setup_socket_server(void);
int handle_socket_config(int sockfd, powergov_config_t *config);
void cleanup_socket_server(int sockfd);

#endif