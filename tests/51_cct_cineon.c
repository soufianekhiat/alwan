/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for CCT methods (Hernandez, Kang) and Cineon transfer function
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../../src/alwan/alwan.h"

/* Test tolerance */
#define TEST_TOLERANCE_REL 1e-6
#define TEST_TOLERANCE_CCT 10.0  /* CCT tolerance in Kelvin for iterative methods */

/* Test assertion macro */
#define TEST_ASSERT_REL(got, expected, tol, msg) do { \
    alwan_scalar _got = (got); \
    alwan_scalar _exp = (expected); \
    alwan_scalar _diff = (_exp != 0.0) ? fabs((_got - _exp) / _exp) : fabs(_got - _exp); \
    if (_diff > (tol)) { \
        printf("FAIL: %s\n  Expected: %.10e, Got: %.10e, RelDiff: %.10e\n", \
               msg, (double)_exp, (double)_got, (double)_diff); \
        return 1; \
    } \
} while(0)

#define TEST_ASSERT_ABS(got, expected, tol, msg) do { \
    alwan_scalar _got = (got); \
    alwan_scalar _exp = (expected); \
    alwan_scalar _diff = fabs(_got - _exp); \
    if (_diff > (tol)) { \
        printf("FAIL: %s\n  Expected: %.10e, Got: %.10e, AbsDiff: %.10e\n", \
               msg, (double)_exp, (double)_got, (double)_diff); \
        return 1; \
    } \
} while(0)

/* ============================================================================
 * Cineon Test Data
 * ============================================================================ */

/* Cineon linear input values */
static alwan_scalar const cineon_linear_input[] = {
#include "reference_values/cineon_linear_input.csv"
};
#define NUM_CINEON_LINEAR (sizeof(cineon_linear_input) / sizeof(cineon_linear_input[0]))

/* Expected encoded values */
static alwan_scalar const cineon_encoded_expected[] = {
#include "reference_values/cineon_encoded.csv"
};

/* Cineon encoded input values for decoding test */
static alwan_scalar const cineon_encoded_input[] = {
#include "reference_values/cineon_encoded_input.csv"
};
#define NUM_CINEON_ENCODED (sizeof(cineon_encoded_input) / sizeof(cineon_encoded_input[0]))

/* Expected decoded values */
static alwan_scalar const cineon_decoded_expected[] = {
#include "reference_values/cineon_decoded.csv"
};

/* ============================================================================
 * CCT Hernandez Test Data
 * ============================================================================ */

/* Hernandez xy input coordinates */
static alwan_scalar const hernandez_xy_input[] = {
#include "reference_values/cct_hernandez_xy_input.csv"
};
#define NUM_HERNANDEZ_XY (sizeof(hernandez_xy_input) / sizeof(hernandez_xy_input[0]) / 2)

/* Expected CCT values from Hernandez */
static alwan_scalar const hernandez_cct_expected[] = {
#include "reference_values/cct_hernandez_output.csv"
};

/* ============================================================================
 * CCT Kang Test Data
 * ============================================================================ */

/* Kang CCT input values */
static alwan_scalar const kang_cct_input[] = {
#include "reference_values/cct_kang_cct_input.csv"
};
#define NUM_KANG_CCT (sizeof(kang_cct_input) / sizeof(kang_cct_input[0]))

/* Expected xy output from Kang forward */
static alwan_scalar const kang_xy_expected[] = {
#include "reference_values/cct_kang_xy_output.csv"
};

/* ============================================================================
 * Test Functions
 * ============================================================================ */

static int test_cineon_encoding(void) {
    printf("  Testing Cineon encoding...\n");

    alwan_scalar encoded[NUM_CINEON_LINEAR];

    /* Apply Cineon encoding */
    int result = alwan_oetf_apply(encoded, ALWAN_TF_CINEON,
                                  cineon_linear_input, NUM_CINEON_LINEAR,
                                  sizeof(alwan_scalar), sizeof(alwan_scalar));
    if (result != 0) {
        printf("FAIL: alwan_oetf_apply returned error %d\n", result);
        return 1;
    }

    /* Check values */
    for (size_t i = 0; i < NUM_CINEON_LINEAR; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Cineon encode [%zu]: linear=%.4f", i, (double)cineon_linear_input[i]);
        TEST_ASSERT_REL(encoded[i], cineon_encoded_expected[i], TEST_TOLERANCE_REL, msg);
    }

    printf("    PASS: %zu encoding tests\n", NUM_CINEON_LINEAR);
    return 0;
}

static int test_cineon_decoding(void) {
    printf("  Testing Cineon decoding...\n");

    alwan_scalar decoded[NUM_CINEON_ENCODED];

    /* Apply Cineon decoding */
    int result = alwan_eotf_apply(decoded, ALWAN_TF_CINEON,
                                  cineon_encoded_input, NUM_CINEON_ENCODED,
                                  sizeof(alwan_scalar), sizeof(alwan_scalar));
    if (result != 0) {
        printf("FAIL: alwan_eotf_apply returned error %d\n", result);
        return 1;
    }

    /* Check values */
    for (size_t i = 0; i < NUM_CINEON_ENCODED; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Cineon decode [%zu]: encoded=%.4f", i, (double)cineon_encoded_input[i]);
        TEST_ASSERT_REL(decoded[i], cineon_decoded_expected[i], TEST_TOLERANCE_REL, msg);
    }

    printf("    PASS: %zu decoding tests\n", NUM_CINEON_ENCODED);
    return 0;
}

