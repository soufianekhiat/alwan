/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 15: Convenience color models (HSV, HSL, CMY, CMYK, YCbCr)
 */

#include "test_common.h"
#include <stdlib.h>

/* Test fixtures: RGB input and expected values */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const test_rgb[] = {
#include "reference_values/test_rgb_colors.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const expected_hsv[] = {
#include "reference_values/rgb_to_hsv.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const expected_hsl[] = {
#include "reference_values/rgb_to_hsl.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const expected_cmy[] = {
#include "reference_values/rgb_to_cmy.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const expected_cmyk[] = {
#include "reference_values/rgb_to_cmyk.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const expected_ycbcr_bt601[] = {
#include "reference_values/rgb_to_ycbcr_bt601.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const expected_ycbcr_bt709[] = {
#include "reference_values/rgb_to_ycbcr_bt709.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const expected_ycbcr_bt2020[] = {
#include "reference_values/rgb_to_ycbcr_bt2020.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const expected_yccbccrc[] = {
#include "reference_values/rgb_to_yccbccrc.csv"
};
ALWAN_DIAG_POP

#define NUM_TEST_COLORS 11

/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */

static int test_hsv_forward(void) {
    /* Test RGB -> HSV conversion */
    for (size_t i = 0; i < NUM_TEST_COLORS; i++) {
        alwan_vec3_f64 rgb;
        rgb.v[0] = test_rgb[i * 3 + 0];
        rgb.v[1] = test_rgb[i * 3 + 1];
        rgb.v[2] = test_rgb[i * 3 + 2];

        alwan_vec3_f64 hsv;
        alwan_rgb_f64 rgb_typed;
        alwan_hsv_f64 hsv_typed;
        ALWAN_MEMCPY(&rgb_typed, &rgb, sizeof(alwan_vec3_f64));
        int status = alwan_rgb_to_hsv_f64(&hsv_typed, &rgb_typed);
        ALWAN_MEMCPY(&hsv, &hsv_typed, sizeof(alwan_vec3_f64));
        TEST_ASSERT(status == ALWAN_OK, "RGB to HSV conversion failed");

        /* Compare with expected values */
        alwan_f64 exp_h = expected_hsv[i * 3 + 0];
        alwan_f64 exp_s = expected_hsv[i * 3 + 1];
        alwan_f64 exp_v = expected_hsv[i * 3 + 2];

        alwan_f64 tol = ALWAN_TEST_TOLERANCE;
        alwan_f64 diff_h = ALWAN_ABS(hsv.v[0] - exp_h);
        alwan_f64 diff_s = ALWAN_ABS(hsv.v[1] - exp_s);
        alwan_f64 diff_v = ALWAN_ABS(hsv.v[2] - exp_v);

        if (diff_h > tol || diff_s > tol || diff_v > tol) {
            printf("HSV forward test failed for color %zu:\n", i);
            printf("  RGB: [%.6f, %.6f, %.6f]\n", rgb.v[0], rgb.v[1], rgb.v[2]);
            printf("  Expected HSV: [%.6f, %.6f, %.6f]\n", exp_h, exp_s, exp_v);
            printf("  Got HSV:      [%.6f, %.6f, %.6f]\n", hsv.v[0], hsv.v[1], hsv.v[2]);
            printf("  Diff:         [%e, %e, %e]\n", diff_h, diff_s, diff_v);
            TEST_ASSERT(0, "HSV values don't match expected");
        }
    }

    TEST_PASS("test_hsv_forward");
}

