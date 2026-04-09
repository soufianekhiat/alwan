/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test suite 69: Map Validation
 * Validates that _map_interleave (batch/SIMD) functions produce identical results
 * to their single-pixel counterparts within ALWAN_TEST_TOLERANCE.
 */

#include "test_common.h"
#include <stdlib.h>
#include <string.h>


/* ----------------------------------------------------------------
 * Grid generation
 * ---------------------------------------------------------------- */

#define MAP_STEP 5
#define MAP_COUNT_1D ((255 / MAP_STEP) + 1)  /* 52 */
#define MAP_COUNT (MAP_COUNT_1D * MAP_COUNT_1D * MAP_COUNT_1D)  /* 140608 */
_Static_assert(MAP_COUNT >= MIN_SIMD_PIXELS, "MAP_COUNT too small to exercise SIMD");

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

static void generate_lab_grid(alwan_f64 *out) {
    size_t idx = 0;
    for (int r = 0; r <= 255; r += MAP_STEP) {
        for (int g = 0; g <= 255; g += MAP_STEP) {
            for (int b = 0; b <= 255; b += MAP_STEP) {
                out[idx * 3 + 0] = (alwan_f64)r / ALWAN_LITERAL(255.0) * ALWAN_LITERAL(100.0);
                out[idx * 3 + 1] = ((alwan_f64)g / ALWAN_LITERAL(255.0) - ALWAN_LITERAL(0.5)) * ALWAN_LITERAL(256.0);
                out[idx * 3 + 2] = ((alwan_f64)b / ALWAN_LITERAL(255.0) - ALWAN_LITERAL(0.5)) * ALWAN_LITERAL(256.0);
                idx++;
            }
        }
    }
}

/* ----------------------------------------------------------------
 * Helper: compare map output vs per-pixel reference
 * ---------------------------------------------------------------- */


static int compare_arrays_tol(alwan_f64 const *map_out, alwan_f64 const *ref,
                              size_t count, size_t stride, char const *name,
                              alwan_f64 tol) {
    for (size_t i = 0; i < count; i++) {
        alwan_f64 const *m = (alwan_f64 const *)((char const *)map_out + i * stride);
        alwan_f64 const *r = (alwan_f64 const *)((char const *)ref + i * stride);
        for (int c = 0; c < 3; c++) {
            alwan_f64 diff = ALWAN_ABS(m[c] - r[c]);
            if (diff > tol) {
                printf("[FAIL] %s: pixel %zu ch %d: map=%.16e ref=%.16e diff=%.16e\n",
                       name, i, c, (double)m[c], (double)r[c], (double)diff);
                return 1;
            }
        }
    }
    return 0;
}

static int compare_arrays(alwan_f64 const *map_out, alwan_f64 const *ref,
                          size_t count, size_t stride, char const *name) {
    return compare_arrays_tol(map_out, ref, count, stride, name, ALWAN_SIMD_TOLERANCE);
}

/* ----------------------------------------------------------------
 * Group A: Simple 3->3 (no extra params)
 * ---------------------------------------------------------------- */

static int test_oklab_maps(void) {
    TEST_START("Map validation: OkLab XYZ<->OkLab, OkLab<->OkLCh");

    size_t const stride = 3 * sizeof(alwan_f64);
    alwan_f64 *grid = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *map_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *ref_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    if (!grid || !map_out || !ref_out) { free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc"); }

    generate_unit_grid(grid);

    /* XYZ -> OkLab */
    alwan_xyz_to_oklab_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_xyz_f64 xyz; xyz.x = grid[i*3+0]; xyz.y = grid[i*3+1]; xyz.z = grid[i*3+2];
        alwan_oklab_f64 r; alwan_xyz_to_oklab_f64(&r, &xyz);
        ref_out[i*3+0] = r.L; ref_out[i*3+1] = r.a; ref_out[i*3+2] = r.b;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "xyz_to_oklab_map_interleave")) goto fail;

    /* OkLab -> XYZ */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_oklab_to_xyz_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_oklab_f64 ok; ok.L = grid[i*3+0]; ok.a = grid[i*3+1]; ok.b = grid[i*3+2];
        alwan_xyz_f64 r; alwan_oklab_to_xyz_f64(&r, &ok);
        ref_out[i*3+0] = r.x; ref_out[i*3+1] = r.y; ref_out[i*3+2] = r.z;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "oklab_to_xyz_map_interleave")) goto fail;

    /* OkLab -> OkLCh */
    generate_unit_grid(grid);
    alwan_xyz_to_oklab_f64_map_interleave(grid, grid, MAP_COUNT, stride, stride);
    alwan_oklab_to_oklch_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_oklab_f64 ok; ok.L = grid[i*3+0]; ok.a = grid[i*3+1]; ok.b = grid[i*3+2];
        alwan_oklch_f64 r; alwan_oklab_to_oklch_f64(&r, &ok);
        ref_out[i*3+0] = r.L; ref_out[i*3+1] = r.C; ref_out[i*3+2] = r.h;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "oklab_to_oklch_map_interleave")) goto fail;

    /* OkLCh -> OkLab */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_oklch_to_oklab_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_oklch_f64 lch; lch.L = grid[i*3+0]; lch.C = grid[i*3+1]; lch.h = grid[i*3+2];
        alwan_oklab_f64 r; alwan_oklch_to_oklab_f64(&r, &lch);
        ref_out[i*3+0] = r.L; ref_out[i*3+1] = r.a; ref_out[i*3+2] = r.b;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "oklch_to_oklab_map_interleave")) goto fail;

    free(grid); free(map_out); free(ref_out);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(map_out); free(ref_out); return 1;
}

static int test_lab_lch_maps(void) {
    TEST_START("Map validation: Lab<->LCh, Luv<->LChuv");

    size_t const stride = 3 * sizeof(alwan_f64);
    alwan_f64 *grid = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *map_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *ref_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    if (!grid || !map_out || !ref_out) { free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc"); }

    generate_lab_grid(grid);

    /* Lab -> LCh */
    alwan_lab_to_lch_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_lab_f64 lab; lab.L = grid[i*3+0]; lab.a = grid[i*3+1]; lab.b = grid[i*3+2];
        alwan_lch_f64 lch; alwan_lab_to_lch_f64(&lch, &lab);
        ref_out[i*3+0] = lch.L; ref_out[i*3+1] = lch.C; ref_out[i*3+2] = lch.h;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "lab_to_lch_map_interleave")) goto fail;

    /* LCh -> Lab */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_lch_to_lab_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_lch_f64 lch; lch.L = grid[i*3+0]; lch.C = grid[i*3+1]; lch.h = grid[i*3+2];
        alwan_lab_f64 lab; alwan_lch_to_lab_f64(&lab, &lch);
        ref_out[i*3+0] = lab.L; ref_out[i*3+1] = lab.a; ref_out[i*3+2] = lab.b;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "lch_to_lab_map_interleave")) goto fail;

    /* Luv -> LChuv */
    generate_lab_grid(grid);
    alwan_luv_to_lchuv_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_luv_f64 luv; luv.L = grid[i*3+0]; luv.u = grid[i*3+1]; luv.v = grid[i*3+2];
        alwan_lchuv_f64 lchuv; alwan_luv_to_lchuv_f64(&lchuv, &luv);
        ref_out[i*3+0] = lchuv.L; ref_out[i*3+1] = lchuv.C; ref_out[i*3+2] = lchuv.h;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "luv_to_lchuv_map_interleave")) goto fail;

    /* LChuv -> Luv */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_lchuv_to_luv_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_lchuv_f64 lchuv; lchuv.L = grid[i*3+0]; lchuv.C = grid[i*3+1]; lchuv.h = grid[i*3+2];
        alwan_luv_f64 luv; alwan_lchuv_to_luv_f64(&luv, &lchuv);
        ref_out[i*3+0] = luv.L; ref_out[i*3+1] = luv.u; ref_out[i*3+2] = luv.v;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "lchuv_to_luv_map_interleave")) goto fail;

    free(grid); free(map_out); free(ref_out);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(map_out); free(ref_out); return 1;
}

