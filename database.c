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

/**
 * @brief Helper function to free redisReply and log error
 *
 * @param reply The redisReply object to be freed
 * @param errorMsg The error message to be logged
 */
void freeReplyAndLogError(redisReply* reply, const char* errorMsg) {
    if (reply) {
        freeReplyObject(reply);
    }
    LOG_ERROR("%s", errorMsg);
}

/**
 * @brief Parse the Redis URI to extract host, port, and password
 *
 * @param redisURI The Redis URI string
 * @param host The extracted host
 * @param port The extracted port
 * @param password The extracted password
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
 * @brief Initialize the database connection
 *
 * @param redisURI The Redis URI string
 * @return int EXIT_SUCCESS on success, EXIT_FAILURE on failure
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
        return EXIT_FAILURE;
    }

    if (strlen(password) > 0) {
        redisReply* reply = redisCommand(redis_context, "AUTH %s", password);
        if (reply->type == REDIS_REPLY_ERROR) {
            freeReplyAndLogError(reply, "Authentication failed");
            redisFree(redis_context);
            return EXIT_FAILURE;
        }
        LOG_INFO("Authentication successful\n");
        freeReplyObject(reply);
    }
    return EXIT_SUCCESS;
}

/**
 * @brief Cleanup the database connection
 */
void db_cleanup() {
    if (redis_context) {
        redisFree(redis_context);
    }
}

/**
 * @brief Helper function to process redis replies
 *
 * @param op_num The number of operations to process
 * @return true on success, false on failure
 */
bool processRedisReplies(int op_num) {
    redisReply* reply = NULL;
    for (int i = 0; i < op_num; i++) {
        int resultCode = redisGetReply(redis_context, (void**)&reply);
        if (resultCode == REDIS_OK) {
            LOG_INFO("Response [%d]: %s", i, reply->str ? reply->str : "(nil)");
            freeReplyObject(reply);
        }
        else {
            freeReplyAndLogError(reply, "Error processing redis reply");
            return false;
        }
    }
    return true;
}

/**
 * @brief Insert a pet document into the database
 *
 * @param collection_name The name of the collection
 * @param doc The JSON document to insert
 * @return true on success, false on failure
 */
bool db_pet_insert(const char* collection_name, const cJSON* doc) {
    int op_num = 0;

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

    LOG_INFO("SADD %s:%s:%s %d", collection_name, "status", status_obj->valuestring, id);
    redisAppendCommand(redis_context, "SADD %s:%s:%s %d", collection_name, "status", status_obj->valuestring, id);
    op_num++;

    cJSON* tags_obj = cJSON_GetObjectItem(doc, "tags");
    if (!store_tags(collection_name, tags_obj, id, &op_num)) {
        return false;
    }

    char* key = malloc(strlen(collection_name) + 20);
    if (key == NULL) {
        LOG_ERROR("Memory allocation failed for key");
        return false;
    }

    sprintf(key, "%s:%s", collection_name, collection_name);
    LOG_INFO("SADD %s %d", key, id_obj->valueint);
    redisAppendCommand(redis_context, "SADD %s %d", key, id_obj->valueint);
    op_num++;
    free(key);

    if (!store_document(collection_name, doc, id)) {
        return false;
    }
    op_num++;

    return processRedisReplies(op_num);
}

/**
 * @brief Update a pet document in the database
 *
 * @param collection_name The name of the collection
 * @param update The JSON document to update
 * @return true on success, false on failure
 */
bool db_pet_update(const char* collection_name, const cJSON* update) {
    cJSON* id_obj = cJSON_GetObjectItem(update, "id");
    if (id_obj == NULL) {
        LOG_ERROR("Update document does not contain an id");
        return false;
    }

    if (!db_pet_delete(collection_name, id_obj->valuestring)) {
        LOG_ERROR("Failed to delete document before updating");
        return false;
    }

    if (!db_pet_insert(collection_name, update)) {
        LOG_ERROR("Failed to insert updated document");
        return false;
    }
    return true;
}

/**
 * @brief Insert a user document into the database
 *
 * @param collection_name The name of the collection
 * @param doc The JSON document to insert
 * @return true on success, false on failure
 */
bool db_user_insert(const char* collection_name, const cJSON* doc) {
    int op_num = 0;

    cJSON* id_obj = cJSON_GetObjectItem(doc, "id");
    if (id_obj == NULL) {
        LOG_ERROR("Document does not contain an id");
        return false;
    }

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
    sprintf(key, "%s:%d", collection_name, id_obj->valueint);
    LOG_INFO("SET %s %s", key, json_str);
    redisAppendCommand(redis_context, "SET %s %s", key, json_str);
    op_num++;

    memset(key, 0, strlen(collection_name) + 20);
    sprintf(key, "%s:%s", collection_name, collection_name);
    LOG_INFO("SADD %s %d", key, id_obj->valueint);
    redisAppendCommand(redis_context, "SADD %s %d", key, id_obj->valueint);
    op_num++;

    cJSON* username_obj = cJSON_GetObjectItem(doc, "username");
    if (username_obj == NULL) {
        LOG_ERROR("Document does not contain a username");
        free(json_str);
        return false;
    }

    memset(key, 0, strlen(collection_name) + 20);
    sprintf(key, "%s:%s:%s", collection_name, "username", username_obj->valuestring);
    LOG_INFO("SADD %s %d", key, id_obj->valueint);
    redisAppendCommand(redis_context, "SADD %s %d", key, id_obj->valueint);
    op_num++;

    free(key);
    free(json_str);

    return processRedisReplies(op_num);
}