static int test_hsv_round_trip(void) {
    /* Test RGB -> HSV -> RGB round-trip */
    for (size_t i = 0; i < NUM_TEST_COLORS; i++) {
        alwan_rgb_f64 rgb_orig;
        rgb_orig.r = test_rgb[i * 3 + 0];
        rgb_orig.g = test_rgb[i * 3 + 1];
        rgb_orig.b = test_rgb[i * 3 + 2];

        alwan_hsv_f64 hsv;
        alwan_rgb_f64 rgb_recon;
        int status;

        status = alwan_rgb_to_hsv_f64(&hsv, &rgb_orig);
        TEST_ASSERT(status == ALWAN_OK, "RGB to HSV failed");

        status = alwan_hsv_to_rgb_f64(&rgb_recon, &hsv);
        TEST_ASSERT(status == ALWAN_OK, "HSV to RGB failed");

        /* Check round-trip accuracy */
        alwan_f64 tol = ALWAN_TEST_TOLERANCE;
        alwan_f64 diff_r = ALWAN_ABS(rgb_recon.r - rgb_orig.r );
        alwan_f64 diff_g = ALWAN_ABS( rgb_recon.g - rgb_orig.g );
        alwan_f64 diff_b = ALWAN_ABS( rgb_recon.b - rgb_orig.b );

        if (diff_r > tol || diff_g > tol || diff_b > tol) {
            printf("HSV round-trip failed for color %zu:\n", i);
            printf("  Original RGB: [%.6f, %.6f, %.6f]\n",
                   rgb_orig.r, rgb_orig.g, rgb_orig.b);
            printf("  HSV:          [%.6f, %.6f, %.6f]\n",
                   hsv.h, hsv.s, hsv.v);
            printf("  Recon RGB:    [%.6f, %.6f, %.6f]\n",
                   rgb_recon.r, rgb_recon.g, rgb_recon.b);
            printf("  Diff:         [%e, %e, %e]\n", diff_r, diff_g, diff_b);
            TEST_ASSERT(0, "HSV round-trip tolerance exceeded");
        }
    }

    TEST_PASS("test_hsv_round_trip");
}

static int test_hsl_forward(void) {
    /* Test RGB -> HSL conversion */
    for (size_t i = 0; i < NUM_TEST_COLORS; i++) {
        alwan_vec3_f64 rgb;
        rgb.v[0] = test_rgb[i * 3 + 0];
        rgb.v[1] = test_rgb[i * 3 + 1];
        rgb.v[2] = test_rgb[i * 3 + 2];

        alwan_vec3_f64 hsl;
        alwan_rgb_f64 rgb_typed;
        alwan_hsl_f64 hsl_typed;
        ALWAN_MEMCPY(&rgb_typed, &rgb, sizeof(alwan_vec3_f64));
        int status = alwan_rgb_to_hsl_f64(&hsl_typed, &rgb_typed);
        ALWAN_MEMCPY(&hsl, &hsl_typed, sizeof(alwan_vec3_f64));
        TEST_ASSERT(status == ALWAN_OK, "RGB to HSL conversion failed");

        /* Compare with expected values */
        alwan_f64 exp_h = expected_hsl[i * 3 + 0];
        alwan_f64 exp_s = expected_hsl[i * 3 + 1];
        alwan_f64 exp_l = expected_hsl[i * 3 + 2];

        alwan_f64 tol = ALWAN_TEST_TOLERANCE;
        alwan_f64 diff_h = ALWAN_ABS(hsl.v[0] - exp_h);
        alwan_f64 diff_s = ALWAN_ABS(hsl.v[1] - exp_s);
        alwan_f64 diff_l = ALWAN_ABS(hsl.v[2] - exp_l);

        if (diff_h > tol || diff_s > tol || diff_l > tol) {
            printf("HSL forward test failed for color %zu:\n", i);
            printf("  RGB: [%.6f, %.6f, %.6f]\n", rgb.v[0], rgb.v[1], rgb.v[2]);
            printf("  Expected HSL: [%.6f, %.6f, %.6f]\n", exp_h, exp_s, exp_l);
            printf("  Got HSL:      [%.6f, %.6f, %.6f]\n", hsl.v[0], hsl.v[1], hsl.v[2]);
            printf("  Diff:         [%e, %e, %e]\n", diff_h, diff_s, diff_l);
            TEST_ASSERT(0, "HSL values don't match expected");
        }
    }

    TEST_PASS("test_hsl_forward");
}

