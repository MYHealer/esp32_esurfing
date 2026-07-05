/**
 * @brief ESP32 移植版 DialerClient
 *
 * 与原版差异:
 * - 移除 web_server / service 相关引用
 * - 移除 __OPENWRT__ / _WIN32 平台分支
 * - 其余认证逻辑完全一致
 */

#include "cipher/CipherInterface.h"
#include "utils/PlatformUtils.h"
#include "utils/Shutdown.h"
#include "utils/Logger.h"
#include "DialerClient.h"
#include "NetClient.h"
#include "States.h"

#include <stdlib.h>
#include <string.h>

typedef enum
{
    AUTH_SUCCESS = 0,
    AUTH_FAILED = 1,
    INIT_SESSION_FAILED = 2,
    GET_TICKET_FAILED = 3,
    LOGIN_FAILED = 4
} AuthStatus;

typedef enum
{
    RUN_SUCCESS = 0,
    RUN_FAILED = 1,
    TIMEOUT_RETRY = 2
} RunStatus;

static const uint64_t table[] = {1, 5, 10, 20, 30};

static bool term()
{
    const char* xml = create_xml_payload(TERM);
    if (xml == NULL) { LOG_ERROR("登出 XML 创建失败"); return false; }

    char* encrypt = session_encrypt(xml);
    if (encrypt == NULL) { LOG_ERROR("登出 XML 加密失败"); return false; }
    LOG_VERBOSE("加密登出内容: %s", encrypt);

    http_resp_t result = post(g_prog_status[tl_thread_idx].auth_cfg.term_url, encrypt);
    uint8_t retry = 1;
    while (result.status != REQUEST_SUCCESS && result.status != REQUEST_HAVE_RES)
    {
        if (retry > 5) { LOG_FATAL("超过重试次数"); free(encrypt); if (result.body_data) free(result.body_data); return false; }
        LOG_ERROR("登出失败 retry=%d/%d", retry, 5);
        retry++;
        sleep_ms(1000, true);
        if (result.body_data) free(result.body_data);
        result = post(g_prog_status[tl_thread_idx].auth_cfg.term_url, encrypt);
    }
    free(encrypt);
    if (result.body_data) free(result.body_data);

    g_prog_status[tl_thread_idx].auth_cfg.auth_time = 0;
    g_prog_status[tl_thread_idx].runtime_status.is_authed = false;
    return true;
}

static bool heartbeat()
{
    const char* xml = create_xml_payload(HEART_BEAT);
    if (xml == NULL) { LOG_ERROR("心跳 XML 创建失败"); return false; }

    char* encrypt = session_encrypt(xml);
    if (encrypt == NULL) { LOG_ERROR("加密心跳 XML 失败"); return false; }
    LOG_VERBOSE("加密心跳: %s", encrypt);

    const http_resp_t result = post(g_prog_status[tl_thread_idx].auth_cfg.keep_url, encrypt);
    free(encrypt);
    if (result.status != REQUEST_HAVE_RES || result.body_size == 0 || result.body_data == NULL)
    {
        LOG_ERROR("心跳响应失败");
        if (result.body_data) free(result.body_data);
        return false;
    }

    char* decrypted_data = session_decrypt(result.body_data);
    free(result.body_data);
    if (decrypted_data == NULL) { LOG_ERROR("解密心跳失败"); return false; }

    char* parsed_interval = xml_parser(decrypted_data, "interval");
    free(decrypted_data);
    if (parsed_interval == NULL) { LOG_ERROR("解析心跳间隔失败"); return false; }

    g_prog_status[tl_thread_idx].auth_cfg.keep_retry = str2uint64(parsed_interval);
    free(parsed_interval);
    return true;
}

