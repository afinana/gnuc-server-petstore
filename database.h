#ifndef DATABASE_H
#define DATABASE_H

#include <stdbool.h>
#include <mongoc/mongoc.h>

/**
 * @brief Initializes the MongoDB connection.
 *
 * Calls mongoc_init(), creates a client pool from the URI,
 * and verifies the connection with a ping.
 *
 * @param uri The MongoDB connection URI.
 * @return int Returns 0 on success, 1 on failure.
 */
int db_init(const char* uri);

/**
 * @brief Cleans up the MongoDB connection.
 *
 * Destroys the client pool and calls mongoc_cleanup().
 */
void db_cleanup(void);

/**
 * @brief Inserts a single document into the specified collection.
 *
 * @param collection_name The name of the collection.
 * @param doc The BSON document to insert. Ownership is NOT transferred.
 * @return true on success, false on failure.
 */
bool db_insert_one(const char* collection_name, const bson_t* doc);

/**
 * @brief Finds a single document matching the filter.
 *
 * @param collection_name The name of the collection.
 * @param filter The BSON query filter. Ownership is NOT transferred.
 * @return bson_t* A heap-allocated BSON document (caller must bson_destroy + bson_free),
 *         or NULL if not found.
 */
bson_t* db_find_one(const char* collection_name, const bson_t* filter);

/**
 * @brief Finds all documents matching the filter and returns as a JSON array string.
 *
 * @param collection_name The name of the collection.
 * @param filter The BSON query filter.
 * @return char* A heap-allocated JSON string, or "[]" on failure (caller must free).
 */
char* db_find_as_json_array(const char* collection_name, const bson_t* filter);

/**
 * @brief Updates a single document matching the filter.
 *
 * @param collection_name The name of the collection.
 * @param filter The BSON query filter.
 * @param update The BSON update document (should contain $set, etc.).
 * @return true on success, false on failure.
 */
bool db_update_one(const char* collection_name, const bson_t* filter, const bson_t* update);

/**
 * @brief Deletes a single document matching the filter.
 *
 * @param collection_name The name of the collection.
 * @param filter The BSON query filter.
 * @return true on success, false on failure.
 */
bool db_delete_one(const char* collection_name, const bson_t* filter);

#endif /* DATABASE_H */
