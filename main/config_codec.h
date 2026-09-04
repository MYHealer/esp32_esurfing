#ifndef CONFIG_CODEC_H
#define CONFIG_CODEC_H

#include <stddef.h>
#include <stdbool.h>

/**
 * @brief 配置数据结构
 *
 * 该头文件不依赖 ESP-IDF，可在宿主机上编译单元测试。
 */
typedef struct {
    char wifi_ssid[32];
    char wifi_password[64];
    char campus_username[64];
    char campus_password[64];
    char channel[16];
} app_config_t;

/**
 * @brief HTML 属性转义，用于把配置值安全地嵌入 value='...'
 *
 * 转义 & < > " ' 五个字符。
 *
 * @param in      输入字符串
 * @param out     输出缓冲区，可为 NULL（仅计算长度）
 * @param out_sz  输出缓冲区大小
 * @return 转义后所需的字节数（不含结尾 \0）。
 *         若返回值 >= out_sz 表示输出被截断，调用方应据此重试。
 *         与 snprintf 语义一致。
 */
size_t html_escape(const char* in, char* out, size_t out_sz);

/**
 * @brief URL 解码（application/x-www-form-urlencoded）
 *
 * '+' → 空格，%XX → 原始字节。非法 % 序列原样保留。
 *
 * @param in      输入字符串（以 \0 结尾）
 * @param out     输出缓冲区，可为 NULL（仅计算长度）
 * @param out_sz  输出缓冲区大小
 * @return 解码后所需的字节数（不含结尾 \0）。返回值 >= out_sz 表示截断。
 */
size_t url_decode(const char* in, char* out, size_t out_sz);

/**
 * @brief 解析 urlencoded 表单体到 app_config_t
 *
 * @param body  表单体，例如 "wifi_ssid=abc&campus_username=123&channel=pc"。
 *              **会被原地修改**（分隔符被替换为 \0）。
 * @param out   输出配置，调用前会被清零
 * @return true 解析成功且 wifi_ssid / campus_username 均非空；
 *         false 缺必填字段（此时 out 仍填充了已解析到的字段，便于回填表单）
 */
bool form_parse(char* body, app_config_t* out);

#endif /* CONFIG_CODEC_H */
