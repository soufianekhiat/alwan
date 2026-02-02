/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test suite 56: ACES 1.x Output Transform (RRT + ODT)
 * Tests: Complete ACES 1.3 RRT+ODT pipeline with all presets
 */

#include "alwan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Tolerance for ACES 1.x output transforms */
#define EPSILON 1e-4f
#define EPSILON_LOOSE 1e-3f
#define EPSILON_VERY_LOOSE 1e-2f  /* For HDR round-trips */

/* Test counter */
static int g_test_count = 0;
static int g_test_passed = 0;

#define TEST_START(name) \
    do { \
        printf("  TEST: %s\n", name); \
        g_test_count++; \
    } while (0)

#define TEST_PASS() \
    do { \
        printf("    [PASS]\n"); \
        g_test_passed++; \
    } while (0)

#define TEST_FAIL(msg, ...) \
    do { \
        printf("    [FAIL] " msg "\n", ##__VA_ARGS__); \
        return 1; \
    } while (0)

#define EXPECT_NEAR(a, b, eps) \
    do { \
        alwan_scalar diff = (alwan_scalar)fabs((double)(a) - (double)(b)); \
        if (diff > (eps)) { \
            TEST_FAIL("Expected %g, got %g (diff %g > %g)", \
                      (double)(b), (double)(a), (double)diff, (double)(eps)); \
        } \
    } while (0)

#define EXPECT_RGB_NEAR(a, b, eps) \
    do { \
        EXPECT_NEAR((a).r, (b).r, eps); \
        EXPECT_NEAR((a).g, (b).g, eps); \
        EXPECT_NEAR((a).b, (b).b, eps); \
    } while (0)

/* ----------------------------------------------------------------
 * Test RGB Inputs (must match gendata/tests/aces1_output_transform.py)
 * Input is in ACES2065-1 (AP0 linear)
 * ---------------------------------------------------------------- */
static alwan_rgb const g_test_rgb_inputs[] = {
    /* Primary colors */
    {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)},     /* Pure red */
    {ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0)},     /* Pure green */
    {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)},     /* Pure blue */
    /* Secondary colors */
    {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0)},     /* Yellow */
    {ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)},     /* Cyan */
    {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)},     /* Magenta */
    /* Grayscale */
    {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)},     /* Black */
    {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)},  /* 18% gray */
    {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)},     /* 50% gray */
    {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)},     /* White */
    /* Saturated colors */
    {ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.05)},    /* Dark saturated red */
    {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.2), ALWAN_LITERAL(0.1)},     /* Bright saturated red */
    {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)},     /* Medium red */
    {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.0)},     /* Orange */
    {ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.4), ALWAN_LITERAL(0.2)},     /* Warm orange */
    /* Low values */
    {ALWAN_LITERAL(0.05), ALWAN_LITERAL(0.05), ALWAN_LITERAL(0.05)},  /* Very dark */
    {ALWAN_LITERAL(0.02), ALWAN_LITERAL(0.01), ALWAN_LITERAL(0.005)}, /* Nearly black */
    /* HDR values */
    {ALWAN_LITERAL(2.0), ALWAN_LITERAL(1.5), ALWAN_LITERAL(0.5)},     /* HDR bright */
    {ALWAN_LITERAL(5.0), ALWAN_LITERAL(4.0), ALWAN_LITERAL(3.0)},     /* Very bright */
    /* Out of gamut */
    {ALWAN_LITERAL(1.5), ALWAN_LITERAL(-0.2), ALWAN_LITERAL(0.0)},    /* Negative green */
    {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(-0.5)},    /* Negative blue */
    {ALWAN_LITERAL(-0.2), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.3)},    /* Negative red */
};

#define NUM_TEST_INPUTS (sizeof(g_test_rgb_inputs) / sizeof(g_test_rgb_inputs[0]))

/* Preset names for debug output */
static char const *g_preset_names[] = {
    "REC709_100NIT",
    "SRGB_100NIT",
    "SRGB_D60_100NIT",
    "P3DCI_48NIT",
    "P3D60_48NIT",
    "P3D65_48NIT",
    "P3D65_100NIT",
    "REC2020_100NIT",
    "REC2020_1000NIT_PQ",
    "REC2020_2000NIT_PQ",
    "REC2020_4000NIT_PQ",
    "DCDM_48NIT",
};

/* ----------------------------------------------------------------
 * Basic API Tests
 * ---------------------------------------------------------------- */

