#ifndef LOG_UTILS_H
#define LOG_UTILS_H

#include <stdio.h>

// ANSI color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_RED     "\033[31m"
#define COLOR_CYAN    "\033[36m"

#define LOG_INFO(format, ...)  fprintf(stdout, COLOR_GREEN  "INFO:  " COLOR_RESET format "\n", ##__VA_ARGS__)
#define LOG_WARN(format, ...)  fprintf(stdout, COLOR_YELLOW "WARN:  " COLOR_RESET format "\n", ##__VA_ARGS__)
#define LOG_ERROR(format, ...) fprintf(stderr, COLOR_RED    "ERROR: " COLOR_RESET format "\n", ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) fprintf(stdout, COLOR_CYAN   "DEBUG: " COLOR_RESET format "\n", ##__VA_ARGS__)

#endif // LOG_UTILS_H
