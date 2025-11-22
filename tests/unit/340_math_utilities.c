/* ================================================================
 * Test 340: Advanced Mathematical & Utility Functions
 * ================================================================
 * Tests for:
 * - Advanced Interpolation Methods
 * - Enhanced Extrapolation Methods
 * - CCT/Duv Optimization
 * - Tristimulus Optimization
 * - Table Interpolation Utilities
 * ================================================================ */

#include "../../src/alwan/alwan.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* Use fabs for floating point absolute value */
#ifndef ALWAN_FABS
#define ALWAN_FABS fabs
#endif

/* Test macros */
#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        return 1; \
    } \
} while(0)

#define TEST_PASS(name) do { \
    printf("  PASS: %s\n", name); \
    return 0; \
} while(0)

#define TOLERANCE 1e-6

/* ----------------------------------------------------------------
 * Advanced Interpolation Tests
 * ---------------------------------------------------------------- */

static int test_interpolation_linear(void) {
    /* Test linear interpolation on simple data */
    alwan_scalar x_in[] = {0.0, 1.0, 2.0, 3.0};
    alwan_scalar y_in[] = {0.0, 1.0, 2.0, 3.0};
    size_t count_in = 4;

    alwan_scalar x_out[] = {0.5, 1.5, 2.5};
    alwan_scalar y_out[3];
    size_t count_out = 3;

    int status = alwan_interpolate(x_in, y_in, count_in, x_out, y_out, count_out, ALWAN_INTERP_LINEAR);

    TEST_ASSERT(status == ALWAN_OK, "Linear interpolation failed");
    TEST_ASSERT(ALWAN_FABS(y_out[0] - 0.5) < TOLERANCE, "y[0.5] should be ~0.5");
    TEST_ASSERT(ALWAN_FABS(y_out[1] - 1.5) < TOLERANCE, "y[1.5] should be ~1.5");
    TEST_ASSERT(ALWAN_FABS(y_out[2] - 2.5) < TOLERANCE, "y[2.5] should be ~2.5");

    printf("  Linear interpolation: y(0.5)=%.3f, y(1.5)=%.3f, y(2.5)=%.3f\n",
           y_out[0], y_out[1], y_out[2]);

    TEST_PASS("Linear interpolation");
}

static int test_interpolation_cubic(void) {
    /* Test cubic interpolation on sine wave */
    alwan_scalar x_in[] = {0.0, 0.5, 1.0, 1.5, 2.0};
    alwan_scalar y_in[5];
    for (int i = 0; i < 5; i++) {
        y_in[i] = sin(x_in[i]);
    }
    size_t count_in = 5;

    alwan_scalar x_out[] = {0.25, 0.75, 1.25};
    alwan_scalar y_out[3];
    size_t count_out = 3;

    int status = alwan_interpolate(x_in, y_in, count_in, x_out, y_out, count_out, ALWAN_INTERP_CUBIC);

    TEST_ASSERT(status == ALWAN_OK, "Cubic interpolation failed");

    /* Cubic should be more accurate than linear for smooth functions */
    alwan_scalar expected_0 = sin(0.25);
    alwan_scalar expected_1 = sin(0.75);
    alwan_scalar expected_2 = sin(1.25);

    printf("  Cubic interpolation:\n");
    printf("    y(0.25): %.6f (expected: %.6f, error: %.6f)\n",
           y_out[0], expected_0, ALWAN_FABS(y_out[0] - expected_0));
    printf("    y(0.75): %.6f (expected: %.6f, error: %.6f)\n",
           y_out[1], expected_1, ALWAN_FABS(y_out[1] - expected_1));
    printf("    y(1.25): %.6f (expected: %.6f, error: %.6f)\n",
           y_out[2], expected_2, ALWAN_FABS(y_out[2] - expected_2));

    TEST_PASS("Cubic interpolation");
}

static int test_interpolation_sprague(void) {
    /* Test Sprague interpolation (5th order) */
    alwan_scalar x_in[10];
    alwan_scalar y_in[10];
    for (int i = 0; i < 10; i++) {
        x_in[i] = i * 10.0;  /* 0, 10, 20, ..., 90 */
        y_in[i] = sin(i * 0.2);
    }
    size_t count_in = 10;

    alwan_scalar x_out[] = {25.0, 45.0, 65.0};
    alwan_scalar y_out[3];
    size_t count_out = 3;

    int status = alwan_interpolate(x_in, y_in, count_in, x_out, y_out, count_out, ALWAN_INTERP_SPRAGUE);

    TEST_ASSERT(status == ALWAN_OK, "Sprague interpolation failed");

    printf("  Sprague interpolation: y(25)=%.6f, y(45)=%.6f, y(65)=%.6f\n",
           y_out[0], y_out[1], y_out[2]);

    TEST_PASS("Sprague interpolation");
}