static int test_hsl_round_trip(void) {
    /* Test RGB -> HSL -> RGB round-trip */
    for (size_t i = 0; i < NUM_TEST_COLORS; i++) {
        alwan_rgb_f64 rgb_orig;
        rgb_orig.r = test_rgb[i * 3 + 0];
        rgb_orig.g = test_rgb[i * 3 + 1];
        rgb_orig.b = test_rgb[i * 3 + 2];

        alwan_hsl_f64 hsl;
        alwan_rgb_f64 rgb_recon;
        int status;

        status = alwan_rgb_to_hsl_f64(&hsl, &rgb_orig);
        TEST_ASSERT(status == ALWAN_OK, "RGB to HSL failed");

        status = alwan_hsl_to_rgb_f64(&rgb_recon, &hsl);
        TEST_ASSERT(status == ALWAN_OK, "HSL to RGB failed");

        /* Check round-trip accuracy */
        alwan_f64 tol = ALWAN_TEST_TOLERANCE;
        alwan_f64 diff_r = ALWAN_ABS(rgb_recon.r - rgb_orig.r);
        alwan_f64 diff_g = ALWAN_ABS(rgb_recon.g - rgb_orig.g);
        alwan_f64 diff_b = ALWAN_ABS(rgb_recon.b - rgb_orig.b);

        if (diff_r > tol || diff_g > tol || diff_b > tol) {
            printf("HSL round-trip failed for color %zu:\n", i);
            printf("  Original RGB: [%.6f, %.6f, %.6f]\n",
                   rgb_orig.r, rgb_orig.g, rgb_orig.b);
            printf("  HSL:          [%.6f, %.6f, %.6f]\n",
                   hsl.h, hsl.s, hsl.l);
            printf("  Recon RGB:    [%.6f, %.6f, %.6f]\n",
                   rgb_recon.r, rgb_recon.g, rgb_recon.b);
            printf("  Diff:         [%e, %e, %e]\n", diff_r, diff_g, diff_b);
            TEST_ASSERT(0, "HSL round-trip tolerance exceeded");
        }
    }

    TEST_PASS("test_hsl_round_trip");
}

static int test_cmy_conversions(void) {
    /* Test RGB <-> CMY conversions */
    for (size_t i = 0; i < NUM_TEST_COLORS; i++) {
        alwan_rgb_f64 rgb;
        rgb.r = test_rgb[i * 3 + 0];
        rgb.g = test_rgb[i * 3 + 1];
        rgb.b = test_rgb[i * 3 + 2];

        alwan_cmy_f64 cmy;
        int status = alwan_rgb_to_cmy_f64(&cmy, &rgb);
        TEST_ASSERT(status == ALWAN_OK, "RGB to CMY failed");

        /* Compare with expected */
        alwan_f64 exp_c = expected_cmy[i * 3 + 0];
        alwan_f64 exp_m = expected_cmy[i * 3 + 1];
        alwan_f64 exp_y = expected_cmy[i * 3 + 2];

        alwan_f64 tol = ALWAN_TEST_TOLERANCE;
        alwan_f64 diff_c = ALWAN_ABS(cmy.c - exp_c);
        alwan_f64 diff_m = ALWAN_ABS(cmy.m - exp_m);
        alwan_f64 diff_y = ALWAN_ABS(cmy.y - exp_y);

        if (diff_c > tol || diff_m > tol || diff_y > tol) {
            printf("CMY forward test failed for color %zu:\n", i);
            printf("  RGB: [%.6f, %.6f, %.6f]\n", rgb.r, rgb.g, rgb.b);
            printf("  Expected CMY: [%.6f, %.6f, %.6f]\n", exp_c, exp_m, exp_y);
            printf("  Got CMY:      [%.6f, %.6f, %.6f]\n", cmy.c, cmy.m, cmy.y);
            TEST_ASSERT(0, "CMY values don't match expected");
        }

        /* Test round-trip */
        alwan_rgb_f64 rgb_recon;
        status = alwan_cmy_to_rgb_f64(&rgb_recon, &cmy);
        TEST_ASSERT(status == ALWAN_OK, "CMY to RGB failed");

        alwan_f64 diff_r = ALWAN_ABS(rgb_recon.r - rgb.r);
        alwan_f64 diff_g = ALWAN_ABS(rgb_recon.g - rgb.g);
        alwan_f64 diff_b = ALWAN_ABS(rgb_recon.b - rgb.b);

        if (diff_r > tol || diff_g > tol || diff_b > tol) {
            printf("CMY round-trip failed for color %zu\n", i);
            TEST_ASSERT(0, "CMY round-trip failed");
        }
    }

    TEST_PASS("test_cmy_conversions");
}

