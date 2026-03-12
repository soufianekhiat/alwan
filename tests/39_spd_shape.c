/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 39: P8.5 SPD Shape Descriptors
 *
 * Tests spectral shape descriptor calculations (peak, FWHM, centroid, bandwidth)
 */

#include "test_common.h"
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Test: Shape descriptor for broad spectrum (D65)
 * ---------------------------------------------------------------- */

static int test_shape_descriptor_d65(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Context creation failed");

    /* Load D65 illuminant */
    alwan_spd d65;
    int status = alwan_spd_illuminant(&d65, ctx, ALWAN_ILLUMINANT_D65);
    TEST_ASSERT(status == ALWAN_OK, "Failed to load D65 illuminant");

    /* Analyze shape */
    alwan_spd_shape shape;
    status = alwan_spd_analyze_shape(&shape, &d65);
    TEST_ASSERT(status == ALWAN_OK, "Failed to analyze D65 shape");

    printf("  D65 Shape Descriptor:\n");
    printf("    Peak wavelength: %.2f nm\n", shape.peak_wavelength);
    printf("    Peak value:      %.6f\n", shape.peak_value);
    printf("    FWHM:            %.2f nm\n", shape.fwhm);
    printf("    Centroid:        %.2f nm\n", shape.centroid);
    printf("    Bandwidth:       %.2f nm\n", shape.bandwidth);

    /* D65 is a broad spectrum, so:
     * - Peak should be in visible range (400-700nm)
     * - FWHM should be large (>100nm for broad spectrum)
     * - Centroid should be around 500-600nm
     * - Bandwidth should be 470nm (830-360) */

    TEST_ASSERT(shape.peak_wavelength >= ALWAN_LITERAL(400.0) &&
                shape.peak_wavelength <= ALWAN_LITERAL(700.0),
                "D65 peak wavelength out of expected range");

    TEST_ASSERT(shape.fwhm > ALWAN_LITERAL(100.0),
                "D65 FWHM too narrow for broad spectrum");

    TEST_ASSERT(shape.centroid >= ALWAN_LITERAL(450.0) &&
                shape.centroid <= ALWAN_LITERAL(650.0),
                "D65 centroid out of expected range");

    TEST_ASSERT(ALWAN_ABS(shape.bandwidth - ALWAN_LITERAL(470.0)) < ALWAN_LITERAL(1.0),
                "D65 bandwidth incorrect");

    TEST_ASSERT(shape.peak_value > ALWAN_LITERAL(0.0),
                "D65 peak value should be positive");

    alwan_spd_destroy(ctx, &d65);
    alwan_destroy(ctx);
    TEST_PASS("Shape descriptor for D65 (broad spectrum)");
}

/* ----------------------------------------------------------------
 * Test: Shape descriptor for narrow spectrum (LED-B1)
 * ---------------------------------------------------------------- */

