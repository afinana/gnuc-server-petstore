// Creator:  9/15/2021 12:00 PM

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "utils.h"

/**
 * @brief Duplicates a string by allocating memory and copying the contents.
 *
 * @param str The input string to duplicate.
 * @return char* A pointer to the newly allocated string, or NULL if memory allocation fails.
 */
char* dupstr(const char* str) {
    if (str == NULL) {
        return NULL;
    }

    size_t len = strlen(str) + 1; // +1 for the null terminator
    char* copy = (char*)malloc(len);
    if (copy == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }

    memcpy(copy, str, len);
    return copy;
}