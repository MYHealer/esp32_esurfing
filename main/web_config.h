#ifndef WEB_CONFIG_H
#define WEB_CONFIG_H

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief 配置数据结构
 */
typedef struct {
    char wifi_ssid[32];
    char wifi_password[64];
    char campus_username[64];
    char campus_password[64];
    char channel[16];
} app_config_t;

/**
 * @brief 启动 Web 配置服务器 (端口 80)
 */
esp_err_t web_config_start(void);

/**
 * @brief 停止 Web 配置服务器
 */
esp_err_t web_config_stop(void);

/**
 * @brief 从 SPIFFS 加载配置
 * @return true 配置存在且完整, false 需要首次配置
 */
bool load_config(app_config_t* cfg);

/**
 * @brief 保存配置到 SPIFFS
 */
bool save_config(const app_config_t* cfg);

/**
 * @brief 获取当前配置状态文本
 */
const char* web_config_get_status(void);

#endif /* WEB_CONFIG_H */