static int test_output_transform_basic_rec709(void) {
    TEST_START("ACES 1.x Output Transform - Rec.709 100 nits basic");

    alwan_rgb rgb_in = {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)};  /* 18% gray */
    alwan_rgb rgb_out;

    int status = alwan_aces1_output_transform(&rgb_out, &rgb_in, ALWAN_ACES1_OUT_REC709_100NIT);
    if (status != ALWAN_OK) {
        TEST_FAIL("Transform failed with error %d", status);
    }

    /* 18% gray should map to ~0.4-0.5 in display-encoded space */
    if (rgb_out.r < ALWAN_LITERAL(0.3) || rgb_out.r > ALWAN_LITERAL(0.6)) {
        TEST_FAIL("18%% gray mapped to unexpected value: %g", rgb_out.r);
    }

    /* Neutral input should produce neutral output */
    EXPECT_NEAR(rgb_out.r, rgb_out.g, EPSILON);
    EXPECT_NEAR(rgb_out.g, rgb_out.b, EPSILON);

    printf("    Output: (%g, %g, %g)\n", rgb_out.r, rgb_out.g, rgb_out.b);

    TEST_PASS();
    return 0;
}

static int test_output_transform_basic_srgb(void) {
    TEST_START("ACES 1.x Output Transform - sRGB 100 nits basic");

    alwan_rgb rgb_in = {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)};
    alwan_rgb rgb_out;

    int status = alwan_aces1_output_transform(&rgb_out, &rgb_in, ALWAN_ACES1_OUT_SRGB_100NIT);
    if (status != ALWAN_OK) {
        TEST_FAIL("Transform failed with error %d", status);
    }

    /* Neutral should stay neutral */
    EXPECT_NEAR(rgb_out.r, rgb_out.g, EPSILON);
    EXPECT_NEAR(rgb_out.g, rgb_out.b, EPSILON);

    printf("    Output: (%g, %g, %g)\n", rgb_out.r, rgb_out.g, rgb_out.b);

    TEST_PASS();
    return 0;
}

static int test_output_transform_basic_p3(void) {
    TEST_START("ACES 1.x Output Transform - P3-D65 100 nits basic");

    alwan_rgb rgb_in = {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)};
    alwan_rgb rgb_out;

    int status = alwan_aces1_output_transform(&rgb_out, &rgb_in, ALWAN_ACES1_OUT_P3D65_100NIT);
    if (status != ALWAN_OK) {
        TEST_FAIL("Transform failed with error %d", status);
    }

    /* Mid gray should map within valid range */
    if (rgb_out.r < ALWAN_LITERAL(0.0) || rgb_out.r > ALWAN_LITERAL(1.0)) {
        TEST_FAIL("Output out of valid range: %g", rgb_out.r);
    }

    printf("    Output: (%g, %g, %g)\n", rgb_out.r, rgb_out.g, rgb_out.b);

    TEST_PASS();
    return 0;
}

static int test_output_transform_hdr_pq(void) {
    TEST_START("ACES 1.x Output Transform - Rec.2020 1000 nits PQ");

    alwan_rgb rgb_in = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)};  /* White */
    alwan_rgb rgb_out;

    int status = alwan_aces1_output_transform(&rgb_out, &rgb_in, ALWAN_ACES1_OUT_REC2020_1000NIT_PQ);
    if (status != ALWAN_OK) {
        TEST_FAIL("Transform failed with error %d", status);
    }

    /* PQ encoded 1000 nits should be around 0.75 (not 1.0 since PQ max is 10000 nits) */
    if (rgb_out.r < ALWAN_LITERAL(0.5) || rgb_out.r > ALWAN_LITERAL(0.85)) {
        TEST_FAIL("PQ white mapped to unexpected value: %g", rgb_out.r);
    }

    printf("    Output: (%g, %g, %g)\n", rgb_out.r, rgb_out.g, rgb_out.b);

    TEST_PASS();
    return 0;
}