static bool login()
{
    const char* xml = create_xml_payload(LOGIN);
    if (xml == NULL) { LOG_ERROR("登录 XML 创建失败"); return false; }

    char* encrypt = session_encrypt(xml);
    if (encrypt == NULL) { LOG_ERROR("加密登录 XML 失败"); return false; }

    const http_resp_t result = post(g_prog_status[tl_thread_idx].auth_cfg.auth_url, encrypt);
    free(encrypt);
    if (result.status != REQUEST_HAVE_RES || result.body_size == 0 || result.body_data == NULL)
    {
        LOG_ERROR("登录响应失败");
        if (result.body_data) free(result.body_data);
        return false;
    }

    char* decrypted_data = session_decrypt(result.body_data);
    free(result.body_data);
    if (decrypted_data == NULL) { LOG_ERROR("解密登录响应失败"); return false; }

    char* parsed_keep_url = xml_parser(decrypted_data, "keep-url");
    if (parsed_keep_url == NULL) { LOG_ERROR("解析 KeepURL 失败"); free(decrypted_data); return false; }
    char* cleaned_keep_url = clean_CDATA(parsed_keep_url);
    free(parsed_keep_url);
    if (cleaned_keep_url == NULL) { LOG_ERROR("清除 KeepURL CDATA 失败"); free(decrypted_data); return false; }
    snprintf(g_prog_status[tl_thread_idx].auth_cfg.keep_url, KEEP_URL_LEN, "%s", safe_str(cleaned_keep_url));
    LOG_INFO("Keep-Url: %s", g_prog_status[tl_thread_idx].auth_cfg.keep_url);
    free(cleaned_keep_url);

    char* parsed_term_url = xml_parser(decrypted_data, "term-url");
    if (parsed_term_url == NULL) { LOG_ERROR("解析 TermURL 失败"); free(decrypted_data); return false; }
    char* cleaned_term_url = clean_CDATA(parsed_term_url);
    free(parsed_term_url);
    if (cleaned_term_url == NULL) { LOG_ERROR("清除 TermURL CDATA 失败"); free(decrypted_data); return false; }
    snprintf(g_prog_status[tl_thread_idx].auth_cfg.term_url, TERM_URL_LEN, "%s", safe_str(cleaned_term_url));
    LOG_INFO("Term-Url: %s", g_prog_status[tl_thread_idx].auth_cfg.term_url);
    free(cleaned_term_url);

    char* parsed_keep_retry = xml_parser(decrypted_data, "keep-retry");
    free(decrypted_data);
    if (parsed_keep_retry == NULL) { LOG_ERROR("解析 KeepRetry 失败"); return false; }
    g_prog_status[tl_thread_idx].auth_cfg.keep_retry = str2uint64(parsed_keep_retry);
    free(parsed_keep_retry);
    LOG_INFO("下一次重试: %" PRIu64 " 秒后", g_prog_status[tl_thread_idx].auth_cfg.keep_retry);
    return true;
}

static bool get_ticket()
{
    LOG_DEBUG("get_ticket 配置: %" PRIu8 ", 下标: %" PRIu8,
              g_prog_status[tl_thread_idx].login_cfg.idx, tl_thread_idx);

    const char* xml = create_xml_payload(GET_TICKET);
    if (xml == NULL) { LOG_ERROR("创建 Ticket XML 失败"); return false; }

    char* encrypt = session_encrypt(xml);
    if (encrypt == NULL) { LOG_ERROR("加密 Ticket XML 失败"); return false; }

    const http_resp_t result = post(g_prog_status[tl_thread_idx].auth_cfg.ticket_url, encrypt);
    free(encrypt);
    if (result.status != REQUEST_HAVE_RES || result.body_size == 0 || result.body_data == NULL)
    {
        LOG_ERROR("获取 Ticket 响应失败");
        if (result.body_data) free(result.body_data);
        return false;
    }

    char* decrypt = session_decrypt(result.body_data);
    free(result.body_data);
    if (decrypt == NULL) { LOG_ERROR("解密 Ticket 失败"); return false; }

    char* parsed_ticket = xml_parser(decrypt, "ticket");
    free(decrypt);
    if (parsed_ticket == NULL) { LOG_ERROR("解析 Ticket 失败"); return false; }

    snprintf(g_prog_status[tl_thread_idx].auth_cfg.ticket, TICKET_LEN, "%s", safe_str(parsed_ticket));
    LOG_INFO("Ticket: %s", g_prog_status[tl_thread_idx].auth_cfg.ticket);
    free(parsed_ticket);
    return true;
}