static int test_xyy_maps(void) {
    TEST_START("Map validation: XYZ<->xyY");

    size_t const stride = 3 * sizeof(alwan_f64);
    alwan_f64 *grid = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *map_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *ref_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    if (!grid || !map_out || !ref_out) { free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc"); }

    generate_unit_grid(grid);

    /* XYZ -> xyY */
    alwan_xyz_to_xyy_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_xyz_f64 xyz; xyz.x = grid[i*3+0]; xyz.y = grid[i*3+1]; xyz.z = grid[i*3+2];
        alwan_xyy_f64 xyy; alwan_xyz_to_xyy_f64(&xyy, &xyz);
        ref_out[i*3+0] = xyy.x; ref_out[i*3+1] = xyy.y; ref_out[i*3+2] = xyy.Y;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "xyz_to_xyy_map_interleave")) goto fail;

    /* xyY -> XYZ */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_xyy_to_xyz_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_xyy_f64 xyy; xyy.x = grid[i*3+0]; xyy.y = grid[i*3+1]; xyy.Y = grid[i*3+2];
        alwan_xyz_f64 xyz; alwan_xyy_to_xyz_f64(&xyz, &xyy);
        ref_out[i*3+0] = xyz.x; ref_out[i*3+1] = xyz.y; ref_out[i*3+2] = xyz.z;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "xyy_to_xyz_map_interleave")) goto fail;

    free(grid); free(map_out); free(ref_out);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(map_out); free(ref_out); return 1;
}

static int test_hsv_hsl_maps(void) {
    TEST_START("Map validation: RGB<->HSV, RGB<->HSL");

    size_t const stride = 3 * sizeof(alwan_f64);
    alwan_f64 *grid = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *map_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *ref_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    if (!grid || !map_out || !ref_out) { free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc"); }

    generate_unit_grid(grid);

    /* RGB -> HSV */
    alwan_rgb_to_hsv_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_rgb_f64 rgb; rgb.r = grid[i*3+0]; rgb.g = grid[i*3+1]; rgb.b = grid[i*3+2];
        alwan_hsv_f64 hsv; alwan_rgb_to_hsv_f64(&hsv, &rgb);
        ref_out[i*3+0] = hsv.h; ref_out[i*3+1] = hsv.s; ref_out[i*3+2] = hsv.v;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "rgb_to_hsv_map_interleave")) goto fail;

    /* HSV -> RGB */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_hsv_to_rgb_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_hsv_f64 hsv; hsv.h = grid[i*3+0]; hsv.s = grid[i*3+1]; hsv.v = grid[i*3+2];
        alwan_rgb_f64 rgb; alwan_hsv_to_rgb_f64(&rgb, &hsv);
        ref_out[i*3+0] = rgb.r; ref_out[i*3+1] = rgb.g; ref_out[i*3+2] = rgb.b;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "hsv_to_rgb_map_interleave")) goto fail;

    /* RGB -> HSL */
    generate_unit_grid(grid);
    alwan_rgb_to_hsl_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_rgb_f64 rgb; rgb.r = grid[i*3+0]; rgb.g = grid[i*3+1]; rgb.b = grid[i*3+2];
        alwan_hsl_f64 hsl; alwan_rgb_to_hsl_f64(&hsl, &rgb);
        ref_out[i*3+0] = hsl.h; ref_out[i*3+1] = hsl.s; ref_out[i*3+2] = hsl.l;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "rgb_to_hsl_map_interleave")) goto fail;

    /* HSL -> RGB */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_hsl_to_rgb_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_hsl_f64 hsl; hsl.h = grid[i*3+0]; hsl.s = grid[i*3+1]; hsl.l = grid[i*3+2];
        alwan_rgb_f64 rgb; alwan_hsl_to_rgb_f64(&rgb, &hsl);
        ref_out[i*3+0] = rgb.r; ref_out[i*3+1] = rgb.g; ref_out[i*3+2] = rgb.b;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "hsl_to_rgb_map_interleave")) goto fail;

    free(grid); free(map_out); free(ref_out);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(map_out); free(ref_out); return 1;
}

/* ----------------------------------------------------------------
 * sRGB convenience maps
 * Validate _map_interleave vs _map_interleave with count=1 (single-pixel functions not implemented)
 * ---------------------------------------------------------------- */

static int test_srgb_convenience_maps(void) {
    TEST_START("Map validation: sRGB convenience (srgb<->xyz, srgb<->lab, srgb<->oklab)");

    size_t const stride = 3 * sizeof(alwan_f64);
    alwan_f64 *grid = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *map_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *ref_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    if (!grid || !map_out || !ref_out) { free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc"); }

    generate_unit_grid(grid);

    /* sRGB -> XYZ: compare map against per-pixel map(count=1) */
    alwan_srgb_to_xyz_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_srgb_to_xyz_f64_map_interleave(&ref_out[i*3], &grid[i*3], 1, stride, stride);
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "srgb_to_xyz_map_interleave")) goto fail;

    /* XYZ -> sRGB */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_xyz_to_srgb_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_xyz_to_srgb_f64_map_interleave(&ref_out[i*3], &grid[i*3], 1, stride, stride);
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "xyz_to_srgb_map_interleave")) goto fail;

    /* sRGB -> Lab */
    generate_unit_grid(grid);
    alwan_srgb_to_lab_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_srgb_to_lab_f64_map_interleave(&ref_out[i*3], &grid[i*3], 1, stride, stride);
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "srgb_to_lab_map_interleave")) goto fail;

    /* Lab -> sRGB */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_lab_to_srgb_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_lab_to_srgb_f64_map_interleave(&ref_out[i*3], &grid[i*3], 1, stride, stride);
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "lab_to_srgb_map_interleave")) goto fail;

    /* sRGB -> OkLab */
    generate_unit_grid(grid);
    alwan_srgb_to_oklab_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_srgb_to_oklab_f64_map_interleave(&ref_out[i*3], &grid[i*3], 1, stride, stride);
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "srgb_to_oklab_map_interleave")) goto fail;

    /* OkLab -> sRGB */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_oklab_to_srgb_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_oklab_to_srgb_f64_map_interleave(&ref_out[i*3], &grid[i*3], 1, stride, stride);
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "oklab_to_srgb_map_interleave")) goto fail;

    free(grid); free(map_out); free(ref_out);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(map_out); free(ref_out); return 1;
}

/* ----------------------------------------------------------------
 * Group B: White point (Lab, Luv)
 * ---------------------------------------------------------------- */

static int test_white_point_maps(void) {
    TEST_START("Map validation: XYZ<->Lab, XYZ<->Luv (with white point)");

    size_t const stride = 3 * sizeof(alwan_f64);
    alwan_f64 *grid = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *map_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *ref_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    if (!grid || !map_out || !ref_out) { free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc"); }

    alwan_xyz_f64 d65; d65.x = g_d65_xyz_y1[0]; d65.y = g_d65_xyz_y1[1]; d65.z = g_d65_xyz_y1[2];

    generate_unit_grid(grid);

    /* XYZ -> Lab */
    alwan_xyz_to_lab_f64_map_interleave(map_out, grid, &d65, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_xyz_f64 xyz; xyz.x = grid[i*3+0]; xyz.y = grid[i*3+1]; xyz.z = grid[i*3+2];
        alwan_lab_f64 lab; alwan_xyz_to_lab_f64(&lab, &xyz, &d65);
        ref_out[i*3+0] = lab.L; ref_out[i*3+1] = lab.a; ref_out[i*3+2] = lab.b;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "xyz_to_lab_map_interleave")) goto fail;

    /* Lab -> XYZ */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_lab_to_xyz_f64_map_interleave(map_out, grid, &d65, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_lab_f64 lab; lab.L = grid[i*3+0]; lab.a = grid[i*3+1]; lab.b = grid[i*3+2];
        alwan_xyz_f64 xyz; alwan_lab_to_xyz_f64(&xyz, &lab, &d65);
        ref_out[i*3+0] = xyz.x; ref_out[i*3+1] = xyz.y; ref_out[i*3+2] = xyz.z;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "lab_to_xyz_map_interleave")) goto fail;

    /* XYZ -> Luv */
    generate_unit_grid(grid);
    alwan_xyz_to_luv_f64_map_interleave(map_out, grid, &d65, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_xyz_f64 xyz; xyz.x = grid[i*3+0]; xyz.y = grid[i*3+1]; xyz.z = grid[i*3+2];
        alwan_luv_f64 luv; alwan_xyz_to_luv_f64(&luv, &xyz, &d65);
        ref_out[i*3+0] = luv.L; ref_out[i*3+1] = luv.u; ref_out[i*3+2] = luv.v;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "xyz_to_luv_map_interleave")) goto fail;

    /* Luv -> XYZ */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_luv_to_xyz_f64_map_interleave(map_out, grid, &d65, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_luv_f64 luv; luv.L = grid[i*3+0]; luv.u = grid[i*3+1]; luv.v = grid[i*3+2];
        alwan_xyz_f64 xyz; alwan_luv_to_xyz_f64(&xyz, &luv, &d65);
        ref_out[i*3+0] = xyz.x; ref_out[i*3+1] = xyz.y; ref_out[i*3+2] = xyz.z;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "luv_to_xyz_map_interleave")) goto fail;

    free(grid); free(map_out); free(ref_out);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(map_out); free(ref_out); return 1;
}

