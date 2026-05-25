#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <hiredis/hiredis.h>
#include "database.h"
#include "log-utils.h"

/**
 * @brief Maximum number of connections in the Redis connection pool.
 */
#define POOL_SIZE 10

/* -------------------------------------------------------------------------
 * Thread-safe connection pool
 * ---------------------------------------------------------------------- */

/** Simple connection pool: array of redisContext pointers guarded by a mutex. */
static redisContext* pool_conns[POOL_SIZE];
static int pool_count = 0;
static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pool_cond = PTHREAD_COND_INITIALIZER;

/** Connection parameters cached for reconnection */
static char* cached_host = NULL;
static int cached_port = 0;
static char* cached_password = NULL;

/**
 * @brief Creates a new Redis connection using the cached parameters.
 * @return redisContext* A new connection or NULL on failure.
 */
static redisContext* create_connection(void) {
    struct timeval timeout = { 5, 0 }; /* 5 seconds */
    redisContext* ctx = redisConnectWithTimeout(cached_host, cached_port, timeout);
    if (ctx == NULL || ctx->err) {
        if (ctx) {
            LOG_ERROR("Redis connection error: %s", ctx->errstr);
            redisFree(ctx);
        } else {
            LOG_ERROR("Redis connection error: cannot allocate context");
        }
        return NULL;
    }

    /* Authenticate if a password was provided */
    if (cached_password && cached_password[0] != '\0') {
        redisReply* reply = redisCommand(ctx, "AUTH %s", cached_password);
        if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
            LOG_ERROR("Redis AUTH failed: %s",
                      reply ? reply->str : "no reply");
            if (reply) freeReplyObject(reply);
            redisFree(ctx);
            return NULL;
        }
        freeReplyObject(reply);
    }
    return ctx;
}

/**
 * @brief Acquires a Redis connection from the pool (blocking if empty).
 * @return redisContext* A pooled connection.
 */
static redisContext* pool_pop(void) {
    pthread_mutex_lock(&pool_mutex);
    while (pool_count == 0) {
        pthread_cond_wait(&pool_cond, &pool_mutex);
    }
    redisContext* ctx = pool_conns[--pool_count];
    pthread_mutex_unlock(&pool_mutex);

    /* Verify the connection is still alive */
    redisReply* reply = redisCommand(ctx, "PING");
    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        LOG_WARN("Stale pool connection, reconnecting...");
        if (reply) freeReplyObject(reply);
        redisFree(ctx);
        ctx = create_connection();
    } else {
        freeReplyObject(reply);
    }
    return ctx;
}

/**
 * @brief Returns a Redis connection to the pool.
 * @param ctx The connection to return.
 */
static void pool_push(redisContext* ctx) {
    if (ctx == NULL) return;
    pthread_mutex_lock(&pool_mutex);
    if (pool_count < POOL_SIZE) {
        pool_conns[pool_count++] = ctx;
        pthread_cond_signal(&pool_cond);
    } else {
        /* Pool is full — discard this connection */
        redisFree(ctx);
    }
    pthread_mutex_unlock(&pool_mutex);
}

/* -------------------------------------------------------------------------
 * URI parsing helpers
 * ---------------------------------------------------------------------- */

/**
 * @brief Parses a Redis URI string of the form redis://[:password@]host:port/
 *
 * Extracts the host, port and optional password components. Falls back to
 * defaults of 127.0.0.1:6379 if parsing fails.
 *
 * @param uri    The Redis URI string.
 * @param host   Output: heap-allocated host string (caller must free).
 * @param port   Output: port number.
 * @param password Output: heap-allocated password string or NULL.
 */
