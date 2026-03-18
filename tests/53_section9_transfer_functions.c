/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for Section 9 transfer functions: Apple Log and DCDM
 * Reference data generated from OpenColorIO 2.5.0
 */

#include <stdio.h>
#include <stdlib.h>
#include "alwan.h"
#include "alwan_internal.h"
#include "test_common.h"


/* ============================================================================
 * Apple Log Test Data (from OCIO BuiltinTransform)
 * ============================================================================ */

/* Apple Log encoded input values (0-1 range) */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const apple_log_decode_input[] = {
#include "reference_values/apple_log_decode_input.csv"
};
ALWAN_DIAG_POP
#define NUM_APPLE_LOG_DECODE (sizeof(apple_log_decode_input) / sizeof(apple_log_decode_input[0]))

/* Expected linear values after decoding */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const apple_log_decode_expected[] = {
#include "reference_values/apple_log_decode_output.csv"
};
ALWAN_DIAG_POP

/* Linear input values for encoding */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const apple_log_encode_input[] = {
#include "reference_values/apple_log_encode_input.csv"
};
ALWAN_DIAG_POP
#define NUM_APPLE_LOG_ENCODE (sizeof(apple_log_encode_input) / sizeof(apple_log_encode_input[0]))

/* Expected Apple Log encoded values */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const apple_log_encode_expected[] = {
#include "reference_values/apple_log_encode_output.csv"
};
ALWAN_DIAG_POP

/* ============================================================================
 * DCDM Test Data (from OCIO BuiltinTransform)
 * ============================================================================ */

/* Linear input values for DCDM encoding */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const dcdm_encode_input[] = {
#include "reference_values/dcdm_encode_input.csv"
};
ALWAN_DIAG_POP
#define NUM_DCDM_ENCODE (sizeof(dcdm_encode_input) / sizeof(dcdm_encode_input[0]))

/* Expected DCDM encoded values */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const dcdm_encode_expected[] = {
#include "reference_values/dcdm_encode_output.csv"
};
ALWAN_DIAG_POP

/* DCDM encoded input values for decoding */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const dcdm_decode_input[] = {
#include "reference_values/dcdm_decode_input.csv"
};
ALWAN_DIAG_POP
#define NUM_DCDM_DECODE (sizeof(dcdm_decode_input) / sizeof(dcdm_decode_input[0]))

/* Expected linear values after decoding */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const dcdm_decode_expected[] = {
#include "reference_values/dcdm_decode_output.csv"
};
ALWAN_DIAG_POP

/* ============================================================================
 * Apple Log Tests
 * ============================================================================ */

static int test_apple_log_decoding(void) {
    printf("  Testing Apple Log decoding (EOTF)...\n");

    alwan_f64 decoded[NUM_APPLE_LOG_DECODE];

    /* Apply Apple Log decoding (EOTF) */
    int result = alwan_eotf_apply(decoded, ALWAN_TF_APPLE_LOG,
                                  apple_log_decode_input,
                                  NUM_APPLE_LOG_DECODE,
                                  sizeof(alwan_f64), sizeof(alwan_f64));
    if (result != 0) {
        printf("FAIL: alwan_eotf_apply returned error %d\n", result);
        return 1;
    }

    /* Check values */
    for (size_t i = 0; i < NUM_APPLE_LOG_DECODE; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Apple Log decode [%zu]: encoded=%.2f",
                 i, (alwan_f64)apple_log_decode_input[i]);
        TEST_ASSERT_ABS(decoded[i], apple_log_decode_expected[i], ALWAN_TEST_TOLERANCE, msg);
    }

    printf("    PASS: %zu decoding tests\n", NUM_APPLE_LOG_DECODE);
    return 0;
}

