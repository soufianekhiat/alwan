/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 04: sRGB transfer functions (OETF/EOTF)
 */

#include "test_common.h"
#include <stdlib.h>

/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */

static int test_srgb_round_trip(void) {
    /* Test round-trip: linear -> encode -> decode -> linear */
    alwan_f64 const test_values[] = {
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

    alwan_f64 encoded[9];
    alwan_f64 decoded[9];

    /* Encode */
    int status = alwan_oetf_apply(encoded, ALWAN_TF_SRGB, test_values, num_values,
                                   sizeof(alwan_f64), sizeof(alwan_f64));
    TEST_ASSERT(status == ALWAN_OK, "sRGB OETF failed");

    /* Decode */
    status = alwan_eotf_apply(decoded, ALWAN_TF_SRGB, encoded, num_values,
                              sizeof(alwan_f64), sizeof(alwan_f64));
    TEST_ASSERT(status == ALWAN_OK, "sRGB EOTF failed");

    /* Verify round-trip */
    for (size_t i = 0; i < num_values; i++) {
        alwan_f64 diff = ALWAN_ABS(decoded[i] - test_values[i]);
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
    /* OETF breakpoint at 0.0031308 -> 0.04045 */
    alwan_f64 const linear_bp = ALWAN_LITERAL(0.0031308);
    alwan_f64 encoded;

    int status = alwan_oetf_apply(&encoded, ALWAN_TF_SRGB, &linear_bp, 1,
                                   sizeof(alwan_f64), sizeof(alwan_f64));
    TEST_ASSERT(status == ALWAN_OK, "sRGB OETF at breakpoint failed");

    /* Expected encoded value: 12.92 * 0.0031308 ~= 0.04045 */
    alwan_f64 expected = ALWAN_LITERAL(0.04045);
    alwan_f64 diff = ALWAN_ABS(encoded - expected);

    /* Use looser tolerance for breakpoint (numerical precision) */
    TEST_ASSERT(diff < ALWAN_LITERAL(0.001), "sRGB OETF breakpoint mismatch");

    TEST_PASS("test_srgb_breakpoint");
}

static int test_srgb_known_values(void) {
    /* Test known sRGB values */
    struct {
        alwan_f64 linear;
        alwan_f64 encoded;
    } const known_pairs[] = {
        {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)},
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)},
        {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.735356983052449)},  /* Approximate */
    };
    size_t const num_pairs = sizeof(known_pairs) / sizeof(known_pairs[0]);

    for (size_t i = 0; i < num_pairs; i++) {
        alwan_f64 encoded, decoded;

        /* Test OETF */
        int status = alwan_oetf_apply(&encoded, ALWAN_TF_SRGB, &known_pairs[i].linear, 1,
                                      sizeof(alwan_f64), sizeof(alwan_f64));
        TEST_ASSERT(status == ALWAN_OK, "sRGB OETF failed");

        alwan_f64 oetf_diff = ALWAN_ABS(encoded - known_pairs[i].encoded);
        if (oetf_diff > ALWAN_TEST_TOLERANCE) {
            printf("OETF mismatch at pair %zu:\n", i);
            printf("  Linear:   %.8f\n", known_pairs[i].linear);
            printf("  Expected: %.8f\n", known_pairs[i].encoded);
            printf("  Got:      %.8f\n", encoded);
            printf("  Diff:     %e\n", oetf_diff);
            TEST_ASSERT(0, "OETF known value mismatch");
        }

        /* Test EOTF */
        status = alwan_eotf_apply(&decoded, ALWAN_TF_SRGB, &known_pairs[i].encoded, 1,
                                  sizeof(alwan_f64), sizeof(alwan_f64));
        TEST_ASSERT(status == ALWAN_OK, "sRGB EOTF failed");

        alwan_f64 eotf_diff = ALWAN_ABS(decoded - known_pairs[i].linear);
        if (eotf_diff > ALWAN_TEST_TOLERANCE) {
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
    alwan_f64 dummy_in = ALWAN_LITERAL(0.5);
    alwan_f64 dummy_out;

    /* Test invalid transfer function enum (cast invalid value to enum) */
    int status = alwan_oetf_apply(&dummy_out, (alwan_transfer_function)999, &dummy_in, 1,
                                   sizeof(alwan_f64), sizeof(alwan_f64));
    TEST_ASSERT(status == ALWAN_E_INVALID, "Should reject invalid OETF enum");

    status = alwan_eotf_apply(&dummy_out, (alwan_transfer_function)999, &dummy_in, 1,
                              sizeof(alwan_f64), sizeof(alwan_f64));
    TEST_ASSERT(status == ALWAN_E_INVALID, "Should reject invalid EOTF enum");

    TEST_PASS("test_srgb_invalid_name");
}

static int test_srgb_dense_lut(void) {
    /* Dense LUT test: encode and decode 256 values */
    #define LUT_SIZE 256
    alwan_f64 linear[LUT_SIZE];
    alwan_f64 encoded[LUT_SIZE];
    alwan_f64 decoded[LUT_SIZE];

    /* Generate linear ramp [0, 1] */
    for (int i = 0; i < LUT_SIZE; i++) {
        linear[i] = (alwan_f64)i / (alwan_f64)(LUT_SIZE - 1);
    }

    /* Encode */
    int status = alwan_oetf_apply(encoded, ALWAN_TF_SRGB, linear, LUT_SIZE,
                                   sizeof(alwan_f64), sizeof(alwan_f64));
    TEST_ASSERT(status == ALWAN_OK, "Dense LUT OETF failed");

    /* Decode */
    status = alwan_eotf_apply(decoded, ALWAN_TF_SRGB, encoded, LUT_SIZE,
                              sizeof(alwan_f64), sizeof(alwan_f64));
    TEST_ASSERT(status == ALWAN_OK, "Dense LUT EOTF failed");

    /* Verify round-trip */
    alwan_f64 max_diff = 0;
    for (int i = 0; i < LUT_SIZE; i++) {
        alwan_f64 diff = ALWAN_ABS(decoded[i] - linear[i]);
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

int test_04_srgb_tf_main(void) {
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
            printf("[FAIL] Test '%s' failed\n", tests[i].name);
        }
    }

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed (out of %zu)\n", passed, failed, num_tests);
    printf("========================================\n");

    return (failed > 0) ? 1 : 0;
}
