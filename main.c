#include <microhttpd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>

#include "handlers.h"
#include "database.h"
#include "log-utils.h"

#define HTTP_CONTENT_TYPE_JSON "application/json"
#define THREAD_POOL_SIZE 4

volatile sig_atomic_t keep_running = 1;

void handle_signal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        LOG_ERROR("Termination signal (%d) received, shutting down...", signal);
        keep_running = 0;
    }
}

/**
 * @brief Creates and sends an HTTP response.
 */
static int send_response(struct MHD_Connection* connection, const char* message, unsigned int status_code) {
    struct MHD_Response* response = MHD_create_response_from_buffer(strlen(message), (void*)message, MHD_RESPMEM_MUST_COPY);
    if (!response) {
        return MHD_NO;
    }
    
    // Set the application/json content type header for RESTful responses
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, HTTP_CONTENT_TYPE_JSON);
    
    // CORS: Allow cross-origin requests from any Origin (needed for frontend UI like Angular)
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    
    // CORS: Allow all standard CRUD and preflight HTTP methods
    MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    
    // CORS: Explicitly permit headers required by browser-based JSON requests
    MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type, Authorization, Origin, Accept");
    
    // CORS: Cache preflight request authorization for 24 hours to reduce latency
    MHD_add_response_header(response, "Access-Control-Max-Age", "86400");
    
    int ret = MHD_queue_response(connection, status_code, response);
    MHD_destroy_response(response);
    return ret;
}

/**
 * @brief Accumulates chunked upload data into a dynamically growing buffer.
 *
 * @param con_cls Pointer to connection-specific data (the buffer).
 * @param upload_data The data chunk received.
 * @param upload_data_size Pointer to the size of the chunk; set to 0 on success.
 * @return true on success, false on memory allocation failure.
 */
static bool accumulate_upload_data(void** con_cls, const char* upload_data, size_t* upload_data_size) {
    char* data = (char*)*con_cls;
    size_t current_length = data ? strlen(data) : 0;
    char* new_data = realloc(data, current_length + *upload_data_size + 1);
    if (new_data == NULL) {
        return false;
    }
    memcpy(new_data + current_length, upload_data, *upload_data_size);
    new_data[current_length + *upload_data_size] = '\0';
    *con_cls = new_data;
    *upload_data_size = 0;
    return true;
}

/**
 * @brief Handles incoming HTTP requests and routes them to the appropriate handler.
 */
