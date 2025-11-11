/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 *
 * Test 00: Context creation/destruction and allocation hooks
 */

#include "../../src/alwan/alwan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Custom allocator tracking
 * ---------------------------------------------------------------- */

static size_t g_alloc_count = 0;
static size_t g_free_count = 0;
static size_t g_bytes_allocated = 0;

static void *test_alloc(size_t size, size_t align) {
    (void)align;
    g_alloc_count++;
    g_bytes_allocated += size;
    return malloc(size);
}

static void test_free(void *ptr) {
    if (ptr) {
        g_free_count++;
        free(ptr);
    }
}

static void reset_counters(void) {
    g_alloc_count = 0;
    g_free_count = 0;
    g_bytes_allocated = 0;
}

/* ----------------------------------------------------------------
 * Test helpers
 * ---------------------------------------------------------------- */

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } \
} while(0)

#define TEST_PASS(name) do { \
    printf("[PASS] %s\n", name); \
    return 0; \
} while(0)

/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */

static int test_create_destroy_default(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context with default config");

    alwan_destroy(ctx);
    TEST_PASS("test_create_destroy_default");
}

static int test_create_destroy_custom_alloc(void) {
    reset_counters();

    alwan_config cfg = {0};
    cfg.alloc_cb = test_alloc;
    cfg.free_cb = test_free;

    alwan_ctx *ctx = alwan_create(&cfg);
    TEST_ASSERT(ctx != NULL, "Failed to create context with custom allocator");
    TEST_ASSERT(g_alloc_count > 0, "Custom allocator was not called");

    size_t allocs_before_destroy = g_alloc_count;
    alwan_destroy(ctx);

    TEST_ASSERT(g_free_count > 0, "Custom free was not called");
    TEST_ASSERT(g_free_count == allocs_before_destroy, "Allocation/free count mismatch");

    TEST_PASS("test_create_destroy_custom_alloc");
}

static int test_create_with_data_root(void) {
    reset_counters();

    alwan_config cfg = {0};
    cfg.alloc_cb = test_alloc;
    cfg.free_cb = test_free;
    cfg.runtime_data_root = "c:/git/alwan/data";

    alwan_ctx *ctx = alwan_create(&cfg);
    TEST_ASSERT(ctx != NULL, "Failed to create context with data root");

    /* Should allocate for context + data root string */
    TEST_ASSERT(g_alloc_count >= 2, "Expected at least 2 allocations (ctx + data_root)");

    alwan_destroy(ctx);
    TEST_PASS("test_create_with_data_root");
}

static int test_destroy_null(void) {
    /* Should not crash */
    alwan_destroy(NULL);
    TEST_PASS("test_destroy_null");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

typedef int (*test_fn)(void);

typedef struct {
    char const *name;
    test_fn fn;
} test_entry;

static test_entry const tests[] = {
    {"create_destroy_default", test_create_destroy_default},
    {"create_destroy_custom_alloc", test_create_destroy_custom_alloc},
    {"create_with_data_root", test_create_with_data_root},
    {"destroy_null", test_destroy_null},
};

int test_00_context_main(void) {
    printf("Running context tests...\n");

    int failed = 0;
    int passed = 0;
    size_t const num_tests = sizeof(tests) / sizeof(tests[0]);

    for (size_t i = 0; i < num_tests; i++) {
        int result = tests[i].fn();
        if (result == 0) {
            passed++;
        } else {
            failed++;
            fprintf(stderr, "[FAIL] Test '%s' failed\n", tests[i].name);
        }
    }

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed (out of %zu)\n", passed, failed, num_tests);
    printf("========================================\n");

    return (failed > 0) ? 1 : 0;
}
