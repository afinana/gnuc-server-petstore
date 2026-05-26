/**
 * @file test_handlers_user.c
 * @brief Unit tests for user handler functions (handlers.c).
 *
 * setUp() and tearDown() are NOT defined here; they are defined once in
 * test_main.c and shared across all suites in the single linked binary.
 *
 * Tests run without a real Redis connection — database_stubs.c provides
 * in-process fakes for all db_* calls.
 *
 * Coverage:
 *  - handle_create_user
 *  - handle_update_user
 *  - handle_delete_user
 *  - handle_get_user_by_username
 *  - handle_get_user_by_id
 *  - handle_get_all_users
 *  - handle_post_user_login
 *  - handle_post_user_logout
 */
#include "../vendor/unity/unity.h"
#include "../stubs/database_stubs.h"
#include "../../handlers.h"
#include <stdlib.h>
#include <string.h>

/* setUp() / tearDown() defined in test_main.c — shared across all suites */
void setUp(void);
void tearDown(void);

/* =========================================================================
 * handle_create_user
 * ====================================================================== */

static void test_create_user_valid_json(void) {
    const char* payload =
        "{\"id\":1,\"username\":\"alice\",\"email\":\"alice@example.com\",\"password\":\"secret\"}";
    int result = handle_create_user(payload);

    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_insert_call_count);
    TEST_ASSERT_EQUAL_STRING("users", stub_db_insert_last_collection);
}

static void test_create_user_db_failure(void) {
    stub_db_insert_ret = false;
    const char* payload = "{\"id\":2,\"username\":\"bob\"}";
    int result = handle_create_user(payload);

    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_insert_call_count);
}

static void test_create_user_invalid_json(void) {
    const char* payload = "this is not JSON";
    int result = handle_create_user(payload);

    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_INT(0, stub_db_insert_call_count);
}

static void test_create_user_null_payload(void) {
    int result = handle_create_user(NULL);

    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_INT(0, stub_db_insert_call_count);
}

static void test_create_user_empty_payload(void) {
    int result = handle_create_user("");

    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_INT(0, stub_db_insert_call_count);
}

/* =========================================================================
 * handle_update_user
 * ====================================================================== */

static void test_update_user_valid_json(void) {
    const char* payload = "{\"id\":1,\"username\":\"alice\",\"email\":\"newalice@example.com\"}";
    int result = handle_update_user(payload);

    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_update_call_count);
}

static void test_update_user_missing_id(void) {
    const char* payload = "{\"username\":\"alice\",\"email\":\"alice@example.com\"}";
    int result = handle_update_user(payload);

    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_INT(0, stub_db_update_call_count);
}

static void test_update_user_db_failure(void) {
    stub_db_update_ret = false;
    const char* payload = "{\"id\":5,\"username\":\"charlie\"}";
    int result = handle_update_user(payload);

    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_update_call_count);
}

static void test_update_user_invalid_json(void) {
    const char* payload = "{broken]";
    int result = handle_update_user(payload);

    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_INT(0, stub_db_update_call_count);
}

static void test_update_user_null_payload(void) {
    int result = handle_update_user(NULL);

    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_INT(0, stub_db_update_call_count);
}

static void test_update_user_empty_payload(void) {
    int result = handle_update_user("");

    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_INT(0, stub_db_update_call_count);
}

/* =========================================================================
 * handle_delete_user
 * ====================================================================== */

static void test_delete_user_numeric_id(void) {
    int result = handle_delete_user("10");

    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_delete_call_count);
    TEST_ASSERT_EQUAL_STRING("users", stub_db_delete_last_collection);
}

static void test_delete_user_by_username(void) {
    /* Non-numeric string → treated as username */
    int result = handle_delete_user("alice");

    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_delete_call_count);
}

static void test_delete_user_db_failure(void) {
    stub_db_delete_ret = false;
    int result = handle_delete_user("bob");

    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_delete_call_count);
}

/* =========================================================================
 * handle_get_user_by_username
 * ====================================================================== */

static void test_get_user_by_username_found(void) {
    stub_db_find_one_json =
        "{\"id\":1,\"username\":\"alice\",\"email\":\"alice@example.com\"}";

    char* result = handle_get_user_by_username("alice");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_NULL(strstr(result, "\"error\""));
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_one_call_count);

    free(result);
}

static void test_get_user_by_username_not_found(void) {
    stub_db_find_one_json = NULL;

    char* result = handle_get_user_by_username("nobody");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_NOT_NULL(strstr(result, "error"));
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_one_call_count);

    free(result);
}

/* =========================================================================
 * handle_get_user_by_id
 * ====================================================================== */

static void test_get_user_by_id_found(void) {
    stub_db_find_one_json =
        "{\"id\":4,\"username\":\"user04@gmail.com\",\"email\":\"user04.finana@gmail.com\"}";

    char* result = handle_get_user_by_id("4");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_NULL(strstr(result, "\"error\""));
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_one_call_count);

    free(result);
}

static void test_get_user_by_id_not_found(void) {
    stub_db_find_one_json = NULL;

    char* result = handle_get_user_by_id("999");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_NOT_NULL(strstr(result, "error"));
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_one_call_count);

    free(result);
}

/* =========================================================================
 * handle_get_all_users
 * ====================================================================== */

static void test_get_all_users_with_results(void) {
    stub_db_find_json = "{\"id\":1,\"username\":\"alice\"}";

    char* result = handle_get_all_users();

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_call_count);

    free(result);
}