static int test_cmyk_conversions(void) {
    /* Test CMY <-> CMYK conversions */
    for (size_t i = 0; i < NUM_TEST_COLORS; i++) {
        alwan_cmy_f64 cmy;
        cmy.c = expected_cmy[i * 3 + 0];
        cmy.m = expected_cmy[i * 3 + 1];
        cmy.y = expected_cmy[i * 3 + 2];

        alwan_cmyk_f64 cmyk_result;
        int status = alwan_cmy_to_cmyk_f64(&cmyk_result, &cmy);
        TEST_ASSERT(status == ALWAN_OK, "CMY to CMYK failed");

        /* Compare with expected */
        alwan_f64 exp_c = expected_cmyk[i * 4 + 0];
        alwan_f64 exp_m = expected_cmyk[i * 4 + 1];
        alwan_f64 exp_y = expected_cmyk[i * 4 + 2];
        alwan_f64 exp_k = expected_cmyk[i * 4 + 3];

        alwan_f64 tol = ALWAN_TEST_TOLERANCE;
        alwan_f64 diff_c = ALWAN_ABS(cmyk_result.c - exp_c);
        alwan_f64 diff_m = ALWAN_ABS(cmyk_result.m - exp_m);
        alwan_f64 diff_y = ALWAN_ABS(cmyk_result.y - exp_y);
        alwan_f64 diff_k = ALWAN_ABS(cmyk_result.k - exp_k);

        if (diff_c > tol || diff_m > tol || diff_y > tol || diff_k > tol) {
            printf("CMYK forward test failed for color %zu:\n", i);
            printf("  CMY: [%.6f, %.6f, %.6f]\n", cmy.c, cmy.m, cmy.y);
            printf("  Expected CMYK: [%.6f, %.6f, %.6f, %.6f]\n", exp_c, exp_m, exp_y, exp_k);
            printf("  Got CMYK:      [%.6f, %.6f, %.6f, %.6f]\n", cmyk_result.c, cmyk_result.m, cmyk_result.y, cmyk_result.k);
            TEST_ASSERT(0, "CMYK values don't match expected");
        }

        /* Test round-trip */
        alwan_cmy_f64 cmy_recon;
        status = alwan_cmyk_to_cmy_f64(&cmy_recon, &cmyk_result);
        TEST_ASSERT(status == ALWAN_OK, "CMYK to CMY failed");

        alwan_f64 diff_c2 = ALWAN_ABS(cmy_recon.c - cmy.c);
        alwan_f64 diff_m2 = ALWAN_ABS(cmy_recon.m - cmy.m);
        alwan_f64 diff_y2 = ALWAN_ABS(cmy_recon.y - cmy.y);

        if (diff_c2 > tol || diff_m2 > tol || diff_y2 > tol) {
            printf("CMYK round-trip failed for color %zu\n", i);
            TEST_ASSERT(0, "CMYK round-trip failed");
        }
    }

    TEST_PASS("test_cmyk_conversions");
}

