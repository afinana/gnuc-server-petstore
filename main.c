#include <microhttpd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <syslog.h>
#include "handlers.h" // Include your API handler functions
#include "database.h"// Include mongodb database functions

#define HTTP_CONTENT_TYPE_JSON "application/json"

/**
 * @brief Creates and sends an HTTP response.
 *
 * @param connection The MHD_Connection object.
 * @param message The response message to send.
 * @param status_code The HTTP status code.
 * @return int Returns MHD_YES on success, MHD_NO on failure.
 */
static int send_response(struct MHD_Connection* connection, const char* message, unsigned int status_code) {
    struct MHD_Response* response = MHD_create_response_from_buffer(strlen(message), (void*)message, MHD_RESPMEM_MUST_COPY);
    if (!response) {
        return MHD_NO;
    }

    // Set the content type to application/json
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, HTTP_CONTENT_TYPE_JSON);

    int ret = MHD_queue_response(connection, status_code, response);
    MHD_destroy_response(response);
    return ret;
}

/**
 * @brief Handles the POST /pet route.
 *
 * @param json_payload The JSON payload containing the pet details.
 * @return int Returns 0 on success, non-zero on failure.
 */
int handle_post_pet(const char* json_payload) {
    return create_pet(json_payload); // Function from handlers.c
}

/**
 * @brief Handles the PUT /pet route.
 *
 * @param json_payload The JSON payload containing the updated pet details.
 * @return int Returns 0 on success, non-zero on failure.
 */
int handle_put_pet(const char* json_payload) {
    return update_pet(json_payload); // Function from handlers.c
}

/**
 * @brief Handles the DELETE /pet/{id} route.
 *
 * @param id The ID of the pet to delete.
 * @return int Returns 0 on success, non-zero on failure.
 */
int handle_delete_pet(const char* id) {
    return delete_pet_by_id(id); // Function from handlers.c
}

/**
 * @brief Handles the GET /pet/findByTags route.
 *
 * @param tags The tags to search for.
 * @return char* A JSON string containing the list of pets that match the tags.
 *         The caller is responsible for freeing the returned string.
 */
char* handle_get_pet_by_tags(const char* tags) {
    return find_pets_by_tags(tags); // Function from handlers.c
}

/**
 * @brief Handles the GET /pet/findByState route.
 *
 * @param state The state to search for.
 * @return char* A JSON string containing the list of pets that match the state.
 *         The caller is responsible for freeing the returned string.
 */
char* handle_get_pet_by_state(const char* state) {
    return find_pets_by_state(state); // Function from handlers.c
}

/**
 * @brief Handles the GET /pet/{petId} route.
 *
 * @param id The ID of the pet to search for.
 * @return char* A JSON string containing the pet details.
 *         The caller is responsible for freeing the returned string.
 */
char* handle_get_pet_by_id(const char* id) {
    return find_pet_by_id(id); // Function from handlers.c
}


// Route handler for Users methods
/**
 * @brief Handles the POST /user route.
 *
 * @param json_payload The JSON payload containing the user details.
 * @return int Returns 0 on success, non-zero on failure.
 */
int handle_post_user(const char* json_payload) {
    return create_user(json_payload); // Function from handlers.c
}

/**
 * @brief Handles the PUT /user route.
 *
 * @param json_payload The JSON payload containing the updated user details.
 * @return int Returns 0 on success, non-zero on failure.
 */
int handle_put_user(const char* json_payload) {
    return update_user(json_payload); // Function from handlers.c
}

/**
 * @brief Handles the DELETE /user/{id} route.
 *
 * @param id The ID of the user to delete.
 * @return int Returns 0 on success, non-zero on failure.
 */
int handle_delete_user(const char* id) {
    return delete_user_by_id(id); // Function from handlers.c
}

/**
 * @brief Handles the GET /user route.
 *
 * @return char* A JSON string containing the list of users.
 *         The caller is responsible for freeing the returned string.
 */