static void parse_redis_uri(const char* uri, char** host, int* port, char** password) {
    *host = strdup("127.0.0.1");
    *port = 6379;
    *password = NULL;

    /* Skip the "redis://" scheme prefix */
    const char* p = uri;
    if (strncmp(p, "redis://", 8) == 0) {
        p += 8;
    }

    /* Check for :password@ pattern */
    const char* at = strchr(p, '@');
    if (at != NULL) {
        /* Password sits between ":" and "@" */
        if (p[0] == ':') {
            size_t pw_len = (size_t)(at - p - 1);
            *password = strndup(p + 1, pw_len);
        }
        p = at + 1;
    }

    /* Remaining: host:port[/...] */
    const char* colon = strchr(p, ':');
    const char* slash = strchr(p, '/');

    if (colon != NULL) {
        free(*host);
        *host = strndup(p, (size_t)(colon - p));
        *port = atoi(colon + 1);
    } else if (slash != NULL) {
        free(*host);
        *host = strndup(p, (size_t)(slash - p));
    } else if (*p != '\0') {
        free(*host);
        *host = strdup(p);
    }
}

/* -------------------------------------------------------------------------
 * Helper: extract ID from a BSON document
 * ---------------------------------------------------------------------- */

/**
 * @brief Extracts the "id" field from a BSON document as a string.
 *
 * Handles both integer and string id types. The caller must free the result.
 *
 * @param doc The BSON document.
 * @return char* The ID as a string, or NULL if not found.
 */
static char* extract_id_string(const bson_t* doc) {
    bson_iter_t iter;
    if (!bson_iter_init_find(&iter, doc, "id")) {
        return NULL;
    }
    if (BSON_ITER_HOLDS_INT64(&iter)) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lld", (long long)bson_iter_int64(&iter));
        return strdup(buf);
    }
    if (BSON_ITER_HOLDS_INT32(&iter)) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", bson_iter_int32(&iter));
        return strdup(buf);
    }
    if (BSON_ITER_HOLDS_UTF8(&iter)) {
        uint32_t len;
        const char* val = bson_iter_utf8(&iter, &len);
        return strndup(val, len);
    }
    return NULL;
}

/* -------------------------------------------------------------------------
 * Secondary index helpers
 *
 * These maintain Redis Sets that map field values to entity IDs, enabling
 * efficient lookups like "find all pets with status=available".
 * ---------------------------------------------------------------------- */

/**
 * @brief Adds secondary index entries for a document.
 *
 * For the "pets" collection: indexes by "status" and each tag in "tags".
 * For the "users" collection: indexes by "username".
 *
 * @param ctx             The Redis connection.
 * @param collection_name The collection name (key prefix).
 * @param id_str          The entity ID string.
 * @param doc             The BSON document containing the fields to index.
 */
static void add_secondary_indexes(redisContext* ctx, const char* collection_name,
                                  const char* id_str, const bson_t* doc) {
    bson_iter_t iter;

    /* Index by "status" field (pets) */
    if (bson_iter_init_find(&iter, doc, "status") &&
        BSON_ITER_HOLDS_UTF8(&iter)) {
        const char* status = bson_iter_utf8(&iter, NULL);
        redisReply* r = redisCommand(ctx, "SADD %s:status:%s %s",
                                     collection_name, status, id_str);
        if (r) freeReplyObject(r);
    }

    /* Index by "username" field (users) — stores username→id mapping */
    if (bson_iter_init_find(&iter, doc, "username") &&
        BSON_ITER_HOLDS_UTF8(&iter)) {
        const char* username = bson_iter_utf8(&iter, NULL);
        redisReply* r = redisCommand(ctx, "SET %s:username:%s %s",
                                     collection_name, username, id_str);
        if (r) freeReplyObject(r);
    }

    /* Index by "tags" array (pets) — each tag's "name" is indexed */
    if (bson_iter_init_find(&iter, doc, "tags") &&
        BSON_ITER_HOLDS_ARRAY(&iter)) {
        bson_iter_t child;
        bson_iter_recurse(&iter, &child);
        while (bson_iter_next(&child)) {
            if (BSON_ITER_HOLDS_DOCUMENT(&child)) {
                bson_iter_t tag_iter;
                bson_iter_recurse(&child, &tag_iter);
                if (bson_iter_find(&tag_iter, "name") &&
                    BSON_ITER_HOLDS_UTF8(&tag_iter)) {
                    const char* tag_name = bson_iter_utf8(&tag_iter, NULL);
                    redisReply* r = redisCommand(ctx, "SADD %s:tag:%s %s",
                                                 collection_name, tag_name, id_str);
                    if (r) freeReplyObject(r);
                }
            }
        }
    }
}

