#ifndef HANDLERS_H
#define HANDLERS_H

/* ---------------------------------------------------------------------------
 * Pet handlers
 * --------------------------------------------------------------------------- */

/**
 * @brief Creates a new pet (POST /v2/pet).
 * @param json_payload  Raw JSON body from the request.
 * @return 0 on success, non-zero on failure.
 */
int create_pet(const char* json_payload);

/**
 * @brief Updates an existing pet (PUT /v2/pet).
 * @param json_payload  Raw JSON body from the request.
 * @return 0 on success, non-zero on failure.
 */
int update_pet(const char* json_payload);

/**
 * @brief Deletes a pet by its numeric ID (DELETE /v2/pet/{petId}).
 * @param id_str  The pet ID as a string extracted from the URL.
 * @return 0 on success, non-zero on failure.
 */
int delete_pet(const char* id_str);

/**
 * @brief Finds a pet by ID (GET /v2/pet/{petId}).
 * @param id_str  The pet ID as a string.
 * @return Heap-allocated JSON string (caller frees), or NULL on failure.
 */
char* find_pet_by_id(const char* id_str);

/**
 * @brief Finds pets by status (GET /v2/pet/findByStatus?status=...).
 * @param statuses  Comma-separated status values.
 * @return Heap-allocated JSON array string (caller frees), or NULL.
 */
char* find_pets_by_status(const char* statuses);

/**
 * @brief Finds pets by tags (GET /v2/pet/findByTags?tags=...).
 * @param tags  Comma-separated tag names.
 * @return Heap-allocated JSON array string (caller frees), or NULL.
 */
char* find_pets_by_tags(const char* tags);

/* ---------------------------------------------------------------------------
 * User handlers
 * --------------------------------------------------------------------------- */

/**
 * @brief Creates a new user (POST /v2/user).
 * @param json_payload  Raw JSON body.
 * @return 0 on success, non-zero on failure.
 */
int create_user(const char* json_payload);

/**
 * @brief Updates a user by username (PUT /v2/user/{username}).
 * @param username       The username from the URL path.
 * @param json_payload   Raw JSON body with updated fields.
 * @return 0 on success, non-zero on failure.
 */
int update_user(const char* username, const char* json_payload);

/**
 * @brief Deletes a user by username (DELETE /v2/user/{username}).
 * @param username  The username from the URL path.
 * @return 0 on success, non-zero on failure.
 */
int delete_user(const char* username);

/**
 * @brief Finds a user by username (GET /v2/user/{username}).
 * @param username  The username from the URL path.
 * @return Heap-allocated JSON string (caller frees), or NULL.
 */
char* get_user_by_name(const char* username);

/**
 * @brief Logs a user in (GET /v2/user/login?username=...&password=...).
 * @param username  The username query parameter.
 * @param password  The password query parameter.
 * @return Heap-allocated JSON string (caller frees), or NULL on failure.
 */
char* login_user(const char* username, const char* password);

/**
 * @brief Logs the current user out (GET /v2/user/logout).
 * @return Heap-allocated JSON string (caller frees).
 */
char* logout_user(void);

#endif /* HANDLERS_H */
