#ifndef LOG_UTILS_H
#define LOG_UTILS_H

#include <stdio.h>
#include <time.h>
#include <sys/time.h>

/**
 * @brief Returns the current timestamp as "YYYY-MM-DD HH:MM:SS.mmm".
 *
 * Uses a static buffer — not thread-safe, but acceptable for logging macros
 * that are already serialised through stdio.
 */
static inline const char* current_time(void) {
    static char buffer[40];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm* ti = localtime(&tv.tv_sec);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", ti);
    snprintf(buffer + 19, sizeof(buffer) - 19, ".%03ld", (long)(tv.tv_usec / 1000));
    return buffer;
}

#define LOG_INFO(fmt, ...)  (void)fprintf(stdout, "[%s] INFO: "  fmt "\n", current_time(), ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  (void)fprintf(stdout, "[%s] WARN: "  fmt "\n", current_time(), ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) (void)fprintf(stderr, "[%s] ERROR: " fmt "\n", current_time(), ##__VA_ARGS__)

#endif /* LOG_UTILS_H */
