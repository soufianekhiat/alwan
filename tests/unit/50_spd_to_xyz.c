/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 *
 * Test 50: Spectral Power Distributions (SPD → XYZ)
 */

#include "../../src/alwan/alwan.h"
#include "../../src/alwan/alwan_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static int test_spd_create_destroy(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Context creation failed");

    alwan_spd spd;
    int status = alwan_spd_create(ctx, ALWAN_LITERAL(400.0), ALWAN_LITERAL(700.0), 31, &spd);
    TEST_ASSERT(status == ALWAN_OK, "SPD creation failed");
    TEST_ASSERT(spd.values != NULL, "SPD values not allocated");
    TEST_ASSERT(spd.count == 31, "SPD count mismatch");
    TEST_ASSERT(spd.wavelength_min == ALWAN_LITERAL(400.0), "SPD min wavelength mismatch");
    TEST_ASSERT(spd.wavelength_max == ALWAN_LITERAL(700.0), "SPD max wavelength mismatch");

    /* Fill with test data */
    for (size_t i = 0; i < spd.count; i++) {
        spd.values[i] = ALWAN_LITERAL(1.0);
    }

    alwan_spd_destroy(ctx, &spd);
    TEST_ASSERT(spd.values == NULL, "SPD not properly destroyed");

    alwan_destroy(ctx);
    TEST_PASS("SPD create/destroy");
}

static int test_illuminant_loading(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Context creation failed");

    /* Test standard illuminants */
    char const *illuminants[] = {"A", "D50", "D55", "D65", "E", "F1", "F2"};
    size_t const num_illuminants = sizeof(illuminants) / sizeof(illuminants[0]);

    for (size_t i = 0; i < num_illuminants; i++) {
        alwan_spd illum;
        int status = alwan_spd_illuminant(ctx, illuminants[i], &illum);

        if (status != ALWAN_OK) {
            printf("  Warning: Could not load illuminant '%s'\n", illuminants[i]);
            continue;
        }

        TEST_ASSERT(illum.values != NULL, "Illuminant values not loaded");
        TEST_ASSERT(illum.count == 471, "Illuminant count should be 471");
        TEST_ASSERT(illum.wavelength_min == ALWAN_LITERAL(360.0), "Illuminant min wavelength");
        TEST_ASSERT(illum.wavelength_max == ALWAN_LITERAL(830.0), "Illuminant max wavelength");

        /* Check that at least some values are non-zero */
        int has_nonzero = 0;
        for (size_t j = 0; j < illum.count; j++) {
            if (illum.values[j] > ALWAN_LITERAL(0.0)) {
                has_nonzero = 1;
                break;
            }
        }
        TEST_ASSERT(has_nonzero, "Illuminant has no non-zero values");

        alwan_spd_destroy(ctx, &illum);
    }

    alwan_destroy(ctx);
    TEST_PASS("Illuminant loading");
}

static int test_spd_resampling(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Context creation failed");

    /* Create source SPD (400-700nm, 10nm steps) */
    alwan_spd src;
    int status = alwan_spd_create(ctx, ALWAN_LITERAL(400.0), ALWAN_LITERAL(700.0), 31, &src);
    TEST_ASSERT(status == ALWAN_OK, "Source SPD creation failed");

    /* Fill with simple ramp */
    for (size_t i = 0; i < src.count; i++) {
        src.values[i] = (Scalar)i / (Scalar)(src.count - 1);
    }

    /* Resample to 420-680nm, 5nm steps (linear) */
    alwan_spd dst_linear;
    status = alwan_spd_resample(ctx, &src,
                                ALWAN_LITERAL(420.0), ALWAN_LITERAL(680.0), 53,
                                ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO, &dst_linear);
    TEST_ASSERT(status == ALWAN_OK, "Linear resampling failed");
    TEST_ASSERT(dst_linear.count == 53, "Resampled count mismatch");

    /* Resample with Catmull-Rom */
    alwan_spd dst_catmull;
    status = alwan_spd_resample(ctx, &src,
                                ALWAN_LITERAL(420.0), ALWAN_LITERAL(680.0), 53,
                                ALWAN_RESAMPLE_CATMULL_ROM, ALWAN_EXTRAPOLATE_ZERO, &dst_catmull);
    TEST_ASSERT(status == ALWAN_OK, "Catmull-Rom resampling failed");

    /* Check that both methods produced values in reasonable range */
    for (size_t i = 0; i < dst_linear.count; i++) {
        TEST_ASSERT(dst_linear.values[i] >= ALWAN_LITERAL(0.0) &&
                   dst_linear.values[i] <= ALWAN_LITERAL(1.0),
                   "Linear resampled value out of range");
        TEST_ASSERT(dst_catmull.values[i] >= ALWAN_LITERAL(-0.1) &&
                   dst_catmull.values[i] <= ALWAN_LITERAL(1.1),
                   "Catmull-Rom resampled value out of range");
    }

    alwan_spd_destroy(ctx, &src);
    alwan_spd_destroy(ctx, &dst_linear);
    alwan_spd_destroy(ctx, &dst_catmull);
    alwan_destroy(ctx);
    TEST_PASS("SPD resampling");
}

