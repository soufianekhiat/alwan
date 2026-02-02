/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 40: P9 Gamut Analysis & Mapping
 *
 * Tests gamut analysis functions including Pointer's Gamut, spectral locus,
 * dominant wavelength, and excitation purity calculations.
 */

#include "alwan.h"
#include "alwan_internal.h"
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
 * P9.1: Pointer's Gamut Tests
 * ---------------------------------------------------------------- */

static int test_pointer_gamut_inside(void) {
    /* Test point inside Pointer's gamut (moderate saturation green) */
    alwan_vec2 xy_green = {{ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.5)}};

    int result = alwan_is_within_pointer_gamut(&xy_green);

    /* This point should be inside - it's a realizable surface color */
    TEST_ASSERT(result == 1, "Expected xy=[0.3, 0.5] to be inside Pointer's gamut");

    printf("  xy=[0.3, 0.5]: %s Pointer's gamut\n",
           result ? "INSIDE" : "OUTSIDE");

    TEST_PASS("Pointer's gamut inside check");
}

static int test_pointer_gamut_outside(void) {
    /* Test point outside Pointer's gamut (very saturated, near spectral locus) */
    alwan_vec2 xy_spectral = {{ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.8)}};

    int result = alwan_is_within_pointer_gamut(&xy_spectral);

    /* This point should be outside - it's too saturated for real surface colors */
    TEST_ASSERT(result == 0, "Expected xy=[0.1, 0.8] to be outside Pointer's gamut");

    printf("  xy=[0.1, 0.8]: %s Pointer's gamut\n",
           result ? "INSIDE" : "OUTSIDE");

    TEST_PASS("Pointer's gamut outside check");
}

static int test_pointer_gamut_boundary(void) {
    /* Test boundary retrieval */
    size_t count = 0;
    alwan_vec2 const *boundary = alwan_pointer_gamut_boundary(&count);

    TEST_ASSERT(boundary != NULL, "Boundary pointer is NULL");
    TEST_ASSERT(count == 32, "Boundary should have 32 points");

    /* Verify boundary points are valid xy chromaticity coordinates */
    for (size_t i = 0; i < count; i++) {
        TEST_ASSERT(boundary[i].v[0] >= ALWAN_LITERAL(0.0) && boundary[i].v[0] <= ALWAN_LITERAL(1.0),
                    "Boundary x out of range");
        TEST_ASSERT(boundary[i].v[1] >= ALWAN_LITERAL(0.0) && boundary[i].v[1] <= ALWAN_LITERAL(1.0),
                    "Boundary y out of range");
    }

    printf("  Boundary has %zu points\n", count);
    printf("  First point: [%.6f, %.6f]\n", boundary[0].v[0], boundary[0].v[1]);
    printf("  Last point:  [%.6f, %.6f]\n", boundary[count-1].v[0], boundary[count-1].v[1]);

    TEST_PASS("Pointer's gamut boundary retrieval");
}

/* ----------------------------------------------------------------
 * P9.2: Spectral Locus Tests
 * ---------------------------------------------------------------- */

static int test_spectral_locus_wavelengths(void) {
    /* Test specific wavelengths */
    alwan_scalar test_wavelengths[] = {
        ALWAN_LITERAL(400.0),
        ALWAN_LITERAL(500.0),
        ALWAN_LITERAL(550.0),
        ALWAN_LITERAL(600.0),
        ALWAN_LITERAL(650.0),
        ALWAN_LITERAL(700.0)
    };

    printf("  Testing spectral locus xy values:\n");

    for (size_t i = 0; i < 6; i++) {
        alwan_scalar wl = test_wavelengths[i];
        alwan_vec2 xy_out;

        int status = alwan_spectral_locus_xy(&xy_out, wl);
        TEST_ASSERT(status == ALWAN_OK, "Failed to compute spectral locus");

        /* Validate xy values are in valid range */
        TEST_ASSERT(xy_out.v[0] >= ALWAN_LITERAL(0.0) && xy_out.v[0] <= ALWAN_LITERAL(1.0),
                    "Spectral locus x out of range");
        TEST_ASSERT(xy_out.v[1] >= ALWAN_LITERAL(0.0) && xy_out.v[1] <= ALWAN_LITERAL(1.0),
                    "Spectral locus y out of range");

        printf("    %.0fnm: xy=[%.6f, %.6f]\n", wl, xy_out.v[0], xy_out.v[1]);
    }

    TEST_PASS("Spectral locus wavelength tests");
}