static bool load_cipher(const bytes_t zsm)
{
    LOG_DEBUG("zsm 长度: %zu", zsm.length);
    if (zsm.data == NULL || zsm.length == 0) { LOG_ERROR("无效 zsm"); return false; }

    char str[zsm.length + 1];
    memcpy(str, zsm.data, zsm.length);
    str[zsm.length] = '\0';

    size_t length = strlen(str);
    if (length < 4 + 38) { LOG_ERROR("zsm 长度不足"); return false; }

    char algo_id[ALGO_ID_LEN];
    memcpy(algo_id, str + length - 37, ALGO_ID_LEN - 1);
    algo_id[ALGO_ID_LEN - 1] = '\0';
    LOG_INFO("Algo ID: %s", algo_id);

    if (init_cipher(algo_id) == false) { LOG_ERROR("初始化加解密工厂失败"); return false; }
    snprintf(g_prog_status[tl_thread_idx].auth_cfg.algo_id, ALGO_ID_LEN, "%s", safe_str(algo_id));
    return true;
}

static void clean_session()
{
    destroy_cipher_factory();
    g_prog_status[tl_thread_idx].runtime_status.is_initialized = 0;
}

static bool init_session()
{
    const http_resp_t result = post(g_prog_status[tl_thread_idx].auth_cfg.ticket_url,
                                     g_prog_status[tl_thread_idx].auth_cfg.algo_id);
    LOG_INFO("init_session: POST status=%d, body_size=%d, body=%p", result.status, result.body_size, result.body_data);
    if (result.status != REQUEST_HAVE_RES || result.body_size == 0 || result.body_data == NULL)
    {
        LOG_ERROR("初始化会话失败 (status=%d)", result.status);
        if (result.body_data) free(result.body_data);
        return false;
    }

    const bytes_t zsm = str2bytes(result.body_data);
    free(result.body_data);
    if (load_cipher(zsm) == false)
    {
        g_prog_status[tl_thread_idx].runtime_status.is_initialized = 0;
        free(zsm.data);
        return false;
    }
    g_prog_status[tl_thread_idx].runtime_status.is_initialized = 1;
    free(zsm.data);
    return true;
}