static int test_cineon_roundtrip(void) {
    printf("  Testing Cineon roundtrip...\n");

    alwan_scalar encoded[NUM_CINEON_LINEAR];
    alwan_scalar roundtrip[NUM_CINEON_LINEAR];

    /* Encode then decode */
    alwan_oetf_apply(encoded, ALWAN_TF_CINEON, cineon_linear_input, NUM_CINEON_LINEAR, sizeof(alwan_scalar), sizeof(alwan_scalar));
    alwan_eotf_apply(roundtrip, ALWAN_TF_CINEON, encoded, NUM_CINEON_LINEAR, sizeof(alwan_scalar), sizeof(alwan_scalar));

    /* Check roundtrip accuracy */
    for (size_t i = 0; i < NUM_CINEON_LINEAR; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Cineon roundtrip [%zu]: original=%.4f", i, (double)cineon_linear_input[i]);
        TEST_ASSERT_REL(roundtrip[i], cineon_linear_input[i], TEST_TOLERANCE_REL, msg);
    }

    printf("    PASS: %zu roundtrip tests\n", NUM_CINEON_LINEAR);
    return 0;
}

static int test_cct_hernandez(void) {
    printf("  Testing CCT Hernandez 1999...\n");

    for (size_t i = 0; i < NUM_HERNANDEZ_XY; i++) {
        alwan_vec2 xy;
        xy.v[0] = hernandez_xy_input[i * 2];
        xy.v[1] = hernandez_xy_input[i * 2 + 1];

        alwan_scalar cct = alwan_cct_hernandez_xy(&xy);

        char msg[128];
        snprintf(msg, sizeof(msg), "Hernandez CCT [%zu]: xy=(%.4f, %.4f)",
                 i, (double)xy.v[0], (double)xy.v[1]);
        TEST_ASSERT_REL(cct, hernandez_cct_expected[i], TEST_TOLERANCE_REL, msg);
    }

    printf("    PASS: %zu Hernandez tests\n", NUM_HERNANDEZ_XY);
    return 0;
}

static int test_cct_kang_forward(void) {
    printf("  Testing CCT Kang 2002 (CCT to xy)...\n");

    for (size_t i = 0; i < NUM_KANG_CCT; i++) {
        alwan_scalar cct = kang_cct_input[i];
        alwan_vec2 xy;
        alwan_cct_to_xy_kang(&xy, cct);

        alwan_scalar expected_x = kang_xy_expected[i * 2];
        alwan_scalar expected_y = kang_xy_expected[i * 2 + 1];

        char msg_x[128], msg_y[128];
        snprintf(msg_x, sizeof(msg_x), "Kang xy.x [%zu]: CCT=%.0fK", i, (double)cct);
        snprintf(msg_y, sizeof(msg_y), "Kang xy.y [%zu]: CCT=%.0fK", i, (double)cct);

        TEST_ASSERT_REL(xy.v[0], expected_x, TEST_TOLERANCE_REL, msg_x);
        TEST_ASSERT_REL(xy.v[1], expected_y, TEST_TOLERANCE_REL, msg_y);
    }

    printf("    PASS: %zu Kang forward tests\n", NUM_KANG_CCT);
    return 0;
}

static int test_cct_kang_inverse(void) {
    printf("  Testing CCT Kang 2002 (xy to CCT)...\n");

    /* Test inverse by converting CCT -> xy -> CCT */
    for (size_t i = 0; i < NUM_KANG_CCT; i++) {
        alwan_scalar original_cct = kang_cct_input[i];

        /* Skip out of range values (Kang valid: 1667-25000K) */
        if (original_cct < 1667.0 || original_cct > 25000.0) continue;

        /* Forward: CCT -> xy */
        alwan_vec2 xy;
        alwan_cct_to_xy_kang(&xy, original_cct);

        /* Inverse: xy -> CCT */
        alwan_scalar recovered_cct = alwan_cct_kang_xy(&xy);

        char msg[128];
        snprintf(msg, sizeof(msg), "Kang inverse [%zu]: original CCT=%.0fK", i, (double)original_cct);

        /* Use absolute tolerance for CCT (iterative method) */
        TEST_ASSERT_ABS(recovered_cct, original_cct, TEST_TOLERANCE_CCT, msg);
    }

    printf("    PASS: Kang inverse tests\n");
    return 0;
}

static int test_cct_methods_comparison(void) {
    printf("  Testing CCT method comparison (D65)...\n");

    /* D65 chromaticity */
    alwan_vec2 d65 = {{0.31270, 0.32900}};

    /* Get CCT from each method */
    alwan_scalar cct_mccamy = alwan_cct_mccamy_xy(&d65);
    alwan_scalar cct_robertson = alwan_cct_robertson_xy(&d65);
    alwan_scalar cct_hernandez = alwan_cct_hernandez_xy(&d65);
    alwan_scalar cct_kang = alwan_cct_kang_xy(&d65);

    printf("    D65 CCT results:\n");
    printf("      McCamy:    %.2f K\n", (double)cct_mccamy);
    printf("      Robertson: %.2f K\n", (double)cct_robertson);
    printf("      Hernandez: %.2f K\n", (double)cct_hernandez);
    printf("      Kang:      %.2f K\n", (double)cct_kang);

    /* All methods should be close to 6500K for D65 */
    alwan_scalar expected_d65 = 6500.0;
    alwan_scalar tolerance = 150.0;  /* 150K tolerance for D65 (Kang method varies more) */

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