/* ----------------------------------------------------------------
 * Group C: ICtCp (use_pq parameter)
 * ---------------------------------------------------------------- */

static int test_ictcp_maps(void) {
    TEST_START("Map validation: ICtCp (RGB<->ICtCp, XYZ<->ICtCp, PQ+HLG)");

    size_t const stride = 3 * sizeof(alwan_f64);
    alwan_f64 *grid = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *map_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *ref_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    if (!grid || !map_out || !ref_out) { free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc"); }

    for (int use_pq = 0; use_pq <= 1; use_pq++) {
        /* RGB -> ICtCp */
        generate_unit_grid(grid);
        alwan_rgb_to_ictcp_f64_map_interleave(map_out, grid, use_pq, MAP_COUNT, stride, stride);
        for (size_t i = 0; i < MAP_COUNT; i++) {
            alwan_rgb_f64 rgb; rgb.r = grid[i*3+0]; rgb.g = grid[i*3+1]; rgb.b = grid[i*3+2];
            alwan_ictcp_f64 ictcp; alwan_rgb_to_ictcp_f64(&ictcp, &rgb, use_pq);
            ref_out[i*3+0] = ictcp.I; ref_out[i*3+1] = ictcp.Ct; ref_out[i*3+2] = ictcp.Cp;
        }
        if (compare_arrays_tol(map_out, ref_out, MAP_COUNT, stride,
                           use_pq ? "rgb_to_ictcp_map_interleave PQ" : "rgb_to_ictcp_map_interleave HLG", ALWAN_SIMD_TOLERANCE)) goto fail;

        /* ICtCp -> RGB */
        memcpy(grid, ref_out, MAP_COUNT * stride);
        alwan_ictcp_to_rgb_f64_map_interleave(map_out, grid, use_pq, MAP_COUNT, stride, stride);
        for (size_t i = 0; i < MAP_COUNT; i++) {
            alwan_ictcp_f64 ictcp; ictcp.I = grid[i*3+0]; ictcp.Ct = grid[i*3+1]; ictcp.Cp = grid[i*3+2];
            alwan_rgb_f64 rgb; alwan_ictcp_to_rgb_f64(&rgb, &ictcp, use_pq);
            ref_out[i*3+0] = rgb.r; ref_out[i*3+1] = rgb.g; ref_out[i*3+2] = rgb.b;
        }
        if (compare_arrays_tol(map_out, ref_out, MAP_COUNT, stride,
                           use_pq ? "ictcp_to_rgb_map_interleave PQ" : "ictcp_to_rgb_map_interleave HLG", ALWAN_SIMD_TOLERANCE)) goto fail;

        /* XYZ -> ICtCp */
        generate_unit_grid(grid);
        alwan_xyz_to_ictcp_f64_map_interleave(map_out, grid, use_pq, MAP_COUNT, stride, stride);
        for (size_t i = 0; i < MAP_COUNT; i++) {
            alwan_xyz_f64 xyz; xyz.x = grid[i*3+0]; xyz.y = grid[i*3+1]; xyz.z = grid[i*3+2];
            alwan_ictcp_f64 ictcp; alwan_xyz_to_ictcp_f64(&ictcp, &xyz, use_pq);
            ref_out[i*3+0] = ictcp.I; ref_out[i*3+1] = ictcp.Ct; ref_out[i*3+2] = ictcp.Cp;
        }
        if (compare_arrays_tol(map_out, ref_out, MAP_COUNT, stride,
                           use_pq ? "xyz_to_ictcp_map_interleave PQ" : "xyz_to_ictcp_map_interleave HLG", ALWAN_SIMD_TOLERANCE)) goto fail;

        /* ICtCp -> XYZ */
        memcpy(grid, ref_out, MAP_COUNT * stride);
        alwan_ictcp_to_xyz_f64_map_interleave(map_out, grid, use_pq, MAP_COUNT, stride, stride);
        for (size_t i = 0; i < MAP_COUNT; i++) {
            alwan_ictcp_f64 ictcp; ictcp.I = grid[i*3+0]; ictcp.Ct = grid[i*3+1]; ictcp.Cp = grid[i*3+2];
            alwan_xyz_f64 xyz; alwan_ictcp_to_xyz_f64(&xyz, &ictcp, use_pq);
            ref_out[i*3+0] = xyz.x; ref_out[i*3+1] = xyz.y; ref_out[i*3+2] = xyz.z;
        }
        if (compare_arrays_tol(map_out, ref_out, MAP_COUNT, stride,
                           use_pq ? "ictcp_to_xyz_map_interleave PQ" : "ictcp_to_xyz_map_interleave HLG", ALWAN_SIMD_TOLERANCE)) goto fail;
    }

    free(grid); free(map_out); free(ref_out);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(map_out); free(ref_out); return 1;
}

/* ----------------------------------------------------------------
 * Group D: JzAzBz (pointer API)
 * ---------------------------------------------------------------- */

static int test_jzazbz_maps(void) {
    TEST_START("Map validation: JzAzBz (XYZ<->JzAzBz, JzAzBz<->JzCzhz)");

    size_t const stride = 3 * sizeof(alwan_f64);
    alwan_f64 *grid = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *map_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *ref_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    if (!grid || !map_out || !ref_out) { free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc"); }

    generate_unit_grid(grid);

    /* XYZ -> JzAzBz */
    alwan_xyz_to_jzazbz_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_xyz_f64 xyz; xyz.x = grid[i*3+0]; xyz.y = grid[i*3+1]; xyz.z = grid[i*3+2];
        alwan_jzazbz_f64 jz; alwan_xyz_to_jzazbz_f64(&jz, &xyz);
        ref_out[i*3+0] = jz.Jz; ref_out[i*3+1] = jz.az; ref_out[i*3+2] = jz.bz;
    }
    if (compare_arrays_tol(map_out, ref_out, MAP_COUNT, stride, "xyz_to_jzazbz_map_interleave", ALWAN_SIMD_TOLERANCE)) goto fail;

    /* JzAzBz -> XYZ */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_jzazbz_to_xyz_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_jzazbz_f64 jz; jz.Jz = grid[i*3+0]; jz.az = grid[i*3+1]; jz.bz = grid[i*3+2];
        alwan_xyz_f64 xyz; alwan_jzazbz_to_xyz_f64(&xyz, &jz);
        ref_out[i*3+0] = xyz.x; ref_out[i*3+1] = xyz.y; ref_out[i*3+2] = xyz.z;
    }
    if (compare_arrays_tol(map_out, ref_out, MAP_COUNT, stride, "jzazbz_to_xyz_map_interleave", ALWAN_SIMD_TOLERANCE)) goto fail;

    /* JzAzBz -> JzCzhz */
    generate_unit_grid(grid);
    alwan_xyz_to_jzazbz_f64_map_interleave(grid, grid, MAP_COUNT, stride, stride);
    alwan_jzazbz_to_jzczhz_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_jzazbz_f64 jz; jz.Jz = grid[i*3+0]; jz.az = grid[i*3+1]; jz.bz = grid[i*3+2];
        alwan_jzczhz_f64 jzch; alwan_jzazbz_to_jzczhz_f64(&jzch, &jz);
        ref_out[i*3+0] = jzch.Jz; ref_out[i*3+1] = jzch.Cz; ref_out[i*3+2] = jzch.hz;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "jzazbz_to_jzczhz_map_interleave")) goto fail;

    /* JzCzhz -> JzAzBz */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_jzczhz_to_jzazbz_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_jzczhz_f64 jzch; jzch.Jz = grid[i*3+0]; jzch.Cz = grid[i*3+1]; jzch.hz = grid[i*3+2];
        alwan_jzazbz_f64 jz; alwan_jzczhz_to_jzazbz_f64(&jz, &jzch);
        ref_out[i*3+0] = jz.Jz; ref_out[i*3+1] = jz.az; ref_out[i*3+2] = jz.bz;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "jzczhz_to_jzazbz_map_interleave")) goto fail;

    free(grid); free(map_out); free(ref_out);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(map_out); free(ref_out); return 1;
}

/* ----------------------------------------------------------------
 * Group E: IPT (pointer API)
 * ---------------------------------------------------------------- */

