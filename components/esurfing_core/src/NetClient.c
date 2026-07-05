#include "utils/PlatformUtils.h"
#include "utils/Logger.h"
#include "NetClient.h"
#include "States.h"
#include "esp_port.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "esp_http_client.h"
#include "esp_err.h"
#include "esp_rom_md5.h"

#define MAX_LEN 128
#define SCHOOL_ID_LENGTH 8
#define DOMAIN_LENGTH 16
#define AREA_LENGTH 8
#define POST_TIMEOUT_MS 10000

static const char s_generate_url[] = "http://www.msftconnecttest.com/connecttest.txt";
static const char s_backup_generate_url[] = "http://connect.rom.miui.com/generate_204";

static char s_school_id[SCHOOL_ID_LENGTH];
static char s_domain[DOMAIN_LENGTH];
static char s_area[AREA_LENGTH];

static bool s_headers_parsed = false;

void reset_net_client_state(void)
{
    memset(s_school_id, 0, sizeof(s_school_id));
    memset(s_domain, 0, sizeof(s_domain));
    memset(s_area, 0, sizeof(s_area));
    s_headers_parsed = false;
}

char* extract_url_param(const char* url, const char* search_str_start)
{
    if (url == NULL)
    {
        LOG_ERROR("URL 为空");
        return NULL;
    }
    const size_t len = strlen(search_str_start);
    char* search_pattern = malloc(len + 2);
    if (search_pattern == NULL)
    {
        LOG_ERROR("分配内存失败");
        return NULL;
    }
    snprintf(search_pattern, len + 2, "%s=", search_str_start);
    char* result = extract_between_tags(url, search_pattern, "&");
    free(search_pattern);
    return result;
}

/* ---- MD5 using ESP ROM API ---- */
static char* calc_md5(const char* data)
{
    unsigned char digest[16];
    char* md5_str = malloc(33);
    if (!md5_str) return NULL;

    md5_context_t ctx;
    esp_rom_md5_init(&ctx);
    esp_rom_md5_update(&ctx, data, strlen(data));
    esp_rom_md5_final(digest, &ctx);

    for (int i = 0; i < 16; i++)
        sprintf(&md5_str[i*2], "%02x", (unsigned int)digest[i]);
    md5_str[32] = '\0';
    return md5_str;
}

/* ---- 替换 curl 写回调 ---- */
typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} write_buf_t;

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
    {
        write_buf_t* buf = (write_buf_t*)evt->user_data;
        if (buf && evt->data && evt->data_len > 0)
        {
            size_t needed = buf->size + evt->data_len + 1;
            if (needed > buf->capacity)
            {
                buf->capacity = needed + 1024;
                char* newp = realloc(buf->data, buf->capacity);
                if (!newp) return ESP_FAIL;
                buf->data = newp;
            }
            memcpy(buf->data + buf->size, evt->data, evt->data_len);
            buf->size += evt->data_len;
            buf->data[buf->size] = '\0';
        }
        break;
    }
    case HTTP_EVENT_ON_HEADER:
    {
        if (!s_headers_parsed && evt->header_key && evt->header_value)
        {
            if (strcasecmp(evt->header_key, "schoolid") == 0 && !s_school_id[0])
            {
                size_t vlen = strlen(evt->header_value);
                size_t copy_len = vlen < SCHOOL_ID_LENGTH - 1 ? vlen : SCHOOL_ID_LENGTH - 1;
                memcpy(s_school_id, evt->header_value, copy_len);
                s_school_id[copy_len] = '\0';
                LOG_INFO("School Id: %s", s_school_id);
            }
            if (strcasecmp(evt->header_key, "domain") == 0 && !s_domain[0])
            {
                size_t vlen = strlen(evt->header_value);
                size_t copy_len = vlen < DOMAIN_LENGTH - 1 ? vlen : DOMAIN_LENGTH - 1;
                memcpy(s_domain, evt->header_value, copy_len);
                s_domain[copy_len] = '\0';
                LOG_INFO("Domain: %s", s_domain);
            }
            if (strcasecmp(evt->header_key, "area") == 0 && !s_area[0])
            {
                size_t vlen = strlen(evt->header_value);
                size_t copy_len = vlen < AREA_LENGTH - 1 ? vlen : AREA_LENGTH - 1;
                memcpy(s_area, evt->header_value, copy_len);
                s_area[copy_len] = '\0';
                LOG_INFO("Area: %s", s_area);
            }
            if (strcasecmp(evt->header_key, "Location") == 0)
            {
                if (tl_thread_idx >= 0 && tl_thread_idx < g_prog_cnt)
                {
                    if (!g_prog_status[tl_thread_idx].last_location_lock)
                    {
                        size_t vlen = strlen(evt->header_value);
                        size_t copy_len = vlen < LAST_LOCATION_LEN - 1 ? vlen : LAST_LOCATION_LEN - 1;
                        memcpy(g_prog_status[tl_thread_idx].last_location, evt->header_value, copy_len);
                        g_prog_status[tl_thread_idx].last_location[copy_len] = '\0';
                        LOG_INFO("Location: %s", g_prog_status[tl_thread_idx].last_location);
                    }
                }
            }
        }
        break;
    }
    default:
        break;
    }
    return ESP_OK;
}

