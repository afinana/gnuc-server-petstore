#include <stdbool.h>
#include <hiredis/hiredis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

#include "database.h" // Include the database header
#include "log-utils.h" // Include the log utils header

redisContext* redis_context = NULL;

#define REDIS_TIMEOUT 5


// Initialization and cleanup functions
/** 
 * @brief Parse the Redis URI to extract the host, port, and password.
 *
 * This function parses the Redis URI to extract the host, port, and password.
 *
 * @param redisURI The URI of the Redis database.
 * @param host The host name of the Redis database.
 * @param port The port number of the Redis database.
 * @param password The password of the Redis database.
 */
void parseRedisURI(const char* redisURI, char* host, int* port, char* password) {

	char temp[256];
	strcpy(temp, redisURI);

	char* uri = temp + strlen("redis://");
	char* atSign = strchr(uri, '@');
	if (atSign) {
		*atSign = '\0';
		char* passwordStart = strchr(uri, ':');
		if (passwordStart) {
			strcpy(password, passwordStart + 1);
		}
		else {
			password[0] = '\0';
		}
		strcpy(host, atSign + 1);
	}
	else {
		strcpy(host, uri);
		password[0] = '\0';
	}

	char* colon = strrchr(host, ':');
	if (colon) {
		*colon = '\0';
		*port = atoi(colon + 1);
	}
	else {
		*port = 6379;
	}
}
/**
 * @brief Initializes the database connection.
 *
 * This function initializes the connection to the database using the provided URI.
 *
 * @param redisURI The URI of the database to connect to.
 * @return int Returns 0 on success, 1 on failure.
 */
int db_init(const char* redisURI) {
	char host[128] = { 0 };
	int port = 6379;
	char password[128] = { 0 };
	struct timeval timeout = { 1, REDIS_TIMEOUT };

	parseRedisURI(redisURI, host, &port, password);

	redis_context = redisConnectWithTimeout(host, port, timeout);
	if (redis_context == NULL || redis_context->err) {
		if (redis_context) {
			LOG_ERROR("Connection error: %s", redis_context->errstr);
			redisFree(redis_context);
		}
		else {
			LOG_ERROR("Connection error: can't allocate redis context");
		}
		return(EXIT_FAILURE);
	}

	if (strlen(password) > 0) {
		redisReply* reply = redisCommand(redis_context, "AUTH %s", password);
		if (reply->type == REDIS_REPLY_ERROR) {
			printf("Authentication failed: %s\n", reply->str);
			freeReplyObject(reply);
			redisFree(redis_context);
			return(EXIT_FAILURE);
		}
		LOG_INFO("Authentication successful\n");
		freeReplyObject(reply);
	}
	return EXIT_SUCCESS;
}
/**
 * @brief Cleans up the database connection.
 *
 * This function closes the connection to the database and releases any resources
 * associated with it. It should be called when database operations are complete.
 */
void db_cleanup() {
	if (redis_context) {
		redisFree(redis_context);
	}
}




// Pet methods
/**
 * @brief Inserts a pet document into the specified collection.
 *
 * @param collection_name The name of the collection to insert the document into.
 * @param doc The document to insert.
 * @return bool Returns true on success, false on failure.
 */
bool db_pet_insert(const char* collection_name, const cJSON* doc) {
	cJSON* id_obj = cJSON_GetObjectItem(doc, "id");
	if (id_obj == NULL) {
		LOG_ERROR("Document does not contain an id");
		return false;
	}
	int id = id_obj->valueint;

	cJSON* status_obj = cJSON_GetObjectItem(doc, "status");
	if (status_obj == NULL) {
		LOG_ERROR("Document does not contain a status");
		return false;
	}

	redisReply* reply = redisCommand(redis_context, "SADD %s:%s %d", collection_name, status_obj->valuestring, id);
	if (reply == NULL) {
		LOG_ERROR("Insert status failed: %s", redis_context->errstr);
		return false;
	}

	cJSON* tags_obj = cJSON_GetObjectItem(doc, "tags");
	if (!store_tags(collection_name, tags_obj, id)) {
		return false;
	}

	reply = redisCommand(redis_context, "SADD %s %d", collection_name, id);
	if (reply == NULL) {
		LOG_ERROR("Insert failed: %s", redis_context->errstr);
		return false;
	}

	if (!store_document(collection_name, doc, id)) {
		return false;
	}
	/*
	while (redisGetReply(redis_context, (void*)&reply) == REDIS_OK) {
		// consume message
		LOG_INFO("Reply: %s", reply->str);
		freeReplyObject(reply);
	}
	*/
	return true;
}


