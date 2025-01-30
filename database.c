#include <stdbool.h>
#include <hiredis/hiredis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

#include "database.h" // Include the database header
#include "log-utils.h" // Include the log utils header



redisContext* redis_context = NULL;

/**
 * @brief Initializes the database connection.
 *
 * @param uri The URI of the database to connect to.
 */
void db_init(const char* uri) {
    struct timeval timeout = { 1, 500000 }; // 1.5 seconds
    redis_context = redisConnectWithTimeout(uri, 6379, timeout);
    if (redis_context == NULL || redis_context->err) {
        if (redis_context) {
            LOG_ERROR("Connection error: %s", redis_context->errstr);
            redisFree(redis_context);
        }
        else {
            LOG_ERROR("Connection error: can't allocate redis context");
        }
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Cleans up the database connection.
 */
void db_cleanup() {
    if (redis_context) {
        redisFree(redis_context);
    }
}

/**
 * @brief Inserts a document into the specified collection.
 *
 * @param collection_name The name of the collection to insert the document into.
 * @param doc The document to insert.
 * @return bool Returns true on success, false on failure.
 */
bool db_insert(const char* collection_name, const cJSON* doc) {

	// Get the id from the document
	cJSON* id_obj = cJSON_GetObjectItem(doc, "id");
	if (id_obj == NULL) {
		LOG_ERROR("Document does not contain an id");
		return false;
	}
	// get the status from the document
	cJSON* status_obj = cJSON_GetObjectItem(doc, "status");
	if (status_obj == NULL) {
		LOG_ERROR("Document does not contain a status");
		return false;
	}
	// store collection:status with the id of document
	redisReply* reply = redisCommand(redis_context, "SADD %s:%s %d", collection_name, status_obj->valuestring, id_obj->valueint);
	if (reply == NULL) {
		LOG_ERROR("Insert status failed: %s", redis_context->errstr);
		return false;
	}
	freeReplyObject(reply);

	// get the tags array from the document and store the id in collection_name:tag
	cJSON* tags_obj = cJSON_GetObjectItem(doc, "tags");
	if (tags_obj != NULL) {
		if (cJSON_IsArray(tags_obj)) {
			int array_size = cJSON_GetArraySize(tags_obj);
			for (int i = 0; i < array_size; i++) {
				cJSON* tag = cJSON_GetArrayItem(tags_obj, i);
				// category is an object we should to get the name and the value
				cJSON* name_obj = cJSON_GetObjectItem(tag, "name");
				if (name_obj == NULL) {
					LOG_ERROR("Category does not contain a name");
					break;
				}
				// store the name in a variable
				char* name = cJSON_GetStringValue(name_obj);
				if (name != NULL) {
					//store id in collection_name:category
					char* key = malloc(strlen(collection_name) + strlen(name) + 2);
					if (key == NULL) {
						LOG_ERROR("Memory allocation failed for key");
						return false;
					}
					sprintf(key, "%s:%s", collection_name, name);
					// use key to store the id in REDIS 
					redisReply* reply = redisCommand(redis_context, "SADD %s:%d", key, id_obj->valueint);
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
	}

	// store the id in collection_name  
	reply = redisCommand(redis_context, "SADD %s %d", collection_name, id_obj->valueint);
	if (reply == NULL) {
		LOG_ERROR("Insert failed: %s", redis_context->errstr);
		return false;
	}
	freeReplyObject(reply);

	// store the document in collection_name:id
	char* json_str = cJSON_PrintUnformatted(doc);
	if (json_str == NULL) {
		LOG_ERROR("Failed to print JSON document");
		return false;
	}

	// create key = collection_name:id_obj->valueint
	char* key = malloc(strlen(collection_name) + 20); // Allocate enough space for collection_name and integer value
	if (key == NULL) {
		LOG_ERROR("Memory allocation failed for key");
		free(json_str);
		return false;
	}
	sprintf(key, "%s:%d", collection_name, id_obj->valueint);

	reply = redisCommand(redis_context, "SET %s %s", key, json_str);
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
bool db_update(const char* collection_name, const cJSON* update) {
   // get the id of update document
	cJSON* id_obj = cJSON_GetObjectItem(update, "id");
	if (id_obj == NULL) {
		LOG_ERROR("Update document does not contain an id");
		return false;
	}
	// delete the document from the database
	if (!db_delete(collection_name, id_obj->valuestring)) {
		LOG_ERROR("Failed to delete document before updating");
		return false;
	}
	// insert the updated document
	if (!db_insert(collection_name, update)) {
		LOG_ERROR("Failed to insert updated document");
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
bool db_delete(const char* collection_name, const char *id) {

	// find the document in the collection_name:id
	cJSON* doc = db_find_one(collection_name, id);
	if (doc == NULL) {
		LOG_ERROR("Document not found");
		return false;
	}

	// get the id of the document
	cJSON* id_obj = cJSON_GetObjectItem(doc, "id");
	if (id_obj == NULL) {
		LOG_ERROR("Document does not contain an id");
		return false;
	}
	// get the status of the document
	cJSON* status_obj = cJSON_GetObjectItem(doc, "status");
	if (status_obj == NULL) {
		LOG_ERROR("Document does not contain a status");
		return false;
	}
	
	// remove document id (SREM) from the set of key "collection_name:status"
	redisReply* reply = redisCommand(redis_context, "SREM %s:%s %d", collection_name, status_obj->valuestring, id_obj->valueint);
	if (reply == NULL) {
		LOG_ERROR("Delete failed: %s", redis_context->errstr);
		return false;
	}
	freeReplyObject(reply);

	
	// read tags array and delete the id from collection_name:tag
	cJSON* tags_obj = cJSON_GetObjectItem(doc, "tags");
	if (tags_obj != NULL) {
		if (cJSON_IsArray(tags_obj)) {
			int array_size = cJSON_GetArraySize(tags_obj);
			for (int i = 0; i < array_size; i++) {
				cJSON* tag = cJSON_GetArrayItem(tags_obj, i);
				cJSON* name_obj = cJSON_GetObjectItem(tag, "name");
				if (name_obj == NULL) {
					LOG_ERROR("Category does not contain a name");
					break;
				}
				char* name = cJSON_GetStringValue(name_obj);
				if (name != NULL) {
					char* key = malloc(strlen(collection_name) + strlen(name) + 2);
					sprintf(key, "%s:%s", collection_name, name);
					redisReply* reply = redisCommand(redis_context, "SREM %s %d", key, id_obj->valueint);
					if (reply == NULL) {
						LOG_ERROR("Delete failed: %s", redis_context->errstr);
					}
				}
			}
		}
	}
	// remove the id of collection_name HSET 
	reply = redisCommand(redis_context, "HDEL %s %d", collection_name, id_obj->valueint);
	if (reply == NULL) {
		LOG_ERROR("Delete failed: %s", redis_context->errstr);
		return false;
	}


	freeReplyObject(reply);
	return true;

}

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
	
	LOG_INFO("Pet found: %s", cJSON_PrintUnformatted(result));
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
			// get all the keys from the collection_name:tag and return an array of documents
			redisReply* reply = redisCommand(redis_context, "HKEYS %s:%s", collection_name, value->valuestring);
			if (reply == NULL) {
				LOG_ERROR("Find failed: %s", redis_context->errstr);
				return NULL;
			}
			// for each key in the collection_name:tag and create an array of documents
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
			
		}
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
	redisReply* reply = redisCommand(redis_context, "KEYS %s:*", collection_name);
	if (reply == NULL) {
		LOG_ERROR("Find failed: %s", redis_context->errstr);
		return NULL;
	}
	// create a new cJSON object collection
	cJSON* result = cJSON_CreateArray();
	// for each key in the collection_name and create an array of documents
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
