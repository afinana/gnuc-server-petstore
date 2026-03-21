#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <microhttpd.h>
#include <mongoc/mongoc.h>
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

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        LOG_ERROR("Termination signal (%d) received, shutting down...", sig);
        keep_running = 0;
    }
}

/* ---------------------------------------------------------------------------
 * HTTP response helper
 * --------------------------------------------------------------------------- */

static enum MHD_Result send_response(struct MHD_Connection* connection,
                                     const char* body,
                                     unsigned int status_code) {
    struct MHD_Response* response =
        MHD_create_response_from_buffer(strlen(body), (void*)body, MHD_RESPMEM_MUST_COPY);
    if (!response) return MHD_NO;

    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, HTTP_CONTENT_TYPE_JSON);
    enum MHD_Result ret = MHD_queue_response(connection, status_code, response);
    MHD_destroy_response(response);
    return ret;
}

/* ---------------------------------------------------------------------------
 * Upload-data accumulator (for POST / PUT bodies)
 * --------------------------------------------------------------------------- */

static bool accumulate_upload_data(void** con_cls,
                                   const char* upload_data,
                                   size_t* upload_data_size) {
    char* data = (char*)*con_cls;
    size_t current_len = data ? strlen(data) : 0;
    char* new_data = realloc(data, current_len + *upload_data_size + 1);
    if (!new_data) return false;

    memcpy(new_data + current_len, upload_data, *upload_data_size);
    new_data[current_len + *upload_data_size] = '\0';
    *con_cls = new_data;
    *upload_data_size = 0;
    return true;
}

/* ---------------------------------------------------------------------------
 * Request router
 * --------------------------------------------------------------------------- */

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

    /* First call: allocate connection-specific buffer */
    if (*con_cls == NULL) {
        *con_cls = calloc(1, sizeof(char));
        return *con_cls ? MHD_YES : MHD_NO;
    }

    LOG_INFO("%s %s", method, url);

    /* ======================================================================
     * PET routes
     * ====================================================================== */

    /* POST /v2/pet — Create a pet */
    if (strcmp(method, "POST") == 0 && strcmp(url, "/v2/pet") == 0) {
        if (*upload_data_size != 0) {
            if (!accumulate_upload_data(con_cls, upload_data, upload_data_size)) return MHD_NO;
            return MHD_YES;
        }
        char* data = (char*)*con_cls;
        int rc = create_pet(data);
        free(data); *con_cls = NULL;
        return rc != 0
            ? send_response(connection, "{\"error\":\"Failed to create pet\"}", MHD_HTTP_INTERNAL_SERVER_ERROR)
            : send_response(connection, "{\"message\":\"Pet created successfully\"}", MHD_HTTP_OK);
    }

    /* PUT /v2/pet — Update a pet */
    if (strcmp(method, "PUT") == 0 && strcmp(url, "/v2/pet") == 0) {
        if (*upload_data_size != 0) {
            if (!accumulate_upload_data(con_cls, upload_data, upload_data_size)) return MHD_NO;
            return MHD_YES;
        }
        char* data = (char*)*con_cls;
        int rc = update_pet(data);
        free(data); *con_cls = NULL;
        return rc != 0
            ? send_response(connection, "{\"error\":\"Failed to update pet\"}", MHD_HTTP_INTERNAL_SERVER_ERROR)
            : send_response(connection, "{\"message\":\"Pet updated successfully\"}", MHD_HTTP_OK);
    }

    /* GET /v2/pet/findByStatus — Find pets by status */
    if (strcmp(method, "GET") == 0 && strcmp(url, "/v2/pet/findByStatus") == 0) {
        const char* status = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "status");
        char* result = find_pets_by_status(status);
        if (!result) return send_response(connection, "{\"error\":\"Failed to find pets by status\"}", MHD_HTTP_INTERNAL_SERVER_ERROR);
        enum MHD_Result ret = send_response(connection, result, MHD_HTTP_OK);
        free(result);
        return ret;
    }

    /* GET /v2/pet/findByTags — Find pets by tags */
    if (strcmp(method, "GET") == 0 && strcmp(url, "/v2/pet/findByTags") == 0) {
        const char* tags = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "tags");
        char* result = find_pets_by_tags(tags);
        if (!result) return send_response(connection, "{\"error\":\"Failed to find pets by tags\"}", MHD_HTTP_INTERNAL_SERVER_ERROR);
        enum MHD_Result ret = send_response(connection, result, MHD_HTTP_OK);
        free(result);
        return ret;
    }

    /* DELETE /v2/pet/{petId} */
    if (strcmp(method, "DELETE") == 0 && strncmp(url, "/v2/pet/", 8) == 0 && strlen(url) > 8) {
        const char* id = url + 8;
        int rc = delete_pet(id);
        return rc != 0
            ? send_response(connection, "{\"error\":\"Failed to delete pet\"}", MHD_HTTP_INTERNAL_SERVER_ERROR)
            : send_response(connection, "{\"message\":\"Pet deleted successfully\"}", MHD_HTTP_OK);
    }

    /* GET /v2/pet/{petId} — Must be AFTER findByStatus and findByTags */
    if (strcmp(method, "GET") == 0 && strncmp(url, "/v2/pet/", 8) == 0 && strlen(url) > 8) {
        const char* id = url + 8;
        char* result = find_pet_by_id(id);
        if (!result) return send_response(connection, "{\"error\":\"Pet not found\"}", MHD_HTTP_NOT_FOUND);
        enum MHD_Result ret = send_response(connection, result, MHD_HTTP_OK);
        free(result);
        return ret;
    }

    /* ======================================================================
     * USER routes — exact matches first, then prefix matches
     * ====================================================================== */

    /* GET /v2/user/login */
    if (strcmp(method, "GET") == 0 && strcmp(url, "/v2/user/login") == 0) {
        const char* username = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "username");
        const char* password = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "password");
        char* result = login_user(username, password);
        if (!result) return send_response(connection, "{\"error\":\"Invalid username/password\"}", MHD_HTTP_BAD_REQUEST);
        enum MHD_Result ret = send_response(connection, result, MHD_HTTP_OK);
        free(result);
        return ret;
    }

    /* GET /v2/user/logout */
    if (strcmp(method, "GET") == 0 && strcmp(url, "/v2/user/logout") == 0) {
        char* result = logout_user();
        enum MHD_Result ret = send_response(connection, result, MHD_HTTP_OK);
        free(result);
        return ret;
    }

    /* POST /v2/user — Create a user */
    if (strcmp(method, "POST") == 0 && strcmp(url, "/v2/user") == 0) {
        if (*upload_data_size != 0) {
            if (!accumulate_upload_data(con_cls, upload_data, upload_data_size)) return MHD_NO;
            return MHD_YES;
        }
        char* data = (char*)*con_cls;
        int rc = create_user(data);
        free(data); *con_cls = NULL;
        return rc != 0
            ? send_response(connection, "{\"error\":\"Failed to create user\"}", MHD_HTTP_INTERNAL_SERVER_ERROR)
            : send_response(connection, "{\"message\":\"User created successfully\"}", MHD_HTTP_OK);
    }

    /* PUT /v2/user/{username} — Update a user */
    if (strcmp(method, "PUT") == 0 && strncmp(url, "/v2/user/", 9) == 0 && strlen(url) > 9) {
        if (*upload_data_size != 0) {
            if (!accumulate_upload_data(con_cls, upload_data, upload_data_size)) return MHD_NO;
            return MHD_YES;
        }
        const char* username = url + 9;
        char* data = (char*)*con_cls;
        int rc = update_user(username, data);
        free(data); *con_cls = NULL;
        return rc != 0
            ? send_response(connection, "{\"error\":\"Failed to update user\"}", MHD_HTTP_INTERNAL_SERVER_ERROR)
            : send_response(connection, "{\"message\":\"User updated successfully\"}", MHD_HTTP_OK);
    }

    /* DELETE /v2/user/{username} */
    if (strcmp(method, "DELETE") == 0 && strncmp(url, "/v2/user/", 9) == 0 && strlen(url) > 9) {
        const char* username = url + 9;
        int rc = delete_user(username);
        return rc != 0
            ? send_response(connection, "{\"error\":\"Failed to delete user\"}", MHD_HTTP_INTERNAL_SERVER_ERROR)
            : send_response(connection, "{\"message\":\"User deleted successfully\"}", MHD_HTTP_OK);
    }

    /* GET /v2/user/{username} — Must be LAST among user routes */
    if (strcmp(method, "GET") == 0 && strncmp(url, "/v2/user/", 9) == 0 && strlen(url) > 9) {
        const char* username = url + 9;
        char* result = get_user_by_name(username);
        if (!result) return send_response(connection, "{\"error\":\"User not found\"}", MHD_HTTP_NOT_FOUND);
        enum MHD_Result ret = send_response(connection, result, MHD_HTTP_OK);
        free(result);
        return ret;
    }

    /* ====================================================================== */

    return send_response(connection, "{\"error\":\"Not found\"}", MHD_HTTP_NOT_FOUND);
}