static int test_ycbcr_bt601(void) {
    /* Test RGB <-> YCbCr (BT.601) */
    for (size_t i = 0; i < NUM_TEST_COLORS; i++) {
        alwan_rgb_f64 rgb;
        rgb.r = test_rgb[i * 3 + 0];
        rgb.g = test_rgb[i * 3 + 1];
        rgb.b = test_rgb[i * 3 + 2];

        alwan_ycbcr_f64 ycbcr;
        int status = alwan_rgb_to_ycbcr(&ycbcr, &rgb, ALWAN_YCBCR_BT601);
        TEST_ASSERT(status == ALWAN_OK, "RGB to YCbCr BT.601 failed");

        /* Compare with expected */
        alwan_f64 exp_y = expected_ycbcr_bt601[i * 3 + 0];
        alwan_f64 exp_cb = expected_ycbcr_bt601[i * 3 + 1];
        alwan_f64 exp_cr = expected_ycbcr_bt601[i * 3 + 2];

        alwan_f64 tol = ALWAN_TEST_TOLERANCE;
        alwan_f64 diff_y = ALWAN_ABS(ycbcr.Y - exp_y);
        alwan_f64 diff_cb = ALWAN_ABS(ycbcr.Cb - exp_cb);
        alwan_f64 diff_cr = ALWAN_ABS(ycbcr.Cr - exp_cr);

        if (diff_y > tol || diff_cb > tol || diff_cr > tol) {
            printf("YCbCr BT.601 forward test failed for color %zu:\n", i);
            printf("  RGB: [%.6f, %.6f, %.6f]\n", rgb.r, rgb.g, rgb.b);
            printf("  Expected YCbCr: [%.6f, %.6f, %.6f]\n", exp_y, exp_cb, exp_cr);
            printf("  Got YCbCr:      [%.6f, %.6f, %.6f]\n",
                   ycbcr.Y, ycbcr.Cb, ycbcr.Cr);
            TEST_ASSERT(0, "YCbCr BT.601 values don't match");
        }

        /* Test round-trip */
        alwan_rgb_f64 rgb_recon;
        status = alwan_ycbcr_to_rgb(&rgb_recon, &ycbcr, ALWAN_YCBCR_BT601);
        TEST_ASSERT(status == ALWAN_OK, "YCbCr to RGB BT.601 failed");

        alwan_f64 diff_r = ALWAN_ABS(rgb_recon.r - rgb.r);
        alwan_f64 diff_g = ALWAN_ABS(rgb_recon.g - rgb.g);
        alwan_f64 diff_b = ALWAN_ABS(rgb_recon.b - rgb.b);

        if (diff_r > tol || diff_g > tol || diff_b > tol) {
            printf("YCbCr BT.601 round-trip failed for color %zu\n", i);
            TEST_ASSERT(0, "YCbCr BT.601 round-trip failed");
        }
    }

    TEST_PASS("test_ycbcr_bt601");
}

static int test_ycbcr_bt709(void) {
    /* Test RGB <-> YCbCr (BT.709) */
    for (size_t i = 0; i < NUM_TEST_COLORS; i++) {
        alwan_rgb_f64 rgb;
        rgb.r = test_rgb[i * 3 + 0];
        rgb.g = test_rgb[i * 3 + 1];
        rgb.b = test_rgb[i * 3 + 2];

        alwan_ycbcr_f64 ycbcr;
        int status = alwan_rgb_to_ycbcr(&ycbcr, &rgb, ALWAN_YCBCR_BT709);
        TEST_ASSERT(status == ALWAN_OK, "RGB to YCbCr BT.709 failed");

        /* Compare with expected */
        alwan_f64 exp_y = expected_ycbcr_bt709[i * 3 + 0];
        alwan_f64 exp_cb = expected_ycbcr_bt709[i * 3 + 1];
        alwan_f64 exp_cr = expected_ycbcr_bt709[i * 3 + 2];

        alwan_f64 tol = ALWAN_TEST_TOLERANCE;
        alwan_f64 diff_y = ALWAN_ABS(ycbcr.Y - exp_y);
        alwan_f64 diff_cb = ALWAN_ABS(ycbcr.Cb - exp_cb);
        alwan_f64 diff_cr = ALWAN_ABS(ycbcr.Cr - exp_cr);

        if (diff_y > tol || diff_cb > tol || diff_cr > tol) {
            printf("YCbCr BT.709 forward test failed for color %zu:\n", i);
            printf("  RGB: [%.6f, %.6f, %.6f]\n", rgb.r, rgb.g, rgb.b);
            printf("  Expected YCbCr: [%.6f, %.6f, %.6f]\n", exp_y, exp_cb, exp_cr);
            printf("  Got YCbCr:      [%.6f, %.6f, %.6f]\n",
                   ycbcr.Y, ycbcr.Cb, ycbcr.Cr);
            TEST_ASSERT(0, "YCbCr BT.709 values don't match");
        }

        /* Test round-trip */
        alwan_rgb_f64 rgb_recon;
        status = alwan_ycbcr_to_rgb(&rgb_recon, &ycbcr, ALWAN_YCBCR_BT709);
        TEST_ASSERT(status == ALWAN_OK, "YCbCr to RGB BT.709 failed");

        alwan_f64 diff_r = ALWAN_ABS(rgb_recon.r - rgb.r);
        alwan_f64 diff_g = ALWAN_ABS(rgb_recon.g - rgb.g);
        alwan_f64 diff_b = ALWAN_ABS(rgb_recon.b - rgb.b);

        if (diff_r > tol || diff_g > tol || diff_b > tol) {
            printf("YCbCr BT.709 round-trip failed for color %zu\n", i);
            TEST_ASSERT(0, "YCbCr BT.709 round-trip failed");
        }
    }

    TEST_PASS("test_ycbcr_bt709");
}

