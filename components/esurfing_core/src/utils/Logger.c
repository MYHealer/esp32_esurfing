/**
 * @brief ESP32 日志实现
 *
 * 使用 ESP_LOG 宏输出到 UART.
 * SPIFFS 文件日志, 行数轮转 (保留 1 个历史文件).
 * 为兼容原版 LOG_INFO/LOG_DEBUG 宏, 内部转发到 ESP_LOG.
 */

#include "utils/Logger.h"
#include "esp_log.h"
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

#define LOG_MAX_LINES    5000   /* 单文件最大行数 */
#define LOG_FILE_CUR     "/spiffs/run.log"
#define LOG_FILE_OLD     "/spiffs/run.log.1"

static log_cfg_t g_log_cfg = {
    .lv = LOG_LEVEL_INFO,
    .log_dir = "/spiffs",
    .log_file = LOG_FILE_CUR,
    .file_handle = NULL,
    .max_lines = LOG_MAX_LINES,
    .cur_lines = 0
};

/* 映射到 ESP_LOG 等级 */
static const char* LOG_TAG = "ESURF";

static void rotate_log(void)
{
    if (!g_log_cfg.file_handle) return;
    fclose(g_log_cfg.file_handle);
    g_log_cfg.file_handle = NULL;

    /* 删除旧文件, 当前文件改名 */
    unlink(LOG_FILE_OLD);
    rename(LOG_FILE_CUR, LOG_FILE_OLD);

    g_log_cfg.file_handle = fopen(LOG_FILE_CUR, "a");
    g_log_cfg.cur_lines = 0;
    if (!g_log_cfg.file_handle)
        ESP_LOGW(LOG_TAG, "日志轮转后文件打开失败, 仅 UART");
}

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
    g_log_cfg.log_file[0] = '\0';
    strncat(g_log_cfg.log_file, LOG_FILE_CUR, sizeof(g_log_cfg.log_file) - 1);

    /* 尝试打开日志文件 */
    g_log_cfg.file_handle = fopen(g_log_cfg.log_file, "a");
    if (!g_log_cfg.file_handle)
    {
        ESP_LOGI(LOG_TAG, "日志文件不可用, 仅输出到 UART");
    }
    else
    {
        /* 估算当前行数 (按文件大小粗略) */
        fseek(g_log_cfg.file_handle, 0, SEEK_END);
        long sz = ftell(g_log_cfg.file_handle);
        g_log_cfg.cur_lines = (uint32_t)(sz / 80); /* 平均每行~80字节 */
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
        if (g_log_cfg.cur_lines >= g_log_cfg.max_lines)
            rotate_log();

        if (g_log_cfg.file_handle)
        {
            char time_buf[32];
            time_t raw = time(NULL);
            struct tm lt;
            if (localtime_r(&raw, &lt))
                strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &lt);
            else
                snprintf(time_buf, sizeof(time_buf), "----/--/-- --:--:--");

            fprintf(g_log_cfg.file_handle, "[%s] [%s:%lu] %s\n", time_buf, file, (unsigned long)line, msg);
            fflush(g_log_cfg.file_handle);
            g_log_cfg.cur_lines++;
        }
    }
}
