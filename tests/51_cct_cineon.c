/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for CCT methods (Hernandez, Kang) and Cineon transfer function
 */

#include "alwan.h"
#include "alwan_internal.h"
#include "test_common.h"
#include <stdio.h>
#include <stdlib.h>

/* ============================================================================
 * Cineon Test Data
 * ============================================================================ */

/* Cineon linear input values */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const cineon_linear_input[] = {
#include "reference_values/cineon_linear_input.csv"
};
ALWAN_DIAG_POP
#define NUM_CINEON_LINEAR (sizeof(cineon_linear_input) / sizeof(cineon_linear_input[0]))

/* Expected encoded values */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const cineon_encoded_expected[] = {
#include "reference_values/cineon_encoded.csv"
};
ALWAN_DIAG_POP

/* Cineon encoded input values for decoding test */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const cineon_encoded_input[] = {
#include "reference_values/cineon_encoded_input.csv"
};
ALWAN_DIAG_POP
#define NUM_CINEON_ENCODED (sizeof(cineon_encoded_input) / sizeof(cineon_encoded_input[0]))

/* Expected decoded values */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const cineon_decoded_expected[] = {
#include "reference_values/cineon_decoded.csv"
};
ALWAN_DIAG_POP

/* ============================================================================
 * CCT Hernandez Test Data
 * ============================================================================ */

/* Hernandez xy input coordinates */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const hernandez_xy_input[] = {
#include "reference_values/cct_hernandez_xy_input.csv"
};
ALWAN_DIAG_POP
#define NUM_HERNANDEZ_XY (sizeof(hernandez_xy_input) / sizeof(hernandez_xy_input[0]) / 2)

/* Expected CCT values from Hernandez */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const hernandez_cct_expected[] = {
#include "reference_values/cct_hernandez_output.csv"
};
ALWAN_DIAG_POP

/* ============================================================================
 * CCT Kang Test Data
 * ============================================================================ */

/* Kang CCT input values */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const kang_cct_input[] = {
#include "reference_values/cct_kang_cct_input.csv"
};
ALWAN_DIAG_POP
#define NUM_KANG_CCT (sizeof(kang_cct_input) / sizeof(kang_cct_input[0]))

/* Expected xy output from Kang forward */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const kang_xy_expected[] = {
#include "reference_values/cct_kang_xy_output.csv"
};
ALWAN_DIAG_POP

/* ============================================================================
 * Test Functions
 * ============================================================================ */

static int test_cineon_encoding(void) {
    printf("  Testing Cineon encoding...\n");

    alwan_f64 encoded[NUM_CINEON_LINEAR];

    /* Apply Cineon encoding */
    int result = alwan_oetf_apply(encoded, ALWAN_TF_CINEON,
                                  cineon_linear_input, NUM_CINEON_LINEAR,
                                  sizeof(alwan_f64), sizeof(alwan_f64));
    if (result != 0) {
        printf("FAIL: alwan_oetf_apply returned error %d\n", result);
        return 1;
    }

    /* Check values */
    for (size_t i = 0; i < NUM_CINEON_LINEAR; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Cineon encode [%zu]: linear=%.4f", i, (double)cineon_linear_input[i]);
        TEST_ASSERT_REL(encoded[i], cineon_encoded_expected[i], ALWAN_TEST_TOLERANCE, msg);
    }

    printf("    PASS: %zu encoding tests\n", NUM_CINEON_LINEAR);
    return 0;
}

static int test_cineon_decoding(void) {
    printf("  Testing Cineon decoding...\n");

    alwan_f64 decoded[NUM_CINEON_ENCODED];

    /* Apply Cineon decoding */
    int result = alwan_eotf_apply(decoded, ALWAN_TF_CINEON,
                                  cineon_encoded_input, NUM_CINEON_ENCODED,
                                  sizeof(alwan_f64), sizeof(alwan_f64));
    if (result != 0) {
        printf("FAIL: alwan_eotf_apply returned error %d\n", result);
        return 1;
    }

    /* Check values */
    for (size_t i = 0; i < NUM_CINEON_ENCODED; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Cineon decode [%zu]: encoded=%.4f", i, (double)cineon_encoded_input[i]);
        TEST_ASSERT_REL(decoded[i], cineon_decoded_expected[i], ALWAN_TEST_TOLERANCE, msg);
    }

    printf("    PASS: %zu decoding tests\n", NUM_CINEON_ENCODED);
    return 0;
}

static int test_cineon_roundtrip(void) {
    printf("  Testing Cineon roundtrip...\n");

    alwan_f64 encoded[NUM_CINEON_LINEAR];
    alwan_f64 roundtrip[NUM_CINEON_LINEAR];

    /* Encode then decode */
    alwan_oetf_apply(encoded, ALWAN_TF_CINEON, cineon_linear_input, NUM_CINEON_LINEAR, sizeof(alwan_f64), sizeof(alwan_f64));
    alwan_eotf_apply(roundtrip, ALWAN_TF_CINEON, encoded, NUM_CINEON_LINEAR, sizeof(alwan_f64), sizeof(alwan_f64));

    /* Check roundtrip accuracy */
    for (size_t i = 0; i < NUM_CINEON_LINEAR; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Cineon roundtrip [%zu]: original=%.4f", i, (double)cineon_linear_input[i]);
        TEST_ASSERT_REL(roundtrip[i], cineon_linear_input[i], ALWAN_TEST_TOLERANCE, msg);
    }

    printf("    PASS: %zu roundtrip tests\n", NUM_CINEON_LINEAR);
    return 0;
}

