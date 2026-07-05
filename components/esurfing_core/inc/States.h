#ifndef ESURFINGCLIENT_STATES_H
#define ESURFINGCLIENT_STATES_H

#include "cipher/CipherInterface.h"
#include "utils/SimThread.h"

#include <stdint.h>
#include <stdbool.h>

#define SCHOOL_NETWORK_SYMBOL 8

#define TICKET_URL_LEN 512
#define USER_AGENT_LEN 32
#define CLIENT_ID_LEN 40
#define HOST_NAME_LEN 16
#define KEEP_URL_LEN 256
#define TERM_URL_LEN 256
#define AUTH_URL_LEN 256
#define MAC_ADDR_LEN 20
#define ALGO_ID_LEN 37
#define TICKET_LEN 40

#define USR_LEN 16
#define PWD_LEN 128
#define CHN_LEN 8

#define IP_LEN 16
#define IF_LEN 16

#define LAST_LOCATION_LEN 512

/** @brief 认证配置 */
typedef struct
{
    char ticket_url[TICKET_URL_LEN];
    char client_id[CLIENT_ID_LEN];
    char host_name[HOST_NAME_LEN];
    char keep_url[KEEP_URL_LEN];
    char term_url[TERM_URL_LEN];
    char auth_url[AUTH_URL_LEN];
    char mac_addr[MAC_ADDR_LEN];
    char algo_id[ALGO_ID_LEN];
    char ticket[TICKET_LEN];
    char client_ip[IP_LEN];
    char ac_ip[IP_LEN];
    cipher_interface_t* cipher;
    uint64_t keep_retry;
    uint64_t auth_time;
    uint64_t tick;
} auth_cfg_t;

/** @brief 登录配置 */
typedef struct
{
    char usr[USR_LEN];
    char pwd[PWD_LEN];
    char chn[CHN_LEN];
    char user_agent[USER_AGENT_LEN];
    uint32_t mark;
    bool use_cus_mark;
    uint8_t idx;
} login_cfg_t;

/** @brief 运行状态 */
typedef struct
{
    bool is_initialized;
    bool is_running;
    bool is_authed;
    bool is_need_reset;
} runtime_status_t;

/** @brief 认证线程状态 */
typedef struct
{
    auth_cfg_t auth_cfg;
    login_cfg_t login_cfg;
    runtime_status_t runtime_status;
    uint64_t thread_id;
    sim_thread_t* thread;
    char last_location[LAST_LOCATION_LEN];
    bool last_location_lock;
} prog_status_t;

/** @brief 程序开始运行时间 */
extern uint64_t g_start_run_tm;

/** @brief 适配器数 */
extern int8_t g_prog_cnt;

/**
 * @brief 线程独立下标
 * 在 FreeRTOS 中用 task-local storage 或直接传参
 * 简化实现：通过 pthread-style 抽象，在回调中通过参数传递
 */
extern int8_t tl_thread_idx;

/** @brief 认证线程状态 */
extern prog_status_t* g_prog_status;

/** @brief 校园网标志 */
extern char g_school_network_symbol[SCHOOL_NETWORK_SYMBOL];

/** @brief 线程保活 */
extern bool g_thread_keep_alive;

/** @brief 需要退出 */
extern bool g_need_exit;

/** @brief 程序启用状态 */
extern bool g_prog_enabled;

/** @brief 需要重启 (ESP32 上无意义, 保留占位) */
extern bool g_need_restart;

/** @brief 刷新状态函数 */
void refresh_states();

#endif //ESURFINGCLIENT_STATES_H
