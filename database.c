#define _GNU_SOURCE
#include "database.h"
#include "log-utils.h"

#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Internal state
 * --------------------------------------------------------------------------- */

#define DB_NAME "petstore"

static mongoc_client_pool_t* pool = NULL;
static mongoc_uri_t*         mongo_uri = NULL;

/* ---------------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------------- */

/**
 * @brief Pops a client from the pool and returns the requested collection.
 *        The caller MUST return the client with mongoc_client_pool_push().
 */
static mongoc_collection_t* get_collection(const char* name, mongoc_client_t** out_client) {
    *out_client = mongoc_client_pool_pop(pool);
    return mongoc_client_get_collection(*out_client, DB_NAME, name);
}

/* ---------------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------------- */

int db_init(const char* uri_string) {
    bson_error_t error;

    mongo_uri = mongoc_uri_new_with_error(uri_string, &error);
    if (!mongo_uri) {
        LOG_ERROR("Failed to parse URI '%s': %s", uri_string, error.message);
        return EXIT_FAILURE;
    }

    pool = mongoc_client_pool_new(mongo_uri);
    if (!pool) {
        LOG_ERROR("Failed to create client pool");
        mongoc_uri_destroy(mongo_uri);
        mongo_uri = NULL;
        return EXIT_FAILURE;
    }

    mongoc_client_pool_set_appname(pool, "petstore-api");

    /* Verify connectivity with a ping */
    mongoc_client_t* client = mongoc_client_pool_pop(pool);
    bson_t* ping = BCON_NEW("ping", BCON_INT32(1));
    bson_t reply;
    bool ok = mongoc_client_command_simple(client, "admin", ping, NULL, &reply, &error);
    bson_destroy(&reply);
    bson_destroy(ping);
    mongoc_client_pool_push(pool, client);

    if (!ok) {
        LOG_ERROR("MongoDB ping failed: %s", error.message);
        mongoc_client_pool_destroy(pool);
        mongoc_uri_destroy(mongo_uri);
        pool = NULL;
        mongo_uri = NULL;
        return EXIT_FAILURE;
    }

    LOG_INFO("Connected to MongoDB (%s)", uri_string);
    return EXIT_SUCCESS;
}

void db_cleanup(void) {
    if (pool) {
        mongoc_client_pool_destroy(pool);
        pool = NULL;
    }
    if (mongo_uri) {
        mongoc_uri_destroy(mongo_uri);
        mongo_uri = NULL;
    }
}

/* ---------------------------------------------------------------------------
 * CRUD operations
 * --------------------------------------------------------------------------- */

bool db_insert_one(const char* collection_name, const bson_t* doc) {
    mongoc_client_t* client;
    mongoc_collection_t* coll = get_collection(collection_name, &client);
    bson_error_t error;

    bool ok = mongoc_collection_insert_one(coll, doc, NULL, NULL, &error);
    if (!ok) {
        LOG_ERROR("Insert into '%s' failed: %s", collection_name, error.message);
    }

    mongoc_collection_destroy(coll);
    mongoc_client_pool_push(pool, client);
    return ok;
}

bson_t* db_find_one(const char* collection_name, const bson_t* filter) {
    mongoc_client_t* client;
    mongoc_collection_t* coll = get_collection(collection_name, &client);

    /* Limit to 1 document */
    bson_t* opts = BCON_NEW("limit", BCON_INT64(1));
    mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(coll, filter, opts, NULL);
    bson_destroy(opts);

    bson_t* result = NULL;
    const bson_t* doc;
    if (mongoc_cursor_next(cursor, &doc)) {
        result = bson_copy(doc);
    }

    bson_error_t error;
    if (mongoc_cursor_error(cursor, &error)) {
        LOG_ERROR("Find in '%s' failed: %s", collection_name, error.message);
    }

    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(coll);
    mongoc_client_pool_push(pool, client);
    return result;
}

char* db_find_as_json_array(const char* collection_name, const bson_t* filter) {
    mongoc_client_t* client;
    mongoc_collection_t* coll = get_collection(collection_name, &client);

    mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(coll, filter, NULL, NULL);

    size_t cap = 1024;
    size_t len = 0;
    char* buf = malloc(cap);
    if (!buf) {
        mongoc_cursor_destroy(cursor);
        mongoc_collection_destroy(coll);
        mongoc_client_pool_push(pool, client);
        return strdup("[]");
    }
    buf[len++] = '[';

    const bson_t* doc;
    bool first = true;
    while (mongoc_cursor_next(cursor, &doc)) {
        size_t jlen;
        char* j = bson_as_relaxed_extended_json(doc, &jlen);

        while (len + jlen + 3 > cap) {
            cap *= 2;
            char* tmp = realloc(buf, cap);
            if (!tmp) { 
                bson_free(j); 
                free(buf);
                mongoc_cursor_destroy(cursor);
                mongoc_collection_destroy(coll);
                mongoc_client_pool_push(pool, client);
                return strdup("[]"); 
            }
            buf = tmp;
        }

        if (!first) buf[len++] = ',';
        memcpy(buf + len, j, jlen);
        len += jlen;
        bson_free(j);
        first = false;
    }

    bson_error_t error;
    if (mongoc_cursor_error(cursor, &error)) {
        LOG_ERROR("Cursor error in '%s': %s", collection_name, error.message);
    }

    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(coll);
    mongoc_client_pool_push(pool, client);

    buf[len++] = ']';
    buf[len] = '\0';
    return buf;
}

bool db_update_one(const char* collection_name, const bson_t* filter, const bson_t* update) {
    mongoc_client_t* client;
    mongoc_collection_t* coll = get_collection(collection_name, &client);
    bson_error_t error;

    bool ok = mongoc_collection_update_one(coll, filter, update, NULL, NULL, &error);
    if (!ok) {
        LOG_ERROR("Update in '%s' failed: %s", collection_name, error.message);
    }

    mongoc_collection_destroy(coll);
    mongoc_client_pool_push(pool, client);
    return ok;
}

bool db_delete_one(const char* collection_name, const bson_t* filter) {
    mongoc_client_t* client;
    mongoc_collection_t* coll = get_collection(collection_name, &client);
    bson_error_t error;

    bool ok = mongoc_collection_delete_one(coll, filter, NULL, NULL, &error);
    if (!ok) {
        LOG_ERROR("Delete from '%s' failed: %s", collection_name, error.message);
    }

    mongoc_collection_destroy(coll);
    mongoc_client_pool_push(pool, client);
    return ok;
}