static int test_ipt_maps(void) {
    TEST_START("Map validation: IPT (XYZ<->IPT)");

    size_t const stride = 3 * sizeof(alwan_f64);
    alwan_f64 *grid = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *map_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *ref_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    if (!grid || !map_out || !ref_out) { free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc"); }

    generate_unit_grid(grid);

    /* XYZ -> IPT */
    alwan_xyz_to_ipt_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_xyz_f64 xyz; xyz.x = grid[i*3+0]; xyz.y = grid[i*3+1]; xyz.z = grid[i*3+2];
        alwan_ipt_f64 ipt; alwan_xyz_to_ipt_f64(&ipt, &xyz);
        ref_out[i*3+0] = ipt.I; ref_out[i*3+1] = ipt.P; ref_out[i*3+2] = ipt.T;
    }
    if (compare_arrays_tol(map_out, ref_out, MAP_COUNT, stride, "xyz_to_ipt_map_interleave", ALWAN_SIMD_TOLERANCE)) goto fail;

    /* IPT -> XYZ */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_ipt_to_xyz_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_ipt_f64 ipt; ipt.I = grid[i*3+0]; ipt.P = grid[i*3+1]; ipt.T = grid[i*3+2];
        alwan_xyz_f64 xyz; alwan_ipt_to_xyz_f64(&xyz, &ipt);
        ref_out[i*3+0] = xyz.x; ref_out[i*3+1] = xyz.y; ref_out[i*3+2] = xyz.z;
    }
    if (compare_arrays_tol(map_out, ref_out, MAP_COUNT, stride, "ipt_to_xyz_map_interleave", ALWAN_SIMD_TOLERANCE)) goto fail;

    free(grid); free(map_out); free(ref_out);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(map_out); free(ref_out); return 1;
}

/* ----------------------------------------------------------------
 * Group F & G: CAM forward/inverse
 * Use smaller grid for CAM (expensive per-pixel)
 * ---------------------------------------------------------------- */

#define CAM_STEP 20
#define CAM_COUNT_1D ((255 / CAM_STEP) + 1)  /* 13 */
#define CAM_COUNT (CAM_COUNT_1D * CAM_COUNT_1D * CAM_COUNT_1D)  /* 2197 */

static void generate_cam_grid(alwan_f64 *out) {
    size_t idx = 0;
    for (int r = 0; r <= 255; r += CAM_STEP) {
        for (int g = 0; g <= 255; g += CAM_STEP) {
            for (int b = 0; b <= 255; b += CAM_STEP) {
                out[idx * 3 + 0] = (alwan_f64)r / ALWAN_LITERAL(255.0) * ALWAN_LITERAL(100.0);
                out[idx * 3 + 1] = (alwan_f64)g / ALWAN_LITERAL(255.0) * ALWAN_LITERAL(100.0);
                out[idx * 3 + 2] = (alwan_f64)b / ALWAN_LITERAL(255.0) * ALWAN_LITERAL(100.0);
                idx++;
            }
        }
    }
}

static int test_ciecam02_maps(void) {
    TEST_START("Map validation: CIECAM02 forward/inverse");

    size_t const stride = 3 * sizeof(alwan_f64);
    alwan_f64 *grid = (alwan_f64 *)malloc(CAM_COUNT * stride);
    alwan_ciecam02_correlates *map_corr = (alwan_ciecam02_correlates *)malloc(CAM_COUNT * sizeof(alwan_ciecam02_correlates));
    alwan_ciecam02_correlates *ref_corr = (alwan_ciecam02_correlates *)malloc(CAM_COUNT * sizeof(alwan_ciecam02_correlates));
    alwan_f64 *map_xyz = (alwan_f64 *)malloc(CAM_COUNT * stride);
    alwan_f64 *ref_xyz = (alwan_f64 *)malloc(CAM_COUNT * stride);
    if (!grid || !map_corr || !ref_corr || !map_xyz || !ref_xyz) {
        free(grid); free(map_corr); free(ref_corr); free(map_xyz); free(ref_xyz);
        TEST_FAIL("malloc");
    }

    alwan_ciecam02_viewing_conditions vc;
    vc.white_xyz.x = g_d65_xyz_y1[0] * ALWAN_LITERAL(100.0);
    vc.white_xyz.y = g_d65_xyz_y1[1] * ALWAN_LITERAL(100.0);
    vc.white_xyz.z = g_d65_xyz_y1[2] * ALWAN_LITERAL(100.0);
    vc.adapting_luminance = ALWAN_LITERAL(64.0);
    vc.background_luminance = ALWAN_LITERAL(0.2);
    vc.surround = ALWAN_CIECAM02_SURROUND_AVERAGE;
    vc.discount_illuminant = 0;

    generate_cam_grid(grid);

    /* Forward map */
    alwan_ciecam02_forward_f64_map_interleave(map_corr, grid, &vc, CAM_COUNT, stride);
    for (size_t i = 0; i < CAM_COUNT; i++) {
        alwan_xyz_f64 xyz; xyz.x = grid[i*3+0]; xyz.y = grid[i*3+1]; xyz.z = grid[i*3+2];
        alwan_ciecam02_forward_f64(&ref_corr[i], &xyz, &vc);
    }
    for (size_t i = 0; i < CAM_COUNT; i++) {
        alwan_f64 dJ = ALWAN_ABS(map_corr[i].J - ref_corr[i].J);
        alwan_f64 dC = ALWAN_ABS(map_corr[i].C - ref_corr[i].C);
        alwan_f64 dh = ALWAN_ABS(map_corr[i].h - ref_corr[i].h);
        alwan_f64 max_d = dJ; if (dC > max_d) max_d = dC; if (dh > max_d) max_d = dh;
        if (max_d > ALWAN_SIMD_TOLERANCE) {
            printf("[FAIL] ciecam02_forward_map_interleave: pixel %zu: diff=%.16e\n", i, (double)max_d);
            goto fail;
        }
    }

    /* Inverse map */
    alwan_ciecam02_inverse_f64_map_interleave(map_xyz, ref_corr, &vc, CAM_COUNT, stride);
    for (size_t i = 0; i < CAM_COUNT; i++) {
        alwan_xyz_f64 xyz; alwan_ciecam02_inverse_f64(&xyz, &ref_corr[i], &vc);
        ref_xyz[i*3+0] = xyz.x; ref_xyz[i*3+1] = xyz.y; ref_xyz[i*3+2] = xyz.z;
    }
    if (compare_arrays_tol(map_xyz, ref_xyz, CAM_COUNT, stride, "ciecam02_inverse_map_interleave", ALWAN_SIMD_TOLERANCE)) goto fail;

    free(grid); free(map_corr); free(ref_corr); free(map_xyz); free(ref_xyz);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(map_corr); free(ref_corr); free(map_xyz); free(ref_xyz); return 1;
}

static int test_cam16_maps(void) {
    TEST_START("Map validation: CAM16 forward/inverse");

    size_t const stride = 3 * sizeof(alwan_f64);
    alwan_f64 *grid = (alwan_f64 *)malloc(CAM_COUNT * stride);
    alwan_cam16_correlates *map_corr = (alwan_cam16_correlates *)malloc(CAM_COUNT * sizeof(alwan_cam16_correlates));
    alwan_cam16_correlates *ref_corr = (alwan_cam16_correlates *)malloc(CAM_COUNT * sizeof(alwan_cam16_correlates));
    alwan_f64 *map_xyz = (alwan_f64 *)malloc(CAM_COUNT * stride);
    alwan_f64 *ref_xyz = (alwan_f64 *)malloc(CAM_COUNT * stride);
    if (!grid || !map_corr || !ref_corr || !map_xyz || !ref_xyz) {
        free(grid); free(map_corr); free(ref_corr); free(map_xyz); free(ref_xyz);
        TEST_FAIL("malloc");
    }

    alwan_cam16_viewing_conditions vc;
    vc.white_xyz.x = g_d65_xyz_y1[0] * ALWAN_LITERAL(100.0);
    vc.white_xyz.y = g_d65_xyz_y1[1] * ALWAN_LITERAL(100.0);
    vc.white_xyz.z = g_d65_xyz_y1[2] * ALWAN_LITERAL(100.0);
    vc.adapting_luminance = ALWAN_LITERAL(64.0);
    vc.background_luminance = ALWAN_LITERAL(0.2);
    vc.surround = ALWAN_CAM16_SURROUND_AVERAGE;
    vc.discount_illuminant = 0;

    generate_cam_grid(grid);

    /* Forward map */
    alwan_cam16_forward_f64_map_interleave(map_corr, grid, &vc, CAM_COUNT, stride);
    for (size_t i = 0; i < CAM_COUNT; i++) {
        alwan_xyz_f64 xyz; xyz.x = grid[i*3+0]; xyz.y = grid[i*3+1]; xyz.z = grid[i*3+2];
        alwan_cam16_forward_f64(&ref_corr[i], &xyz, &vc);
    }
    for (size_t i = 0; i < CAM_COUNT; i++) {
        alwan_f64 dJ = ALWAN_ABS(map_corr[i].J - ref_corr[i].J);
        alwan_f64 dC = ALWAN_ABS(map_corr[i].C - ref_corr[i].C);
        alwan_f64 dh = ALWAN_ABS(map_corr[i].h - ref_corr[i].h);
        alwan_f64 max_d = dJ; if (dC > max_d) max_d = dC; if (dh > max_d) max_d = dh;
        if (max_d > ALWAN_SIMD_TOLERANCE) {
            printf("[FAIL] cam16_forward_map_interleave: pixel %zu: diff=%.16e\n", i, (double)max_d);
            goto fail;
        }
    }

    /* Inverse map */
    alwan_cam16_inverse_f64_map_interleave(map_xyz, ref_corr, &vc, CAM_COUNT, stride);
    for (size_t i = 0; i < CAM_COUNT; i++) {
        alwan_xyz_f64 xyz; alwan_cam16_inverse_f64(&xyz, &ref_corr[i], &vc);
        ref_xyz[i*3+0] = xyz.x; ref_xyz[i*3+1] = xyz.y; ref_xyz[i*3+2] = xyz.z;
    }
    if (compare_arrays_tol(map_xyz, ref_xyz, CAM_COUNT, stride, "cam16_inverse_map_interleave", ALWAN_SIMD_TOLERANCE)) goto fail;

    free(grid); free(map_corr); free(ref_corr); free(map_xyz); free(ref_xyz);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(map_corr); free(ref_corr); free(map_xyz); free(ref_xyz); return 1;
}

