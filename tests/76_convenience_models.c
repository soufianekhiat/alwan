/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 76: Extended Convenience Models
 * HLC, Cubehelix, HSLuv, HPLuv, Okhsl, Okhsv
 */

#include "test_common.h"
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * HLC Tests (cylindrical CIELAB reordering)
 * ---------------------------------------------------------------- */

static int test_hlc_roundtrip(void) {
    alwan_lch lch_in = {50.0, 30.0, 120.0};
    alwan_hlc hlc;
    alwan_lch lch_out;

    alwan_lch_to_hlc(&hlc, &lch_in);
    TEST_ASSERT(ALWAN_ABS(hlc.H - ALWAN_LITERAL(120.0)) < ALWAN_LITERAL(1e-10), "HLC hue mismatch");
    TEST_ASSERT(ALWAN_ABS(hlc.L - ALWAN_LITERAL(50.0)) < ALWAN_LITERAL(1e-10), "HLC lightness mismatch");
    TEST_ASSERT(ALWAN_ABS(hlc.C - ALWAN_LITERAL(30.0)) < ALWAN_LITERAL(1e-10), "HLC chroma mismatch");

    alwan_hlc_to_lch(&lch_out, &hlc);
    TEST_ASSERT(ALWAN_ABS(lch_out.L - lch_in.L) < ALWAN_LITERAL(1e-10), "LCH roundtrip L mismatch");
    TEST_ASSERT(ALWAN_ABS(lch_out.C - lch_in.C) < ALWAN_LITERAL(1e-10), "LCH roundtrip C mismatch");
    TEST_ASSERT(ALWAN_ABS(lch_out.h - lch_in.h) < ALWAN_LITERAL(1e-10), "LCH roundtrip h mismatch");

    TEST_PASS("HLC <-> LCH roundtrip");
}

/* ----------------------------------------------------------------
 * Cubehelix Tests
 * ---------------------------------------------------------------- */

static int test_cubehelix_roundtrip(void) {
    /* Test several cubehelix colors for roundtrip accuracy */
    alwan_cubehelix test_inputs[] = {
        {  0.0, 0.0, 0.5},   /* gray (s=0) */
        {  0.0, 1.0, 0.5},   /* full saturation mid lightness */
        {120.0, 0.8, 0.3},   /* green-ish, dark */
        {240.0, 0.6, 0.7},   /* blue-ish, light */
        {  0.0, 0.0, 0.0},   /* black */
        {  0.0, 0.0, 1.0}    /* white */
    };
    size_t const num_tests = sizeof(test_inputs) / sizeof(test_inputs[0]);

    for (size_t i = 0; i < num_tests; i++) {
        alwan_rgb rgb;
        alwan_cubehelix_to_rgb(&rgb, &test_inputs[i]);

        /* Gray (s=0) should produce equal channels */
        if (i == 0) {
            TEST_ASSERT(ALWAN_ABS(rgb.r - ALWAN_LITERAL(0.5)) < ALWAN_LITERAL(0.01) &&
                       ALWAN_ABS(rgb.g - ALWAN_LITERAL(0.5)) < ALWAN_LITERAL(0.01) &&
                       ALWAN_ABS(rgb.b - ALWAN_LITERAL(0.5)) < ALWAN_LITERAL(0.01),
                       "Cubehelix: s=0 should produce gray");
        }

        /* Black */
        if (i == 4) {
            TEST_ASSERT(rgb.r < ALWAN_LITERAL(0.01) && rgb.g < ALWAN_LITERAL(0.01) &&
                       rgb.b < ALWAN_LITERAL(0.01), "Cubehelix: l=0 should be black");
        }

        /* White */
        if (i == 5) {
            TEST_ASSERT(rgb.r > ALWAN_LITERAL(0.99) && rgb.g > ALWAN_LITERAL(0.99) &&
                       rgb.b > ALWAN_LITERAL(0.99), "Cubehelix: l=1 should be white");
        }

        /* Roundtrip for non-degenerate cases */
        if (test_inputs[i].s > ALWAN_LITERAL(0.01) &&
            test_inputs[i].l > ALWAN_LITERAL(0.01) &&
            test_inputs[i].l < ALWAN_LITERAL(0.99)) {
            alwan_cubehelix ch_out;
            alwan_rgb_to_cubehelix(&ch_out, &rgb);
            alwan_scalar h_diff = ALWAN_ABS(ch_out.h - test_inputs[i].h);
            if (h_diff > ALWAN_LITERAL(180.0)) h_diff = ALWAN_LITERAL(360.0) - h_diff;
            TEST_ASSERT(h_diff < ALWAN_LITERAL(0.1), "Cubehelix roundtrip hue mismatch");
            TEST_ASSERT(ALWAN_ABS(ch_out.s - test_inputs[i].s) < ALWAN_LITERAL(0.01),
                       "Cubehelix roundtrip saturation mismatch");
            TEST_ASSERT(ALWAN_ABS(ch_out.l - test_inputs[i].l) < ALWAN_LITERAL(0.01),
                       "Cubehelix roundtrip lightness mismatch");
        }
    }

    /* Verify luminance monotonicity: for s=0, output = l */
    {
        alwan_cubehelix gray;
        gray.h = ALWAN_LITERAL(0.0);
        gray.s = ALWAN_LITERAL(0.0);
        alwan_scalar prev_luma = ALWAN_LITERAL(-1.0);
        for (int k = 0; k <= 10; k++) {
            gray.l = (alwan_scalar)k / ALWAN_LITERAL(10.0);
            alwan_rgb rgb;
            alwan_cubehelix_to_rgb(&rgb, &gray);
            alwan_scalar luma = ALWAN_LITERAL(0.299) * rgb.r + ALWAN_LITERAL(0.587) * rgb.g
                              + ALWAN_LITERAL(0.114) * rgb.b;
            TEST_ASSERT(luma >= prev_luma - ALWAN_LITERAL(0.001),
                       "Cubehelix: luminance should increase with l for s=0");
            prev_luma = luma;
        }
    }

    TEST_PASS("Cubehelix roundtrip and properties");
}

