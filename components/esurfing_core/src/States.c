#include "utils/PlatformUtils.h"
#include "utils/Logger.h"
#include "States.h"
#include <string.h>
#include <stdlib.h>

uint64_t            g_start_run_tm = 0;
int8_t              g_prog_cnt = 0;
int8_t              tl_thread_idx = -1;
prog_status_t*      g_prog_status = NULL;
char                g_school_network_symbol[SCHOOL_NETWORK_SYMBOL] = {0};
bool                g_thread_keep_alive = false;
bool                g_need_exit = false;
bool                g_prog_enabled = false;
bool                g_need_restart = false;

void refresh_states()
{
    if (g_prog_status == NULL) return;

    /* 初始化 algo_id (默认 GUID, init_session 时服务器会返回真正的 algo_id) */
    snprintf(g_prog_status[tl_thread_idx].auth_cfg.algo_id, ALGO_ID_LEN,
             "00000000-0000-0000-0000-000000000000");

    /* 生成随机 client_id (UUID v4 格式) */
    uint8_t rnd[16];
    get_rand_bytes(rnd, 16);
    rnd[6] = (rnd[6] & 0x0F) | 0x40; /* version 4 */
    rnd[8] = (rnd[8] & 0x3F) | 0x80; /* variant 1 */

    char uuid[37];
    snprintf(uuid, sizeof(uuid),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        rnd[0], rnd[1], rnd[2], rnd[3],
        rnd[4], rnd[5], rnd[6], rnd[7],
        rnd[8], rnd[9], rnd[10], rnd[11], rnd[12], rnd[13], rnd[14], rnd[15]);

    snprintf(g_prog_status[tl_thread_idx].auth_cfg.client_id, CLIENT_ID_LEN, "%s", uuid);

    /* 生成随机 host_name (20 hex chars, 即 10 bytes) */
    uint8_t host_rnd[10];
    get_rand_bytes(host_rnd, 10);
    host_rnd[0] &= 0xFE; /* 清除 bit 0 */
    char host[21];
    for (int i = 0; i < 10; i++)
        snprintf(host + i * 2, 3, "%02x", host_rnd[i]);
    host[20] = '\0';
    snprintf(g_prog_status[tl_thread_idx].auth_cfg.host_name, HOST_NAME_LEN, "%s", host);

    /* 生成随机 MAC 地址 */
    uint8_t mac_rnd[6];
    get_rand_bytes(mac_rnd, 6);
    mac_rnd[0] &= 0xFE; /* 清除 bit 0 (locally administered) */
    char mac[18];
    snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
        mac_rnd[0], mac_rnd[1], mac_rnd[2], mac_rnd[3], mac_rnd[4], mac_rnd[5]);
    snprintf(g_prog_status[tl_thread_idx].auth_cfg.mac_addr, MAC_ADDR_LEN, "%s", mac);

    LOG_DEBUG("刷新状态: client_id=%s, host=%s, mac=%s",
              g_prog_status[tl_thread_idx].auth_cfg.client_id,
              g_prog_status[tl_thread_idx].auth_cfg.host_name,
              g_prog_status[tl_thread_idx].auth_cfg.mac_addr);
}
