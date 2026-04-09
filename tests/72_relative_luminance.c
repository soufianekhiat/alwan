/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 72: Relative Luminance
 * Validates alwan_relative_luminance for all supported standards,
 * the space-descriptor variant, the kr/kb variant, map functions,
 * and cross-checks against known coefficient values.
 */

#include "test_common.h"
#include <stdlib.h>
#include <string.h>

/* Test inputs: reuse the same 11 test colors */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const test_rgb[] = {
#include "reference_values/test_rgb_colors.csv"
};
ALWAN_DIAG_POP

#define NUM_TEST_COLORS 11

/* ----------------------------------------------------------------
 * Known coefficient tables for verification
 * ---------------------------------------------------------------- */

typedef struct {
    alwan_luma_standard standard;
    char const *name;
    alwan_f64 kr, kg, kb;
} luma_test_entry;

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static luma_test_entry const g_standards[] = {
    { ALWAN_LUMA_BT601,       "BT.601",       0.299,                 0.587,                 0.114                 },
    { ALWAN_LUMA_BT709,       "BT.709",       0.2126,                0.7152,                0.0722                },
    { ALWAN_LUMA_BT2020,      "BT.2020",      0.2627,                0.6780,                0.0593                },
    { ALWAN_LUMA_ACES_AP1,    "ACES AP1",     0.27222871678091454,   0.67408176581114831,   0.053689517407937051  },
    { ALWAN_LUMA_ACES_AP0,    "ACES AP0",     0.34396644976507512181,0.72816609661348574711,-0.07213254637856078560},
    { ALWAN_LUMA_DISPLAY_P3,  "Display P3",   0.22897456406974869836,0.69173852183650641479,0.07928691409374499879},
    { ALWAN_LUMA_DCI_P3,      "DCI-P3",       0.20949167791273051731,0.72159525416104375317,0.06891306792622581287},
    { ALWAN_LUMA_ADOBE_RGB,   "Adobe RGB",    0.29734497525053604772,0.62736356625546607635,0.07529145849399787594},
    { ALWAN_LUMA_PROPHOTO_RGB,"ProPhoto RGB", 0.28807112822929331619,0.71184321781010140295,0.00008565396052593485},
};
ALWAN_DIAG_POP

#define NUM_STANDARDS (sizeof(g_standards) / sizeof(g_standards[0]))

/* ----------------------------------------------------------------
 * Test: Coefficients sum to 1.0 (except AP0 which is derived from NPM)
 * ---------------------------------------------------------------- */

static int test_coefficient_sum(void) {
    for (size_t i = 0; i < NUM_STANDARDS; i++) {
        alwan_f64 sum = g_standards[i].kr + g_standards[i].kg + g_standards[i].kb;
        alwan_f64 diff = ALWAN_ABS(sum - ALWAN_LITERAL(1.0));
        if (diff > ALWAN_LITERAL(1e-10)) {
            printf("Coefficient sum for %s: %.16e (diff from 1.0: %e)\n",
                   g_standards[i].name, (double)sum, (double)diff);
            TEST_ASSERT(0, "Coefficients don't sum to 1.0");
        }
    }
    TEST_PASS("test_coefficient_sum");
}

/* ----------------------------------------------------------------
 * Test: Per-pixel luminance matches manual dot product
 * ---------------------------------------------------------------- */

static int test_per_pixel_all_standards(void) {
    for (size_t s = 0; s < NUM_STANDARDS; s++) {
        for (size_t i = 0; i < NUM_TEST_COLORS; i++) {
            alwan_rgb_f64 rgb;
            rgb.r = test_rgb[i * 3 + 0];
            rgb.g = test_rgb[i * 3 + 1];
            rgb.b = test_rgb[i * 3 + 2];

            alwan_f64 Y_api;
            int status = alwan_relative_luminance(&Y_api, &rgb, g_standards[s].standard);
            TEST_ASSERT(status == ALWAN_OK, "alwan_relative_luminance failed");

            /* Manual computation */
            alwan_f64 Y_manual = g_standards[s].kr * rgb.r
                                  + g_standards[s].kg * rgb.g
                                  + g_standards[s].kb * rgb.b;

            alwan_f64 diff = ALWAN_ABS(Y_api - Y_manual);
            if (diff > ALWAN_TEST_TOLERANCE) {
                printf("%s color %zu: api=%.16e manual=%.16e diff=%e\n",
                       g_standards[s].name, i, (double)Y_api, (double)Y_manual, (double)diff);
                TEST_ASSERT(0, "Luminance mismatch");
            }
        }
    }
    TEST_PASS("test_per_pixel_all_standards");
}