static int test_cct_hernandez(void) {
    printf("  Testing CCT Hernandez 1999...\n");

    for (size_t i = 0; i < NUM_HERNANDEZ_XY; i++) {
        alwan_vec2_f64 xy;
        xy.v[0] = hernandez_xy_input[i * 2];
        xy.v[1] = hernandez_xy_input[i * 2 + 1];

        alwan_f64 cct = alwan_cct_hernandez_xy_f64(&xy);

        char msg[128];
        snprintf(msg, sizeof(msg), "Hernandez CCT [%zu]: xy=(%.4f, %.4f)",
                 i, (double)xy.v[0], (double)xy.v[1]);
        TEST_ASSERT_REL(cct, hernandez_cct_expected[i], ALWAN_TEST_TOLERANCE, msg);
    }

    printf("    PASS: %zu Hernandez tests\n", NUM_HERNANDEZ_XY);
    return 0;
}

static int test_cct_kang_forward(void) {
    printf("  Testing CCT Kang 2002 (CCT to xy)...\n");

    for (size_t i = 0; i < NUM_KANG_CCT; i++) {
        alwan_f64 cct = kang_cct_input[i];
        alwan_vec2_f64 xy;
        alwan_cct_to_xy_kang_f64(&xy, cct);

        alwan_f64 expected_x = kang_xy_expected[i * 2];
        alwan_f64 expected_y = kang_xy_expected[i * 2 + 1];

        char msg_x[128], msg_y[128];
        snprintf(msg_x, sizeof(msg_x), "Kang xy.x [%zu]: CCT=%.0fK", i, (double)cct);
        snprintf(msg_y, sizeof(msg_y), "Kang xy.y [%zu]: CCT=%.0fK", i, (double)cct);

        TEST_ASSERT_REL(xy.v[0], expected_x, ALWAN_TEST_TOLERANCE, msg_x);
        TEST_ASSERT_REL(xy.v[1], expected_y, ALWAN_TEST_TOLERANCE, msg_y);
    }

    printf("    PASS: %zu Kang forward tests\n", NUM_KANG_CCT);
    return 0;
}

static int test_cct_kang_inverse(void) {
    printf("  Testing CCT Kang 2002 (xy to CCT)...\n");

    /* Test inverse by converting CCT -> xy -> CCT */
    for (size_t i = 0; i < NUM_KANG_CCT; i++) {
        alwan_f64 original_cct = kang_cct_input[i];

        /* Skip out of range values (Kang valid: 1667-25000K) */
        if (original_cct < 1667.0 || original_cct > 25000.0) continue;

        /* Forward: CCT -> xy */
        alwan_vec2_f64 xy;
        alwan_cct_to_xy_kang_f64(&xy, original_cct);

        /* Inverse: xy -> CCT */
        alwan_f64 recovered_cct = alwan_cct_kang_xy_f64(&xy);

        char msg[128];
        snprintf(msg, sizeof(msg), "Kang inverse [%zu]: original CCT=%.0fK", i, (double)original_cct);

        /* Use absolute tolerance for CCT (iterative method) */
        TEST_ASSERT_ABS(recovered_cct, original_cct, ALWAN_TEST_TOLERANCE, msg);
    }

    printf("    PASS: Kang inverse tests\n");
    return 0;
}

static int test_cct_methods_comparison(void) {
    printf("  Testing CCT method comparison (D65)...\n");

    /* D65 chromaticity */
    alwan_vec2_f64 d65 = {{0.31270, 0.32900}};

    /* Get CCT from each method */
    alwan_f64 cct_mccamy = alwan_cct_mccamy_xy_f64(&d65);
    alwan_f64 cct_robertson = alwan_cct_robertson_xy_f64(&d65);
    alwan_f64 cct_hernandez = alwan_cct_hernandez_xy_f64(&d65);
    alwan_f64 cct_kang = alwan_cct_kang_xy_f64(&d65);

    printf("    D65 CCT results:\n");
    printf("      McCamy:    %.2f K\n", (double)cct_mccamy);
    printf("      Robertson: %.2f K\n", (double)cct_robertson);
    printf("      Hernandez: %.2f K\n", (double)cct_hernandez);
    printf("      Kang:      %.2f K\n", (double)cct_kang);

    /* All methods should be close to 6500K for D65 */
    alwan_f64 expected_d65 = ALWAN_LITERAL(6500.0);
    alwan_f64 tolerance = ALWAN_LITERAL(150.0);  /* 150K tolerance for D65 (Kang method varies more) */

    TEST_ASSERT_ABS(cct_mccamy, expected_d65, tolerance, "McCamy D65");
    TEST_ASSERT_ABS(cct_robertson, expected_d65, tolerance, "Robertson D65");
    TEST_ASSERT_ABS(cct_hernandez, expected_d65, tolerance, "Hernandez D65");
    TEST_ASSERT_ABS(cct_kang, expected_d65, tolerance, "Kang D65");

    printf("    PASS: All methods within %.0fK of expected D65\n", tolerance);
    return 0;
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int test_51_cct_cineon_main(void) {
    printf("CCT and Cineon Tests\n");
    printf("====================\n");

    int failures = 0;

    /* Cineon tests */
    failures += test_cineon_encoding();
    failures += test_cineon_decoding();
    failures += test_cineon_roundtrip();

    /* CCT tests */
    failures += test_cct_hernandez();
    failures += test_cct_kang_forward();
    failures += test_cct_kang_inverse();
    failures += test_cct_methods_comparison();

    if (failures == 0) {
        printf("\nAll CCT and Cineon tests PASSED!\n");
    } else {
        printf("\n%d test(s) FAILED!\n", failures);
    }

    return failures;
}
