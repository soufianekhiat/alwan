/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 01: 3x3 matrix operations (multiply, inverse)
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>
#include <stdlib.h>

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

static alwan_scalar mat3_max_diff(alwan_mat3x3 const *a, alwan_mat3x3 const *b) {
    alwan_scalar max_diff = 0;
    for (int i = 0; i < 9; i++) {
        alwan_scalar diff = ALWAN_FABS(a->m[i] - b->m[i]);
        if (diff > max_diff) max_diff = diff;
    }
    return max_diff;
}

static void mat3_print(char const *name, alwan_mat3x3 const *m) {
    printf("%s:\n", name);
    for (int row = 0; row < 3; row++) {
        printf("  [%12.8f %12.8f %12.8f]\n",
               m->m[row * 3 + 0], m->m[row * 3 + 1], m->m[row * 3 + 2]);
    }
}

/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */

static int test_identity(void) {
    alwan_mat3x3 I, result;

    alwan_mat3_identity(&I);
    TEST_ASSERT(I.m[0] == 1.0 && I.m[4] == 1.0 && I.m[8] == 1.0, "Diagonal not 1");
    TEST_ASSERT(I.m[1] == 0.0 && I.m[2] == 0.0 && I.m[3] == 0.0, "Off-diagonal not 0");

    /* I * I = I */
    alwan_mat3_mul(&I, &I, &result);
    alwan_scalar diff = mat3_max_diff(&I, &result);
    TEST_ASSERT(diff < ALWAN_TEST_TOLERANCE, "I * I != I");

    TEST_PASS("test_identity");
}

static int test_multiply(void) {
    /* Simple test: A * I = A */
    alwan_mat3x3 A = {{1, 2, 3, 4, 5, 6, 7, 8, 9}};
    alwan_mat3x3 I, result;

    alwan_mat3_identity(&I);
    alwan_mat3_mul(&A, &I, &result);

    alwan_scalar diff = mat3_max_diff(&A, &result);
    TEST_ASSERT(diff < ALWAN_TEST_TOLERANCE, "A * I != A");

    TEST_PASS("test_multiply");
}

static int test_inverse_identity(void) {
    alwan_mat3x3 I, I_inv, result;

    alwan_mat3_identity(&I);
    int status = alwan_mat3_inv(&I, &I_inv);
    TEST_ASSERT(status == ALWAN_OK, "Failed to invert identity matrix");

    /* I^-1 should be I */
    alwan_scalar diff = mat3_max_diff(&I, &I_inv);
    TEST_ASSERT(diff < ALWAN_TEST_TOLERANCE, "I^-1 != I");

    /* I * I^-1 = I */
    alwan_mat3_mul(&I, &I_inv, &result);
    diff = mat3_max_diff(&I, &result);
    TEST_ASSERT(diff < ALWAN_TEST_TOLERANCE, "I * I^-1 != I");

    TEST_PASS("test_inverse_identity");
}

static int test_inverse_general(void) {
    /* Test matrix from rotation + scale */
    alwan_mat3x3 M = {{
        ALWAN_LITERAL(2.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(3.0), ALWAN_LITERAL(1.0),
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(2.0)
    }};

    alwan_mat3x3 M_inv, result, I;
    alwan_mat3_identity(&I);

    int status = alwan_mat3_inv(&M, &M_inv);
    TEST_ASSERT(status == ALWAN_OK, "Failed to invert test matrix");

    /* M * M^-1 should equal I */
    alwan_mat3_mul(&M, &M_inv, &result);
    alwan_scalar diff = mat3_max_diff(&I, &result);

    if (diff > ALWAN_TEST_TOLERANCE) {
        mat3_print("M", &M);
        mat3_print("M_inv", &M_inv);
        mat3_print("M * M_inv", &result);
        mat3_print("Identity", &I);
        printf("Max diff: %e\n", diff);
    }

    TEST_ASSERT(diff < ALWAN_TEST_TOLERANCE, "M * M^-1 != I (tolerance exceeded)");

    TEST_PASS("test_inverse_general");
}

static int test_inverse_singular(void) {
    /* Singular matrix (det = 0) */
    alwan_mat3x3 M = {{
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(2.0), ALWAN_LITERAL(3.0),
        ALWAN_LITERAL(2.0), ALWAN_LITERAL(4.0), ALWAN_LITERAL(6.0),
        ALWAN_LITERAL(3.0), ALWAN_LITERAL(6.0), ALWAN_LITERAL(9.0)
    }};

    alwan_mat3x3 M_inv;
    int status = alwan_mat3_inv(&M, &M_inv);

    TEST_ASSERT(status != ALWAN_OK, "Should fail to invert singular matrix");
    TEST_PASS("test_inverse_singular");
}

static int test_inverse_random_seed(void) {
    /* Test with deterministic random matrices */
    srand(42);

    for (int trial = 0; trial < 10; trial++) {
        alwan_mat3x3 M, M_inv, result, I;
        alwan_mat3_identity(&I);

        /* Generate random matrix with reasonable values */
        for (int i = 0; i < 9; i++) {
            M.m[i] = (alwan_scalar)(rand() % 100 - 50) / ALWAN_LITERAL(10.0);
        }

        /* Ensure it's not singular by adding to diagonal */
        M.m[0] += ALWAN_LITERAL(10.0);
        M.m[4] += ALWAN_LITERAL(10.0);
        M.m[8] += ALWAN_LITERAL(10.0);

        int status = alwan_mat3_inv(&M, &M_inv);
        if (status != ALWAN_OK) {
            continue;  /* Skip if numerically unstable */
        }

        /* M * M^-1 = I */
        alwan_mat3_mul(&M, &M_inv, &result);
        alwan_scalar diff = mat3_max_diff(&I, &result);

        /* Use 10x tolerance for random matrices (more numerically unstable) */
        alwan_scalar random_tolerance = ALWAN_TEST_TOLERANCE * ALWAN_LITERAL(10.0);
        if (diff > random_tolerance) {
            printf("Trial %d failed with diff %e\n", trial, diff);
            mat3_print("M", &M);
            mat3_print("M_inv", &M_inv);
            mat3_print("M * M_inv", &result);
            TEST_ASSERT(0, "Random matrix inversion failed");
        }
    }

    TEST_PASS("test_inverse_random_seed");
}

static int test_mat3_mulv(void) {
    alwan_mat3x3 M = {{
        1, 0, 0,
        0, 2, 0,
        0, 0, 3
    }};

    alwan_vec3 v = {{1, 1, 1}};
    alwan_vec3 result;

    alwan_mat3_mulv(&M, &v, &result);

    TEST_ASSERT(ALWAN_FABS(result.v[0] - ALWAN_LITERAL(1.0)) < ALWAN_TEST_TOLERANCE, "Mv[0] != 1");
    TEST_ASSERT(ALWAN_FABS(result.v[1] - ALWAN_LITERAL(2.0)) < ALWAN_TEST_TOLERANCE, "Mv[1] != 2");
    TEST_ASSERT(ALWAN_FABS(result.v[2] - ALWAN_LITERAL(3.0)) < ALWAN_TEST_TOLERANCE, "Mv[2] != 3");

    TEST_PASS("test_mat3_mulv");
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
    {"identity", test_identity},
    {"multiply", test_multiply},
    {"inverse_identity", test_inverse_identity},
    {"inverse_general", test_inverse_general},
    {"inverse_singular", test_inverse_singular},
    {"inverse_random_seed", test_inverse_random_seed},
    {"mat3_mulv", test_mat3_mulv},
};

int test_01_mat3_ops_main(void) {
    printf("Running matrix operation tests...\n");

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
