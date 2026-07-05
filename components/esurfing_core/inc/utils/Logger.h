#ifndef ESURFINGCLIENT_LOGGER_H
#define ESURFINGCLIENT_LOGGER_H

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

#ifndef PATH_MAX
#define PATH_MAX 64
#endif

typedef enum {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_FATAL = 1,
    LOG_LEVEL_ERROR = 2,
    LOG_LEVEL_WARN  = 3,
    LOG_LEVEL_INFO  = 4,
    LOG_LEVEL_DEBUG = 5,
    LOG_LEVEL_VERBOSE = 6
} LogLevel;

typedef struct {
    LogLevel    lv;
    char        log_dir[PATH_MAX];
    char        log_file[PATH_MAX];
    FILE*       file_handle;
    size_t      max_lines;
    size_t      cur_lines;
} log_cfg_t;

/*
 * Use a 2-level macro trick so that PRIu8/PRIu64 etc. get expanded
 * before the LOG_* macro processes the arguments.
 * This avoids Clang error "expected ')'" when PRI macros expand to
 * parenthesised strings like ("lu").
 */
#define LOG_OUT(level, ...) log_out((level), __FILE__, __LINE__, __VA_ARGS__)

#define LOG_VERBOSE(...)  LOG_OUT(LOG_LEVEL_VERBOSE, __VA_ARGS__)
#define LOG_DEBUG(...)    LOG_OUT(LOG_LEVEL_DEBUG,   __VA_ARGS__)
#define LOG_INFO(...)     LOG_OUT(LOG_LEVEL_INFO,    __VA_ARGS__)
#define LOG_WARN(...)     LOG_OUT(LOG_LEVEL_WARN,    __VA_ARGS__)
#define LOG_ERROR(...)    LOG_OUT(LOG_LEVEL_ERROR,   __VA_ARGS__)
#define LOG_FATAL(...)    LOG_OUT(LOG_LEVEL_FATAL,   __VA_ARGS__)

#define LOG_WEB_VERBOSE(file, line, ...) log_out(LOG_LEVEL_VERBOSE, (file), (line), __VA_ARGS__)
#define LOG_WEB_INFO(file, line, ...)    log_out(LOG_LEVEL_INFO,    (file), (line), __VA_ARGS__)
#define LOG_WEB_ERROR(file, line, ...)   log_out(LOG_LEVEL_ERROR,   (file), (line), __VA_ARGS__)

void log_out(LogLevel level, const char* file, uint32_t line, const char* fmt, ...);
LogLevel get_logger_level(void);
void set_logger_level(LogLevel lv);
bool init_logger(void);
void clean_logger(void);

#endif //ESURFINGCLIENT_LOGGER_H
