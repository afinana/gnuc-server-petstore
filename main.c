#include <microhttpd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>

#include "handlers.h" // Include your API handler functions
#include "database.h" // Include Redis database functions
#include "log-utils.h" // Include the log utils header

#define HTTP_CONTENT_TYPE_JSON "application/json"

volatile sig_atomic_t keep_running = 1;

void handle_signal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        keep_running = 0;
    }
}

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
            if (handle_create_pet(data) != 0) {
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
            if (handle_update_pet(data) != 0) {
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
            if (handle_create_user(data) != 0) {
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

    // Read the port from the environment variable
    const char* env_port = getenv("port");
    int listen_port = (env_port != NULL) ? atoi(env_port) : 8080;

    // Read the database URI from the environment variable
    const char* db_uri = getenv("redisURI");
    if (db_uri == NULL) {
        // Use default URI if not provided
        db_uri = "127.0.0.1";
    }
    // Log db_uri
    LOG_INFO("redisURI: %s", db_uri);

    // Initialize the database
    db_init(db_uri);

    // Start the HTTP server
    //daemon = MHD_start_daemon(MHD_USE_POLL_INTERNAL_THREAD | MHD_USE_ERROR_LOG,
    daemon = MHD_start_daemon( MHD_USE_INTERNAL_POLLING_THREAD,
        listen_port,
        NULL,
        NULL,
        &request_handler,
        NULL,
        MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int)120,
        MHD_OPTION_END);

    if (NULL == daemon) {
        LOG_ERROR("Failed to start HTTP server");
        db_cleanup();
        return 1;
    }
    LOG_INFO("Server is running on http://localhost:%d", listen_port);

    // Set up signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Keep running until a termination signal is received    
    while (keep_running) {
        sleep(1);
    }

    MHD_stop_daemon(daemon);

    // Cleanup the database connection
    db_cleanup();

    LOG_WARN("Server is down");

    return 0;
}
