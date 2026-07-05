/**
 * @brief Web 配置后台 - HTTP 服务器 + SPIFFS 配置读写
 */

#include "web_config.h"
#include "wifi_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "cJSON.h"

static const char* TAG = "WEB";

static httpd_handle_t s_server = NULL;

/* ============ HTML 页面模板 ============ */

static const char* HTML_HEAD =
    "<!DOCTYPE html><html lang='zh-CN'><head>"
    "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>ESurfing ESP32 配置</title>"
    "<style>"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:-apple-system,sans-serif;background:#f5f5f5;padding:20px;color:#333}"
    "h1{font-size:20px;margin-bottom:20px;text-align:center}"
    ".card{background:#fff;border-radius:12px;padding:20px;margin-bottom:16px;box-shadow:0 1px 4px rgba(0,0,0,.1)}"
    "h2{font-size:15px;margin-bottom:12px;color:#666}"
    "label{font-size:13px;color:#888;display:block;margin-bottom:4px}"
    "input{width:100%;padding:10px 12px;border:1px solid #ddd;border-radius:8px;font-size:14px;margin-bottom:12px}"
    "input:focus{outline:none;border-color:#007aff;box-shadow:0 0 0 2px rgba(0,122,255,.2)}"
    ".btn{width:100%;padding:12px;border:none;border-radius:8px;font-size:15px;font-weight:600;cursor:pointer}"
    ".btn-primary{background:#007aff;color:#fff}"
    ".btn-primary:active{background:#005bbf}"
    ".btn-warning{background:#ff9500;color:#fff}"
    ".btn-warning:active{background:#cc7700}"
    ".status{text-align:center;font-size:13px;color:#888;margin-top:12px}"
    ".status.ok{color:#34c759}"
    ".status.err{color:#ff3b30}"
    "hr{border:none;border-top:1px solid #eee;margin:16px 0}"
    "</style></head><body>"
    "<h1>🔧 ESurfing ESP32</h1>";

static const char* HTML_FOOT =
    "<div class='card'><h2>操作</h2>"
    "<button class='btn btn-warning' onclick='fetch(\"/restart\",{method:\"POST\"}).then(()=>alert(\"重启中...\"))'>重启设备</button>"
    "<div class='status' id='status'>状态: <span id='status_text'>检查中...</span></div>"
    "</div>"
    "<script>"
    "fetch('/status').then(r=>r.json()).then(d=>{"
    "  let s=document.getElementById('status_text');"
    "  if(d.connected){s.textContent='已连接 '+d.ssid+' ('+d.ip+')';s.className='ok';}"
    "  else{s.textContent='WiFi 未连接';s.className='err';}"
    "}).catch(()=>{document.getElementById('status_text').textContent='无法获取状态'})"
    "</script></body></html>";

/* ============ 配置读写 ============ */

bool load_config(app_config_t* cfg)
{
    memset(cfg, 0, sizeof(*cfg));

    FILE* f = fopen("/spiffs/config.json", "r");
    if (!f) {
        ESP_LOGI(TAG, "config.json 不存在，需首次配置");
        return false;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return false; }

    char* json = malloc(len + 1);
    fread(json, 1, len, f);
    json[len] = 0;
    fclose(f);

    cJSON* root = cJSON_Parse(json);
    free(json);
    if (!root) return false;

    cJSON* item;
    if ((item = cJSON_GetObjectItem(root, "wifi_ssid")) && cJSON_IsString(item))
        strncpy(cfg->wifi_ssid, item->valuestring, sizeof(cfg->wifi_ssid) - 1);
    if ((item = cJSON_GetObjectItem(root, "wifi_password")) && cJSON_IsString(item))
        strncpy(cfg->wifi_password, item->valuestring, sizeof(cfg->wifi_password) - 1);
    if ((item = cJSON_GetObjectItem(root, "campus_username")) && cJSON_IsString(item))
        strncpy(cfg->campus_username, item->valuestring, sizeof(cfg->campus_username) - 1);
    if ((item = cJSON_GetObjectItem(root, "campus_password")) && cJSON_IsString(item))
        strncpy(cfg->campus_password, item->valuestring, sizeof(cfg->campus_password) - 1);
    if ((item = cJSON_GetObjectItem(root, "channel")) && cJSON_IsString(item))
        strncpy(cfg->channel, item->valuestring, sizeof(cfg->channel) - 1);

    cJSON_Delete(root);

    if (cfg->wifi_ssid[0] == 0 || cfg->campus_username[0] == 0)
        return false;

    return true;
}