/* ----------------------------------------------------------------
 * Group H: _map_interleave_ex typed format validation
 *
 * Tests that _map_interleave_ex functions (typed pixel I/O) produce results
 * matching the scalar _map_interleave functions across all pixel formats.
 * For integer formats, the reference uses the same quantized input
 * to isolate output quantization error only.
 * ---------------------------------------------------------------- */

#define MAPEX_STEP 15
#define MAPEX_COUNT_1D ((255 / MAPEX_STEP) + 1)  /* 18 */
#define MAPEX_COUNT (MAPEX_COUNT_1D * MAPEX_COUNT_1D * MAPEX_COUNT_1D)  /* 5832 */

static size_t mapex_typed_stride(alwan_pixel_format fmt) {
    return 3 * test_fmt_elem_size(fmt);
}

static alwan_f64 mapex_tol(alwan_pixel_format fmt) {
    /* Output quantization adds up to 0.5 LSB of rounding error. */
    switch (fmt) {
    case ALWAN_PIXEL_U8:  return ALWAN_LITERAL(0.5) / ALWAN_LITERAL(255.0) + ALWAN_LITERAL(1e-4);
    case ALWAN_PIXEL_U16: return ALWAN_LITERAL(0.5) / ALWAN_LITERAL(65535.0) + ALWAN_LITERAL(3e-5);
    case ALWAN_PIXEL_F32: return ALWAN_LITERAL(1e-2);
    default:              return ALWAN_SIMD_TOLERANCE;
    }
}

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

/* Compare _map_interleave_ex output vs _map_interleave reference.
 * For integer formats, skip pixels where reference is outside [0,1]
 * because clamping makes comparison meaningless.
 * For F32, use relative tolerance since output magnitudes vary widely
 * (e.g. PQ inverse can produce XYZ > 10000). */
static int compare_mapex(alwan_f64 const *ex_out, alwan_f64 const *ref_out,
                          size_t count, size_t stride, char const *name,
                          alwan_f64 tol, alwan_pixel_format fmt) {
    int is_int = (fmt == ALWAN_PIXEL_U8 || fmt == ALWAN_PIXEL_U16);
    int is_f32 = (fmt == ALWAN_PIXEL_F32);
    alwan_f64 max_excess = ALWAN_LITERAL(0.0);
    alwan_f64 max_diff   = ALWAN_LITERAL(0.0);
    alwan_f64 max_tol_at = tol;
    size_t max_i = 0; int max_c = 0;
    for (size_t i = 0; i < count; i++) {
        alwan_f64 const *e = (alwan_f64 const *)((char const *)ex_out + i * stride);
        alwan_f64 const *r = (alwan_f64 const *)((char const *)ref_out + i * stride);
        /* For integer formats, skip the whole pixel if any channel is out of [0,1]
         * (clamped output makes comparison with un-clamped reference meaningless). */
        if (is_int) {
            int skip = 0;
            for (int c = 0; c < 3; c++)
                if (r[c] < ALWAN_LITERAL(0.0) || r[c] > ALWAN_LITERAL(1.0)) skip = 1;
            if (skip) continue;
        }
        for (int c = 0; c < 3; c++) {
            alwan_f64 diff = ALWAN_ABS(e[c] - r[c]);
            alwan_f64 actual_tol = tol;
            if (is_f32) {
                /* Scale tolerance by output magnitude for proper relative comparison.
                 * Functions with large outputs (e.g. jzazbz_to_xyz absolute XYZ)
                 * need relative rather than absolute tolerance. */
                alwan_f64 mag = ALWAN_ABS(r[c]);
                alwan_f64 scale = mag > ALWAN_LITERAL(1.0) ? mag : ALWAN_LITERAL(1.0);
                actual_tol = tol * scale;
            }
            alwan_f64 excess = diff - actual_tol;
            if (excess > max_excess) {
                max_excess = excess; max_diff = diff; max_tol_at = actual_tol;
                max_i = i; max_c = c;
            }
        }
    }
    if (max_excess > ALWAN_LITERAL(0.0)) {
        alwan_f64 const *e = (alwan_f64 const *)((char const *)ex_out + max_i * stride);
        alwan_f64 const *r = (alwan_f64 const *)((char const *)ref_out + max_i * stride);
        printf("[FAIL] %s [%s]: pixel %zu ch %d: ex=%.16e ref=%.16e diff=%.16e (tol=%.16e)\n",
               name, test_fmt_name(fmt), max_i, max_c,
               (double)e[max_c], (double)r[max_c], (double)max_diff, (double)max_tol_at);
        return 1;
    }
    return 0;
}

/* ---- Table-driven runner for simple 3->3 pattern ---- */

typedef int (*mapex_fn3)(alwan_f64 *, alwan_f64 const *, size_t, size_t, size_t);
typedef int (*mapex_fn3_ex)(void *, alwan_pixel_format, void const *, alwan_pixel_format,
                             size_t, size_t, size_t);
typedef alwan_f64 (*mapex_tol_fn)(alwan_pixel_format);

typedef struct { char const *name; mapex_fn3 map; mapex_fn3_ex map_ex; mapex_tol_fn tol; } mapex_entry3;

/* jzazbz_to_xyz with out-of-gamut [0,1]^3 inputs: PQ inverse EOTF amplifies
 * f32/f64 precision differences to ~18% relative for extreme az/bz values. */
static alwan_f64 jzazbz_inv_tol(alwan_pixel_format fmt) {
    if (fmt == ALWAN_PIXEL_F32) return ALWAN_LITERAL(0.25);
    return mapex_tol(fmt);
}

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
            alwan_f64 tol = entries[e].tol ? entries[e].tol(fmt) : mapex_tol(fmt);

            alwan_scatter3(tin, fmt, grid, MAPEX_COUNT, ss, ts);

            /* For lossy formats, run reference on quantized input
             * to isolate output quantization error only */
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

/* ---- H1: Simple 3->3 maps ---- */