/* ----------------------------------------------------------------
 * HSLuv Tests
 * ---------------------------------------------------------------- */

static int test_hsluv_roundtrip(void) {
    /* Test sRGB -> HSLuv -> sRGB roundtrip */
    alwan_rgb test_srgb[] = {
        {0.0, 0.0, 0.0},           /* black */
        {1.0, 1.0, 1.0},           /* white */
        {0.5, 0.5, 0.5},           /* mid gray */
        {1.0, 0.0, 0.0},           /* red */
        {0.0, 1.0, 0.0},           /* green */
        {0.0, 0.0, 1.0},           /* blue */
        {0.8, 0.2, 0.4},           /* arbitrary */
        {0.3, 0.7, 0.5},           /* arbitrary */
    };
    size_t const num_tests = sizeof(test_srgb) / sizeof(test_srgb[0]);
    alwan_scalar const tol = ALWAN_LITERAL(0.005);

    for (size_t i = 0; i < num_tests; i++) {
        alwan_hsluv hsluv;
        alwan_rgb roundtrip;
        alwan_srgb_to_hsluv(&hsluv, &test_srgb[i]);
        alwan_hsluv_to_srgb(&roundtrip, &hsluv);

        alwan_scalar diff = ALWAN_ABS(roundtrip.r - test_srgb[i].r)
                          + ALWAN_ABS(roundtrip.g - test_srgb[i].g)
                          + ALWAN_ABS(roundtrip.b - test_srgb[i].b);
        if (diff > tol) {
            printf("  HSLuv roundtrip %zu: in=[%.4f %.4f %.4f] hsluv=[%.2f %.2f %.2f] out=[%.4f %.4f %.4f]\n",
                   i, test_srgb[i].r, test_srgb[i].g, test_srgb[i].b,
                   hsluv.h, hsluv.s, hsluv.l,
                   roundtrip.r, roundtrip.g, roundtrip.b);
        }
        TEST_ASSERT(diff < tol, "HSLuv roundtrip mismatch");
    }

    /* Check ranges: H [0-360], S [0-100], L [0-100] */
    {
        alwan_rgb srgb = {0.8, 0.2, 0.4};
        alwan_hsluv hsluv;
        alwan_srgb_to_hsluv(&hsluv, &srgb);
        TEST_ASSERT(hsluv.h >= ALWAN_LITERAL(0.0) && hsluv.h <= ALWAN_LITERAL(360.0), "HSLuv H out of range");
        TEST_ASSERT(hsluv.s >= ALWAN_LITERAL(0.0) && hsluv.s <= ALWAN_LITERAL(100.1), "HSLuv S out of range");
        TEST_ASSERT(hsluv.l >= ALWAN_LITERAL(0.0) && hsluv.l <= ALWAN_LITERAL(100.1), "HSLuv L out of range");
    }

    /* Gray should have S=0 */
    {
        alwan_rgb gray = {0.5, 0.5, 0.5};
        alwan_hsluv hsluv;
        alwan_srgb_to_hsluv(&hsluv, &gray);
        TEST_ASSERT(hsluv.s < ALWAN_LITERAL(0.1), "HSLuv: gray should have S near 0");
    }

    TEST_PASS("HSLuv roundtrip and properties");
}