static int test_spectral_locus_interpolation(void) {
    /* Test interpolation with fractional wavelengths */
    alwan_vec2 xy_500, xy_500_5;

    int status1 = alwan_spectral_locus_xy(&xy_500, ALWAN_LITERAL(500.0));
    int status2 = alwan_spectral_locus_xy(&xy_500_5, ALWAN_LITERAL(500.5));

    TEST_ASSERT(status1 == ALWAN_OK && status2 == ALWAN_OK,
                "Failed to compute spectral locus for interpolation test");

    /* Values should be very close but not identical (fractional wavelength forces interpolation) */
    alwan_scalar diff_x = ALWAN_ABS(xy_500_5.v[0] - xy_500.v[0]);
    alwan_scalar diff_y = ALWAN_ABS(xy_500_5.v[1] - xy_500.v[1]);

    TEST_ASSERT(diff_x < ALWAN_LITERAL(0.1), "Interpolation x diff too large");
    TEST_ASSERT(diff_y < ALWAN_LITERAL(0.1), "Interpolation y diff too large");
    TEST_ASSERT(diff_x > ALWAN_LITERAL(0.0) || diff_y > ALWAN_LITERAL(0.0),
                "Interpolation resulted in identical values");

    printf("  500.0nm: xy=[%.6f, %.6f]\n", xy_500.v[0], xy_500.v[1]);
    printf("  500.5nm: xy=[%.6f, %.6f]\n", xy_500_5.v[0], xy_500_5.v[1]);
    printf("  Diff:    [%.6f, %.6f]\n", diff_x, diff_y);

    TEST_PASS("Spectral locus interpolation");
}

/* ----------------------------------------------------------------
 * P9.3: Dominant Wavelength & Excitation Purity Tests
 * ---------------------------------------------------------------- */

static int test_dominant_wavelength_green(void) {
    /* Test dominant wavelength for green */
    alwan_vec2 xy_green = {{ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.6)}};
    alwan_vec2 xy_d65 = {{ALWAN_LITERAL(0.31271), ALWAN_LITERAL(0.32902)}};

    alwan_scalar wl_out;
    alwan_vec2 xy_wl_out, xy_cw_out;

    int status = alwan_dominant_wavelength(&wl_out, &xy_wl_out, &xy_cw_out,
                                            &xy_green, &xy_d65);

    TEST_ASSERT(status == ALWAN_OK, "Failed to compute dominant wavelength");

    /* Validate wavelength is in visible range */
    TEST_ASSERT(wl_out >= ALWAN_LITERAL(360.0) && wl_out <= ALWAN_LITERAL(830.0),
                "Dominant wavelength out of range");

    /* Validate spectral locus point is in valid xy range */
    TEST_ASSERT(xy_wl_out.v[0] >= ALWAN_LITERAL(0.0) && xy_wl_out.v[0] <= ALWAN_LITERAL(1.0),
                "Spectral locus x out of range");
    TEST_ASSERT(xy_wl_out.v[1] >= ALWAN_LITERAL(0.0) && xy_wl_out.v[1] <= ALWAN_LITERAL(1.0),
                "Spectral locus y out of range");

    printf("  Green xy=[0.3, 0.6]:\n");
    printf("    Dominant wavelength: %.2fnm\n", wl_out);
    printf("    Spectral locus point: [%.6f, %.6f]\n",
           xy_wl_out.v[0], xy_wl_out.v[1]);

    TEST_PASS("Dominant wavelength for green");
}

