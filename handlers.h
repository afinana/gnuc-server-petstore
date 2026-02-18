#ifndef HANDLERS_H
#define HANDLERS_H

/**
 * @brief Creates a new pet from the given JSON payload.
 *
 * @param json_payload The JSON payload containing the pet details.
 * @return int Returns 0 on success, non-zero on failure.
 */
int handle_create_pet(const char* json_payload);

/**
 * @brief Updates an existing pet with the given JSON payload.
 *
 * @param json_payload The JSON payload containing the updated pet details.
 * @return int Returns 0 on success, non-zero on failure.
 */
int handle_update_pet(const char* json_payload);

/**
 * @brief Deletes a pet with the given ID.
 *
 * @param id The ID of the pet to delete.
 * @return int Returns 0 on success, non-zero on failure.
 */
int handle_delete_pet(const char* id);

/**
 * @brief Finds pets by the given tags.
 *
 * @param tags The tags to search for.
 * @return char* A JSON string containing the list of pets that match the tags.
 *         The caller is responsible for freeing the returned string.
 */
char* handle_get_pet_by_tags(const char* tags);

/**
 * @brief Finds pets by the given status list.
 *
 * @param statuses The list of state to search for.
 * @return char* A JSON string containing the list of pets that match the different state.
 *         The caller is responsible for freeing the returned string.
 */
char* handle_get_pet_by_state(const char* statuses);

/**
 * @brief Finds a pet by the given ID.
 *
 * @param id The ID of the pet to search for.
 * @return char* A JSON string containing the pet details.
 *         The caller is responsible for freeing the returned string.
 */
char* handle_get_pet_by_id(const char* id);

// User methods
/**
 * @brief Creates a new user from the given JSON payload.
 *
 * @param json_payload The JSON payload containing the user details.
 * @return int Returns 0 on success, non-zero on failure.
 */
int handle_create_user(const char* json_payload);

/**
 * @brief Updates an existing user with the given JSON payload.
 *
 * @param json_payload The JSON payload containing the updated user details.
 * @return int Returns 0 on success, non-zero on failure.
 */
int handle_update_user(const char* json_payload);

/**
 * @brief Deletes a user with the given ID.
 *
 * @param id The ID of the user to delete.
 * @return int Returns 0 on success, non-zero on failure.
 */
int handle_delete_user(const char* id);

/**
 * @brief Handles the POST /user/login route.
 *
 * @param json_payload The JSON payload containing the user login details.
 * @return int Returns 0 on success, non-zero on failure.
 */
int handle_post_user_login(const char* json_payload);

/**
 * @brief Handles the POST /user/logout route.
 *
 * @param username The username of the user to log out.
 * @return char* A JSON string containing the logout message.
 *         The caller is responsible for freeing the returned string.
 */
char* handle_post_user_logout(const char* username);

/**
 * @brief Finds a user by the given ID.
 *
 * @param id The ID of the user to search for.
 * @return char* A JSON string containing the user details.
 *         The caller is responsible for freeing the returned string.
 */
char* handle_get_user_by_id(const char* id);

/**
 * @brief Finds all users.
 *
 * @return char* A JSON string containing the list of users.
 *         The caller is responsible for freeing the returned string.
 */
char* handle_get_all_users();

/**
 * @brief Finds users by the given username.
 *
 * @param username The username to search for.
 * @return char* A JSON string containing the list of users that match the username.
 *         The caller is responsible for freeing the returned string.
 */
char* handle_get_user_by_username(const char* username);

#endif