static int test_shape_descriptor_led(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Context creation failed");

    /* Load LED-B1 illuminant (narrow blue-pumped phosphor LED) */
    alwan_spd led;
    int status = alwan_spd_illuminant(&led, ctx, ALWAN_ILLUMINANT_LED_B1);
    TEST_ASSERT(status == ALWAN_OK, "Failed to load LED-B1 illuminant");

    /* Analyze shape */
    alwan_spd_shape shape;
    status = alwan_spd_analyze_shape(&shape, &led);
    TEST_ASSERT(status == ALWAN_OK, "Failed to analyze LED-B1 shape");

    printf("  LED-B1 Shape Descriptor:\n");
    printf("    Peak wavelength: %.2f nm\n", shape.peak_wavelength);
    printf("    Peak value:      %.6f\n", shape.peak_value);
    printf("    FWHM:            %.2f nm\n", shape.fwhm);
    printf("    Centroid:        %.2f nm\n", shape.centroid);
    printf("    Bandwidth:       %.2f nm\n", shape.bandwidth);

    /* LED-B1 is a phosphor-based LED with:
     * - Peak in visible range
     * - May have narrower features than D65
     * - Centroid in visible range
     * - Same bandwidth as D65 (full SPD range) */

    TEST_ASSERT(shape.peak_wavelength >= ALWAN_LITERAL(360.0) &&
                shape.peak_wavelength <= ALWAN_LITERAL(830.0),
                "LED-B1 peak wavelength out of range");

    TEST_ASSERT(shape.fwhm > ALWAN_LITERAL(0.0),
                "LED-B1 FWHM should be positive");

    TEST_ASSERT(shape.centroid >= ALWAN_LITERAL(360.0) &&
                shape.centroid <= ALWAN_LITERAL(830.0),
                "LED-B1 centroid out of range");

    TEST_ASSERT(ALWAN_ABS(shape.bandwidth - ALWAN_LITERAL(470.0)) < ALWAN_LITERAL(1.0),
                "LED-B1 bandwidth incorrect");

    TEST_ASSERT(shape.peak_value > ALWAN_LITERAL(0.0),
                "LED-B1 peak value should be positive");

    alwan_spd_destroy(ctx, &led);
    alwan_destroy(ctx);
    TEST_PASS("Shape descriptor for LED-B1 (narrow spectrum)");
}

/* ----------------------------------------------------------------
 * Test: Shape descriptor for blackbody
 * ---------------------------------------------------------------- */

static int test_shape_descriptor_blackbody(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Context creation failed");

    /* Generate blackbody at 6500K (similar to D65) */
    alwan_spd bb;
    int status = alwan_spd_blackbody(&bb, ctx, ALWAN_LITERAL(6500.0),
                                  ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 471);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create blackbody");

    /* Analyze shape */
    alwan_spd_shape shape;
    status = alwan_spd_analyze_shape(&shape, &bb);
    TEST_ASSERT(status == ALWAN_OK, "Failed to analyze blackbody shape");

    printf("  Blackbody 6500K Shape Descriptor:\n");
    printf("    Peak wavelength: %.2f nm\n", shape.peak_wavelength);
    printf("    Peak value:      %.6f\n", shape.peak_value);
    printf("    FWHM:            %.2f nm\n", shape.fwhm);
    printf("    Centroid:        %.2f nm\n", shape.centroid);
    printf("    Bandwidth:       %.2f nm\n", shape.bandwidth);

    /* Blackbody at 6500K follows Wien's displacement law:
     * Peak wavelength ~= 2898 / T(K) um = 2898 / 6500 ~= 0.446 um = 446 nm
     * Peak should be in blue-green range */

    TEST_ASSERT(shape.peak_wavelength >= ALWAN_LITERAL(400.0) &&
                shape.peak_wavelength <= ALWAN_LITERAL(500.0),
                "Blackbody peak wavelength not in expected range");

    TEST_ASSERT(shape.fwhm > ALWAN_LITERAL(100.0),
                "Blackbody FWHM too narrow");

    TEST_ASSERT(shape.peak_value > ALWAN_LITERAL(0.0),
                "Blackbody peak value should be positive");

    alwan_spd_destroy(ctx, &bb);
    alwan_destroy(ctx);
    TEST_PASS("Shape descriptor for blackbody");
}

/* ----------------------------------------------------------------
 * Main test entry point
 * ---------------------------------------------------------------- */

int test_39_spd_shape_main(void) {
    printf("\n========================================\n");
    printf("Test 39: SPD Shape Descriptors (P8.5)\n");
    printf("========================================\n\n");

    int result = 0;

    result = test_shape_descriptor_d65();
    if (result != 0) return result;

    result = test_shape_descriptor_led();
    if (result != 0) return result;

    result = test_shape_descriptor_blackbody();
    if (result != 0) return result;

    printf("\nAll SPD shape descriptor tests passed!\n");
    return 0;
}
