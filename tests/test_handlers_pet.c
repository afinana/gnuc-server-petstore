/**
 * @file test_handlers_pet.c
 * @brief Unit tests for pet handler functions (handlers.c).
 *
 * setUp() and tearDown() are NOT defined here; they are defined once in
 * test_main.c and shared across all suites in the single linked binary.
 *
 * Tests run without a real MongoDB connection — database_stubs.c provides
 * in-process fakes for all db_* calls.
 *
 * Coverage:
 *  - handle_create_pet
 *  - handle_update_pet
 *  - handle_delete_pet
 *  - handle_get_pet_by_id
 *  - handle_get_pet_by_tags
 *  - handle_get_pet_by_state
 *  - handle_get_all_pets
 */
#include "../vendor/unity/unity.h"
#include "../stubs/database_stubs.h"
#include "../../handlers.h"
#include <stdlib.h>
#include <string.h>

/* setUp() / tearDown() defined in test_main.c — shared across all suites */
/* Declared here only to satisfy compilers that warn about implicit decls   */
void setUp(void);
void tearDown(void);

/* =========================================================================
 * handle_create_pet
 * ====================================================================== */

static void test_create_pet_valid_json(void) {
    const char* payload = "{\"id\":1,\"name\":\"Buddy\",\"status\":\"available\"}";
    int result = handle_create_pet(payload);

    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_insert_call_count);
    TEST_ASSERT_EQUAL_STRING("pets", stub_db_insert_last_collection);
}

static void test_create_pet_db_failure(void) {
    stub_db_insert_ret = false;
    const char* payload = "{\"id\":2,\"name\":\"Max\",\"status\":\"pending\"}";
    int result = handle_create_pet(payload);

    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_insert_call_count);
}

static void test_create_pet_invalid_json(void) {
    /* Malformed JSON — bson_new_from_json should fail */
    const char* payload = "not-valid-json";
    int result = handle_create_pet(payload);

    TEST_ASSERT_NOT_EQUAL(0, result);
    /* db_insert should NOT be called when JSON parse fails */
    TEST_ASSERT_EQUAL_INT(0, stub_db_insert_call_count);
}

static void test_create_pet_minimal_json(void) {
    /* Only required fields */
    const char* payload = "{\"id\":99}";
    int result = handle_create_pet(payload);

    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_insert_call_count);
}

static void test_create_pet_null_payload(void) {
    int result = handle_create_pet(NULL);

    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_INT(0, stub_db_insert_call_count);
}

static void test_create_pet_empty_payload(void) {
    int result = handle_create_pet("");

    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_INT(0, stub_db_insert_call_count);
}

/* =========================================================================
 * handle_update_pet
 * ====================================================================== */

static void test_update_pet_valid_json(void) {
    const char* payload = "{\"id\":1,\"name\":\"Buddy Updated\",\"status\":\"sold\"}";
    int result = handle_update_pet(payload);

    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_update_call_count);
}

static void test_update_pet_missing_id_field(void) {
    /* No "id" key — handler should return failure without calling db_update */
    const char* payload = "{\"name\":\"NoId\",\"status\":\"available\"}";
    int result = handle_update_pet(payload);

    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_INT(0, stub_db_update_call_count);
}

static void test_update_pet_db_failure(void) {
    stub_db_update_ret = false;
    const char* payload = "{\"id\":3,\"name\":\"Charlie\",\"status\":\"available\"}";
    int result = handle_update_pet(payload);

    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_update_call_count);
}

static void test_update_pet_invalid_json(void) {
    const char* payload = "{bad json}";
    int result = handle_update_pet(payload);

    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_INT(0, stub_db_update_call_count);
}

static void test_update_pet_null_payload(void) {
    int result = handle_update_pet(NULL);

    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_INT(0, stub_db_update_call_count);
}

static void test_update_pet_empty_payload(void) {
    int result = handle_update_pet("");

    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_INT(0, stub_db_update_call_count);
}

/* =========================================================================
 * handle_delete_pet
 * ====================================================================== */

static void test_delete_pet_numeric_id(void) {
    int result = handle_delete_pet("42");

    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_delete_call_count);
    TEST_ASSERT_EQUAL_STRING("pets", stub_db_delete_last_collection);
}

static void test_delete_pet_string_id(void) {
    int result = handle_delete_pet("fluffy-uuid-abc");

    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_delete_call_count);
}

static void test_delete_pet_db_failure(void) {
    stub_db_delete_ret = false;
    int result = handle_delete_pet("1");

    TEST_ASSERT_NOT_EQUAL(0, result);
}

/* =========================================================================
 * handle_get_pet_by_id
 * ====================================================================== */

static void test_get_pet_by_id_found(void) {
    stub_db_find_one_json = "{\"id\":1,\"name\":\"Buddy\",\"status\":\"available\"}";

    char* result = handle_get_pet_by_id("1");

    TEST_ASSERT_NOT_NULL(result);
    /* Result should contain the pet name, not an error */
    TEST_ASSERT_NULL(strstr(result, "\"error\""));
    TEST_ASSERT_NOT_NULL(strstr(result, "Buddy"));
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_one_call_count);

    free(result);
}

static void test_get_pet_by_id_not_found(void) {
    stub_db_find_one_json = NULL; /* simulates no document in DB */

    char* result = handle_get_pet_by_id("999");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_NOT_NULL(strstr(result, "error"));
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_one_call_count);

    free(result);
}

static void test_get_pet_by_id_string_id(void) {
    stub_db_find_one_json = "{\"id\":\"pet-abc\",\"name\":\"Rex\"}";

    char* result = handle_get_pet_by_id("pet-abc");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_one_call_count);

    free(result);
}