/* ----------------------------------------------------------------
 * Test: kr/kb variant matches standard variant
 * ---------------------------------------------------------------- */

static int test_kr_kb_variant(void) {
    for (size_t s = 0; s < NUM_STANDARDS; s++) {
        for (size_t i = 0; i < NUM_TEST_COLORS; i++) {
            alwan_rgb_f64 rgb;
            rgb.r = test_rgb[i * 3 + 0];
            rgb.g = test_rgb[i * 3 + 1];
            rgb.b = test_rgb[i * 3 + 2];

            alwan_f64 Y_std;
            alwan_relative_luminance(&Y_std, &rgb, g_standards[s].standard);

            alwan_f64 Y_kr_kb;
            alwan_relative_luminance_kr_kb_f64(&Y_kr_kb, &rgb,
                                           g_standards[s].kr, g_standards[s].kb);

            alwan_f64 diff = ALWAN_ABS(Y_std - Y_kr_kb);
            if (diff > ALWAN_TEST_TOLERANCE) {
                printf("%s color %zu: std=%.16e kr_kb=%.16e diff=%e\n",
                       g_standards[s].name, i, (double)Y_std, (double)Y_kr_kb, (double)diff);
                TEST_ASSERT(0, "kr_kb variant mismatch");
            }
        }
    }
    TEST_PASS("test_kr_kb_variant");
}

/* ----------------------------------------------------------------
 * Test: Known luminance values for pure white / black / primaries
 * ---------------------------------------------------------------- */

static int test_known_values(void) {
    /* White (1,1,1) should give Y = kr + kg + kb = 1.0 for all standards */
    alwan_rgb_f64 white = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)};
    for (size_t s = 0; s < NUM_STANDARDS; s++) {
        alwan_f64 Y;
        alwan_relative_luminance(&Y, &white, g_standards[s].standard);
        alwan_f64 diff = ALWAN_ABS(Y - ALWAN_LITERAL(1.0));
        if (diff > ALWAN_LITERAL(1e-10)) {
            printf("%s white luminance: %.16e (expected 1.0)\n",
                   g_standards[s].name, (double)Y);
            TEST_ASSERT(0, "White luminance != 1.0");
        }
    }

    /* Black (0,0,0) should give Y = 0.0 */
    alwan_rgb_f64 black = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
    for (size_t s = 0; s < NUM_STANDARDS; s++) {
        alwan_f64 Y;
        alwan_relative_luminance(&Y, &black, g_standards[s].standard);
        TEST_ASSERT(Y == ALWAN_LITERAL(0.0), "Black luminance != 0.0");
    }

    /* Pure red (1,0,0) in BT.709 should give Y = 0.2126 */
    alwan_rgb_f64 red = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
    alwan_f64 Y_red;
    alwan_relative_luminance(&Y_red, &red, ALWAN_LUMA_BT709);
    TEST_ASSERT_NEAR(Y_red, ALWAN_LITERAL(0.2126), ALWAN_TEST_TOLERANCE, "BT.709 red luminance");

    /* Pure green (0,1,0) in BT.709 should give Y = 0.7152 */
    alwan_rgb_f64 green = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0)};
    alwan_f64 Y_green;
    alwan_relative_luminance(&Y_green, &green, ALWAN_LUMA_BT709);
    TEST_ASSERT_NEAR(Y_green, ALWAN_LITERAL(0.7152), ALWAN_TEST_TOLERANCE, "BT.709 green luminance");

    /* Pure blue (0,0,1) in BT.709 should give Y = 0.0722 */
    alwan_rgb_f64 blue = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)};
    alwan_f64 Y_blue;
    alwan_relative_luminance(&Y_blue, &blue, ALWAN_LUMA_BT709);
    TEST_ASSERT_NEAR(Y_blue, ALWAN_LITERAL(0.0722), ALWAN_TEST_TOLERANCE, "BT.709 blue luminance");

    TEST_PASS("test_known_values");
}