/**
 * @brief Updates a pet document in the specified collection.
 *
 * @param collection_name The name of the collection to update the document in.
 * @param query The query to find the document to update.
 * @param update The update to apply to the document.
 * @return bool Returns true on success, false on failure.
 */
bool db_pet_update(const char* collection_name, const cJSON* update) {
	// get the id of update document
	cJSON* id_obj = cJSON_GetObjectItem(update, "id");
	if (id_obj == NULL) {
		LOG_ERROR("Update document does not contain an id");
		return false;
	}
	// delete the document from the database
	if (!db_pet_delete(collection_name, id_obj->valuestring)) {
		LOG_ERROR("Failed to delete document before updating");
		return false;
	}
	// insert the updated document
	if (!db_pet_insert(collection_name, update)) {
		LOG_ERROR("Failed to insert updated document");
		return false;
	}
	return true;
}

// User collection methods
/**
 * @brief Inserts a user document into the specified collection.
 *
 * @param collection_name The name of the collection to insert the document into.
 * @param doc The document to insert.
 * @return bool Returns true on success, false on failure.
 */
bool db_user_insert(const char* collection_name, const cJSON* doc) {

	// Get the id from the document
	cJSON* id_obj = cJSON_GetObjectItem(doc, "id");
	if (id_obj == NULL) {
		LOG_ERROR("Document does not contain an id");
		return false;
	}
	
	// store the document in collection_name:id
	char* json_str = cJSON_PrintUnformatted(doc);
	if (json_str == NULL) {
		LOG_ERROR("Failed to print JSON document");
		return false;
	}

	// create key = collection_name:id
	char* key = malloc(strlen(collection_name) + 20); // Allocate enough space for collection_name and integer value
	if (key == NULL) {
		LOG_ERROR("Memory allocation failed for key");
		free(json_str);
		return false;
	}
	
	// store the id in collection_name  
	redisReply*  reply = redisCommand(redis_context, "SADD %s %d", collection_name, id_obj->valueint);
	if (reply == NULL) {
		LOG_ERROR("Insert failed: %s", redis_context->errstr);
		return false;
	}
	freeReplyObject(reply);

	// create a index for the username
	cJSON* username_obj = cJSON_GetObjectItem(doc, "username");
	if (username_obj == NULL) {
		LOG_ERROR("Document does not contain a username");
		free(json_str);
		free(key);
		return false;
	}
	sprintf(key, "%s:%s", collection_name, username_obj->valuestring);
	
	
	reply = redisCommand(redis_context, "SET %s %d", key, id_obj->valueint);
	if (reply == NULL) {
		LOG_ERROR("Insert failed: %s", redis_context->errstr);
		free(json_str);
		free(key);
		return false;
	}
	free(key);
	freeReplyObject(reply);
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
bool db_user_update(const char* collection_name, const cJSON* update) {
   // get the id of update document
	cJSON* id_obj = cJSON_GetObjectItem(update, "id");
	if (id_obj == NULL) {
		LOG_ERROR("Update document does not contain an id");
		return false;
	}
	// delete the document from the database
	if (!db_user_delete(collection_name, id_obj->valuestring)) {
		LOG_ERROR("Failed to delete document before updating");
		return false;
	}
	// insert the updated document
	if (!db_user_insert(collection_name, update)) {
		LOG_ERROR("Failed to insert updated document");
		return false;
	}
	return true;
}


/**
 * @brief Deletes a document from the user collection.
 *
 * @param collection_name The name of the collection to delete the document from.
 * @param query The query to find the document to delete.
 * @return bool Returns true on success, false on failure.
 */
bool db_user_delete(const char* collection_name, const char* id) {
	cJSON* doc = db_find_one(collection_name, id);
	if (doc == NULL) {
		LOG_ERROR("Document not found");
		return false;
	}

	cJSON* id_obj = cJSON_GetObjectItem(doc, "id");
	if (id_obj == NULL) {
		LOG_ERROR("Document does not contain an id");
		return false;
	}
	int doc_id = id_obj->valueint;
	cJSON* status_obj = cJSON_GetObjectItem(doc, "username");
	if (!remove_document_from_field(collection_name, status_obj, doc_id)) {
		return false;
	}

	if (!remove_document_from_collection(collection_name, doc_id)) {
		return false;
	}

	return true;
}

/**
 * @brief Deletes a document from the specified collection.
 *
 * @param collection_name The name of the collection to delete the document from.
 * @param query The query to find the document to delete.
 * @return bool Returns true on success, false on failure.
 */
bool db_pet_delete(const char* collection_name, const char* id) {
	cJSON* doc = db_find_one(collection_name, id);
	if (doc == NULL) {
		LOG_ERROR("Document not found");
		return false;
	}

	cJSON* id_obj = cJSON_GetObjectItem(doc, "id");
	if (id_obj == NULL) {
		LOG_ERROR("Document does not contain an id");
		return false;
	}
	int doc_id = id_obj->valueint;
	cJSON* status_obj = cJSON_GetObjectItem(doc, "status");
	if (!remove_document_from_field(collection_name, status_obj, doc_id)) {
		return false;
	}

	if (!remove_document_from_tags(collection_name, doc, doc_id)) {
		return false;
	}

	if (!remove_document_from_collection(collection_name, doc_id)) {
		return false;
	}

	return true;
}

// Helper functions for pet methods

bool store_tags(const char* collection_name, const cJSON* tags_obj, int id) {
	if (tags_obj != NULL && cJSON_IsArray(tags_obj)) {
		int array_size = cJSON_GetArraySize(tags_obj);
		for (int i = 0; i < array_size; i++) {
			cJSON* tag = cJSON_GetArrayItem(tags_obj, i);
			cJSON* name_obj = cJSON_GetObjectItem(tag, "name");
			if (name_obj == NULL) {
				LOG_ERROR("Category does not contain a name");
				return false;
			}
			char* name = cJSON_GetStringValue(name_obj);
			if (name != NULL) {
				char* key = malloc(strlen(collection_name) + strlen(name) + 2);
				if (key == NULL) {
					LOG_ERROR("Memory allocation failed for key");
					return false;
				}
				sprintf(key, "%s:%s", collection_name, name);
				redisReply* reply = redisAppendCommand(redis_context, "SADD %s %d", key, id);
				if (reply == NULL) {
					LOG_ERROR("Insert failed: %s", redis_context->errstr);
					free(key);
					return false;
				}
				free(key);
				freeReplyObject(reply);
			}
		}
	}
	return true;
}


bool remove_document_from_tags(const char* collection_name, const cJSON* doc, int id) {
	cJSON* tags_obj = cJSON_GetObjectItem(doc, "tags");
	if (tags_obj != NULL && cJSON_IsArray(tags_obj)) {
		int array_size = cJSON_GetArraySize(tags_obj);
		for (int i = 0; i < array_size; i++) {
			cJSON* tag = cJSON_GetArrayItem(tags_obj, i);
			cJSON* name_obj = cJSON_GetObjectItem(tag, "name");
			if (name_obj == NULL) {
				LOG_ERROR("Category does not contain a name");
				return false;
			}
			char* name = cJSON_GetStringValue(name_obj);
			if (name != NULL) {
				char* key = malloc(strlen(collection_name) + strlen(name) + 2);
				if (key == NULL) {
					LOG_ERROR("Memory allocation failed for key");
					return false;
				}
				sprintf(key, "%s:%s", collection_name, name);
				redisReply* reply = redisCommand(redis_context, "SREM %s %d", key, id);
				if (reply == NULL) {
					LOG_ERROR("Delete failed: %s", redis_context->errstr);
					free(key);
					return false;
				}
				free(key);
				freeReplyObject(reply);
			}
		}
	}
	return true;
}


// Common methods for all collections

/**
 * @brief Finds a document in the specified collection that matches the query.
 *
 * @param collection_name The name of the collection to search.
 * @param query The query to find the document.
 * @return cJSON* A JSON document containing the results of the query.
 *         The caller is responsible for freeing the returned document.
 */
cJSON* db_find_one(const char* collection_name, const char* id) {
	// concat collection_name:id
	cJSON* result = NULL;
	char* key = malloc(strlen(collection_name) + strlen(id) + 2);
	sprintf(key, "%s:%s", collection_name, id);
    
    // For simplicity, assume query is the key
    redisReply* reply = redisCommand(redis_context, "GET %s", key);
    if (reply == NULL || reply->type != REDIS_REPLY_STRING) {
        if (reply) freeReplyObject(reply);
        return NULL;
    }
	// get the JSON document from the reply
	char* json = strdup(reply->str);	
    result = cJSON_Parse(json);

	freeReplyObject(reply);
	free(json);
	if (result == NULL) {
		LOG_ERROR("Failed to parse JSON");
		return NULL;
	}
    return result;
}

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
cJSON* db_find(const char* collection_name, const cJSON* query) {

	// LOG INFO query
	LOG_INFO("db_find executing query: %s", cJSON_PrintUnformatted(query));

	// get the operator, field and value
	cJSON* operator_obj = cJSON_GetObjectItem(query, "operator");
	if (operator_obj == NULL) {
		LOG_ERROR("Query does not contain an operator");
		return NULL;
	}
	cJSON* field_obj = cJSON_GetObjectItem(query, "field");
	if (field_obj == NULL) {
		LOG_ERROR("Query does not contain a field");
		return NULL;
	}
	cJSON* value_obj = cJSON_GetObjectItem(query, "value");
	if (value_obj == NULL) {
		LOG_ERROR("Query does not contain a value");
		return NULL;
	}
	// get the operator, field and value
	//char* operator = cJSON_GetStringValue(operator_obj);
	char* field = cJSON_GetStringValue(field_obj);
	if (field == NULL) {
		LOG_ERROR("Field is not a string");
		return NULL;	
	}
	// get the value
	if (cJSON_IsString(value_obj)) {
		char* value = cJSON_GetStringValue(value_obj);
		if (value == NULL) {
			LOG_ERROR("Value is not a string");
			return NULL;
		}
		// get all the keys from the collection_name:status
		redisReply* reply = redisCommand(redis_context, "HKEYS %s:%s", collection_name, value);
		if (reply == NULL) {
			LOG_ERROR("Find failed: %s", redis_context->errstr);
			return NULL;
		}
		// for each key in the collection_name:status and create an array of documents
		cJSON* result = cJSON_CreateArray();
		for (int i = 0; i < (int) reply->elements; i++) {
			// get the id from the key
			char* id = reply->element[i]->str;
			// get all keys of the document from collection_name:id
			redisReply* reply = redisCommand(redis_context, "HKEYS %s:%s", collection_name, id);
			if (reply == NULL) {
				LOG_ERROR("Find failed: %s", redis_context->errstr);
				return NULL;
			}
			// for each key in the collection_name:id get the value calling function db_find_one 
			// && create an array of documents
			cJSON* doc = db_find_one(collection_name, id);
			if (doc != NULL) {
				cJSON_AddItemToArray(result, doc);
			}
		}
		return result;
	}
	else if (cJSON_IsArray(value_obj)) {
		// get the array size
		int array_size = cJSON_GetArraySize(value_obj);
		// create a new cJSON object collection
		cJSON* result = cJSON_CreateArray();
		
		// iterate over the array
		for (int i = 0; i < array_size; i++) {
			// get the value from the array
			cJSON* value = cJSON_GetArrayItem(value_obj, i);
			if (value == NULL) {
				LOG_ERROR("Value is not a string");
				return NULL;
			}
			char key[20];
			sprintf(key, "%s:%s", collection_name, value->valuestring);
			// log redis command
			LOG_INFO("SMEMBERS %s", key);
			
			// get all the keys from the collection_name:tag and return an array of documents
			redisReply* reply = redisCommand(redis_context, "SMEMBERS %s", key);
			if (reply == NULL) {
				LOG_ERROR("Find failed: %s", redis_context->errstr);
				return NULL;
			}
			// for each key in the collection_name:tag and create an array of documents
			for (size_t i = 0; i < reply->elements; i++) {
				// get the id from the key
				char* set_id = reply->element[i]->str;
				LOG_INFO("Find collection: %s with ID: %s", collection_name, set_id);

				// for each key in the collection_name:id get the value calling function db_find_one 
				// && create an array of documents
				cJSON* doc = db_find_one(collection_name, set_id);
				if (doc == NULL) {
					LOG_ERROR("Document not found for ID: %s", set_id);
					continue;
				}
				cJSON_AddItemToArray(result, doc);
			
			} // for each element of SET
		
		}// for each state

		return result;
	}
	else {
		LOG_ERROR("Value is not a string or an array");
		return NULL;
	}
	

}

/**
 * @brief Finds all documents in the specified collection.
 *
 * @param collection_name The name of the collection to search.
 * @return cJSON* A JSON document containing the results of the query.
 *         The caller is responsible for freeing the returned document.
 */
cJSON* db_find_all(const char* collection_name) {
	// get all keys from the collection_name
	LOG_INFO("SMEMBERS %s", collection_name);

	redisReply* reply = redisCommand(redis_context, "SMEMBERS %s", collection_name);
	if (reply == NULL) {
		LOG_ERROR("Find failed: %s", redis_context->errstr);
		return NULL;
	}
	// create a new cJSON object collection
	cJSON* result = cJSON_CreateArray();
	// for each key in the collection_name and create an array of documents
	for (size_t i = 0; i < reply->elements; i++) {
		// get the id from the key
		char* id = reply->element[i]->str;	
		// for each key in the collection_name:id get the value calling function db_find_one 
		// && create an array of documents
		cJSON* doc = db_find_one(collection_name, id);
		if (doc != NULL) {
			cJSON_AddItemToArray(result, doc);
		}
	}
	return result;
}

// Common utils shared by all collections

bool remove_document_from_field(const char* collection_name, const cJSON* field_name, int id) {

	if (field_name != NULL) {
		redisReply* reply = redisCommand(redis_context, "SREM %s:%s %d", collection_name, field_name->valuestring, id);
		if (reply == NULL) {
			LOG_ERROR("Delete failed: %s", redis_context->errstr);
			return false;
		}
		freeReplyObject(reply);
	}
	return true;
}

bool remove_document_from_collection(const char* collection_name, int id) {
	redisReply* reply = redisCommand(redis_context, "HDEL %s %d", collection_name, id);
	if (reply == NULL) {
		LOG_ERROR("Delete failed: %s", redis_context->errstr);
		return false;
	}
	freeReplyObject(reply);
	return true;
}
bool store_document(const char* collection_name, const cJSON* doc, int id) {
	char* json_str = cJSON_PrintUnformatted(doc);
	if (json_str == NULL) {
		LOG_ERROR("Failed to print JSON document");
		return false;
	}

	char* key = malloc(strlen(collection_name) + 20);
	if (key == NULL) {
		LOG_ERROR("Memory allocation failed for key");
		free(json_str);
		return false;
	}
	sprintf(key, "%s:%d", collection_name, id);
	redisReply* reply = redisCommand(redis_context, "SET %s %s", key, json_str);
	if (reply == NULL) {
		LOG_ERROR("Insert failed: %s", redis_context->errstr);
		free(json_str);
		free(key);
		return false;
	}
	freeReplyObject(reply);
	free(key);
	free(json_str);
	return true;
}