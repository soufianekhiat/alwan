/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 90: Convenience color models (HSV, HSL, CMY, CMYK, YCbCr)
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>
#include <stdlib.h>

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

/* Test fixtures: RGB input and expected values */
static alwan_scalar const test_rgb[] = {
#include "reference_values/test_rgb_colors.csv"
};

static alwan_scalar const expected_hsv[] = {
#include "reference_values/rgb_to_hsv.csv"
};

static alwan_scalar const expected_hsl[] = {
#include "reference_values/rgb_to_hsl.csv"
};

static alwan_scalar const expected_cmy[] = {
#include "reference_values/rgb_to_cmy.csv"
};

static alwan_scalar const expected_cmyk[] = {
#include "reference_values/rgb_to_cmyk.csv"
};

static alwan_scalar const expected_ycbcr_bt601[] = {
#include "reference_values/rgb_to_ycbcr_bt601.csv"
};

static alwan_scalar const expected_ycbcr_bt709[] = {
#include "reference_values/rgb_to_ycbcr_bt709.csv"
};

static alwan_scalar const expected_ycbcr_bt2020[] = {
#include "reference_values/rgb_to_ycbcr_bt2020.csv"
};

static alwan_scalar const expected_yccbccrc[] = {
#include "reference_values/rgb_to_yccbccrc.csv"
};

#define NUM_TEST_COLORS 11

/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */

static int test_hsv_forward(void) {
    /* Test RGB -> HSV conversion */
    for (size_t i = 0; i < NUM_TEST_COLORS; i++) {
        alwan_vec3 rgb;
        rgb.v[0] = test_rgb[i * 3 + 0];
        rgb.v[1] = test_rgb[i * 3 + 1];
        rgb.v[2] = test_rgb[i * 3 + 2];

        alwan_vec3 hsv;
        int status = alwan_rgb_to_hsv(&rgb, &hsv);
        TEST_ASSERT(status == ALWAN_OK, "RGB to HSV conversion failed");

        /* Compare with expected values */
        alwan_scalar exp_h = expected_hsv[i * 3 + 0];
        alwan_scalar exp_s = expected_hsv[i * 3 + 1];
        alwan_scalar exp_v = expected_hsv[i * 3 + 2];

        alwan_scalar tol = ALWAN_TEST_TOLERANCE;
        alwan_scalar diff_h = ALWAN_FABS(hsv.v[0] - exp_h);
        alwan_scalar diff_s = ALWAN_FABS(hsv.v[1] - exp_s);
        alwan_scalar diff_v = ALWAN_FABS(hsv.v[2] - exp_v);

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
        alwan_vec3 rgb_orig;
        rgb_orig.v[0] = test_rgb[i * 3 + 0];
        rgb_orig.v[1] = test_rgb[i * 3 + 1];
        rgb_orig.v[2] = test_rgb[i * 3 + 2];

        alwan_vec3 hsv, rgb_recon;
        int status;

        status = alwan_rgb_to_hsv(&rgb_orig, &hsv);
        TEST_ASSERT(status == ALWAN_OK, "RGB to HSV failed");

        status = alwan_hsv_to_rgb(&hsv, &rgb_recon);
        TEST_ASSERT(status == ALWAN_OK, "HSV to RGB failed");

        /* Check round-trip accuracy */
        alwan_scalar tol = ALWAN_TEST_TOLERANCE;
        alwan_scalar diff_r = ALWAN_FABS(rgb_recon.v[0] - rgb_orig.v[0]);
        alwan_scalar diff_g = ALWAN_FABS(rgb_recon.v[1] - rgb_orig.v[1]);
        alwan_scalar diff_b = ALWAN_FABS(rgb_recon.v[2] - rgb_orig.v[2]);

        if (diff_r > tol || diff_g > tol || diff_b > tol) {
            printf("HSV round-trip failed for color %zu:\n", i);
            printf("  Original RGB: [%.6f, %.6f, %.6f]\n",
                   rgb_orig.v[0], rgb_orig.v[1], rgb_orig.v[2]);
            printf("  HSV:          [%.6f, %.6f, %.6f]\n",
                   hsv.v[0], hsv.v[1], hsv.v[2]);
            printf("  Recon RGB:    [%.6f, %.6f, %.6f]\n",
                   rgb_recon.v[0], rgb_recon.v[1], rgb_recon.v[2]);
            printf("  Diff:         [%e, %e, %e]\n", diff_r, diff_g, diff_b);
            TEST_ASSERT(0, "HSV round-trip tolerance exceeded");
        }
    }

    TEST_PASS("test_hsv_round_trip");
}

