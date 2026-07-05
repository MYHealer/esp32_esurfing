#ifndef ESURFINGCLIENT_PLATFORMUTILS_H
#define ESURFINGCLIENT_PLATFORMUTILS_H

#include "States.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define SEP '/'
#define XML_BUFFER_SIZE 1024
#define NAME_LENGTH 128

typedef enum
{
    GET_TICKET = 1,
    LOGIN = 2,
    HEART_BEAT = 3,
    TERM = 4
} XmlChoose;

typedef enum
{
    CONSOLE_FORMAT = 1,
    FILE_FORMAT = 2
} TimeFormat;

typedef struct
{
    uint8_t* data;
    size_t length;
} bytes_t;

/** @brief WIFI SSID (从 sdkconfig / menuconfig 读取) */
extern char g_wifi_ssid[32];
extern char g_wifi_pass[64];

/**
 * @brief 获取程序运行目录 (ESP32 上返回 SPIFFS 挂载点 /spiffs)
 */
bool get_exec_dir(char* dir_array);

/** @brief XML 解析 */
char* xml_parser(const char* xml_data, const char* tag);

/** @brief 文本转字节 */
bytes_t str2bytes(const char* str);

/** @brief 字符串转 uint64 */
uint64_t str2uint64(const char* str);

/** @brief uint64 转字符串 */
char* uint642str(const uint64_t num);

/** @brief 获取当前时间毫秒 (使用 esp_timer) */
uint64_t get_cur_tm_ms(void);

/** @brief 获取随机字节 (使用 esp_fill_random) */
void get_rand_bytes(uint8_t* buf, size_t len);

/** @brief 睡眠 (使用 vTaskDelay / FreeRTOS) */
void sleep_ms(uint64_t ms, bool can_stop);

/** @brief 获取格式化时间 */
void get_fmt_time(char* buf, TimeFormat fmt);

/** @brief 安全字符串 (非空返回自身, 空返回 "") */
const char* safe_str(const char* str);

/** @brief 创建 XML 认证负载 */
char* create_xml_payload(XmlChoose choose);

/** @brief 提取标签间内容 */
char* extract_between_tags(const char* text, const char* start_tag, const char* end_tag);

/** @brief 清除 CDATA */
char* clean_CDATA(const char* text);

/** @brief 保存配置文件 (SPIFFS) */
bool save_cfg(char* configs_str);

/** @brief 加载配置文件 (SPIFFS) */
bool load_cfg(void);

#endif // ESURFINGCLIENT_PLATFORMUTILS_H
