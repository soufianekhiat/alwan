/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 71: Linear sRGB <-> HSV/HSL composite functions
 * Validates that the composite functions (OETF/EOTF + HSV/HSL) produce
 * results consistent with manual chaining, and that round-trips, map,
 * and map_ex variants all agree.
 */

#include "test_common.h"
#include "core/alwan_core.h"
#include <stdlib.h>
#include <string.h>

/* Test inputs: reuse the same 11 test colors as test 15 */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const test_rgb[] = {
#include "reference_values/test_rgb_colors.csv"
};
ALWAN_DIAG_POP

#define NUM_TEST_COLORS 11

/* ----------------------------------------------------------------
 * Grid generation for map tests (same as test 69)
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

static int compare_arrays(alwan_f64 const *map_out, alwan_f64 const *ref,
                          size_t count, size_t stride, char const *name) {
    for (size_t i = 0; i < count; i++) {
        alwan_f64 const *m = (alwan_f64 const *)((char const *)map_out + i * stride);
        alwan_f64 const *r = (alwan_f64 const *)((char const *)ref + i * stride);
        for (int c = 0; c < 3; c++) {
            alwan_f64 diff = ALWAN_ABS(m[c] - r[c]);
            if (diff > ALWAN_TEST_TOLERANCE) {
                printf("[FAIL] %s: pixel %zu ch %d: map=%.16e ref=%.16e diff=%.16e\n",
                       name, i, c, (double)m[c], (double)r[c], (double)diff);
                return 1;
            }
        }
    }
    return 0;
}

/* ----------------------------------------------------------------
 * Test: linear_srgb_to_hsv matches manual OETF + rgb_to_hsv
 * ---------------------------------------------------------------- */

static int test_linear_srgb_to_hsv_consistency(void) {
    for (size_t i = 0; i < NUM_TEST_COLORS; i++) {
        alwan_rgb_f64 linear;
        linear.r = test_rgb[i * 3 + 0];
        linear.g = test_rgb[i * 3 + 1];
        linear.b = test_rgb[i * 3 + 2];

        /* Composite function */
        alwan_hsv_f64 hsv_composite;
        int status = alwan_linear_srgb_to_hsv_f64(&hsv_composite, &linear);
        TEST_ASSERT(status == ALWAN_OK, "alwan_linear_srgb_to_hsv failed");

        /* Manual: OETF then HSV */
        alwan_rgb_f64 encoded;
        encoded.r = alwan_srgb_oetf_f64(linear.r);
        encoded.g = alwan_srgb_oetf_f64(linear.g);
        encoded.b = alwan_srgb_oetf_f64(linear.b);
        alwan_hsv_f64 hsv_manual;
        status = alwan_rgb_to_hsv_f64(&hsv_manual, &encoded);
        TEST_ASSERT(status == ALWAN_OK, "manual rgb_to_hsv failed");

        alwan_f64 tol = ALWAN_TEST_TOLERANCE;
        alwan_f64 diff_h = ALWAN_ABS(hsv_composite.h - hsv_manual.h);
        alwan_f64 diff_s = ALWAN_ABS(hsv_composite.s - hsv_manual.s);
        alwan_f64 diff_v = ALWAN_ABS(hsv_composite.v - hsv_manual.v);

        if (diff_h > tol || diff_s > tol || diff_v > tol) {
            printf("linear_srgb_to_hsv consistency failed for color %zu:\n", i);
            printf("  Composite: [%.10e, %.10e, %.10e]\n",
                   hsv_composite.h, hsv_composite.s, hsv_composite.v);
            printf("  Manual:    [%.10e, %.10e, %.10e]\n",
                   hsv_manual.h, hsv_manual.s, hsv_manual.v);
            TEST_ASSERT(0, "linear_srgb_to_hsv != OETF + rgb_to_hsv");
        }
    }
    TEST_PASS("test_linear_srgb_to_hsv_consistency");
}

/* ----------------------------------------------------------------
 * Test: linear_srgb_to_hsl matches manual OETF + rgb_to_hsl
 * ---------------------------------------------------------------- */