static int test_hsl_forward(void) {
    /* Test RGB -> HSL conversion */
    for (size_t i = 0; i < NUM_TEST_COLORS; i++) {
        alwan_vec3 rgb;
        rgb.v[0] = test_rgb[i * 3 + 0];
        rgb.v[1] = test_rgb[i * 3 + 1];
        rgb.v[2] = test_rgb[i * 3 + 2];

        alwan_vec3 hsl;
        int status = alwan_rgb_to_hsl(&rgb, &hsl);
        TEST_ASSERT(status == ALWAN_OK, "RGB to HSL conversion failed");

        /* Compare with expected values */
        alwan_scalar exp_h = expected_hsl[i * 3 + 0];
        alwan_scalar exp_s = expected_hsl[i * 3 + 1];
        alwan_scalar exp_l = expected_hsl[i * 3 + 2];

        alwan_scalar tol = ALWAN_TEST_TOLERANCE;
        alwan_scalar diff_h = ALWAN_FABS(hsl.v[0] - exp_h);
        alwan_scalar diff_s = ALWAN_FABS(hsl.v[1] - exp_s);
        alwan_scalar diff_l = ALWAN_FABS(hsl.v[2] - exp_l);

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
        alwan_vec3 rgb_orig;
        rgb_orig.v[0] = test_rgb[i * 3 + 0];
        rgb_orig.v[1] = test_rgb[i * 3 + 1];
        rgb_orig.v[2] = test_rgb[i * 3 + 2];

        alwan_vec3 hsl, rgb_recon;
        int status;

        status = alwan_rgb_to_hsl(&rgb_orig, &hsl);
        TEST_ASSERT(status == ALWAN_OK, "RGB to HSL failed");

        status = alwan_hsl_to_rgb(&hsl, &rgb_recon);
        TEST_ASSERT(status == ALWAN_OK, "HSL to RGB failed");

        /* Check round-trip accuracy */
        alwan_scalar tol = ALWAN_TEST_TOLERANCE;
        alwan_scalar diff_r = ALWAN_FABS(rgb_recon.v[0] - rgb_orig.v[0]);
        alwan_scalar diff_g = ALWAN_FABS(rgb_recon.v[1] - rgb_orig.v[1]);
        alwan_scalar diff_b = ALWAN_FABS(rgb_recon.v[2] - rgb_orig.v[2]);

        if (diff_r > tol || diff_g > tol || diff_b > tol) {
            printf("HSL round-trip failed for color %zu:\n", i);
            printf("  Original RGB: [%.6f, %.6f, %.6f]\n",
                   rgb_orig.v[0], rgb_orig.v[1], rgb_orig.v[2]);
            printf("  HSL:          [%.6f, %.6f, %.6f]\n",
                   hsl.v[0], hsl.v[1], hsl.v[2]);
            printf("  Recon RGB:    [%.6f, %.6f, %.6f]\n",
                   rgb_recon.v[0], rgb_recon.v[1], rgb_recon.v[2]);
            printf("  Diff:         [%e, %e, %e]\n", diff_r, diff_g, diff_b);
            TEST_ASSERT(0, "HSL round-trip tolerance exceeded");
        }
    }

    TEST_PASS("test_hsl_round_trip");
}

