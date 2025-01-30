#ifndef DATABASE_H
#define DATABASE_H

#include <stdbool.h> // Include this header if you are working in a C environment
#include <cjson/cJSON.h> // Include cJSON header

/**
 * @brief Initializes the database connection.
 *
 * This function establishes a connection to the database specified by the URI.
 * It must be called before any other database operations.
 *
 * @param uri The URI of the database to connect to.
 */
void db_init(const char* uri);

/**
 * @brief Cleans up the database connection.
 *
 * This function closes the connection to the database and releases any resources
 * associated with it. It should be called when database operations are complete.
 */
void db_cleanup();

/**
 * @brief Inserts a document into the specified collection.
 *
 * This function inserts a JSON document into the specified collection in the database.
 *
 * @param collection_name The name of the collection to insert the document into.
 * @param doc The JSON document to insert.
 * @return bool Returns true on success, false on failure.
 */
bool db_insert(const char* collection_name, const cJSON* doc);

/**
 * @brief Updates a document in the specified collection.
 *
 * This function updates a document in the specified collection based on the provided query.
 * The update is applied to the document(s) that match the query.
 *
 * @param collection_name The name of the collection to update the document in.
 * @param query The query to find the document to update.
 * @param update The update to apply to the document.
 * @return bool Returns true on success, false on failure.
 */
bool db_update(const char* collection_name, const cJSON* update);

/**
 * @brief Deletes a document from the specified collection.
 *
 * This function deletes a document from the specified collection based on the provided query.
 *
 * @param collection_name The name of the collection to delete the document from.
 * @param query The query to find the document to delete.
 * @return bool Returns true on success, false on failure.
 */
bool db_delete(const char* collection_name, const char* id);

/**
 * @brief Finds documents in the specified collection that match the query.
 *
 * @param collection_name The name of the collection to search.
 * @param query The query to find the documents.
 * Example:
	1. { "operator": "eq", "field": "collection.status", "value": "active" }
	2. { "operator": "eq", "field": "collection.tags.name", "value": ["tag1", "tag2"] }
 * @return cJSON* A JSON document containing the results of the query.
 *         The caller is responsible for freeing the returned document.
 */
cJSON* db_find(const char* collection_name, const cJSON* query);

/**
 * @brief Finds a document in the specified collection that matches the query.
 *
 * This function searches for a single document in the specified collection that matches the provided query.
 * It returns a JSON document containing the result of the query.
 *
 * @param collection_name The name of the collection to search.
 * @param query The query to find the document.
 * @return cJSON* A JSON document containing the result of the query.
 *         The caller is responsible for freeing the returned document.
 */
cJSON* db_find_one(const char* collection_name, const char* id);

/**
 * @brief Finds all documents in the specified collection.
 *
 * @param collection_name The name of the collection to search.
 * @return cJSON* A JSON document containing the results of the query.
 *         The caller is responsible for freeing the returned document.
 */
cJSON* db_find_all(const char* collection_name);

#endif