static int test_linear_srgb_to_hsl_consistency(void) {
    for (size_t i = 0; i < NUM_TEST_COLORS; i++) {
        alwan_rgb_f64 linear;
        linear.r = test_rgb[i * 3 + 0];
        linear.g = test_rgb[i * 3 + 1];
        linear.b = test_rgb[i * 3 + 2];

        alwan_hsl_f64 hsl_composite;
        int status = alwan_linear_srgb_to_hsl_f64(&hsl_composite, &linear);
        TEST_ASSERT(status == ALWAN_OK, "alwan_linear_srgb_to_hsl failed");

        alwan_rgb_f64 encoded;
        encoded.r = alwan_srgb_oetf_f64(linear.r);
        encoded.g = alwan_srgb_oetf_f64(linear.g);
        encoded.b = alwan_srgb_oetf_f64(linear.b);
        alwan_hsl_f64 hsl_manual;
        status = alwan_rgb_to_hsl_f64(&hsl_manual, &encoded);
        TEST_ASSERT(status == ALWAN_OK, "manual rgb_to_hsl failed");

        alwan_f64 tol = ALWAN_TEST_TOLERANCE;
        alwan_f64 diff_h = ALWAN_ABS(hsl_composite.h - hsl_manual.h);
        alwan_f64 diff_s = ALWAN_ABS(hsl_composite.s - hsl_manual.s);
        alwan_f64 diff_l = ALWAN_ABS(hsl_composite.l - hsl_manual.l);

        if (diff_h > tol || diff_s > tol || diff_l > tol) {
            printf("linear_srgb_to_hsl consistency failed for color %zu:\n", i);
            printf("  Composite: [%.10e, %.10e, %.10e]\n",
                   hsl_composite.h, hsl_composite.s, hsl_composite.l);
            printf("  Manual:    [%.10e, %.10e, %.10e]\n",
                   hsl_manual.h, hsl_manual.s, hsl_manual.l);
            TEST_ASSERT(0, "linear_srgb_to_hsl != OETF + rgb_to_hsl");
        }
    }
    TEST_PASS("test_linear_srgb_to_hsl_consistency");
}

/* ----------------------------------------------------------------
 * Test: HSV round-trip (linear -> HSV -> linear)
 * ---------------------------------------------------------------- */

static int test_linear_srgb_hsv_round_trip(void) {
    for (size_t i = 0; i < NUM_TEST_COLORS; i++) {
        alwan_rgb_f64 rgb_orig;
        rgb_orig.r = test_rgb[i * 3 + 0];
        rgb_orig.g = test_rgb[i * 3 + 1];
        rgb_orig.b = test_rgb[i * 3 + 2];

        alwan_hsv_f64 hsv;
        int status = alwan_linear_srgb_to_hsv_f64(&hsv, &rgb_orig);
        TEST_ASSERT(status == ALWAN_OK, "linear_srgb_to_hsv failed");

        alwan_rgb_f64 rgb_recon;
        status = alwan_hsv_to_linear_srgb_f64(&rgb_recon, &hsv);
        TEST_ASSERT(status == ALWAN_OK, "hsv_to_linear_srgb failed");

        alwan_f64 tol = ALWAN_TEST_TOLERANCE;
        alwan_f64 diff_r = ALWAN_ABS(rgb_recon.r - rgb_orig.r);
        alwan_f64 diff_g = ALWAN_ABS(rgb_recon.g - rgb_orig.g);
        alwan_f64 diff_b = ALWAN_ABS(rgb_recon.b - rgb_orig.b);

        if (diff_r > tol || diff_g > tol || diff_b > tol) {
            printf("linear_srgb HSV round-trip failed for color %zu:\n", i);
            printf("  Original: [%.10e, %.10e, %.10e]\n",
                   rgb_orig.r, rgb_orig.g, rgb_orig.b);
            printf("  HSV:      [%.10e, %.10e, %.10e]\n", hsv.h, hsv.s, hsv.v);
            printf("  Recon:    [%.10e, %.10e, %.10e]\n",
                   rgb_recon.r, rgb_recon.g, rgb_recon.b);
            printf("  Diff:     [%e, %e, %e]\n", diff_r, diff_g, diff_b);
            TEST_ASSERT(0, "linear_srgb HSV round-trip tolerance exceeded");
        }
    }
    TEST_PASS("test_linear_srgb_hsv_round_trip");
}

/* ----------------------------------------------------------------
 * Test: HSL round-trip (linear -> HSL -> linear)
 * ---------------------------------------------------------------- */