bool save_config(const app_config_t* cfg)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "wifi_ssid", cfg->wifi_ssid);
    cJSON_AddStringToObject(root, "wifi_password", cfg->wifi_password);
    cJSON_AddStringToObject(root, "campus_username", cfg->campus_username);
    cJSON_AddStringToObject(root, "campus_password", cfg->campus_password);
    cJSON_AddStringToObject(root, "channel", cfg->channel);

    char* json = cJSON_Print(root);
    cJSON_Delete(root);

    FILE* f = fopen("/spiffs/config.json", "w");
    if (!f) { free(json); return false; }
    fwrite(json, 1, strlen(json), f);
    fclose(f);
    free(json);
    return true;
}

/* ============ HTTP 处理器 ============ */

static esp_err_t root_get_handler(httpd_req_t* req)
{
    app_config_t cfg = {0};
    load_config(&cfg);

    const char* phone_sel = (strcmp(cfg.channel, "pc") == 0) ? "" : " selected";
    const char* pc_sel = (strcmp(cfg.channel, "pc") == 0) ? " selected" : "";

    char* html = NULL;
    size_t len = asprintf(&html,
        "%s"
        "<div class='card'><h2>WiFi 设置</h2>"
        "<form id='config' action='/save' method='POST'>"
        "<label>WiFi SSID</label><input name='wifi_ssid' value='%s' placeholder='校园网 WiFi 名称'>"
        "<label>WiFi 密码</label><input name='wifi_password' type='password' value='%s' placeholder='WiFi 密码（留空为开放网络）'>"
        "<hr><h2>校园网账号</h2>"
        "<label>用户名</label><input name='campus_username' value='%s' placeholder='学号/手机号'>"
        "<label>密码</label><input name='campus_password' type='password' value='%s' placeholder='密码'>"
        "<label>通道</label>"
        "<select name='channel' style='width:100%%;padding:10px 12px;border:1px solid #ddd;border-radius:8px;font-size:14px;margin-bottom:12px;background:#fff'>"
        "<option value='phone'%s>手机 (phone)</option>"
        "<option value='pc'%s>电脑 (pc)</option>"
        "</select>"
        "<button type='submit' class='btn btn-primary'>保存并重启</button>"
        "</form></div>"
        "%s",
        HTML_HEAD,
        cfg.wifi_ssid, cfg.wifi_password,
        cfg.campus_username, cfg.campus_password,
        phone_sel, pc_sel,
        HTML_FOOT);

    if (html) {
        httpd_resp_set_type(req, "text/html; charset=utf-8");
        httpd_resp_send(req, html, len);
        free(html);
    }
    return ESP_OK;
}

