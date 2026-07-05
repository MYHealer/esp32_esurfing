#ifndef ESP_ESURFING_PORT_H
#define ESP_ESURFING_PORT_H

/**
 * @brief ESP32 porting layer for ESurfingClient
 *
 * Defines macros and platform identifiers used by the original code.
 */

#define __ESP32__ 1

/* Remove original platform-specific modules */
#define REMOVE_SERVICE    1
#define REMOVE_WEB_SERVER 1
#define REMOVE_PROCESS    1

/* PATH_MAX for ESP32 (SPIFFS path length) */
#ifndef PATH_MAX
#define PATH_MAX 64
#endif

#endif /* ESP_ESURFING_PORT_H */
