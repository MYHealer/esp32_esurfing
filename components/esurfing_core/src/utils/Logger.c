/**
 * @brief ESP32 日志实现
 *
 * 使用 ESP_LOG 宏输出到 UART.
 * 可选 SPIFFS 文件日志 (简化版, 不轮转).
 * 为兼容原版 LOG_INFO/LOG_DEBUG 宏, 内部转发到 ESP_LOG.
 */

#include "utils/Logger.h"
#include "esp_log.h"
#include <stdarg.h>
#include <time.h>

static log_cfg_t g_log_cfg = {
    .lv = LOG_LEVEL_INFO,
    .log_dir = "/spiffs",
    .log_file = "/spiffs/run.log",
    .file_handle = NULL,
    .max_lines = 10000,
    .cur_lines = 0
};

/* 映射到 ESP_LOG 等级 */
static const char* LOG_TAG = "ESURF";

LogLevel get_logger_level(void)
{
    return g_log_cfg.lv;
}

void set_logger_level(LogLevel lv)
{
    g_log_cfg.lv = lv;
}

bool init_logger(void)
{
    g_log_cfg.lv = LOG_LEVEL_INFO;
    g_log_cfg.cur_lines = 0;
    g_log_cfg.file_handle = NULL;

    /* 尝试打开日志文件 (可选) */
    g_log_cfg.file_handle = fopen(g_log_cfg.log_file, "a");
    if (!g_log_cfg.file_handle)
    {
        ESP_LOGI(LOG_TAG, "日志文件不可用, 仅输出到 UART");
    }

    ESP_LOGI(LOG_TAG, "日志系统初始化完成, 等级: %d", g_log_cfg.lv);
    return true;
}

void clean_logger(void)
{
    if (g_log_cfg.file_handle)
    {
        fclose(g_log_cfg.file_handle);
        g_log_cfg.file_handle = NULL;
    }
}

void log_out(LogLevel level, const char* file, uint32_t line, const char* fmt, ...)
{
    if (level > g_log_cfg.lv) return;

    /* 格式化消息 */
    char msg[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    /* 输出到 ESP_LOG */
    switch (level)
    {
    case LOG_LEVEL_FATAL:
    case LOG_LEVEL_ERROR:
        ESP_LOGE(LOG_TAG, "[%s:%lu] %s", file, (unsigned long)line, msg);
        break;
    case LOG_LEVEL_WARN:
        ESP_LOGW(LOG_TAG, "[%s:%lu] %s", file, (unsigned long)line, msg);
        break;
    case LOG_LEVEL_INFO:
        ESP_LOGI(LOG_TAG, "[%s:%lu] %s", file, (unsigned long)line, msg);
        break;
    case LOG_LEVEL_DEBUG:
    case LOG_LEVEL_VERBOSE:
        ESP_LOGD(LOG_TAG, "[%s:%lu] %s", file, (unsigned long)line, msg);
        break;
    default:
        ESP_LOGI(LOG_TAG, "[%s:%lu] %s", file, (unsigned long)line, msg);
        break;
    }

    /* 可选: 写入日志文件 */
    if (g_log_cfg.file_handle)
    {
        char time_buf[32];
        time_t raw = time(NULL);
        struct tm lt;
        if (localtime_r(&raw, &lt))
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &lt);
        else
            snprintf(time_buf, sizeof(time_buf), "\?\?\?\?-\?\?-?? ??:??:??");

        fprintf(g_log_cfg.file_handle, "[%s] [%s:%lu] %s\n", time_buf, file, (unsigned long)line, msg);
        fflush(g_log_cfg.file_handle);
        g_log_cfg.cur_lines++;
    }
}