/**
 * @brief Removes secondary index entries for a document.
 *
 * Reads the existing document from Redis, parses it, and removes the
 * entity ID from all relevant index sets.
 *
 * @param ctx             The Redis connection.
 * @param collection_name The collection name (key prefix).
 * @param id_str          The entity ID string.
 */
static void remove_secondary_indexes(redisContext* ctx, const char* collection_name,
                                     const char* id_str) {
    /* Fetch existing doc to know which indexes to clean */
    char key[256];
    snprintf(key, sizeof(key), "%s:%s", collection_name, id_str);

    redisReply* reply = redisCommand(ctx, "HGET %s data", key);
    if (reply == NULL || reply->type != REDIS_REPLY_STRING) {
        if (reply) freeReplyObject(reply);
        return;
    }

    bson_error_t err;
    bson_t* doc = bson_new_from_json((const uint8_t*)reply->str, -1, &err);
    freeReplyObject(reply);
    if (!doc) return;

    bson_iter_t iter;

    /* Remove status index */
    if (bson_iter_init_find(&iter, doc, "status") &&
        BSON_ITER_HOLDS_UTF8(&iter)) {
        const char* status = bson_iter_utf8(&iter, NULL);
        redisReply* r = redisCommand(ctx, "SREM %s:status:%s %s",
                                     collection_name, status, id_str);
        if (r) freeReplyObject(r);
    }

    /* Remove username index */
    if (bson_iter_init_find(&iter, doc, "username") &&
        BSON_ITER_HOLDS_UTF8(&iter)) {
        const char* username = bson_iter_utf8(&iter, NULL);
        redisReply* r = redisCommand(ctx, "DEL %s:username:%s",
                                     collection_name, username);
        if (r) freeReplyObject(r);
    }

    /* Remove tag indexes */
    if (bson_iter_init_find(&iter, doc, "tags") &&
        BSON_ITER_HOLDS_ARRAY(&iter)) {
        bson_iter_t child;
        bson_iter_recurse(&iter, &child);
        while (bson_iter_next(&child)) {
            if (BSON_ITER_HOLDS_DOCUMENT(&child)) {
                bson_iter_t tag_iter;
                bson_iter_recurse(&child, &tag_iter);
                if (bson_iter_find(&tag_iter, "name") &&
                    BSON_ITER_HOLDS_UTF8(&tag_iter)) {
                    const char* tag_name = bson_iter_utf8(&tag_iter, NULL);
                    redisReply* r = redisCommand(ctx, "SREM %s:tag:%s %s",
                                                 collection_name, tag_name, id_str);
                    if (r) freeReplyObject(r);
                }
            }
        }
    }

    bson_destroy(doc);
}

/* -------------------------------------------------------------------------
 * Public API implementation
 * ---------------------------------------------------------------------- */

/**
 * @brief Initializes the Redis connection pool.
 *
 * Parses the URI, creates POOL_SIZE connections, and stores them in the pool.
 *
 * @param uri Redis URI (redis://[:password@]host:port/).
 */
void db_init(const char* uri) {
    parse_redis_uri(uri, &cached_host, &cached_port, &cached_password);

    LOG_INFO("Connecting to Redis at %s:%d", cached_host, cached_port);

    for (int i = 0; i < POOL_SIZE; i++) {
        redisContext* ctx = create_connection();
        if (ctx == NULL) {
            LOG_ERROR("Failed to create Redis connection %d/%d", i + 1, POOL_SIZE);
            exit(EXIT_FAILURE);
        }
        pool_conns[pool_count++] = ctx;
    }

    LOG_INFO("Redis connection pool initialized (%d connections)", POOL_SIZE);
}

/**
 * @brief Cleans up all Redis connections in the pool and frees cached params.
 */