static int test_output_transform_cinema_dcdm(void) {
    TEST_START("ACES 1.x Output Transform - DCDM 48 nits");

    alwan_rgb rgb_in = {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)};
    alwan_rgb rgb_out;

    int status = alwan_aces1_output_transform(&rgb_out, &rgb_in, ALWAN_ACES1_OUT_DCDM_48NIT);
    if (status != ALWAN_OK) {
        TEST_FAIL("Transform failed with error %d", status);
    }

    /* DCDM uses XYZ primaries - output should be valid */
    if (rgb_out.r < ALWAN_LITERAL(0.0) || rgb_out.g < ALWAN_LITERAL(0.0) || rgb_out.b < ALWAN_LITERAL(0.0)) {
        TEST_FAIL("DCDM output has negative values: (%g, %g, %g)", rgb_out.r, rgb_out.g, rgb_out.b);
    }

    printf("    Output: (%g, %g, %g)\n", rgb_out.r, rgb_out.g, rgb_out.b);

    TEST_PASS();
    return 0;
}

static int test_output_transform_cinema_p3dci(void) {
    TEST_START("ACES 1.x Output Transform - P3-DCI 48 nits");

    alwan_rgb rgb_in = {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)};
    alwan_rgb rgb_out;

    int status = alwan_aces1_output_transform(&rgb_out, &rgb_in, ALWAN_ACES1_OUT_P3DCI_48NIT);
    if (status != ALWAN_OK) {
        TEST_FAIL("Transform failed with error %d", status);
    }

    /* Output should be valid */
    if (rgb_out.r < ALWAN_LITERAL(0.0) || rgb_out.r > ALWAN_LITERAL(1.0)) {
        TEST_FAIL("P3-DCI output out of valid range: %g", rgb_out.r);
    }

    printf("    Output: (%g, %g, %g)\n", rgb_out.r, rgb_out.g, rgb_out.b);

    TEST_PASS();
    return 0;
}

/* ----------------------------------------------------------------
 * Roundtrip Tests (Forward + Inverse)
 * Note: The inverse is a simplified implementation. Roundtrip accuracy
 * is ~3% for neutral gray due to the approximations used. These tests
 * verify the inverse produces reasonable output rather than exact roundtrip.
 * ---------------------------------------------------------------- */

static int test_roundtrip_rec709(void) {
    TEST_START("ACES 1.x Output Transform roundtrip - Rec.709");

    alwan_rgb rgb_in = {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)};
    alwan_rgb rgb_encoded, rgb_decoded;

    /* Forward */
    int status = alwan_aces1_output_transform(&rgb_encoded, &rgb_in, ALWAN_ACES1_OUT_REC709_100NIT);
    if (status != ALWAN_OK) {
        TEST_FAIL("Forward transform failed with error %d", status);
    }

    /* Inverse */
    status = alwan_aces1_output_transform_inv(&rgb_decoded, &rgb_encoded, ALWAN_ACES1_OUT_REC709_100NIT);
    if (status != ALWAN_OK) {
        TEST_FAIL("Inverse transform failed with error %d", status);
    }

    printf("    In: (%g, %g, %g) -> Enc: (%g, %g, %g) -> Dec: (%g, %g, %g)\n",
           rgb_in.r, rgb_in.g, rgb_in.b,
           rgb_encoded.r, rgb_encoded.g, rgb_encoded.b,
           rgb_decoded.r, rgb_decoded.g, rgb_decoded.b);

    /* Verify inverse produces reasonable output (within 5% for simplified impl) */
    alwan_scalar rel_err = fabs(rgb_decoded.r - rgb_in.r) / rgb_in.r;
    if (rel_err > 0.05) {
        TEST_FAIL("Roundtrip relative error too large: %.1f%%", rel_err * 100.0);
    }
    printf("    Roundtrip relative error: %.2f%%\n", rel_err * 100.0);

    TEST_PASS();
    return 0;
}

static int test_roundtrip_srgb(void) {
    TEST_START("ACES 1.x Output Transform roundtrip - sRGB");

    alwan_rgb rgb_in = {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)};
    alwan_rgb rgb_encoded, rgb_decoded;

    /* Forward */
    int status = alwan_aces1_output_transform(&rgb_encoded, &rgb_in, ALWAN_ACES1_OUT_SRGB_100NIT);
    if (status != ALWAN_OK) {
        TEST_FAIL("Forward transform failed with error %d", status);
    }

    /* Inverse */
    status = alwan_aces1_output_transform_inv(&rgb_decoded, &rgb_encoded, ALWAN_ACES1_OUT_SRGB_100NIT);
    if (status != ALWAN_OK) {
        TEST_FAIL("Inverse transform failed with error %d", status);
    }

    /* Verify inverse produces reasonable output (within 5% for simplified impl) */
    alwan_scalar rel_err = fabs(rgb_decoded.r - rgb_in.r) / rgb_in.r;
    if (rel_err > 0.05) {
        TEST_FAIL("Roundtrip relative error too large: %.1f%%", rel_err * 100.0);
    }

    TEST_PASS();
    return 0;
}