static enum MHD_Result request_handler(void* cls,
    struct MHD_Connection* connection,
    const char* url,
    const char* method,
    const char* version,
    const char* upload_data,
    size_t* upload_data_size,
    void** con_cls) {

    (void)cls;
    (void)version;

    // First call: allocate connection-specific data
    if (*con_cls == NULL) {
        *con_cls = calloc(1, sizeof(char));
        if (*con_cls == NULL) {
            return MHD_NO;
        }
        return MHD_YES;
    }

    LOG_INFO("%s %s", method, url);

    // Handle CORS preflight (OPTIONS)
    if (strcmp(method, "OPTIONS") == 0) {
        return send_response(connection, "", MHD_HTTP_OK);
    }

    // ========================
    // Pet routes
    // ========================

    // POST /v2/pet — Create a pet
    if (strcmp(method, "POST") == 0 && strcmp(url, "/v2/pet") == 0) {
        if (*upload_data_size != 0) {
            if (!accumulate_upload_data(con_cls, upload_data, upload_data_size)) {
                return MHD_NO;
            }
            return MHD_YES;
        }
        char* data = (char*)*con_cls;
        int result = handle_create_pet(data);
        free(data);
        *con_cls = NULL;
        return result != 0
            ? send_response(connection, "{\"error\":\"Failed to create pet\"}", MHD_HTTP_INTERNAL_SERVER_ERROR)
            : send_response(connection, "{\"message\":\"Pet created successfully\"}", MHD_HTTP_OK);
    }

    // PUT /v2/pet — Update a pet
    if (strcmp(method, "PUT") == 0 && strcmp(url, "/v2/pet") == 0) {
        if (*upload_data_size != 0) {
            if (!accumulate_upload_data(con_cls, upload_data, upload_data_size)) {
                return MHD_NO;
            }
            return MHD_YES;
        }
        char* data = (char*)*con_cls;
        int result = handle_update_pet(data);
        free(data);
        *con_cls = NULL;
        return result != 0
            ? send_response(connection, "{\"error\":\"Failed to update pet\"}", MHD_HTTP_INTERNAL_SERVER_ERROR)
            : send_response(connection, "{\"message\":\"Pet updated successfully\"}", MHD_HTTP_OK);
    }

    // GET /v2/pet — List all pets (exact match, must precede /v2/pet/{id})
    if (strcmp(method, "GET") == 0 && strcmp(url, "/v2/pet") == 0) {
        char* result = handle_get_all_pets();
        if (result == NULL) {
            return send_response(connection, "{\"error\":\"Failed to find pets\"}", MHD_HTTP_INTERNAL_SERVER_ERROR);
        }
        int ret = send_response(connection, result, MHD_HTTP_OK);
        free(result);
        return ret;
    }

    // GET /v2/pet/findByTags — Search by tags (must come before /v2/pet/{id})
    if (strcmp(method, "GET") == 0 && strcmp(url, "/v2/pet/findByTags") == 0) {
        const char* tags = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "tags");
        char* result = handle_get_pet_by_tags(tags);
        if (result == NULL) {
            return send_response(connection, "{\"error\":\"Failed to find pets by tags\"}", MHD_HTTP_INTERNAL_SERVER_ERROR);
        }
        int ret = send_response(connection, result, MHD_HTTP_OK);
        free(result);
        return ret;
    }

    // GET /v2/pet/findByStatus — Search by status (must come before /v2/pet/{id})
    if (strcmp(method, "GET") == 0 && strcmp(url, "/v2/pet/findByStatus") == 0) {
        const char* state = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "status");
        char* result = handle_get_pet_by_state(state);
        if (result == NULL) {
            return send_response(connection, "{\"error\":\"Failed to find pets by state\"}", MHD_HTTP_INTERNAL_SERVER_ERROR);
        }
        int ret = send_response(connection, result, MHD_HTTP_OK);
        free(result);
        return ret;
    }

    // DELETE /v2/pet/{id} — Delete a pet
    if (strcmp(method, "DELETE") == 0 && strncmp(url, "/v2/pet/", 8) == 0 && strlen(url) > 8) {
        const char* id = url + 8;
        if (handle_delete_pet(id) != 0) {
            return send_response(connection, "{\"error\":\"Failed to delete pet\"}", MHD_HTTP_INTERNAL_SERVER_ERROR);
        }
        return send_response(connection, "{\"message\":\"Pet deleted successfully\"}", MHD_HTTP_OK);
    }

    // GET /v2/pet/{petId} — Get pet by ID
    if (strcmp(method, "GET") == 0 && strncmp(url, "/v2/pet/", 8) == 0 && strlen(url) > 8) {
        const char* id = url + 8;
        char* result = handle_get_pet_by_id(id);
        if (result == NULL) {
            return send_response(connection, "{\"error\":\"Failed to find pet by ID\"}", MHD_HTTP_INTERNAL_SERVER_ERROR);
        }
        int ret = send_response(connection, result, MHD_HTTP_OK);
        free(result);
        return ret;
    }

    // ========================
    // User routes — exact matches first, then prefix matches
    // ========================

    // POST /v2/user/login — Login (exact match, must precede /v2/user/{username})
    if (strcmp(method, "POST") == 0 && strcmp(url, "/v2/user/login") == 0) {
        if (*upload_data_size != 0) {
            if (!accumulate_upload_data(con_cls, upload_data, upload_data_size)) {
                return MHD_NO;
            }
            return MHD_YES;
        }
        char* data = (char*)*con_cls;
        int result = handle_post_user_login(data);
        free(data);
        *con_cls = NULL;
        return result != 0
            ? send_response(connection, "{\"error\":\"Failed to login user\"}", MHD_HTTP_INTERNAL_SERVER_ERROR)
            : send_response(connection, "{\"message\":\"User logged in successfully\"}", MHD_HTTP_OK);
    }

    // POST /v2/user/logout — Logout (exact match, must precede /v2/user/{username})
    if (strcmp(method, "POST") == 0 && strcmp(url, "/v2/user/logout") == 0) {
        const char* username = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "username");
        char* result = handle_post_user_logout(username);
        if (result == NULL) {
            return send_response(connection, "{\"error\":\"Failed to logout user\"}", MHD_HTTP_INTERNAL_SERVER_ERROR);
        }
        int ret = send_response(connection, result, MHD_HTTP_OK);
        free(result);
        return ret;
    }

    // POST /v2/user — Create a user (exact match)
    if (strcmp(method, "POST") == 0 && strcmp(url, "/v2/user") == 0) {
        if (*upload_data_size != 0) {
            if (!accumulate_upload_data(con_cls, upload_data, upload_data_size)) {
                return MHD_NO;
            }
            return MHD_YES;
        }
        char* data = (char*)*con_cls;
        int result = handle_create_user(data);
        free(data);
        *con_cls = NULL;
        return result != 0
            ? send_response(connection, "{\"error\":\"Failed to create user\"}", MHD_HTTP_INTERNAL_SERVER_ERROR)
            : send_response(connection, "{\"message\":\"User created successfully\"}", MHD_HTTP_OK);
    }

    // GET /v2/user — List all users (exact match, must precede /v2/user/{username})
    if (strcmp(method, "GET") == 0 && strcmp(url, "/v2/user") == 0) {
        char* result = handle_get_all_users();
        if (result == NULL) {
            return send_response(connection, "{\"error\":\"Failed to find users\"}", MHD_HTTP_INTERNAL_SERVER_ERROR);
        }
        int ret = send_response(connection, result, MHD_HTTP_OK);
        free(result);
        return ret;
    }

    // PUT /v2/user — Update a user (exact match)
    if (strcmp(method, "PUT") == 0 && strcmp(url, "/v2/user") == 0) {
        if (*upload_data_size != 0) {
            if (!accumulate_upload_data(con_cls, upload_data, upload_data_size)) {
                return MHD_NO;
            }
            return MHD_YES;
        }
        char* data = (char*)*con_cls;
        int result = handle_update_user(data);
        free(data);
        *con_cls = NULL;
        return result != 0
            ? send_response(connection, "{\"error\":\"Failed to update user\"}", MHD_HTTP_INTERNAL_SERVER_ERROR)
            : send_response(connection, "{\"message\":\"User updated successfully\"}", MHD_HTTP_OK);
    }

    // DELETE /v2/user/{username} — Delete a user (prefix match)
    if (strcmp(method, "DELETE") == 0 && strncmp(url, "/v2/user/", 9) == 0 && strlen(url) > 9) {
        const char* username = url + 9;
        if (handle_delete_user(username) != 0) {
            return send_response(connection, "{\"error\":\"Failed to delete user\"}", MHD_HTTP_INTERNAL_SERVER_ERROR);
        }
        return send_response(connection, "{\"message\":\"User deleted successfully\"}", MHD_HTTP_OK);
    }

    // GET /v2/user/findByName/{username} — Get user by username (prefix match, must precede /v2/user/{id})
    // This allows dedicated lookup using the 'username' field (e.g. from /v2/user/findByName/alice)
    if (strcmp(method, "GET") == 0 && strncmp(url, "/v2/user/findByName/", 20) == 0 && strlen(url) > 20) {
        const char* username = url + 20; // Extract username parameter starting after '/v2/user/findByName/'
        char* result = handle_get_user_by_username(username);
        if (result == NULL) {
            return send_response(connection, "{\"error\":\"Failed to find user by username\"}", MHD_HTTP_INTERNAL_SERVER_ERROR);
        }
        int ret = send_response(connection, result, MHD_HTTP_OK);
        free(result);
        return ret;
    }

    // GET /v2/user/{id} — Get user by ID (prefix match, must be last)
    // This allows dedicated lookup using either a numeric user ID or a standard ID segment
    if (strcmp(method, "GET") == 0 && strncmp(url, "/v2/user/", 9) == 0 && strlen(url) > 9) {
        const char* id = url + 9; // Extract ID parameter starting after '/v2/user/'
        char* result = handle_get_user_by_id(id);
        if (result == NULL) {
            return send_response(connection, "{\"error\":\"Failed to find user by ID\"}", MHD_HTTP_INTERNAL_SERVER_ERROR);
        }
        int ret = send_response(connection, result, MHD_HTTP_OK);
        free(result);
        return ret;
    }

    // No route matched
    return send_response(connection, "{\"error\":\"Not found\"}", MHD_HTTP_NOT_FOUND);
}

