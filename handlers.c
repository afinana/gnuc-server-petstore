#include "handlers.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "log-utils.h" // Include the log utils header
#include <cjson/cJSON.h>

/**
 * @brief Creates a new pet from the given JSON payload.
 *
 * @param json_payload The JSON payload containing the pet details.
 * @return int Returns EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int handle_create_pet(const char* json_payload) {
    LOG_INFO("handle_create_pet");
    cJSON* doc = cJSON_Parse(json_payload);
    if (!doc) {
        LOG_ERROR("Failed to parse JSON");
        return EXIT_FAILURE;
    }

    if (!db_insert("pets", doc)) {
        LOG_ERROR("Failed to insert pet");
        cJSON_Delete(doc);
        return EXIT_FAILURE;
    }

    cJSON_Delete(doc);
    return EXIT_SUCCESS;
}

/**
 * @brief Updates an existing pet with the given JSON payload.
 *
 * @param json_payload The JSON payload containing the updated pet details.
 * @return int Returns EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int handle_update_pet(const char* json_payload) {
    LOG_INFO("handle_update_pet");
    cJSON* update = cJSON_Parse(json_payload);
    if (!update) {
        LOG_ERROR("Failed to parse JSON");
        return EXIT_FAILURE;
    }

    // Read the JSON payload and extract the id field
    cJSON* id_item = cJSON_GetObjectItem(update, "id");
    if (!cJSON_IsString(id_item)) {
        LOG_ERROR("Failed to find 'id' field in JSON");
        cJSON_Delete(update);
        return EXIT_FAILURE;
    }

    if (!db_update("pets", id_item->valuestring, update)) {
        LOG_ERROR("Failed to update pet");
        cJSON_Delete(update);
        return EXIT_FAILURE;
    }

    cJSON_Delete(update);
    return EXIT_SUCCESS;
}

/**
 * @brief Updates an existing user with the given JSON payload.
 *
 * @param json_payload The JSON payload containing the updated user details.
 * @return int Returns EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int handle_update_user(const char* json_payload) {
    LOG_INFO("handle_update_user");
    cJSON* update = cJSON_Parse(json_payload);
    if (!update) {
        LOG_ERROR("Failed to parse JSON");
        return EXIT_FAILURE;
    }

    // Read the JSON payload and extract the id field
    cJSON* id_item = cJSON_GetObjectItem(update, "id");
    if (!cJSON_IsString(id_item)) {
        LOG_ERROR("Failed to find 'id' field in JSON");
        cJSON_Delete(update);
        return EXIT_FAILURE;
    }

    if (!db_update("users", id_item->valuestring, update)) {
        LOG_ERROR("Failed to update user");
        cJSON_Delete(update);
        return EXIT_FAILURE;
    }

    cJSON_Delete(update);
    return EXIT_SUCCESS;
}

/**
 * @brief Deletes a pet with the given ID.
 *
 * @param id The ID of the pet to delete.
 * @return int Returns EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int handle_delete_pet(const char* id) {
    LOG_INFO("delete pet with the id: %s", id);

    // Create query JSON
    cJSON* query = cJSON_CreateObject();
    cJSON_AddItemToObject(query, "id", cJSON_CreateString(id));

    if (!db_delete("pets", query)) {
        LOG_ERROR("Failed to delete pet");
        cJSON_Delete(query);
        return EXIT_FAILURE;
    }

    cJSON_Delete(query);
    return EXIT_SUCCESS;
}

/**
 * @brief Finds pets by the given tags.
 *
 * @param tags The tags to search for.
 * @return char* A JSON string containing the list of pets that match the tags: "tag01,tag02".
 *         The caller is responsible for freeing the returned string.
 */
