/**
 * @file test_main.c
 * @brief Aggregated test runner for all Petstore API unit tests.
 *
 * setUp() is called by Unity before each RUN_TEST() invocation and resets
 * all database stubs so each test starts from a clean state.
 * tearDown() is called after each test (currently a no-op).
 *
 * Build & run via:
 *   make test
 */
#include "vendor/unity/unity.h"
#include "stubs/database_stubs.h"

/* Forward declarations — each suite exposes its run_*_tests() function */
void run_pet_handler_tests(void);
void run_user_handler_tests(void);

/*
 * setUp / tearDown — defined ONCE here and shared by all suites linked into
 * this single binary.  Suite files declare (but do not define) them.
 */
void setUp(void) {
    stub_db_reset();
}

void tearDown(void) {
    /* nothing — stubs hold no persistent resources */
}

int main(void) {
    UNITY_BEGIN();

    run_pet_handler_tests();
    run_user_handler_tests();

    return UNITY_END();
}