static int test_xyz_from_constant_spd(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Context creation failed");

    /* Create constant SPD (all values = 1.0) */
    alwan_spd reflectance;
    int status = alwan_spd_create(ctx, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 471, &reflectance);
    TEST_ASSERT(status == ALWAN_OK, "SPD creation failed");

    for (size_t i = 0; i < reflectance.count; i++) {
        reflectance.values[i] = ALWAN_LITERAL(1.0);
    }

    /* Load D65 illuminant */
    alwan_spd d65;
    status = alwan_spd_illuminant(ctx, "D65", &d65);
    TEST_ASSERT(status == ALWAN_OK, "D65 loading failed");

    /* Compute XYZ with both integration methods */
    alwan_vec3 xyz_trap, xyz_simp;

    status = alwan_xyz_from_spd(ctx, &reflectance, &d65,
                                ALWAN_OBSERVER_CIE_1931_2DEG,
                                ALWAN_INTEGRATE_TRAPEZOID,
                                ALWAN_LITERAL(0.0),  /* No bandpass correction */
                                &xyz_trap);
    TEST_ASSERT(status == ALWAN_OK, "XYZ integration (trapezoid) failed");

    status = alwan_xyz_from_spd(ctx, &reflectance, &d65,
                                ALWAN_OBSERVER_CIE_1931_2DEG,
                                ALWAN_INTEGRATE_SIMPSON,
                                ALWAN_LITERAL(0.0),  /* No bandpass correction */
                                &xyz_simp);
    TEST_ASSERT(status == ALWAN_OK, "XYZ integration (Simpson) failed");

    /* Check that XYZ values are positive and reasonable */
    TEST_ASSERT(xyz_trap.v[0] > ALWAN_LITERAL(0.0), "X (trap) should be positive");
    TEST_ASSERT(xyz_trap.v[1] > ALWAN_LITERAL(0.0), "Y (trap) should be positive");
    TEST_ASSERT(xyz_trap.v[2] > ALWAN_LITERAL(0.0), "Z (trap) should be positive");

    TEST_ASSERT(xyz_simp.v[0] > ALWAN_LITERAL(0.0), "X (Simpson) should be positive");
    TEST_ASSERT(xyz_simp.v[1] > ALWAN_LITERAL(0.0), "Y (Simpson) should be positive");
    TEST_ASSERT(xyz_simp.v[2] > ALWAN_LITERAL(0.0), "Z (Simpson) should be positive");

    /* Both methods should give similar results (within 10%) */
    Scalar diff_X = ALWAN_FABS(xyz_trap.v[0] - xyz_simp.v[0]) / xyz_trap.v[0];
    Scalar diff_Y = ALWAN_FABS(xyz_trap.v[1] - xyz_simp.v[1]) / xyz_trap.v[1];
    Scalar diff_Z = ALWAN_FABS(xyz_trap.v[2] - xyz_simp.v[2]) / xyz_trap.v[2];

    TEST_ASSERT(diff_X < ALWAN_LITERAL(0.1), "X values differ too much between methods");
    TEST_ASSERT(diff_Y < ALWAN_LITERAL(0.1), "Y values differ too much between methods");
    TEST_ASSERT(diff_Z < ALWAN_LITERAL(0.1), "Z values differ too much between methods");

    printf("  Trapezoid: XYZ = [%.4f, %.4f, %.4f]\n",
           xyz_trap.v[0], xyz_trap.v[1], xyz_trap.v[2]);
    printf("  Simpson:   XYZ = [%.4f, %.4f, %.4f]\n",
           xyz_simp.v[0], xyz_simp.v[1], xyz_simp.v[2]);

    alwan_spd_destroy(ctx, &reflectance);
    alwan_spd_destroy(ctx, &d65);
    alwan_destroy(ctx);
    TEST_PASS("XYZ from constant SPD");
}