char* handle_get_pet_by_tags(const char* tags) {
    LOG_INFO("find pets with the given tags: %s", tags);

    // Split the tags string by comma
    char* tags_copy = strdup(tags); // Duplicate the string for manipulation
    if (tags_copy == NULL) {
        LOG_ERROR("Memory allocation failed");
        return NULL;
    }

    // Create query JSON
    cJSON* query = cJSON_CreateObject();
    cJSON* tags_array = cJSON_CreateArray();

    // Split the tags string by comma
    char* token = strtok(tags_copy, ",");
    while (token != NULL) {
        cJSON_AddItemToArray(tags_array, cJSON_CreateString(token));
        token = strtok(NULL, ",");
    }

    cJSON_AddItemToObject(query, "tags", tags_array);

    // Execute query
    cJSON* result = db_find("pets", query);
    char* json = NULL;
    if (result) {
        json = cJSON_PrintUnformatted(result);
        cJSON_Delete(result);
    }
    else {
        LOG_ERROR("No pets found with the given tags");
        json = strdup("[]");
    }

    cJSON_Delete(query);
    free(tags_copy);
    return json;
}

/**
 * @brief Finds pets by the given statuses.
 *
 * @param statuses The statuses to search for.
 * @return char* A JSON string containing the list of pets that match the list of status: "available,sold".
 *         The caller is responsible for freeing the returned string.
 */
char* handle_get_pet_by_state(const char* statuses) {
    LOG_INFO("find_pets_by_state with the given statuses: %s", statuses);

    // Duplicate the string for manipulation
    char* statuses_copy = strdup(statuses);
    if (statuses_copy == NULL) {
        LOG_ERROR("Memory allocation failed");
        return NULL;
    }

    // Create query JSON
    cJSON* query = cJSON_CreateObject();
    cJSON* status_array = cJSON_CreateArray();

    // Split the statuses string by comma
    char* token = strtok(statuses_copy, ",");
    while (token != NULL) {
        cJSON_AddItemToArray(status_array, cJSON_CreateString(token));
        token = strtok(NULL, ",");
    }

    cJSON_AddItemToObject(query, "status", status_array);

    // Execute query
    cJSON* result = db_find("pets", query);
    char* json = NULL;
    if (result) {
        json = cJSON_PrintUnformatted(result);
        cJSON_Delete(result);
    }
    else {
        LOG_ERROR("No pets found in the given state");
        json = strdup("[]");
    }

    cJSON_Delete(query);
    free(statuses_copy);
    return json;
}

/**
 * @brief Finds a pet by the given ID.
 *
 * @param id The ID of the pet to search for.
 * @return char* A JSON string containing the pet details.
 *         The caller is responsible for freeing the returned string.
 */
char* handle_get_pet_by_id(const char* id) {
    LOG_INFO("find_pet_by_id with the given id: %s", id);

    cJSON* result = db_find_one("pets", id);
    char* json = NULL;
    if (result) {
        json = cJSON_PrintUnformatted(result);
        LOG_INFO("Pet found: %s", json);
        cJSON_Delete(result);
    }
    else {
        LOG_ERROR("No pet found with the given ID");
        json = strdup("{\"error\":\"Failed to find pets by id\"}");
    }
    return json;
}

// User methods
/**
 * @brief Creates a new user from the given JSON payload.
 *
 * @param json_payload The JSON payload containing the user details.
 * @return int Returns EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int handle_create_user(const char* json_payload) {
    LOG_INFO("handle_create_user");
    cJSON* doc = cJSON_Parse(json_payload);
    if (!doc) {
        LOG_ERROR("Failed to parse JSON");
        return EXIT_FAILURE;
    }

    if (!db_insert("users", doc)) {
        LOG_ERROR("Failed to insert user");
        cJSON_Delete(doc);
        return EXIT_FAILURE;
    }

    cJSON_Delete(doc);
    return EXIT_SUCCESS;
}

/**
 * @brief Deletes a user with the given ID.
 *
 * @param id The ID of the user to delete.
 * @return int Returns EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int handle_delete_user(const char* id) {
    LOG_INFO("delete user with the id: %s", id);

    if (!db_delete("users", id)) {
        LOG_ERROR("Failed to delete user");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/**
 * @brief Finds users by the given username.
 *
 * @param username The username to search for.
 * @return char* A JSON string containing the list of users that match the username.
 *         The caller is responsible for freeing the returned string.
 */
