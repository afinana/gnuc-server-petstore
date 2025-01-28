#include <stdbool.h>
#include <mongoc/mongoc.h>
#include <bson/bson.h>
#include <stdio.h>
#include <stdlib.h>
#include "database.h"

mongoc_client_t* client = NULL;
mongoc_collection_t* collection = NULL;

/**
 * @brief Initializes the database connection.
 * 
 * @param uri The URI of the database to connect to.
 */
void db_init(const char* uri) {
    mongoc_init();
    client = mongoc_client_new(uri);
    if (!client) {
        fprintf(stderr, "Failed to initialize MongoDB client\n");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Cleans up the database connection.
 */
void db_cleanup() {
    if (client) {
        mongoc_client_destroy(client);
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
    collection = mongoc_client_get_collection(client, "petstore", collection_name);
    if (!mongoc_collection_insert_one(collection, doc, NULL, NULL, &error)) {
        fprintf(stderr, "Insert failed: %s\n", error.message);
        mongoc_collection_destroy(collection);
        return false;
    }
    mongoc_collection_destroy(collection);
    return true;
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
    collection = mongoc_client_get_collection(client, "petstore", collection_name);
    if (!mongoc_collection_update_one(collection, query, update, NULL, NULL, &error)) {
        fprintf(stderr, "Update failed: %s\n", error.message);
        mongoc_collection_destroy(collection);
        return false;
    }
    mongoc_collection_destroy(collection);
    return true;
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
    collection = mongoc_client_get_collection(client, "petstore", collection_name);

    if (!mongoc_collection_delete_one(collection, query, NULL, &reply, &error)) {
        fprintf(stderr, "Delete failed: %s\n", error.message);
        bson_destroy(&reply);
        mongoc_collection_destroy(collection);
        return false;
    }
    bson_destroy(&reply);
    mongoc_collection_destroy(collection);
    return true;
}


/**
 * @brief Finds document in the specified collection that match the query.
 * 
 * @param collection_name The name of the collection to search.
 * @param query The query to find the document.
 * @return bson_t* A BSON document containing the results of the query. 
 *         The caller is responsible for freeing the returned document.
 */
bson_t* db_find_one(const char* collection_name, const bson_t* query) {
    mongoc_cursor_t* cursor;
    const bson_t* doc;
    bson_t* result = NULL;

    collection = mongoc_client_get_collection(client, "petstore", collection_name);
    cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL);

    // iterate for all items
    if (mongoc_cursor_next(cursor, &doc)) {
        result = bson_copy(doc);
    }

    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(collection);
    return result;
}

/**
 * @brief Finds documents in the specified collection that match the query.
 *
 * @param collection_name The name of the collection to search.
 * @param query The query to find the documents.
 * @return bson_t* A BSON document containing the results of the query.
 *         The caller is responsible for freeing the returned document.
 */

bson_t* db_find(const char* collection_name, const bson_t* query) {
    mongoc_cursor_t* cursor;
    const bson_t* doc;
    bson_t* result = bson_new();
    bson_t child;

    collection = mongoc_client_get_collection(client, "petstore", collection_name);
    cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL);

    BSON_APPEND_ARRAY_BEGIN(result, "collection", &child);
    int index = 0;
    char key[16];

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
    return result;
}
