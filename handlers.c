#include "handlers.h"
#include "database.h"
#include <stdlib.h>
#include <bson/bson.h>
#include <stdio.h>
#include <string.h>
#include "log-utils.h" // Include the log utils header

/**
 * @brief Creates a new pet from the given JSON payload.
 *
 * @param json_payload The JSON payload containing the pet details.
 * @return int Returns EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int create_pet(const char* json_payload) {
    LOG_INFO("create_pet");
    bson_error_t error;
    bson_t* doc = bson_new_from_json((const uint8_t*)json_payload, -1, &error);
    if (!doc) {
        LOG_ERROR("Failed to parse JSON: %s", error.message);
        return EXIT_FAILURE;
    }

    if (!db_insert("pets", doc)) {
        LOG_ERROR("Failed to insert pet");
        bson_destroy(doc);
        return EXIT_FAILURE;
    }

    bson_destroy(doc);
    return EXIT_SUCCESS;
}

/**
 * @brief Updates an existing pet with the given JSON payload.
 *
 * @param json_payload The JSON payload containing the updated pet details.
 * @return int Returns EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int update_pet(const char* json_payload) {
    LOG_INFO("update_pet");
    bson_error_t error;
    bson_t* update = bson_new_from_json((const uint8_t*)json_payload, -1, &error);
    if (!update) {
        LOG_ERROR("Failed to parse JSON: %s", error.message);
        return EXIT_FAILURE;
    }

    // Read the JSON payload and extract the id field
    bson_iter_t iter;
    const char* id_str = NULL;
    if (bson_iter_init_find(&iter, update, "id") && BSON_ITER_HOLDS_UTF8(&iter)) {
        id_str = bson_iter_utf8(&iter, NULL);
    }
    else {
        LOG_ERROR("Failed to find 'id' field in JSON");
        bson_destroy(update);
        return EXIT_FAILURE;
    }

    // Convert the id string to an OID
    bson_oid_t oid;
    if (!bson_oid_is_valid(id_str, strlen(id_str))) {
        LOG_ERROR("Invalid ID");
        bson_destroy(update);
        return EXIT_FAILURE;
    }
    bson_oid_init_from_string(&oid, id_str);
    bson_t* query = BCON_NEW("_id", BCON_OID(&oid));

    if (!db_update("pets", query, update)) {
        LOG_ERROR("Failed to update pet");
        bson_destroy(query);
        bson_destroy(update);
        return EXIT_FAILURE;
    }

    bson_destroy(query);
    bson_destroy(update);
    return EXIT_SUCCESS;
}

/**
 * @brief Deletes a pet with the given ID.
 *
 * @param id The ID of the pet to delete.
 * @return int Returns EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int delete_pet_by_id(const char* id) {
    LOG_INFO("delete pet with the id: %s", id);
    // delete pet by id
    bson_oid_t oid;
    if (!bson_oid_is_valid(id, strlen(id))) {
        LOG_ERROR("Invalid ID");
        return EXIT_FAILURE;
    }
    bson_oid_init_from_string(&oid, id);
    bson_t* query = BCON_NEW("_id", BCON_OID(&oid));
    if (!db_delete("pets", query)) {
        LOG_ERROR("Failed to delete pet");
        bson_destroy(query);
        return EXIT_FAILURE;
    }
    bson_destroy(query);
    return EXIT_SUCCESS;
}

/**
 * @brief Finds pets by the given tags.
 *
 * @param tags The tags to search for.
 * @return char* A JSON string containing the list of pets that match the tags: "tag01,tag02".
 *         The caller is responsible for freeing the returned string.
 */