static AuthStatus auth()
{
    const char portal_start_tag[] = "<!--//config.campus.js.chinatelecom.com";
    const char portal_end_tag[] = "//config.campus.js.chinatelecom.com-->";

    const http_resp_t resp = get(g_prog_status[tl_thread_idx].last_location);
    if (resp.status != REQUEST_HAVE_RES || resp.body_size == 0 || resp.body_data == NULL)
    {
        LOG_ERROR("响应为空");
        if (resp.body_data) free(resp.body_data);
        return AUTH_FAILED;
    }

    char* portal_config = extract_between_tags(resp.body_data, portal_start_tag, portal_end_tag);
    free(resp.body_data);
    if (portal_config == NULL) { LOG_ERROR("提取门户配置失败"); return AUTH_FAILED; }

    char* auth_url = xml_parser(portal_config, "auth-url");
    if (auth_url == NULL) { free(portal_config); return AUTH_FAILED; }
    char* cleaned_auth_url = clean_CDATA(auth_url);
    free(auth_url);
    if (cleaned_auth_url == NULL) { free(portal_config); return AUTH_FAILED; }
    snprintf(g_prog_status[tl_thread_idx].auth_cfg.auth_url, AUTH_URL_LEN, "%s", safe_str(cleaned_auth_url));
    LOG_INFO("Auth URL: %s", g_prog_status[tl_thread_idx].auth_cfg.auth_url);
    free(cleaned_auth_url);

    char* ticket_url = xml_parser(portal_config, "ticket-url");
    free(portal_config);
    if (ticket_url == NULL) { LOG_ERROR("提取 Ticket URL 失败"); return AUTH_FAILED; }
    char* cleaned_ticket_url = clean_CDATA(ticket_url);
    free(ticket_url);
    if (cleaned_ticket_url == NULL) { LOG_ERROR("清除 Ticket URL CDATA 失败"); return AUTH_FAILED; }
    snprintf(g_prog_status[tl_thread_idx].auth_cfg.ticket_url, TICKET_URL_LEN, "%s", safe_str(cleaned_ticket_url));
    LOG_INFO("Ticket URL: %s", g_prog_status[tl_thread_idx].auth_cfg.ticket_url);

    char* client_ip = extract_url_param(cleaned_ticket_url, "wlanuserip");
    if (client_ip == NULL) { free(cleaned_ticket_url); return AUTH_FAILED; }
    snprintf(g_prog_status[tl_thread_idx].auth_cfg.client_ip, IP_LEN, "%s", safe_str(client_ip));
    LOG_INFO("Client IP: %s", g_prog_status[tl_thread_idx].auth_cfg.client_ip);
    free(client_ip);

    char* ac_ip = extract_url_param(cleaned_ticket_url, "wlanacip");
    free(cleaned_ticket_url);
    if (ac_ip == NULL) { LOG_ERROR("提取 AC IP 失败"); return AUTH_FAILED; }
    snprintf(g_prog_status[tl_thread_idx].auth_cfg.ac_ip, IP_LEN, "%s", safe_str(ac_ip));
    LOG_INFO("AC IP: %s", g_prog_status[tl_thread_idx].auth_cfg.ac_ip);
    free(ac_ip);

    if (init_session() == false) { LOG_FATAL("初始化会话失败"); return INIT_SESSION_FAILED; }
    if (get_ticket() == false)   { LOG_FATAL("获取 Ticket 失败");   return GET_TICKET_FAILED; }
    if (login() == false)        { LOG_ERROR("登录失败");            return LOGIN_FAILED; }

    g_prog_status[tl_thread_idx].auth_cfg.tick = get_cur_tm_ms();
    g_prog_status[tl_thread_idx].auth_cfg.auth_time = get_cur_tm_ms();
    g_prog_status[tl_thread_idx].runtime_status.is_authed = true;
    LOG_INFO("已认证登录");
    sleep_ms(5000, false);
    return AUTH_SUCCESS;
}

static void clean()
{
    if (g_prog_status[tl_thread_idx].runtime_status.is_initialized)
    {
        if (g_prog_status[tl_thread_idx].runtime_status.is_authed) term();
        clean_session();
    }
    memset(&g_prog_status[tl_thread_idx].auth_cfg, 0, sizeof(auth_cfg_t));
    memset(&g_prog_status[tl_thread_idx].runtime_status, 0, sizeof(runtime_status_t));
}

static void reset()
{
    clean();
    refresh_states();
}

