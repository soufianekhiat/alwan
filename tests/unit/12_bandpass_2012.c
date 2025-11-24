/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * M6 Tests: CIE 2012 observers, extrapolation, bandpass correction
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>
#include <stdlib.h>

/* Test helpers */
#define TEST_ASSERT(cond, msg) \
    do { if (!(cond)) { printf("FAIL: %s\n", msg); return 1; } } while (0)

#define TEST_PASS(msg) \
    do { printf("  [PASS] %s\n", msg); return 0; } while (0)

/* Test CIE 2012 observers */
static int test_cie_2012_observers(void) {
    alwan_ctx *ctx = alwan_create(NULL);

    /* Create constant reflectance SPD (50% gray) */
    alwan_spd reflectance;
    int status = alwan_spd_create(ctx, ALWAN_LITERAL(400.0), ALWAN_LITERAL(700.0), 31, &reflectance);
    TEST_ASSERT(status == ALWAN_OK, "SPD creation failed");

    for (size_t i = 0; i < reflectance.count; i++) {
        reflectance.values[i] = ALWAN_LITERAL(0.5);
    }

    /* Load D65 illuminant */
    alwan_spd d65;
    status = alwan_spd_illuminant(ctx, ALWAN_ILLUMINANT_D65, &d65);
    TEST_ASSERT(status == ALWAN_OK, "D65 loading failed");

    /* Test CIE 2012 2° observer */
    alwan_vec3 xyz_2012_2deg;
    status = alwan_xyz_from_spd(ctx, &reflectance, &d65,
                                ALWAN_OBSERVER_CIE_2012_2DEG,
                                ALWAN_INTEGRATE_SIMPSON,
                                ALWAN_LITERAL(0.0),
                                &xyz_2012_2deg);
    TEST_ASSERT(status == ALWAN_OK, "CIE 2012 2° observer failed");
    TEST_ASSERT(xyz_2012_2deg.v[0] > ALWAN_LITERAL(0.0), "2012 2° X should be positive");
    TEST_ASSERT(xyz_2012_2deg.v[1] > ALWAN_LITERAL(0.0), "2012 2° Y should be positive");
    TEST_ASSERT(xyz_2012_2deg.v[2] > ALWAN_LITERAL(0.0), "2012 2° Z should be positive");

    /* Test CIE 2012 10° observer */
    alwan_vec3 xyz_2012_10deg;
    status = alwan_xyz_from_spd(ctx, &reflectance, &d65,
                                ALWAN_OBSERVER_CIE_2012_10DEG,
                                ALWAN_INTEGRATE_SIMPSON,
                                ALWAN_LITERAL(0.0),
                                &xyz_2012_10deg);
    TEST_ASSERT(status == ALWAN_OK, "CIE 2012 10° observer failed");
    TEST_ASSERT(xyz_2012_10deg.v[0] > ALWAN_LITERAL(0.0), "2012 10° X should be positive");
    TEST_ASSERT(xyz_2012_10deg.v[1] > ALWAN_LITERAL(0.0), "2012 10° Y should be positive");
    TEST_ASSERT(xyz_2012_10deg.v[2] > ALWAN_LITERAL(0.0), "2012 10° Z should be positive");

    /* 2° and 10° observers should give slightly different results */
    alwan_scalar diff_x = ALWAN_FABS(xyz_2012_2deg.v[0] - xyz_2012_10deg.v[0]);
    alwan_scalar diff_y = ALWAN_FABS(xyz_2012_2deg.v[1] - xyz_2012_10deg.v[1]);
    alwan_scalar diff_z = ALWAN_FABS(xyz_2012_2deg.v[2] - xyz_2012_10deg.v[2]);

    TEST_ASSERT(diff_x + diff_y + diff_z > ALWAN_LITERAL(0.01),
                "2° and 10° observers should differ");

    printf("  CIE 2012 2°:  XYZ = [%.4f, %.4f, %.4f]\n",
           (double)xyz_2012_2deg.v[0], (double)xyz_2012_2deg.v[1], (double)xyz_2012_2deg.v[2]);
    printf("  CIE 2012 10°: XYZ = [%.4f, %.4f, %.4f]\n",
           (double)xyz_2012_10deg.v[0], (double)xyz_2012_10deg.v[1], (double)xyz_2012_10deg.v[2]);

    alwan_spd_destroy(ctx, &reflectance);
    alwan_spd_destroy(ctx, &d65);
    alwan_destroy(ctx);
    TEST_PASS("CIE 2012 observers");
}