static int test_ex_simple_maps(void) {
    TEST_START("_map_interleave_ex: Simple 3->3 (OkLab, Lab/LCh, xyY, JzAzBz, IPT)");
    alwan_f64 *grid = (alwan_f64 *)malloc(MAPEX_COUNT * 3 * sizeof(alwan_f64));
    if (!grid) { TEST_FAIL("malloc"); }
    generate_mapex_grid(grid);

    static const mapex_entry3 entries[] = {
        {"xyz_to_oklab",      alwan_xyz_to_oklab_map_interleave,      alwan_xyz_to_oklab_map_interleave_ex},
        {"oklab_to_xyz",      alwan_oklab_to_xyz_map_interleave,      alwan_oklab_to_xyz_map_interleave_ex},
        {"oklab_to_oklch",    alwan_oklab_to_oklch_map_interleave,    alwan_oklab_to_oklch_map_interleave_ex},
        {"oklch_to_oklab",    alwan_oklch_to_oklab_map_interleave,    alwan_oklch_to_oklab_map_interleave_ex},
        {"lab_to_lch",        alwan_lab_to_lch_map_interleave,        alwan_lab_to_lch_map_interleave_ex},
        {"lch_to_lab",        alwan_lch_to_lab_map_interleave,        alwan_lch_to_lab_map_interleave_ex},
        {"luv_to_lchuv",      alwan_luv_to_lchuv_map_interleave,      alwan_luv_to_lchuv_map_interleave_ex},
        {"lchuv_to_luv",      alwan_lchuv_to_luv_map_interleave,      alwan_lchuv_to_luv_map_interleave_ex},
        {"xyz_to_xyy",        alwan_xyz_to_xyy_map_interleave,        alwan_xyz_to_xyy_map_interleave_ex},
        {"xyy_to_xyz",        alwan_xyy_to_xyz_map_interleave,        alwan_xyy_to_xyz_map_interleave_ex},
        {"xyz_to_jzazbz",    alwan_xyz_to_jzazbz_map_interleave,    alwan_xyz_to_jzazbz_map_interleave_ex,    NULL},
        {"jzazbz_to_xyz",    alwan_jzazbz_to_xyz_map_interleave,    alwan_jzazbz_to_xyz_map_interleave_ex,    jzazbz_inv_tol},
        {"jzazbz_to_jzczhz", alwan_jzazbz_to_jzczhz_map_interleave, alwan_jzazbz_to_jzczhz_map_interleave_ex, NULL},
        {"jzczhz_to_jzazbz", alwan_jzczhz_to_jzazbz_map_interleave, alwan_jzczhz_to_jzazbz_map_interleave_ex, NULL},
        {"xyz_to_ipt",        alwan_xyz_to_ipt_map_interleave,        alwan_xyz_to_ipt_map_interleave_ex},
        {"ipt_to_xyz",        alwan_ipt_to_xyz_map_interleave,        alwan_ipt_to_xyz_map_interleave_ex},
    };

    int r = run_mapex3(entries, sizeof(entries) / sizeof(entries[0]), grid);
    free(grid);
    if (r) return 1;
    TEST_PASS_MSG(); return 0;
}

/* ---- H2: sRGB convenience maps ---- */

static int test_ex_srgb_maps(void) {
    TEST_START("_map_interleave_ex: sRGB convenience (srgb<->xyz, srgb<->lab, srgb<->oklab)");
    alwan_f64 *grid = (alwan_f64 *)malloc(MAPEX_COUNT * 3 * sizeof(alwan_f64));
    if (!grid) { TEST_FAIL("malloc"); }
    generate_mapex_grid(grid);

    static const mapex_entry3 entries[] = {
        {"srgb_to_xyz",   alwan_srgb_to_xyz_map_interleave,   alwan_srgb_to_xyz_map_interleave_ex},
        {"xyz_to_srgb",   alwan_xyz_to_srgb_map_interleave,   alwan_xyz_to_srgb_map_interleave_ex},
        {"srgb_to_lab",   alwan_srgb_to_lab_map_interleave,   alwan_srgb_to_lab_map_interleave_ex},
        {"lab_to_srgb",   alwan_lab_to_srgb_map_interleave,   alwan_lab_to_srgb_map_interleave_ex},
        {"srgb_to_oklab", alwan_srgb_to_oklab_map_interleave, alwan_srgb_to_oklab_map_interleave_ex},
        {"oklab_to_srgb", alwan_oklab_to_srgb_map_interleave, alwan_oklab_to_srgb_map_interleave_ex},
    };

    int r = run_mapex3(entries, sizeof(entries) / sizeof(entries[0]), grid);
    free(grid);
    if (r) return 1;
    TEST_PASS_MSG(); return 0;
}

/* ---- H3: HSV/HSL maps ---- */

static int test_ex_hsv_hsl_maps(void) {
    TEST_START("_map_interleave_ex: HSV/HSL (rgb<->hsv, rgb<->hsl)");
    alwan_f64 *grid = (alwan_f64 *)malloc(MAPEX_COUNT * 3 * sizeof(alwan_f64));
    if (!grid) { TEST_FAIL("malloc"); }
    generate_mapex_grid(grid);

    static const mapex_entry3 entries[] = {
        {"rgb_to_hsv", alwan_rgb_to_hsv_map_interleave, alwan_rgb_to_hsv_map_interleave_ex},
        {"hsv_to_rgb", alwan_hsv_to_rgb_map_interleave, alwan_hsv_to_rgb_map_interleave_ex},
        {"rgb_to_hsl", alwan_rgb_to_hsl_map_interleave, alwan_rgb_to_hsl_map_interleave_ex},
        {"hsl_to_rgb", alwan_hsl_to_rgb_map_interleave, alwan_hsl_to_rgb_map_interleave_ex},
    };

    int r = run_mapex3(entries, sizeof(entries) / sizeof(entries[0]), grid);
    free(grid);
    if (r) return 1;
    TEST_PASS_MSG(); return 0;
}

/* ---- H4: White point maps ---- */

static int test_ex_white_point_maps(void) {
    TEST_START("_map_interleave_ex: White point (xyz<->lab, xyz<->luv with D65)");

    size_t const ss = 3 * sizeof(alwan_f64);
    size_t const mt = 3 * sizeof(double);
    alwan_f64 *grid  = (alwan_f64 *)malloc(MAPEX_COUNT * ss);
    alwan_f64 *ref   = (alwan_f64 *)malloc(MAPEX_COUNT * ss);
    alwan_f64 *ex    = (alwan_f64 *)malloc(MAPEX_COUNT * ss);
    alwan_f64 *qgrid = (alwan_f64 *)malloc(MAPEX_COUNT * ss);
    void *tin  = malloc(MAPEX_COUNT * mt);
    void *tout = malloc(MAPEX_COUNT * mt);
    if (!grid || !ref || !ex || !qgrid || !tin || !tout) {
        free(grid); free(ref); free(ex); free(qgrid); free(tin); free(tout);
        TEST_FAIL("malloc");
    }

    generate_mapex_grid(grid);
    alwan_xyz_f64 d65; d65.x = g_d65_xyz_y1[0]; d65.y = g_d65_xyz_y1[1]; d65.z = g_d65_xyz_y1[2];

    typedef int (*fn_w)(alwan_f64 *, alwan_f64 const *, alwan_xyz_f64 const *, size_t, size_t, size_t);
    typedef int (*fn_w_ex)(void *, alwan_pixel_format, void const *, alwan_pixel_format,
                            alwan_xyz_f64 const *, size_t, size_t, size_t);

    struct { char const *name; fn_w map; fn_w_ex map_ex; } entries[] = {
        {"xyz_to_lab", alwan_xyz_to_lab_map_interleave, alwan_xyz_to_lab_map_interleave_ex},
        {"lab_to_xyz", alwan_lab_to_xyz_map_interleave, alwan_lab_to_xyz_map_interleave_ex},
        {"xyz_to_luv", alwan_xyz_to_luv_map_interleave, alwan_xyz_to_luv_map_interleave_ex},
        {"luv_to_xyz", alwan_luv_to_xyz_map_interleave, alwan_luv_to_xyz_map_interleave_ex},
    };

    for (int e = 0; e < 4; e++) {
        for (int f = 0; f < 4; f++) {
            alwan_pixel_format fmt = TEST_PIXEL_FMTS[f];
            size_t ts = mapex_typed_stride(fmt);
            alwan_f64 tol = mapex_tol(fmt);

            alwan_scatter3(tin, fmt, grid, MAPEX_COUNT, ss, ts);

            if (fmt != ALWAN_PIXEL_F64) {
                alwan_collect3(qgrid, tin, fmt, MAPEX_COUNT, ts, ss);
                entries[e].map(ref, qgrid, &d65, MAPEX_COUNT, ss, ss);
            } else {
                entries[e].map(ref, grid, &d65, MAPEX_COUNT, ss, ss);
            }

            entries[e].map_ex(tout, fmt, tin, fmt, &d65, MAPEX_COUNT, ts, ts);
            alwan_collect3(ex, tout, fmt, MAPEX_COUNT, ts, ss);
            if (compare_mapex(ex, ref, MAPEX_COUNT, ss, entries[e].name, tol, fmt))
                goto fail;
        }
    }

    free(grid); free(ref); free(ex); free(qgrid); free(tin); free(tout);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(ref); free(ex); free(qgrid); free(tin); free(tout);
    return 1;
}

/* ---- H5: ICtCp maps (PQ + HLG) ---- */

