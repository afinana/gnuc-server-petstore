#define _GNU_SOURCE
#include "handlers.h"
#include "database.h"
#include "log-utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------------- */

/**
 * @brief Parses a raw JSON string into a bson_t document.
 * @return Heap-allocated bson_t (caller must bson_destroy + free), or NULL.
 */
static bson_t* parse_json_to_bson(const char* json) {
    bson_error_t error;
    bson_t* doc = bson_new_from_json((const uint8_t*)json, -1, &error);
    if (!doc) {
        LOG_ERROR("JSON parse error: %s", error.message);
    }
    return doc;
}

/**
 * @brief Converts a bson_t document to a heap-allocated JSON string.
 *        Uses relaxed extended JSON for cleaner numeric output.
 */
static char* bson_to_json(const bson_t* doc) {
    size_t len;
    char* json = bson_as_relaxed_extended_json(doc, &len);
    /* bson_as_relaxed_extended_json returns bson_malloc'd memory.
     * We strdup so the caller can simply free(). */
    char* copy = strdup(json);
    bson_free(json);
    return copy;
}



/**
 * @brief Parses a comma-separated string into an array of tokens.
 *        Caller must free the returned array AND each element.
 */
static char** split_csv(const char* csv, int* out_count) {
    *out_count = 0;
    if (!csv || csv[0] == '\0') return NULL;

    char* copy = strdup(csv);
    if (!copy) return NULL;

    /* Count tokens */
    int count = 1;
    for (const char* p = csv; *p; p++) {
        if (*p == ',') count++;
    }

    char** tokens = malloc(sizeof(char*) * (size_t)count);
    if (!tokens) { free(copy); return NULL; }

    int i = 0;
    char* saveptr;
    char* tok = strtok_r(copy, ",", &saveptr);
    while (tok && i < count) {
        /* Trim leading whitespace */
        while (*tok == ' ') tok++;
        tokens[i++] = strdup(tok);
        tok = strtok_r(NULL, ",", &saveptr);
    }

    free(copy);
    *out_count = i;
    return tokens;
}

/* ---------------------------------------------------------------------------
 * Pet handlers
 * --------------------------------------------------------------------------- */