static int _do_http_request(const char* url, const char* post_data, http_resp_t* resp)
{
    write_buf_t buf = {0};
    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = _http_event_handler,
        .user_data = &buf,
        .timeout_ms = POST_TIMEOUT_MS,
        .keep_alive_enable = false,
        .skip_cert_common_name_check = true,
        .method = post_data ? HTTP_METHOD_POST : HTTP_METHOD_GET,
        .max_redirection_count = 0,  /* 不自动跟随重定向，与原始 libcurl 行为一致 */
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client)
    {
        LOG_ERROR("esp_http_client 初始化失败");
        resp->status = REQUEST_INIT_ERROR;
        return -1;
    }

    /* 强制 HTTP/1.0 避免 chunked 编码问题 */
    esp_http_client_set_header(client, "Connection", "close");
    esp_http_client_set_header(client, "Accept-Encoding", "identity");

    /* 计算 MD5 Checksum */
    char md5_hash_str[MAX_LEN] = {0};
    char ua[MAX_LEN] = {0};
    char c_id[MAX_LEN] = {0};
    char a_id[MAX_LEN] = {0};
    char cdc_sid[MAX_LEN] = {0};
    char cdc_d[MAX_LEN] = {0};
    char cdc_a[MAX_LEN] = {0};

    if (post_data)
    {
        char* md5_hash = calc_md5(post_data);
        if (md5_hash)
        {
            snprintf(md5_hash_str, MAX_LEN, "%s", safe_str(md5_hash));
            free(md5_hash);
        }
    }

    if (tl_thread_idx >= 0 && tl_thread_idx < g_prog_cnt)
    {
        snprintf(ua, MAX_LEN, "%s", safe_str(g_prog_status[tl_thread_idx].login_cfg.user_agent));
        snprintf(c_id, MAX_LEN, "%s", safe_str(g_prog_status[tl_thread_idx].auth_cfg.client_id));
        snprintf(a_id, MAX_LEN, "%s", safe_str(g_prog_status[tl_thread_idx].auth_cfg.algo_id));
    }
    snprintf(cdc_sid, MAX_LEN, "%s", safe_str(s_school_id));
    snprintf(cdc_d, MAX_LEN, "%s", safe_str(s_domain));
    snprintf(cdc_a, MAX_LEN, "%s", safe_str(s_area));

    /* 设置请求头 */
    if (md5_hash_str[0])
        esp_http_client_set_header(client, "CDC-Checksum", md5_hash_str);
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    if (ua[0])
        esp_http_client_set_header(client, "User-Agent", ua);
    esp_http_client_set_header(client, "Accept", "text/html,text/xml,application/xhtml+xml,application/x-javascript,*/*");
    if (c_id[0])
        esp_http_client_set_header(client, "Client-ID", c_id);
    if (a_id[0])
        esp_http_client_set_header(client, "Algo-ID", a_id);
    if (cdc_sid[0])
        esp_http_client_set_header(client, "CDC-SchoolId", cdc_sid);
    if (cdc_d[0])
        esp_http_client_set_header(client, "CDC-Domain", cdc_d);
    if (cdc_a[0])
        esp_http_client_set_header(client, "CDC-Area", cdc_a);

    /* 设置请求方式 */
    if (post_data)
    {
        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_post_field(client, post_data, strlen(post_data));
    }
    else
    {
        esp_http_client_set_method(client, HTTP_METHOD_GET);
    }

    /* 用 perform 执行 (GET/POST 统一) */
    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    if (err != ESP_OK)
        LOG_WARN("perform: %s, status=%d, body=%d bytes", esp_err_to_name(err), status_code, buf.size);

    if (buf.data && buf.size > 0) { resp->body_data = buf.data; resp->body_size = buf.size; }
    else { resp->body_data = NULL; resp->body_size = 0; if (buf.data) free(buf.data); }
    esp_http_client_cleanup(client);

    switch (status_code)
    {
    case 302: resp->status = REQUEST_REDIRECT; break;
    case 200: resp->status = REQUEST_HAVE_RES; break;
    case 204: resp->status = REQUEST_SUCCESS; break;
    default: LOG_ERROR("HTTP 响应码: %d", status_code); resp->status = REQUEST_ERROR; break;
    }
    return status_code;
}