void db_cleanup(void) {
    pthread_mutex_lock(&pool_mutex);
    for (int i = 0; i < pool_count; i++) {
        if (pool_conns[i]) {
            redisFree(pool_conns[i]);
            pool_conns[i] = NULL;
        }
    }
    pool_count = 0;
    pthread_mutex_unlock(&pool_mutex);

    free(cached_host);
    free(cached_password);
    cached_host = NULL;
    cached_password = NULL;

    LOG_INFO("Redis connection pool destroyed");
}

/**
 * @brief Inserts a BSON document into Redis.
 *
 * The document is stored as a JSON string in a Redis Hash at key
 * {collection_name}:{id}, with the hash field "data". Secondary indexes
 * are maintained for "status", "username", and "tags" fields.
 *
 * @param collection_name The collection name used as key prefix.
 * @param doc The BSON document to insert.
 * @return bool true on success, false on failure.
 */
bool db_insert(const char* collection_name, const bson_t* doc) {
    char* id_str = extract_id_string(doc);
    if (!id_str) {
        LOG_ERROR("Insert failed: document has no 'id' field");
        return false;
    }

    /* Convert BSON to JSON for storage */
    char* json = bson_as_relaxed_extended_json(doc, NULL);
    if (!json) {
        LOG_ERROR("Insert failed: BSON-to-JSON conversion error");
        free(id_str);
        return false;
    }

    char key[256];
    snprintf(key, sizeof(key), "%s:%s", collection_name, id_str);

    redisContext* ctx = pool_pop();
    if (!ctx) {
        LOG_ERROR("Insert failed: no Redis connection available");
        bson_free(json);
        free(id_str);
        return false;
    }

    /* Store the document as a hash field */
    redisReply* reply = redisCommand(ctx, "HSET %s data %s", key, json);
    bool success = (reply != NULL && reply->type != REDIS_REPLY_ERROR);
    if (!success) {
        LOG_ERROR("Insert HSET failed: %s",
                  reply ? reply->str : "no reply");
    }
    if (reply) freeReplyObject(reply);

    /* Maintain secondary indexes */
    if (success) {
        add_secondary_indexes(ctx, collection_name, id_str, doc);
    }

    pool_push(ctx);
    bson_free(json);
    free(id_str);
    return success;
}

/**
 * @brief Updates a document in Redis.
 *
 * Extracts the "id" from the query, removes old indexes, stores the
 * updated document, and rebuilds indexes.
 *
 * @param collection_name The collection name (key prefix).
 * @param query BSON query containing the "id" to match.
 * @param update BSON update document (expects "$set" wrapper).
 * @return bool true on success, false on failure.
 */
bool db_update(const char* collection_name, const bson_t* query, const bson_t* update) {
    char* id_str = extract_id_string(query);
    if (!id_str) {
        LOG_ERROR("Update failed: query has no 'id' field");
        return false;
    }

    /* Extract the $set sub-document to get the actual data */
    bson_iter_t iter;
    const bson_t* set_doc = NULL;
    bson_t set_doc_val;
    if (bson_iter_init_find(&iter, update, "$set") &&
        BSON_ITER_HOLDS_DOCUMENT(&iter)) {
        const uint8_t* data;
        uint32_t len;
        bson_iter_document(&iter, &len, &data);
        if (bson_init_static(&set_doc_val, data, len)) {
            set_doc = &set_doc_val;
        }
    }

    if (!set_doc) {
        /* Fall back to using the update document directly */
        set_doc = update;
    }

    char* json = bson_as_relaxed_extended_json(set_doc, NULL);
    if (!json) {
        LOG_ERROR("Update failed: BSON-to-JSON conversion error");
        free(id_str);
        return false;
    }

    char key[256];
    snprintf(key, sizeof(key), "%s:%s", collection_name, id_str);

    redisContext* ctx = pool_pop();
    if (!ctx) {
        LOG_ERROR("Update failed: no Redis connection available");
        bson_free(json);
        free(id_str);
        return false;
    }

    /* Remove old secondary indexes before overwriting */
    remove_secondary_indexes(ctx, collection_name, id_str);

    /* Overwrite the document */
    redisReply* reply = redisCommand(ctx, "HSET %s data %s", key, json);
    bool success = (reply != NULL && reply->type != REDIS_REPLY_ERROR);
    if (!success) {
        LOG_ERROR("Update HSET failed: %s",
                  reply ? reply->str : "no reply");
    }
    if (reply) freeReplyObject(reply);

    /* Rebuild secondary indexes with the new data */
    if (success) {
        add_secondary_indexes(ctx, collection_name, id_str, set_doc);
    }

    pool_push(ctx);
    bson_free(json);
    free(id_str);
    return success;
}