static void test_get_all_users_empty(void) {
    stub_db_find_json = NULL;

    char* result = handle_get_all_users();

    /* Stub returns a valid (empty results) bson_t, so result is not NULL */
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_call_count);

    free(result);
}

static void test_get_all_users_db_failure(void) {
    stub_db_find_fail = true;

    char* result = handle_get_all_users();

    TEST_ASSERT_NULL(result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_call_count);
}

/* =========================================================================
 * handle_post_user_login
 * ====================================================================== */

static void test_login_valid_credentials(void) {
    /* Set up: user exists in DB with matching password */
    stub_db_find_one_json =
        "{\"username\":\"admin\",\"password\":\"admin\"}";
    const char* payload = "{\"username\":\"admin\",\"password\":\"admin\"}";
    int result = handle_post_user_login(payload);

    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_one_call_count);
}

static void test_login_invalid_password(void) {
    /* User found but password doesn't match */
    stub_db_find_one_json =
        "{\"username\":\"admin\",\"password\":\"admin\"}";
    const char* payload = "{\"username\":\"admin\",\"password\":\"wrongpassword\"}";
    int result = handle_post_user_login(payload);

    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_one_call_count);
}

static void test_login_invalid_username(void) {
    /* User not found in DB */
    stub_db_find_one_json = NULL;
    const char* payload = "{\"username\":\"hacker\",\"password\":\"admin\"}";
    int result = handle_post_user_login(payload);

    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_one_call_count);
}

static void test_login_user_missing_password_field(void) {
    /* User found but no password field stored */
    stub_db_find_one_json = "{\"username\":\"admin\"}";
    const char* payload = "{\"username\":\"admin\",\"password\":\"admin\"}";
    int result = handle_post_user_login(payload);

    TEST_ASSERT_NOT_EQUAL(0, result);
}

static void test_login_missing_username_field(void) {
    const char* payload = "{\"password\":\"admin\"}";
    int result = handle_post_user_login(payload);

    TEST_ASSERT_NOT_EQUAL(0, result);
}

static void test_login_missing_password_field(void) {
    const char* payload = "{\"username\":\"admin\"}";
    int result = handle_post_user_login(payload);

    TEST_ASSERT_NOT_EQUAL(0, result);
}

static void test_login_malformed_json(void) {
    const char* payload = "not-json-at-all";
    int result = handle_post_user_login(payload);

    TEST_ASSERT_NOT_EQUAL(0, result);
}

static void test_login_empty_json(void) {
    const char* payload = "{}";
    int result = handle_post_user_login(payload);

    TEST_ASSERT_NOT_EQUAL(0, result);
}

/* =========================================================================
 * handle_post_user_logout
 * ====================================================================== */

static void test_logout_with_username(void) {
    char* result = handle_post_user_logout("alice");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_NOT_NULL(strstr(result, "logged out"));

    free(result);
}

static void test_logout_null_username(void) {
    char* result = handle_post_user_logout(NULL);

    /* Should return an error message, not crash */
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_NOT_NULL(strstr(result, "error"));

    free(result);
}

static void test_logout_empty_username(void) {
    char* result = handle_post_user_logout("");

    /* Empty string is truthy in C (non-NULL), so handler treats as success */
    TEST_ASSERT_NOT_NULL(result);

    free(result);
}

/* =========================================================================
 * Test runner — called from test_main.c
 * ====================================================================== */

void run_user_handler_tests(void) {
    /* handle_create_user */
    RUN_TEST(test_create_user_valid_json);
    RUN_TEST(test_create_user_db_failure);
    RUN_TEST(test_create_user_invalid_json);
    RUN_TEST(test_create_user_null_payload);
    RUN_TEST(test_create_user_empty_payload);

    /* handle_update_user */
    RUN_TEST(test_update_user_valid_json);
    RUN_TEST(test_update_user_missing_id);
    RUN_TEST(test_update_user_db_failure);
    RUN_TEST(test_update_user_invalid_json);
    RUN_TEST(test_update_user_null_payload);
    RUN_TEST(test_update_user_empty_payload);

    /* handle_delete_user */
    RUN_TEST(test_delete_user_numeric_id);
    RUN_TEST(test_delete_user_by_username);
    RUN_TEST(test_delete_user_db_failure);

    /* handle_get_user_by_username */
    RUN_TEST(test_get_user_by_username_found);
    RUN_TEST(test_get_user_by_username_not_found);

    /* handle_get_user_by_id */
    RUN_TEST(test_get_user_by_id_found);
    RUN_TEST(test_get_user_by_id_not_found);

    /* handle_get_all_users */
    RUN_TEST(test_get_all_users_with_results);
    RUN_TEST(test_get_all_users_empty);
    RUN_TEST(test_get_all_users_db_failure);

    /* handle_post_user_login */
    RUN_TEST(test_login_valid_credentials);
    RUN_TEST(test_login_invalid_password);
    RUN_TEST(test_login_invalid_username);
    RUN_TEST(test_login_user_missing_password_field);
    RUN_TEST(test_login_missing_username_field);
    RUN_TEST(test_login_missing_password_field);
    RUN_TEST(test_login_malformed_json);
    RUN_TEST(test_login_empty_json);

    /* handle_post_user_logout */
    RUN_TEST(test_logout_with_username);
    RUN_TEST(test_logout_null_username);
    RUN_TEST(test_logout_empty_username);
}