/* ============ 公开 API ============ */

http_resp_t post(const char* url, const char* data)
{
    LOG_VERBOSE("POST %s", url);
    LOG_VERBOSE("POST 数据: %s", data);

    http_resp_t resp = {0};
    s_headers_parsed = false;
    _do_http_request(url, data, &resp);

    if (resp.status == REQUEST_HAVE_RES)
        LOG_DEBUG("POST 响应: %d bytes", resp.body_size);

    return resp;
}

http_resp_t get(const char* url)
{
    LOG_VERBOSE("GET %s", url);

    http_resp_t resp = {0};
    s_headers_parsed = false;
    _do_http_request(url, NULL, &resp);
    LOG_INFO("GET result: status=%d, body=%p, body_size=%d", resp.status, resp.body_data, resp.body_size);
    return resp;
}

NetworkStatus check_network_status(void)
{
    http_resp_t resp = get(s_generate_url);

    /* REQUEST_WARN = 响应不完整但 headers 已收到 (captive portal 截断) */
    if (resp.status == REQUEST_WARN)
    {
        LOG_INFO("check_net: REQUEST_WARN -> REQUEST_REDIRECT (captive portal)");
        if (resp.body_data) free(resp.body_data);
        return REQUEST_REDIRECT;
    }

    if (resp.body_data == NULL || resp.status == REQUEST_ERROR)
    {
        /* DNS/socket 错误，用备用地址检测 */
        LOG_WARN("DNS 解析/socket 错误, 用备用地址检测连接");
        if (resp.body_data) free(resp.body_data);

        http_resp_t backup = {0};
        _do_http_request(s_backup_generate_url, NULL, &backup);
        if (backup.body_data) free(backup.body_data);

        if (backup.status == REQUEST_WARN || backup.status == REQUEST_ERROR)
            return REQUEST_REDIRECT;
        return backup.status;
    }

    NetworkStatus st = resp.status;
    if (resp.body_data) free(resp.body_data);
    return st;
}

