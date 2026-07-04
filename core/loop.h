#ifndef POWERGOV_LOOP_H
#define POWERGOV_LOOP_H

#include "../include/powergov/types.h"

void powergov_loop(powergov_config_t *config);
void powergov_request_shutdown(void);

int setup_socket_server(void);
int handle_socket_config(int sockfd, powergov_config_t *config);
void cleanup_socket_server(int sockfd);

#endif