static int test_interpolation_akima(void) {
    /* Test Akima spline (non-overshooting) */
    alwan_scalar x_in[] = {0.0, 1.0, 2.0, 3.0, 4.0, 5.0};
    alwan_scalar y_in[] = {0.0, 1.0, 0.0, 1.0, 0.0, 1.0};  /* Oscillating */
    size_t count_in = 6;

    alwan_scalar x_out[] = {0.5, 1.5, 2.5, 3.5, 4.5};
    alwan_scalar y_out[5];
    size_t count_out = 5;

    int status = alwan_interpolate(x_in, y_in, count_in, x_out, y_out, count_out, ALWAN_INTERP_AKIMA);

    TEST_ASSERT(status == ALWAN_OK, "Akima interpolation failed");

    /* Akima should not overshoot significantly */
    for (size_t i = 0; i < count_out; i++) {
        TEST_ASSERT(y_out[i] >= -0.5 && y_out[i] <= 1.5, "Akima overshoot detected");
    }

    printf("  Akima interpolation values: %.3f, %.3f, %.3f, %.3f, %.3f\n",
           y_out[0], y_out[1], y_out[2], y_out[3], y_out[4]);

    TEST_PASS("Akima interpolation");
}

/* ----------------------------------------------------------------
 * Enhanced Extrapolation Tests
 * ---------------------------------------------------------------- */

static int test_extrapolation_constant(void) {
    /* Test constant extrapolation */
    alwan_scalar x_in[] = {1.0, 2.0, 3.0, 4.0};
    alwan_scalar y_in[] = {2.0, 4.0, 6.0, 8.0};
    size_t count_in = 4;

    alwan_scalar x_out[] = {0.0, 5.0};  /* Outside range */
    alwan_scalar y_out[2];
    size_t count_out = 2;

    int status = alwan_extrapolate(x_in, y_in, count_in, x_out, y_out, count_out, ALWAN_EXTRAP_CONSTANT);

    TEST_ASSERT(status == ALWAN_OK, "Constant extrapolation failed");
    TEST_ASSERT(ALWAN_FABS(y_out[0] - 2.0) < TOLERANCE, "Left boundary should be 2.0");
    TEST_ASSERT(ALWAN_FABS(y_out[1] - 8.0) < TOLERANCE, "Right boundary should be 8.0");

    printf("  Constant extrapolation: y(0.0)=%.3f, y(5.0)=%.3f\n", y_out[0], y_out[1]);

    TEST_PASS("Constant extrapolation");
}

static int test_extrapolation_linear(void) {
    /* Test linear extrapolation */
    alwan_scalar x_in[] = {1.0, 2.0, 3.0, 4.0};
    alwan_scalar y_in[] = {2.0, 4.0, 6.0, 8.0};
    size_t count_in = 4;

    alwan_scalar x_out[] = {0.0, 5.0};  /* Outside range */
    alwan_scalar y_out[2];
    size_t count_out = 2;

    int status = alwan_extrapolate(x_in, y_in, count_in, x_out, y_out, count_out, ALWAN_EXTRAP_LINEAR);

    TEST_ASSERT(status == ALWAN_OK, "Linear extrapolation failed");
    TEST_ASSERT(ALWAN_FABS(y_out[0] - 0.0) < TOLERANCE, "y(0.0) should be ~0.0");
    TEST_ASSERT(ALWAN_FABS(y_out[1] - 10.0) < TOLERANCE, "y(5.0) should be ~10.0");

    printf("  Linear extrapolation: y(0.0)=%.3f, y(5.0)=%.3f\n", y_out[0], y_out[1]);

    TEST_PASS("Linear extrapolation");
}