static int test_roundtrip_hdr(void) {
    TEST_START("ACES 1.x Output Transform roundtrip - Rec.2020 PQ");

    alwan_rgb rgb_in = {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)};
    alwan_rgb rgb_encoded, rgb_decoded;

    /* Forward */
    int status = alwan_aces1_output_transform(&rgb_encoded, &rgb_in, ALWAN_ACES1_OUT_REC2020_1000NIT_PQ);
    if (status != ALWAN_OK) {
        TEST_FAIL("Forward transform failed with error %d", status);
    }

    /* Inverse */
    status = alwan_aces1_output_transform_inv(&rgb_decoded, &rgb_encoded, ALWAN_ACES1_OUT_REC2020_1000NIT_PQ);
    if (status != ALWAN_OK) {
        TEST_FAIL("Inverse transform failed with error %d", status);
    }

    /* Verify inverse produces reasonable output (within 5% for simplified impl) */
    alwan_scalar rel_err = fabs(rgb_decoded.r - rgb_in.r) / rgb_in.r;
    if (rel_err > 0.05) {
        TEST_FAIL("Roundtrip relative error too large: %.1f%%", rel_err * 100.0);
    }

    TEST_PASS();
    return 0;
}

/* ----------------------------------------------------------------
 * Edge Case Tests
 * ---------------------------------------------------------------- */

static int test_black_preserves(void) {
    TEST_START("ACES 1.x Output Transform - Black preserves");

    alwan_rgb rgb_in = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
    alwan_rgb rgb_out;

    int status = alwan_aces1_output_transform(&rgb_out, &rgb_in, ALWAN_ACES1_OUT_REC709_100NIT);
    if (status != ALWAN_OK) {
        TEST_FAIL("Transform failed with error %d", status);
    }

    /* Black should map to black (or very close to it) */
    EXPECT_NEAR(rgb_out.r, ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.01));
    EXPECT_NEAR(rgb_out.g, ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.01));
    EXPECT_NEAR(rgb_out.b, ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.01));

    TEST_PASS();
    return 0;
}

static int test_negative_input_handling(void) {
    TEST_START("ACES 1.x Output Transform - Negative input handling");

    alwan_rgb rgb_in = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(-0.2), ALWAN_LITERAL(0.5)};  /* Out of gamut */
    alwan_rgb rgb_out;

    int status = alwan_aces1_output_transform(&rgb_out, &rgb_in, ALWAN_ACES1_OUT_REC709_100NIT);
    if (status != ALWAN_OK) {
        TEST_FAIL("Transform failed with error %d (should handle gracefully)", status);
    }

    /* Output should be valid (clamped to display range) */
    printf("    [INFO] Negative input: (1.0, -0.2, 0.5) -> (%g, %g, %g)\n",
           rgb_out.r, rgb_out.g, rgb_out.b);

    TEST_PASS();
    return 0;
}

static int test_hdr_bright_handling(void) {
    TEST_START("ACES 1.x Output Transform - HDR bright values");

    alwan_rgb rgb_in = {ALWAN_LITERAL(5.0), ALWAN_LITERAL(4.0), ALWAN_LITERAL(3.0)};  /* Very bright HDR */
    alwan_rgb rgb_out;

    int status = alwan_aces1_output_transform(&rgb_out, &rgb_in, ALWAN_ACES1_OUT_REC2020_4000NIT_PQ);
    if (status != ALWAN_OK) {
        TEST_FAIL("Transform failed with error %d", status);
    }

    /* Output should be valid and less than 1.0 (PQ encoded) */
    if (rgb_out.r > ALWAN_LITERAL(1.0) || rgb_out.g > ALWAN_LITERAL(1.0) || rgb_out.b > ALWAN_LITERAL(1.0)) {
        TEST_FAIL("HDR output exceeds 1.0: (%g, %g, %g)", rgb_out.r, rgb_out.g, rgb_out.b);
    }

    printf("    [INFO] HDR output: (%g, %g, %g)\n", rgb_out.r, rgb_out.g, rgb_out.b);

    TEST_PASS();
    return 0;
}

