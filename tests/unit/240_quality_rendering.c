/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 240: P4 Light Quality & Rendering Metrics
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

/* Quality metrics can vary based on implementation details
 * Using generous tolerance for cross-library validation */
#define QUALITY_METRIC_TOL ALWAN_LITERAL(5.0)

/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */

static int test_cri_d65(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    alwan_spd d65_spd;
    int status = alwan_spd_illuminant(ctx, "D65", &d65_spd);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create D65 SPD");

    alwan_scalar ra = alwan_cri_ra(ctx, &d65_spd);
    printf("  D65 CRI Ra = %.2f\n", ra);
    TEST_ASSERT(ra >= ALWAN_LITERAL(95.0) && ra <= ALWAN_LITERAL(105.0), "D65 CRI Ra should be ~100");

    alwan_spd_destroy(ctx, &d65_spd);
    alwan_destroy(ctx);
    TEST_PASS("CRI Ra for D65 illuminant");
}

static int test_cri_blackbody(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    /* Test a few blackbody temperatures */
    alwan_scalar test_ccts[] = {
        ALWAN_LITERAL(2700.0),
        ALWAN_LITERAL(4000.0),
        ALWAN_LITERAL(6500.0)
    };
    int num_tests = sizeof(test_ccts) / sizeof(test_ccts[0]);

    for (int i = 0; i < num_tests; i++) {
        alwan_spd bb_spd;
        int status = alwan_spd_blackbody(ctx, test_ccts[i], ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 95, &bb_spd);
        TEST_ASSERT(status == ALWAN_OK, "Failed to create blackbody SPD");

        alwan_scalar ra = alwan_cri_ra(ctx, &bb_spd);
        printf("  BB %.0fK CRI Ra = %.2f\n", test_ccts[i], ra);
        /* Blackbody radiators should have CRI ~100 */
        TEST_ASSERT(ra >= ALWAN_LITERAL(95.0) && ra <= ALWAN_LITERAL(105.0),
                    "Blackbody CRI Ra should be ~100");

        alwan_spd_destroy(ctx, &bb_spd);
    }

    alwan_destroy(ctx);
    TEST_PASS("CRI Ra for blackbody illuminants");
}

static int test_cri_illuminant_a(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    alwan_spd a_spd;
    int status = alwan_spd_illuminant(ctx, "A", &a_spd);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create illuminant A SPD");

    alwan_scalar ra = alwan_cri_ra(ctx, &a_spd);
    printf("  Illuminant A CRI Ra = %.2f\n", ra);
    /* Illuminant A is the reference for CRI, should be 100 */
    TEST_ASSERT(ra >= ALWAN_LITERAL(95.0) && ra <= ALWAN_LITERAL(105.0),
                "Illuminant A CRI Ra should be ~100");

    alwan_spd_destroy(ctx, &a_spd);
    alwan_destroy(ctx);
    TEST_PASS("CRI Ra for illuminant A");
}

static int test_cqs_d65(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    alwan_spd d65_spd;
    int status = alwan_spd_illuminant(ctx, "D65", &d65_spd);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create D65 SPD");

    alwan_scalar cqs = alwan_cqs_calculate(ctx, &d65_spd);
    printf("  D65 CQS = %.2f\n", cqs);
    TEST_ASSERT(cqs >= ALWAN_LITERAL(95.0) && cqs <= ALWAN_LITERAL(105.0),
                "D65 CQS should be ~100");

    alwan_spd_destroy(ctx, &d65_spd);
    alwan_destroy(ctx);
    TEST_PASS("CQS for D65 illuminant");
}

static int test_cqs_blackbody(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    alwan_spd bb_spd;
    int status = alwan_spd_blackbody(ctx, ALWAN_LITERAL(6500.0), ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 95, &bb_spd);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create blackbody SPD");

    alwan_scalar cqs = alwan_cqs_calculate(ctx, &bb_spd);
    printf("  BB 6500K CQS = %.2f\n", cqs);
    /* Blackbody should have high CQS */
    TEST_ASSERT(cqs >= ALWAN_LITERAL(90.0) && cqs <= ALWAN_LITERAL(105.0),
                "Blackbody CQS should be high");

    alwan_spd_destroy(ctx, &bb_spd);
    alwan_destroy(ctx);
    TEST_PASS("CQS for blackbody illuminant");
}

static int test_tm30_d65(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    alwan_spd d65_spd;
    int status = alwan_spd_illuminant(ctx, "D65", &d65_spd);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create D65 SPD");

    alwan_scalar rf = alwan_tm30_rf(ctx, &d65_spd);
    TEST_ASSERT(rf >= ALWAN_LITERAL(95.0) && rf <= ALWAN_LITERAL(105.0),
                "D65 TM-30 Rf should be ~100");

    printf("  D65 TM-30 Rf = %.2f\n", rf);

    alwan_spd_destroy(ctx, &d65_spd);
    alwan_destroy(ctx);
    TEST_PASS("TM-30 Rf for D65 illuminant");
}

