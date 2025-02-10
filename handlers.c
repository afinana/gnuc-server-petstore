#include "database.h"
#include "handlers.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cjson/cJSON.h>
#include "log-utils.h" // Include the log utils header

// Helper function to parse JSON and log errors
static cJSON* parse_json(const char* json_payload) {
    cJSON* doc = cJSON_Parse(json_payload);
    if (!doc) {
        LOG_ERROR("Failed to parse JSON");
    }
    return doc;
}

// Helper function to create a query JSON object
static cJSON* create_query(const char* field, const char* operator, const char* values) {
    cJSON* query = cJSON_CreateObject();
    cJSON_AddStringToObject(query, "operator", operator);
    cJSON_AddStringToObject(query, "field", field);

    cJSON* value_array = cJSON_CreateArray();
    char* values_copy = strdup(values);
    if (values_copy == NULL) {
        LOG_ERROR("Memory allocation failed");
        cJSON_Delete(query);
        return NULL;
    }

    char* token = strtok(values_copy, ",");
    while (token != NULL) {
        cJSON_AddItemToArray(value_array, cJSON_CreateString(token));
        token = strtok(NULL, ",");
    }
    cJSON_AddItemToObject(query, "value", value_array);
    free(values_copy);

    return query;
}

/**
 * @brief Creates a new pet from the given JSON payload.
 *
 * @param json_payload The JSON payload containing the pet details.
 * @return int Returns EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int handle_create_pet(const char* json_payload) {
    LOG_INFO("handle_create_pet");
    cJSON* doc = parse_json(json_payload);
    if (!doc) return EXIT_FAILURE;

    int result = db_pet_insert("pets", doc) ? EXIT_SUCCESS : EXIT_FAILURE;
    if (result == EXIT_FAILURE) {
        LOG_ERROR("Failed to insert pet");
    }

    cJSON_Delete(doc);
    return result;
}

/**
 * @brief Updates an existing pet with the given JSON payload.
 *
 * @param json_payload The JSON payload containing the updated pet details.
 * @return int Returns EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int handle_update_pet(const char* json_payload) {
    LOG_INFO("handle_update_pet");
    cJSON* update = parse_json(json_payload);
    if (!update) return EXIT_FAILURE;

    // Read the JSON payload and extract the id field
    cJSON* id_item = cJSON_GetObjectItem(update, "id");
    if (!cJSON_IsString(id_item)) {
        LOG_ERROR("Failed to find 'id' field in JSON");
        cJSON_Delete(update);
        return EXIT_FAILURE;
    }

    int result = db_pet_update("pets", update) ? EXIT_SUCCESS : EXIT_FAILURE;
    if (result == EXIT_FAILURE) {
        LOG_ERROR("Failed to update pet");
    }

    cJSON_Delete(update);
    return result;
}

/**
 * @brief Deletes a pet with the given ID.
 *
 * @param id The ID of the pet to delete.
 * @return int Returns EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int handle_delete_pet(const char* id) {
    LOG_INFO("delete pet with the id: %s", id);

    int result = db_pet_delete("pets", id) ? EXIT_SUCCESS : EXIT_FAILURE;
    if (result == EXIT_FAILURE) {
        LOG_ERROR("Failed to delete pet");
    }
    return result;
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

    cJSON* query = create_query("pets:tags", "eq", tags);
    if (!query) return strdup("[]");

    cJSON* result = db_find("pets", query);
    char* json = result ? cJSON_PrintUnformatted(result) : strdup("[]");
    if (!result) {
        LOG_ERROR("No pets found with the given tags");
    }

    cJSON_Delete(query);
    cJSON_Delete(result);
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

    cJSON* query = create_query("pets:status", "eq", statuses);
    if (!query) return strdup("[]");

    LOG_INFO("handle_get_pet_by_state query: %s", cJSON_PrintUnformatted(query));

    cJSON* result = db_find("pets", query);
    char* json = result ? cJSON_PrintUnformatted(result) : strdup("[]");
    if (!result) {
        LOG_ERROR("No pets found in the given state");
    }

    cJSON_Delete(query);
    cJSON_Delete(result);
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
    char* json = result ? cJSON_PrintUnformatted(result) : strdup("{\"error\":\"Failed to find pet by id\"}");
    if (!result) {
        LOG_ERROR("No pet found with the given ID");
    }

    cJSON_Delete(result);
    return json;
}

/**
 * @brief Creates a new user from the given JSON payload.
 *
 * @param json_payload The JSON payload containing the user details.
 * @return int Returns EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int handle_create_user(const char* json_payload) {
    LOG_INFO("handle_create_user");
    cJSON* doc = parse_json(json_payload);
    if (!doc) return EXIT_FAILURE;

    int result = db_user_insert("users", doc) ? EXIT_SUCCESS : EXIT_FAILURE;
    if (result == EXIT_FAILURE) {
        LOG_ERROR("Failed to insert user");
    }

    cJSON_Delete(doc);
    return result;
}

/**
 * @brief Updates an existing user with the given JSON payload.
 *
 * @param json_payload The JSON payload containing the updated user details.
 * @return int Returns EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int handle_update_user(const char* json_payload) {
    LOG_INFO("handle_update_user");
    cJSON* update = parse_json(json_payload);
    if (!update) return EXIT_FAILURE;

    int result = db_user_update("users", update) ? EXIT_SUCCESS : EXIT_FAILURE;
    if (result == EXIT_FAILURE) {
        LOG_ERROR("Failed to update user");
    }

    cJSON_Delete(update);
    return result;
}

/**
 * @brief Deletes a user with the given ID.
 *
 * @param id The ID of the user to delete.
 * @return int Returns EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int handle_delete_user(const char* id) {
    LOG_INFO("delete user with the id: %s", id);

    int result = db_user_delete("users", id) ? EXIT_SUCCESS : EXIT_FAILURE;
    if (result == EXIT_FAILURE) {
        LOG_ERROR("Failed to delete user");
    }
    return result;
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

    // Example query: { "operator": "eq", "field" : "username", "value" : "email_user@example.com" }
    cJSON* query = create_query("users:username", "eq", username);
    if (!query) return strdup("[]");

    char* json = NULL;
    cJSON* result = db_find("users", query);

    // if result is an array and not empty returns first element of the array
    cJSON* user = NULL;
    if (cJSON_IsArray(result)) {
        user = cJSON_GetArrayItem(result, 0);
    }

    json = user ? cJSON_PrintUnformatted(user) : strdup("{\"error\":\"No users found with the given username\"}");
    if (!result) {
        LOG_ERROR("No users found with the given username");
    }

    cJSON_Delete(query);
    cJSON_Delete(result);
    return json;
}

/**
 * @brief Handles the POST /user/logout route.
 *
 * @param username The username of the user to logout.
 * @return char* A JSON string containing the result of the logout operation.
 *         The caller is responsible for freeing the returned string.
 */
