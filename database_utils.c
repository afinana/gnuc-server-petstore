/**
 * @file database_utils.c
 * @brief Internal database utility functions — URI parsing and ID extraction.
 *
 * Extracted from database.c so they can be compiled and unit-tested
 * independently without a live Redis connection or hiredis dependency.
 */
#include "database_utils.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -------------------------------------------------------------------------
 * URI parsing
 * ---------------------------------------------------------------------- */

/**
 * @brief Parses a Redis URI string of the form redis://[:password@]host:port/
 *
 * Extracts the host, port and optional password components. Falls back to
 * defaults of 127.0.0.1:6379 if parsing fails.
 *
 * Examples:
 *   redis://127.0.0.1:6379/
 *   redis://:mypassword@redis.example.com:6380/
 *   redis://localhost/          (port defaults to 6379)
 */
void parse_redis_uri(const char* uri, char** host, int* port, char** password) {
    *host     = strdup("127.0.0.1");
    *port     = 6379;
    *password = NULL;

    if (!uri) return;

    /* Skip the "redis://" scheme prefix */
    const char* p = uri;
    if (strncmp(p, "redis://", 8) == 0) {
        p += 8;
    }

    /* Check for :password@ pattern */
    const char* at = strchr(p, '@');
    if (at != NULL) {
        /* Password sits between leading ":" and "@" */
        if (p[0] == ':') {
            size_t pw_len = (size_t)(at - p - 1);
            *password = strndup(p + 1, pw_len);
        }
        p = at + 1;
    }

    /* Remaining string: host:port[/...] */
    const char* colon = strchr(p, ':');
    const char* slash = strchr(p, '/');

    if (colon != NULL) {
        free(*host);
        *host = strndup(p, (size_t)(colon - p));
        *port = atoi(colon + 1);
    } else if (slash != NULL) {
        free(*host);
        *host = strndup(p, (size_t)(slash - p));
    } else if (*p != '\0') {
        free(*host);
        *host = strdup(p);
    }
}

/* -------------------------------------------------------------------------
 * ID extraction
 * ---------------------------------------------------------------------- */

/**
 * @brief Extracts the "id" field from a BSON document as a heap string.
 *
 * Supports int32, int64, and UTF-8 string id types.
 * Returns NULL if the field is absent or has an unsupported type.
 */
char* extract_id_string(const bson_t* doc) {
    bson_iter_t iter;
    if (!bson_iter_init_find(&iter, doc, "id")) {
        return NULL;
    }
    if (BSON_ITER_HOLDS_INT64(&iter)) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lld", (long long)bson_iter_int64(&iter));
        return strdup(buf);
    }
    if (BSON_ITER_HOLDS_INT32(&iter)) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", bson_iter_int32(&iter));
        return strdup(buf);
    }
    if (BSON_ITER_HOLDS_UTF8(&iter)) {
        uint32_t len;
        const char* val = bson_iter_utf8(&iter, &len);
        return strndup(val, len);
    }
    return NULL;
}