static int test_tm30_blackbody(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    alwan_spd bb_spd;
    int status = alwan_spd_blackbody(ctx, ALWAN_LITERAL(4000.0), ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 95, &bb_spd);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create blackbody SPD");

    alwan_scalar rf = alwan_tm30_rf(ctx, &bb_spd);
    printf("  BB 4000K TM-30 Rf = %.2f\n", rf);
    /* Blackbody should have high TM-30 */
    TEST_ASSERT(rf >= ALWAN_LITERAL(95.0) && rf <= ALWAN_LITERAL(105.0),
                "Blackbody TM-30 Rf should be ~100");

    alwan_spd_destroy(ctx, &bb_spd);
    alwan_destroy(ctx);
    TEST_PASS("TM-30 Rf for blackbody illuminant");
}

static int test_cie224_d65(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    alwan_spd d65_spd;
    int status = alwan_spd_illuminant(ctx, "D65", &d65_spd);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create D65 SPD");

    alwan_scalar rf = alwan_cie224_rf(ctx, &d65_spd);
    TEST_ASSERT(rf >= ALWAN_LITERAL(95.0) && rf <= ALWAN_LITERAL(105.0),
                "D65 CIE 224 Rf should be ~100");

    printf("  D65 CIE 224:2017 Rf = %.2f\n", rf);

    alwan_spd_destroy(ctx, &d65_spd);
    alwan_destroy(ctx);
    TEST_PASS("CIE 224:2017 Rf for D65 illuminant");
}

static int test_tm30_cie224_match(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    alwan_spd d65_spd;
    int status = alwan_spd_illuminant(ctx, "D65", &d65_spd);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create D65 SPD");

    alwan_scalar tm30 = alwan_tm30_rf(ctx, &d65_spd);
    alwan_scalar cie224 = alwan_cie224_rf(ctx, &d65_spd);

    alwan_scalar diff = ALWAN_FABS(tm30 - cie224);
    TEST_ASSERT(diff < ALWAN_LITERAL(0.01),
                "TM-30 and CIE 224 should return identical values");

    printf("  TM-30 Rf = %.4f, CIE 224 Rf = %.4f, diff = %.6f\n",
           tm30, cie224, diff);

    alwan_spd_destroy(ctx, &d65_spd);
    alwan_destroy(ctx);
    TEST_PASS("TM-30 and CIE 224:2017 return same values");
}

static int test_cri_null_inputs(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    alwan_scalar result = alwan_cri_ra(ctx, NULL);
    TEST_ASSERT(result < ALWAN_LITERAL(0.0),
                "CRI should return error for NULL SPD input");

    result = alwan_cri_ra(NULL, NULL);
    TEST_ASSERT(result < ALWAN_LITERAL(0.0),
                "CRI should return error for NULL context");

    alwan_destroy(ctx);
    TEST_PASS("CRI null input handling");
}

static int test_cqs_null_inputs(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    alwan_scalar result = alwan_cqs_calculate(ctx, NULL);
    TEST_ASSERT(result < ALWAN_LITERAL(0.0),
                "CQS should return error for NULL SPD input");

    result = alwan_cqs_calculate(NULL, NULL);
    TEST_ASSERT(result < ALWAN_LITERAL(0.0),
                "CQS should return error for NULL context");

    alwan_destroy(ctx);
    TEST_PASS("CQS null input handling");
}

static int test_tm30_null_inputs(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    alwan_scalar result = alwan_tm30_rf(ctx, NULL);
    TEST_ASSERT(result < ALWAN_LITERAL(0.0),
                "TM-30 should return error for NULL SPD input");

    result = alwan_tm30_rf(NULL, NULL);
    TEST_ASSERT(result < ALWAN_LITERAL(0.0),
                "TM-30 should return error for NULL context");

    alwan_destroy(ctx);
    TEST_PASS("TM-30 null input handling");
}

static int test_cie224_null_inputs(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    alwan_scalar result = alwan_cie224_rf(ctx, NULL);
    TEST_ASSERT(result < ALWAN_LITERAL(0.0),
                "CIE 224 should return error for NULL SPD input");

    result = alwan_cie224_rf(NULL, NULL);
    TEST_ASSERT(result < ALWAN_LITERAL(0.0),
                "CIE 224 should return error for NULL context");

    alwan_destroy(ctx);
    TEST_PASS("CIE 224:2017 null input handling");
}

/* ----------------------------------------------------------------
 * Test registration
 * ---------------------------------------------------------------- */

static int (*tests[])(void) = {
    test_cri_d65,
    test_cri_blackbody,
    test_cri_illuminant_a,
    test_cqs_d65,
    test_cqs_blackbody,
    test_tm30_d65,
    test_tm30_blackbody,
    test_cie224_d65,
    test_tm30_cie224_match,
    test_cri_null_inputs,
    test_cqs_null_inputs,
    test_tm30_null_inputs,
    test_cie224_null_inputs,
    NULL
};

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_240_quality_rendering_main(void) {
    printf("=== P4 Light Quality & Rendering Metrics Tests ===\n");

    int passed = 0;
    int total = 0;

    for (int i = 0; tests[i] != NULL; i++) {
        total++;
        if (tests[i]() == 0) {
            passed++;
        }
    }

    if (passed == total) {
        printf("\n=== All P4 quality tests passed (%d/%d) ===\n", passed, total);
        return 0;
    } else {
        fprintf(stderr, "\n=== %d/%d P4 quality test(s) failed ===\n",
                total - passed, total);
        return 1;
    }
}