static esp_err_t save_post_handler(httpd_req_t* req)
{
    char content[512] = {0};
    int ret, remaining = req->content_len;
    if (remaining >= (int)sizeof(content)) remaining = sizeof(content) - 1;
    if (remaining > 0) {
        ret = httpd_req_recv(req, content, remaining);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "读取失败");
            return ESP_FAIL;
        }
        content[ret] = 0;
    }

    /* 解析表单数据 */
    app_config_t cfg = {0};
    strncpy(cfg.channel, "phone", sizeof(cfg.channel) - 1);

    char* key = content;
    char* val;
    while (key && *key) {
        char* next = strchr(key, '&');
        if (next) { *next = 0; next++; }
        val = strchr(key, '=');
        if (val) {
            *val = 0; val++;
            /* URL 解码 (简化: 只处理 %2C 和 +) */
            char dec[128];
            int di = 0;
            for (int i = 0; val[i] && di < 126; i++) {
                if (val[i] == '+') dec[di++] = ' ';
                else if (val[i] == '%' && val[i+1] && val[i+2]) {
                    char hex[3] = {val[i+1], val[i+2], 0};
                    dec[di++] = strtol(hex, NULL, 16);
                    i += 2;
                } else dec[di++] = val[i];
            }
            dec[di] = 0;

            if (strcmp(key, "wifi_ssid") == 0)
                strncpy(cfg.wifi_ssid, dec, sizeof(cfg.wifi_ssid) - 1);
            else if (strcmp(key, "wifi_password") == 0)
                strncpy(cfg.wifi_password, dec, sizeof(cfg.wifi_password) - 1);
            else if (strcmp(key, "campus_username") == 0)
                strncpy(cfg.campus_username, dec, sizeof(cfg.campus_username) - 1);
            else if (strcmp(key, "campus_password") == 0)
                strncpy(cfg.campus_password, dec, sizeof(cfg.campus_password) - 1);
            else if (strcmp(key, "channel") == 0)
                strncpy(cfg.channel, dec, sizeof(cfg.channel) - 1);
        }
        key = next;
    }

    if (cfg.wifi_ssid[0] == 0 || cfg.campus_username[0] == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "WiFi SSID 和校园网账号不能为空");
        return ESP_FAIL;
    }

    save_config(&cfg);

    /* 同步写入 ESurfingClient.json (认证代码用) */
    {
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "enabled", true);
        cJSON_AddNumberToObject(root, "log_lv", 4);
        cJSON* accounts = cJSON_AddArrayToObject(root, "accounts");
        cJSON* acct = cJSON_CreateObject();
        cJSON_AddStringToObject(acct, "username", cfg.campus_username);
        cJSON_AddStringToObject(acct, "password", cfg.campus_password);
        cJSON_AddStringToObject(acct, "channel", cfg.channel[0] ? cfg.channel : "phone");
        cJSON_AddStringToObject(acct, "mark", "");
        cJSON_AddItemToArray(accounts, acct);
        char* json = cJSON_Print(root);
        cJSON_Delete(root);
        FILE* f = fopen("/spiffs/ESurfingClient.json", "w");
        if (f) { fwrite(json, 1, strlen(json), f); fclose(f); }
        free(json);
    }

    /* 返回成功页面并自动重启 */
    const char* resp = "<html><head><meta charset='utf-8'><meta http-equiv='refresh' content='3'></head>"
        "<body style='font-family:sans-serif;padding:40px;text-align:center'>"
        "<h2>已保存</h2><p>设备正在重启...</p></body></html>";
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, resp, strlen(resp));

    /* 延迟重启 */
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}

static esp_err_t restart_post_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "重启中...");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

static esp_err_t status_get_handler(httpd_req_t* req)
{
    char sta_ip[16] = "0.0.0.0";
    char sta_ssid[33] = "";
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("STA_DEF");
    if (netif) {
        esp_netif_ip_info_t ip;
        if (esp_netif_get_ip_info(netif, &ip) == ESP_OK)
            snprintf(sta_ip, sizeof(sta_ip), IPSTR, IP2STR(&ip.ip));
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK)
            memcpy(sta_ssid, ap.ssid, sizeof(ap.ssid));
    }
    char json[256];
    snprintf(json, sizeof(json),
        "{\"connected\":%s,\"ip\":\"%s\",\"ssid\":\"%s\"}",
        wifi_is_connected() ? "true" : "false", sta_ip, sta_ssid);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    return ESP_OK;
}

/* ============ 启动/停止 ============ */

esp_err_t web_config_start(void)
{
    if (s_server) return ESP_OK;

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.max_uri_handlers = 8;
    cfg.stack_size = 4096;

    httpd_uri_t uri_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
    };
    httpd_uri_t uri_save = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = save_post_handler,
    };
    httpd_uri_t uri_restart = {
        .uri = "/restart",
        .method = HTTP_POST,
        .handler = restart_post_handler,
    };
    httpd_uri_t uri_status = {
        .uri = "/status",
        .method = HTTP_GET,
        .handler = status_get_handler,
    };

    if (httpd_start(&s_server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "HTTP 服务器启动失败");
        return ESP_FAIL;
    }

    httpd_register_uri_handler(s_server, &uri_root);
    httpd_register_uri_handler(s_server, &uri_save);
    httpd_register_uri_handler(s_server, &uri_restart);
    httpd_register_uri_handler(s_server, &uri_status);

    ESP_LOGI(TAG, "Web 配置后台: http://192.168.4.1");
    return ESP_OK;
}

esp_err_t web_config_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    return ESP_OK;
}

const char* web_config_get_status(void)
{
    return wifi_is_connected() ? "WiFi 已连接" : "WiFi 未连接";
}
