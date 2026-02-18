#include <stdbool.h>
#include <mongoc/mongoc.h>
#include <bson/bson.h>
#include <stdio.h>
#include <stdlib.h>
#include "database.h"
#include "log-utils.h"

// Connection pool for thread-safe concurrent access
static mongoc_client_pool_t* pool = NULL;
static mongoc_uri_t* mongo_uri = NULL;

/**
 * @brief Initializes the database connection pool.
 *
 * @param uri The URI of the database to connect to.
 */
void db_init(const char* uri) {
    mongoc_init();

    bson_error_t error;
    mongo_uri = mongoc_uri_new_with_error(uri, &error);
    if (!mongo_uri) {
        LOG_ERROR("Failed to parse MongoDB URI: %s", error.message);
        exit(EXIT_FAILURE);
    }

    pool = mongoc_client_pool_new(mongo_uri);
    if (!pool) {
        LOG_ERROR("Failed to create MongoDB connection pool");
        mongoc_uri_destroy(mongo_uri);
        exit(EXIT_FAILURE);
    }

    mongoc_client_pool_max_size(pool, 10);
}

/**
 * @brief Cleans up the database connection pool.
 */
void db_cleanup() {
    if (pool) {
        mongoc_client_pool_destroy(pool);
    }
    if (mongo_uri) {
        mongoc_uri_destroy(mongo_uri);
    }
    mongoc_cleanup();
}

/**
 * @brief Inserts a document into the specified collection.
 *
 * @param collection_name The name of the collection to insert the document into.
 * @param doc The document to insert.
 * @return bool Returns true on success, false on failure.
 */
bool db_insert(const char* collection_name, const bson_t* doc) {
    bson_error_t error;
    mongoc_client_t* client = mongoc_client_pool_pop(pool);
    mongoc_collection_t* collection = mongoc_client_get_collection(client, "petstore", collection_name);

    bool success = mongoc_collection_insert_one(collection, doc, NULL, NULL, &error);
    if (!success) {
        LOG_ERROR("Insert failed: %s", error.message);
    }

    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(pool, client);
    return success;
}

/**
 * @brief Updates a document in the specified collection.
 *
 * @param collection_name The name of the collection to update the document in.
 * @param query The query to find the document to update.
 * @param update The update to apply to the document.
 * @return bool Returns true on success, false on failure.
 */
bool db_update(const char* collection_name, const bson_t* query, const bson_t* update) {
    bson_error_t error;
    mongoc_client_t* client = mongoc_client_pool_pop(pool);
    mongoc_collection_t* collection = mongoc_client_get_collection(client, "petstore", collection_name);

    bool success = mongoc_collection_update_one(collection, query, update, NULL, NULL, &error);
    if (!success) {
        LOG_ERROR("Update failed: %s", error.message);
    }

    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(pool, client);
    return success;
}

/**
 * @brief Deletes a document from the specified collection.
 *
 * @param collection_name The name of the collection to delete the document from.
 * @param query The query to find the document to delete.
 * @return bool Returns true on success, false on failure.
 */
bool db_delete(const char* collection_name, const bson_t* query) {
    bson_error_t error;
    bson_t reply;
    mongoc_client_t* client = mongoc_client_pool_pop(pool);
    mongoc_collection_t* collection = mongoc_client_get_collection(client, "petstore", collection_name);

    bool success = mongoc_collection_delete_one(collection, query, NULL, &reply, &error);
    if (!success) {
        LOG_ERROR("Delete failed: %s", error.message);
    }

    bson_destroy(&reply);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(pool, client);
    return success;
}

/**
 * @brief Finds a single document in the specified collection that matches the query.
 *
 * @param collection_name The name of the collection to search.
 * @param query The query to find the document.
 * @return bson_t* A BSON document containing the result.
 *         The caller is responsible for freeing the returned document.
 */
bson_t* db_find_one(const char* collection_name, const bson_t* query) {
    bson_t* result = NULL;
    mongoc_client_t* client = mongoc_client_pool_pop(pool);
    mongoc_collection_t* collection = mongoc_client_get_collection(client, "petstore", collection_name);
    mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL);

    const bson_t* doc;
    if (mongoc_cursor_next(cursor, &doc)) {
        result = bson_copy(doc);
    }

    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(pool, client);
    return result;
}

/**
 * @brief Finds documents in the specified collection that match the query.
 *
 * @param collection_name The name of the collection to search.
 * @param query The query to find the documents.
 * @return bson_t* A BSON array document containing the results.
 *         The caller is responsible for freeing the returned document.
 */
bson_t* db_find(const char* collection_name, const bson_t* query) {
    bson_t* result = bson_new();
    bson_t child;
    mongoc_client_t* client = mongoc_client_pool_pop(pool);
    mongoc_collection_t* collection = mongoc_client_get_collection(client, "petstore", collection_name);
    mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL);

    BSON_APPEND_ARRAY_BEGIN(result, "collection", &child);
    int index = 0;
    char key[16];
    const bson_t* doc;

    while (mongoc_cursor_next(cursor, &doc)) {
        snprintf(key, sizeof(key), "%d", index++);
        BSON_APPEND_DOCUMENT(&child, key, doc);
    }
    bson_append_array_end(result, &child);

    if (mongoc_cursor_error(cursor, NULL)) {
        bson_destroy(result);
        result = NULL;
    }

    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(pool, client);
    return result;
}
