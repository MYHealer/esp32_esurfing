#ifndef ESURFINGCLIENT_DIALERCLIENT_H
#define ESURFINGCLIENT_DIALERCLIENT_H

/**
 * @brief 认证线程入口
 */
int dialer_app(void* arg);

/**
 * @brief 工作函数 (ESP32 上作为 task 入口)
 */
void work(void);

#endif // ESURFINGCLIENT_DIALERCLIENT_H