static int test_apple_log_encoding(void) {
    printf("  Testing Apple Log encoding (OETF)...\n");

    alwan_f64 encoded[NUM_APPLE_LOG_ENCODE];

    /* Apply Apple Log encoding (OETF) */
    int result = alwan_oetf_apply(encoded, ALWAN_TF_APPLE_LOG,
                                  apple_log_encode_input,
                                  NUM_APPLE_LOG_ENCODE,
                                  sizeof(alwan_f64), sizeof(alwan_f64));
    if (result != 0) {
        printf("FAIL: alwan_oetf_apply returned error %d\n", result);
        return 1;
    }

    /* Check values */
    for (size_t i = 0; i < NUM_APPLE_LOG_ENCODE; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Apple Log encode [%zu]: linear=%.4f",
                 i, (double)apple_log_encode_input[i]);
        /* For very large values (HDR), use relative tolerance; for small values, absolute */
        if (ALWAN_ABS(apple_log_encode_expected[i]) > ALWAN_LITERAL(0.01)) {
            TEST_ASSERT_REL(encoded[i], apple_log_encode_expected[i], ALWAN_TEST_TOLERANCE, msg);
        } else {
            TEST_ASSERT_ABS(encoded[i], apple_log_encode_expected[i], ALWAN_TEST_TOLERANCE, msg);
        }
    }

    printf("    PASS: %zu encoding tests\n", NUM_APPLE_LOG_ENCODE);
    return 0;
}

/* ============================================================================
 * DCDM Tests
 * ============================================================================ */

static int test_dcdm_encoding(void) {
    printf("  Testing DCDM encoding (OETF)...\n");

    alwan_f64 encoded[NUM_DCDM_ENCODE];

    /* Apply DCDM encoding (OETF) */
    int result = alwan_oetf_apply(encoded, ALWAN_TF_DCDM,
                                  dcdm_encode_input,
                                  NUM_DCDM_ENCODE,
                                  sizeof(alwan_f64), sizeof(alwan_f64));
    if (result != 0) {
        printf("FAIL: alwan_oetf_apply returned error %d\n", result);
        return 1;
    }

    /* Check values */
    for (size_t i = 0; i < NUM_DCDM_ENCODE; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "DCDM encode [%zu]: linear=%.4f",
                 i, (double)dcdm_encode_input[i]);
        /* For very large values, use relative tolerance; for small values, absolute */
        if (ALWAN_ABS(dcdm_encode_expected[i]) > ALWAN_LITERAL(0.01)) {
            TEST_ASSERT_REL(encoded[i], dcdm_encode_expected[i], ALWAN_TEST_TOLERANCE, msg);
        } else {
            TEST_ASSERT_ABS(encoded[i], dcdm_encode_expected[i], ALWAN_TEST_TOLERANCE, msg);
        }
    }

    printf("    PASS: %zu encoding tests\n", NUM_DCDM_ENCODE);
    return 0;
}

static int test_dcdm_decoding(void) {
    printf("  Testing DCDM decoding (EOTF)...\n");

    alwan_f64 decoded[NUM_DCDM_DECODE];

    /* Apply DCDM decoding (EOTF) */
    int result = alwan_eotf_apply(decoded, ALWAN_TF_DCDM,
                                  dcdm_decode_input,
                                  NUM_DCDM_DECODE,
                                  sizeof(alwan_f64), sizeof(alwan_f64));
    if (result != 0) {
        printf("FAIL: alwan_eotf_apply returned error %d\n", result);
        return 1;
    }

    /* Check values */
    for (size_t i = 0; i < NUM_DCDM_DECODE; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "DCDM decode [%zu]: encoded=%.2f",
                 i, (double)dcdm_decode_input[i]);
        /* For very large values, use relative tolerance; for small values, absolute */
        if (ALWAN_ABS(dcdm_decode_expected[i]) > ALWAN_LITERAL(0.01)) {
            TEST_ASSERT_REL(decoded[i], dcdm_decode_expected[i], ALWAN_TEST_TOLERANCE, msg);
        } else {
            TEST_ASSERT_ABS(decoded[i], dcdm_decode_expected[i], ALWAN_TEST_TOLERANCE, msg);
        }
    }

    printf("    PASS: %zu decoding tests\n", NUM_DCDM_DECODE);
    return 0;
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int test_section9_transfer_functions(void) {
    printf("Testing Section 9 Transfer Functions (Apple Log, DCDM)...\n");
    int failures = 0;

    /* Apple Log tests */
    printf("\n  Apple Log (iPhone 15 Pro+):\n");
    if (test_apple_log_decoding() != 0) failures++;
    if (test_apple_log_encoding() != 0) failures++;

    /* DCDM tests */
    printf("\n  DCDM (Digital Cinema):\n");
    if (test_dcdm_encoding() != 0) failures++;
    if (test_dcdm_decoding() != 0) failures++;

    if (failures == 0) {
        printf("\nAll Section 9 Transfer Functions tests PASSED!\n");
    } else {
        printf("\nSection 9 Transfer Functions: %d test(s) FAILED\n", failures);
    }

    return failures;
}