static RunStatus run()
{
    static uint8_t retry_timeout = 1;
    static uint8_t retry_auth = 1;
    static uint64_t retry_auth_time = 0;

    switch (check_network_status())
    {
    case REQUEST_SUCCESS:
        retry_timeout = 1;
        retry_auth = 1;
        if (g_prog_status[tl_thread_idx].runtime_status.is_initialized &&
            g_prog_status[tl_thread_idx].runtime_status.is_authed)
        {
            if (g_prog_status[tl_thread_idx].auth_cfg.keep_retry != 0)
            {
                if (get_cur_tm_ms() - g_prog_status[tl_thread_idx].auth_cfg.tick >=
                    g_prog_status[tl_thread_idx].auth_cfg.keep_retry * 1000)
                {
                    LOG_INFO("发送心跳");
                    uint8_t rh = 1;
                    while (heartbeat() == false)
                    {
                        if (rh > 5) { LOG_FATAL("心跳重试耗尽"); return RUN_FAILED; }
                        LOG_ERROR("心跳失败 retry=%d/5", rh);
                        rh++;
                        sleep_ms(1000, true);
                    }
                    LOG_INFO("下次心跳: %" PRIu64 " 秒后", g_prog_status[tl_thread_idx].auth_cfg.keep_retry);
                    g_prog_status[tl_thread_idx].auth_cfg.tick = get_cur_tm_ms();
                }
            }
        }
        sleep_ms(1000, false);
        return RUN_SUCCESS;

    case REQUEST_REDIRECT:
        retry_timeout = 1;
        LOG_INFO("需要认证");
        if (g_prog_status[tl_thread_idx].runtime_status.is_initialized) { reset(); }
        g_prog_status[tl_thread_idx].runtime_status.is_running = true;

        if (auth() != AUTH_SUCCESS)
        {
            if (!g_prog_status[tl_thread_idx].runtime_status.is_running) return RUN_FAILED;
            if (retry_auth > 5) { LOG_FATAL("认证重试耗尽"); return RUN_FAILED; }
            retry_auth_time = 60000 * table[retry_auth - 1];
            LOG_ERROR("认证失败 retry=%d/5, 下次 %" PRIu64 " 秒后", retry_auth, retry_auth_time / 1000);
            retry_auth++;
            sleep_ms(retry_auth_time, true);
        }
        return RUN_SUCCESS;

    case REQUEST_WARN:
        retry_auth = 1;
        if (retry_timeout > 5) { LOG_ERROR("超时重试耗尽"); return RUN_FAILED; }
        LOG_WARN("超时 retry=%d/5", retry_timeout);
        sleep_ms(10000, true);
        retry_timeout++;
        return TIMEOUT_RETRY;

    default:
        retry_timeout = 1;
        retry_auth = 1;
        LOG_ERROR("未知错误，30s 后重试");
        sleep_ms(30000, true);
        return RUN_SUCCESS;
    }
}

int dialer_app(void* arg)
{
    tl_thread_idx = (int8_t)(intptr_t)arg;
    g_prog_status[tl_thread_idx].runtime_status.is_running = true;
    g_prog_status[tl_thread_idx].thread_id = sim_thread_cur_id();

    LOG_DEBUG("认证线程 %" PRId8 " 启动, 配置: %" PRIu8,
              tl_thread_idx, g_prog_status[tl_thread_idx].login_cfg.idx);

    refresh_states();
    if (get_last_location() == REQUEST_ERROR)
        g_prog_status[tl_thread_idx].runtime_status.is_running = false;

    while (g_prog_status[tl_thread_idx].runtime_status.is_running)
    {
        const RunStatus rs = run();
        if (rs == RUN_FAILED || g_prog_status[tl_thread_idx].runtime_status.is_need_reset)
        {
            if (rs == RUN_FAILED) LOG_ERROR("线程错误, 退出");
            else if (g_prog_status[tl_thread_idx].runtime_status.is_need_reset)
                LOG_INFO("线程需要重置");
            g_prog_status[tl_thread_idx].runtime_status.is_running = false;
            break;
        }
    }

    clean();
    return 0;
}