/* ----------------------------------------------------------------
 * Test: Null pointer rejection
 * ---------------------------------------------------------------- */

static int test_null_pointers(void) {
    alwan_rgb_f64 rgb = {0};
    alwan_f64 Y;

    TEST_ASSERT(alwan_relative_luminance(NULL, &rgb, ALWAN_LUMA_BT709) == ALWAN_E_INVALID,
                "should reject null output");
    TEST_ASSERT(alwan_relative_luminance(&Y, NULL, ALWAN_LUMA_BT709) == ALWAN_E_INVALID,
                "should reject null input");
    TEST_ASSERT(alwan_relative_luminance_kr_kb_f64(NULL, &rgb, 0.2126, 0.0722) == ALWAN_E_INVALID,
                "kr_kb should reject null output");
    TEST_ASSERT(alwan_relative_luminance_kr_kb_f64(&Y, NULL, 0.2126, 0.0722) == ALWAN_E_INVALID,
                "kr_kb should reject null input");
    TEST_ASSERT(alwan_relative_luminance_space_f64(NULL, &rgb, NULL) == ALWAN_E_INVALID,
                "space should reject null output");
    TEST_ASSERT(alwan_relative_luminance_space_f64(&Y, NULL, NULL) == ALWAN_E_INVALID,
                "space should reject null input");
    TEST_ASSERT(alwan_relative_luminance_space_f64(&Y, &rgb, NULL) == ALWAN_E_INVALID,
                "space should reject null space");

    TEST_PASS("test_null_pointers");
}

/* ----------------------------------------------------------------
 * Test: Map vs per-pixel consistency
 * ---------------------------------------------------------------- */

#define MAP_STEP 5
#define MAP_COUNT_1D ((255 / MAP_STEP) + 1)
#define MAP_COUNT (MAP_COUNT_1D * MAP_COUNT_1D * MAP_COUNT_1D)

static void generate_unit_grid(alwan_f64 *out) {
    size_t idx = 0;
    for (int r = 0; r <= 255; r += MAP_STEP) {
        for (int g = 0; g <= 255; g += MAP_STEP) {
            for (int b = 0; b <= 255; b += MAP_STEP) {
                out[idx * 3 + 0] = (alwan_f64)r / ALWAN_LITERAL(255.0);
                out[idx * 3 + 1] = (alwan_f64)g / ALWAN_LITERAL(255.0);
                out[idx * 3 + 2] = (alwan_f64)b / ALWAN_LITERAL(255.0);
                idx++;
            }
        }
    }
}

static int test_luminance_maps(void) {
    TEST_START("Map validation: relative luminance (all standards)");

    size_t const in_stride = 3 * sizeof(alwan_f64);
    size_t const out_stride = sizeof(alwan_f64);
    alwan_f64 *grid    = (alwan_f64 *)malloc(MAP_COUNT * in_stride);
    alwan_f64 *map_out = (alwan_f64 *)malloc(MAP_COUNT * out_stride);
    alwan_f64 *ref_out = (alwan_f64 *)malloc(MAP_COUNT * out_stride);
    if (!grid || !map_out || !ref_out) {
        free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc");
    }

    generate_unit_grid(grid);

    for (size_t s = 0; s < NUM_STANDARDS; s++) {
        /* Map version */
        alwan_relative_luminance_f64_map_interleave(map_out, grid, MAP_COUNT,
                                                 g_standards[s].standard,
                                                 in_stride, out_stride);

        /* Per-pixel reference */
        for (size_t i = 0; i < MAP_COUNT; i++) {
            alwan_rgb_f64 rgb;
            rgb.r = grid[i * 3 + 0];
            rgb.g = grid[i * 3 + 1];
            rgb.b = grid[i * 3 + 2];
            alwan_relative_luminance(&ref_out[i], &rgb, g_standards[s].standard);
        }

        /* Compare */
        for (size_t i = 0; i < MAP_COUNT; i++) {
            alwan_f64 diff = ALWAN_ABS(map_out[i] - ref_out[i]);
            if (diff > ALWAN_TEST_TOLERANCE) {
                printf("[FAIL] %s map pixel %zu: map=%.16e ref=%.16e diff=%e\n",
                       g_standards[s].name, i,
                       (double)map_out[i], (double)ref_out[i], (double)diff);
                goto fail;
            }
        }
    }

    free(grid); free(map_out); free(ref_out);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(map_out); free(ref_out); return 1;
}

