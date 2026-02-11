/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 32: P4 Light Quality & Rendering Metrics
 */

#include "test_common.h"
#include <stdlib.h>

/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */

static int test_cri_d65(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    alwan_spd d65_spd;
    int status = alwan_spd_illuminant(&d65_spd, ctx, ALWAN_ILLUMINANT_D65);
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
        int status = alwan_spd_blackbody(&bb_spd, ctx, test_ccts[i], ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 95);
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
    int status = alwan_spd_illuminant(&a_spd, ctx, ALWAN_ILLUMINANT_A);
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
    int status = alwan_spd_illuminant(&d65_spd, ctx, ALWAN_ILLUMINANT_D65);
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
    int status = alwan_spd_blackbody(&bb_spd, ctx, ALWAN_LITERAL(6500.0), ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 95);
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
    int status = alwan_spd_illuminant(&d65_spd, ctx, ALWAN_ILLUMINANT_D65);
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
    int status = alwan_spd_blackbody(&bb_spd, ctx, ALWAN_LITERAL(4000.0), ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 95);
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
    int status = alwan_spd_illuminant(&d65_spd, ctx, ALWAN_ILLUMINANT_D65);
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
    int status = alwan_spd_illuminant(&d65_spd, ctx, ALWAN_ILLUMINANT_D65);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create D65 SPD");

    alwan_scalar tm30 = alwan_tm30_rf(ctx, &d65_spd);
    alwan_scalar cie224 = alwan_cie224_rf(ctx, &d65_spd);

    alwan_scalar diff = ALWAN_ABS(tm30 - cie224);
    TEST_ASSERT(diff < ALWAN_LITERAL(0.01),
                "TM-30 and CIE 224 should return identical values");

    printf("  TM-30 Rf = %.4f, CIE 224 Rf = %.4f, diff = %.6f\n",
           tm30, cie224, diff);

    alwan_spd_destroy(ctx, &d65_spd);
    alwan_destroy(ctx);
    TEST_PASS("TM-30 and CIE 224:2017 return same values");
}

/* SSI (Spectral Similarity Index) Tests */

static int test_ssi_perfect_match(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    alwan_spd d65_spd1, d65_spd2;
    int status = alwan_spd_illuminant(&d65_spd1, ctx, ALWAN_ILLUMINANT_D65);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create D65 SPD");
    status = alwan_spd_illuminant(&d65_spd2, ctx, ALWAN_ILLUMINANT_D65);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create D65 SPD");

    alwan_scalar ssi = alwan_ssi_calculate(ctx, &d65_spd1, &d65_spd2);
    printf("  D65 vs D65 SSI = %.2f\n", ssi);
    TEST_ASSERT(ssi >= ALWAN_LITERAL(99.0) && ssi <= ALWAN_LITERAL(101.0),
                "Perfect match SSI should be ~100");

    alwan_spd_destroy(ctx, &d65_spd1);
    alwan_spd_destroy(ctx, &d65_spd2);
    alwan_destroy(ctx);
    TEST_PASS("SSI perfect match (D65 vs D65)");
}