static int test_ex_ictcp_maps(void) {
    TEST_START("_map_interleave_ex: ICtCp (rgb/xyz<->ictcp, PQ+HLG)");

    size_t const ss = 3 * sizeof(alwan_f64);
    size_t const mt = 3 * sizeof(double);
    alwan_f64 *grid  = (alwan_f64 *)malloc(MAPEX_COUNT * ss);
    alwan_f64 *ref   = (alwan_f64 *)malloc(MAPEX_COUNT * ss);
    alwan_f64 *ex    = (alwan_f64 *)malloc(MAPEX_COUNT * ss);
    alwan_f64 *qgrid = (alwan_f64 *)malloc(MAPEX_COUNT * ss);
    void *tin  = malloc(MAPEX_COUNT * mt);
    void *tout = malloc(MAPEX_COUNT * mt);
    if (!grid || !ref || !ex || !qgrid || !tin || !tout) {
        free(grid); free(ref); free(ex); free(qgrid); free(tin); free(tout);
        TEST_FAIL("malloc");
    }

    generate_mapex_grid(grid);

    typedef int (*fn_pq)(alwan_f64 *, alwan_f64 const *, int, size_t, size_t, size_t);
    typedef int (*fn_pq_ex)(void *, alwan_pixel_format, void const *, alwan_pixel_format,
                             int, size_t, size_t, size_t);

    /* f32_tol: override for F32 format (0 = use default mapex_tol).
     * Inverse PQ functions (ictcp_to_rgb/xyz) use PQ EOTF — same as jzazbz_to_xyz —
     * and diverge ~15% f32/f64 for out-of-gamut [0,1]^3 inputs. */
    struct { char const *name; fn_pq map; fn_pq_ex map_ex; alwan_f64 f32_tol; } entries[] = {
        {"rgb_to_ictcp",  alwan_rgb_to_ictcp_map_interleave,  alwan_rgb_to_ictcp_map_interleave_ex,  ALWAN_LITERAL(0.0)},
        {"ictcp_to_rgb",  alwan_ictcp_to_rgb_map_interleave,  alwan_ictcp_to_rgb_map_interleave_ex,  ALWAN_LITERAL(0.25)},
        {"xyz_to_ictcp",  alwan_xyz_to_ictcp_map_interleave,  alwan_xyz_to_ictcp_map_interleave_ex,  ALWAN_LITERAL(0.0)},
        {"ictcp_to_xyz",  alwan_ictcp_to_xyz_map_interleave,  alwan_ictcp_to_xyz_map_interleave_ex,  ALWAN_LITERAL(0.25)},
    };

    for (int pq = 0; pq <= 1; pq++) {
        for (int e = 0; e < 4; e++) {
            for (int f = 0; f < 4; f++) {
                alwan_pixel_format fmt = TEST_PIXEL_FMTS[f];
                size_t ts = mapex_typed_stride(fmt);
                alwan_f64 tol = mapex_tol(fmt);
                if (fmt == ALWAN_PIXEL_F32 && entries[e].f32_tol > ALWAN_LITERAL(0.0))
                    tol = entries[e].f32_tol;

                alwan_scatter3(tin, fmt, grid, MAPEX_COUNT, ss, ts);

                if (fmt != ALWAN_PIXEL_F64) {
                    alwan_collect3(qgrid, tin, fmt, MAPEX_COUNT, ts, ss);
                    entries[e].map(ref, qgrid, pq, MAPEX_COUNT, ss, ss);
                } else {
                    entries[e].map(ref, grid, pq, MAPEX_COUNT, ss, ss);
                }

                entries[e].map_ex(tout, fmt, tin, fmt, pq, MAPEX_COUNT, ts, ts);
                alwan_collect3(ex, tout, fmt, MAPEX_COUNT, ts, ss);

                char label[64];
                snprintf(label, sizeof(label), "%s %s", entries[e].name, pq ? "PQ" : "HLG");
                if (compare_mapex(ex, ref, MAPEX_COUNT, ss, label, tol, fmt))
                    goto fail;
            }
        }
    }

    free(grid); free(ref); free(ex); free(qgrid); free(tin); free(tout);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(ref); free(ex); free(qgrid); free(tin); free(tout);
    return 1;
}

/* ---- H6: CAM maps ---- */

static int test_ex_cam_maps(void) {
    TEST_START("_map_interleave_ex: CAM (CIECAM02/CAM16 forward/inverse)");

    size_t const ss = 3 * sizeof(alwan_f64);
    size_t const mt = 3 * sizeof(double);
    alwan_f64 *grid     = (alwan_f64 *)malloc(MAPEX_COUNT * ss);
    alwan_f64 *ref_xyz  = (alwan_f64 *)malloc(MAPEX_COUNT * ss);
    alwan_f64 *ex_xyz   = (alwan_f64 *)malloc(MAPEX_COUNT * ss);
    alwan_f64 *qgrid    = (alwan_f64 *)malloc(MAPEX_COUNT * ss);
    void *typed_xyz        = malloc(MAPEX_COUNT * mt);
    alwan_ciecam02_correlates *c02_ref = (alwan_ciecam02_correlates *)malloc(MAPEX_COUNT * sizeof(*c02_ref));
    alwan_ciecam02_correlates *c02_ex  = (alwan_ciecam02_correlates *)malloc(MAPEX_COUNT * sizeof(*c02_ex));
    alwan_cam16_correlates    *c16_ref = (alwan_cam16_correlates *)malloc(MAPEX_COUNT * sizeof(*c16_ref));
    alwan_cam16_correlates    *c16_ex  = (alwan_cam16_correlates *)malloc(MAPEX_COUNT * sizeof(*c16_ex));
    if (!grid || !ref_xyz || !ex_xyz || !qgrid || !typed_xyz ||
        !c02_ref || !c02_ex || !c16_ref || !c16_ex) {
        free(grid); free(ref_xyz); free(ex_xyz); free(qgrid); free(typed_xyz);
        free(c02_ref); free(c02_ex); free(c16_ref); free(c16_ex);
        TEST_FAIL("malloc");
    }

    generate_mapex_grid(grid);

    alwan_ciecam02_viewing_conditions vc02;
    vc02.white_xyz.x = ALWAN_LITERAL(0.95047);
    vc02.white_xyz.y = ALWAN_LITERAL(1.0);
    vc02.white_xyz.z = ALWAN_LITERAL(1.08883);
    vc02.adapting_luminance = ALWAN_LITERAL(64.0);
    vc02.background_luminance = ALWAN_LITERAL(0.2);
    vc02.surround = ALWAN_CIECAM02_SURROUND_AVERAGE;
    vc02.discount_illuminant = 0;

    alwan_cam16_viewing_conditions vc16;
    vc16.white_xyz.x = ALWAN_LITERAL(0.95047);
    vc16.white_xyz.y = ALWAN_LITERAL(1.0);
    vc16.white_xyz.z = ALWAN_LITERAL(1.08883);
    vc16.adapting_luminance = ALWAN_LITERAL(64.0);
    vc16.background_luminance = ALWAN_LITERAL(0.2);
    vc16.surround = ALWAN_CAM16_SURROUND_AVERAGE;
    vc16.discount_illuminant = 0;

    for (int f = 0; f < 4; f++) {
        alwan_pixel_format fmt = TEST_PIXEL_FMTS[f];
        size_t ts = mapex_typed_stride(fmt);
        alwan_f64 tol = mapex_tol(fmt);

        alwan_f64 cam_fwd_tol = ALWAN_SIMD_TOLERANCE;

        alwan_scatter3(typed_xyz, fmt, grid, MAPEX_COUNT, ss, ts);

        /* --- CIECAM02 --- */

        /* Forward reference (on quantized input for lossy formats) */
        if (fmt != ALWAN_PIXEL_F64) {
            alwan_collect3(qgrid, typed_xyz, fmt, MAPEX_COUNT, ts, ss);
            alwan_ciecam02_forward_f64_map_interleave(c02_ref, qgrid, &vc02, MAPEX_COUNT, ss);
        } else {
            alwan_ciecam02_forward_f64_map_interleave(c02_ref, grid, &vc02, MAPEX_COUNT, ss);
        }

        /* Forward _map_interleave_ex */
        alwan_ciecam02_forward_map_interleave_ex(c02_ex, typed_xyz, fmt, &vc02, MAPEX_COUNT, ts);
        for (size_t i = 0; i < MAPEX_COUNT; i++) {
            alwan_f64 dJ = ALWAN_ABS(c02_ex[i].J - c02_ref[i].J);
            alwan_f64 dC = ALWAN_ABS(c02_ex[i].C - c02_ref[i].C);
            alwan_f64 dh = ALWAN_ABS(c02_ex[i].h - c02_ref[i].h);
            alwan_f64 max_d = dJ; if (dC > max_d) max_d = dC; if (dh > max_d) max_d = dh;
            if (max_d > cam_fwd_tol) {
                printf("[FAIL] ciecam02_forward_map_interleave_ex [%s]: pixel %zu: diff=%.6e (tol=%.6e)\n",
                       test_fmt_name(fmt), i, (double)max_d, (double)cam_fwd_tol);
                goto fail;
            }
        }

        /* Inverse reference */
        alwan_ciecam02_inverse_f64_map_interleave(ref_xyz, c02_ref, &vc02, MAPEX_COUNT, ss);

        /* Inverse _map_interleave_ex (correlates in, typed XYZ out) */
        alwan_ciecam02_inverse_map_interleave_ex(typed_xyz, fmt, c02_ref, &vc02, MAPEX_COUNT, ts);
        alwan_collect3(ex_xyz, typed_xyz, fmt, MAPEX_COUNT, ts, ss);
        if (compare_mapex(ex_xyz, ref_xyz, MAPEX_COUNT, ss,
                          "ciecam02_inverse", tol, fmt)) goto fail;

        /* --- CAM16 --- */

        alwan_scatter3(typed_xyz, fmt, grid, MAPEX_COUNT, ss, ts);

        if (fmt != ALWAN_PIXEL_F64) {
            alwan_cam16_forward_f64_map_interleave(c16_ref, qgrid, &vc16, MAPEX_COUNT, ss);
        } else {
            alwan_cam16_forward_f64_map_interleave(c16_ref, grid, &vc16, MAPEX_COUNT, ss);
        }

        alwan_cam16_forward_map_interleave_ex(c16_ex, typed_xyz, fmt, &vc16, MAPEX_COUNT, ts);
        for (size_t i = 0; i < MAPEX_COUNT; i++) {
            alwan_f64 dJ = ALWAN_ABS(c16_ex[i].J - c16_ref[i].J);
            alwan_f64 dC = ALWAN_ABS(c16_ex[i].C - c16_ref[i].C);
            alwan_f64 dh = ALWAN_ABS(c16_ex[i].h - c16_ref[i].h);
            alwan_f64 max_d = dJ; if (dC > max_d) max_d = dC; if (dh > max_d) max_d = dh;
            if (max_d > cam_fwd_tol) {
                printf("[FAIL] cam16_forward_map_interleave_ex [%s]: pixel %zu: diff=%.6e (tol=%.6e)\n",
                       test_fmt_name(fmt), i, (double)max_d, (double)cam_fwd_tol);
                goto fail;
            }
        }

        alwan_cam16_inverse_f64_map_interleave(ref_xyz, c16_ref, &vc16, MAPEX_COUNT, ss);

        alwan_cam16_inverse_map_interleave_ex(typed_xyz, fmt, c16_ref, &vc16, MAPEX_COUNT, ts);
        alwan_collect3(ex_xyz, typed_xyz, fmt, MAPEX_COUNT, ts, ss);
        if (compare_mapex(ex_xyz, ref_xyz, MAPEX_COUNT, ss,
                          "cam16_inverse", tol, fmt)) goto fail;
    }

    free(grid); free(ref_xyz); free(ex_xyz); free(qgrid); free(typed_xyz);
    free(c02_ref); free(c02_ex); free(c16_ref); free(c16_ex);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(ref_xyz); free(ex_xyz); free(qgrid); free(typed_xyz);
    free(c02_ref); free(c02_ex); free(c16_ref); free(c16_ex);
    return 1;
}