/* ----------------------------------------------------------------
 * Test: Space-descriptor map vs enum map consistency (BT.709)
 * ---------------------------------------------------------------- */

static int test_space_descriptor_map(void) {
    TEST_START("Map validation: space descriptor vs enum (BT.709)");

    /* Build a BT.709 space descriptor manually with known NPM */
    alwan_rgb_space_desc bt709;
    memset(&bt709, 0, sizeof(bt709));
    /* Y row of sRGB/BT.709 NPM */
    bt709.rgb_to_xyz.m[3] = ALWAN_LUMA_KR_BT709;
    bt709.rgb_to_xyz.m[4] = ALWAN_LUMA_KG_BT709;
    bt709.rgb_to_xyz.m[5] = ALWAN_LUMA_KB_BT709;
    bt709.has_matrices = 1;

    size_t const in_stride = 3 * sizeof(alwan_f64);
    size_t const out_stride = sizeof(alwan_f64);
    alwan_f64 *grid      = (alwan_f64 *)malloc(MAP_COUNT * in_stride);
    alwan_f64 *map_enum  = (alwan_f64 *)malloc(MAP_COUNT * out_stride);
    alwan_f64 *map_space = (alwan_f64 *)malloc(MAP_COUNT * out_stride);
    if (!grid || !map_enum || !map_space) {
        free(grid); free(map_enum); free(map_space); TEST_FAIL("malloc");
    }

    generate_unit_grid(grid);

    alwan_relative_luminance_f64_map_interleave(map_enum, grid, MAP_COUNT,
                                             ALWAN_LUMA_BT709,
                                             in_stride, out_stride);
    alwan_relative_luminance_space_f64_map_interleave(map_space, grid, MAP_COUNT,
                                                   &bt709,
                                                   in_stride, out_stride);

    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_f64 diff = ALWAN_ABS(map_enum[i] - map_space[i]);
        if (diff > ALWAN_TEST_TOLERANCE) {
            printf("[FAIL] space vs enum pixel %zu: enum=%.16e space=%.16e diff=%e\n",
                   i, (double)map_enum[i], (double)map_space[i], (double)diff);
            free(grid); free(map_enum); free(map_space);
            return 1;
        }
    }

    free(grid); free(map_enum); free(map_space);
    TEST_PASS_MSG(); return 0;
}

/* ----------------------------------------------------------------
 * Test runner
 * ---------------------------------------------------------------- */

int test_72_relative_luminance_main(void) {
    printf("Test Suite 72: Relative Luminance\n");
    printf("========================================\n\n");

    printf("Coefficient validation\n");
    printf("--------------------------------\n");
    if (test_coefficient_sum() != 0) return 1;
    if (test_known_values() != 0) return 1;

    printf("\nPer-pixel tests\n");
    printf("--------------------------------\n");
    if (test_per_pixel_all_standards() != 0) return 1;
    if (test_kr_kb_variant() != 0) return 1;
    if (test_null_pointers() != 0) return 1;

    printf("\nMap validation\n");
    printf("--------------------------------\n");
    if (test_luminance_maps() != 0) return 1;
    if (test_space_descriptor_map() != 0) return 1;

    printf("\nAll test 72 tests passed!\n");
    return 0;
}
