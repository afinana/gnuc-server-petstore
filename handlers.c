#include "database.h"
#include "handlers.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cjson/cJSON.h>
#include "log-utils.h"

/**
 * @brief Helper: converts a JSON string to a BSON document.
 *
 * @param json_str The JSON string to convert.
 * @return bson_t* The resulting BSON document, or NULL on error.
 *         The caller must free with bson_destroy().
 */
static bson_t* json_to_bson(const char* json_str) {
    bson_error_t error;
    bson_t* doc = bson_new_from_json((const uint8_t*)json_str, -1, &error);
    if (!doc) {
        LOG_ERROR("Failed to parse JSON to BSON: %s", error.message);
    }
    return doc;
}

/**
 * @brief Helper: converts a BSON document to a JSON string.
 *
 * @param doc The BSON document.
 * @return char* The JSON string. Caller must free with bson_free().
 */
static char* bson_to_json(const bson_t* doc) {
    return bson_as_relaxed_extended_json(doc, NULL);
}

// ========================
// Pet handlers
// ========================

/**
 * @brief Creates a new pet from the given JSON payload.
 */
int handle_create_pet(const char* json_payload) {
    LOG_INFO("handle_create_pet");
    bson_t* doc = json_to_bson(json_payload);
    if (!doc) return EXIT_FAILURE;

    bool success = db_insert("pets", doc);
    if (!success) {
        LOG_ERROR("Failed to insert pet");
    }

    bson_destroy(doc);
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

/**
 * @brief Updates an existing pet with the given JSON payload.
 */
int handle_update_pet(const char* json_payload) {
    LOG_INFO("handle_update_pet");
    bson_t* doc = json_to_bson(json_payload);
    if (!doc) return EXIT_FAILURE;

    // Extract the "id" field to build the query filter
    bson_iter_t iter;
    if (!bson_iter_init_find(&iter, doc, "id")) {
        LOG_ERROR("Failed to find 'id' field in JSON");
        bson_destroy(doc);
        return EXIT_FAILURE;
    }

    bson_t* query = bson_new();
    BSON_APPEND_VALUE(query, "id", bson_iter_value(&iter));

    bson_t* update = bson_new();
    BSON_APPEND_DOCUMENT(update, "$set", doc);

    bool success = db_update("pets", query, update);
    if (!success) {
        LOG_ERROR("Failed to update pet");
    }

    bson_destroy(update);
    bson_destroy(query);
    bson_destroy(doc);
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

/**
 * @brief Deletes a pet with the given ID.
 */
int handle_delete_pet(const char* id) {
    LOG_INFO("delete pet with the id: %s", id);

    // Try to parse as integer first, fall back to string
    char* endptr;
    long id_val = strtol(id, &endptr, 10);

    bson_t* query = bson_new();
    if (*endptr == '\0') {
        // ID is numeric
        BSON_APPEND_INT64(query, "id", id_val);
    } else {
        // ID is a string
        BSON_APPEND_UTF8(query, "id", id);
    }

    bool success = db_delete("pets", query);
    if (!success) {
        LOG_ERROR("Failed to delete pet");
    }

    bson_destroy(query);
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

/**
 * @brief Finds pets by the given tags (comma-separated).
 */
char* handle_get_pet_by_tags(const char* tags) {
    if (tags == NULL || tags[0] == '\0') {
        LOG_ERROR("No tags provided");
        return strdup("[]");
    }
    LOG_INFO("find pets with the given tags: %s", tags);

    // Build query: { "tags.name": { "$in": ["tag1", "tag2"] } }
    bson_t* query = bson_new();
    bson_t in_doc, in_array;
    BSON_APPEND_DOCUMENT_BEGIN(query, "tags.name", &in_doc);
    BSON_APPEND_ARRAY_BEGIN(&in_doc, "$in", &in_array);

    char* tags_copy = strdup(tags);
    if (!tags_copy) {
        LOG_ERROR("Memory allocation failed");
        bson_destroy(query);
        return NULL;
    }

    int index = 0;
    char key[16];
    char* saveptr;
    char* token = strtok_r(tags_copy, ",", &saveptr);
    while (token != NULL) {
        snprintf(key, sizeof(key), "%d", index++);
        BSON_APPEND_UTF8(&in_array, key, token);
        token = strtok_r(NULL, ",", &saveptr);
    }
    free(tags_copy);

    bson_append_array_end(&in_doc, &in_array);
    bson_append_document_end(query, &in_doc);

    char* query_json = bson_to_json(query);
    LOG_INFO("find_pets_by_tags Query: %s", query_json);
    bson_free(query_json);

    // Execute query
    bson_t* result = db_find("pets", query);
    char* json = NULL;
    if (result) {
        json = bson_to_json(result);
        bson_destroy(result);
    } else {
        LOG_ERROR("No pets found with the given tags");
        json = strdup("[]");
    }

    bson_destroy(query);
    return json;
}

/**
 * @brief Finds pets by the given statuses (comma-separated).
 */
char* handle_get_pet_by_state(const char* statuses) {
    if (statuses == NULL || statuses[0] == '\0') {
        LOG_ERROR("No statuses provided");
        return strdup("[]");
    }
    LOG_INFO("find_pets_by_state with the given statuses: %s", statuses);

    // Build query: { "status": { "$in": ["available", "sold"] } }
    bson_t* query = bson_new();
    bson_t in_doc, in_array;
    BSON_APPEND_DOCUMENT_BEGIN(query, "status", &in_doc);
    BSON_APPEND_ARRAY_BEGIN(&in_doc, "$in", &in_array);

    char* statuses_copy = strdup(statuses);
    if (!statuses_copy) {
        LOG_ERROR("Memory allocation failed");
        bson_destroy(query);
        return NULL;
    }

    int index = 0;
    char key[16];
    char* saveptr;
    char* status = strtok_r(statuses_copy, ",", &saveptr);
    while (status != NULL) {
        snprintf(key, sizeof(key), "%d", index++);
        BSON_APPEND_UTF8(&in_array, key, status);
        status = strtok_r(NULL, ",", &saveptr);
    }
    free(statuses_copy);

    bson_append_array_end(&in_doc, &in_array);
    bson_append_document_end(query, &in_doc);

    char* query_json = bson_to_json(query);
    LOG_INFO("find_pets_by_state Query: %s", query_json);
    bson_free(query_json);

    // Execute query
    bson_t* result = db_find("pets", query);
    char* json = NULL;
    if (result) {
        json = bson_to_json(result);
        bson_destroy(result);
    } else {
        LOG_ERROR("No pets found in the given state");
        json = strdup("[]");
    }

    bson_destroy(query);
    return json;
}

/**
 * @brief Finds a pet by the given ID.
 */
char* handle_get_pet_by_id(const char* id) {
    LOG_INFO("find_pet_by_id with the given id: %s", id);

    // Try to parse ID as integer
    char* endptr;
    long id_val = strtol(id, &endptr, 10);

    bson_t* query = bson_new();
    if (*endptr == '\0') {
        BSON_APPEND_INT64(query, "id", id_val);
    } else {
        BSON_APPEND_UTF8(query, "id", id);
    }

    bson_t* result = db_find_one("pets", query);
    char* json = NULL;
    if (result) {
        json = bson_to_json(result);
        bson_destroy(result);
    } else {
        LOG_ERROR("No pet found with the given ID");
        json = strdup("{\"error\":\"Pet not found\"}");
    }

    bson_destroy(query);
    return json;
}

// ========================
// User handlers
// ========================

/**
 * @brief Creates a new user from the given JSON payload.
 */
int handle_create_user(const char* json_payload) {
    LOG_INFO("handle_create_user");
    bson_t* doc = json_to_bson(json_payload);
    if (!doc) return EXIT_FAILURE;

    bool success = db_insert("users", doc);
    if (!success) {
        LOG_ERROR("Failed to insert user");
    }

    bson_destroy(doc);
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

/**
 * @brief Updates an existing user with the given JSON payload.
 */
int handle_update_user(const char* json_payload) {
    LOG_INFO("handle_update_user");
    bson_t* doc = json_to_bson(json_payload);
    if (!doc) return EXIT_FAILURE;

    bson_iter_t iter;
    if (!bson_iter_init_find(&iter, doc, "id")) {
        LOG_ERROR("Failed to find 'id' field in JSON");
        bson_destroy(doc);
        return EXIT_FAILURE;
    }

    bson_t* query = bson_new();
    BSON_APPEND_VALUE(query, "id", bson_iter_value(&iter));

    bson_t* update = bson_new();
    BSON_APPEND_DOCUMENT(update, "$set", doc);

    bool success = db_update("users", query, update);
    if (!success) {
        LOG_ERROR("Failed to update user");
    }

    bson_destroy(update);
    bson_destroy(query);
    bson_destroy(doc);
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

/**
 * @brief Deletes a user with the given ID (or username used as ID).
 */
int handle_delete_user(const char* id) {
    LOG_INFO("delete user with the id: %s", id);

    char* endptr;
    long id_val = strtol(id, &endptr, 10);

    bson_t* query = bson_new();
    if (*endptr == '\0') {
        BSON_APPEND_INT64(query, "id", id_val);
    } else {
        // If not a number, treat as username
        BSON_APPEND_UTF8(query, "username", id);
    }

    bool success = db_delete("users", query);
    if (!success) {
        LOG_ERROR("Failed to delete user");
    }

    bson_destroy(query);
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

/**
 * @brief Finds a user by the given username.
 */
char* handle_get_user_by_username(const char* username) {
    LOG_INFO("find_user_by_username with the given username: %s", username);

    bson_t* query = bson_new();
    BSON_APPEND_UTF8(query, "username", username);

    bson_t* result = db_find_one("users", query);
    char* json = NULL;
    if (result) {
        json = bson_to_json(result);
        bson_destroy(result);
    } else {
        LOG_ERROR("No user found with the given username");
        json = strdup("{\"error\":\"User not found\"}");
    }

    bson_destroy(query);
    return json;
}

/**
 * @brief Handles the POST /user/logout route.
 */
char* handle_post_user_logout(const char* username) {
    LOG_INFO("handle_post_user_logout with the given username: %s",
             username ? username : "(null)");

    char* result = username
        ? strdup("{\"message\":\"User logged out successfully\"}")
        : strdup("{\"error\":\"Failed to logout user\"}");
    return result;
}

/**
 * @brief Handles the POST /user/login route.
 * Validates username/password from JSON payload using cJSON.
 */
int handle_post_user_login(const char* json_payload) {
    LOG_INFO("handle_post_user_login");

    cJSON* doc = cJSON_Parse(json_payload);
    if (!doc) {
        LOG_ERROR("Failed to parse login JSON");
        return EXIT_FAILURE;
    }

    cJSON* username_item = cJSON_GetObjectItem(doc, "username");
    cJSON* password_item = cJSON_GetObjectItem(doc, "password");

    if (!cJSON_IsString(username_item) || !cJSON_IsString(password_item)) {
        LOG_ERROR("Missing 'username' or 'password' field in JSON");
        cJSON_Delete(doc);
        return EXIT_FAILURE;
    }

    const char* username = username_item->valuestring;
    const char* password = password_item->valuestring;

    // Simple hardcoded validation (replace with DB lookup in production)
    int result = EXIT_FAILURE;
    if (username && password &&
        strcmp(username, "admin") == 0 && strcmp(password, "admin") == 0) {
        result = EXIT_SUCCESS;
    }

    cJSON_Delete(doc);
    return result;
}

/**
 * @brief Finds all users.
 */
char* handle_get_all_users(void) {
    LOG_INFO("find_all_users");

    bson_t* result = db_find_all("users");
    char* json = NULL;
    if (result) {
        json = bson_to_json(result);
        bson_destroy(result);
    } else {
        LOG_ERROR("No users found");
        json = strdup("[]");
    }

    return json;
}

/**
 * @brief Finds a user by the given ID.
 */
char* handle_get_user_by_id(const char* id) {
    LOG_INFO("find_user_by_id with the given id: %s", id);

    char* endptr;
    long id_val = strtol(id, &endptr, 10);

    bson_t* query = bson_new();
    if (*endptr == '\0') {
        BSON_APPEND_INT64(query, "id", id_val);
    } else {
        BSON_APPEND_UTF8(query, "id", id);
    }

    bson_t* result = db_find_one("users", query);
    char* json = NULL;
    if (result) {
        json = bson_to_json(result);
        bson_destroy(result);
    } else {
        LOG_ERROR("No user found with the given ID");
        json = strdup("{\"error\":\"User not found\"}");
    }

    bson_destroy(query);
    return json;
}