static int test_excitation_purity_green(void) {
    /* Test excitation purity for green */
    alwan_vec2 xy_green = {{ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.6)}};
    alwan_vec2 xy_d65 = {{ALWAN_LITERAL(0.31271), ALWAN_LITERAL(0.32902)}};

    alwan_scalar purity_out;

    int status = alwan_excitation_purity(&purity_out, &xy_green, &xy_d65);
    TEST_ASSERT(status == ALWAN_OK, "Failed to compute excitation purity");

    /* Validate purity is in valid range [0, 1] */
    TEST_ASSERT(purity_out >= ALWAN_LITERAL(0.0) && purity_out <= ALWAN_LITERAL(1.0),
                "Excitation purity out of range");

    printf("  Green xy=[0.3, 0.6]:\n");
    printf("    Excitation purity: %.4f\n", purity_out);

    TEST_PASS("Excitation purity for green");
}

static int test_dominant_wavelength_red(void) {
    /* Test dominant wavelength for red */
    alwan_vec2 xy_red = {{ALWAN_LITERAL(0.6), ALWAN_LITERAL(0.3)}};
    alwan_vec2 xy_d65 = {{ALWAN_LITERAL(0.31271), ALWAN_LITERAL(0.32902)}};

    alwan_scalar wl_out;

    int status = alwan_dominant_wavelength(&wl_out, NULL, NULL, &xy_red, &xy_d65);
    TEST_ASSERT(status == ALWAN_OK, "Failed to compute dominant wavelength");

    /* Validate wavelength is in visible range */
    TEST_ASSERT(wl_out >= ALWAN_LITERAL(360.0) && wl_out <= ALWAN_LITERAL(830.0),
                "Dominant wavelength out of range");

    printf("  Red xy=[0.6, 0.3]: dominant wavelength = %.2fnm\n", wl_out);

    TEST_PASS("Dominant wavelength for red");
}

static int test_dominant_wavelength_blue(void) {
    /* Test dominant wavelength for blue */
    alwan_vec2 xy_blue = {{ALWAN_LITERAL(0.15), ALWAN_LITERAL(0.06)}};
    alwan_vec2 xy_d65 = {{ALWAN_LITERAL(0.31271), ALWAN_LITERAL(0.32902)}};

    alwan_scalar wl_out;

    int status = alwan_dominant_wavelength(&wl_out, NULL, NULL, &xy_blue, &xy_d65);
    TEST_ASSERT(status == ALWAN_OK, "Failed to compute dominant wavelength");

    /* Validate wavelength is in visible range */
    TEST_ASSERT(wl_out >= ALWAN_LITERAL(360.0) && wl_out <= ALWAN_LITERAL(830.0),
                "Dominant wavelength out of range");

    printf("  Blue xy=[0.15, 0.06]: dominant wavelength = %.2fnm\n", wl_out);

    TEST_PASS("Dominant wavelength for blue");
}

/* ----------------------------------------------------------------
 * P9.6: Gamut Coverage Metrics Tests
 * ---------------------------------------------------------------- */

