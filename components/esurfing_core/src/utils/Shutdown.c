/**
 * @brief ESP32 关闭处理 (简化版)
 *
 * 原版需要处理 signal/Winsvc/console 事件.
 * ESP32 上使用 g_need_exit 标志 + 任务清理.
 */

#include "utils/PlatformUtils.h"
#include "utils/Shutdown.h"
#include "utils/Logger.h"
#include "States.h"

#include <stdlib.h>

void shut(const int8_t exit_code)
{
    LOG_INFO("程序正在关闭 (exit_code=%" PRId8 ")", exit_code);
    g_need_exit = true;
    g_thread_keep_alive = false;

    /* 等待所有认证线程结束 */
    for (int8_t i = 0; i < g_prog_cnt; i++)
    {
        int result_code = 0;
        g_prog_status[i].runtime_status.is_running = false;
        sim_thread_join(g_prog_status[i].thread, &result_code);
        LOG_DEBUG("认证线程 %" PRId8 " 退出码: %d", i, result_code);
    }

    clean_logger();
    LOG_INFO("程序关闭完成");
}

void init_shutdown_hook(void)
{
    /* ESP32 上不需要 signal handler */
    LOG_DEBUG("关闭钩子已初始化 (ESP32 模式)");
}
