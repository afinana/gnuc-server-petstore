#ifndef DATABASE_H
#define DATABASE_H

#include <stdbool.h> // Include this header if you are working in a C environment
#include <cjson/cJSON.h> // Include cJSON header

/**
 * @brief Initializes the database connection.
 *
 * This function initializes the connection to the database using the provided URI.
 *
 * @param redisURI The URI of the database to connect to.
 * @return int Returns 0 on success, 1 on failure.
 */
int db_init(const char* redisURI);

/**
 * @brief Cleans up the database connection.
 *
 * This function closes the connection to the database and releases any resources
 * associated with it. It should be called when database operations are complete.
 */
void db_cleanup();

/**
 * @brief Inserts a pet document into the specified collection.
 *
 * This function inserts a JSON document into the specified collection in the database.
 *
 * @param collection_name The name of the collection to insert the document into.
 * @param doc The JSON document to insert.
 * @return bool Returns true on success, false on failure.
 */
bool db_pet_insert(const char* collection_name, const cJSON* doc);

/**
 * @brief Inserts a user document into the specified collection.
 *
 * This function inserts a JSON document into the specified collection in the database.
 *
 * @param collection_name The name of the collection to insert the document into.
 * @param doc The JSON document to insert.
 * @return bool Returns true on success, false on failure.
 */
bool db_user_insert(const char* collection_name, const cJSON* doc);

/**
 * @brief Updates a pet document in the specified collection.
 *
 * This function updates a document in the specified collection based on the provided update.
 * The update is applied to the document that matches the id.
 *
 * @param collection_name The name of the collection to update the document in.
 * @param update The update to apply to the document.
 * @return bool Returns true on success, false on failure.
 */
bool db_pet_update(const char* collection_name, const cJSON* update);

/**
 * @brief Updates a user document in the specified collection.
 *
 * This function updates a document in the specified collection based on the provided update.
 * The update is applied to the document that matches the id.
 *
 * @param collection_name The name of the collection to update the document in.
 * @param update The update to apply to the document.
 * @return bool Returns true on success, false on failure.
 */
bool db_user_update(const char* collection_name, const cJSON* update);

/**
 * @brief Deletes a document from the pet collection.
 *
 * This function deletes a document from the specified collection based on the provided id.
 *
 * @param collection_name The name of the collection to delete the document from.
 * @param id The id of the document to delete.
 * @return bool Returns true on success, false on failure.
 */
bool db_pet_delete(const char* collection_name, const char* id);

/**
 * @brief Deletes a document from the user collection.
 *
 * This function deletes a document from the specified collection based on the provided id.
 *
 * @param collection_name The name of the collection to delete the document from.
 * @param id The id of the document to delete.
 * @return bool Returns true on success, false on failure.
 */
bool db_user_delete(const char* collection_name, const char* id);

/**
 * @brief Finds documents in the specified collection that match the query.
 *
 * @param collection_name The name of the collection to search.
 * @param query The query to find the documents.
 * Example:
 * 1. { "operator": "eq", "field": "collection.status", "value": "active" }
 * 2. { "operator": "eq", "field": "collection.tags.name", "value": ["tag1", "tag2"] }
 * @return cJSON* A JSON document containing the results of the query.
 *         The caller is responsible for freeing the returned document.
 */
cJSON* db_find(const char* collection_name, const cJSON* query);

/**
 * @brief Finds a document in the specified collection that matches the id.
 *
 * This function searches for a single document in the specified collection that matches the provided id.
 * It returns a JSON document containing the result of the query.
 *
 * @param collection_name The name of the collection to search.
 * @param id The id of the document to find.
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

// Helper functions for pet methods
bool store_tags(const char* collection_name, const cJSON* tags_obj, int id, int* num_op);
bool store_document(const char* collection_name, const cJSON* doc, int id);
bool remove_document_from_collection(const char* collection_name, int id, int* op_num);
bool remove_document_from_field(const char* field_id, const cJSON* field_name, int id, int* op_num);
bool remove_document_from_tags(const char* collection_name, const cJSON* doc, int id, int* op_number);

#endif // DATABASE_H
