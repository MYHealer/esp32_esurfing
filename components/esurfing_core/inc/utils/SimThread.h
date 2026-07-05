#ifndef ESURFINGCLIENT_SIMTHREAD_H
#define ESURFINGCLIENT_SIMTHREAD_H

#include <stdint.h>

/**
 * @brief FreeRTOS 线程抽象层
 *
 * 兼容原项目 sim_thread_t API, 底层使用 FreeRTOS xTaskCreate
 */

typedef struct SimThread sim_thread_t;
typedef int (*sim_thread_func)(void* arg);

sim_thread_t* sim_thread_create(sim_thread_func func, void* arg);
int sim_thread_join(sim_thread_t* thread, int* exit_code);
int sim_thread_detach(sim_thread_t* thread);
uint64_t sim_thread_cur_id(void);
void sim_thread_destroy(sim_thread_t* thread);

#endif // ESURFINGCLIENT_SIMTHREAD_H
