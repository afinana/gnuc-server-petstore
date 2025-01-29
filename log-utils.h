#ifndef LOG_UTILS_H
#define LOG_UTILS_H

#include <stdio.h>

#define LOG_INFO(format, ...) fprintf(stdout, "INFO: " format "\n", ##__VA_ARGS__)
#define LOG_WARN(format, ...) fprintf(stdout, "WARN: " format "\n", ##__VA_ARGS__)
#define LOG_ERROR(format, ...) fprintf(stderr, "ERROR: " format "\n", ##__VA_ARGS__)

#endif // LOG_UTILS_H