/**
 * @brief Deletes a document from Redis.
 *
 * Removes both the Hash key and all secondary index entries.
 *
 * @param collection_name The collection name (key prefix).
 * @param query BSON query identifying the document (by "id" or "username").
 * @return bool true on success, false on failure.
 */
bool db_delete(const char* collection_name, const bson_t* query) {
    /* Try to resolve the ID — either from "id" field or "username" index */
    char* id_str = extract_id_string(query);

    redisContext* ctx = pool_pop();
    if (!ctx) {
        LOG_ERROR("Delete failed: no Redis connection available");
        free(id_str);
        return false;
    }

    /* If no "id" field, check for "username" lookup */
    if (!id_str) {
        bson_iter_t iter;
        if (bson_iter_init_find(&iter, query, "username") &&
            BSON_ITER_HOLDS_UTF8(&iter)) {
            const char* username = bson_iter_utf8(&iter, NULL);
            redisReply* r = redisCommand(ctx, "GET %s:username:%s",
                                         collection_name, username);
            if (r && r->type == REDIS_REPLY_STRING) {
                id_str = strdup(r->str);
            }
            if (r) freeReplyObject(r);
        }
    }

    if (!id_str) {
        LOG_ERROR("Delete failed: cannot resolve document ID");
        pool_push(ctx);
        return false;
    }

    /* Remove secondary indexes first (needs the data still present) */
    remove_secondary_indexes(ctx, collection_name, id_str);

    /* Delete the main key */
    char key[256];
    snprintf(key, sizeof(key), "%s:%s", collection_name, id_str);
    redisReply* reply = redisCommand(ctx, "DEL %s", key);
    bool success = (reply != NULL && reply->type != REDIS_REPLY_ERROR);
    if (!success) {
        LOG_ERROR("Delete DEL failed: %s",
                  reply ? reply->str : "no reply");
    }
    if (reply) freeReplyObject(reply);

    pool_push(ctx);
    free(id_str);
    return success;
}

/**
 * @brief Finds a single document by query.
 *
 * Supports lookup by "id" (direct key access), "username" (via secondary
 * index), or "status" (returns first match from the index set).
 *
 * @param collection_name The collection name (key prefix).
 * @param query BSON query document.
 * @return bson_t* The matching document, or NULL if not found.
 */
