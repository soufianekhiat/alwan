/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 *
 * Main test runner - runs all unit tests consecutively
 */

#include <stdio.h>
#include <stdlib.h>

/* Forward declarations of test main functions */
extern int test_00_context_main(void);
extern int test_01_mat3_ops_main(void);
extern int test_02_data_embed_main(void);

/* Test registry */
typedef struct {
    char const *name;
    int (*test_fn)(void);
} test_suite;

static test_suite const g_test_suites[] = {
    {"00_context", test_00_context_main},
    {"01_mat3_ops", test_01_mat3_ops_main},
    {"02_data_embed", test_02_data_embed_main},
};

int main(void) {
    printf("========================================\n");
    printf("Alwan Unit Test Runner\n");
    printf("========================================\n\n");

    int total_failed = 0;
    int total_passed = 0;
    size_t const num_suites = sizeof(g_test_suites) / sizeof(g_test_suites[0]);

    for (size_t i = 0; i < num_suites; i++) {
        printf("\n");
        printf("========================================\n");
        printf("Running test suite: %s\n", g_test_suites[i].name);
        printf("========================================\n");

        int result = g_test_suites[i].test_fn();

        if (result == 0) {
            total_passed++;
            printf("[PASS] Test suite '%s' passed\n", g_test_suites[i].name);
        } else {
            total_failed++;
            printf("[FAIL] Test suite '%s' failed with code %d\n",
                   g_test_suites[i].name, result);
        }
    }

    printf("\n");
    printf("========================================\n");
    printf("Overall Results\n");
    printf("========================================\n");
    printf("Test suites passed: %d\n", total_passed);
    printf("Test suites failed: %d\n", total_failed);
    printf("Total test suites:  %zu\n", num_suites);
    printf("========================================\n");

    if (total_failed > 0) {
        printf("\nSome tests FAILED!\n");
        return 1;
    } else {
        printf("\nAll tests PASSED!\n");
        return 0;
    }
}