static int test_extrapolation_exponential(void) {
    /* Test exponential extrapolation (for SPDs) */
    alwan_scalar x_in[] = {400.0, 500.0, 600.0, 700.0};
    alwan_scalar y_in[] = {1.0, 0.5, 0.25, 0.125};  /* Exponential decay */
    size_t count_in = 4;

    alwan_scalar x_out[] = {300.0, 800.0};  /* Outside range */
    alwan_scalar y_out[2];
    size_t count_out = 2;

    int status = alwan_extrapolate(x_in, y_in, count_in, x_out, y_out, count_out, ALWAN_EXTRAP_EXPONENTIAL);

    TEST_ASSERT(status == ALWAN_OK, "Exponential extrapolation failed");
    TEST_ASSERT(y_out[0] >= 0.0, "Extrapolated value should be non-negative");
    TEST_ASSERT(y_out[1] >= 0.0, "Extrapolated value should be non-negative");

    printf("  Exponential extrapolation: y(300)=%.6f, y(800)=%.6f\n", y_out[0], y_out[1]);

    TEST_PASS("Exponential extrapolation");
}

/* ----------------------------------------------------------------
 * CCT/Duv Optimization Tests
 * ---------------------------------------------------------------- */

static int test_cct_d65(void) {
    /* Test CCT/Duv optimization for D65 (6504K) */
    alwan_vec2 xy_d65 = {{0.31271, 0.32902}};  /* D65 chromaticity */
    alwan_scalar cct_out, duv_out;

    int status = alwan_cct_duv_optimize(&xy_d65, &cct_out, &duv_out);

    /* Print actual values for debugging */
    printf("  D65: CCT=%.1fK, Duv=%.6f (expected ~6504K, Duv~0)\n", cct_out, duv_out);

    TEST_ASSERT(status == ALWAN_OK, "CCT/Duv optimization failed for D65");
    TEST_ASSERT(cct_out >= 6400.0 && cct_out <= 6600.0, "D65 CCT should be ~6504K");
    TEST_ASSERT(ALWAN_FABS(duv_out) < 0.01, "D65 Duv should be near 0");

    TEST_PASS("CCT/Duv optimization for D65");
}

static int test_cct_a(void) {
    /* Test CCT/Duv optimization for Illuminant A (2856K) */
    alwan_vec2 xy_a = {{0.44757, 0.40745}};  /* Illuminant A chromaticity */
    alwan_scalar cct_out, duv_out;

    int status = alwan_cct_duv_optimize(&xy_a, &cct_out, &duv_out);

    TEST_ASSERT(status == ALWAN_OK, "CCT/Duv optimization failed for A");
    TEST_ASSERT(cct_out >= 2800.0 && cct_out <= 2900.0, "Illuminant A CCT should be ~2856K");
    TEST_ASSERT(ALWAN_FABS(duv_out) < 0.01, "Illuminant A Duv should be near 0");

    printf("  Illuminant A: CCT=%.1fK, Duv=%.6f (expected ~2856K, Duv~0)\n", cct_out, duv_out);

    TEST_PASS("CCT/Duv optimization for Illuminant A");
}

static int test_cct_off_locus(void) {
    /* Test CCT/Duv for point off Planckian locus */
    alwan_vec2 xy_off = {{0.35, 0.40}};  /* Arbitrary point */
    alwan_scalar cct_out, duv_out;

    int status = alwan_cct_duv_optimize(&xy_off, &cct_out, &duv_out);

    TEST_ASSERT(status == ALWAN_OK, "CCT/Duv optimization failed for off-locus point");
    TEST_ASSERT(cct_out >= 1000.0 && cct_out <= 25000.0, "CCT should be in valid range");
    TEST_ASSERT(duv_out >= 0.0, "Duv should be non-negative");

    printf("  Off-locus point (x=0.35, y=0.40): CCT=%.1fK, Duv=%.6f\n", cct_out, duv_out);

    TEST_PASS("CCT/Duv optimization for off-locus point");
}

/* ----------------------------------------------------------------
 * Tristimulus Optimization Tests
 * ---------------------------------------------------------------- */