static int test_linear_srgb_hsl_round_trip(void) {
    for (size_t i = 0; i < NUM_TEST_COLORS; i++) {
        alwan_rgb_f64 rgb_orig;
        rgb_orig.r = test_rgb[i * 3 + 0];
        rgb_orig.g = test_rgb[i * 3 + 1];
        rgb_orig.b = test_rgb[i * 3 + 2];

        alwan_hsl_f64 hsl;
        int status = alwan_linear_srgb_to_hsl_f64(&hsl, &rgb_orig);
        TEST_ASSERT(status == ALWAN_OK, "linear_srgb_to_hsl failed");

        alwan_rgb_f64 rgb_recon;
        status = alwan_hsl_to_linear_srgb_f64(&rgb_recon, &hsl);
        TEST_ASSERT(status == ALWAN_OK, "hsl_to_linear_srgb failed");

        alwan_f64 tol = ALWAN_TEST_TOLERANCE;
        alwan_f64 diff_r = ALWAN_ABS(rgb_recon.r - rgb_orig.r);
        alwan_f64 diff_g = ALWAN_ABS(rgb_recon.g - rgb_orig.g);
        alwan_f64 diff_b = ALWAN_ABS(rgb_recon.b - rgb_orig.b);

        if (diff_r > tol || diff_g > tol || diff_b > tol) {
            printf("linear_srgb HSL round-trip failed for color %zu:\n", i);
            printf("  Original: [%.10e, %.10e, %.10e]\n",
                   rgb_orig.r, rgb_orig.g, rgb_orig.b);
            printf("  HSL:      [%.10e, %.10e, %.10e]\n", hsl.h, hsl.s, hsl.l);
            printf("  Recon:    [%.10e, %.10e, %.10e]\n",
                   rgb_recon.r, rgb_recon.g, rgb_recon.b);
            printf("  Diff:     [%e, %e, %e]\n", diff_r, diff_g, diff_b);
            TEST_ASSERT(0, "linear_srgb HSL round-trip tolerance exceeded");
        }
    }
    TEST_PASS("test_linear_srgb_hsl_round_trip");
}

/* ----------------------------------------------------------------
 * Test: Null pointer rejection
 * ---------------------------------------------------------------- */

static int test_null_pointers(void) {
    alwan_rgb_f64 rgb = {0};
    alwan_hsv_f64 hsv = {0};
    alwan_hsl_f64 hsl = {0};

    TEST_ASSERT(alwan_linear_srgb_to_hsv_f64(NULL, &rgb) == ALWAN_E_INVALID,
                "linear_srgb_to_hsv should reject null output");
    TEST_ASSERT(alwan_linear_srgb_to_hsv_f64(&hsv, NULL) == ALWAN_E_INVALID,
                "linear_srgb_to_hsv should reject null input");
    TEST_ASSERT(alwan_hsv_to_linear_srgb_f64(NULL, &hsv) == ALWAN_E_INVALID,
                "hsv_to_linear_srgb should reject null output");
    TEST_ASSERT(alwan_hsv_to_linear_srgb_f64(&rgb, NULL) == ALWAN_E_INVALID,
                "hsv_to_linear_srgb should reject null input");
    TEST_ASSERT(alwan_linear_srgb_to_hsl_f64(NULL, &rgb) == ALWAN_E_INVALID,
                "linear_srgb_to_hsl should reject null output");
    TEST_ASSERT(alwan_linear_srgb_to_hsl_f64(&hsl, NULL) == ALWAN_E_INVALID,
                "linear_srgb_to_hsl should reject null input");
    TEST_ASSERT(alwan_hsl_to_linear_srgb_f64(NULL, &hsl) == ALWAN_E_INVALID,
                "hsl_to_linear_srgb should reject null output");
    TEST_ASSERT(alwan_hsl_to_linear_srgb_f64(&rgb, NULL) == ALWAN_E_INVALID,
                "hsl_to_linear_srgb should reject null input");

    TEST_PASS("test_null_pointers");
}

/* ----------------------------------------------------------------
 * Test: Map vs per-pixel consistency
 * ---------------------------------------------------------------- */