char* handle_get_all_users() {
    return find_all_users(); // Function from handlers.c
}


/**
 * @brief Handles the GET /user/{username} route.
 *
 * @param username The username of the user to search for.
 * @return char* A JSON string containing the user details.
 *         The caller is responsible for freeing the returned string.
 */
char* handle_get_user_by_username(const char* username) {
    return find_user_by_username(username); // Function from handlers.c
}


/**
 * @brief Handles incoming HTTP requests and routes them to the appropriate handler.
 *
 * @param cls Unused parameter.
 * @param connection The MHD_Connection object.
 * @param url The requested URL.
 * @param method The HTTP method (GET, POST, PUT, DELETE).
 * @param version The HTTP version.
 * @param upload_data The data uploaded in the request.
 * @param upload_data_size The size of the uploaded data.
 * @param con_cls Pointer to connection-specific data.
 * @return MHD_Result Returns MHD_YES on success, MHD_NO on failure.
 */
static enum MHD_Result request_handler(void* cls,
    struct MHD_Connection* connection,
    const char* url,
    const char* method,
    const char* version,
    const char* upload_data,
    size_t* upload_data_size,
    void** con_cls) {

    (void)cls; // Mark unused parameter
    (void)version; // Mark unused parameter

    // Allocate memory for connection-specific data if not already allocated
    if (*con_cls == NULL) {
        *con_cls = calloc(1, sizeof(char));
        if (*con_cls == NULL) {
            return MHD_NO;
        }
        return MHD_YES;
    }
    // Log the request
    syslog(LOG_INFO, "%s %s", method, url);

    // Handle POST /pet
    if (strcmp(method, "POST") == 0 && strcmp(url, "/v2/pet") == 0) {
        if (*upload_data_size != 0) {
            // Accumulate the uploaded data
            char* data = (char*)*con_cls;
            size_t current_length = data ? strlen(data) : 0;
            data = realloc(data, current_length + *upload_data_size + 1);
            if (data == NULL) {
                return MHD_NO;
            }
            memcpy(data + current_length, upload_data, *upload_data_size);
            data[current_length + *upload_data_size] = '\0';
            *con_cls = data;
            *upload_data_size = 0;
            return MHD_YES;
        }
        else {
            // Process the accumulated data
            char* data = (char*)*con_cls;
            if (handle_post_pet(data) != 0) {
                free(data);
                return send_response(connection, "Failed to create pet", MHD_HTTP_INTERNAL_SERVER_ERROR);
            }
            free(data);
            return send_response(connection, "Pet created successfully", MHD_HTTP_OK);
        }
    }
    // Handle PUT /pet
    else if (strcmp(method, "PUT") == 0 && strcmp(url, "/v2/pet") == 0) {
        if (*upload_data_size != 0) {
            // Accumulate the uploaded data
            char* data = (char*)*con_cls;
            size_t current_length = data ? strlen(data) : 0;
            data = realloc(data, current_length + *upload_data_size + 1);
            if (data == NULL) {
                return MHD_NO;
            }
            memcpy(data + current_length, upload_data, *upload_data_size);
            data[current_length + *upload_data_size] = '\0';
            *con_cls = data;
            *upload_data_size = 0;
            return MHD_YES;
        }
        else {
            // Process the accumulated data
            char* data = (char*)*con_cls;
            if (handle_put_pet(data) != 0) {
                free(data);
                return send_response(connection, "Failed to update pet", MHD_HTTP_INTERNAL_SERVER_ERROR);
            }
            free(data);
            return send_response(connection, "Pet updated successfully", MHD_HTTP_OK);
        }
    }
    // Handle DELETE /pet/{id}
    else if (strncmp(url, "/v2/pet/", 7) == 0 && strcmp(method, "DELETE") == 0) {
        const char* id = url + 7; // Extract ID from URL
        if (handle_delete_pet(id) != 0) {
            return send_response(connection, "Failed to delete pet", MHD_HTTP_INTERNAL_SERVER_ERROR);
        }
        return send_response(connection, "Pet deleted successfully", MHD_HTTP_OK);
    }
    // Handle GET /pet/findByTags
    else if (strcmp(url, "/v2/pet/findByTags") == 0 && strcmp(method, "GET") == 0) {
        const char* tags = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "tags");
        char* result = handle_get_pet_by_tags(tags);
        if (result == NULL) {
            return send_response(connection, "Failed to find pets by tags", MHD_HTTP_INTERNAL_SERVER_ERROR);
        }
        int ret = send_response(connection, result, MHD_HTTP_OK);
        free(result);
        return ret;
    }
    // Handle GET /pet/findByState
    else if (strcmp(url, "/v2/pet/findByStatus") == 0 && strcmp(method, "GET") == 0) {
        const char* state = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "status");
        char* result = handle_get_pet_by_state(state);
        if (result == NULL) {
            return send_response(connection, "Failed to find pets by state", MHD_HTTP_INTERNAL_SERVER_ERROR);
        }
        int ret = send_response(connection, result, MHD_HTTP_OK);
        free(result);
        return ret;
    }
    // Handle GET /pet/{petId}
    else if (strncmp(url, "/v2/pet/", 7) == 0 && strcmp(method, "GET") == 0) {
        const char* id = url + 8; // Extract ID from URL
        char* result = handle_get_pet_by_id(id);
        if (result == NULL) {
            return send_response(connection, "Failed to find pet by ID", MHD_HTTP_INTERNAL_SERVER_ERROR);
        }
        int ret = send_response(connection, result, MHD_HTTP_OK);
        free(result);
        return ret;
    }
    // User methods POST /v2/user
    else if (strcmp(url, "/v2/user") == 0 && strcmp(method, "POST") == 0) {
        if (*upload_data_size != 0) {
            // Accumulate the uploaded data
            char* data = (char*)*con_cls;
            size_t current_length = data ? strlen(data) : 0;
            data = realloc(data, current_length + *upload_data_size + 1);
            if (data == NULL) {
                return MHD_NO;
            }
            memcpy(data + current_length, upload_data, *upload_data_size);
            data[current_length + *upload_data_size] = '\0';
            *con_cls = data;
            *upload_data_size = 0;
            return MHD_YES;
        }
        else {
            // Process the accumulated data
            char* data = (char*)*con_cls;
            if (handle_post_user(data) != 0) {
                free(data);
                return send_response(connection, "Failed to create user", MHD_HTTP_INTERNAL_SERVER_ERROR);
            }
            free(data);
            return send_response(connection, "User created successfully", MHD_HTTP_OK);
        }
    }
    // User methods GET /v2/user/{username}
    else if (strncmp(url, "/v2/user/", 9) == 0 && strcmp(method, "GET") == 0) {
        const char* username = url + 9; // Extract ID from URL
        char* result = handle_get_user_by_username(username);
        if (result == NULL) {
            return send_response(connection, "Failed to find user by username", MHD_HTTP_INTERNAL_SERVER_ERROR);
        }
        int ret = send_response(connection, result, MHD_HTTP_OK);
        free(result);
        return ret;
    }
    // User method GET /v2/user
    else if (strcmp(url, "/v2/user") == 0 && strcmp(method, "GET") == 0) {
        char* result = handle_get_all_users();
        if (result == NULL) {
            return send_response(connection, "Failed to find users", MHD_HTTP_INTERNAL_SERVER_ERROR);
        }
        int ret = send_response(connection, result, MHD_HTTP_OK);
        free(result);
        return ret;
    }
    // User methods DELETE /v2/user/{username}
    else if (strncmp(url, "/v2/user/", 9) == 0 && strcmp(method, "DELETE") == 0) {
        const char* username = url + 9; // Extract ID from URL
        if (handle_delete_user(username) != 0) {
            return send_response(connection, "Failed to delete user", MHD_HTTP_INTERNAL_SERVER_ERROR);
        }
        return send_response(connection, "User deleted successfully", MHD_HTTP_OK);
    }
    // User methods POST /v2/user/login
    else if (strcmp(url, "/v2/user/login") == 0 && strcmp(method, "POST") == 0) {
        if (*upload_data_size != 0) {
            // Accumulate the uploaded data
            char* data = (char*)*con_cls;
            size_t current_length = data ? strlen(data) : 0;
            data = realloc(data, current_length + *upload_data_size + 1);
            if (data == NULL) {
                return MHD_NO;
            }
            memcpy(data + current_length, upload_data, *upload_data_size);
            data[current_length + *upload_data_size] = '\0';
            *con_cls = data;
            *upload_data_size = 0;
            return MHD_YES;
        }
        else {
            // Process the accumulated data
            char* data = (char*)*con_cls;
            if (handle_post_user_login(data) != 0) {
                free(data);
                return send_response(connection, "Failed to login user", MHD_HTTP_INTERNAL_SERVER_ERROR);
            }
            free(data);
            return send_response(connection, "User logged in successfully", MHD_HTTP_OK);
        }
    }
    // User methods GET /v2/user/logout
    else if (strcmp(url, "/v2/user/logout") == 0 && strcmp(method, "POST") == 0) {
        const char* username = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "username");
        char* result = handle_post_user_logout(username);
        if (result == NULL) {
            return send_response(connection, "Failed to logout user", MHD_HTTP_INTERNAL_SERVER_ERROR);
        }
        int ret = send_response(connection, result, MHD_HTTP_OK);
        free(result);
        return ret;
    }

    // If no route matches, return 404
    return send_response(connection, "Not found", MHD_HTTP_NOT_FOUND);
}

