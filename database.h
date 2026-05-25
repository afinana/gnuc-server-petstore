#ifndef DATABASE_H
#define DATABASE_H

#include <stdbool.h>
#include <mongoc/mongoc.h>

/**
 * @brief Initializes the MongoDB connection pool.
 *
 * @param uri The MongoDB connection URI string.
 */
void db_init(const char* uri);

/**
 * @brief Cleans up the MongoDB connection pool and resources.
 */
void db_cleanup(void);

/**
 * @brief Inserts a BSON document into the specified collection.
 *
 * @param collection_name The name of the collection to insert into.
 * @param doc The BSON document to insert.
 * @return bool Returns true on success, false on failure.
 */
bool db_insert(const char* collection_name, const bson_t* doc);

/**
 * @brief Updates a document in the specified collection.
 *
 * @param collection_name The name of the collection.
 * @param query The BSON query to match the document.
 * @param update The BSON update document.
 * @return bool Returns true on success, false on failure.
 */
bool db_update(const char* collection_name, const bson_t* query, const bson_t* update);

/**
 * @brief Deletes a document from the specified collection.
 *
 * @param collection_name The name of the collection.
 * @param query The BSON query to match the document to delete.
 * @return bool Returns true on success, false on failure.
 */
bool db_delete(const char* collection_name, const bson_t* query);

/**
 * @brief Finds a single document matching the query.
 *
 * @param collection_name The name of the collection to search.
 * @param query The BSON query to match.
 * @return bson_t* A copy of the matching document, or NULL if not found.
 *         The caller is responsible for freeing with bson_destroy().
 */
bson_t* db_find_one(const char* collection_name, const bson_t* query);

/**
 * @brief Finds all documents matching the query.
 *
 * @param collection_name The name of the collection to search.
 * @param query The BSON query to match.
 * @return bson_t* A BSON document containing an array of results.
 *         The caller is responsible for freeing with bson_destroy().
 */
bson_t* db_find(const char* collection_name, const bson_t* query);

/**
 * @brief Finds all documents in the specified collection.
 *
 * @param collection_name The name of the collection to search.
 * @return bson_t* A BSON document containing an array of all documents.
 *         The caller is responsible for freeing with bson_destroy().
 */
bson_t* db_find_all(const char* collection_name);

#endif // DATABASE_H