/**
 * @brief Update a user document in the database
 *
 * @param collection_name The name of the collection
 * @param update The JSON document to update
 * @return true on success, false on failure
 */
bool db_user_update(const char* collection_name, const cJSON* update) {
    cJSON* id_obj = cJSON_GetObjectItem(update, "id");
    if (id_obj == NULL) {
        LOG_ERROR("Update document does not contain an id");
        return false;
    }

    if (!db_user_delete(collection_name, id_obj->valuestring)) {
        LOG_ERROR("Failed to delete document before updating");
        return false;
    }

    if (!db_user_insert(collection_name, update)) {
        LOG_ERROR("Failed to insert updated document");
        return false;
    }
    return true;
}

/**
 * @brief Delete a user document from the database
 *
 * @param collection_name The name of the collection
 * @param id The id of the document to delete
 * @return true on success, false on failure
 */
bool db_user_delete(const char* collection_name, const char* id) {
    int op_num = 0;
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
    cJSON* field_obj = cJSON_GetObjectItem(doc, "username");

    char* field_id = malloc(strlen(collection_name) + 20);
    if (field_id == NULL) {
        LOG_ERROR("Memory allocation failed for key");
        return false;
    }
    sprintf(field_id, "%s:%s", collection_name, "username");
    if (!remove_document_from_field(field_id, field_obj, doc_id, &op_num)) {
        free(field_id);
        return false;
    }

    if (!remove_document_from_collection(collection_name, doc_id, &op_num)) {
        free(field_id);
        return false;
    }

    free(field_id);

    return processRedisReplies(op_num);
}

/**
 * @brief Delete a pet document from the database
 *
 * @param collection_name The name of the collection
 * @param id The id of the document to delete
 * @return true on success, false on failure
 */
bool db_pet_delete(const char* collection_name, const char* id) {
    int op_num = 0;
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

    if (!remove_document_from_field(collection_name, status_obj, doc_id, &op_num)) {
        return false;
    }

    if (!remove_document_from_tags(collection_name, doc, doc_id, &op_num)) {
        return false;
    }

    if (!remove_document_from_collection(collection_name, doc_id, &op_num)) {
        return false;
    }

    return processRedisReplies(op_num);
}

/**
 * @brief Helper function to store tags in the database
 *
 * @param collection_name The name of the collection
 * @param tags_obj The JSON object containing tags
 * @param id The id of the document
 * @param num_op The number of operations
 * @return true on success, false on failure
 */
bool store_tags(const char* collection_name, const cJSON* tags_obj, int id, int* num_op) {
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
                LOG_INFO("SADD %s:%s:%s %d", collection_name, "tags", name, id);
                redisAppendCommand(redis_context, "SADD %s:%s:%s %d", collection_name, "tags", name, id);
                (*num_op)++;
            }
        }
    }
    return true;
}

/**
 * @brief Helper function to remove document from tags in the database
 *
 * @param collection_name The name of the collection
 * @param doc The JSON document
 * @param id The id of the document
 * @param op_number The number of operations
 * @return true on success, false on failure
 */
bool remove_document_from_tags(const char* collection_name, const cJSON* doc, int id, int* op_number) {
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
                LOG_INFO("SREM %s %d", key, id);
                redisAppendCommand(redis_context, "SREM %s %d", key, id);
                (*op_number)++;
                free(key);
            }
        }
    }
    return true;
}

/**
 * @brief Find a single document in the database
 *
 * @param collection_name The name of the collection
 * @param id The id of the document to find
 * @return cJSON* The JSON document found, or NULL on failure
 */
cJSON* db_find_one(const char* collection_name, const char* id) {
    cJSON* result = NULL;
    redisReply* reply = NULL;

    LOG_INFO("GET %s:%s", collection_name, id);
    redisAppendCommand(redis_context, "GET %s:%s", collection_name, id);

    if (redisGetReply(redis_context, (void**)&reply) == REDIS_OK) {
        if (reply == NULL || reply->type != REDIS_REPLY_STRING) {
            freeReplyAndLogError(reply, "Failed to retrieve response");
            return NULL;
        }
        char* json = strdup(reply->str);
        result = cJSON_Parse(json);
        freeReplyObject(reply);
        free(json);
        if (result == NULL) {
            LOG_ERROR("Failed to parse JSON");
            return NULL;
        }
    }
    else {
        LOG_ERROR("Failed to retrieve response");
    }
    return result;
}
/**
 * @brief Find documents in the database based on a query
 *
 * @param collection_name The name of the collection
 * @param query The JSON query object
 * @return cJSON* The JSON array of documents found, or NULL on failure
 */