static int test_xyz_both_observers(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Context creation failed");

    /* Create neutral reflectance SPD */
    alwan_spd reflectance;
    int status = alwan_spd_create(ctx, ALWAN_LITERAL(400.0), ALWAN_LITERAL(700.0), 31, &reflectance);
    TEST_ASSERT(status == ALWAN_OK, "SPD creation failed");

    for (size_t i = 0; i < reflectance.count; i++) {
        reflectance.values[i] = ALWAN_LITERAL(0.5);  /* 50% reflectance */
    }

    /* Load E illuminant (equal energy) */
    alwan_spd illum_e;
    status = alwan_spd_illuminant(ctx, "E", &illum_e);
    TEST_ASSERT(status == ALWAN_OK, "E illuminant loading failed");

    /* Test both observers */
    alwan_vec3 xyz_2deg, xyz_10deg;

    status = alwan_xyz_from_spd(ctx, &reflectance, &illum_e,
                                ALWAN_OBSERVER_CIE_1931_2DEG,
                                ALWAN_INTEGRATE_SIMPSON,
                                ALWAN_LITERAL(0.0),  /* No bandpass correction */
                                &xyz_2deg);
    TEST_ASSERT(status == ALWAN_OK, "XYZ with 2° observer failed");

    status = alwan_xyz_from_spd(ctx, &reflectance, &illum_e,
                                ALWAN_OBSERVER_CIE_1964_10DEG,
                                ALWAN_INTEGRATE_SIMPSON,
                                ALWAN_LITERAL(0.0),  /* No bandpass correction */
                                &xyz_10deg);
    TEST_ASSERT(status == ALWAN_OK, "XYZ with 10° observer failed");

    /* Both observers should give positive XYZ */
    TEST_ASSERT(xyz_2deg.v[0] > ALWAN_LITERAL(0.0), "2° observer X should be positive");
    TEST_ASSERT(xyz_2deg.v[1] > ALWAN_LITERAL(0.0), "2° observer Y should be positive");
    TEST_ASSERT(xyz_2deg.v[2] > ALWAN_LITERAL(0.0), "2° observer Z should be positive");

    TEST_ASSERT(xyz_10deg.v[0] > ALWAN_LITERAL(0.0), "10° observer X should be positive");
    TEST_ASSERT(xyz_10deg.v[1] > ALWAN_LITERAL(0.0), "10° observer Y should be positive");
    TEST_ASSERT(xyz_10deg.v[2] > ALWAN_LITERAL(0.0), "10° observer Z should be positive");

    printf("  CIE 1931 2°:  XYZ = [%.4f, %.4f, %.4f]\n",
           xyz_2deg.v[0], xyz_2deg.v[1], xyz_2deg.v[2]);
    printf("  CIE 1964 10°: XYZ = [%.4f, %.4f, %.4f]\n",
           xyz_10deg.v[0], xyz_10deg.v[1], xyz_10deg.v[2]);

    alwan_spd_destroy(ctx, &reflectance);
    alwan_spd_destroy(ctx, &illum_e);
    alwan_destroy(ctx);
    TEST_PASS("XYZ with both observers");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_50_spd_to_xyz_main(void) {
    int failures = 0;

    failures += test_spd_create_destroy();
    failures += test_illuminant_loading();
    failures += test_spd_resampling();
    failures += test_xyz_from_constant_spd();
    failures += test_xyz_both_observers();

    if (failures == 0) {
        printf("\n=== All SPD tests passed ===\n");
        return 0;
    } else {
        fprintf(stderr, "\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
