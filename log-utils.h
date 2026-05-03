#ifndef LOG_UTILS_H
#define LOG_UTILS_H

#include <stdio.h>
#include <time.h>
#include <sys/time.h>

/**
 * @brief Get the current time as a string (thread-safe version).
 *
 * Uses a thread-local buffer to avoid race conditions when called
 * from multiple HTTP handler threads simultaneously.
 *
 * @return const char* The current time in "YYYY-MM-DD HH:MM:SS.mmm" format.
 */
static inline const char* current_time(void) {
    static _Thread_local char buffer[32];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm* timeinfo = localtime(&tv.tv_sec);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    snprintf(buffer + 19, sizeof(buffer) - 19, ".%03d", (int)(tv.tv_usec / 1000));
    return buffer;
}

#define LOG_INFO(format, ...) fprintf(stdout, "[%s] INFO: " format "\n", current_time(), ##__VA_ARGS__)
#define LOG_WARN(format, ...) fprintf(stdout, "[%s] WARN: " format "\n", current_time(), ##__VA_ARGS__)
#define LOG_ERROR(format, ...) fprintf(stderr, "[%s] ERROR: " format "\n", current_time(), ##__VA_ARGS__)

#endif // LOG_UTILS_H