/**
 * @brief The main function that initializes the database, starts the HTTP server, and waits for user input to stop the server.
 *
 * @return int Returns 0 on success, 1 on failure.
 */
int main() {
    struct MHD_Daemon* daemon;

    // Open a connection to the syslog
    openlog("petstore_server", LOG_PID | LOG_CONS, LOG_USER);

    // Read the port from the environment variable
    const char* env_port = getenv("PORT");
    int listen_port = (env_port != NULL) ? atoi(env_port) : 8080;

    // Read the database URI from the environment variable
    const char* db_uri = getenv("DB_URI");
    if (db_uri == NULL) {
        // Use default URI if not provided
        db_uri = "mongodb://root:example@127.0.0.1:27017/admin?retryWrites=true&loadBalanced=false&connectTimeoutMS=10000&authSource=admin&authMechanism=SCRAM-SHA-256";
    }
    // Log db_uri
    syslog(LOG_INFO, "DB_URI: %s", db_uri);

    // Initialize the database
    db_init(db_uri);

    // Start the HTTP server
    daemon = MHD_start_daemon(MHD_USE_POLL_INTERNAL_THREAD | MHD_USE_ERROR_LOG,
        listen_port,
        NULL,
        NULL,
        &request_handler,
        NULL,
        MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int)120,
        MHD_OPTION_END);

    if (NULL == daemon) {
        syslog(LOG_ERR, "Failed to start HTTP server");
        db_cleanup();
        closelog();
        return 1;
    }

    syslog(LOG_INFO, "Server is running on http://localhost:%d", listen_port);
	// show the message by console
	printf("Server is running on http://localhost:%d\n", listen_port);
    
    
    (void)getc(stdin); // Wait until the user presses Enter

 
    MHD_stop_daemon(daemon);

    // Cleanup the database connection
    db_cleanup();

    // Log db_uri
    syslog(LOG_ALERT, "Server is down");

    // Close the connection to the syslog
    closelog();

    return 0;
}