char* handle_post_user_logout(const char* username) {
    LOG_INFO("handle_post_user_logout with the given username: %s", username);

    // Handle user logout
    char* result = username ? strdup("{\"message\":\"User logged out successfully\"}") : strdup("{\"error\":\"Failed to logout user\"}");
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
    cJSON* doc = parse_json(json_payload);
    if (!doc) return EXIT_FAILURE;

    // Check if the username and password fields are present
    cJSON* username_item = cJSON_GetObjectItem(doc, "username");
    cJSON* password_item = cJSON_GetObjectItem(doc, "password");
    if (!cJSON_IsString(username_item) || !cJSON_IsString(password_item)) {
        LOG_ERROR("Missing 'username' or 'password' field in JSON");
        cJSON_Delete(doc);
        return EXIT_FAILURE;
    }

    // Check if the username and password match
    int result = (strcmp(username_item->valuestring, "admin") == 0 && strcmp(password_item->valuestring, "admin") == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
    if (result == EXIT_FAILURE) {
        LOG_ERROR("Invalid username or password");
    }

    cJSON_Delete(doc);
    return result;
}

/**
 * @brief Finds all users.
 *
 * @return char* A JSON string containing the list of users.
 *         The caller is responsible for freeing the returned string.
 */
char* handle_get_all_users() {
    LOG_INFO("find_all_users");

    // Use method find_all to get all users
    cJSON* result = db_find_all("users");
    char* json = result ? cJSON_PrintUnformatted(result) : strdup("[]");
    if (!result) {
        LOG_ERROR("No users found");
    }

    cJSON_Delete(result);
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

    cJSON* result = db_find_one("users", id);
    char* json = result ? cJSON_PrintUnformatted(result) : strdup("{\"error\":\"Failed to find user by id\"}");
    if (!result) {
        LOG_ERROR("No user found with the given ID");
    }

    cJSON_Delete(result);
    return json;
}
