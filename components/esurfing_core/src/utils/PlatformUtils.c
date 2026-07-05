/**
 * @brief ESP32 平台工具函数实现
 *
 * 替换原版 PlatformUtils.c 中的:
 * - get_cur_tm_ms: 使用 esp_timer_get_time()
 * - get_rand_bytes: 使用 esp_fill_random()
 * - sleep_ms: 使用 vTaskDelay()
 * - get_exec_dir: SPIFFS 挂载点
 * - load_cfg / save_cfg: SPIFFS 文件操作
 *
 * 保留:
 * - xml_parser / extract_between_tags / clean_CDATA
 * - create_xml_payload
 * - safe_str / str2bytes / str2uint64 / uint642str
 * - get_fmt_time (ESP32 有 newlib POSIX 时间函数)
 */

#include "utils/PlatformUtils.h"
#include "utils/Logger.h"
#include "utils/cJSON.h"
#include "States.h"
#include "esp_port.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_system.h"

/* ESP32 上 SPIFFS 配置路径 */
#define ESP_CONFIG_DIR  "/spiffs"
#define ESP_CONFIG_FILE "/spiffs/ESurfingClient.json"

static char config_file[PATH_MAX + 1];

static const char s_default_cfg[] = "{\n"
                                    "   \"enabled\": true,\n"
                                    "   \"log_lv\": 4,\n"
                                    "   \"accounts\": [\n"
                                    "       {\n"
                                    "           \"username\": \"\",\n"
                                    "           \"password\": \"\",\n"
                                    "           \"channel\": \"phone\",\n"
                                    "           \"mark\": \"\"\n"
                                    "       }\n"
                                    "   ]\n"
                                    "}\n";

/* ========== 获取执行目录 (SPIFFS) ========== */
bool get_exec_dir(char* dir_array)
{
    if (dir_array)
    {
        snprintf(dir_array, PATH_MAX, "%s", ESP_CONFIG_DIR);
        return true;
    }
    return false;
}

/* ========== XML 解析 ========== */
char* xml_parser(const char* xml_data, const char* tag)
{
    if (xml_data == NULL || tag == NULL) return NULL;

    char start_tag[256];
    snprintf(start_tag, sizeof(start_tag), "<%s>", tag);
    char end_tag[256];
    snprintf(end_tag, sizeof(end_tag), "</%s>", tag);

    const char* start_pos = strstr(xml_data, start_tag);
    if (!start_pos) return NULL;
    start_pos += strlen(start_tag);

    const char* end_pos = strstr(start_pos, end_tag);
    if (!end_pos) return NULL;

    size_t content_length = end_pos - start_pos;
    if (content_length <= 0) return NULL;

    char* content = malloc(content_length + 1);
    if (!content) return NULL;
    memcpy(content, start_pos, content_length);
    content[content_length] = '\0';
    return content;
}

/* ========== 工具函数 ========== */
bytes_t str2bytes(const char* str)
{
    bytes_t ba = {0};
    if (!str) return ba;
    ba.length = strlen(str);
    ba.data = (uint8_t*)malloc(ba.length);
    if (ba.data) memcpy(ba.data, str, ba.length);
    return ba;
}

uint64_t str2uint64(const char* str)
{
    if (!str) return 0;
    while (isspace((unsigned char)*str)) str++;
    if (*str == '\0') return 0;
    char* end_ptr;
    errno = 0;
    uint64_t value = strtoll(str, &end_ptr, 10);
    if (errno == ERANGE) return 0;
    if (end_ptr == str) return 0;
    while (isspace((unsigned char)*end_ptr)) end_ptr++;
    if (*end_ptr != '\0') return 0;
    return value;
}

char* uint642str(const uint64_t num)
{
    char* result = malloc(22);
    if (!result) return NULL;
    snprintf(result, 22, "%" PRIu64, num);
    return result;
}

/* ========== 时间 ========== */
uint64_t get_cur_tm_ms()
{
    /* esp_timer_get_time() 返回微秒 */
    return esp_timer_get_time() / 1000LL;
}