static int test_ycbcr_bt2020(void) {
    /* Test RGB <-> YCbCr (BT.2020) */
    for (size_t i = 0; i < NUM_TEST_COLORS; i++) {
        alwan_rgb_f64 rgb;
        rgb.r = test_rgb[i * 3 + 0];
        rgb.g = test_rgb[i * 3 + 1];
        rgb.b = test_rgb[i * 3 + 2];

        alwan_ycbcr_f64 ycbcr;
        int status = alwan_rgb_to_ycbcr(&ycbcr, &rgb, ALWAN_YCBCR_BT2020);
        TEST_ASSERT(status == ALWAN_OK, "RGB to YCbCr BT.2020 failed");

        /* Compare with expected */
        alwan_f64 exp_y = expected_ycbcr_bt2020[i * 3 + 0];
        alwan_f64 exp_cb = expected_ycbcr_bt2020[i * 3 + 1];
        alwan_f64 exp_cr = expected_ycbcr_bt2020[i * 3 + 2];

        alwan_f64 tol = ALWAN_TEST_TOLERANCE;
        alwan_f64 diff_y = ALWAN_ABS(ycbcr.Y - exp_y);
        alwan_f64 diff_cb = ALWAN_ABS(ycbcr.Cb - exp_cb);
        alwan_f64 diff_cr = ALWAN_ABS(ycbcr.Cr - exp_cr);

        if (diff_y > tol || diff_cb > tol || diff_cr > tol) {
            printf("YCbCr BT.2020 forward test failed for color %zu:\n", i);
            printf("  RGB: [%.6f, %.6f, %.6f]\n", rgb.r, rgb.g, rgb.b);
            printf("  Expected YCbCr: [%.6f, %.6f, %.6f]\n", exp_y, exp_cb, exp_cr);
            printf("  Got YCbCr:      [%.6f, %.6f, %.6f]\n",
                   ycbcr.Y, ycbcr.Cb, ycbcr.Cr);
            TEST_ASSERT(0, "YCbCr BT.2020 values don't match");
        }

        /* Test round-trip */
        alwan_rgb_f64 rgb_recon;
        status = alwan_ycbcr_to_rgb(&rgb_recon, &ycbcr, ALWAN_YCBCR_BT2020);
        TEST_ASSERT(status == ALWAN_OK, "YCbCr to RGB BT.2020 failed");

        alwan_f64 diff_r = ALWAN_ABS(rgb_recon.r - rgb.r);
        alwan_f64 diff_g = ALWAN_ABS(rgb_recon.g - rgb.g);
        alwan_f64 diff_b = ALWAN_ABS(rgb_recon.b - rgb.b);

        if (diff_r > tol || diff_g > tol || diff_b > tol) {
            printf("YCbCr BT.2020 round-trip failed for color %zu\n", i);
            TEST_ASSERT(0, "YCbCr BT.2020 round-trip failed");
        }
    }

    TEST_PASS("test_ycbcr_bt2020");
}

