#include "../include/powergov/client.h"
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <unistd.h>

#define PG_SOCK_TIMEOUT_MS        3000
#define PG_SOCK_TIMEOUT_BUNDLE_MS 10000

static int set_socket_timeouts(int sock, int ms)
{
    struct timeval tv;

    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000L;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0)
        return -1;
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0)
        return -1;
    return 0;
}

static int socket_exchange_timeout(int cmd, int value, int value2,
                                   void *reply, size_t reply_len, int timeout_ms)
{
    int sock;
    struct sockaddr_un addr;
    powergov_socket_msg_t msg;
    ssize_t n;
    size_t got;

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
        return -1;

    set_socket_timeouts(sock, timeout_ms);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, POWERGOV_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        close(sock);
        return -1;
    }

    memset(&msg, 0, sizeof(msg));
    msg.magic = POWERGOV_SOCKET_MAGIC;
    msg.cmd = cmd;
    msg.value = value;
    msg.value2 = value2;

    if (write(sock, &msg, sizeof(msg)) != (ssize_t)sizeof(msg))
    {
        close(sock);
        return -1;
    }

    if (reply && reply_len > 0)
    {
        got = 0;
        while (got < reply_len)
        {
            n = read(sock, (char *)reply + got, reply_len - got);
            if (n <= 0)
            {
                close(sock);
                return -1;
            }
            got += (size_t)n;
        }
    }

    close(sock);
    return 0;
}

static int socket_exchange(int cmd, int value, int value2,
                           void *reply, size_t reply_len)
{
    return socket_exchange_timeout(cmd, value, value2, reply, reply_len,
                                   PG_SOCK_TIMEOUT_MS);
}

int powergov_client_ping(void)
{
    powergov_socket_status_t st;
    return socket_exchange(POWERGOV_SOCKET_CMD_QUERY_FULL_CONFIG, 0, 0,
                           &st, sizeof(st));
}

int powergov_client_set_user_mode(powergov_user_mode_t mode)
{
    return socket_exchange(POWERGOV_SOCKET_CMD_SET_USER_MODE, (int)mode, 0,
                           NULL, 0);
}

int powergov_client_set_battery_threshold(int threshold)
{
    return socket_exchange(POWERGOV_SOCKET_CMD_SET_BATTERY_THRESHOLD,
                           threshold, 0, NULL, 0);
}

int powergov_client_set_feature(powergov_feature_id_t id, int enabled)
{
    return socket_exchange(POWERGOV_SOCKET_CMD_SET_FEATURE, (int)id,
                           enabled ? 1 : 0, NULL, 0);
}

int powergov_client_set_tuning(powergov_tuning_id_t id, int value)
{
    return socket_exchange(POWERGOV_SOCKET_CMD_SET_TUNING, (int)id, value,
                           NULL, 0);
}

int powergov_client_query_tuning(powergov_reply_tuning_t *out)
{
    if (!out)
        return -1;
    return socket_exchange(POWERGOV_SOCKET_CMD_QUERY_TUNING, 0, 0,
                           out, sizeof(*out));
}

int powergov_client_query_status(powergov_reply_status_t *out)
{
    if (!out)
        return -1;
    return socket_exchange(POWERGOV_SOCKET_CMD_QUERY_STATUS, 0, 0,
                           out, sizeof(*out));
}

int powergov_client_query_system(powergov_reply_system_t *out)
{
    if (!out)
        return -1;
    return socket_exchange(POWERGOV_SOCKET_CMD_QUERY_SYSTEM, 0, 0,
                           out, sizeof(*out));
}

int powergov_client_query_cpu(powergov_reply_cpu_t *out)
{
    if (!out)
        return -1;
    return socket_exchange(POWERGOV_SOCKET_CMD_QUERY_CPU, 0, 0,
                           out, sizeof(*out));
}

int powergov_client_query_compat(powergov_reply_compat_t *out)
{
    if (!out)
        return -1;
    return socket_exchange(POWERGOV_SOCKET_CMD_QUERY_COMPAT, 0, 0,
                           out, sizeof(*out));
}

int powergov_client_query_metrics(powergov_reply_metrics_t *out)
{
    if (!out)
        return -1;
    return socket_exchange(POWERGOV_SOCKET_CMD_QUERY_METRICS, 0, 0,
                           out, sizeof(*out));
}

int powergov_client_query_log(int lines, powergov_reply_log_t *out)
{
    if (!out)
        return -1;
    return socket_exchange(POWERGOV_SOCKET_CMD_QUERY_LOG, lines, 0,
                           out, sizeof(*out));
}

int powergov_client_query_bundle(unsigned int mask, int log_lines,
                                 powergov_reply_bundle_t *out)
{
    if (!out)
        return -1;
    return socket_exchange_timeout(POWERGOV_SOCKET_CMD_QUERY_BUNDLE, (int)mask,
                                   log_lines, out, sizeof(*out),
                                   PG_SOCK_TIMEOUT_BUNDLE_MS);
}

const char *powergov_user_mode_title(powergov_user_mode_t mode)
{
    switch (mode)
    {
    case POWERGOV_USER_MAX_BATTERY: return "Modo inteligente";
    case POWERGOV_USER_BALANCED:    return "Equilibrado";
    case POWERGOV_USER_PERFORMANCE: return "Máximo rendimiento";
    case POWERGOV_USER_CUSTOM:      return "Personalizado";
    default:                        return "Desconocido";
    }
}

const char *powergov_user_mode_subtitle(powergov_user_mode_t mode)
{
    switch (mode)
    {
    case POWERGOV_USER_MAX_BATTERY:
        return "Automático — ahorra en batería, afloja enchufado";
    case POWERGOV_USER_BALANCED:
        return "Intermedio manual entre autonomía y velocidad";
    case POWERGOV_USER_PERFORMANCE:
        return "Manual — prioriza velocidad incluso en batería";
    default:
        return "";
    }
}

const char *powergov_compat_state_str(int state)
{
    switch (state)
    {
    case POWERGOV_COMPAT_SUPPORTED:   return "supported";
    case POWERGOV_COMPAT_PARTIAL:     return "partial";
    case POWERGOV_COMPAT_CONFLICT:    return "conflict";
    default:                          return "unsupported";
    }
}