static int test_gamut_volume_ratio(void) {
    /* Test volume ratio between sRGB and BT.2020 */
    alwan_rgb_space_desc srgb, bt2020;

    /* sRGB primaries (D65 white point) - primaries_xy[6]: rx, ry, gx, gy, bx, by */
    srgb.primaries_xy[0] = ALWAN_LITERAL(0.64);
    srgb.primaries_xy[1] = ALWAN_LITERAL(0.33);
    srgb.primaries_xy[2] = ALWAN_LITERAL(0.30);
    srgb.primaries_xy[3] = ALWAN_LITERAL(0.60);
    srgb.primaries_xy[4] = ALWAN_LITERAL(0.15);
    srgb.primaries_xy[5] = ALWAN_LITERAL(0.06);
    srgb.white_xy[0] = ALWAN_LITERAL(0.31271);
    srgb.white_xy[1] = ALWAN_LITERAL(0.32902);
    srgb.oetf = ALWAN_TF_LINEAR;
    srgb.eotf = ALWAN_TF_LINEAR;

    /* BT.2020 primaries (D65 white point) */
    bt2020.primaries_xy[0] = ALWAN_LITERAL(0.708);
    bt2020.primaries_xy[1] = ALWAN_LITERAL(0.292);
    bt2020.primaries_xy[2] = ALWAN_LITERAL(0.170);
    bt2020.primaries_xy[3] = ALWAN_LITERAL(0.797);
    bt2020.primaries_xy[4] = ALWAN_LITERAL(0.131);
    bt2020.primaries_xy[5] = ALWAN_LITERAL(0.046);
    bt2020.white_xy[0] = ALWAN_LITERAL(0.31271);
    bt2020.white_xy[1] = ALWAN_LITERAL(0.32902);
    bt2020.oetf = ALWAN_TF_LINEAR;
    bt2020.eotf = ALWAN_TF_LINEAR;

    alwan_scalar ratio_bt2020_srgb, ratio_srgb_bt2020;

    int status1 = alwan_gamut_volume_ratio(&ratio_bt2020_srgb, &bt2020, &srgb);
    int status2 = alwan_gamut_volume_ratio(&ratio_srgb_bt2020, &srgb, &bt2020);

    TEST_ASSERT(status1 == ALWAN_OK, "Failed to compute BT.2020/sRGB ratio");
    TEST_ASSERT(status2 == ALWAN_OK, "Failed to compute sRGB/BT.2020 ratio");

    /* BT.2020 has roughly 2x the volume of sRGB */
    TEST_ASSERT(ratio_bt2020_srgb > ALWAN_LITERAL(1.5) && ratio_bt2020_srgb < ALWAN_LITERAL(2.5),
                "BT.2020/sRGB ratio out of expected range");

    /* Ratios should be reciprocals */
    alwan_scalar product = ratio_bt2020_srgb * ratio_srgb_bt2020;
    TEST_ASSERT(ALWAN_ABS(product - ALWAN_LITERAL(1.0)) < ALWAN_LITERAL(0.01),
                "Ratios should be reciprocals");

    printf("  BT.2020 / sRGB volume ratio: %.4f\n", ratio_bt2020_srgb);
    printf("  sRGB / BT.2020 volume ratio: %.4f\n", ratio_srgb_bt2020);

    TEST_PASS("Gamut volume ratio");
}

