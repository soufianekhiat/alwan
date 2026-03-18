/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 65: Hero Wavelength Sampling & Wyman 2013 CMF Fit
 */

#include "test_common.h"
#include <stdlib.h>

#include "core/alwan_hero_wavelength_core.h"

/* ----------------------------------------------------------------
 * Wyman CMF fit: basic sanity at key wavelengths
 * ---------------------------------------------------------------- */

static int test_wyman_cmf_sanity(void) {
    /* At 555 nm (peak of ybar), ybar should be near 1.0 */
    alwan_f64 ybar_555 = alwan_wyman_ybar_f64_v(ALWAN_LITERAL(555.0));
    TEST_ASSERT(ybar_555 > ALWAN_LITERAL(0.8), "ybar(555) should be near 1.0");
    TEST_ASSERT(ybar_555 < ALWAN_LITERAL(1.1), "ybar(555) upper bound");

    /* At 440 nm (peak of zbar), zbar should be large */
    alwan_f64 zbar_440 = alwan_wyman_zbar_f64_v(ALWAN_LITERAL(440.0));
    TEST_ASSERT(zbar_440 > ALWAN_LITERAL(1.0), "zbar(440) should be > 1.0");

    /* At 600 nm (peak of xbar), xbar should be large */
    alwan_f64 xbar_600 = alwan_wyman_xbar_f64_v(ALWAN_LITERAL(600.0));
    TEST_ASSERT(xbar_600 > ALWAN_LITERAL(0.8), "xbar(600) should be > 0.8");

    /* Outside visible range, CMFs should be near zero */
    alwan_f64 xbar_380 = alwan_wyman_xbar_f64_v(ALWAN_LITERAL(380.0));
    alwan_f64 ybar_380 = alwan_wyman_ybar_f64_v(ALWAN_LITERAL(380.0));
    TEST_ASSERT(xbar_380 < ALWAN_LITERAL(0.1), "xbar(380) near zero");
    TEST_ASSERT(ybar_380 < ALWAN_LITERAL(0.1), "ybar(380) near zero");

    TEST_PASS("wyman cmf sanity");
}

/* ----------------------------------------------------------------
 * Hero wavelength sample: range check
 * ---------------------------------------------------------------- */

static int test_hero_sample_range(void) {
    /* u=0 should give 380 nm */
    alwan_f64 lambda_0 = alwan_hero_wavelength_sample_f64_v(ALWAN_LITERAL(0.0));
    TEST_ASSERT_NEAR(lambda_0, ALWAN_LITERAL(380.0), ALWAN_LITERAL(1e-10),
                     "hero sample u=0");

    /* u=1 should give 780 nm */
    alwan_f64 lambda_1 = alwan_hero_wavelength_sample_f64_v(ALWAN_LITERAL(1.0));
    TEST_ASSERT_NEAR(lambda_1, ALWAN_LITERAL(780.0), ALWAN_LITERAL(1e-10),
                     "hero sample u=1");

    /* u=0.5 should give 580 nm */
    alwan_f64 lambda_mid = alwan_hero_wavelength_sample_f64_v(ALWAN_LITERAL(0.5));
    TEST_ASSERT_NEAR(lambda_mid, ALWAN_LITERAL(580.0), ALWAN_LITERAL(1e-10),
                     "hero sample u=0.5");

    TEST_PASS("hero sample range");
}

/* ----------------------------------------------------------------
 * Hero wavelength PDF
 * ---------------------------------------------------------------- */

static int test_hero_pdf(void) {
    alwan_f64 pdf = alwan_hero_wavelength_pdf_f64_v(ALWAN_LITERAL(550.0));
    TEST_ASSERT_NEAR(pdf, ALWAN_ONE / ALWAN_LITERAL(400.0), ALWAN_LITERAL(1e-10),
                     "hero pdf = 1/400");

    TEST_PASS("hero pdf");
}

/* ----------------------------------------------------------------
 * Stratified sampling: wrapping correctness
 * ---------------------------------------------------------------- */

static int test_hero_stratified(void) {
    alwan_f64 hero = ALWAN_LITERAL(500.0);
    int N = 4;

    for (int i = 0; i < N; i++) {
        alwan_f64 lambda = alwan_hero_wavelength_stratified_f64_v(hero, i, N);
        TEST_ASSERT(lambda >= ALWAN_LITERAL(380.0) && lambda <= ALWAN_LITERAL(780.0),
                    "stratified wavelength in range");
    }

    /* First sample should be the hero itself */
    alwan_f64 first = alwan_hero_wavelength_stratified_f64_v(hero, 0, N);
    TEST_ASSERT_NEAR(first, hero, ALWAN_LITERAL(1e-10), "stratified i=0 == hero");

    TEST_PASS("hero stratified");
}

/* ----------------------------------------------------------------
 * Wavelength to XYZ via Wyman fit
 * ---------------------------------------------------------------- */

static int test_hero_to_xyz(void) {
    alwan_xyz xyz = alwan_hero_wavelength_to_xyz_f64_v(ALWAN_LITERAL(555.0));

    /* At 555 nm, Y should be dominant */
    TEST_ASSERT(xyz.y > ALWAN_LITERAL(0.8), "hero_to_xyz Y(555) > 0.8");
    TEST_ASSERT(xyz.x >= ALWAN_ZERO, "hero_to_xyz X >= 0");
    TEST_ASSERT(xyz.z >= ALWAN_ZERO, "hero_to_xyz Z >= 0");

    TEST_PASS("hero to xyz");
}

/* ----------------------------------------------------------------
 * Hero wavelength API wrappers
 * ---------------------------------------------------------------- */

static int test_hero_api(void) {
    alwan_f64 lambda;
    int status = alwan_hero_wavelength_sample(&lambda, ALWAN_LITERAL(0.5));
    TEST_ASSERT(status == ALWAN_OK, "hero sample api failed");
    TEST_ASSERT_NEAR(lambda, ALWAN_LITERAL(580.0), ALWAN_LITERAL(1e-10),
                     "hero sample api value");

    alwan_xyz xyz;
    alwan_hero_wavelength_to_xyz(&xyz, ALWAN_LITERAL(555.0));
    TEST_ASSERT(xyz.y > ALWAN_LITERAL(0.8), "hero to_xyz api Y");

    /* Batch */
    alwan_f64 lambdas[4];
    alwan_xyz weights[4];
    status = alwan_hero_wavelength_batch(lambdas, weights, 4, ALWAN_LITERAL(0.3));
    TEST_ASSERT(status == ALWAN_OK, "hero batch api failed");
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT(lambdas[i] >= ALWAN_LITERAL(380.0) && lambdas[i] <= ALWAN_LITERAL(780.0),
                    "hero batch lambda in range");
    }

    TEST_PASS("hero api");
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */

int test_65_hero_wavelength_main(void) {
    int failures = 0;

    failures += test_wyman_cmf_sanity();
    failures += test_hero_sample_range();
    failures += test_hero_pdf();
    failures += test_hero_stratified();
    failures += test_hero_to_xyz();
    failures += test_hero_api();

    if (failures == 0) {
        printf("\n=== All hero wavelength tests passed ===\n");
        return 0;
    } else {
        printf("\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