cJSON* db_find(const char* collection_name, const cJSON* query) {
    int op_num = 0;
    int op_getid_num = 0;

    LOG_INFO("db_find query: %s", cJSON_PrintUnformatted(query));

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
    if (!cJSON_IsArray(value_obj)) {
        LOG_ERROR("Value is not an array");
        return NULL;
    }

    int array_size = cJSON_GetArraySize(value_obj);
    for (int i = 0; i < array_size; i++) {
        cJSON* value = cJSON_GetArrayItem(value_obj, i);
        if (value == NULL) {
            LOG_ERROR("Value is not a string");
            return NULL;
        }
        LOG_INFO("SMEMBERS %s:%s", field_obj->valuestring, value->valuestring);
        redisAppendCommand(redis_context, "SMEMBERS %s:%s", field_obj->valuestring, value->valuestring);
        op_num++;
    }

    redisReply* reply = NULL;
    for (int i = 0; i < op_num; i++) {
        int resultCode = redisGetReply(redis_context, (void**)&reply);
        if (resultCode == REDIS_OK) {
            for (size_t j = 0; j < reply->elements; j++) {
                char* set_id = reply->element[j]->str;
                LOG_INFO("GET %s:%s", collection_name, set_id);
                redisAppendCommand(redis_context, "GET %s:%s", collection_name, set_id);
                op_getid_num++;
            }
            freeReplyObject(reply);
        }
        else {
            freeReplyAndLogError(reply, "Error processing redis reply");
            return NULL;
        }
    }

    reply = NULL;
    cJSON* result = cJSON_CreateArray();

    for (int i = 0; i < op_getid_num; i++) {
        int resultCode = redisGetReply(redis_context, (void**)&reply);
        if (resultCode == REDIS_OK) {
            LOG_INFO("Response [%d]: %s", i, reply->str ? reply->str : "(nil)");
            if (reply->type == REDIS_REPLY_STRING) {
                char* json = strdup(reply->str);
                cJSON* doc = cJSON_Parse(json);
                free(json);
                if (doc != NULL) {
                    cJSON_AddItemToArray(result, doc);
                }
            }
            freeReplyObject(reply);
        }
        else {
            freeReplyAndLogError(reply, "Error processing redis reply");
            return NULL;
        }
    }
    return result;
}

/**
 * @brief Find all documents in a collection
 *
 * @param collection_name The name of the collection
 * @return cJSON* The JSON array of documents found, or NULL on failure
 */
cJSON* db_find_all(const char* collection_name) {
    LOG_INFO("SMEMBERS %s:%s", collection_name, collection_name);
    redisAppendCommand(redis_context, "SMEMBERS %s:%s", collection_name, collection_name);

    redisReply* reply = NULL;
    int op_getid_num = 0;

    cJSON* result = cJSON_CreateArray();

    for (int i = 0; i < op_getid_num; i++) {
        int resultCode = redisGetReply(redis_context, (void**)&reply);
        if (resultCode == REDIS_OK) {
            LOG_INFO("Response [%d]: %s", i, reply->str ? reply->str : "(nil)");
            if (reply->type == REDIS_REPLY_STRING) {
                char* json = strdup(reply->str);
                cJSON* doc = cJSON_Parse(json);
                free(json);
                if (doc != NULL) {
                    cJSON_AddItemToArray(result, doc);
                }
            }
            freeReplyObject(reply);
        }
        else {
            freeReplyAndLogError(reply, "Error processing redis reply");
            return NULL;
        }
    }
    return result;
}

/**
 * @brief Helper function to remove a document from a field in the database
 *
 * @param field_id The field identifier
 * @param field_name The field name
 * @param id The id of the document
 * @param op_num The number of operations
 * @return true on success, false on failure
 */
bool remove_document_from_field(const char* field_id, const cJSON* field_name, int id, int* op_num) {
    if (field_name != NULL) {
        LOG_INFO("SREM %s:%s %d", field_id, field_name->valuestring, id);
        redisAppendCommand(redis_context, "SREM %s:%s %d", field_id, field_name->valuestring, id);
        (*op_num)++;
    }
    return true;
}

/**
 * @brief Helper function to remove a document from a collection in the database
 *
 * @param collection_name The name of the collection
 * @param id The id of the document
 * @param op_num The number of operations
 * @return true on success, false on failure
 */
bool remove_document_from_collection(const char* collection_name, int id, int* op_num) {
    LOG_INFO("SREM %s %d", collection_name, id);
    redisAppendCommand(redis_context, "SREM %s %d", collection_name, id);
    (*op_num)++;
    return true;
}

/**
 * @brief Helper function to store a document in the database
 *
 * @param collection_name The name of the collection
 * @param doc The JSON document to store
 * @param id The id of the document
 * @return true on success, false on failure
 */
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

    LOG_INFO("SET %s %s", key, json_str);
    redisAppendCommand(redis_context, "SET %s %s", key, json_str);

    free(key);
    free(json_str);
    return true;
}