static int test_ssi_illuminant_pairs(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    /* Test D65 vs D50 */
    alwan_spd d65_spd, d50_spd;
    int status = alwan_spd_illuminant(&d65_spd, ctx, ALWAN_ILLUMINANT_D65);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create D65 SPD");
    status = alwan_spd_illuminant(&d50_spd, ctx, ALWAN_ILLUMINANT_D50);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create D50 SPD");

    alwan_scalar ssi_d65_d50 = alwan_ssi_calculate(ctx, &d65_spd, &d50_spd);
    printf("  D65 vs D50 SSI = %.2f\n", ssi_d65_d50);
    TEST_ASSERT(ssi_d65_d50 >= ALWAN_LITERAL(75.0) && ssi_d65_d50 <= ALWAN_LITERAL(95.0),
                "D65 vs D50 SSI should be ~84");

    /* Test D65 vs A */
    alwan_spd a_spd;
    status = alwan_spd_illuminant(&a_spd, ctx, ALWAN_ILLUMINANT_A);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create A SPD");

    alwan_scalar ssi_d65_a = alwan_ssi_calculate(ctx, &d65_spd, &a_spd);
    printf("  D65 vs A SSI = %.2f\n", ssi_d65_a);
    TEST_ASSERT(ssi_d65_a >= ALWAN_LITERAL(30.0) && ssi_d65_a <= ALWAN_LITERAL(70.0),
                "D65 vs A SSI should show low similarity");

    /* Test BB6500K vs D65 */
    alwan_spd bb_spd;
    status = alwan_spd_blackbody(&bb_spd, ctx, ALWAN_LITERAL(6500.0), ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 95);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create blackbody SPD");

    alwan_scalar ssi_bb_d65 = alwan_ssi_calculate(ctx, &bb_spd, &d65_spd);
    printf("  BB6500K vs D65 SSI = %.2f\n", ssi_bb_d65);
    TEST_ASSERT(ssi_bb_d65 >= ALWAN_LITERAL(85.0) && ssi_bb_d65 <= ALWAN_LITERAL(95.0),
                "BB6500K vs D65 SSI should be ~91");

    alwan_spd_destroy(ctx, &d65_spd);
    alwan_spd_destroy(ctx, &d50_spd);
    alwan_spd_destroy(ctx, &a_spd);
    alwan_spd_destroy(ctx, &bb_spd);
    alwan_destroy(ctx);
    TEST_PASS("SSI for various illuminant pairs");
}

/* Metamerism Index Tests */

static int test_metamerism_basic(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    /* Create two simple flat reflectance spectra */
    alwan_spd refl1, refl2;
    int status = alwan_spd_create(&refl1, ctx, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 95);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create reflectance SPD 1");
    status = alwan_spd_create(&refl2, ctx, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 95);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create reflectance SPD 2");

    /* Set flat reflectance of 0.5 for both (gray) */
    for (size_t i = 0; i < 95; i++) {
        refl1.values[i] = ALWAN_LITERAL(0.5);
        refl2.values[i] = ALWAN_LITERAL(0.5);
    }

    /* Get illuminants */
    alwan_spd d65_spd, a_spd;
    status = alwan_spd_illuminant(&d65_spd, ctx, ALWAN_ILLUMINANT_D65);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create D65 SPD");
    status = alwan_spd_illuminant(&a_spd, ctx, ALWAN_ILLUMINANT_A);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create A SPD");

    /* Calculate metamerism index (same reflectance, different illuminant) */
    alwan_scalar mi = alwan_metamerism_index(ctx, &refl1, &refl2, &d65_spd, &a_spd,
                                             ALWAN_OBSERVER_CIE_1931_2DEG);
    printf("  Same gray reflectance, D65->A: MI = %.2f\n", mi);

    /* For identical reflectances, MI should be near zero regardless of illuminant */
    TEST_ASSERT(mi >= ALWAN_LITERAL(0.0) && mi <= ALWAN_LITERAL(0.5),
                "Identical reflectances should have MI ~0");

    alwan_spd_destroy(ctx, &refl1);
    alwan_spd_destroy(ctx, &refl2);
    alwan_spd_destroy(ctx, &d65_spd);
    alwan_spd_destroy(ctx, &a_spd);
    alwan_destroy(ctx);
    TEST_PASS("Metamerism Index basic test");
}