/* =========================================================================
 * handle_get_pet_by_tags
 * ====================================================================== */

static void test_get_pet_by_tags_null(void) {
    char* result = handle_get_pet_by_tags(NULL);

    /* Should return empty array, not crash */
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("[]", result);
    /* db_find should NOT be called when no tags are provided */
    TEST_ASSERT_EQUAL_INT(0, stub_db_find_call_count);

    free(result);
}

static void test_get_pet_by_tags_empty_string(void) {
    char* result = handle_get_pet_by_tags("");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("[]", result);
    TEST_ASSERT_EQUAL_INT(0, stub_db_find_call_count);

    free(result);
}

static void test_get_pet_by_tags_single_tag(void) {
    stub_db_find_json = "{\"id\":1,\"name\":\"Buddy\",\"tags\":[{\"name\":\"dog\"}]}";

    char* result = handle_get_pet_by_tags("dog");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_call_count);

    free(result);
}

static void test_get_pet_by_tags_multiple_tags(void) {
    stub_db_find_json = "{\"id\":2,\"name\":\"Max\",\"tags\":[{\"name\":\"cat\"},{\"name\":\"indoor\"}]}";

    char* result = handle_get_pet_by_tags("cat,indoor");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_call_count);

    free(result);
}

static void test_get_pet_by_tags_no_results(void) {
    stub_db_find_json = NULL; /* empty results array */

    char* result = handle_get_pet_by_tags("rare-tag");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_call_count);

    free(result);
}

/* =========================================================================
 * handle_get_pet_by_state
 * ====================================================================== */

static void test_get_pet_by_state_null(void) {
    char* result = handle_get_pet_by_state(NULL);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("[]", result);
    TEST_ASSERT_EQUAL_INT(0, stub_db_find_call_count);

    free(result);
}

static void test_get_pet_by_state_empty_string(void) {
    char* result = handle_get_pet_by_state("");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("[]", result);
    TEST_ASSERT_EQUAL_INT(0, stub_db_find_call_count);

    free(result);
}

static void test_get_pet_by_state_single_status(void) {
    stub_db_find_json = "{\"id\":1,\"name\":\"Buddy\",\"status\":\"available\"}";

    char* result = handle_get_pet_by_state("available");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_call_count);

    free(result);
}

static void test_get_pet_by_state_multiple_statuses(void) {
    stub_db_find_json = "{\"id\":2,\"name\":\"Max\",\"status\":\"sold\"}";

    char* result = handle_get_pet_by_state("available,sold,pending");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_call_count);

    free(result);
}

static void test_get_pet_by_state_no_results(void) {
    stub_db_find_json = NULL;

    char* result = handle_get_pet_by_state("unknown-status");

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_call_count);

    free(result);
}

/* =========================================================================
 * handle_get_all_pets
 * ====================================================================== */

static void test_get_all_pets_with_results(void) {
    stub_db_find_json = "{\"id\":1,\"name\":\"Buddy\"}";

    char* result = handle_get_all_pets();

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_call_count);

    free(result);
}

static void test_get_all_pets_empty(void) {
    stub_db_find_json = NULL;

    char* result = handle_get_all_pets();

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_call_count);

    free(result);
}

static void test_get_all_pets_db_failure(void) {
    /* Simulate connection failure — handle_get_all_pets should return NULL */
    stub_db_find_fail = true;

    char* result = handle_get_all_pets();

    TEST_ASSERT_NULL(result);
    TEST_ASSERT_EQUAL_INT(1, stub_db_find_call_count);
}

/* =========================================================================
 * Test runner — called from test_main.c
 * ====================================================================== */

void run_pet_handler_tests(void) {
    /* handle_create_pet */
    RUN_TEST(test_create_pet_valid_json);
    RUN_TEST(test_create_pet_db_failure);
    RUN_TEST(test_create_pet_invalid_json);
    RUN_TEST(test_create_pet_minimal_json);
    RUN_TEST(test_create_pet_null_payload);
    RUN_TEST(test_create_pet_empty_payload);

    /* handle_update_pet */
    RUN_TEST(test_update_pet_valid_json);
    RUN_TEST(test_update_pet_missing_id_field);
    RUN_TEST(test_update_pet_db_failure);
    RUN_TEST(test_update_pet_invalid_json);
    RUN_TEST(test_update_pet_null_payload);
    RUN_TEST(test_update_pet_empty_payload);

    /* handle_delete_pet */
    RUN_TEST(test_delete_pet_numeric_id);
    RUN_TEST(test_delete_pet_string_id);
    RUN_TEST(test_delete_pet_db_failure);

    /* handle_get_pet_by_id */
    RUN_TEST(test_get_pet_by_id_found);
    RUN_TEST(test_get_pet_by_id_not_found);
    RUN_TEST(test_get_pet_by_id_string_id);

    /* handle_get_pet_by_tags */
    RUN_TEST(test_get_pet_by_tags_null);
    RUN_TEST(test_get_pet_by_tags_empty_string);
    RUN_TEST(test_get_pet_by_tags_single_tag);
    RUN_TEST(test_get_pet_by_tags_multiple_tags);
    RUN_TEST(test_get_pet_by_tags_no_results);

    /* handle_get_pet_by_state */
    RUN_TEST(test_get_pet_by_state_null);
    RUN_TEST(test_get_pet_by_state_empty_string);
    RUN_TEST(test_get_pet_by_state_single_status);
    RUN_TEST(test_get_pet_by_state_multiple_statuses);
    RUN_TEST(test_get_pet_by_state_no_results);

    /* handle_get_all_pets */
    RUN_TEST(test_get_all_pets_with_results);
    RUN_TEST(test_get_all_pets_empty);
    RUN_TEST(test_get_all_pets_db_failure);
}
