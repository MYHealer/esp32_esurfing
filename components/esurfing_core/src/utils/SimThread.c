/**
 * @brief FreeRTOS 线程抽象实现
 *
 * 原项目使用 pthread / Win32 线程, ESP32 使用 FreeRTOS 任务.
 * API 兼容原接口 pthread 风格.
 */

#include "utils/SimThread.h"
#include "utils/Logger.h"
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ESURFING_TASK_STACK_SIZE (6 * 1024)
#define ESURFING_TASK_PRIORITY   (tskIDLE_PRIORITY + 3)

struct SimThread
{
    TaskHandle_t handle;
    int exit_code;
    volatile bool finished;
};

/* 参数包裹: func + arg + 指向 SimThread 的指针 */
typedef struct {
    sim_thread_func func;
    void* func_arg;
    sim_thread_t* thread;
} task_pkg_t;

static void task_entry(void* pvParameters)
{
    task_pkg_t* pkg = (task_pkg_t*)pvParameters;
    sim_thread_t* thr = pkg->thread;

    int ret = pkg->func(pkg->func_arg);
    thr->exit_code = ret;
    thr->finished = true;

    free(pkg);
    vTaskDelete(NULL);
}

sim_thread_t* sim_thread_create(sim_thread_func func, void* arg)
{
    sim_thread_t* thread = (sim_thread_t*)malloc(sizeof(sim_thread_t));
    if (!thread) return NULL;
    thread->handle = NULL;
    thread->exit_code = 0;
    thread->finished = false;

    task_pkg_t* pkg = (task_pkg_t*)malloc(sizeof(task_pkg_t));
    if (!pkg) { free(thread); return NULL; }
    pkg->func = func;
    pkg->func_arg = arg;
    pkg->thread = thread;

    BaseType_t ret = xTaskCreate(
        task_entry, "esurf_auth",
        ESURFING_TASK_STACK_SIZE,
        pkg,
        ESURFING_TASK_PRIORITY,
        &thread->handle
    );

    if (ret != pdPASS)
    {
        LOG_ERROR("xTaskCreate 失败: %d", ret);
        free(pkg);
        free(thread);
        return NULL;
    }

    return thread;
}

int sim_thread_join(sim_thread_t* thread, int* exit_code)
{
    if (!thread) return -1;

    /* 轮询等待任务结束 */
    while (!thread->finished) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (exit_code) *exit_code = thread->exit_code;
    thread->handle = NULL; /* 任务已删除, 句柄无效 */
    return 0;
}

int sim_thread_detach(sim_thread_t* thread)
{
    /* FreeRTOS: 任务结束后自动清理, detach 只需置空句柄 */
    if (!thread) return -1;
    thread->handle = NULL;
    return 0;
}

uint64_t sim_thread_cur_id(void)
{
    return (uint64_t)xTaskGetCurrentTaskHandle();
}

void sim_thread_destroy(sim_thread_t* thread)
{
    if (!thread) return;
    if (thread->handle) {
        /* 可以强制删除, 但风险大: vTaskDelete(thread->handle); */
    }
    free(thread);
}