char* handle_get_user_by_username(const char* username) {
    LOG_INFO("find_users_by_username with the given username: %s", username);

    // Create query JSON
    cJSON* query = cJSON_CreateObject();
    cJSON_AddItemToObject(query, "username", cJSON_CreateString(username));

    cJSON* result = db_find("users", query);
    char* json = NULL;
    if (result) {
        json = cJSON_PrintUnformatted(result);
        cJSON_Delete(result);
    }
    else {
        LOG_ERROR("No users found with the given username");
        json = strdup("[]");
    }

    cJSON_Delete(query);
    return json;
}

/**
 * @brief Handles the GET /user/logout route.
 *
 * @param username The username of the user to logout.
 * @return char* A JSON string containing the result of the logout operation.
 *         The caller is responsible for freeing the returned string.
 */
char* handle_post_user_logout(const char* username) {
    LOG_INFO("handle_get_user_logout with the given username: %s", username);

    // Handle user logout
    char* result = NULL;
    if (username != NULL) {
        result = strdup("{\"message\":\"User logged out successfully\"}");
    }
    else {
        result = strdup("{\"error\":\"Failed to logout user\"}");
    }
    return result;
}

/**
 * @brief Handles the POST /user/login route.
 *
 * @param json_payload The JSON payload containing the user login details.
 * @return int Returns EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int handle_post_user_login(const char* json_payload) {
    LOG_INFO("handle_post_user_login");
    cJSON* doc = cJSON_Parse(json_payload);
    if (!doc) {
        LOG_ERROR("Failed to parse JSON");
        return EXIT_FAILURE;
    }

    // Check if the username and password fields are present
    cJSON* username_item = cJSON_GetObjectItem(doc, "username");
    cJSON* password_item = cJSON_GetObjectItem(doc, "password");
    if (!cJSON_IsString(username_item) || !cJSON_IsString(password_item)) {
        LOG_ERROR("Missing 'username' or 'password' field in JSON");
        cJSON_Delete(doc);
        return EXIT_FAILURE;
    }

    // Check if the username and password match
    if (strcmp(username_item->valuestring, "admin") == 0 && strcmp(password_item->valuestring, "admin") == 0) {
        cJSON_Delete(doc);
        return EXIT_SUCCESS;
    }

    LOG_ERROR("Invalid username or password");
    cJSON_Delete(doc);
    return EXIT_FAILURE;
}

/**
 * @brief Finds all users.
 *
 * @return char* A JSON string containing the list of users.
 *         The caller is responsible for freeing the returned string.
 */
char* handle_get_all_users() {
    LOG_INFO("find_all_users");

    // Create query JSON
    cJSON* query = cJSON_CreateObject();

    cJSON* result = db_find("users", query);
    char* json = NULL;
    if (result) {
        json = cJSON_PrintUnformatted(result);
        cJSON_Delete(result);
    }
    else {
        LOG_ERROR("No users found");
        json = strdup("[]");
    }

    cJSON_Delete(query);
    return json;
}

/**
 * @brief Finds a user by the given ID.
 *
 * @param id The ID of the user to search for.
 * @return char* A JSON string containing the user details.
 *         The caller is responsible for freeing the returned string.
 */
char* handle_get_user_by_id(const char* id) {
    LOG_INFO("find_user_by_id with the given id : %s", id);

    // Create query JSON
    cJSON* query = cJSON_CreateObject();
    cJSON_AddItemToObject(query, "id", cJSON_CreateString(id));

    cJSON* result = db_find_one("users", query);
    char* json = NULL;
    if (result) {
        json = cJSON_PrintUnformatted(result);
        LOG_INFO("User found: %s", json);
        cJSON_Delete(result);
    }
    else {
        LOG_ERROR("No user found with the given ID");
        json = strdup("{\"error\":\"Failed to find user by id\"}");
    }

    cJSON_Delete(query);
    return json;
}