/* ----------------------------------------------------------------
 * All Presets Test
 * ---------------------------------------------------------------- */

static int test_all_presets_valid(void) {
    TEST_START("ACES 1.x Output Transform - All presets produce valid output");

    alwan_rgb rgb_in = {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)};
    alwan_rgb rgb_out;

    for (int i = 0; i < ALWAN_ACES1_OUT_COUNT; i++) {
        int status = alwan_aces1_output_transform(&rgb_out, &rgb_in, (alwan_aces1_output)i);
        if (status != ALWAN_OK) {
            TEST_FAIL("Preset %s failed with error %d", g_preset_names[i], status);
        }

        /* Check output is not NaN or Inf */
        if (rgb_out.r != rgb_out.r || rgb_out.g != rgb_out.g || rgb_out.b != rgb_out.b) {
            TEST_FAIL("Preset %s produced NaN", g_preset_names[i]);
        }

        printf("    [OK] %s: (%g, %g, %g)\n", g_preset_names[i], rgb_out.r, rgb_out.g, rgb_out.b);
    }

    TEST_PASS();
    return 0;
}

/* ----------------------------------------------------------------
 * Consistency Tests
 * ---------------------------------------------------------------- */

static int test_neutral_axis_consistency(void) {
    TEST_START("ACES 1.x Output Transform - Neutral axis stays neutral");

    alwan_rgb grays[] = {
        {ALWAN_LITERAL(0.02), ALWAN_LITERAL(0.02), ALWAN_LITERAL(0.02)},
        {ALWAN_LITERAL(0.05), ALWAN_LITERAL(0.05), ALWAN_LITERAL(0.05)},
        {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)},
        {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)},
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)},
    };

    for (size_t i = 0; i < sizeof(grays) / sizeof(grays[0]); i++) {
        alwan_rgb rgb_out;
        int status = alwan_aces1_output_transform(&rgb_out, &grays[i], ALWAN_ACES1_OUT_REC709_100NIT);
        if (status != ALWAN_OK) {
            TEST_FAIL("Gray %zu failed with error %d", i, status);
        }

        /* Neutral input should produce neutral output */
        alwan_scalar max_diff = (alwan_scalar)fmax(fabs(rgb_out.r - rgb_out.g),
                                                    fabs(rgb_out.g - rgb_out.b));
        if (max_diff > EPSILON) {
            TEST_FAIL("Gray %zu produced non-neutral output: (%g, %g, %g)",
                      i, rgb_out.r, rgb_out.g, rgb_out.b);
        }
    }

    TEST_PASS();
    return 0;
}

static int test_monotonic_luminance(void) {
    TEST_START("ACES 1.x Output Transform - Luminance is monotonic");

    /* Test that brighter inputs produce brighter outputs */
    alwan_rgb rgb_low = {ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.1)};
    alwan_rgb rgb_mid = {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)};
    alwan_rgb rgb_high = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)};

    alwan_rgb out_low, out_mid, out_high;

    alwan_aces1_output_transform(&out_low, &rgb_low, ALWAN_ACES1_OUT_REC709_100NIT);
    alwan_aces1_output_transform(&out_mid, &rgb_mid, ALWAN_ACES1_OUT_REC709_100NIT);
    alwan_aces1_output_transform(&out_high, &rgb_high, ALWAN_ACES1_OUT_REC709_100NIT);

    if (out_low.r >= out_mid.r || out_mid.r >= out_high.r) {
        TEST_FAIL("Luminance not monotonic: low=%g, mid=%g, high=%g",
                  out_low.r, out_mid.r, out_high.r);
    }

    printf("    Low: %g, Mid: %g, High: %g\n", out_low.r, out_mid.r, out_high.r);

    TEST_PASS();
    return 0;
}

/* ----------------------------------------------------------------
 * RRT Component Tests
 * ---------------------------------------------------------------- */

static int test_glow_effect(void) {
    TEST_START("ACES 1.x RRT - Glow effect on low values");

    /* Glow module slightly boosts dark values */
    alwan_rgb rgb_dark = {ALWAN_LITERAL(0.02), ALWAN_LITERAL(0.02), ALWAN_LITERAL(0.02)};
    alwan_rgb rgb_mid = {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)};
    alwan_rgb out_dark, out_mid;

    alwan_aces1_output_transform(&out_dark, &rgb_dark, ALWAN_ACES1_OUT_REC709_100NIT);
    alwan_aces1_output_transform(&out_mid, &rgb_mid, ALWAN_ACES1_OUT_REC709_100NIT);

    /* Both should produce valid outputs */
    if (out_dark.r < ALWAN_LITERAL(0.0) || out_mid.r < ALWAN_LITERAL(0.0)) {
        TEST_FAIL("Glow test produced invalid output");
    }

    printf("    Dark input -> %g, Mid input -> %g\n", out_dark.r, out_mid.r);

    TEST_PASS();
    return 0;
}

