/**
 * ESurfingClient - ESP32-C3 SuperMini
 * 校园网天翼宽带自动认证客户端
 * AP 常开 (192.168.4.1) 供 Web 配置
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "esp_err.h"
#include "esp_sntp.h"

#include "wifi_manager.h"
#include "web_config.h"
#include "DialerClient.h"

static const char* TAG = "MAIN";

static void init_sntp(void)
{
    ESP_LOGI(TAG, "初始化 SNTP...");
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "pool.ntp.org");
    esp_sntp_init();
    /* 等待时间同步 (最多5秒) */
    for (int i = 0; i < 50 && esp_sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED; i++)
        vTaskDelay(pdMS_TO_TICKS(100));
    time_t now = time(NULL);
    if (now > 1700000000)
        ESP_LOGI(TAG, "NTP 同步完成: %lld", (long long)now);
    else
        ESP_LOGW(TAG, "NTP 同步超时, 时间可能不准");
}

static esp_err_t init_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "spiffs",
        .max_files = 5,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS 挂载失败: %s", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0, used = 0;
    esp_spiffs_info("spiffs", &total, &used);
    ESP_LOGI(TAG, "SPIFFS: total=%zu, used=%zu", total, used);
    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESurfingClient ESP32 ===");

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* SPIFFS */
    if (init_spiffs() != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS 初始化失败");
        return;
    }

    /* WiFi: AP 常开 (ESurfing-Config) + STA */
    if (wifi_init() != ESP_OK) {
        ESP_LOGE(TAG, "WiFi 初始化失败");
        return;
    }

    /* Web 配置后台 */
    web_config_start();

    /* 加载配置并连接 STA */
    app_config_t cfg;
    if (load_config(&cfg)) {
        ESP_LOGI(TAG, "连接 WiFi: %s", cfg.wifi_ssid);
        wifi_connect(cfg.wifi_ssid, cfg.wifi_password);
    } else {
        ESP_LOGW(TAG, "无配置，请连 AP: ESurfing-Config → http://192.168.4.1");
    }

    /* 等待 STA 连接并启动认证 */
    if (wifi_wait_connected(60000)) {
        /* WiFi 已连通, 先同步时间再认证 */
        init_sntp();
        ESP_LOGI(TAG, "STA 已连接，启动认证...");
        work();
    } else {
        ESP_LOGW(TAG, "STA 未连接，AP 仍可用");
    }

    while (1) { vTaskDelay(pdMS_TO_TICKS(60000)); }
}
