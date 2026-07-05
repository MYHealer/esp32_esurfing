#ifndef ESURFINGCLIENT_NETCLIENT_H
#define ESURFINGCLIENT_NETCLIENT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    REQUEST_ERROR = 0,
    REQUEST_INIT_ERROR = 1,
    REQUEST_WARN = 2,
    REQUEST_HAVE_RES = 200,
    REQUEST_SUCCESS = 204,
    REQUEST_REDIRECT = 302
} NetworkStatus;

typedef struct {
    NetworkStatus status;
    char* body_data;
    size_t body_size;
} http_resp_t;

/**
 * @brief 截取 URL 中指定参数
 */
char* extract_url_param(const char* url, const char* search_str_start);

/**
 * @brief 带默认头的 POST (使用 esp_http_client)
 */
http_resp_t post(const char* url, const char* data);

/**
 * @brief 带默认头的 GET (使用 esp_http_client)
 */
http_resp_t get(const char* url);

/**
 * @brief 检测网络状态 (get generate_204)
 */
NetworkStatus check_network_status(void);

/**
 * @brief 获取认证配置 URL (检测重定向)
 */
NetworkStatus get_last_location(void);

/**
 * @brief 重置网络客户端内部状态 (school_id/domain/area)
 */
void reset_net_client_state(void);

#endif //ESURFINGCLIENT_NETCLIENT_H
