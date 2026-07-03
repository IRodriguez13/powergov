#ifndef LOOP_H
#define LOOP_H

typedef struct
{
    int battery_safe_enabled;
    int battery_threshold;
} powergov_config_t;

#define POWERGOV_SOCKET_MAGIC 0x50474F56

typedef enum
{
    POWERGOV_SOCKET_CMD_SET_BATTERY_THRESHOLD = 1,
    POWERGOV_SOCKET_CMD_QUERY_BATTERY_CONFIG = 2
} powergov_socket_cmd_t;

typedef struct
{
    int magic;
    int cmd;
    int value;
} powergov_socket_msg_t;

typedef struct
{
    int battery_safe_enabled;
    int battery_threshold;
} powergov_socket_status_t;

void powergov_loop(powergov_config_t *config);
void powergov_request_shutdown(void);
int setup_socket_server(void);
int handle_socket_config(int sockfd, powergov_config_t *config);
void cleanup_socket_server(int sockfd);

#endif