static int test_optimize_spectrum(void) {
    /* Test spectrum optimization for target XYZ */
    alwan_vec3 target_xyz = {{50.0, 50.0, 50.0}};  /* Mid-gray */
    alwan_ctx *ctx;
    alwan_spd spd_out;
    alwan_scalar spd_values[81];  /* Allocate storage for SPD values */
    int status;

    /* Initialize SPD with storage */
    spd_out.values = spd_values;
    spd_out.count = 0;  /* Will be set by the function */

    ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    status = alwan_optimize_spectrum_for_xyz(&target_xyz, ALWAN_OBSERVER_CIE_1931_2DEG, ctx, &spd_out);

    TEST_ASSERT(status == ALWAN_OK, "Spectrum optimization failed");
    TEST_ASSERT(spd_out.count == 81, "SPD should have 81 wavelengths");
    TEST_ASSERT(ALWAN_FABS(spd_out.wavelength_min - 380.0) < TOLERANCE, "Min wavelength should be 380nm");
    TEST_ASSERT(ALWAN_FABS(spd_out.wavelength_max - 780.0) < TOLERANCE, "Max wavelength should be 780nm");

    /* Check that SPD values are non-negative */
    for (size_t i = 0; i < spd_out.count; i++) {
        TEST_ASSERT(spd_out.values[i] >= 0.0, "SPD values should be non-negative");
    }

    printf("  Optimized SPD for XYZ[50,50,50]: %zu wavelengths, range %.1f-%.1fnm\n",
           spd_out.count, spd_out.wavelength_min, spd_out.wavelength_max);

    alwan_destroy(ctx);

    TEST_PASS("Tristimulus optimization");
}

/* ----------------------------------------------------------------
 * 1D Table Interpolation Tests
 * ---------------------------------------------------------------- */

static int test_table_1d_linear(void) {
    /* Test 1D table interpolation (linear) */
    alwan_scalar table[] = {0.0, 1.0, 2.0, 3.0, 4.0, 5.0};
    size_t size = 6;

    alwan_scalar y0 = alwan_table_interp_1d(table, size, 0.0, ALWAN_INTERP_LINEAR);
    alwan_scalar y1 = alwan_table_interp_1d(table, size, 0.5, ALWAN_INTERP_LINEAR);
    alwan_scalar y2 = alwan_table_interp_1d(table, size, 1.0, ALWAN_INTERP_LINEAR);

    TEST_ASSERT(ALWAN_FABS(y0 - 0.0) < TOLERANCE, "y(0.0) should be 0.0");
    TEST_ASSERT(ALWAN_FABS(y1 - 2.5) < TOLERANCE, "y(0.5) should be 2.5");
    TEST_ASSERT(ALWAN_FABS(y2 - 5.0) < TOLERANCE, "y(1.0) should be 5.0");

    printf("  1D table linear: y(0.0)=%.3f, y(0.5)=%.3f, y(1.0)=%.3f\n", y0, y1, y2);

    TEST_PASS("1D table interpolation (linear)");
}

static int test_table_1d_cubic(void) {
    /* Test 1D table interpolation (cubic) */
    alwan_scalar table[] = {0.0, 1.0, 4.0, 9.0, 16.0, 25.0};  /* x^2 */
    size_t size = 6;

    alwan_scalar y = alwan_table_interp_1d(table, size, 0.5, ALWAN_INTERP_CUBIC);

    /* Cubic should approximate x^2 better than linear */
    printf("  1D table cubic: y(0.5)=%.3f\n", y);

    TEST_PASS("1D table interpolation (cubic)");
}

/* ----------------------------------------------------------------
 * 3D Table Interpolation Tests
 * ---------------------------------------------------------------- */

static int test_table_3d_trilinear(void) {
    /* Test 3D trilinear interpolation */
    /* Simple 2x2x2 identity LUT */
    size_t sizes[3] = {2, 2, 2};
    alwan_scalar table[2 * 2 * 2 * 3] = {
        /* [0,0,0] */ 0.0, 0.0, 0.0,
        /* [1,0,0] */ 1.0, 0.0, 0.0,
        /* [0,1,0] */ 0.0, 1.0, 0.0,
        /* [1,1,0] */ 1.0, 1.0, 0.0,
        /* [0,0,1] */ 0.0, 0.0, 1.0,
        /* [1,0,1] */ 1.0, 0.0, 1.0,
        /* [0,1,1] */ 0.0, 1.0, 1.0,
        /* [1,1,1] */ 1.0, 1.0, 1.0
    };

    alwan_vec3 rgb_in = {{0.5, 0.5, 0.5}};
    alwan_vec3 rgb_out;

    int status = alwan_table_interp_3d_trilinear(table, sizes, &rgb_in, &rgb_out);

    TEST_ASSERT(status == ALWAN_OK, "3D trilinear interpolation failed");
    TEST_ASSERT(ALWAN_FABS(rgb_out.v[0] - 0.5) < TOLERANCE, "R should be ~0.5");
    TEST_ASSERT(ALWAN_FABS(rgb_out.v[1] - 0.5) < TOLERANCE, "G should be ~0.5");
    TEST_ASSERT(ALWAN_FABS(rgb_out.v[2] - 0.5) < TOLERANCE, "B should be ~0.5");

    printf("  3D trilinear: [0.5,0.5,0.5] -> [%.3f,%.3f,%.3f]\n",
           rgb_out.v[0], rgb_out.v[1], rgb_out.v[2]);

    TEST_PASS("3D trilinear interpolation");
}

