#ifndef WEB_CONFIG_H
#define WEB_CONFIG_H

#include <stdbool.h>
#include "esp_err.h"
#include "config_codec.h"

/*
 * app_config_t 定义在 config_codec.h, 该文件不依赖 ESP-IDF,
 * 便于在宿主机上编译单元测试 (tests/test_config_codec.c)。
 * 此处不再重复定义, 避免两套结构体漂移。
 */

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