/* ----------------------------------------------------------------
 * HPLuv Tests
 * ---------------------------------------------------------------- */

static int test_hpluv_roundtrip(void) {
    /* HPLuv -> sRGB -> HPLuv roundtrip */
    alwan_hpluv test_hpluv[] = {
        {  0.0,   0.0,  50.0},   /* gray */
        {  0.0,  50.0,  50.0},   /* half sat red hue */
        {120.0,  50.0,  70.0},   /* greenish pastel */
        {240.0,  30.0,  30.0},   /* dark blue pastel */
    };
    size_t const num_tests = sizeof(test_hpluv) / sizeof(test_hpluv[0]);
    alwan_scalar const tol = ALWAN_LITERAL(0.5);

    for (size_t i = 0; i < num_tests; i++) {
        alwan_rgb rgb;
        alwan_hpluv roundtrip;
        alwan_hpluv_to_srgb(&rgb, &test_hpluv[i]);

        /* Output should be in [0,1] since HPLuv guarantees gamut */
        TEST_ASSERT(rgb.r >= ALWAN_LITERAL(-0.01) && rgb.r <= ALWAN_LITERAL(1.01) &&
                   rgb.g >= ALWAN_LITERAL(-0.01) && rgb.g <= ALWAN_LITERAL(1.01) &&
                   rgb.b >= ALWAN_LITERAL(-0.01) && rgb.b <= ALWAN_LITERAL(1.01),
                   "HPLuv: output should be in sRGB gamut");

        alwan_srgb_to_hpluv(&roundtrip, &rgb);

        alwan_scalar diff_l = ALWAN_ABS(roundtrip.l - test_hpluv[i].l);
        TEST_ASSERT(diff_l < tol, "HPLuv roundtrip L mismatch");

        if (test_hpluv[i].s > ALWAN_LITERAL(1.0)) {
            alwan_scalar diff_s = ALWAN_ABS(roundtrip.s - test_hpluv[i].s);
            TEST_ASSERT(diff_s < tol, "HPLuv roundtrip S mismatch");
        }
    }

    TEST_PASS("HPLuv roundtrip and gamut safety");
}

/* ----------------------------------------------------------------
 * Okhsl Tests
 * ---------------------------------------------------------------- */

static int test_okhsl_roundtrip(void) {
    /* Test sRGB -> Okhsl -> sRGB roundtrip */
    alwan_rgb test_srgb[] = {
        {0.0, 0.0, 0.0},           /* black */
        {1.0, 1.0, 1.0},           /* white */
        {0.5, 0.5, 0.5},           /* mid gray */
        {1.0, 0.0, 0.0},           /* red */
        {0.0, 1.0, 0.0},           /* green */
        {0.0, 0.0, 1.0},           /* blue */
        {0.8, 0.2, 0.4},           /* arbitrary */
        {0.1, 0.3, 0.6},           /* arbitrary */
    };
    size_t const num_tests = sizeof(test_srgb) / sizeof(test_srgb[0]);
    alwan_scalar const tol = ALWAN_LITERAL(0.01);

    for (size_t i = 0; i < num_tests; i++) {
        alwan_okhsl okhsl;
        alwan_rgb roundtrip;
        alwan_srgb_to_okhsl(&okhsl, &test_srgb[i]);
        alwan_okhsl_to_srgb(&roundtrip, &okhsl);

        alwan_scalar diff = ALWAN_ABS(roundtrip.r - test_srgb[i].r)
                          + ALWAN_ABS(roundtrip.g - test_srgb[i].g)
                          + ALWAN_ABS(roundtrip.b - test_srgb[i].b);
        if (diff > tol) {
            printf("  Okhsl roundtrip %zu: in=[%.4f %.4f %.4f] okhsl=[%.4f %.4f %.4f] out=[%.4f %.4f %.4f]\n",
                   i, test_srgb[i].r, test_srgb[i].g, test_srgb[i].b,
                   okhsl.h, okhsl.s, okhsl.l,
                   roundtrip.r, roundtrip.g, roundtrip.b);
        }
        TEST_ASSERT(diff < tol, "Okhsl roundtrip mismatch");
    }

    /* Check ranges: all in [0,1] */
    {
        alwan_rgb srgb = {0.8, 0.2, 0.4};
        alwan_okhsl okhsl;
        alwan_srgb_to_okhsl(&okhsl, &srgb);
        TEST_ASSERT(okhsl.h >= ALWAN_LITERAL(-0.01) && okhsl.h <= ALWAN_LITERAL(1.01), "Okhsl h out of range");
        TEST_ASSERT(okhsl.s >= ALWAN_LITERAL(-0.01) && okhsl.s <= ALWAN_LITERAL(1.01), "Okhsl s out of range");
        TEST_ASSERT(okhsl.l >= ALWAN_LITERAL(-0.01) && okhsl.l <= ALWAN_LITERAL(1.01), "Okhsl l out of range");
    }

    /* Gray should have s near 0 */
    {
        alwan_rgb gray = {0.5, 0.5, 0.5};
        alwan_okhsl okhsl;
        alwan_srgb_to_okhsl(&okhsl, &gray);
        TEST_ASSERT(okhsl.s < ALWAN_LITERAL(0.01), "Okhsl: gray should have s near 0");
    }

    TEST_PASS("Okhsl roundtrip and properties");
}

