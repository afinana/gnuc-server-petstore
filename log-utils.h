#ifndef LOG_UTILS_H
#define LOG_UTILS_H

#include <stdio.h>
#include <time.h>

/**
 * @brief Get the current time as a string.
 *
 * This function returns the current time as a string in the format "YYYY-MM-DD HH:MM:SS".
 *
 * @return const char* The current time as a string.
 */
static const char* current_time() {
    static char buffer[24]; // Adjust buffer size to accommodate milliseconds
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm* timeinfo = localtime(&tv.tv_sec);
    strftime(buffer, sizeof(buffer) - 4, "%Y-%m-%d %H:%M:%S", timeinfo);
    snprintf(buffer + 19, 5, ".%03ld", tv.tv_usec / 1000); // Add milliseconds
    return buffer;
}


#define LOG_INFO(format, ...) fprintf(stdout, "[%s] INFO: " format "\n", current_time(), ##__VA_ARGS__)
#define LOG_WARN(format, ...) fprintf(stdout, "[%s] WARN: " format "\n", current_time(), ##__VA_ARGS__)
#define LOG_ERROR(format, ...) fprintf(stderr, "[%s] ERROR: " format "\n", current_time(), ##__VA_ARGS__)

#endif // LOG_UTILS_H
#include <sys/time.h> // Include for gettimeofday

