/**
 * @file database_utils.h
 * @brief Internal database utility functions.
 *
 * These helpers are extracted from database.c so they can be unit-tested
 * independently without requiring a live Redis connection.
 */
#ifndef DATABASE_UTILS_H
#define DATABASE_UTILS_H

#include <bson/bson.h>

/**
 * @brief Parses a Redis URI string of the form redis://[:password@]host:port/
 *
 * Extracts the host, port and optional password components. Falls back to
 * defaults (127.0.0.1:6379, no password) if any component is missing.
 *
 * @param uri      The Redis URI string.
 * @param host     Output: heap-allocated host string (caller must free).
 * @param port     Output: port number.
 * @param password Output: heap-allocated password string, or NULL if absent.
 *                 Caller must free if non-NULL.
 */
void parse_redis_uri(const char* uri, char** host, int* port, char** password);

/**
 * @brief Extracts the "id" field from a BSON document as a string.
 *
 * Handles int32, int64, and UTF-8 string id types. The caller must free
 * the returned string with free().
 *
 * @param doc The BSON document.
 * @return char* The ID as a heap-allocated string, or NULL if not found.
 */
char* extract_id_string(const bson_t* doc);

#endif /* DATABASE_UTILS_H */