static int test_gamut_coverage(void) {
    /* Test coverage of sRGB by BT.2020 and vice versa */
    alwan_rgb_space_desc srgb, bt2020;

    /* sRGB primaries (D65 white point) - primaries_xy[6]: rx, ry, gx, gy, bx, by */
    srgb.primaries_xy[0] = ALWAN_LITERAL(0.64);
    srgb.primaries_xy[1] = ALWAN_LITERAL(0.33);
    srgb.primaries_xy[2] = ALWAN_LITERAL(0.30);
    srgb.primaries_xy[3] = ALWAN_LITERAL(0.60);
    srgb.primaries_xy[4] = ALWAN_LITERAL(0.15);
    srgb.primaries_xy[5] = ALWAN_LITERAL(0.06);
    srgb.white_xy[0] = ALWAN_LITERAL(0.31271);
    srgb.white_xy[1] = ALWAN_LITERAL(0.32902);
    srgb.oetf = ALWAN_TF_LINEAR;
    srgb.eotf = ALWAN_TF_LINEAR;

    /* BT.2020 primaries (D65 white point) */
    bt2020.primaries_xy[0] = ALWAN_LITERAL(0.708);
    bt2020.primaries_xy[1] = ALWAN_LITERAL(0.292);
    bt2020.primaries_xy[2] = ALWAN_LITERAL(0.170);
    bt2020.primaries_xy[3] = ALWAN_LITERAL(0.797);
    bt2020.primaries_xy[4] = ALWAN_LITERAL(0.131);
    bt2020.primaries_xy[5] = ALWAN_LITERAL(0.046);
    bt2020.white_xy[0] = ALWAN_LITERAL(0.31271);
    bt2020.white_xy[1] = ALWAN_LITERAL(0.32902);
    bt2020.oetf = ALWAN_TF_LINEAR;
    bt2020.eotf = ALWAN_TF_LINEAR;

    alwan_scalar coverage_srgb_by_bt2020, coverage_bt2020_by_srgb;

    /* sRGB should be ~100% covered by BT.2020 (BT.2020 is wider) */
    int status1 = alwan_gamut_coverage(&coverage_srgb_by_bt2020, &srgb, &bt2020, 50000, 12345);
    TEST_ASSERT(status1 == ALWAN_OK, "Failed to compute sRGB coverage by BT.2020");
    TEST_ASSERT(coverage_srgb_by_bt2020 > ALWAN_LITERAL(95.0),
                "sRGB should be almost fully covered by BT.2020");

    /* BT.2020 should be ~50% covered by sRGB (sRGB is narrower) */
    int status2 = alwan_gamut_coverage(&coverage_bt2020_by_srgb, &bt2020, &srgb, 50000, 12345);
    TEST_ASSERT(status2 == ALWAN_OK, "Failed to compute BT.2020 coverage by sRGB");
    TEST_ASSERT(coverage_bt2020_by_srgb > ALWAN_LITERAL(30.0) &&
                coverage_bt2020_by_srgb < ALWAN_LITERAL(70.0),
                "BT.2020 coverage by sRGB out of expected range");

    printf("  sRGB covered by BT.2020: %.2f%%\n", coverage_srgb_by_bt2020);
    printf("  BT.2020 covered by sRGB: %.2f%%\n", coverage_bt2020_by_srgb);

    TEST_PASS("Gamut coverage");
}

/* ================================================================
 * P9.5: Advanced Gamut Mapping Tests
 * ================================================================ */

static int test_gamut_map_simple_clip(void) {
    /* Test simple clipping method */
    alwan_rgb_space_desc srgb;
    srgb.primaries_xy[0] = ALWAN_LITERAL(0.64);
    srgb.primaries_xy[1] = ALWAN_LITERAL(0.33);
    srgb.primaries_xy[2] = ALWAN_LITERAL(0.30);
    srgb.primaries_xy[3] = ALWAN_LITERAL(0.60);
    srgb.primaries_xy[4] = ALWAN_LITERAL(0.15);
    srgb.primaries_xy[5] = ALWAN_LITERAL(0.06);
    srgb.white_xy[0] = ALWAN_LITERAL(0.31271);
    srgb.white_xy[1] = ALWAN_LITERAL(0.32902);
    srgb.oetf = ALWAN_TF_LINEAR;
    srgb.eotf = ALWAN_TF_LINEAR;

    /* Test with out-of-gamut color (oversaturated red) */
    alwan_rgb rgb_in, rgb_out;
    rgb_in.r = ALWAN_LITERAL(1.5);
    rgb_in.g = ALWAN_LITERAL(-0.2);
    rgb_in.b = ALWAN_LITERAL(0.1);

    int status = alwan_gamut_map_advanced(&rgb_out, ALWAN_GAMUT_MAP_CLIP, &srgb, &rgb_in);

    TEST_ASSERT(status == ALWAN_OK, "Failed to clip RGB color");
    TEST_ASSERT(rgb_out.r == ALWAN_LITERAL(1.0), "Red component not clipped to 1.0");
    TEST_ASSERT(rgb_out.g == ALWAN_LITERAL(0.0), "Green component not clipped to 0.0");
    TEST_ASSERT(rgb_out.b >= ALWAN_LITERAL(0.0) && rgb_out.b <= ALWAN_LITERAL(1.0),
                "Blue component out of range");

    printf("  Simple clip: [1.5, -0.2, 0.1] -> [%.3f, %.3f, %.3f]\n",
           rgb_out.r, rgb_out.g, rgb_out.b);

    TEST_PASS("Simple gamut clipping");
}

