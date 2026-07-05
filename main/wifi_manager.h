#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED
} wifi_state_t;

esp_err_t wifi_init(void);
esp_err_t wifi_connect(const char* ssid, const char* password);

wifi_state_t wifi_get_state(void);
bool wifi_wait_connected(uint32_t timeout_ms);
bool wifi_is_connected(void);
int wifi_get_rssi(void);
esp_err_t wifi_reconnect(void);

#endif /* WIFI_MANAGER_H */