bson_t* db_find_one(const char* collection_name, const bson_t* query) {
    redisContext* ctx = pool_pop();
    if (!ctx) {
        LOG_ERROR("find_one failed: no Redis connection available");
        return NULL;
    }

    char* id_str = NULL;
    bson_iter_t iter;

    /* Try direct "id" lookup first */
    id_str = extract_id_string(query);

    /* If no "id", try "username" secondary index */
    if (!id_str && bson_iter_init_find(&iter, query, "username") &&
        BSON_ITER_HOLDS_UTF8(&iter)) {
        const char* username = bson_iter_utf8(&iter, NULL);
        redisReply* r = redisCommand(ctx, "GET %s:username:%s",
                                     collection_name, username);
        if (r && r->type == REDIS_REPLY_STRING) {
            id_str = strdup(r->str);
        }
        if (r) freeReplyObject(r);
    }

    /* If no "id" and no "username", try "status" — return first match */
    if (!id_str && bson_iter_init_find(&iter, query, "status") &&
        BSON_ITER_HOLDS_UTF8(&iter)) {
        const char* status = bson_iter_utf8(&iter, NULL);
        redisReply* r = redisCommand(ctx, "SRANDMEMBER %s:status:%s",
                                     collection_name, status);
        if (r && r->type == REDIS_REPLY_STRING) {
            id_str = strdup(r->str);
        }
        if (r) freeReplyObject(r);
    }

    if (!id_str) {
        pool_push(ctx);
        return NULL;
    }

    /* Fetch the document */
    char key[256];
    snprintf(key, sizeof(key), "%s:%s", collection_name, id_str);
    redisReply* reply = redisCommand(ctx, "HGET %s data", key);

    bson_t* result = NULL;
    if (reply && reply->type == REDIS_REPLY_STRING) {
        bson_error_t err;
        result = bson_new_from_json((const uint8_t*)reply->str, -1, &err);
        if (!result) {
            LOG_ERROR("find_one: JSON parse error: %s", err.message);
        }
    }
    if (reply) freeReplyObject(reply);

    pool_push(ctx);
    free(id_str);
    return result;
}

/**
 * @brief Finds all documents matching a query.
 *
 * Supports queries with "status" and "tags.name" fields (using "$in"
 * operator). For each matching ID found via the index set, the full
 * document is fetched and added to the result array.
 *
 * @param collection_name The collection name (key prefix).
 * @param query BSON query document.
 * @return bson_t* A BSON document with a "results" array, or NULL on error.
 */
bson_t* db_find(const char* collection_name, const bson_t* query) {
    redisContext* ctx = pool_pop();
    if (!ctx) {
        LOG_ERROR("find failed: no Redis connection available");
        return NULL;
    }

    bson_t* result = bson_new();
    bson_t child;
    BSON_APPEND_ARRAY_BEGIN(result, "results", &child);

    int index = 0;
    char idx_key[16];
    bson_iter_t iter;

    /* Check for status query: { "status": { "$in": ["available", ...] } } */
    if (bson_iter_init_find(&iter, query, "status") &&
        BSON_ITER_HOLDS_DOCUMENT(&iter)) {
        bson_iter_t in_iter;
        bson_iter_recurse(&iter, &in_iter);
        if (bson_iter_find(&in_iter, "$in") && BSON_ITER_HOLDS_ARRAY(&in_iter)) {
            bson_iter_t arr_iter;
            bson_iter_recurse(&in_iter, &arr_iter);
            while (bson_iter_next(&arr_iter)) {
                if (BSON_ITER_HOLDS_UTF8(&arr_iter)) {
                    const char* status = bson_iter_utf8(&arr_iter, NULL);
                    /* Get all IDs with this status */
                    redisReply* members = redisCommand(ctx, "SMEMBERS %s:status:%s",
                                                       collection_name, status);
                    if (members && members->type == REDIS_REPLY_ARRAY) {
                        for (size_t i = 0; i < members->elements; i++) {
                            char doc_key[256];
                            snprintf(doc_key, sizeof(doc_key), "%s:%s",
                                     collection_name, members->element[i]->str);
                            redisReply* doc_reply = redisCommand(ctx, "HGET %s data", doc_key);
                            if (doc_reply && doc_reply->type == REDIS_REPLY_STRING) {
                                bson_error_t err;
                                bson_t* doc = bson_new_from_json(
                                    (const uint8_t*)doc_reply->str, -1, &err);
                                if (doc) {
                                    snprintf(idx_key, sizeof(idx_key), "%d", index++);
                                    BSON_APPEND_DOCUMENT(&child, idx_key, doc);
                                    bson_destroy(doc);
                                }
                            }
                            if (doc_reply) freeReplyObject(doc_reply);
                        }
                    }
                    if (members) freeReplyObject(members);
                }
            }
        }
    }

    /* Check for tags query: { "tags.name": { "$in": ["tag1", ...] } } */
    if (bson_iter_init_find(&iter, query, "tags.name") &&
        BSON_ITER_HOLDS_DOCUMENT(&iter)) {
        bson_iter_t in_iter;
        bson_iter_recurse(&iter, &in_iter);
        if (bson_iter_find(&in_iter, "$in") && BSON_ITER_HOLDS_ARRAY(&in_iter)) {
            bson_iter_t arr_iter;
            bson_iter_recurse(&in_iter, &arr_iter);
            while (bson_iter_next(&arr_iter)) {
                if (BSON_ITER_HOLDS_UTF8(&arr_iter)) {
                    const char* tag = bson_iter_utf8(&arr_iter, NULL);
                    redisReply* members = redisCommand(ctx, "SMEMBERS %s:tag:%s",
                                                       collection_name, tag);
                    if (members && members->type == REDIS_REPLY_ARRAY) {
                        for (size_t i = 0; i < members->elements; i++) {
                            char doc_key[256];
                            snprintf(doc_key, sizeof(doc_key), "%s:%s",
                                     collection_name, members->element[i]->str);
                            redisReply* doc_reply = redisCommand(ctx, "HGET %s data", doc_key);
                            if (doc_reply && doc_reply->type == REDIS_REPLY_STRING) {
                                bson_error_t err;
                                bson_t* doc = bson_new_from_json(
                                    (const uint8_t*)doc_reply->str, -1, &err);
                                if (doc) {
                                    snprintf(idx_key, sizeof(idx_key), "%d", index++);
                                    BSON_APPEND_DOCUMENT(&child, idx_key, doc);
                                    bson_destroy(doc);
                                }
                            }
                            if (doc_reply) freeReplyObject(doc_reply);
                        }
                    }
                    if (members) freeReplyObject(members);
                }
            }
        }
    }

    bson_append_array_end(result, &child);

    pool_push(ctx);
    return result;
}