static int test_table_3d_tetrahedral(void) {
    /* Test 3D tetrahedral interpolation */
    /* Simple 2x2x2 identity LUT */
    size_t sizes[3] = {2, 2, 2};
    alwan_scalar table[2 * 2 * 2 * 3] = {
        /* [0,0,0] */ 0.0, 0.0, 0.0,
        /* [1,0,0] */ 1.0, 0.0, 0.0,
        /* [0,1,0] */ 0.0, 1.0, 0.0,
        /* [1,1,0] */ 1.0, 1.0, 0.0,
        /* [0,0,1] */ 0.0, 0.0, 1.0,
        /* [1,0,1] */ 1.0, 0.0, 1.0,
        /* [0,1,1] */ 0.0, 1.0, 1.0,
        /* [1,1,1] */ 1.0, 1.0, 1.0
    };

    alwan_vec3 rgb_in = {{0.5, 0.5, 0.5}};
    alwan_vec3 rgb_out;

    int status = alwan_table_interp_3d_tetrahedral(table, sizes, &rgb_in, &rgb_out);

    TEST_ASSERT(status == ALWAN_OK, "3D tetrahedral interpolation failed");
    TEST_ASSERT(ALWAN_FABS(rgb_out.v[0] - 0.5) < TOLERANCE, "R should be ~0.5");
    TEST_ASSERT(ALWAN_FABS(rgb_out.v[1] - 0.5) < TOLERANCE, "G should be ~0.5");
    TEST_ASSERT(ALWAN_FABS(rgb_out.v[2] - 0.5) < TOLERANCE, "B should be ~0.5");

    printf("  3D tetrahedral: [0.5,0.5,0.5] -> [%.3f,%.3f,%.3f]\n",
           rgb_out.v[0], rgb_out.v[1], rgb_out.v[2]);

    TEST_PASS("3D tetrahedral interpolation");
}

/* ----------------------------------------------------------------
 * Main Test Runner
 * ---------------------------------------------------------------- */

int test_340_math_utilities_main(void) {
    int total_tests = 0;
    int failed_tests = 0;

    printf("\n========================================\n");
    printf("Test 340: Math Utilities\n");
    printf("========================================\n\n");

    printf("Advanced Interpolation Methods\n");
    printf("------------------------------\n");
    total_tests++; if (test_interpolation_linear() != 0) failed_tests++;
    total_tests++; if (test_interpolation_cubic() != 0) failed_tests++;
    total_tests++; if (test_interpolation_sprague() != 0) failed_tests++;
    total_tests++; if (test_interpolation_akima() != 0) failed_tests++;

    printf("\nEnhanced Extrapolation Methods\n");
    printf("------------------------------\n");
    total_tests++; if (test_extrapolation_constant() != 0) failed_tests++;
    total_tests++; if (test_extrapolation_linear() != 0) failed_tests++;
    total_tests++; if (test_extrapolation_exponential() != 0) failed_tests++;

    printf("\nCCT/Duv Optimization\n");
    printf("--------------------\n");
    total_tests++; if (test_cct_d65() != 0) failed_tests++;
    total_tests++; if (test_cct_a() != 0) failed_tests++;
    total_tests++; if (test_cct_off_locus() != 0) failed_tests++;

    printf("\nTristimulus Optimization\n");
    printf("------------------------\n");
    total_tests++; if (test_optimize_spectrum() != 0) failed_tests++;

    printf("\n1D Table Interpolation\n");
    printf("----------------------\n");
    total_tests++; if (test_table_1d_linear() != 0) failed_tests++;
    total_tests++; if (test_table_1d_cubic() != 0) failed_tests++;

    printf("\n3D Table Interpolation\n");
    printf("----------------------\n");
    total_tests++; if (test_table_3d_trilinear() != 0) failed_tests++;
    total_tests++; if (test_table_3d_tetrahedral() != 0) failed_tests++;

    printf("\n========================================\n");
    printf("Test Results: %d/%d tests passed\n", total_tests - failed_tests, total_tests);
    printf("========================================\n");

    return (failed_tests == 0) ? 0 : 1;
}