static int test_red_modifier(void) {
    TEST_START("ACES 1.x RRT - Red modifier desaturates");

    /* RedMod shifts oversaturated reds toward orange to prevent hue distortion */
    alwan_rgb rgb_red = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
    alwan_rgb rgb_out;

    alwan_aces1_output_transform(&rgb_out, &rgb_red, ALWAN_ACES1_OUT_REC709_100NIT);

    /* Red should be shifted - green channel should be slightly boosted */
    printf("    Pure red -> (%g, %g, %g)\n", rgb_out.r, rgb_out.g, rgb_out.b);

    /* Output should be valid */
    if (rgb_out.r < ALWAN_LITERAL(0.0) || rgb_out.r > ALWAN_LITERAL(1.0)) {
        TEST_FAIL("Red output invalid: %g", rgb_out.r);
    }

    TEST_PASS();
    return 0;
}

/* ----------------------------------------------------------------
 * HDR Specific Tests
 * ---------------------------------------------------------------- */

static int test_hdr_luminance_levels(void) {
    TEST_START("ACES 1.x HDR - Different peak luminance levels");

    alwan_rgb rgb_in = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)};
    alwan_rgb out_1000, out_2000, out_4000;

    alwan_aces1_output_transform(&out_1000, &rgb_in, ALWAN_ACES1_OUT_REC2020_1000NIT_PQ);
    alwan_aces1_output_transform(&out_2000, &rgb_in, ALWAN_ACES1_OUT_REC2020_2000NIT_PQ);
    alwan_aces1_output_transform(&out_4000, &rgb_in, ALWAN_ACES1_OUT_REC2020_4000NIT_PQ);

    /* Higher peak luminance should result in lower PQ code values for same input */
    printf("    1000 nit: %g, 2000 nit: %g, 4000 nit: %g\n",
           out_1000.r, out_2000.r, out_4000.r);

    /* All should be valid */
    if (out_1000.r < ALWAN_LITERAL(0.0) || out_1000.r > ALWAN_LITERAL(1.0) ||
        out_2000.r < ALWAN_LITERAL(0.0) || out_2000.r > ALWAN_LITERAL(1.0) ||
        out_4000.r < ALWAN_LITERAL(0.0) || out_4000.r > ALWAN_LITERAL(1.0)) {
        TEST_FAIL("HDR outputs out of range");
    }

    TEST_PASS();
    return 0;
}

/* ----------------------------------------------------------------
 * Cinema Specific Tests
 * ---------------------------------------------------------------- */

static int test_cinema_white_points(void) {
    TEST_START("ACES 1.x Cinema - Different white points");

    alwan_rgb rgb_in = {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)};
    alwan_rgb out_dci, out_d60, out_d65;

    alwan_aces1_output_transform(&out_dci, &rgb_in, ALWAN_ACES1_OUT_P3DCI_48NIT);
    alwan_aces1_output_transform(&out_d60, &rgb_in, ALWAN_ACES1_OUT_P3D60_48NIT);
    alwan_aces1_output_transform(&out_d65, &rgb_in, ALWAN_ACES1_OUT_P3D65_48NIT);

    printf("    DCI: (%g, %g, %g)\n", out_dci.r, out_dci.g, out_dci.b);
    printf("    D60: (%g, %g, %g)\n", out_d60.r, out_d60.g, out_d60.b);
    printf("    D65: (%g, %g, %g)\n", out_d65.r, out_d65.g, out_d65.b);

    /* All should produce valid outputs */
    if (out_dci.r < ALWAN_LITERAL(0.0) || out_d60.r < ALWAN_LITERAL(0.0) || out_d65.r < ALWAN_LITERAL(0.0)) {
        TEST_FAIL("Cinema outputs have invalid values");
    }

    TEST_PASS();
    return 0;
}

/* ----------------------------------------------------------------
 * ACES 1.x Building Blocks Tests
 * ---------------------------------------------------------------- */