static int test_linear_srgb_hsv_hsl_maps(void) {
    TEST_START("Map validation: linear_sRGB<->HSV, linear_sRGB<->HSL");

    size_t const stride = 3 * sizeof(alwan_f64);
    alwan_f64 *grid    = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *map_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *ref_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    if (!grid || !map_out || !ref_out) {
        free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc");
    }

    generate_unit_grid(grid);

    /* Linear sRGB -> HSV */
    alwan_linear_srgb_to_hsv_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_rgb_f64 rgb; rgb.r = grid[i*3+0]; rgb.g = grid[i*3+1]; rgb.b = grid[i*3+2];
        alwan_hsv_f64 hsv; alwan_linear_srgb_to_hsv_f64(&hsv, &rgb);
        ref_out[i*3+0] = hsv.h; ref_out[i*3+1] = hsv.s; ref_out[i*3+2] = hsv.v;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride,
                       "linear_srgb_to_hsv_map_interleave")) goto fail;

    /* HSV -> Linear sRGB */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_hsv_to_linear_srgb_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_hsv_f64 hsv; hsv.h = grid[i*3+0]; hsv.s = grid[i*3+1]; hsv.v = grid[i*3+2];
        alwan_rgb_f64 rgb; alwan_hsv_to_linear_srgb_f64(&rgb, &hsv);
        ref_out[i*3+0] = rgb.r; ref_out[i*3+1] = rgb.g; ref_out[i*3+2] = rgb.b;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride,
                       "hsv_to_linear_srgb_map_interleave")) goto fail;

    /* Linear sRGB -> HSL */
    generate_unit_grid(grid);
    alwan_linear_srgb_to_hsl_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_rgb_f64 rgb; rgb.r = grid[i*3+0]; rgb.g = grid[i*3+1]; rgb.b = grid[i*3+2];
        alwan_hsl_f64 hsl; alwan_linear_srgb_to_hsl_f64(&hsl, &rgb);
        ref_out[i*3+0] = hsl.h; ref_out[i*3+1] = hsl.s; ref_out[i*3+2] = hsl.l;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride,
                       "linear_srgb_to_hsl_map_interleave")) goto fail;

    /* HSL -> Linear sRGB */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_hsl_to_linear_srgb_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_hsl_f64 hsl; hsl.h = grid[i*3+0]; hsl.s = grid[i*3+1]; hsl.l = grid[i*3+2];
        alwan_rgb_f64 rgb; alwan_hsl_to_linear_srgb_f64(&rgb, &hsl);
        ref_out[i*3+0] = rgb.r; ref_out[i*3+1] = rgb.g; ref_out[i*3+2] = rgb.b;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride,
                       "hsl_to_linear_srgb_map_interleave")) goto fail;

    free(grid); free(map_out); free(ref_out);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(map_out); free(ref_out); return 1;
}

/* ----------------------------------------------------------------
 * Test: Map_ex vs map consistency (typed format validation)
 * ---------------------------------------------------------------- */

#define MAPEX_STEP 17
#define MAPEX_COUNT_1D ((255 / MAPEX_STEP) + 1)
#define MAPEX_COUNT (MAPEX_COUNT_1D * MAPEX_COUNT_1D * MAPEX_COUNT_1D)

static void generate_mapex_grid(alwan_f64 *out) {
    size_t idx = 0;
    for (int r = 0; r <= 255; r += MAPEX_STEP) {
        for (int g = 0; g <= 255; g += MAPEX_STEP) {
            for (int b = 0; b <= 255; b += MAPEX_STEP) {
                out[idx * 3 + 0] = (alwan_f64)r / ALWAN_LITERAL(255.0);
                out[idx * 3 + 1] = (alwan_f64)g / ALWAN_LITERAL(255.0);
                out[idx * 3 + 2] = (alwan_f64)b / ALWAN_LITERAL(255.0);
                idx++;
            }
        }
    }
}

static size_t mapex_typed_stride(alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8:  return 3 * sizeof(uint8_t);
    case ALWAN_PIXEL_U16: return 3 * sizeof(uint16_t);
    case ALWAN_PIXEL_F32: return 3 * sizeof(float);
    case ALWAN_PIXEL_F64: return 3 * sizeof(double);
    }
    return 0;
}

static alwan_f64 mapex_tol(alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8:  return ALWAN_LITERAL(3e-3);
    case ALWAN_PIXEL_U16: return ALWAN_LITERAL(1e-4);
    case ALWAN_PIXEL_F32: return ALWAN_LITERAL(1e-5);
    case ALWAN_PIXEL_F64: return ALWAN_TEST_TOLERANCE;
    }
    return ALWAN_TEST_TOLERANCE;
}

static int compare_mapex(alwan_f64 const *ex, alwan_f64 const *ref,
                         size_t count, size_t stride, char const *name,
                         alwan_f64 tol, alwan_pixel_format fmt) {
    for (size_t i = 0; i < count; i++) {
        alwan_f64 const *e = (alwan_f64 const *)((char const *)ex + i * stride);
        alwan_f64 const *r = (alwan_f64 const *)((char const *)ref + i * stride);
        for (int c = 0; c < 3; c++) {
            alwan_f64 diff = ALWAN_ABS(e[c] - r[c]);
            if (diff > tol) {
                printf("[FAIL] %s [%s]: pixel %zu ch %d: ex=%.16e ref=%.16e diff=%.16e\n",
                       name, test_fmt_name(fmt), i, c,
                       (double)e[c], (double)r[c], (double)diff);
                return 1;
            }
        }
    }
    return 0;
}

typedef int (*mapex_fn3)(alwan_f64 *, alwan_f64 const *, size_t, size_t, size_t);
typedef int (*mapex_fn3_ex)(void *, alwan_pixel_format, void const *, alwan_pixel_format,
                             size_t, size_t, size_t);