/**
 * @brief Frees connection-specific data when a request completes.
 */
static void request_completed(void* cls, struct MHD_Connection* connection,
    void** con_cls, enum MHD_RequestTerminationCode toe) {
    (void)cls;
    (void)connection;
    (void)toe;
    if (*con_cls != NULL) {
        free(*con_cls);
        *con_cls = NULL;
    }
}

/**
 * @brief The main function. Initializes database, starts the HTTP server with
 *        a thread pool, and waits for a termination signal.
 */
int main(void) {
    struct MHD_Daemon* daemon;

    const char* env_port = getenv("port");
    int listen_port = (env_port != NULL) ? atoi(env_port) : 8080;

    const char* db_uri = getenv("mongoURI");
    if (db_uri == NULL) {
        db_uri = "mongodb://root@127.0.0.1:27017/admin?retryWrites=true&loadBalanced=false&connectTimeoutMS=10000&authSource=admin&authMechanism=SCRAM-SHA-256";
    }
    LOG_INFO("mongoURI: %s", db_uri);

    db_init(db_uri);

    // Start the HTTP server with a thread pool for concurrent request handling
    daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_ERROR_LOG,
        listen_port,
        NULL,
        NULL,
        &request_handler,
        NULL,
        MHD_OPTION_THREAD_POOL_SIZE, (unsigned int)THREAD_POOL_SIZE,
        MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int)120,
        MHD_OPTION_NOTIFY_COMPLETED, &request_completed, NULL,
        MHD_OPTION_END);

    if (NULL == daemon) {
        LOG_ERROR("Failed to start HTTP server");
        db_cleanup();
        return 1;
    }
    LOG_INFO("Server is running on http://localhost:%d (thread pool: %d)", listen_port, THREAD_POOL_SIZE);
    fflush(stdout);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    while (keep_running) {
        sleep(1);
    }

    MHD_stop_daemon(daemon);
    db_cleanup();

    LOG_WARN("Server is down");

    return 0;
}
