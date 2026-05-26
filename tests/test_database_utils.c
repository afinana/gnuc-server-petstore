/**
 * @file test_database_utils.c
 * @brief Unit tests for database_utils.c (parse_redis_uri, extract_id_string).
 *
 * This is a SEPARATE test binary from test_runner — it links directly against
 * database_utils.c (no hiredis/Redis dependency needed) and its own main().
 *
 * Build & run via:
 *   make test-utils
 */
#include "unity.h"
#include "database_utils.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* =========================================================================
 * parse_redis_uri
 * ====================================================================== */

static void test_parse_uri_full(void) {
    char* host = NULL; int port = 0; char* password = NULL;
    parse_redis_uri("redis://:mypassword@redis.example.com:6380/", &host, &port, &password);

    TEST_ASSERT_EQUAL_STRING("redis.example.com", host);
    TEST_ASSERT_EQUAL_INT(6380, port);
    TEST_ASSERT_NOT_NULL(password);
    TEST_ASSERT_EQUAL_STRING("mypassword", password);

    free(host); free(password);
}

static void test_parse_uri_no_password(void) {
    char* host = NULL; int port = 0; char* password = NULL;
    parse_redis_uri("redis://127.0.0.1:6379/", &host, &port, &password);

    TEST_ASSERT_EQUAL_STRING("127.0.0.1", host);
    TEST_ASSERT_EQUAL_INT(6379, port);
    TEST_ASSERT_NULL(password);

    free(host);
}

static void test_parse_uri_no_port(void) {
    char* host = NULL; int port = 0; char* password = NULL;
    parse_redis_uri("redis://localhost/", &host, &port, &password);

    TEST_ASSERT_EQUAL_STRING("localhost", host);
    TEST_ASSERT_EQUAL_INT(6379, port);   /* default */
    TEST_ASSERT_NULL(password);

    free(host);
}

static void test_parse_uri_no_scheme(void) {
    char* host = NULL; int port = 0; char* password = NULL;
    /* Without the redis:// prefix — treated as host:port directly */
    parse_redis_uri("myhost:1234", &host, &port, &password);

    TEST_ASSERT_EQUAL_STRING("myhost", host);
    TEST_ASSERT_EQUAL_INT(1234, port);

    free(host);
}

static void test_parse_uri_defaults_on_empty(void) {
    char* host = NULL; int port = 0; char* password = NULL;
    parse_redis_uri("redis://", &host, &port, &password);

    /* Falls back to defaults */
    TEST_ASSERT_NOT_NULL(host);
    TEST_ASSERT_EQUAL_INT(6379, port);

    free(host);
}

static void test_parse_uri_null_uri(void) {
    char* host = NULL; int port = 0; char* password = NULL;
    parse_redis_uri(NULL, &host, &port, &password);

    /* Must not crash and must set defaults */
    TEST_ASSERT_NOT_NULL(host);
    TEST_ASSERT_EQUAL_INT(6379, port);

    free(host);
}

static void test_parse_uri_empty_password(void) {
    /* redis://:@host:6379/ — explicit empty password */
    char* host = NULL; int port = 0; char* password = NULL;
    parse_redis_uri("redis://:@myhost:6379/", &host, &port, &password);

    TEST_ASSERT_EQUAL_STRING("myhost", host);
    TEST_ASSERT_EQUAL_INT(6379, port);
    /* Empty password string — not NULL, but length 0 */
    TEST_ASSERT_NOT_NULL(password);
    TEST_ASSERT_EQUAL_INT(0, (int)strlen(password));

    free(host); free(password);
}

static void test_parse_uri_non_standard_port(void) {
    char* host = NULL; int port = 0; char* password = NULL;
    parse_redis_uri("redis://:secret@cache.internal:9999/db0", &host, &port, &password);

    TEST_ASSERT_EQUAL_STRING("cache.internal", host);
    TEST_ASSERT_EQUAL_INT(9999, port);
    TEST_ASSERT_EQUAL_STRING("secret", password);

    free(host); free(password);
}

/* =========================================================================
 * extract_id_string
 * ====================================================================== */

static void test_extract_id_int32(void) {
    bson_error_t err;
    bson_t* doc = bson_new_from_json((const uint8_t*)"{\"id\":42}", -1, &err);
    TEST_ASSERT_NOT_NULL(doc);

    char* id = extract_id_string(doc);
    TEST_ASSERT_NOT_NULL(id);
    TEST_ASSERT_EQUAL_STRING("42", id);

    free(id);
    bson_destroy(doc);
}

static void test_extract_id_int64(void) {
    /* bson_new_from_json parses integers as int32 unless $numberLong is used */
    bson_t* doc = bson_new();
    BSON_APPEND_INT64(doc, "id", 9999999999LL);

    char* id = extract_id_string(doc);
    TEST_ASSERT_NOT_NULL(id);
    TEST_ASSERT_EQUAL_STRING("9999999999", id);

    free(id);
    bson_destroy(doc);
}

static void test_extract_id_string_type(void) {
    bson_error_t err;
    bson_t* doc = bson_new_from_json((const uint8_t*)"{\"id\":\"pet-uuid-abc\"}", -1, &err);
    TEST_ASSERT_NOT_NULL(doc);

    char* id = extract_id_string(doc);
    TEST_ASSERT_NOT_NULL(id);
    TEST_ASSERT_EQUAL_STRING("pet-uuid-abc", id);

    free(id);
    bson_destroy(doc);
}

static void test_extract_id_missing_field(void) {
    bson_error_t err;
    bson_t* doc = bson_new_from_json((const uint8_t*)"{\"name\":\"Buddy\"}", -1, &err);
    TEST_ASSERT_NOT_NULL(doc);

    char* id = extract_id_string(doc);
    TEST_ASSERT_NULL(id);   /* No "id" field → must return NULL */

    bson_destroy(doc);
}

static void test_extract_id_zero(void) {
    bson_t* doc = bson_new();
    BSON_APPEND_INT32(doc, "id", 0);

    char* id = extract_id_string(doc);
    TEST_ASSERT_NOT_NULL(id);
    TEST_ASSERT_EQUAL_STRING("0", id);

    free(id);
    bson_destroy(doc);
}

static void test_extract_id_negative(void) {
    bson_t* doc = bson_new();
    BSON_APPEND_INT32(doc, "id", -1);

    char* id = extract_id_string(doc);
    TEST_ASSERT_NOT_NULL(id);
    TEST_ASSERT_EQUAL_STRING("-1", id);

    free(id);
    bson_destroy(doc);
}

/* =========================================================================
 * main
 * ====================================================================== */

int main(void) {
    UNITY_BEGIN();

    /* parse_redis_uri */
    RUN_TEST(test_parse_uri_full);
    RUN_TEST(test_parse_uri_no_password);
    RUN_TEST(test_parse_uri_no_port);
    RUN_TEST(test_parse_uri_no_scheme);
    RUN_TEST(test_parse_uri_defaults_on_empty);
    RUN_TEST(test_parse_uri_null_uri);
    RUN_TEST(test_parse_uri_empty_password);
    RUN_TEST(test_parse_uri_non_standard_port);

    /* extract_id_string */
    RUN_TEST(test_extract_id_int32);
    RUN_TEST(test_extract_id_int64);
    RUN_TEST(test_extract_id_string_type);
    RUN_TEST(test_extract_id_missing_field);
    RUN_TEST(test_extract_id_zero);
    RUN_TEST(test_extract_id_negative);

    return UNITY_END();
}