static int test_gamut_map_adaptive_l0(void) {
    /* Test adaptive L0 method */
    alwan_rgb_space_desc srgb;
    srgb.primaries_xy[0] = ALWAN_LITERAL(0.64);
    srgb.primaries_xy[1] = ALWAN_LITERAL(0.33);
    srgb.primaries_xy[2] = ALWAN_LITERAL(0.30);
    srgb.primaries_xy[3] = ALWAN_LITERAL(0.60);
    srgb.primaries_xy[4] = ALWAN_LITERAL(0.15);
    srgb.primaries_xy[5] = ALWAN_LITERAL(0.06);
    srgb.white_xy[0] = ALWAN_LITERAL(0.31271);
    srgb.white_xy[1] = ALWAN_LITERAL(0.32902);
    srgb.oetf = ALWAN_TF_LINEAR;
    srgb.eotf = ALWAN_TF_LINEAR;

    /* Test with oversaturated green */
    alwan_rgb rgb_in, rgb_out;
    rgb_in.r = ALWAN_LITERAL(-0.3);
    rgb_in.g = ALWAN_LITERAL(1.8);
    rgb_in.b = ALWAN_LITERAL(-0.5);

    int status = alwan_gamut_map_advanced(&rgb_out, ALWAN_GAMUT_MAP_ADAPTIVE_L0, &srgb, &rgb_in);

    TEST_ASSERT(status == ALWAN_OK, "Failed to map RGB color with adaptive L0");
    TEST_ASSERT(rgb_out.r >= ALWAN_LITERAL(0.0) && rgb_out.r <= ALWAN_LITERAL(1.0),
                "Red component out of range");
    TEST_ASSERT(rgb_out.g >= ALWAN_LITERAL(0.0) && rgb_out.g <= ALWAN_LITERAL(1.0),
                "Green component out of range");
    TEST_ASSERT(rgb_out.b >= ALWAN_LITERAL(0.0) && rgb_out.b <= ALWAN_LITERAL(1.0),
                "Blue component out of range");

    /* Green should be the dominant component */
    TEST_ASSERT(rgb_out.g > rgb_out.r && rgb_out.g > rgb_out.b,
                "Green should be dominant component");

    printf("  Adaptive L0: [-0.3, 1.8, -0.5] -> [%.3f, %.3f, %.3f]\n",
           rgb_out.r, rgb_out.g, rgb_out.b);

    TEST_PASS("Adaptive L0 gamut mapping");
}

static int test_gamut_map_adaptive_cusp(void) {
    /* Test adaptive cusp method */
    alwan_rgb_space_desc srgb;
    srgb.primaries_xy[0] = ALWAN_LITERAL(0.64);
    srgb.primaries_xy[1] = ALWAN_LITERAL(0.33);
    srgb.primaries_xy[2] = ALWAN_LITERAL(0.30);
    srgb.primaries_xy[3] = ALWAN_LITERAL(0.60);
    srgb.primaries_xy[4] = ALWAN_LITERAL(0.15);
    srgb.primaries_xy[5] = ALWAN_LITERAL(0.06);
    srgb.white_xy[0] = ALWAN_LITERAL(0.31271);
    srgb.white_xy[1] = ALWAN_LITERAL(0.32902);
    srgb.oetf = ALWAN_TF_LINEAR;
    srgb.eotf = ALWAN_TF_LINEAR;

    /* Test with oversaturated blue */
    alwan_rgb rgb_in, rgb_out;
    rgb_in.r = ALWAN_LITERAL(-0.1);
    rgb_in.g = ALWAN_LITERAL(0.2);
    rgb_in.b = ALWAN_LITERAL(1.9);

    int status = alwan_gamut_map_advanced(&rgb_out, ALWAN_GAMUT_MAP_ADAPTIVE_CUSP, &srgb, &rgb_in);

    TEST_ASSERT(status == ALWAN_OK, "Failed to map RGB color with adaptive cusp");
    TEST_ASSERT(rgb_out.r >= ALWAN_LITERAL(0.0) && rgb_out.r <= ALWAN_LITERAL(1.0),
                "Red component out of range");
    TEST_ASSERT(rgb_out.g >= ALWAN_LITERAL(0.0) && rgb_out.g <= ALWAN_LITERAL(1.0),
                "Green component out of range");
    TEST_ASSERT(rgb_out.b >= ALWAN_LITERAL(0.0) && rgb_out.b <= ALWAN_LITERAL(1.0),
                "Blue component out of range");

    /* Blue should be the dominant component */
    TEST_ASSERT(rgb_out.b > rgb_out.r && rgb_out.b > rgb_out.g,
                "Blue should be dominant component");

    printf("  Adaptive cusp: [-0.1, 0.2, 1.9] -> [%.3f, %.3f, %.3f]\n",
           rgb_out.r, rgb_out.g, rgb_out.b);

    TEST_PASS("Adaptive cusp gamut mapping");
}