static int test_metamerism_different_reflectances(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    /* Create two different reflectance spectra */
    alwan_spd refl1, refl2;
    int status = alwan_spd_create(&refl1, ctx, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 95);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create reflectance SPD 1");
    status = alwan_spd_create(&refl2, ctx, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 95);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create reflectance SPD 2");

    /* Set different reflectances */
    for (size_t i = 0; i < 95; i++) {
        refl1.values[i] = ALWAN_LITERAL(0.3);  /* Darker gray */
        refl2.values[i] = ALWAN_LITERAL(0.7);  /* Lighter gray */
    }

    /* Get illuminants */
    alwan_spd d65_spd, a_spd;
    status = alwan_spd_illuminant(&d65_spd, ctx, ALWAN_ILLUMINANT_D65);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create D65 SPD");
    status = alwan_spd_illuminant(&a_spd, ctx, ALWAN_ILLUMINANT_A);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create A SPD");

    /* Calculate metamerism index */
    alwan_scalar mi = alwan_metamerism_index(ctx, &refl1, &refl2, &d65_spd, &a_spd,
                                             ALWAN_OBSERVER_CIE_1931_2DEG);
    printf("  Dark vs light gray, D65->A: MI = %.2f\n", mi);

    /* Different reflectances should have significant MI */
    TEST_ASSERT(mi > ALWAN_LITERAL(10.0),
                "Different reflectances should have measurable MI");

    alwan_spd_destroy(ctx, &refl1);
    alwan_spd_destroy(ctx, &refl2);
    alwan_spd_destroy(ctx, &d65_spd);
    alwan_spd_destroy(ctx, &a_spd);
    alwan_destroy(ctx);
    TEST_PASS("Metamerism Index with different reflectances");
}

static int test_ssi_null_inputs(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    alwan_spd d65_spd;
    int status = alwan_spd_illuminant(&d65_spd, ctx, ALWAN_ILLUMINANT_D65);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create D65 SPD");

    alwan_scalar result = alwan_ssi_calculate(ctx, NULL, &d65_spd);
    TEST_ASSERT(result < ALWAN_LITERAL(0.0),
                "SSI should return error for NULL test SPD");

    result = alwan_ssi_calculate(ctx, &d65_spd, NULL);
    TEST_ASSERT(result < ALWAN_LITERAL(0.0),
                "SSI should return error for NULL reference SPD");

    result = alwan_ssi_calculate(NULL, &d65_spd, &d65_spd);
    TEST_ASSERT(result < ALWAN_LITERAL(0.0),
                "SSI should return error for NULL context");

    alwan_spd_destroy(ctx, &d65_spd);
    alwan_destroy(ctx);
    TEST_PASS("SSI null input handling");
}

static int test_metamerism_null_inputs(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    alwan_spd refl_spd, d65_spd;
    int status = alwan_spd_create(&refl_spd, ctx, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 95);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create reflectance SPD");
    status = alwan_spd_illuminant(&d65_spd, ctx, ALWAN_ILLUMINANT_D65);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create D65 SPD");

    alwan_scalar result = alwan_metamerism_index(ctx, NULL, &refl_spd, &d65_spd, &d65_spd,
                                                 ALWAN_OBSERVER_CIE_1931_2DEG);
    TEST_ASSERT(result < ALWAN_LITERAL(0.0),
                "Metamerism should return error for NULL sample reflectance");

    result = alwan_metamerism_index(ctx, &refl_spd, NULL, &d65_spd, &d65_spd,
                                   ALWAN_OBSERVER_CIE_1931_2DEG);
    TEST_ASSERT(result < ALWAN_LITERAL(0.0),
                "Metamerism should return error for NULL reference reflectance");

    result = alwan_metamerism_index(NULL, &refl_spd, &refl_spd, &d65_spd, &d65_spd,
                                   ALWAN_OBSERVER_CIE_1931_2DEG);
    TEST_ASSERT(result < ALWAN_LITERAL(0.0),
                "Metamerism should return error for NULL context");

    alwan_spd_destroy(ctx, &refl_spd);
    alwan_spd_destroy(ctx, &d65_spd);
    alwan_destroy(ctx);
    TEST_PASS("Metamerism null input handling");
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
    test_ssi_perfect_match,
    test_ssi_illuminant_pairs,
    test_metamerism_basic,
    test_metamerism_different_reflectances,
    test_cri_null_inputs,
    test_cqs_null_inputs,
    test_tm30_null_inputs,
    test_cie224_null_inputs,
    test_ssi_null_inputs,
    test_metamerism_null_inputs,
    NULL
};

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_32_quality_rendering_main(void) {
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
        printf("\n=== %d/%d P4 quality test(s) failed ===\n",
                total - passed, total);
        return 1;
    }
}