/* ---------------------------------------------------------------------------
 * Connection cleanup callback
 * --------------------------------------------------------------------------- */

static void request_completed(void* cls, struct MHD_Connection* connection,
                              void** con_cls, enum MHD_RequestTerminationCode toe) {
    (void)cls; (void)connection; (void)toe;
    if (*con_cls) {
        free(*con_cls);
        *con_cls = NULL;
    }
}

/* ---------------------------------------------------------------------------
 * main
 * --------------------------------------------------------------------------- */

int main(void) {
    /* Initialise the MongoDB C Driver */
    mongoc_init();

    const char* port_str = getenv("PORT");
    int listen_port = (port_str != NULL) ? atoi(port_str) : 8080;

    const char* mongo_uri = getenv("MONGO_URI");
    if (!mongo_uri) {
        mongo_uri = "mongodb://localhost:27017";
    }
    LOG_INFO("MONGO_URI: %s", mongo_uri);

    if (db_init(mongo_uri) != 0) {
        LOG_ERROR("Database initialisation failed — exiting");
        mongoc_cleanup();
        return EXIT_FAILURE;
    }

    struct MHD_Daemon* daemon = MHD_start_daemon(
        MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_ERROR_LOG,
        (uint16_t)listen_port,
        NULL, NULL,
        &request_handler, NULL,
        MHD_OPTION_THREAD_POOL_SIZE, (unsigned int)THREAD_POOL_SIZE,
        MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int)120,
        MHD_OPTION_NOTIFY_COMPLETED, &request_completed, NULL,
        MHD_OPTION_END);

    if (!daemon) {
        LOG_ERROR("Failed to start HTTP server");
        db_cleanup();
        mongoc_cleanup();
        return EXIT_FAILURE;
    }

    LOG_INFO("Server running on http://localhost:%d (threads: %d)", listen_port, THREAD_POOL_SIZE);
    fflush(stdout);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    while (keep_running) {
        sleep(1);
    }

    MHD_stop_daemon(daemon);
    db_cleanup();
    mongoc_cleanup();

    LOG_WARN("Server stopped");
    return EXIT_SUCCESS;
}
