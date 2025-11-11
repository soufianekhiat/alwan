/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 *
 * Test 11: sRGB transfer functions (OETF/EOTF)
 */

#include "../../src/alwan/alwan.h"
#include "../../src/alwan/alwan_internal.h"
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

/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */

static int test_srgb_round_trip(void) {
    /* Test round-trip: linear → encode → decode → linear */
    Scalar const test_values[] = {
        ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.0001),
        ALWAN_LITERAL(0.001),
        ALWAN_LITERAL(0.01),
        ALWAN_LITERAL(0.1),
        ALWAN_LITERAL(0.18),  /* 18% gray */
        ALWAN_LITERAL(0.5),
        ALWAN_LITERAL(0.9),
        ALWAN_LITERAL(1.0)
    };
    size_t const num_values = sizeof(test_values) / sizeof(test_values[0]);

    Scalar encoded[9];
    Scalar decoded[9];

    /* Encode */
    int status = alwan_oetf_apply("srgb", test_values, num_values, sizeof(Scalar),
                                   encoded, sizeof(Scalar));
    TEST_ASSERT(status == ALWAN_OK, "sRGB OETF failed");

    /* Decode */
    status = alwan_eotf_apply("srgb", encoded, num_values, sizeof(Scalar),
                              decoded, sizeof(Scalar));
    TEST_ASSERT(status == ALWAN_OK, "sRGB EOTF failed");

    /* Verify round-trip */
    for (size_t i = 0; i < num_values; i++) {
        Scalar diff = ALWAN_FABS(decoded[i] - test_values[i]);
        if (diff > ALWAN_TEST_TOLERANCE) {
            printf("Round-trip failed at index %zu:\n", i);
            printf("  Input:   %.8f\n", test_values[i]);
            printf("  Encoded: %.8f\n", encoded[i]);
            printf("  Decoded: %.8f\n", decoded[i]);
            printf("  Diff:    %e\n", diff);
            TEST_ASSERT(0, "Round-trip tolerance exceeded");
        }
    }

    TEST_PASS("test_srgb_round_trip");
}

static int test_srgb_breakpoint(void) {
    /* Test the breakpoint where formula changes */
    /* OETF breakpoint at 0.0031308 → 0.04045 */
    Scalar const linear_bp = ALWAN_LITERAL(0.0031308);
    Scalar encoded;

    int status = alwan_oetf_apply("srgb", &linear_bp, 1, sizeof(Scalar),
                                   &encoded, sizeof(Scalar));
    TEST_ASSERT(status == ALWAN_OK, "sRGB OETF at breakpoint failed");

    /* Expected encoded value: 12.92 * 0.0031308 ≈ 0.04045 */
    Scalar expected = ALWAN_LITERAL(0.04045);
    Scalar diff = ALWAN_FABS(encoded - expected);

    /* Use looser tolerance for breakpoint (numerical precision) */
    TEST_ASSERT(diff < ALWAN_LITERAL(0.001), "sRGB OETF breakpoint mismatch");

    TEST_PASS("test_srgb_breakpoint");
}