static void get_school_ip_symbol(void)
{
    const char* school_ip = extract_url_param(g_prog_status[0].last_location, "wlanuserip");
    if (!school_ip) return;
    const char* first_dot = strchr(school_ip, '.');
    if (!first_dot) { free((void*)school_ip); return; }
    const char* second_dot = strchr(first_dot + 1, '.');
    if (!second_dot) { free((void*)school_ip); return; }

    char tmp[64];
    size_t len = second_dot - school_ip;
    if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
    memcpy(tmp, school_ip, len);
    tmp[len] = '\0';

    memcpy(g_school_network_symbol, tmp, len < SCHOOL_NETWORK_SYMBOL ? len : SCHOOL_NETWORK_SYMBOL - 1);
    g_school_network_symbol[len < SCHOOL_NETWORK_SYMBOL ? len : SCHOOL_NETWORK_SYMBOL - 1] = '\0';
    LOG_INFO("校园网标志: %s", g_school_network_symbol);
    free((void*)school_ip);
}

NetworkStatus get_last_location(void)
{
    http_resp_t resp = {0};
    uint8_t retry = 1;

    do
    {
        if (resp.body_data) { free(resp.body_data); resp.body_data = NULL; resp.body_size = 0; }
        resp = get(s_generate_url);
        LOG_INFO("get_last_location: get() returned status=%d, body=%p", resp.status, resp.body_data);

        if ((resp.body_data == NULL || resp.status == REQUEST_ERROR) && resp.status != REQUEST_WARN)
        {
            if (resp.body_data) free(resp.body_data);
            http_resp_t backup = {0};
            _do_http_request(s_backup_generate_url, NULL, &backup);
            if (backup.body_data) free(backup.body_data);

            if (backup.status == REQUEST_WARN || backup.status == REQUEST_ERROR)
                resp.status = REQUEST_SUCCESS;
            else
                resp.status = backup.status;
        }

        switch (resp.status)
        {
        case REQUEST_REDIRECT:
        case REQUEST_WARN:  /* incomplete data 但 Location 头已捕获 */
            break;
        case REQUEST_SUCCESS:
            retry = 1;
            LOG_INFO("已连接互联网, 等待重定向...");
            sleep_ms(10000, true);
            break;
        default:
            if (retry > 5)
            {
                LOG_FATAL("超过重试次数");
                if (resp.body_data) free(resp.body_data);
                return REQUEST_ERROR;
            }
            LOG_WARN("非重定向状态=%d, 重试 %d/5", resp.status, retry);
            retry++;
            sleep_ms(1000, true);
            break;
        }
    } while (resp.status != REQUEST_REDIRECT &&
             !(resp.status == REQUEST_WARN && g_prog_status[tl_thread_idx].last_location[0]));

    if (resp.status != REQUEST_REDIRECT && resp.status != REQUEST_WARN)
    {
        LOG_ERROR("无法获取重定向 URL (status=%d)", resp.status);
        if (resp.body_data) free(resp.body_data);
        return REQUEST_ERROR;
    }

    /* 跟随 302 重定向, 直到拿到 portal 页面 */
    uint8_t redirect_count = 0;
    while (resp.status == REQUEST_REDIRECT || resp.status == REQUEST_WARN)
    {
        if (++redirect_count > 10) { LOG_ERROR("重定向循环超过 10 次"); break; }
        LOG_INFO("跟随重定向 #%d -> %s", redirect_count, safe_str(g_prog_status[tl_thread_idx].last_location));
        if (resp.body_data) { free(resp.body_data); resp.body_data = NULL; }
        resp = get(g_prog_status[tl_thread_idx].last_location);
        LOG_INFO("重定向结果: status=%d, body_size=%d", resp.status, resp.body_size);
        if (resp.body_data && resp.body_size > 0)
            LOG_INFO("Body 前 200 字节: %.200s", resp.body_data);
    }

    g_prog_status[tl_thread_idx].last_location_lock = true;
    LOG_DEBUG("配置 %" PRIu8 " 获取认证配置 URL: %s",
              g_prog_status[tl_thread_idx].login_cfg.idx,
              g_prog_status[tl_thread_idx].last_location);

    get_school_ip_symbol();

    if (resp.body_data) free(resp.body_data);
    return REQUEST_REDIRECT;
}