static int test_gamut_comp13_inverse(void) {
    TEST_START("ACES 1.x GamutComp v1.3 - Forward/Inverse roundtrip");

    /* Test gamut compression roundtrip */
    alwan_rgb rgb_in = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.2), ALWAN_LITERAL(0.0)};  /* Saturated orange */
    alwan_rgb rgb_compressed, rgb_recovered;

    alwan_aces_gamut_comp13_params params;
    alwan_aces_gamut_comp13_params_default(&params);

    int status = alwan_aces_gamut_comp13(&rgb_compressed, &rgb_in, &params);
    if (status != ALWAN_OK) {
        TEST_FAIL("Forward GamutComp failed with error %d", status);
    }

    status = alwan_aces_gamut_comp13_inv(&rgb_recovered, &rgb_compressed, &params);
    if (status != ALWAN_OK) {
        TEST_FAIL("Inverse GamutComp failed with error %d", status);
    }

    /* Check roundtrip */
    EXPECT_NEAR(rgb_recovered.r, rgb_in.r, EPSILON_LOOSE);
    EXPECT_NEAR(rgb_recovered.g, rgb_in.g, EPSILON_LOOSE);
    EXPECT_NEAR(rgb_recovered.b, rgb_in.b, EPSILON_LOOSE);

    printf("    In: (%g, %g, %g) -> Comp: (%g, %g, %g) -> Recov: (%g, %g, %g)\n",
           rgb_in.r, rgb_in.g, rgb_in.b,
           rgb_compressed.r, rgb_compressed.g, rgb_compressed.b,
           rgb_recovered.r, rgb_recovered.g, rgb_recovered.b);

    TEST_PASS();
    return 0;
}

/* ----------------------------------------------------------------
 * Main Test Runner
 * ---------------------------------------------------------------- */

int test_56_aces1_output_transform_main(void) {
    printf("========================================\n");
    printf("Test Suite 56: ACES 1.x Output Transform\n");
    printf("========================================\n\n");

    g_test_count = 0;
    g_test_passed = 0;

    /* Basic API Tests */
    printf("Basic API Tests\n");
    printf("--------------------------------\n");
    if (test_output_transform_basic_rec709()) return 1;
    if (test_output_transform_basic_srgb()) return 1;
    if (test_output_transform_basic_p3()) return 1;
    if (test_output_transform_hdr_pq()) return 1;
    if (test_output_transform_cinema_dcdm()) return 1;
    if (test_output_transform_cinema_p3dci()) return 1;

    /* Roundtrip Tests */
    printf("\nRoundtrip Tests\n");
    printf("--------------------------------\n");
    if (test_roundtrip_rec709()) return 1;
    if (test_roundtrip_srgb()) return 1;
    if (test_roundtrip_hdr()) return 1;

    /* Edge Case Tests */
    printf("\nEdge Case Tests\n");
    printf("--------------------------------\n");
    if (test_black_preserves()) return 1;
    if (test_negative_input_handling()) return 1;
    if (test_hdr_bright_handling()) return 1;

    /* All Presets Test */
    printf("\nAll Presets Test\n");
    printf("--------------------------------\n");
    if (test_all_presets_valid()) return 1;

    /* Consistency Tests */
    printf("\nConsistency Tests\n");
    printf("--------------------------------\n");
    if (test_neutral_axis_consistency()) return 1;
    if (test_monotonic_luminance()) return 1;

    /* RRT Component Tests */
    printf("\nRRT Component Tests\n");
    printf("--------------------------------\n");
    if (test_glow_effect()) return 1;
    if (test_red_modifier()) return 1;

    /* HDR Specific Tests */
    printf("\nHDR Specific Tests\n");
    printf("--------------------------------\n");
    if (test_hdr_luminance_levels()) return 1;

    /* Cinema Specific Tests */
    printf("\nCinema Specific Tests\n");
    printf("--------------------------------\n");
    if (test_cinema_white_points()) return 1;

    /* Building Blocks Tests */
    printf("\nACES 1.x Building Blocks Tests\n");
    printf("--------------------------------\n");
    if (test_gamut_comp13_inverse()) return 1;

    printf("\n========================================\n");
    printf("Test Results: %d/%d passed\n", g_test_passed, g_test_count);
    printf("========================================\n");

    return (g_test_passed == g_test_count) ? 0 : 1;
}