static int test_gamut_map_chroma_compress(void) {
    /* Test chroma compression method */
    alwan_rgb_space_desc srgb;
    srgb.primaries_xy[0] = ALWAN_LITERAL(0.64);
    srgb.primaries_xy[1] = ALWAN_LITERAL(0.33);
    srgb.primaries_xy[2] = ALWAN_LITERAL(0.30);
    srgb.primaries_xy[3] = ALWAN_LITERAL(0.60);
    srgb.primaries_xy[4] = ALWAN_LITERAL(0.15);
    srgb.primaries_xy[5] = ALWAN_LITERAL(0.06);
    srgb.white_xy[0] = ALWAN_LITERAL(0.31271);
    srgb.white_xy[1] = ALWAN_LITERAL(0.32902);
    srgb.oetf = ALWAN_TF_LINEAR;
    srgb.eotf = ALWAN_TF_LINEAR;

    /* Test with oversaturated cyan */
    alwan_rgb rgb_in, rgb_out;
    rgb_in.r = ALWAN_LITERAL(-0.4);
    rgb_in.g = ALWAN_LITERAL(1.6);
    rgb_in.b = ALWAN_LITERAL(1.7);

    int status = alwan_gamut_map_advanced(&rgb_out, ALWAN_GAMUT_MAP_CHROMA_COMPRESS, &srgb, &rgb_in);

    TEST_ASSERT(status == ALWAN_OK, "Failed to compress chroma");
    TEST_ASSERT(rgb_out.r >= ALWAN_LITERAL(0.0) && rgb_out.r <= ALWAN_LITERAL(1.0),
                "Red component out of range");
    TEST_ASSERT(rgb_out.g >= ALWAN_LITERAL(0.0) && rgb_out.g <= ALWAN_LITERAL(1.0),
                "Green component out of range");
    TEST_ASSERT(rgb_out.b >= ALWAN_LITERAL(0.0) && rgb_out.b <= ALWAN_LITERAL(1.0),
                "Blue component out of range");

    /* Cyan has high G and B, low R */
    TEST_ASSERT(rgb_out.g > rgb_out.r && rgb_out.b > rgb_out.r,
                "G and B should be higher than R for cyan");

    printf("  Chroma compress: [-0.4, 1.6, 1.7] -> [%.3f, %.3f, %.3f]\n",
           rgb_out.r, rgb_out.g, rgb_out.b);

    TEST_PASS("Chroma compression gamut mapping");
}

