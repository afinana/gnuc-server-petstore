/**
 * @file database_stubs.h
 * @brief Stub control API for database functions in unit tests.
 *
 * Call stub_db_reset() in setUp(), then configure return values per test.
 * After the system-under-test runs, inspect the recorded call info.
 */
#ifndef DATABASE_STUBS_H
#define DATABASE_STUBS_H

#include <stdbool.h>
#include <stddef.h>

/* -------------------------------------------------------------------------
 * Stub state — set these before calling the handler under test
 * ---------------------------------------------------------------------- */

/** Return value for db_insert() stub. Default: true */
extern bool stub_db_insert_ret;

/** Return value for db_update() stub. Default: true */
extern bool stub_db_update_ret;

/** Return value for db_delete() stub. Default: true */
extern bool stub_db_delete_ret;

/**
 * JSON string that db_find_one() will return (converted to bson_t).
 * Set to NULL to simulate "not found". Default: NULL
 */
extern const char* stub_db_find_one_json;

/**
 * JSON string that db_find() will place inside the "results" array.
 * Set to NULL to simulate cursor error / empty results. Default: NULL
 */
extern const char* stub_db_find_json;

/* -------------------------------------------------------------------------
 * Call recording — inspect these after calling the handler under test
 * ---------------------------------------------------------------------- */

/** Number of times db_insert() was called since last reset */
extern int stub_db_insert_call_count;

/** Collection name passed to the last db_insert() call */
extern char stub_db_insert_last_collection[64];

/** Number of times db_update() was called since last reset */
extern int stub_db_update_call_count;

/** Number of times db_delete() was called since last reset */
extern int stub_db_delete_call_count;

/** Collection name passed to the last db_delete() call */
extern char stub_db_delete_last_collection[64];

/** Number of times db_find_one() was called since last reset */
extern int stub_db_find_one_call_count;

/** Number of times db_find() was called since last reset */
extern int stub_db_find_call_count;

/* -------------------------------------------------------------------------
 * Utility
 * ---------------------------------------------------------------------- */

/** Reset all stubs to default values and clear call records. */
void stub_db_reset(void);

#endif /* DATABASE_STUBS_H */
