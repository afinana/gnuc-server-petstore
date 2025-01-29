#include <stdbool.h>
#include <hiredis/hiredis.h>
#include <stdio.h>
#include <stdlib.h>
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
    char* json_str = cJSON_PrintUnformatted(doc);
    redisReply* reply = redisCommand(redis_context, "SET %s %s", collection_name, json_str);
    free(json_str);

    if (reply == NULL) {
        LOG_ERROR("Insert failed: %s", redis_context->errstr);
        return false;
    }

    bool success = (reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "OK") == 0);
    freeReplyObject(reply);
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
bool db_update(const char* collection_name, const char* id, const cJSON* update) {
    // For simplicity, assume query is the key and update is the new value
    char* json_str = cJSON_PrintUnformatted(update);
	// concat collection_name:id
	char* key = malloc(strlen(collection_name) + strlen(id) + 2);
	sprintf(key, "%s:%s", collection_name, id);


    redisReply* reply = redisCommand(redis_context, "SET %s %s", key, json_str);
    free(json_str);

    if (reply == NULL) {
        LOG_ERROR("Update failed: %s", redis_context->errstr);
        return false;
    }

    bool success = (reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "OK") == 0);
    freeReplyObject(reply);
    return success;
}

/**
 * @brief Deletes a document from the specified collection.
 *
 * @param collection_name The name of the collection to delete the document from.
 * @param query The query to find the document to delete.
 * @return bool Returns true on success, false on failure.
 */
bool db_delete(const char* collection_name, const char *id) {
	
	// read the document from the database
	cJSON* doc = db_find_one(collection_name, id);
	if (doc == NULL) {
		LOG_ERROR("Failed to find document to delete");
		return false;
	}
    
    // concat collection_name:id
	char* key = malloc(strlen(collection_name) + strlen(id) + 2);
	sprintf(key, "%s:%s", collection_name, id);

    // For simplicity, assume query is the key
    redisReply* reply = redisCommand(redis_context, "DEL %s", id);

    if (reply == NULL) {
        LOG_ERROR("Delete failed: %s", redis_context->errstr);
        return false;
    }

    bool success = (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
    freeReplyObject(reply);
    return success;
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
	char* key = malloc(strlen(collection_name) + strlen(id) + 2);
	sprintf(key, "%s:%s", collection_name, id);
    
    // For simplicity, assume query is the key
    redisReply* reply = redisCommand(redis_context, "GET %s", collection_name);

    if (reply == NULL || reply->type != REDIS_REPLY_STRING) {
        if (reply) freeReplyObject(reply);
        return NULL;
    }

    cJSON* result = cJSON_Parse(reply->str);
    freeReplyObject(reply);
    return result;
}

/**
 * @brief Finds documents in the specified collection that match the query.
 *
 * @param collection_name The name of the collection to search.
 * @param query The query to find the documents.
 * @return cJSON* A JSON document containing the results of the query.
 *         The caller is responsible for freeing the returned document.
 */
cJSON* db_find(const char* collection_name, const cJSON* query) {
    // For simplicity, assume query is the key
    redisReply* reply = redisCommand(redis_context, "GET %s", collection_name);

    if (reply == NULL || reply->type != REDIS_REPLY_STRING) {
        if (reply) freeReplyObject(reply);
        return NULL;
    }

    cJSON* result = cJSON_Parse(reply->str);
    freeReplyObject(reply);
    return result;
}