static int test_yccbccrc(void) {
    /* Test RGB <-> YcCbcCrc (constant luminance BT.2020) */
    /* Note: YcCbcCrc cannot perfectly round-trip pure colors at RGB boundaries */
    for (size_t i = 0; i < NUM_TEST_COLORS; i++) {
        alwan_rgb_f64 rgb;
        rgb.r = test_rgb[i * 3 + 0];
        rgb.g = test_rgb[i * 3 + 1];
        rgb.b = test_rgb[i * 3 + 2];

        alwan_yccbccrc_f64 ycc;
        int status = alwan_rgb_to_yccbccrc_f64(&ycc, &rgb, 10);
        TEST_ASSERT(status == ALWAN_OK, "RGB to YcCbcCrc failed");

        /* Compare with expected */
        alwan_f64 exp_yc = expected_yccbccrc[i * 3 + 0];
        alwan_f64 exp_cbc = expected_yccbccrc[i * 3 + 1];
        alwan_f64 exp_crc = expected_yccbccrc[i * 3 + 2];

        alwan_f64 tol = ALWAN_TEST_TOLERANCE;
        alwan_f64 diff_yc = ALWAN_ABS(ycc.Yc - exp_yc);
        alwan_f64 diff_cbc = ALWAN_ABS(ycc.Cbc - exp_cbc);
        alwan_f64 diff_crc = ALWAN_ABS(ycc.Crc - exp_crc);

        if (diff_yc > tol || diff_cbc > tol || diff_crc > tol) {
            printf("YcCbcCrc forward test failed for color %zu:\n", i);
            printf("  RGB: [%.6f, %.6f, %.6f]\n", rgb.r, rgb.g, rgb.b);
            printf("  Expected YcCbcCrc: [%.6f, %.6f, %.6f]\n", exp_yc, exp_cbc, exp_crc);
            printf("  Got YcCbcCrc:      [%.6f, %.6f, %.6f]\n",
                   ycc.Yc, ycc.Cbc, ycc.Crc);
            TEST_ASSERT(0, "YcCbcCrc values don't match");
        }

        /* Test round-trip for non-boundary colors only */
        /* YcCbcCrc is inherently lossy for pure RGB primaries and secondaries */
        int is_boundary_color = (rgb.r == 1.0 || rgb.g == 1.0 || rgb.b == 1.0) &&
                               ((rgb.r == 0.0 ? 1 : 0) + (rgb.g == 0.0 ? 1 : 0) + (rgb.b == 0.0 ? 1 : 0)) >= 1;

        if (!is_boundary_color) {
            alwan_rgb_f64 rgb_recon;
            status = alwan_yccbccrc_to_rgb_f64(&rgb_recon, &ycc, 10);
            TEST_ASSERT(status == ALWAN_OK, "YcCbcCrc to RGB failed");

            alwan_f64 diff_r = ALWAN_ABS(rgb_recon.r - rgb.r);
            alwan_f64 diff_g = ALWAN_ABS(rgb_recon.g - rgb.g);
            alwan_f64 diff_b = ALWAN_ABS(rgb_recon.b - rgb.b);

            if (diff_r > tol || diff_g > tol || diff_b > tol) {
                printf("YcCbcCrc round-trip failed for color %zu\n", i);
                printf("  Original:    [%.6f, %.6f, %.6f]\n",
                       rgb.r, rgb.g, rgb.b);
                printf("  Reconstructed: [%.6f, %.6f, %.6f]\n",
                       rgb_recon.r, rgb_recon.g, rgb_recon.b);
                printf("  Diff:        [%e, %e, %e]\n", diff_r, diff_g, diff_b);
                TEST_ASSERT(0, "YcCbcCrc round-trip failed");
            }
        }
    }

    TEST_PASS("test_yccbccrc");
}

/* ----------------------------------------------------------------
 * Test runner
 * ---------------------------------------------------------------- */

int test_15_conv_convenience_main(void) {
    printf("Running M9 Convenience Color Models tests...\n");

    if (test_hsv_forward() != 0) return 1;
    if (test_hsv_round_trip() != 0) return 1;
    if (test_hsl_forward() != 0) return 1;
    if (test_hsl_round_trip() != 0) return 1;
    if (test_cmy_conversions() != 0) return 1;
    if (test_cmyk_conversions() != 0) return 1;
    if (test_ycbcr_bt601() != 0) return 1;
    if (test_ycbcr_bt709() != 0) return 1;
    if (test_ycbcr_bt2020() != 0) return 1;
    if (test_yccbccrc() != 0) return 1;

    printf("All M9 tests passed!\n");
    return 0;
}
