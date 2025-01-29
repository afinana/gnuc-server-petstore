#ifndef DATABASE_H
#define DATABASE_H

#include <stdbool.h> // Include this header if you are working in a C environment
#include <bson/bson.h> // Ensure you include the header where bson_t is defined

/**
 * @brief Initializes the database connection.
 * 
 * @param uri The URI of the database to connect to.
 */
void db_init(const char* uri);

/**
 * @brief Cleans up the database connection.
 */
void db_cleanup();

/**
 * @brief Inserts a document into the specified collection.
 * 
 * @param collection_name The name of the collection to insert the document into.
 * @param doc The document to insert.
 * @return bool Returns true on success, false on failure.
 */
bool db_insert(const char* collection_name, const bson_t* doc);

/**
 * @brief Updates a document in the specified collection.
 * 
 * @param collection_name The name of the collection to update the document in.
 * @param query The query to find the document to update.
 * @param update The update to apply to the document.
 * @return bool Returns true on success, false on failure.
 */
bool db_update(const char* collection_name, const bson_t* query, const bson_t* update);

/**
 * @brief Deletes a document from the specified collection.
 * 
 * @param collection_name The name of the collection to delete the document from.
 * @param query The query to find the document to delete.
 * @return bool Returns true on success, false on failure.
 */
bool db_delete(const char* collection_name, const bson_t* query);

/**
 * @brief Finds document in the specified collection that match the query.
 * 
 * @param collection_name The name of the collection to search.
 * @param query The query to find the document.
 * @return bson_t* A BSON document containing the results of the query. 
 *         The caller is responsible for freeing the returned document.
 */
bson_t* db_find(const char* collection_name, const bson_t* query);

/**
 * @brief Find document in the specified collection that match the query.
 *
 * @param collection_name The name of the collection to search.
 * @param query The query to find the document.
 * @return bson_t* A BSON document containing the results of the query.
 *         The caller is responsible for freeing the returned document.
 */
bson_t* db_find_one(const char* collection_name, const bson_t* query);


#endif