static int test_gamut_map_in_gamut(void) {
    /* Test that in-gamut colors are preserved */
    alwan_rgb_space_desc srgb;
    srgb.primaries_xy[0] = ALWAN_LITERAL(0.64);
    srgb.primaries_xy[1] = ALWAN_LITERAL(0.33);
    srgb.primaries_xy[2] = ALWAN_LITERAL(0.30);
    srgb.primaries_xy[3] = ALWAN_LITERAL(0.60);
    srgb.primaries_xy[4] = ALWAN_LITERAL(0.15);
    srgb.primaries_xy[5] = ALWAN_LITERAL(0.06);
    srgb.white_xy[0] = ALWAN_LITERAL(0.31271);
    srgb.white_xy[1] = ALWAN_LITERAL(0.32902);
    srgb.oetf = ALWAN_TF_LINEAR;
    srgb.eotf = ALWAN_TF_LINEAR;

    /* Test with valid in-gamut color */
    alwan_rgb rgb_in, rgb_out;
    rgb_in.r = ALWAN_LITERAL(0.7);
    rgb_in.g = ALWAN_LITERAL(0.3);
    rgb_in.b = ALWAN_LITERAL(0.5);

    int status = alwan_gamut_map_advanced(&rgb_out, ALWAN_GAMUT_MAP_ADAPTIVE_L0, &srgb, &rgb_in);

    TEST_ASSERT(status == ALWAN_OK, "Failed to map in-gamut color");

    /* In-gamut color should be mostly preserved */
    alwan_scalar diff_r = ALWAN_ABS(rgb_out.r - rgb_in.r);
    alwan_scalar diff_g = ALWAN_ABS(rgb_out.g - rgb_in.g);
    alwan_scalar diff_b = ALWAN_ABS(rgb_out.b - rgb_in.b);

    TEST_ASSERT(diff_r < ALWAN_LITERAL(0.01) && diff_g < ALWAN_LITERAL(0.01) && diff_b < ALWAN_LITERAL(0.01),
                "In-gamut color should be preserved");

    printf("  In-gamut: [%.3f, %.3f, %.3f] -> [%.3f, %.3f, %.3f]\n",
           rgb_in.r, rgb_in.g, rgb_in.b,
           rgb_out.r, rgb_out.g, rgb_out.b);

    TEST_PASS("In-gamut color preservation");
}

/* ----------------------------------------------------------------
 * Main test entry point
 * ---------------------------------------------------------------- */

int test_40_gamut_analysis_main(void) {
    printf("\n========================================\n");
    printf("Test 40: P9 Gamut Analysis & Mapping\n");
    printf("========================================\n\n");

    int result = 0;

    /* P9.1: Pointer's Gamut */
    printf("P9.1: Pointer's Gamut\n");
    printf("---------------------\n");
    result = test_pointer_gamut_inside();
    if (result != 0) return result;

    result = test_pointer_gamut_outside();
    if (result != 0) return result;

    result = test_pointer_gamut_boundary();
    if (result != 0) return result;

    /* P9.2: Spectral Locus */
    printf("\nP9.2: Spectral Locus\n");
    printf("--------------------\n");
    result = test_spectral_locus_wavelengths();
    if (result != 0) return result;

    result = test_spectral_locus_interpolation();
    if (result != 0) return result;

    /* P9.3: Dominant Wavelength & Excitation Purity */
    printf("\nP9.3: Dominant Wavelength & Excitation Purity\n");
    printf("----------------------------------------------\n");
    result = test_dominant_wavelength_green();
    if (result != 0) return result;

    result = test_excitation_purity_green();
    if (result != 0) return result;

    result = test_dominant_wavelength_red();
    if (result != 0) return result;

    result = test_dominant_wavelength_blue();
    if (result != 0) return result;

    /* P9.5: Advanced Gamut Mapping */
    printf("\nP9.5: Advanced Gamut Mapping\n");
    printf("----------------------------\n");
    result = test_gamut_map_simple_clip();
    if (result != 0) return result;

    result = test_gamut_map_adaptive_l0();
    if (result != 0) return result;

    result = test_gamut_map_adaptive_cusp();
    if (result != 0) return result;

    result = test_gamut_map_chroma_compress();
    if (result != 0) return result;

    result = test_gamut_map_in_gamut();
    if (result != 0) return result;

    /* P9.6: Gamut Coverage Metrics */
    printf("\nP9.6: Gamut Coverage Metrics\n");
    printf("----------------------------\n");
    result = test_gamut_volume_ratio();
    if (result != 0) return result;

    result = test_gamut_coverage();
    if (result != 0) return result;

    printf("\nAll P9 gamut analysis tests passed!\n");
    return 0;
}
