#ifndef DATABASE_H
#define DATABASE_H

#include <stdbool.h>
#include <bson/bson.h>

/**
 * @brief Initializes the Redis connection.
 *
 * Parses a Redis URI (redis://[:password@]host:port/) and establishes
 * the connection, authenticating if a password is present.
 *
 * @param uri The Redis connection URI string.
 */
void db_init(const char* uri);

/**
 * @brief Cleans up the Redis connection and resources.
 */
void db_cleanup(void);

/**
 * @brief Inserts a BSON document into the specified collection.
 *
 * Stores the document as a JSON string in a Redis Hash keyed by
 * {collection}:{id}. Also maintains secondary index sets for fields
 * like "status", "tags", and "username".
 *
 * @param collection_name The name of the collection (key prefix).
 * @param doc The BSON document to insert.
 * @return bool Returns true on success, false on failure.
 */
bool db_insert(const char* collection_name, const bson_t* doc);

/**
 * @brief Updates a document in the specified collection.
 *
 * Extracts the query "id" to locate the existing record, then applies
 * the update (expects a "$set" wrapper consistent with MongoDB-style
 * update documents).
 *
 * @param collection_name The name of the collection (key prefix).
 * @param query The BSON query to match the document.
 * @param update The BSON update document (with "$set" wrapper).
 * @return bool Returns true on success, false on failure.
 */
bool db_update(const char* collection_name, const bson_t* query, const bson_t* update);

/**
 * @brief Deletes a document from the specified collection.
 *
 * Removes the Redis Hash and cleans up any secondary index entries.
 *
 * @param collection_name The name of the collection (key prefix).
 * @param query The BSON query to match the document to delete.
 * @return bool Returns true on success, false on failure.
 */
bool db_delete(const char* collection_name, const bson_t* query);

/**
 * @brief Finds a single document matching the query.
 *
 * Supports lookup by "id" (direct key), "username" (via secondary index),
 * and "status" (returns first match from index set).
 *
 * @param collection_name The name of the collection (key prefix).
 * @param query The BSON query to match.
 * @return bson_t* A copy of the matching document, or NULL if not found.
 *         The caller is responsible for freeing with bson_destroy().
 */
bson_t* db_find_one(const char* collection_name, const bson_t* query);

/**
 * @brief Finds all documents matching the query.
 *
 * Supports queries by "status" and "tags.name" (using "$in" operator)
 * via Redis Set-based secondary indexes.
 *
 * @param collection_name The name of the collection (key prefix).
 * @param query The BSON query to match.
 * @return bson_t* A BSON document containing an array of results.
 *         The caller is responsible for freeing with bson_destroy().
 */
bson_t* db_find(const char* collection_name, const bson_t* query);

/**
 * @brief Finds all documents in the specified collection.
 *
 * Uses Redis SCAN to iterate over all keys matching the collection prefix.
 *
 * @param collection_name The name of the collection (key prefix).
 * @return bson_t* A BSON document containing an array of all documents.
 *         The caller is responsible for freeing with bson_destroy().
 */
bson_t* db_find_all(const char* collection_name);

#endif // DATABASE_H