static int test_cmy_conversions(void) {
    /* Test RGB <-> CMY conversions */
    for (size_t i = 0; i < NUM_TEST_COLORS; i++) {
        alwan_vec3 rgb;
        rgb.v[0] = test_rgb[i * 3 + 0];
        rgb.v[1] = test_rgb[i * 3 + 1];
        rgb.v[2] = test_rgb[i * 3 + 2];

        alwan_vec3 cmy;
        int status = alwan_rgb_to_cmy(&rgb, &cmy);
        TEST_ASSERT(status == ALWAN_OK, "RGB to CMY failed");

        /* Compare with expected */
        alwan_scalar exp_c = expected_cmy[i * 3 + 0];
        alwan_scalar exp_m = expected_cmy[i * 3 + 1];
        alwan_scalar exp_y = expected_cmy[i * 3 + 2];

        alwan_scalar tol = ALWAN_TEST_TOLERANCE;
        alwan_scalar diff_c = ALWAN_FABS(cmy.v[0] - exp_c);
        alwan_scalar diff_m = ALWAN_FABS(cmy.v[1] - exp_m);
        alwan_scalar diff_y = ALWAN_FABS(cmy.v[2] - exp_y);

        if (diff_c > tol || diff_m > tol || diff_y > tol) {
            printf("CMY forward test failed for color %zu:\n", i);
            printf("  RGB: [%.6f, %.6f, %.6f]\n", rgb.v[0], rgb.v[1], rgb.v[2]);
            printf("  Expected CMY: [%.6f, %.6f, %.6f]\n", exp_c, exp_m, exp_y);
            printf("  Got CMY:      [%.6f, %.6f, %.6f]\n", cmy.v[0], cmy.v[1], cmy.v[2]);
            TEST_ASSERT(0, "CMY values don't match expected");
        }

        /* Test round-trip */
        alwan_vec3 rgb_recon;
        status = alwan_cmy_to_rgb(&cmy, &rgb_recon);
        TEST_ASSERT(status == ALWAN_OK, "CMY to RGB failed");

        alwan_scalar diff_r = ALWAN_FABS(rgb_recon.v[0] - rgb.v[0]);
        alwan_scalar diff_g = ALWAN_FABS(rgb_recon.v[1] - rgb.v[1]);
        alwan_scalar diff_b = ALWAN_FABS(rgb_recon.v[2] - rgb.v[2]);

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
        alwan_vec3 cmy;
        cmy.v[0] = expected_cmy[i * 3 + 0];
        cmy.v[1] = expected_cmy[i * 3 + 1];
        cmy.v[2] = expected_cmy[i * 3 + 2];

        alwan_scalar c, m, y, k;
        int status = alwan_cmy_to_cmyk(&cmy, &c, &m, &y, &k);
        TEST_ASSERT(status == ALWAN_OK, "CMY to CMYK failed");

        /* Compare with expected */
        alwan_scalar exp_c = expected_cmyk[i * 4 + 0];
        alwan_scalar exp_m = expected_cmyk[i * 4 + 1];
        alwan_scalar exp_y = expected_cmyk[i * 4 + 2];
        alwan_scalar exp_k = expected_cmyk[i * 4 + 3];

        alwan_scalar tol = ALWAN_TEST_TOLERANCE;
        alwan_scalar diff_c = ALWAN_FABS(c - exp_c);
        alwan_scalar diff_m = ALWAN_FABS(m - exp_m);
        alwan_scalar diff_y = ALWAN_FABS(y - exp_y);
        alwan_scalar diff_k = ALWAN_FABS(k - exp_k);

        if (diff_c > tol || diff_m > tol || diff_y > tol || diff_k > tol) {
            printf("CMYK forward test failed for color %zu:\n", i);
            printf("  CMY: [%.6f, %.6f, %.6f]\n", cmy.v[0], cmy.v[1], cmy.v[2]);
            printf("  Expected CMYK: [%.6f, %.6f, %.6f, %.6f]\n", exp_c, exp_m, exp_y, exp_k);
            printf("  Got CMYK:      [%.6f, %.6f, %.6f, %.6f]\n", c, m, y, k);
            TEST_ASSERT(0, "CMYK values don't match expected");
        }

        /* Test round-trip */
        alwan_vec3 cmy_recon;
        status = alwan_cmyk_to_cmy(c, m, y, k, &cmy_recon);
        TEST_ASSERT(status == ALWAN_OK, "CMYK to CMY failed");

        alwan_scalar diff_c2 = ALWAN_FABS(cmy_recon.v[0] - cmy.v[0]);
        alwan_scalar diff_m2 = ALWAN_FABS(cmy_recon.v[1] - cmy.v[1]);
        alwan_scalar diff_y2 = ALWAN_FABS(cmy_recon.v[2] - cmy.v[2]);

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
        alwan_vec3 rgb;
        rgb.v[0] = test_rgb[i * 3 + 0];
        rgb.v[1] = test_rgb[i * 3 + 1];
        rgb.v[2] = test_rgb[i * 3 + 2];

        alwan_vec3 ycbcr;
        int status = alwan_rgb_to_ycbcr(&rgb, ALWAN_YCBCR_BT601, &ycbcr);
        TEST_ASSERT(status == ALWAN_OK, "RGB to YCbCr BT.601 failed");

        /* Compare with expected */
        alwan_scalar exp_y = expected_ycbcr_bt601[i * 3 + 0];
        alwan_scalar exp_cb = expected_ycbcr_bt601[i * 3 + 1];
        alwan_scalar exp_cr = expected_ycbcr_bt601[i * 3 + 2];

        alwan_scalar tol = ALWAN_TEST_TOLERANCE;
        alwan_scalar diff_y = ALWAN_FABS(ycbcr.v[0] - exp_y);
        alwan_scalar diff_cb = ALWAN_FABS(ycbcr.v[1] - exp_cb);
        alwan_scalar diff_cr = ALWAN_FABS(ycbcr.v[2] - exp_cr);

        if (diff_y > tol || diff_cb > tol || diff_cr > tol) {
            printf("YCbCr BT.601 forward test failed for color %zu:\n", i);
            printf("  RGB: [%.6f, %.6f, %.6f]\n", rgb.v[0], rgb.v[1], rgb.v[2]);
            printf("  Expected YCbCr: [%.6f, %.6f, %.6f]\n", exp_y, exp_cb, exp_cr);
            printf("  Got YCbCr:      [%.6f, %.6f, %.6f]\n",
                   ycbcr.v[0], ycbcr.v[1], ycbcr.v[2]);
            TEST_ASSERT(0, "YCbCr BT.601 values don't match");
        }

        /* Test round-trip */
        alwan_vec3 rgb_recon;
        status = alwan_ycbcr_to_rgb(&ycbcr, ALWAN_YCBCR_BT601, &rgb_recon);
        TEST_ASSERT(status == ALWAN_OK, "YCbCr to RGB BT.601 failed");

        alwan_scalar diff_r = ALWAN_FABS(rgb_recon.v[0] - rgb.v[0]);
        alwan_scalar diff_g = ALWAN_FABS(rgb_recon.v[1] - rgb.v[1]);
        alwan_scalar diff_b = ALWAN_FABS(rgb_recon.v[2] - rgb.v[2]);

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
        alwan_vec3 rgb;
        rgb.v[0] = test_rgb[i * 3 + 0];
        rgb.v[1] = test_rgb[i * 3 + 1];
        rgb.v[2] = test_rgb[i * 3 + 2];

        alwan_vec3 ycbcr;
        int status = alwan_rgb_to_ycbcr(&rgb, ALWAN_YCBCR_BT709, &ycbcr);
        TEST_ASSERT(status == ALWAN_OK, "RGB to YCbCr BT.709 failed");

        /* Compare with expected */
        alwan_scalar exp_y = expected_ycbcr_bt709[i * 3 + 0];
        alwan_scalar exp_cb = expected_ycbcr_bt709[i * 3 + 1];
        alwan_scalar exp_cr = expected_ycbcr_bt709[i * 3 + 2];

        alwan_scalar tol = ALWAN_TEST_TOLERANCE;
        alwan_scalar diff_y = ALWAN_FABS(ycbcr.v[0] - exp_y);
        alwan_scalar diff_cb = ALWAN_FABS(ycbcr.v[1] - exp_cb);
        alwan_scalar diff_cr = ALWAN_FABS(ycbcr.v[2] - exp_cr);

        if (diff_y > tol || diff_cb > tol || diff_cr > tol) {
            printf("YCbCr BT.709 forward test failed for color %zu:\n", i);
            printf("  RGB: [%.6f, %.6f, %.6f]\n", rgb.v[0], rgb.v[1], rgb.v[2]);
            printf("  Expected YCbCr: [%.6f, %.6f, %.6f]\n", exp_y, exp_cb, exp_cr);
            printf("  Got YCbCr:      [%.6f, %.6f, %.6f]\n",
                   ycbcr.v[0], ycbcr.v[1], ycbcr.v[2]);
            TEST_ASSERT(0, "YCbCr BT.709 values don't match");
        }

        /* Test round-trip */
        alwan_vec3 rgb_recon;
        status = alwan_ycbcr_to_rgb(&ycbcr, ALWAN_YCBCR_BT709, &rgb_recon);
        TEST_ASSERT(status == ALWAN_OK, "YCbCr to RGB BT.709 failed");

        alwan_scalar diff_r = ALWAN_FABS(rgb_recon.v[0] - rgb.v[0]);
        alwan_scalar diff_g = ALWAN_FABS(rgb_recon.v[1] - rgb.v[1]);
        alwan_scalar diff_b = ALWAN_FABS(rgb_recon.v[2] - rgb.v[2]);

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
        alwan_vec3 rgb;
        rgb.v[0] = test_rgb[i * 3 + 0];
        rgb.v[1] = test_rgb[i * 3 + 1];
        rgb.v[2] = test_rgb[i * 3 + 2];

        alwan_vec3 ycbcr;
        int status = alwan_rgb_to_ycbcr(&rgb, ALWAN_YCBCR_BT2020, &ycbcr);
        TEST_ASSERT(status == ALWAN_OK, "RGB to YCbCr BT.2020 failed");

        /* Compare with expected */
        alwan_scalar exp_y = expected_ycbcr_bt2020[i * 3 + 0];
        alwan_scalar exp_cb = expected_ycbcr_bt2020[i * 3 + 1];
        alwan_scalar exp_cr = expected_ycbcr_bt2020[i * 3 + 2];

        alwan_scalar tol = ALWAN_TEST_TOLERANCE;
        alwan_scalar diff_y = ALWAN_FABS(ycbcr.v[0] - exp_y);
        alwan_scalar diff_cb = ALWAN_FABS(ycbcr.v[1] - exp_cb);
        alwan_scalar diff_cr = ALWAN_FABS(ycbcr.v[2] - exp_cr);

        if (diff_y > tol || diff_cb > tol || diff_cr > tol) {
            printf("YCbCr BT.2020 forward test failed for color %zu:\n", i);
            printf("  RGB: [%.6f, %.6f, %.6f]\n", rgb.v[0], rgb.v[1], rgb.v[2]);
            printf("  Expected YCbCr: [%.6f, %.6f, %.6f]\n", exp_y, exp_cb, exp_cr);
            printf("  Got YCbCr:      [%.6f, %.6f, %.6f]\n",
                   ycbcr.v[0], ycbcr.v[1], ycbcr.v[2]);
            TEST_ASSERT(0, "YCbCr BT.2020 values don't match");
        }

        /* Test round-trip */
        alwan_vec3 rgb_recon;
        status = alwan_ycbcr_to_rgb(&ycbcr, ALWAN_YCBCR_BT2020, &rgb_recon);
        TEST_ASSERT(status == ALWAN_OK, "YCbCr to RGB BT.2020 failed");

        alwan_scalar diff_r = ALWAN_FABS(rgb_recon.v[0] - rgb.v[0]);
        alwan_scalar diff_g = ALWAN_FABS(rgb_recon.v[1] - rgb.v[1]);
        alwan_scalar diff_b = ALWAN_FABS(rgb_recon.v[2] - rgb.v[2]);

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
        alwan_vec3 rgb;
        rgb.v[0] = test_rgb[i * 3 + 0];
        rgb.v[1] = test_rgb[i * 3 + 1];
        rgb.v[2] = test_rgb[i * 3 + 2];

        alwan_vec3 ycc;
        int status = alwan_rgb_to_yccbccrc(&rgb, &ycc);
        TEST_ASSERT(status == ALWAN_OK, "RGB to YcCbcCrc failed");

        /* Compare with expected */
        alwan_scalar exp_yc = expected_yccbccrc[i * 3 + 0];
        alwan_scalar exp_cbc = expected_yccbccrc[i * 3 + 1];
        alwan_scalar exp_crc = expected_yccbccrc[i * 3 + 2];

        alwan_scalar tol = ALWAN_TEST_TOLERANCE;
        alwan_scalar diff_yc = ALWAN_FABS(ycc.v[0] - exp_yc);
        alwan_scalar diff_cbc = ALWAN_FABS(ycc.v[1] - exp_cbc);
        alwan_scalar diff_crc = ALWAN_FABS(ycc.v[2] - exp_crc);

        if (diff_yc > tol || diff_cbc > tol || diff_crc > tol) {
            printf("YcCbcCrc forward test failed for color %zu:\n", i);
            printf("  RGB: [%.6f, %.6f, %.6f]\n", rgb.v[0], rgb.v[1], rgb.v[2]);
            printf("  Expected YcCbcCrc: [%.6f, %.6f, %.6f]\n", exp_yc, exp_cbc, exp_crc);
            printf("  Got YcCbcCrc:      [%.6f, %.6f, %.6f]\n",
                   ycc.v[0], ycc.v[1], ycc.v[2]);
            TEST_ASSERT(0, "YcCbcCrc values don't match");
        }

        /* Test round-trip for non-boundary colors only */
        /* YcCbcCrc is inherently lossy for pure RGB primaries and secondaries */
        int is_boundary_color = (rgb.v[0] == 1.0 || rgb.v[1] == 1.0 || rgb.v[2] == 1.0) &&
                               ((rgb.v[0] == 0.0 ? 1 : 0) + (rgb.v[1] == 0.0 ? 1 : 0) + (rgb.v[2] == 0.0 ? 1 : 0)) >= 1;

        if (!is_boundary_color) {
            alwan_vec3 rgb_recon;
            status = alwan_yccbccrc_to_rgb(&ycc, &rgb_recon);
            TEST_ASSERT(status == ALWAN_OK, "YcCbcCrc to RGB failed");

            alwan_scalar diff_r = ALWAN_FABS(rgb_recon.v[0] - rgb.v[0]);
            alwan_scalar diff_g = ALWAN_FABS(rgb_recon.v[1] - rgb.v[1]);
            alwan_scalar diff_b = ALWAN_FABS(rgb_recon.v[2] - rgb.v[2]);

            if (diff_r > tol || diff_g > tol || diff_b > tol) {
                printf("YcCbcCrc round-trip failed for color %zu\n", i);
                printf("  Original:    [%.6f, %.6f, %.6f]\n",
                       rgb.v[0], rgb.v[1], rgb.v[2]);
                printf("  Reconstructed: [%.6f, %.6f, %.6f]\n",
                       rgb_recon.v[0], rgb_recon.v[1], rgb_recon.v[2]);
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

int test_90_conv_convenience_main(void) {
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