/* ----------------------------------------------------------------
 * Gamut map SIMD validation
 *
 * Gamut map functions (clip, hue-preserving, CSS) are not covered by
 * the fixture tests at large batch sizes.  Run them on the full 140K
 * grid and compare the batch output against a per-pixel reference to
 * ensure the SIMD path produces identical results.
 * ---------------------------------------------------------------- */

static int test_gamut_map_simd(void) {
    TEST_START("Map validation: gamut clip + CSS (batch vs per-pixel)");

    size_t const stride = 3 * sizeof(alwan_f64);
    alwan_f64 *grid    = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *map_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    alwan_f64 *ref_out = (alwan_f64 *)malloc(MAP_COUNT * stride);
    if (!grid || !map_out || !ref_out) { free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc"); }

    /* Generate grid with some out-of-gamut values [-0.5, 1.5] */
    {
        size_t idx = 0;
        for (int r = 0; r <= 255; r += MAP_STEP)
            for (int g = 0; g <= 255; g += MAP_STEP)
                for (int b = 0; b <= 255; b += MAP_STEP, idx++) {
                    grid[idx*3+0] = (alwan_f64)r / ALWAN_LITERAL(255.0) * ALWAN_LITERAL(2.0) - ALWAN_LITERAL(0.5);
                    grid[idx*3+1] = (alwan_f64)g / ALWAN_LITERAL(255.0) * ALWAN_LITERAL(2.0) - ALWAN_LITERAL(0.5);
                    grid[idx*3+2] = (alwan_f64)b / ALWAN_LITERAL(255.0) * ALWAN_LITERAL(2.0) - ALWAN_LITERAL(0.5);
                }
    }

    /* Gamut clip: batch vs per-pixel */
    alwan_gamut_f64_map_interleave(map_out, ALWAN_GAMUT_MAP_CLIP, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_gamut_f64_map_interleave(&ref_out[i*3], ALWAN_GAMUT_MAP_CLIP, &grid[i*3], 1, stride, stride);
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "gamut_map_clip")) goto fail;

    /* CSS gamut map: batch vs per-pixel (uses unit-range input) */
    {
        size_t idx = 0;
        for (int r = 0; r <= 255; r += MAP_STEP)
            for (int g = 0; g <= 255; g += MAP_STEP)
                for (int b = 0; b <= 255; b += MAP_STEP, idx++) {
                    grid[idx*3+0] = (alwan_f64)r / ALWAN_LITERAL(255.0);
                    grid[idx*3+1] = (alwan_f64)g / ALWAN_LITERAL(255.0);
                    grid[idx*3+2] = (alwan_f64)b / ALWAN_LITERAL(255.0);
                }
    }

    alwan_css_gamut_f64_map_interleave(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_css_gamut_f64_map_interleave(&ref_out[i*3], &grid[i*3], 1, stride, stride);
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "css_gamut_map")) goto fail;

    free(grid); free(map_out); free(ref_out);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(map_out); free(ref_out); return 1;
}

/* ----------------------------------------------------------------
 * Main Test Runner
 * ---------------------------------------------------------------- */

int test_69_map_validation_main(void) {
    printf("========================================\n");
    printf("Test Suite 69: Map Validation\n");
    printf("========================================\n\n");

    test_count = 0;
    test_passed = 0;

    /* Group A: Simple 3->3 */
    printf("Group A: Simple 3->3 maps\n");
    printf("--------------------------------\n");
    if (test_oklab_maps()) return 1;
    if (test_lab_lch_maps()) return 1;
    if (test_xyy_maps()) return 1;
    if (test_hsv_hsl_maps()) return 1;

    /* sRGB convenience */
    printf("\nsRGB Convenience maps\n");
    printf("--------------------------------\n");
    if (test_srgb_convenience_maps()) return 1;

    /* Group B: White point */
    printf("\nGroup B: White point maps\n");
    printf("--------------------------------\n");
    if (test_white_point_maps()) return 1;

    /* Group C: ICtCp */
    printf("\nGroup C: ICtCp maps\n");
    printf("--------------------------------\n");
    if (test_ictcp_maps()) return 1;

    /* Group D: JzAzBz */
    printf("\nGroup D: JzAzBz maps\n");
    printf("--------------------------------\n");
    if (test_jzazbz_maps()) return 1;

    /* Group E: IPT */
    printf("\nGroup E: IPT maps\n");
    printf("--------------------------------\n");
    if (test_ipt_maps()) return 1;

    /* Group F & G: CAM models */
    printf("\nGroup F & G: CAM forward/inverse maps\n");
    printf("--------------------------------\n");
    if (test_ciecam02_maps()) return 1;
    if (test_cam16_maps()) return 1;

    /* Group H: _map_interleave_ex typed format validation */
    printf("\nGroup H: _map_interleave_ex typed format validation\n");
    printf("--------------------------------\n");
    if (test_ex_simple_maps()) return 1;
    if (test_ex_srgb_maps()) return 1;
    if (test_ex_hsv_hsl_maps()) return 1;
    if (test_ex_white_point_maps()) return 1;
    if (test_ex_ictcp_maps()) return 1;
    if (test_ex_cam_maps()) return 1;

    /* Group I: Gamut maps (SIMD batch vs per-pixel) */
    printf("\nGroup I: Gamut map SIMD validation\n");
    printf("--------------------------------\n");
    if (test_gamut_map_simd()) return 1;

    printf("\n========================================\n");
    printf("Test Results: %d/%d passed\n", test_passed, test_count);
    printf("========================================\n");

    return (test_passed == test_count) ? 0 : 1;
}