/* ----------------------------------------------------------------
 * Okhsv Tests
 * ---------------------------------------------------------------- */

static int test_okhsv_roundtrip(void) {
    /* Test sRGB -> Okhsv -> sRGB roundtrip */
    alwan_rgb test_srgb[] = {
        {0.0, 0.0, 0.0},           /* black */
        {1.0, 1.0, 1.0},           /* white */
        {0.5, 0.5, 0.5},           /* mid gray */
        {1.0, 0.0, 0.0},           /* red */
        {0.0, 1.0, 0.0},           /* green */
        {0.0, 0.0, 1.0},           /* blue */
        {0.8, 0.2, 0.4},           /* arbitrary */
        {0.2, 0.6, 0.9},           /* arbitrary */
    };
    size_t const num_tests = sizeof(test_srgb) / sizeof(test_srgb[0]);
    alwan_scalar const tol = ALWAN_LITERAL(0.01);

    for (size_t i = 0; i < num_tests; i++) {
        alwan_okhsv okhsv;
        alwan_rgb roundtrip;
        alwan_srgb_to_okhsv(&okhsv, &test_srgb[i]);
        alwan_okhsv_to_srgb(&roundtrip, &okhsv);

        alwan_scalar diff = ALWAN_ABS(roundtrip.r - test_srgb[i].r)
                          + ALWAN_ABS(roundtrip.g - test_srgb[i].g)
                          + ALWAN_ABS(roundtrip.b - test_srgb[i].b);
        if (diff > tol) {
            printf("  Okhsv roundtrip %zu: in=[%.4f %.4f %.4f] okhsv=[%.4f %.4f %.4f] out=[%.4f %.4f %.4f]\n",
                   i, test_srgb[i].r, test_srgb[i].g, test_srgb[i].b,
                   okhsv.h, okhsv.s, okhsv.v,
                   roundtrip.r, roundtrip.g, roundtrip.b);
        }
        TEST_ASSERT(diff < tol, "Okhsv roundtrip mismatch");
    }

    /* Check ranges */
    {
        alwan_rgb srgb = {0.8, 0.2, 0.4};
        alwan_okhsv okhsv;
        alwan_srgb_to_okhsv(&okhsv, &srgb);
        TEST_ASSERT(okhsv.h >= ALWAN_LITERAL(-0.01) && okhsv.h <= ALWAN_LITERAL(1.01), "Okhsv h out of range");
        TEST_ASSERT(okhsv.s >= ALWAN_LITERAL(-0.01) && okhsv.s <= ALWAN_LITERAL(1.01), "Okhsv s out of range");
        TEST_ASSERT(okhsv.v >= ALWAN_LITERAL(-0.01) && okhsv.v <= ALWAN_LITERAL(1.01), "Okhsv v out of range");
    }

    /* Gray should have s near 0 */
    {
        alwan_rgb gray = {0.5, 0.5, 0.5};
        alwan_okhsv okhsv;
        alwan_srgb_to_okhsv(&okhsv, &gray);
        TEST_ASSERT(okhsv.s < ALWAN_LITERAL(0.01), "Okhsv: gray should have s near 0");
    }

    /* Black should have v near 0 */
    {
        alwan_rgb black = {0.0, 0.0, 0.0};
        alwan_okhsv okhsv;
        alwan_srgb_to_okhsv(&okhsv, &black);
        TEST_ASSERT(okhsv.v < ALWAN_LITERAL(0.01), "Okhsv: black should have v near 0");
    }

    TEST_PASS("Okhsv roundtrip and properties");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_76_convenience_models_main(void) {
    int failures = 0;

    failures += test_hlc_roundtrip();
    failures += test_cubehelix_roundtrip();
    failures += test_hsluv_roundtrip();
    failures += test_hpluv_roundtrip();
    failures += test_okhsl_roundtrip();
    failures += test_okhsv_roundtrip();

    if (failures == 0) {
        printf("\n=== All convenience model tests passed ===\n");
        return 0;
    } else {
        printf("\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