char* find_pets_by_tags(const char* tags) {
    LOG_INFO("find pets with the given tags: %s", tags);
    // Split the tags string by comma
    char* tags_copy = strdup(tags); // Duplicate the string for manipulation
    if (tags_copy == NULL) {
        LOG_ERROR("Memory allocation failed");
        return NULL;
    }
    bson_t* query = bson_new();

    // get all tags splits by in threadsafe mode
    char* saveToken;
    char* token = strtok_r(tags_copy, ",", &saveToken);

    // Build the query: { tags: { $elemMatch: { name: "tag01" } }, tags: { $elemMatch: { name: "tag02" } } }    
    // Add each tag to the query array
    while (token != NULL) {
        bson_t* tag_elem = bson_new();
        bson_t* tag_doc = bson_new();

        BSON_APPEND_UTF8(tag_doc, "name", token);
        BSON_APPEND_DOCUMENT(tag_elem, "$elemMatch", tag_doc);
        // Construct the final query
        BSON_APPEND_DOCUMENT(query, "tags", tag_elem);

        bson_destroy(tag_doc);
        bson_destroy(tag_elem);
        token = strtok_r(NULL, ",", &saveToken);
    }

    LOG_INFO("find_pets_by_tags Query : %s", bson_as_json(query, NULL));

    // Execute Mongo query
    bson_t* result = db_find("pets", query);
    char* json = NULL;
    if (result) {
        json = bson_as_relaxed_extended_json(result, NULL);
        //LOG_INFO("Pets found: %s", json);
        bson_destroy(result);
    }
    else {
        LOG_ERROR("No pets found with the given tags");
        json = strdup("[]");
    }

    bson_destroy(query);
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
char* find_pets_by_state(const char* statuses) {
    LOG_INFO("find_pets_by_state with the given statuses: %s", statuses);

    // Duplicate the string for manipulation
    char* statuses_copy = strdup(statuses);
    if (statuses_copy == NULL) {
        LOG_ERROR("Memory allocation failed");
        return NULL;
    }

    // Initialize BSON query
    bson_t* query = bson_new();
    bson_t* status_array = bson_new();
    bson_t child;

    // Split the statuses string by comma using strtok_r
    // build query : db.pets.find({status: { $in: ["available", "sold"]}})
    char* saveptr;
    char* status = strtok_r(statuses_copy, ",", &saveptr);

    BSON_APPEND_ARRAY_BEGIN(status_array, "$in", &child);
    int index = 0;
    char key[16];

    while (status != NULL) {
        snprintf(key, sizeof(key), "%d", index++);
        BSON_APPEND_UTF8(&child, key, status);
        status = strtok_r(NULL, ",", &saveptr);
    }
    bson_append_array_end(status_array, &child);

    BSON_APPEND_DOCUMENT(query, "status", status_array);

    LOG_INFO("find_pets_by_state Query: %s", bson_as_json(query, NULL));
    // Execute query
    bson_t* result = db_find("pets", query);
    char* json = NULL;
    if (result) {
        json = bson_as_relaxed_extended_json(result, NULL);
        // LOG_INFO("Pets found: %s", json);
        bson_destroy(result);
    }
    else {
        LOG_ERROR("No pets found in the given state");
        json = strdup("[]");
    }

    bson_destroy(status_array);
    bson_destroy(query);
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
char* find_pet_by_id(const char* id) {
    LOG_INFO("find_pet_by_id with the given id: %s", id);

    // find pet by id
    bson_oid_t oid;
    if (!bson_oid_is_valid(id, strlen(id))) {
        LOG_ERROR("Invalid ID");
        return NULL;
    }
    bson_oid_init_from_string(&oid, id);
    bson_t* query = BCON_NEW("_id", BCON_OID(&oid));

    bson_t* result = db_find("pets", query);
    char* json = NULL;
    if (result) {
        json = bson_as_relaxed_extended_json(result, NULL);
        LOG_INFO("Pet found: %s", json);
        bson_destroy(result);
    }
    else {
        LOG_ERROR("No pet found with the given ID");
        json = strdup("{\"error\":\"Failed to find pets by id\"}");
    }

    bson_destroy(query);
    return json;
}

// User methods
/**
 * @brief Creates a new user from the given JSON payload.
 *
 * @param json_payload The JSON payload containing the user details.
 * @return int Returns EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int create_user(const char* json_payload) {
    LOG_INFO("create_user");
    bson_error_t error;
    bson_t* doc = bson_new_from_json((const uint8_t*)json_payload, -1, &error);
    if (!doc) {
        LOG_ERROR("Failed to parse JSON: %s", error.message);
        return EXIT_FAILURE;
    }
    if (!db_insert("users", doc)) {
        LOG_ERROR("Failed to insert user");
        bson_destroy(doc);
        return EXIT_FAILURE;
    }
    bson_destroy(doc);
    return EXIT_SUCCESS;
}

/**
 * @brief Updates an existing user with the given JSON payload.
 *
 * @param json_payload The JSON payload containing the updated user details.
 * @return int Returns EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int update_user(const char* json_payload) {
    LOG_INFO("update_user");
    bson_error_t error;
    bson_t* update = bson_new_from_json((const uint8_t*)json_payload, -1, &error);
    if (!update) {
        LOG_ERROR("Failed to parse JSON: %s", error.message);
        return EXIT_FAILURE;
    }
    // Read the JSON payload and extract the id field
    bson_iter_t iter;
    bson_oid_t oid;
    if (bson_iter_init_find(&iter, update, "id") && BSON_ITER_HOLDS_OID(&iter)) {
        bson_oid_copy(bson_iter_oid(&iter), &oid);
    }
    else {
        LOG_ERROR("Failed to find 'id' field in JSON");
        bson_destroy(update);
        return EXIT_FAILURE;
    }
    // Convert the oid to a string
    char id[25];
    bson_oid_to_string(&oid, id);
    bson_t* query = BCON_NEW("_id", BCON_OID(&oid));
    if (!db_update("users", query, update)) {
        LOG_ERROR("Failed to update user");
        bson_destroy(query);
        bson_destroy(update);
        return EXIT_FAILURE;
    }
    bson_destroy(query);
    bson_destroy(update);
    return EXIT_SUCCESS;
}

/**
 * @brief Deletes a user with the given ID.
 *
 * @param id The ID of the user to delete.
 * @return int Returns EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
int delete_user_by_id(const char* id) {
    LOG_INFO("delete user with the id: %s", id);
    // delete user by id
    bson_oid_t oid;
    if (!bson_oid_is_valid(id, strlen(id))) {
        LOG_ERROR("Invalid ID");
        return EXIT_FAILURE;
    }
    bson_oid_init_from_string(&oid, id);
    bson_t* query = BCON_NEW("_id", BCON_OID(&oid));
    if (!db_delete("users", query)) {
        LOG_ERROR("Failed to delete user");
        bson_destroy(query);
        return EXIT_FAILURE;
    }
    bson_destroy(query);
    return EXIT_SUCCESS;
}

/**
 * @brief Finds users by the given username.
 *
 * @param username The username to search for.
 * @return char* A JSON string containing the list of users that match the username.
 *         The caller is responsible for freeing the returned string.
 */
char* find_user_by_username(const char* username) {
    LOG_INFO("find_users_by_username with the given username: %s", username);
    // find users by username
    bson_t* query = BCON_NEW("username", BCON_UTF8(username));
    bson_t* result = db_find("users", query);
    char* json = NULL;
    if (result) {
        json = bson_as_relaxed_extended_json(result, NULL);
        // LOG_INFO("Users found: %s", json);
        bson_destroy(result);
    }
    else {
        LOG_ERROR("No users found with the given username");
        json = strdup("[]");
    }
    bson_destroy(query);
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
    // handle user logout
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
    bson_error_t error;
    bson_t doc;
    if (!bson_init_from_json(&doc, json_payload, -1, &error)) {
        LOG_ERROR("Failed to parse JSON: %s", error.message);
        return EXIT_FAILURE;
    }
    // Check if the username and password fields are present
    if (!bson_has_field(&doc, "username") || !bson_has_field(&doc, "password")) {
        LOG_ERROR("Missing 'username' or 'password' field in JSON");
        bson_destroy(&doc);
        return EXIT_FAILURE;
    }

    // Check if the username and password fields are strings
    bson_iter_t iter;
    if (bson_iter_init_find(&iter, &doc, "username") && BSON_ITER_HOLDS_UTF8(&iter) &&
        bson_iter_init_find(&iter, &doc, "password") && BSON_ITER_HOLDS_UTF8(&iter)) {
        // Check if the username and password match
        const char* username = bson_iter_utf8(&iter, NULL);
        bson_iter_init_find(&iter, &doc, "password");
        const char* password = bson_iter_utf8(&iter, NULL);
        if (strcmp(username, "admin") == 0 && strcmp(password, "admin") == 0) {
            bson_destroy(&doc);
            return EXIT_SUCCESS;
        }
    }
    LOG_ERROR("Invalid username or password");
    bson_destroy(&doc);
    return EXIT_FAILURE;
}

/**
 * @brief Finds all users.
 *
 * @return char* A JSON string containing the list of users.
 *         The caller is responsible for freeing the returned string.
 */
char* find_all_users() {
    LOG_INFO("find_all_users");
    // find all users
    bson_t* query = bson_new();
    bson_t* result = db_find("users", query);
    char* json = NULL;
    if (result) {
        json = bson_as_relaxed_extended_json(result, NULL);
        // LOG_INFO("Users found: %s", json);
        bson_destroy(result);
    }
    else {
        LOG_ERROR("No users found");
        json = strdup("[]");
    }
    bson_destroy(query);
    return json;
}

/**
 * @brief Finds a user by the given ID.
 *
 * @param id The ID of the user to search for.
 * @return char* A JSON string containing the user details.
 *         The caller is responsible for freeing the returned string.
 */
char* find_user_by_id(const char* id) {
    LOG_INFO("find_user_by_id with the given id : %s", id);
    // find user by id
    bson_oid_t oid;
    if (!bson_oid_is_valid(id, strlen(id))) {
        LOG_ERROR("Invalid ID");
        return NULL;
    }
    bson_oid_init_from_string(&oid, id);
    bson_t* query = BCON_NEW("_id", BCON_OID(&oid));
    bson_t* result = db_find("users", query);
    char* json = NULL;
    if (result) {
        json = bson_as_relaxed_extended_json(result, NULL);
        // LOG_INFO("User found: %s", json);
        bson_destroy(result);
    }
    else {
        LOG_ERROR("No user found with the given ID");
        json = strdup("{\"error\":\"Failed to find user by id\"}");
    }
    bson_destroy(query);
    return json;
}