int create_pet(const char* json_payload) {
    LOG_INFO("create_pet");
    bson_t* doc = parse_json_to_bson(json_payload);
    if (!doc) return EXIT_FAILURE;

    bool ok = db_insert_one("pets", doc);
    bson_destroy(doc);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int update_pet(const char* json_payload) {
    LOG_INFO("update_pet");
    bson_t* doc = parse_json_to_bson(json_payload);
    if (!doc) return EXIT_FAILURE;

    /* Extract the "id" field to build the filter */
    bson_iter_t iter;
    if (!bson_iter_init_find(&iter, doc, "id")) {
        LOG_ERROR("Missing 'id' field in pet JSON");
        bson_destroy(doc);
        return EXIT_FAILURE;
    }

    int64_t pet_id = 0;
    if (BSON_ITER_HOLDS_INT64(&iter)) {
        pet_id = bson_iter_int64(&iter);
    } else if (BSON_ITER_HOLDS_INT32(&iter)) {
        pet_id = (int64_t)bson_iter_int32(&iter);
    } else if (BSON_ITER_HOLDS_DOUBLE(&iter)) {
        pet_id = (int64_t)bson_iter_double(&iter);
    } else {
        LOG_ERROR("'id' field is not a numeric type");
        bson_destroy(doc);
        return EXIT_FAILURE;
    }

    bson_t* filter = BCON_NEW("id", BCON_INT64(pet_id));
    bson_t* update = BCON_NEW("$set", BCON_DOCUMENT(doc));

    bool ok = db_update_one("pets", filter, update);

    bson_destroy(update);
    bson_destroy(filter);
    bson_destroy(doc);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int delete_pet(const char* id_str) {
    LOG_INFO("delete_pet id=%s", id_str);

    int64_t pet_id = strtoll(id_str, NULL, 10);
    bson_t* filter = BCON_NEW("id", BCON_INT64(pet_id));

    bool ok = db_delete_one("pets", filter);
    bson_destroy(filter);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

char* find_pet_by_id(const char* id_str) {
    LOG_INFO("find_pet_by_id id=%s", id_str);

    int64_t pet_id = strtoll(id_str, NULL, 10);
    bson_t* filter = BCON_NEW("id", BCON_INT64(pet_id));

    bson_t* doc = db_find_one("pets", filter);
    bson_destroy(filter);

    if (!doc) {
        LOG_ERROR("Pet not found with id=%s", id_str);
        return NULL;
    }

    char* json = bson_to_json(doc);
    bson_destroy(doc);
    return json;
}

char* find_pets_by_status(const char* statuses) {
    if (!statuses || statuses[0] == '\0') {
        LOG_ERROR("No status values provided");
        return strdup("[]");
    }
    LOG_INFO("find_pets_by_status statuses=%s", statuses);

    int count = 0;
    char** tokens = split_csv(statuses, &count);
    if (!tokens || count == 0) return strdup("[]");

    /* Build filter: { "status": { "$in": ["available", "sold", ...] } } */
    bson_t* filter = bson_new();
    bson_t in_doc, in_array;
    bson_append_document_begin(filter, "status", -1, &in_doc);
    bson_append_array_begin(&in_doc, "$in", -1, &in_array);
    for (int i = 0; i < count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "%d", i);
        bson_append_utf8(&in_array, key, -1, tokens[i], -1);
        free(tokens[i]);
    }
    bson_append_array_end(&in_doc, &in_array);
    bson_append_document_end(filter, &in_doc);
    free(tokens);

    char* result = db_find_as_json_array("pets", filter);
    bson_destroy(filter);

    return result;
}

char* find_pets_by_tags(const char* tags) {
    if (!tags || tags[0] == '\0') {
        LOG_ERROR("No tag values provided");
        return strdup("[]");
    }
    LOG_INFO("find_pets_by_tags tags=%s", tags);

    int count = 0;
    char** tokens = split_csv(tags, &count);
    if (!tokens || count == 0) return strdup("[]");

    /* Build filter: { "tags.name": { "$in": ["tag1", "tag2", ...] } } */
    bson_t* filter = bson_new();
    bson_t in_doc, in_array;
    bson_append_document_begin(filter, "tags.name", -1, &in_doc);
    bson_append_array_begin(&in_doc, "$in", -1, &in_array);
    for (int i = 0; i < count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "%d", i);
        bson_append_utf8(&in_array, key, -1, tokens[i], -1);
        free(tokens[i]);
    }
    bson_append_array_end(&in_doc, &in_array);
    bson_append_document_end(filter, &in_doc);
    free(tokens);

    char* result = db_find_as_json_array("pets", filter);
    bson_destroy(filter);

    return result;
}

/* ---------------------------------------------------------------------------
 * User handlers
 * --------------------------------------------------------------------------- */

int create_user(const char* json_payload) {
    LOG_INFO("create_user");
    bson_t* doc = parse_json_to_bson(json_payload);
    if (!doc) return EXIT_FAILURE;

    bool ok = db_insert_one("users", doc);
    bson_destroy(doc);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int update_user(const char* username, const char* json_payload) {
    LOG_INFO("update_user username=%s", username);
    bson_t* doc = parse_json_to_bson(json_payload);
    if (!doc) return EXIT_FAILURE;

    bson_t* filter = BCON_NEW("username", BCON_UTF8(username));
    bson_t* update = BCON_NEW("$set", BCON_DOCUMENT(doc));

    bool ok = db_update_one("users", filter, update);

    bson_destroy(update);
    bson_destroy(filter);
    bson_destroy(doc);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int delete_user(const char* username) {
    LOG_INFO("delete_user username=%s", username);

    bson_t* filter = BCON_NEW("username", BCON_UTF8(username));
    bool ok = db_delete_one("users", filter);
    bson_destroy(filter);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

char* get_user_by_name(const char* username) {
    LOG_INFO("get_user_by_name username=%s", username);

    bson_t* filter = BCON_NEW("username", BCON_UTF8(username));
    bson_t* doc = db_find_one("users", filter);
    bson_destroy(filter);

    if (!doc) {
        LOG_ERROR("User not found: %s", username);
        return NULL;
    }

    char* json = bson_to_json(doc);
    bson_destroy(doc);
    return json;
}

char* login_user(const char* username, const char* password) {
    LOG_INFO("login_user username=%s", username);

    if (!username || !password) {
        LOG_ERROR("Missing username or password");
        return NULL;
    }

    /* Look up the user by username */
    bson_t* filter = BCON_NEW("username", BCON_UTF8(username));
    bson_t* doc = db_find_one("users", filter);
    bson_destroy(filter);

    if (!doc) {
        LOG_ERROR("User not found: %s", username);
        return NULL;
    }

    /* Compare password */
    bson_iter_t iter;
    if (bson_iter_init_find(&iter, doc, "password") && BSON_ITER_HOLDS_UTF8(&iter)) {
        const char* stored_pw = bson_iter_utf8(&iter, NULL);
        if (strcmp(stored_pw, password) == 0) {
            bson_destroy(doc);
            return strdup("{\"message\":\"User logged in successfully\"}");
        }
    }

    bson_destroy(doc);
    return NULL;
}

char* logout_user(void) {
    LOG_INFO("logout_user");
    return strdup("{\"message\":\"User logged out successfully\"}");
}