typedef struct { char const *name; mapex_fn3 map; mapex_fn3_ex map_ex; } mapex_entry3;

static int run_mapex3(mapex_entry3 const *entries, size_t n,
                       alwan_f64 const *grid) {
    size_t const ss = 3 * sizeof(alwan_f64);
    size_t const mt = 3 * sizeof(double);
    alwan_f64 *ref   = (alwan_f64 *)malloc(MAPEX_COUNT * ss);
    alwan_f64 *ex    = (alwan_f64 *)malloc(MAPEX_COUNT * ss);
    alwan_f64 *qgrid = (alwan_f64 *)malloc(MAPEX_COUNT * ss);
    void *tin  = malloc(MAPEX_COUNT * mt);
    void *tout = malloc(MAPEX_COUNT * mt);
    if (!ref || !ex || !qgrid || !tin || !tout) {
        free(ref); free(ex); free(qgrid); free(tin); free(tout);
        return 1;
    }
    for (size_t e = 0; e < n; e++) {
        for (int f = 0; f < 4; f++) {
            alwan_pixel_format fmt = TEST_PIXEL_FMTS[f];
            size_t ts = mapex_typed_stride(fmt);
            alwan_f64 tol = mapex_tol(fmt);

            alwan_scatter3(tin, fmt, grid, MAPEX_COUNT, ss, ts);

            if (fmt != ALWAN_PIXEL_F64) {
                alwan_collect3(qgrid, tin, fmt, MAPEX_COUNT, ts, ss);
                entries[e].map(ref, qgrid, MAPEX_COUNT, ss, ss);
            } else {
                entries[e].map(ref, grid, MAPEX_COUNT, ss, ss);
            }

            entries[e].map_ex(tout, fmt, tin, fmt, MAPEX_COUNT, ts, ts);
            alwan_collect3(ex, tout, fmt, MAPEX_COUNT, ts, ss);
            if (compare_mapex(ex, ref, MAPEX_COUNT, ss, entries[e].name, tol, fmt)) {
                free(ref); free(ex); free(qgrid); free(tin); free(tout);
                return 1;
            }
        }
    }
    free(ref); free(ex); free(qgrid); free(tin); free(tout);
    return 0;
}

static int test_linear_srgb_hsv_hsl_maps_ex(void) {
    TEST_START("_map_interleave_ex: linear_sRGB<->HSV/HSL");
    alwan_f64 *grid = (alwan_f64 *)malloc(MAPEX_COUNT * 3 * sizeof(alwan_f64));
    if (!grid) { TEST_FAIL("malloc"); }
    generate_mapex_grid(grid);

    static const mapex_entry3 entries[] = {
        {"linear_srgb_to_hsv", alwan_linear_srgb_to_hsv_map_interleave, alwan_linear_srgb_to_hsv_map_interleave_ex},
        {"hsv_to_linear_srgb", alwan_hsv_to_linear_srgb_map_interleave, alwan_hsv_to_linear_srgb_map_interleave_ex},
        {"linear_srgb_to_hsl", alwan_linear_srgb_to_hsl_map_interleave, alwan_linear_srgb_to_hsl_map_interleave_ex},
        {"hsl_to_linear_srgb", alwan_hsl_to_linear_srgb_map_interleave, alwan_hsl_to_linear_srgb_map_interleave_ex},
    };

    int r = run_mapex3(entries, sizeof(entries) / sizeof(entries[0]), grid);
    free(grid);
    if (r) return 1;
    TEST_PASS_MSG(); return 0;
}

/* ----------------------------------------------------------------
 * Test runner
 * ---------------------------------------------------------------- */

int test_71_linear_srgb_hsv_hsl_main(void) {
    printf("Test Suite 71: Linear sRGB <-> HSV/HSL\n");
    printf("========================================\n\n");

    printf("Per-pixel tests\n");
    printf("--------------------------------\n");
    if (test_linear_srgb_to_hsv_consistency() != 0) return 1;
    if (test_linear_srgb_to_hsl_consistency() != 0) return 1;
    if (test_linear_srgb_hsv_round_trip() != 0) return 1;
    if (test_linear_srgb_hsl_round_trip() != 0) return 1;
    if (test_null_pointers() != 0) return 1;

    printf("\nMap validation\n");
    printf("--------------------------------\n");
    if (test_linear_srgb_hsv_hsl_maps() != 0) return 1;

    printf("\nMap_ex typed format validation\n");
    printf("--------------------------------\n");
    if (test_linear_srgb_hsv_hsl_maps_ex() != 0) return 1;

    printf("\nAll test 71 tests passed!\n");
    return 0;
}