static int test_srgb_known_values(void) {
    /* Test known sRGB values */
    struct {
        Scalar linear;
        Scalar encoded;
    } const known_pairs[] = {
        {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)},
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)},
        {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.735356983052449)},  /* Approximate */
    };
    size_t const num_pairs = sizeof(known_pairs) / sizeof(known_pairs[0]);

    for (size_t i = 0; i < num_pairs; i++) {
        Scalar encoded, decoded;

        /* Test OETF */
        int status = alwan_oetf_apply("srgb", &known_pairs[i].linear, 1, sizeof(Scalar),
                                      &encoded, sizeof(Scalar));
        TEST_ASSERT(status == ALWAN_OK, "sRGB OETF failed");

        Scalar oetf_diff = ALWAN_FABS(encoded - known_pairs[i].encoded);
        if (oetf_diff > ALWAN_TEST_TOLERANCE * ALWAN_LITERAL(10.0)) {
            printf("OETF mismatch at pair %zu:\n", i);
            printf("  Linear:   %.8f\n", known_pairs[i].linear);
            printf("  Expected: %.8f\n", known_pairs[i].encoded);
            printf("  Got:      %.8f\n", encoded);
            printf("  Diff:     %e\n", oetf_diff);
            TEST_ASSERT(0, "OETF known value mismatch");
        }

        /* Test EOTF */
        status = alwan_eotf_apply("srgb", &known_pairs[i].encoded, 1, sizeof(Scalar),
                                  &decoded, sizeof(Scalar));
        TEST_ASSERT(status == ALWAN_OK, "sRGB EOTF failed");

        Scalar eotf_diff = ALWAN_FABS(decoded - known_pairs[i].linear);
        if (eotf_diff > ALWAN_TEST_TOLERANCE * ALWAN_LITERAL(10.0)) {
            printf("EOTF mismatch at pair %zu:\n", i);
            printf("  Encoded:  %.8f\n", known_pairs[i].encoded);
            printf("  Expected: %.8f\n", known_pairs[i].linear);
            printf("  Got:      %.8f\n", decoded);
            printf("  Diff:     %e\n", eotf_diff);
            TEST_ASSERT(0, "EOTF known value mismatch");
        }
    }

    TEST_PASS("test_srgb_known_values");
}

static int test_srgb_invalid_name(void) {
    Scalar dummy_in = ALWAN_LITERAL(0.5);
    Scalar dummy_out;

    /* Test invalid transfer function name */
    int status = alwan_oetf_apply("invalid_tf", &dummy_in, 1, sizeof(Scalar),
                                   &dummy_out, sizeof(Scalar));
    TEST_ASSERT(status == ALWAN_E_INVALID, "Should reject invalid OETF name");

    status = alwan_eotf_apply("invalid_tf", &dummy_in, 1, sizeof(Scalar),
                              &dummy_out, sizeof(Scalar));
    TEST_ASSERT(status == ALWAN_E_INVALID, "Should reject invalid EOTF name");

    TEST_PASS("test_srgb_invalid_name");
}

static int test_srgb_dense_lut(void) {
    /* Dense LUT test: encode and decode 256 values */
    #define LUT_SIZE 256
    Scalar linear[LUT_SIZE];
    Scalar encoded[LUT_SIZE];
    Scalar decoded[LUT_SIZE];

    /* Generate linear ramp [0, 1] */
    for (int i = 0; i < LUT_SIZE; i++) {
        linear[i] = (Scalar)i / (Scalar)(LUT_SIZE - 1);
    }

    /* Encode */
    int status = alwan_oetf_apply("srgb", linear, LUT_SIZE, sizeof(Scalar),
                                   encoded, sizeof(Scalar));
    TEST_ASSERT(status == ALWAN_OK, "Dense LUT OETF failed");

    /* Decode */
    status = alwan_eotf_apply("srgb", encoded, LUT_SIZE, sizeof(Scalar),
                              decoded, sizeof(Scalar));
    TEST_ASSERT(status == ALWAN_OK, "Dense LUT EOTF failed");

    /* Verify round-trip */
    Scalar max_diff = 0;
    for (int i = 0; i < LUT_SIZE; i++) {
        Scalar diff = ALWAN_FABS(decoded[i] - linear[i]);
        if (diff > max_diff) max_diff = diff;
    }

    printf("  Dense LUT max round-trip error: %e\n", max_diff);
    TEST_ASSERT(max_diff < ALWAN_TEST_TOLERANCE, "Dense LUT round-trip failed");

    TEST_PASS("test_srgb_dense_lut");
    #undef LUT_SIZE
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
    {"srgb_round_trip", test_srgb_round_trip},
    {"srgb_breakpoint", test_srgb_breakpoint},
    {"srgb_known_values", test_srgb_known_values},
    {"srgb_invalid_name", test_srgb_invalid_name},
    {"srgb_dense_lut", test_srgb_dense_lut},
};

int test_11_srgb_tf_main(void) {
    printf("Running sRGB transfer function tests...\n");

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