/* Test extrapolation modes */
static int test_extrapolation_modes(void) {
    alwan_ctx *ctx = alwan_create(NULL);

    /* Create SPD with limited range (450-650nm) */
    alwan_spd src;
    int status = alwan_spd_create(ctx, ALWAN_LITERAL(450.0), ALWAN_LITERAL(650.0), 21, &src);
    TEST_ASSERT(status == ALWAN_OK, "SPD creation failed");

    /* Fill with linear ramp */
    for (size_t i = 0; i < src.count; i++) {
        src.values[i] = ALWAN_LITERAL(0.1) + (alwan_scalar)i / (alwan_scalar)(src.count - 1) * ALWAN_LITERAL(0.8);
    }

    /* Resample to wider range (400-700nm) with different extrapolation modes */
    alwan_spd dst_zero, dst_const, dst_linear;

    /* Test ZERO extrapolation */
    status = alwan_spd_resample(ctx, &src,
                                ALWAN_LITERAL(400.0), ALWAN_LITERAL(700.0), 31,
                                ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO,
                                &dst_zero);
    TEST_ASSERT(status == ALWAN_OK, "Zero extrapolation failed");
    /* Check that values outside original range are zero */
    TEST_ASSERT(dst_zero.values[0] == ALWAN_LITERAL(0.0), "Zero extrap: first value should be 0");
    TEST_ASSERT(dst_zero.values[dst_zero.count - 1] == ALWAN_LITERAL(0.0),
                "Zero extrap: last value should be 0");

    /* Test CONSTANT extrapolation */
    status = alwan_spd_resample(ctx, &src,
                                ALWAN_LITERAL(400.0), ALWAN_LITERAL(700.0), 31,
                                ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_CONSTANT,
                                &dst_const);
    TEST_ASSERT(status == ALWAN_OK, "Constant extrapolation failed");
    /* Check that edge values are repeated */
    TEST_ASSERT(dst_const.values[0] == src.values[0],
                "Const extrap: first value should match edge");
    TEST_ASSERT(dst_const.values[dst_const.count - 1] == src.values[src.count - 1],
                "Const extrap: last value should match edge");

    /* Test LINEAR extrapolation */
    status = alwan_spd_resample(ctx, &src,
                                ALWAN_LITERAL(400.0), ALWAN_LITERAL(700.0), 31,
                                ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_LINEAR,
                                &dst_linear);
    TEST_ASSERT(status == ALWAN_OK, "Linear extrapolation failed");
    /* Linear extrapolation should extend the trend */
    TEST_ASSERT(dst_linear.values[0] < src.values[0],
                "Linear extrap: should extrapolate below first value");

    printf("  Zero extrap edges:   [%.4f, %.4f]\n",
           (double)dst_zero.values[0], (double)dst_zero.values[dst_zero.count - 1]);
    printf("  Const extrap edges:  [%.4f, %.4f]\n",
           (double)dst_const.values[0], (double)dst_const.values[dst_const.count - 1]);
    printf("  Linear extrap edges: [%.4f, %.4f]\n",
           (double)dst_linear.values[0], (double)dst_linear.values[dst_linear.count - 1]);

    alwan_spd_destroy(ctx, &src);
    alwan_spd_destroy(ctx, &dst_zero);
    alwan_spd_destroy(ctx, &dst_const);
    alwan_spd_destroy(ctx, &dst_linear);
    alwan_destroy(ctx);
    TEST_PASS("Extrapolation modes");
}

/* Test bandpass correction parameter (placeholder test) */
static int test_bandpass_parameter(void) {
    alwan_ctx *ctx = alwan_create(NULL);

    /* Create simple reflectance */
    alwan_spd reflectance;
    int status = alwan_spd_create(ctx, ALWAN_LITERAL(400.0), ALWAN_LITERAL(700.0), 31, &reflectance);
    TEST_ASSERT(status == ALWAN_OK, "SPD creation failed");

    for (size_t i = 0; i < reflectance.count; i++) {
        reflectance.values[i] = ALWAN_LITERAL(0.5);
    }

    /* Load D65 */
    alwan_spd d65;
    status = alwan_spd_illuminant(ctx, ALWAN_ILLUMINANT_D65, &d65);
    TEST_ASSERT(status == ALWAN_OK, "D65 loading failed");

    /* Test with different bandpass values (currently no-op) */
    alwan_vec3 xyz_no_bp, xyz_with_bp;

    status = alwan_xyz_from_spd(ctx, &reflectance, &d65,
                                ALWAN_OBSERVER_CIE_1931_2DEG,
                                ALWAN_INTEGRATE_SIMPSON,
                                ALWAN_LITERAL(0.0),  /* No bandpass correction */
                                &xyz_no_bp);
    TEST_ASSERT(status == ALWAN_OK, "XYZ without bandpass failed");

    status = alwan_xyz_from_spd(ctx, &reflectance, &d65,
                                ALWAN_OBSERVER_CIE_1931_2DEG,
                                ALWAN_INTEGRATE_SIMPSON,
                                ALWAN_LITERAL(10.0),  /* 10nm bandpass (not yet implemented) */
                                &xyz_with_bp);
    TEST_ASSERT(status == ALWAN_OK, "XYZ with bandpass parameter failed");

    /* Since bandpass correction is not yet implemented, results should be identical */
    alwan_scalar diff = ALWAN_FABS(xyz_no_bp.v[0] - xyz_with_bp.v[0]) +
                  ALWAN_FABS(xyz_no_bp.v[1] - xyz_with_bp.v[1]) +
                  ALWAN_FABS(xyz_no_bp.v[2] - xyz_with_bp.v[2]);
    TEST_ASSERT(diff < ALWAN_EPSILON, "Bandpass parameter should be accepted (impl pending)");

    alwan_spd_destroy(ctx, &reflectance);
    alwan_spd_destroy(ctx, &d65);
    alwan_destroy(ctx);
    TEST_PASS("Bandpass parameter");
}

/* Main test runner */
int test_12_bandpass_2012_main(void) {
    printf("=== M6: CIE 2012 Observers & Extrapolation Tests ===\n");

    int failed = 0;
    failed += test_cie_2012_observers();
    failed += test_extrapolation_modes();
    failed += test_bandpass_parameter();

    if (failed == 0) {
        printf("\n=== All M6 tests passed ===\n");
    } else {
        printf("\n=== %d M6 test(s) failed ===\n", failed);
    }

    return failed;
}