void get_fmt_time(char* buf, const TimeFormat fmt)
{
    time_t raw_tm = time(NULL);
    if (raw_tm == (time_t)-1) return;

    /* ESP32 上 localtime_r 可用 (newlib) */
    struct tm local_tm;
    if (localtime_r(&raw_tm, &local_tm) == NULL) return;

    switch (fmt)
    {
    case CONSOLE_FORMAT:
        strftime(buf, 32, "%Y-%m-%d %H:%M:%S", &local_tm);
        break;
    case FILE_FORMAT:
        strftime(buf, 32, "%Y%m%d-%H%M%S", &local_tm);
        break;
    }
}

/* ========== 随机字节 (ESP32 硬件随机数) ========== */
void get_rand_bytes(uint8_t* buf, const size_t len)
{
    if (!buf || len == 0) return;
    for (size_t i = 0; i < len; i++)
    {
        buf[i] = (uint8_t)esp_random();
    }
}

/* ========== 睡眠 (FreeRTOS vTaskDelay) ========== */
void sleep_ms(const uint64_t ms, const bool can_stop)
{
    if (ms == 0) return;

    if (can_stop)
    {
        uint64_t elapsed = 0;
        const uint64_t SEGMENT_MS = 100;

        while (elapsed < ms && g_thread_keep_alive)
        {
            if (tl_thread_idx >= 0 && tl_thread_idx < g_prog_cnt)
            {
                if (!g_prog_status[tl_thread_idx].runtime_status.is_running ||
                    g_prog_status[tl_thread_idx].runtime_status.is_need_reset)
                {
                    return;
                }
            }
            else
            {
                if (g_need_exit) return;
            }

            uint64_t sleep_time = ms - elapsed;
            if (sleep_time > SEGMENT_MS) sleep_time = SEGMENT_MS;

            vTaskDelay(pdMS_TO_TICKS(sleep_time));
            elapsed += sleep_time;
        }
    }
    else
    {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
}

/* ========== 安全字符串 ========== */
const char* safe_str(const char* str)
{
    return str ? str : "";
}

/* ========== XML 负载生成 ========== */
char* create_xml_payload(const XmlChoose choose)
{
    char cur_tm[32];
    get_fmt_time(cur_tm, CONSOLE_FORMAT);
    static char xml[XML_BUFFER_SIZE] = "";
    uint16_t xml_len = 0;

    switch (choose)
    {
    case GET_TICKET:
        xml_len = snprintf(xml, XML_BUFFER_SIZE,
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<request>\n"
            "    <user-agent>%s</user-agent>\n"
            "    <client-id>%s</client-id>\n"
            "    <local-time>%s</local-time>\n"
            "    <host-name>%s</host-name>\n"
            "    <ipv4>%s</ipv4>\n"
            "    <ipv6></ipv6>\n"
            "    <mac>%s</mac>\n"
            "    <ostag>%s</ostag>\n"
            "    <gwip>%s</gwip>\n"
            "</request>\n",
            safe_str(g_prog_status[tl_thread_idx].login_cfg.user_agent),
            safe_str(g_prog_status[tl_thread_idx].auth_cfg.client_id),
            safe_str(cur_tm),
            safe_str(g_prog_status[tl_thread_idx].auth_cfg.host_name),
            safe_str(g_prog_status[tl_thread_idx].auth_cfg.client_ip),
            safe_str(g_prog_status[tl_thread_idx].auth_cfg.mac_addr),
            safe_str(g_prog_status[tl_thread_idx].auth_cfg.host_name),
            safe_str(g_prog_status[tl_thread_idx].auth_cfg.ac_ip)
        );
        break;
    case LOGIN:
        xml_len = snprintf(xml, XML_BUFFER_SIZE,
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<request>\n"
            "    <user-agent>%s</user-agent>\n"
            "    <client-id>%s</client-id>\n"
            "    <ticket>%s</ticket>\n"
            "    <local-time>%s</local-time>\n"
            "    <userid>%s</userid>\n"
            "    <passwd>%s</passwd>\n"
            "</request>\n",
            safe_str(g_prog_status[tl_thread_idx].login_cfg.user_agent),
            safe_str(g_prog_status[tl_thread_idx].auth_cfg.client_id),
            safe_str(g_prog_status[tl_thread_idx].auth_cfg.ticket),
            safe_str(cur_tm),
            safe_str(g_prog_status[tl_thread_idx].login_cfg.usr),
            safe_str(g_prog_status[tl_thread_idx].login_cfg.pwd)
        );
        break;
    case HEART_BEAT:
    case TERM:
        xml_len = snprintf(xml, XML_BUFFER_SIZE,
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<request>\n"
            "    <user-agent>%s</user-agent>\n"
            "    <client-id>%s</client-id>\n"
            "    <local-time>%s</local-time>\n"
            "    <host-name>%s</host-name>\n"
            "    <ipv4>%s</ipv4>\n"
            "    <ticket>%s</ticket>\n"
            "    <ipv6></ipv6>\n"
            "    <mac>%s</mac>\n"
            "    <ostag>%s</ostag>\n"
            "</request>\n",
            safe_str(g_prog_status[tl_thread_idx].login_cfg.user_agent),
            safe_str(g_prog_status[tl_thread_idx].auth_cfg.client_id),
            safe_str(cur_tm),
            safe_str(g_prog_status[tl_thread_idx].auth_cfg.host_name),
            safe_str(g_prog_status[tl_thread_idx].auth_cfg.client_ip),
            safe_str(g_prog_status[tl_thread_idx].auth_cfg.ticket),
            safe_str(g_prog_status[tl_thread_idx].auth_cfg.mac_addr),
            safe_str(g_prog_status[tl_thread_idx].auth_cfg.host_name)
        );
        break;
    default:
        LOG_ERROR("XML 选择代码错误");
        return NULL;
    }

    if (xml_len <= 0) { LOG_ERROR("XML 创建失败"); return NULL; }
    if (xml_len >= XML_BUFFER_SIZE) { LOG_ERROR("XML 内容过长"); return NULL; }
    return xml;
}

/* ========== 标签提取 ========== */
char* extract_between_tags(const char* text, const char* start_tag, const char* end_tag)
{
    if (!text) { LOG_ERROR("传入文本为空"); return NULL; }
    char* start = strstr(text, start_tag);
    if (!start) { LOG_ERROR("未找到开头标签: %s", start_tag); return NULL; }
    start += strlen(start_tag);
    char* end = strstr(start, end_tag);
    if (!end) { LOG_WARN("未找到结尾标签: %s", end_tag); return NULL; }
    size_t len = end - start;
    char* result = malloc(len + 1);
    if (!result) { LOG_ERROR("分配内存失败"); return NULL; }
    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}

char* clean_CDATA(const char* text)
{
    return extract_between_tags(text, "<![CDATA[", "]]>");
}

/* ========== 配置文件 (SPIFFS) ========== */

bool save_cfg(char* configs_str)
{
    LOG_INFO("保存配置到 %s", config_file);
    FILE* cfg_file = fopen(config_file, "w");
    if (!cfg_file)
    {
        LOG_ERROR("无法打开配置文件: %s", config_file);
        return false;
    }
    fprintf(cfg_file, "%s", configs_str);
    fclose(cfg_file);

    /* 更新全局配置 */
    cJSON* configs = cJSON_Parse(configs_str);
    if (!configs) return false;

    const cJSON* enabled = cJSON_GetObjectItem(configs, "enabled");
    const cJSON* log_lv = cJSON_GetObjectItem(configs, "log_lv");
    const cJSON* accounts = cJSON_GetObjectItem(configs, "accounts");
    const cJSON* account = accounts ? cJSON_GetArrayItem(accounts, 0) : NULL;

    if (enabled) g_prog_enabled = enabled->valueint;
    if (log_lv) set_logger_level((LogLevel)log_lv->valueint);
    if (account)
    {
        const cJSON* username = cJSON_GetObjectItem(account, "username");
        const cJSON* password = cJSON_GetObjectItem(account, "password");
        const cJSON* channel = cJSON_GetObjectItem(account, "channel");
        if (username) snprintf(g_prog_status[0].login_cfg.usr, USR_LEN, "%s", username->valuestring);
        if (password) snprintf(g_prog_status[0].login_cfg.pwd, PWD_LEN, "%s", password->valuestring);
        if (channel)  snprintf(g_prog_status[0].login_cfg.chn, CHN_LEN, "%s", channel->valuestring);
    }

    cJSON_Delete(configs);
    return true;
}

bool load_cfg(void)
{
    /* ESP32 上配置文件固定路径 */
    snprintf(config_file, sizeof(config_file), "%s", ESP_CONFIG_FILE);

    FILE* cfg_file = fopen(config_file, "r");
    if (!cfg_file)
    {
        LOG_ERROR("无法打开配置文件: %s", config_file);
        LOG_INFO("创建默认配置文件, 请填写账号后重启");
        FILE* new_cfg = fopen(config_file, "w");
        if (!new_cfg)
        {
            LOG_FATAL("无法创建配置文件, 请检查 SPIFFS");
            while (true) { if (g_need_exit) return false; vTaskDelay(pdMS_TO_TICKS(10000)); }
        }
        fprintf(new_cfg, "%s", s_default_cfg);
        fclose(new_cfg);
        LOG_INFO("默认配置文件已创建, 请填写账号密码后重启设备");
        while (true) { if (g_need_exit) return false; vTaskDelay(pdMS_TO_TICKS(10000)); }
    }

    fseek(cfg_file, 0, SEEK_END);
    long len = ftell(cfg_file);
    fseek(cfg_file, 0, SEEK_SET);

    char* cfg_data = malloc(len + 1);
    if (!cfg_data) { fclose(cfg_file); return false; }
    fread(cfg_data, 1, len, cfg_file);
    cfg_data[len] = '\0';
    fclose(cfg_file);

    /* 解析 JSON, 完全与原版相同 */
    cJSON* cfg_json = cJSON_Parse(cfg_data);
    free(cfg_data);
    if (!cfg_json)
    {
        LOG_FATAL("JSON 解析失败");
        while (true) { if (g_need_exit) return false; vTaskDelay(pdMS_TO_TICKS(10000)); }
    }

    const cJSON* log_lv = cJSON_GetObjectItem(cfg_json, "log_lv");
    if (log_lv && cJSON_IsNumber(log_lv)) set_logger_level(log_lv->valueint);

    const cJSON* enabled = cJSON_GetObjectItem(cfg_json, "enabled");
    if (!enabled || cJSON_IsFalse(enabled))
    {
        LOG_WARN("程序被禁用, 请修改配置文件后重启");
        cJSON_Delete(cfg_json);
        while (true) { if (g_need_exit) return false; vTaskDelay(pdMS_TO_TICKS(10000)); }
    }
    g_prog_enabled = true;

    const cJSON* accounts = cJSON_GetObjectItem(cfg_json, "accounts");
    if (!accounts || !cJSON_IsArray(accounts) || cJSON_GetArraySize(accounts) == 0)
    {
        LOG_FATAL("没有账号数据");
        cJSON_Delete(cfg_json);
        while (true) { if (g_need_exit) return false; vTaskDelay(pdMS_TO_TICKS(10000)); }
    }

    /* ESP32 只支持单账号 (简化) */
    const cJSON* account = cJSON_GetArrayItem(accounts, 0);
    const cJSON* usr = cJSON_GetObjectItem(account, "username");
    const cJSON* pwd = cJSON_GetObjectItem(account, "password");
    const cJSON* chn = cJSON_GetObjectItem(account, "channel");

    if (!usr || !usr->valuestring[0] || !pwd || !pwd->valuestring[0])
    {
        LOG_FATAL("账号或密码为空");
        cJSON_Delete(cfg_json);
        while (true) { if (g_need_exit) return false; vTaskDelay(pdMS_TO_TICKS(10000)); }
    }

    snprintf(g_prog_status[0].login_cfg.usr, USR_LEN, "%s", usr->valuestring);
    snprintf(g_prog_status[0].login_cfg.pwd, PWD_LEN, "%s", pwd->valuestring);

    if (chn && chn->valuestring[0])
        snprintf(g_prog_status[0].login_cfg.chn, CHN_LEN, "%s", chn->valuestring);
    else
        snprintf(g_prog_status[0].login_cfg.chn, CHN_LEN, "%s", "phone");

    /* 生成 UA */
    if (strcmp(g_prog_status[0].login_cfg.chn, "pc") == 0)
        snprintf(g_prog_status[0].login_cfg.user_agent, USER_AGENT_LEN, "CCTP/Linux64/1003");
    else
        snprintf(g_prog_status[0].login_cfg.user_agent, USER_AGENT_LEN, "CCTP/android64_vpn/2093");

    g_prog_status[0].login_cfg.idx = 1;
    g_prog_cnt = 1;

    cJSON_Delete(cfg_json);
    LOG_INFO("配置加载完成: 用户=%s, 通道=%s", g_prog_status[0].login_cfg.usr, g_prog_status[0].login_cfg.chn);
    return true;
}