void work(void)
{
    g_thread_keep_alive = true;
    g_prog_status = calloc(1, sizeof(prog_status_t));

    init_shutdown_hook();
    if (init_logger() == false) return;

    LOG_INFO("--------------------------------------------");
    LOG_INFO(" ESurfingClient ESP32 移植版");
    LOG_INFO(" 基于 BadGhost/ESurfingClient-CVersion v2");
    LOG_INFO(" 项目: github.com/BadGhost520/ESurfingClient-CVersion");
    LOG_INFO("--------------------------------------------");

    if (load_cfg() == false) shut(1);

    /* 检测网络状态, 直到需要认证或已联网 */
    NetworkStatus status;
    uint8_t retry_network = 1;
    do
    {
        if (g_need_exit) break;
        status = check_network_status();
        switch (status)
        {
        case REQUEST_SUCCESS:
            LOG_INFO("已连接到互联网");
            sleep_ms(10000, true);
            break;
        case REQUEST_ERROR:
        case REQUEST_INIT_ERROR:
            if (retry_network > 5) { retry_network = 1; }
            LOG_ERROR("网络错误 retry=%d, 30s 后重试", retry_network);
            retry_network++;
            sleep_ms(30000, true);
            break;
        default:
            sleep_ms(1000, true);
            break;
        }
    } while (status != REQUEST_REDIRECT && status != REQUEST_SUCCESS);

    /* 创建认证线程 (ESP32 简化版: 仅一个账号) */
    LOG_DEBUG("创建认证线程");
    for (uint8_t i = 0; i < g_prog_cnt; i++)
    {
        g_prog_status[i].thread = sim_thread_create(dialer_app, (void*)(intptr_t)i);
        uint8_t rc = 1;
        while (g_prog_status[i].thread == NULL)
        {
            if (rc > 5) { LOG_FATAL("线程创建失败"); shut(1); }
            LOG_ERROR("线程创建失败 retry=%d/5", rc);
            g_prog_status[i].thread = sim_thread_create(dialer_app, (void*)(intptr_t)i);
            rc++;
        }
    }

    /* 线程守护 */
    sleep_ms(5000, false);
    LOG_INFO("线程守护开启");
    uint64_t check_time = 0;
    while (g_thread_keep_alive)
    {
        if (check_time > 299999) { check_time = 0; LOG_INFO("线程守护持续运行"); }
        for (uint8_t i = 0; i < g_prog_cnt; i++)
        {
            /* 47h50m 重新认证 */
            if (get_cur_tm_ms() - g_prog_status[i].auth_cfg.auth_time >= 172200000 &&
                g_prog_status[i].auth_cfg.auth_time != 0)
            {
                LOG_WARN("认证超时 47h50m, 重置认证");
                for (uint8_t j = 0; j < g_prog_cnt; j++)
                {
                    g_prog_status[j].runtime_status.is_need_reset = true;
                    uint8_t wte = 1;
                    while (g_prog_status[j].runtime_status.is_authed)
                    {
                        if (wte > 5) { LOG_FATAL("登出超时"); sim_thread_destroy(g_prog_status[j].thread); }
                        wte++;
                        sleep_ms(2000, true);
                    }
                }
            }

            /* 守护: 重启已退出的线程 */
            if (g_prog_status[i].runtime_status.is_running == false)
            {
                int rc = 0;
                sim_thread_join(g_prog_status[i].thread, &rc);
                LOG_INFO("重启认证线程 %" PRIu8, i);
                g_prog_status[i].thread = sim_thread_create(dialer_app, (void*)(intptr_t)i);
                uint8_t rtc = 1;
                while (g_prog_status[i].thread == NULL)
                {
                    if (rtc > 5) { LOG_FATAL("线程创建失败"); shut(1); }
                    LOG_ERROR("线程创建失败 retry=%d/5", rtc);
                    g_prog_status[i].thread = sim_thread_create(dialer_app, (void*)(intptr_t)i);
                    rtc++;
                }
                while (!g_prog_status[i].runtime_status.is_running) sleep_ms(100, false);
            }
        }
        sleep_ms(10, false);
        check_time += 10;
    }

    LOG_INFO("线程守护关闭");
    while (g_thread_keep_alive == false) sleep_ms(10000, false);
}