/**
 * @brief Finds all documents in the specified collection.
 *
 * Uses Redis SCAN to iterate over all keys matching the pattern
 * {collection_name}:* (excluding index keys that contain ":status:",
 * ":username:", or ":tag:").
 *
 * @param collection_name The collection name (key prefix).
 * @return bson_t* A BSON document with a "results" array.
 */
bson_t* db_find_all(const char* collection_name) {
    redisContext* ctx = pool_pop();
    if (!ctx) {
        LOG_ERROR("find_all failed: no Redis connection available");
        return NULL;
    }

    bson_t* result = bson_new();
    bson_t child;
    BSON_APPEND_ARRAY_BEGIN(result, "results", &child);

    int index = 0;
    char idx_key[16];
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "%s:*", collection_name);

    /* Use SCAN to iterate without blocking the server */
    long long cursor = 0;
    do {
        redisReply* reply = redisCommand(ctx, "SCAN %lld MATCH %s COUNT 100",
                                          cursor, pattern);
        if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements != 2) {
            if (reply) freeReplyObject(reply);
            break;
        }

        cursor = atoll(reply->element[0]->str);
        redisReply* keys = reply->element[1];

        for (size_t i = 0; i < keys->elements; i++) {
            const char* key = keys->element[i]->str;

            /* Skip secondary index keys */
            if (strstr(key, ":status:") || strstr(key, ":username:") ||
                strstr(key, ":tag:")) {
                continue;
            }

            /* Fetch the document */
            redisReply* doc_reply = redisCommand(ctx, "HGET %s data", key);
            if (doc_reply && doc_reply->type == REDIS_REPLY_STRING) {
                bson_error_t err;
                bson_t* doc = bson_new_from_json(
                    (const uint8_t*)doc_reply->str, -1, &err);
                if (doc) {
                    snprintf(idx_key, sizeof(idx_key), "%d", index++);
                    BSON_APPEND_DOCUMENT(&child, idx_key, doc);
                    bson_destroy(doc);
                }
            }
            if (doc_reply) freeReplyObject(doc_reply);
        }

        freeReplyObject(reply);
    } while (cursor != 0);

    bson_append_array_end(result, &child);

    pool_push(ctx);
    return result;
}