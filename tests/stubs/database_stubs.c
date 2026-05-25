/**
 * @file database_stubs.c
 * @brief Stub implementations of all database.h functions for unit testing.
 *
 * These replace the real MongoDB-backed implementations so tests can run
 * without a live MongoDB instance.  Each stub:
 *   - Records that it was called (call count, collection name, etc.)
 *   - Returns a configurable value set via the stub_db_* variables in
 *     database_stubs.h.
 *
 * Usage in a test:
 *   stub_db_reset();                        // clean state in setUp()
 *   stub_db_find_one_json = "{\"id\":1}";   // configure return value
 *   char* result = handle_get_pet_by_id("1");
 *   TEST_ASSERT_NOT_NULL(result);
 *   free(result);
 */
#include "database_stubs.h"
#include <string.h>
#include <stdlib.h>
#include <mongoc/mongoc.h>

/* -------------------------------------------------------------------------
 * Stub state variables (definitions)
 * ---------------------------------------------------------------------- */

bool         stub_db_insert_ret                = true;
bool         stub_db_update_ret                = true;
bool         stub_db_delete_ret                = true;
const char*  stub_db_find_one_json             = NULL;
const char*  stub_db_find_json                 = NULL;

int          stub_db_insert_call_count         = 0;
char         stub_db_insert_last_collection[64] = {0};

int          stub_db_update_call_count         = 0;

int          stub_db_delete_call_count         = 0;
char         stub_db_delete_last_collection[64] = {0};

int          stub_db_find_one_call_count       = 0;
int          stub_db_find_call_count           = 0;

/* -------------------------------------------------------------------------
 * Reset helper
 * ---------------------------------------------------------------------- */

void stub_db_reset(void) {
    stub_db_insert_ret          = true;
    stub_db_update_ret          = true;
    stub_db_delete_ret          = true;
    stub_db_find_one_json       = NULL;
    stub_db_find_json           = NULL;

    stub_db_insert_call_count   = 0;
    stub_db_update_call_count   = 0;
    stub_db_delete_call_count   = 0;
    stub_db_find_one_call_count = 0;
    stub_db_find_call_count     = 0;

    memset(stub_db_insert_last_collection, 0, sizeof(stub_db_insert_last_collection));
    memset(stub_db_delete_last_collection, 0, sizeof(stub_db_delete_last_collection));
}

/* -------------------------------------------------------------------------
 * Stub implementations of db_*
 * ---------------------------------------------------------------------- */

/* db_init / db_cleanup — no-ops in unit tests */
void db_init(const char* uri) {
    (void)uri;
    /* no MongoDB in unit tests */
}

void db_cleanup(void) {
    /* nothing to clean up */
}

bool db_insert(const char* collection_name, const bson_t* doc) {
    (void)doc;
    stub_db_insert_call_count++;
    strncpy(stub_db_insert_last_collection, collection_name,
            sizeof(stub_db_insert_last_collection) - 1);
    return stub_db_insert_ret;
}

bool db_update(const char* collection_name, const bson_t* query, const bson_t* update) {
    (void)collection_name;
    (void)query;
    (void)update;
    stub_db_update_call_count++;
    return stub_db_update_ret;
}

bool db_delete(const char* collection_name, const bson_t* query) {
    (void)query;
    stub_db_delete_call_count++;
    strncpy(stub_db_delete_last_collection, collection_name,
            sizeof(stub_db_delete_last_collection) - 1);
    return stub_db_delete_ret;
}

bson_t* db_find_one(const char* collection_name, const bson_t* query) {
    (void)collection_name;
    (void)query;
    stub_db_find_one_call_count++;

    if (stub_db_find_one_json == NULL) {
        return NULL;
    }

    bson_error_t error;
    bson_t* doc = bson_new_from_json(
        (const uint8_t*)stub_db_find_one_json, -1, &error);
    /* If parse fails, return NULL — the handler should handle that path */
    return doc;
}

bson_t* db_find(const char* collection_name, const bson_t* query) {
    (void)collection_name;
    (void)query;
    stub_db_find_call_count++;

    /* Build result as { "results": [ <stub_db_find_json item> ] } */
    bson_t* result = bson_new();
    bson_t child;
    BSON_APPEND_ARRAY_BEGIN(result, "results", &child);

    if (stub_db_find_json != NULL) {
        bson_error_t error;
        bson_t* item = bson_new_from_json(
            (const uint8_t*)stub_db_find_json, -1, &error);
        if (item) {
            BSON_APPEND_DOCUMENT(&child, "0", item);
            bson_destroy(item);
        }
    }

    bson_append_array_end(result, &child);
    return result;
}

bson_t* db_find_all(const char* collection_name) {
    return db_find(collection_name, NULL);
}
