/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test suite 69: Map Validation
 * Validates that _map (batch/SIMD) functions produce identical results
 * to their single-pixel counterparts within TEST_TOLERANCE.
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

static void generate_unit_grid(alwan_scalar *out) {
    size_t idx = 0;
    for (int r = 0; r <= 255; r += MAP_STEP) {
        for (int g = 0; g <= 255; g += MAP_STEP) {
            for (int b = 0; b <= 255; b += MAP_STEP) {
                out[idx * 3 + 0] = (alwan_scalar)r / ALWAN_LITERAL(255.0);
                out[idx * 3 + 1] = (alwan_scalar)g / ALWAN_LITERAL(255.0);
                out[idx * 3 + 2] = (alwan_scalar)b / ALWAN_LITERAL(255.0);
                idx++;
            }
        }
    }
}

static void generate_lab_grid(alwan_scalar *out) {
    size_t idx = 0;
    for (int r = 0; r <= 255; r += MAP_STEP) {
        for (int g = 0; g <= 255; g += MAP_STEP) {
            for (int b = 0; b <= 255; b += MAP_STEP) {
                out[idx * 3 + 0] = (alwan_scalar)r / ALWAN_LITERAL(255.0) * ALWAN_LITERAL(100.0);
                out[idx * 3 + 1] = ((alwan_scalar)g / ALWAN_LITERAL(255.0) - ALWAN_LITERAL(0.5)) * ALWAN_LITERAL(256.0);
                out[idx * 3 + 2] = ((alwan_scalar)b / ALWAN_LITERAL(255.0) - ALWAN_LITERAL(0.5)) * ALWAN_LITERAL(256.0);
                idx++;
            }
        }
    }
}

/* ----------------------------------------------------------------
 * Helper: compare map output vs per-pixel reference
 * ---------------------------------------------------------------- */

/*
 * SIMD map functions use fmadd (fused multiply-add) in matrix multiplies,
 * which has different rounding than the scalar a*b+c. When followed by
 * transcendental functions (pow, exp, log), these tiny differences get
 * amplified to ~1e-9 in f64. MAP_TOLERANCE accounts for this.
 */
#ifdef ALWAN_SCALAR_IS_FLOAT
#define MAP_TOLERANCE ALWAN_LITERAL(1e-4)
#else
#define MAP_TOLERANCE ALWAN_LITERAL(1e-8)
#endif

static int compare_arrays_tol(alwan_scalar const *map_out, alwan_scalar const *ref,
                              size_t count, size_t stride, char const *name,
                              alwan_scalar tol) {
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *m = (alwan_scalar const *)((char const *)map_out + i * stride);
        alwan_scalar const *r = (alwan_scalar const *)((char const *)ref + i * stride);
        for (int c = 0; c < 3; c++) {
            alwan_scalar diff = ALWAN_ABS(m[c] - r[c]);
            if (diff > tol) {
                printf("[FAIL] %s: pixel %zu ch %d: map=%.16e ref=%.16e diff=%.16e\n",
                       name, i, c, (double)m[c], (double)r[c], (double)diff);
                return 1;
            }
        }
    }
    return 0;
}

static int compare_arrays(alwan_scalar const *map_out, alwan_scalar const *ref,
                          size_t count, size_t stride, char const *name) {
    return compare_arrays_tol(map_out, ref, count, stride, name, TEST_TOLERANCE);
}

/* ----------------------------------------------------------------
 * Group A: Simple 3->3 (no extra params)
 * ---------------------------------------------------------------- */

static int test_oklab_maps(void) {
    TEST_START("Map validation: OkLab XYZ<->OkLab, OkLab<->OkLCh");

    size_t const stride = 3 * sizeof(alwan_scalar);
    alwan_scalar *grid = (alwan_scalar *)malloc(MAP_COUNT * stride);
    alwan_scalar *map_out = (alwan_scalar *)malloc(MAP_COUNT * stride);
    alwan_scalar *ref_out = (alwan_scalar *)malloc(MAP_COUNT * stride);
    if (!grid || !map_out || !ref_out) { free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc"); }

    generate_unit_grid(grid);

    /* XYZ -> OkLab */
    alwan_xyz_to_oklab_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_xyz xyz; xyz.x = grid[i*3+0]; xyz.y = grid[i*3+1]; xyz.z = grid[i*3+2];
        alwan_oklab r; alwan_xyz_to_oklab(&r, &xyz);
        ref_out[i*3+0] = r.L; ref_out[i*3+1] = r.a; ref_out[i*3+2] = r.b;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "xyz_to_oklab_map")) goto fail;

    /* OkLab -> XYZ */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_oklab_to_xyz_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_oklab ok; ok.L = grid[i*3+0]; ok.a = grid[i*3+1]; ok.b = grid[i*3+2];
        alwan_xyz r; alwan_oklab_to_xyz(&r, &ok);
        ref_out[i*3+0] = r.x; ref_out[i*3+1] = r.y; ref_out[i*3+2] = r.z;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "oklab_to_xyz_map")) goto fail;

    /* OkLab -> OkLCh */
    generate_unit_grid(grid);
    alwan_xyz_to_oklab_map(grid, grid, MAP_COUNT, stride, stride);
    alwan_oklab_to_oklch_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_oklab ok; ok.L = grid[i*3+0]; ok.a = grid[i*3+1]; ok.b = grid[i*3+2];
        alwan_oklch r; alwan_oklab_to_oklch(&r, &ok);
        ref_out[i*3+0] = r.L; ref_out[i*3+1] = r.C; ref_out[i*3+2] = r.h;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "oklab_to_oklch_map")) goto fail;

    /* OkLCh -> OkLab */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_oklch_to_oklab_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_oklch lch; lch.L = grid[i*3+0]; lch.C = grid[i*3+1]; lch.h = grid[i*3+2];
        alwan_oklab r; alwan_oklch_to_oklab(&r, &lch);
        ref_out[i*3+0] = r.L; ref_out[i*3+1] = r.a; ref_out[i*3+2] = r.b;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "oklch_to_oklab_map")) goto fail;

    free(grid); free(map_out); free(ref_out);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(map_out); free(ref_out); return 1;
}

static int test_lab_lch_maps(void) {
    TEST_START("Map validation: Lab<->LCh, Luv<->LChuv");

    size_t const stride = 3 * sizeof(alwan_scalar);
    alwan_scalar *grid = (alwan_scalar *)malloc(MAP_COUNT * stride);
    alwan_scalar *map_out = (alwan_scalar *)malloc(MAP_COUNT * stride);
    alwan_scalar *ref_out = (alwan_scalar *)malloc(MAP_COUNT * stride);
    if (!grid || !map_out || !ref_out) { free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc"); }

    generate_lab_grid(grid);

    /* Lab -> LCh */
    alwan_lab_to_lch_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_lab lab; lab.L = grid[i*3+0]; lab.a = grid[i*3+1]; lab.b = grid[i*3+2];
        alwan_lch lch; alwan_lab_to_lch(&lch, &lab);
        ref_out[i*3+0] = lch.L; ref_out[i*3+1] = lch.C; ref_out[i*3+2] = lch.h;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "lab_to_lch_map")) goto fail;

    /* LCh -> Lab */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_lch_to_lab_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_lch lch; lch.L = grid[i*3+0]; lch.C = grid[i*3+1]; lch.h = grid[i*3+2];
        alwan_lab lab; alwan_lch_to_lab(&lab, &lch);
        ref_out[i*3+0] = lab.L; ref_out[i*3+1] = lab.a; ref_out[i*3+2] = lab.b;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "lch_to_lab_map")) goto fail;

    /* Luv -> LChuv */
    generate_lab_grid(grid);
    alwan_luv_to_lchuv_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_luv luv; luv.L = grid[i*3+0]; luv.u = grid[i*3+1]; luv.v = grid[i*3+2];
        alwan_lchuv lchuv; alwan_luv_to_lchuv(&lchuv, &luv);
        ref_out[i*3+0] = lchuv.L; ref_out[i*3+1] = lchuv.C; ref_out[i*3+2] = lchuv.h;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "luv_to_lchuv_map")) goto fail;

    /* LChuv -> Luv */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_lchuv_to_luv_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_lchuv lchuv; lchuv.L = grid[i*3+0]; lchuv.C = grid[i*3+1]; lchuv.h = grid[i*3+2];
        alwan_luv luv; alwan_lchuv_to_luv(&luv, &lchuv);
        ref_out[i*3+0] = luv.L; ref_out[i*3+1] = luv.u; ref_out[i*3+2] = luv.v;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "lchuv_to_luv_map")) goto fail;

    free(grid); free(map_out); free(ref_out);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(map_out); free(ref_out); return 1;
}

static int test_xyy_maps(void) {
    TEST_START("Map validation: XYZ<->xyY");

    size_t const stride = 3 * sizeof(alwan_scalar);
    alwan_scalar *grid = (alwan_scalar *)malloc(MAP_COUNT * stride);
    alwan_scalar *map_out = (alwan_scalar *)malloc(MAP_COUNT * stride);
    alwan_scalar *ref_out = (alwan_scalar *)malloc(MAP_COUNT * stride);
    if (!grid || !map_out || !ref_out) { free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc"); }

    generate_unit_grid(grid);

    /* XYZ -> xyY */
    alwan_xyz_to_xyy_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_xyz xyz; xyz.x = grid[i*3+0]; xyz.y = grid[i*3+1]; xyz.z = grid[i*3+2];
        alwan_xyy xyy; alwan_xyz_to_xyy(&xyy, &xyz);
        ref_out[i*3+0] = xyy.x; ref_out[i*3+1] = xyy.y; ref_out[i*3+2] = xyy.Y;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "xyz_to_xyy_map")) goto fail;

    /* xyY -> XYZ */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_xyy_to_xyz_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_xyy xyy; xyy.x = grid[i*3+0]; xyy.y = grid[i*3+1]; xyy.Y = grid[i*3+2];
        alwan_xyz xyz; alwan_xyy_to_xyz(&xyz, &xyy);
        ref_out[i*3+0] = xyz.x; ref_out[i*3+1] = xyz.y; ref_out[i*3+2] = xyz.z;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "xyy_to_xyz_map")) goto fail;

    free(grid); free(map_out); free(ref_out);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(map_out); free(ref_out); return 1;
}

static int test_hsv_hsl_maps(void) {
    TEST_START("Map validation: RGB<->HSV, RGB<->HSL");

    size_t const stride = 3 * sizeof(alwan_scalar);
    alwan_scalar *grid = (alwan_scalar *)malloc(MAP_COUNT * stride);
    alwan_scalar *map_out = (alwan_scalar *)malloc(MAP_COUNT * stride);
    alwan_scalar *ref_out = (alwan_scalar *)malloc(MAP_COUNT * stride);
    if (!grid || !map_out || !ref_out) { free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc"); }

    generate_unit_grid(grid);

    /* RGB -> HSV */
    alwan_rgb_to_hsv_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_rgb rgb; rgb.r = grid[i*3+0]; rgb.g = grid[i*3+1]; rgb.b = grid[i*3+2];
        alwan_hsv hsv; alwan_rgb_to_hsv(&hsv, &rgb);
        ref_out[i*3+0] = hsv.h; ref_out[i*3+1] = hsv.s; ref_out[i*3+2] = hsv.v;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "rgb_to_hsv_map")) goto fail;

    /* HSV -> RGB */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_hsv_to_rgb_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_hsv hsv; hsv.h = grid[i*3+0]; hsv.s = grid[i*3+1]; hsv.v = grid[i*3+2];
        alwan_rgb rgb; alwan_hsv_to_rgb(&rgb, &hsv);
        ref_out[i*3+0] = rgb.r; ref_out[i*3+1] = rgb.g; ref_out[i*3+2] = rgb.b;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "hsv_to_rgb_map")) goto fail;

    /* RGB -> HSL */
    generate_unit_grid(grid);
    alwan_rgb_to_hsl_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_rgb rgb; rgb.r = grid[i*3+0]; rgb.g = grid[i*3+1]; rgb.b = grid[i*3+2];
        alwan_hsl hsl; alwan_rgb_to_hsl(&hsl, &rgb);
        ref_out[i*3+0] = hsl.h; ref_out[i*3+1] = hsl.s; ref_out[i*3+2] = hsl.l;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "rgb_to_hsl_map")) goto fail;

    /* HSL -> RGB */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_hsl_to_rgb_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_hsl hsl; hsl.h = grid[i*3+0]; hsl.s = grid[i*3+1]; hsl.l = grid[i*3+2];
        alwan_rgb rgb; alwan_hsl_to_rgb(&rgb, &hsl);
        ref_out[i*3+0] = rgb.r; ref_out[i*3+1] = rgb.g; ref_out[i*3+2] = rgb.b;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "hsl_to_rgb_map")) goto fail;

    free(grid); free(map_out); free(ref_out);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(map_out); free(ref_out); return 1;
}

/* ----------------------------------------------------------------
 * sRGB convenience maps
 * Validate _map vs _map with count=1 (single-pixel functions not implemented)
 * ---------------------------------------------------------------- */

static int test_srgb_convenience_maps(void) {
    TEST_START("Map validation: sRGB convenience (srgb<->xyz, srgb<->lab, srgb<->oklab)");

    size_t const stride = 3 * sizeof(alwan_scalar);
    alwan_scalar *grid = (alwan_scalar *)malloc(MAP_COUNT * stride);
    alwan_scalar *map_out = (alwan_scalar *)malloc(MAP_COUNT * stride);
    alwan_scalar *ref_out = (alwan_scalar *)malloc(MAP_COUNT * stride);
    if (!grid || !map_out || !ref_out) { free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc"); }

    generate_unit_grid(grid);

    /* sRGB -> XYZ: compare map against per-pixel map(count=1) */
    alwan_srgb_to_xyz_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_srgb_to_xyz_map(&ref_out[i*3], &grid[i*3], 1, stride, stride);
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "srgb_to_xyz_map")) goto fail;

    /* XYZ -> sRGB */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_xyz_to_srgb_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_xyz_to_srgb_map(&ref_out[i*3], &grid[i*3], 1, stride, stride);
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "xyz_to_srgb_map")) goto fail;

    /* sRGB -> Lab */
    generate_unit_grid(grid);
    alwan_srgb_to_lab_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_srgb_to_lab_map(&ref_out[i*3], &grid[i*3], 1, stride, stride);
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "srgb_to_lab_map")) goto fail;

    /* Lab -> sRGB */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_lab_to_srgb_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_lab_to_srgb_map(&ref_out[i*3], &grid[i*3], 1, stride, stride);
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "lab_to_srgb_map")) goto fail;

    /* sRGB -> OkLab */
    generate_unit_grid(grid);
    alwan_srgb_to_oklab_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_srgb_to_oklab_map(&ref_out[i*3], &grid[i*3], 1, stride, stride);
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "srgb_to_oklab_map")) goto fail;

    /* OkLab -> sRGB */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_oklab_to_srgb_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_oklab_to_srgb_map(&ref_out[i*3], &grid[i*3], 1, stride, stride);
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "oklab_to_srgb_map")) goto fail;

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

    size_t const stride = 3 * sizeof(alwan_scalar);
    alwan_scalar *grid = (alwan_scalar *)malloc(MAP_COUNT * stride);
    alwan_scalar *map_out = (alwan_scalar *)malloc(MAP_COUNT * stride);
    alwan_scalar *ref_out = (alwan_scalar *)malloc(MAP_COUNT * stride);
    if (!grid || !map_out || !ref_out) { free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc"); }

    alwan_xyz d65; d65.x = ALWAN_LITERAL(0.95047); d65.y = ALWAN_LITERAL(1.0); d65.z = ALWAN_LITERAL(1.08883);

    generate_unit_grid(grid);

    /* XYZ -> Lab */
    alwan_xyz_to_lab_map(map_out, grid, &d65, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_xyz xyz; xyz.x = grid[i*3+0]; xyz.y = grid[i*3+1]; xyz.z = grid[i*3+2];
        alwan_lab lab; alwan_xyz_to_lab(&lab, &xyz, &d65);
        ref_out[i*3+0] = lab.L; ref_out[i*3+1] = lab.a; ref_out[i*3+2] = lab.b;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "xyz_to_lab_map")) goto fail;

    /* Lab -> XYZ */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_lab_to_xyz_map(map_out, grid, &d65, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_lab lab; lab.L = grid[i*3+0]; lab.a = grid[i*3+1]; lab.b = grid[i*3+2];
        alwan_xyz xyz; alwan_lab_to_xyz(&xyz, &lab, &d65);
        ref_out[i*3+0] = xyz.x; ref_out[i*3+1] = xyz.y; ref_out[i*3+2] = xyz.z;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "lab_to_xyz_map")) goto fail;

    /* XYZ -> Luv */
    generate_unit_grid(grid);
    alwan_xyz_to_luv_map(map_out, grid, &d65, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_xyz xyz; xyz.x = grid[i*3+0]; xyz.y = grid[i*3+1]; xyz.z = grid[i*3+2];
        alwan_luv luv; alwan_xyz_to_luv(&luv, &xyz, &d65);
        ref_out[i*3+0] = luv.L; ref_out[i*3+1] = luv.u; ref_out[i*3+2] = luv.v;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "xyz_to_luv_map")) goto fail;

    /* Luv -> XYZ */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_luv_to_xyz_map(map_out, grid, &d65, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_luv luv; luv.L = grid[i*3+0]; luv.u = grid[i*3+1]; luv.v = grid[i*3+2];
        alwan_xyz xyz; alwan_luv_to_xyz(&xyz, &luv, &d65);
        ref_out[i*3+0] = xyz.x; ref_out[i*3+1] = xyz.y; ref_out[i*3+2] = xyz.z;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "luv_to_xyz_map")) goto fail;

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

    size_t const stride = 3 * sizeof(alwan_scalar);
    alwan_scalar *grid = (alwan_scalar *)malloc(MAP_COUNT * stride);
    alwan_scalar *map_out = (alwan_scalar *)malloc(MAP_COUNT * stride);
    alwan_scalar *ref_out = (alwan_scalar *)malloc(MAP_COUNT * stride);
    if (!grid || !map_out || !ref_out) { free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc"); }

    for (int use_pq = 0; use_pq <= 1; use_pq++) {
        /* RGB -> ICtCp */
        generate_unit_grid(grid);
        alwan_rgb_to_ictcp_map(map_out, grid, use_pq, MAP_COUNT, stride, stride);
        for (size_t i = 0; i < MAP_COUNT; i++) {
            alwan_rgb rgb; rgb.r = grid[i*3+0]; rgb.g = grid[i*3+1]; rgb.b = grid[i*3+2];
            alwan_ictcp ictcp; alwan_rgb_to_ictcp(&ictcp, &rgb, use_pq);
            ref_out[i*3+0] = ictcp.I; ref_out[i*3+1] = ictcp.Ct; ref_out[i*3+2] = ictcp.Cp;
        }
        if (compare_arrays_tol(map_out, ref_out, MAP_COUNT, stride,
                           use_pq ? "rgb_to_ictcp_map PQ" : "rgb_to_ictcp_map HLG", MAP_TOLERANCE)) goto fail;

        /* ICtCp -> RGB */
        memcpy(grid, ref_out, MAP_COUNT * stride);
        alwan_ictcp_to_rgb_map(map_out, grid, use_pq, MAP_COUNT, stride, stride);
        for (size_t i = 0; i < MAP_COUNT; i++) {
            alwan_ictcp ictcp; ictcp.I = grid[i*3+0]; ictcp.Ct = grid[i*3+1]; ictcp.Cp = grid[i*3+2];
            alwan_rgb rgb; alwan_ictcp_to_rgb(&rgb, &ictcp, use_pq);
            ref_out[i*3+0] = rgb.r; ref_out[i*3+1] = rgb.g; ref_out[i*3+2] = rgb.b;
        }
        if (compare_arrays_tol(map_out, ref_out, MAP_COUNT, stride,
                           use_pq ? "ictcp_to_rgb_map PQ" : "ictcp_to_rgb_map HLG", MAP_TOLERANCE)) goto fail;

        /* XYZ -> ICtCp */
        generate_unit_grid(grid);
        alwan_xyz_to_ictcp_map(map_out, grid, use_pq, MAP_COUNT, stride, stride);
        for (size_t i = 0; i < MAP_COUNT; i++) {
            alwan_xyz xyz; xyz.x = grid[i*3+0]; xyz.y = grid[i*3+1]; xyz.z = grid[i*3+2];
            alwan_ictcp ictcp; alwan_xyz_to_ictcp(&ictcp, &xyz, use_pq);
            ref_out[i*3+0] = ictcp.I; ref_out[i*3+1] = ictcp.Ct; ref_out[i*3+2] = ictcp.Cp;
        }
        if (compare_arrays_tol(map_out, ref_out, MAP_COUNT, stride,
                           use_pq ? "xyz_to_ictcp_map PQ" : "xyz_to_ictcp_map HLG", MAP_TOLERANCE)) goto fail;

        /* ICtCp -> XYZ */
        memcpy(grid, ref_out, MAP_COUNT * stride);
        alwan_ictcp_to_xyz_map(map_out, grid, use_pq, MAP_COUNT, stride, stride);
        for (size_t i = 0; i < MAP_COUNT; i++) {
            alwan_ictcp ictcp; ictcp.I = grid[i*3+0]; ictcp.Ct = grid[i*3+1]; ictcp.Cp = grid[i*3+2];
            alwan_xyz xyz; alwan_ictcp_to_xyz(&xyz, &ictcp, use_pq);
            ref_out[i*3+0] = xyz.x; ref_out[i*3+1] = xyz.y; ref_out[i*3+2] = xyz.z;
        }
        if (compare_arrays_tol(map_out, ref_out, MAP_COUNT, stride,
                           use_pq ? "ictcp_to_xyz_map PQ" : "ictcp_to_xyz_map HLG", MAP_TOLERANCE)) goto fail;
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

    size_t const stride = 3 * sizeof(alwan_scalar);
    alwan_scalar *grid = (alwan_scalar *)malloc(MAP_COUNT * stride);
    alwan_scalar *map_out = (alwan_scalar *)malloc(MAP_COUNT * stride);
    alwan_scalar *ref_out = (alwan_scalar *)malloc(MAP_COUNT * stride);
    if (!grid || !map_out || !ref_out) { free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc"); }

    generate_unit_grid(grid);

    /* XYZ -> JzAzBz */
    alwan_xyz_to_jzazbz_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_xyz xyz; xyz.x = grid[i*3+0]; xyz.y = grid[i*3+1]; xyz.z = grid[i*3+2];
        alwan_jzazbz jz; alwan_xyz_to_jzazbz(&jz, &xyz);
        ref_out[i*3+0] = jz.Jz; ref_out[i*3+1] = jz.az; ref_out[i*3+2] = jz.bz;
    }
    if (compare_arrays_tol(map_out, ref_out, MAP_COUNT, stride, "xyz_to_jzazbz_map", MAP_TOLERANCE)) goto fail;

    /* JzAzBz -> XYZ */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_jzazbz_to_xyz_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_jzazbz jz; jz.Jz = grid[i*3+0]; jz.az = grid[i*3+1]; jz.bz = grid[i*3+2];
        alwan_xyz xyz; alwan_jzazbz_to_xyz(&xyz, &jz);
        ref_out[i*3+0] = xyz.x; ref_out[i*3+1] = xyz.y; ref_out[i*3+2] = xyz.z;
    }
    if (compare_arrays_tol(map_out, ref_out, MAP_COUNT, stride, "jzazbz_to_xyz_map", MAP_TOLERANCE)) goto fail;

    /* JzAzBz -> JzCzhz */
    generate_unit_grid(grid);
    alwan_xyz_to_jzazbz_map(grid, grid, MAP_COUNT, stride, stride);
    alwan_jzazbz_to_jzczhz_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_jzazbz jz; jz.Jz = grid[i*3+0]; jz.az = grid[i*3+1]; jz.bz = grid[i*3+2];
        alwan_jzczhz jzch; alwan_jzazbz_to_jzczhz(&jzch, &jz);
        ref_out[i*3+0] = jzch.Jz; ref_out[i*3+1] = jzch.Cz; ref_out[i*3+2] = jzch.hz;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "jzazbz_to_jzczhz_map")) goto fail;

    /* JzCzhz -> JzAzBz */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_jzczhz_to_jzazbz_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_jzczhz jzch; jzch.Jz = grid[i*3+0]; jzch.Cz = grid[i*3+1]; jzch.hz = grid[i*3+2];
        alwan_jzazbz jz; alwan_jzczhz_to_jzazbz(&jz, &jzch);
        ref_out[i*3+0] = jz.Jz; ref_out[i*3+1] = jz.az; ref_out[i*3+2] = jz.bz;
    }
    if (compare_arrays(map_out, ref_out, MAP_COUNT, stride, "jzczhz_to_jzazbz_map")) goto fail;

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

    size_t const stride = 3 * sizeof(alwan_scalar);
    alwan_scalar *grid = (alwan_scalar *)malloc(MAP_COUNT * stride);
    alwan_scalar *map_out = (alwan_scalar *)malloc(MAP_COUNT * stride);
    alwan_scalar *ref_out = (alwan_scalar *)malloc(MAP_COUNT * stride);
    if (!grid || !map_out || !ref_out) { free(grid); free(map_out); free(ref_out); TEST_FAIL("malloc"); }

    generate_unit_grid(grid);

    /* XYZ -> IPT */
    alwan_xyz_to_ipt_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_xyz xyz; xyz.x = grid[i*3+0]; xyz.y = grid[i*3+1]; xyz.z = grid[i*3+2];
        alwan_ipt ipt; alwan_xyz_to_ipt(&ipt, &xyz);
        ref_out[i*3+0] = ipt.I; ref_out[i*3+1] = ipt.P; ref_out[i*3+2] = ipt.T;
    }
    if (compare_arrays_tol(map_out, ref_out, MAP_COUNT, stride, "xyz_to_ipt_map", MAP_TOLERANCE)) goto fail;

    /* IPT -> XYZ */
    memcpy(grid, ref_out, MAP_COUNT * stride);
    alwan_ipt_to_xyz_map(map_out, grid, MAP_COUNT, stride, stride);
    for (size_t i = 0; i < MAP_COUNT; i++) {
        alwan_ipt ipt; ipt.I = grid[i*3+0]; ipt.P = grid[i*3+1]; ipt.T = grid[i*3+2];
        alwan_xyz xyz; alwan_ipt_to_xyz(&xyz, &ipt);
        ref_out[i*3+0] = xyz.x; ref_out[i*3+1] = xyz.y; ref_out[i*3+2] = xyz.z;
    }
    if (compare_arrays_tol(map_out, ref_out, MAP_COUNT, stride, "ipt_to_xyz_map", MAP_TOLERANCE)) goto fail;

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

static void generate_cam_grid(alwan_scalar *out) {
    size_t idx = 0;
    for (int r = 0; r <= 255; r += CAM_STEP) {
        for (int g = 0; g <= 255; g += CAM_STEP) {
            for (int b = 0; b <= 255; b += CAM_STEP) {
                out[idx * 3 + 0] = (alwan_scalar)r / ALWAN_LITERAL(255.0) * ALWAN_LITERAL(100.0);
                out[idx * 3 + 1] = (alwan_scalar)g / ALWAN_LITERAL(255.0) * ALWAN_LITERAL(100.0);
                out[idx * 3 + 2] = (alwan_scalar)b / ALWAN_LITERAL(255.0) * ALWAN_LITERAL(100.0);
                idx++;
            }
        }
    }
}

static int test_ciecam02_maps(void) {
    TEST_START("Map validation: CIECAM02 forward/inverse");

    size_t const stride = 3 * sizeof(alwan_scalar);
    alwan_scalar *grid = (alwan_scalar *)malloc(CAM_COUNT * stride);
    alwan_ciecam02_correlates *map_corr = (alwan_ciecam02_correlates *)malloc(CAM_COUNT * sizeof(alwan_ciecam02_correlates));
    alwan_ciecam02_correlates *ref_corr = (alwan_ciecam02_correlates *)malloc(CAM_COUNT * sizeof(alwan_ciecam02_correlates));
    alwan_scalar *map_xyz = (alwan_scalar *)malloc(CAM_COUNT * stride);
    alwan_scalar *ref_xyz = (alwan_scalar *)malloc(CAM_COUNT * stride);
    if (!grid || !map_corr || !ref_corr || !map_xyz || !ref_xyz) {
        free(grid); free(map_corr); free(ref_corr); free(map_xyz); free(ref_xyz);
        TEST_FAIL("malloc");
    }

    alwan_ciecam02_viewing_conditions vc;
    vc.white_xyz.x = ALWAN_LITERAL(95.047);
    vc.white_xyz.y = ALWAN_LITERAL(100.0);
    vc.white_xyz.z = ALWAN_LITERAL(108.883);
    vc.adapting_luminance = ALWAN_LITERAL(64.0);
    vc.background_luminance = ALWAN_LITERAL(0.2);
    vc.surround = ALWAN_CIECAM02_SURROUND_AVERAGE;
    vc.discount_illuminant = 0;

    generate_cam_grid(grid);

    /* Forward map */
    alwan_ciecam02_forward_map(map_corr, grid, &vc, CAM_COUNT, stride);
    for (size_t i = 0; i < CAM_COUNT; i++) {
        alwan_xyz xyz; xyz.x = grid[i*3+0]; xyz.y = grid[i*3+1]; xyz.z = grid[i*3+2];
        alwan_ciecam02_forward(&ref_corr[i], &xyz, &vc);
    }
    for (size_t i = 0; i < CAM_COUNT; i++) {
        alwan_scalar dJ = ALWAN_ABS(map_corr[i].J - ref_corr[i].J);
        alwan_scalar dC = ALWAN_ABS(map_corr[i].C - ref_corr[i].C);
        alwan_scalar dh = ALWAN_ABS(map_corr[i].h - ref_corr[i].h);
        alwan_scalar max_d = dJ; if (dC > max_d) max_d = dC; if (dh > max_d) max_d = dh;
        if (max_d > MAP_TOLERANCE) {
            printf("[FAIL] ciecam02_forward_map: pixel %zu: diff=%.16e\n", i, (double)max_d);
            goto fail;
        }
    }

    /* Inverse map */
    alwan_ciecam02_inverse_map(map_xyz, ref_corr, &vc, CAM_COUNT, stride);
    for (size_t i = 0; i < CAM_COUNT; i++) {
        alwan_xyz xyz; alwan_ciecam02_inverse(&xyz, &ref_corr[i], &vc);
        ref_xyz[i*3+0] = xyz.x; ref_xyz[i*3+1] = xyz.y; ref_xyz[i*3+2] = xyz.z;
    }
    if (compare_arrays_tol(map_xyz, ref_xyz, CAM_COUNT, stride, "ciecam02_inverse_map", MAP_TOLERANCE)) goto fail;

    free(grid); free(map_corr); free(ref_corr); free(map_xyz); free(ref_xyz);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(map_corr); free(ref_corr); free(map_xyz); free(ref_xyz); return 1;
}

static int test_cam16_maps(void) {
    TEST_START("Map validation: CAM16 forward/inverse");

    size_t const stride = 3 * sizeof(alwan_scalar);
    alwan_scalar *grid = (alwan_scalar *)malloc(CAM_COUNT * stride);
    alwan_cam16_correlates *map_corr = (alwan_cam16_correlates *)malloc(CAM_COUNT * sizeof(alwan_cam16_correlates));
    alwan_cam16_correlates *ref_corr = (alwan_cam16_correlates *)malloc(CAM_COUNT * sizeof(alwan_cam16_correlates));
    alwan_scalar *map_xyz = (alwan_scalar *)malloc(CAM_COUNT * stride);
    alwan_scalar *ref_xyz = (alwan_scalar *)malloc(CAM_COUNT * stride);
    if (!grid || !map_corr || !ref_corr || !map_xyz || !ref_xyz) {
        free(grid); free(map_corr); free(ref_corr); free(map_xyz); free(ref_xyz);
        TEST_FAIL("malloc");
    }

    alwan_cam16_viewing_conditions vc;
    vc.white_xyz.x = ALWAN_LITERAL(95.047);
    vc.white_xyz.y = ALWAN_LITERAL(100.0);
    vc.white_xyz.z = ALWAN_LITERAL(108.883);
    vc.adapting_luminance = ALWAN_LITERAL(64.0);
    vc.background_luminance = ALWAN_LITERAL(0.2);
    vc.surround = ALWAN_CAM16_SURROUND_AVERAGE;
    vc.discount_illuminant = 0;

    generate_cam_grid(grid);

    /* Forward map */
    alwan_cam16_forward_map(map_corr, grid, &vc, CAM_COUNT, stride);
    for (size_t i = 0; i < CAM_COUNT; i++) {
        alwan_xyz xyz; xyz.x = grid[i*3+0]; xyz.y = grid[i*3+1]; xyz.z = grid[i*3+2];
        alwan_cam16_forward(&ref_corr[i], &xyz, &vc);
    }
    for (size_t i = 0; i < CAM_COUNT; i++) {
        alwan_scalar dJ = ALWAN_ABS(map_corr[i].J - ref_corr[i].J);
        alwan_scalar dC = ALWAN_ABS(map_corr[i].C - ref_corr[i].C);
        alwan_scalar dh = ALWAN_ABS(map_corr[i].h - ref_corr[i].h);
        alwan_scalar max_d = dJ; if (dC > max_d) max_d = dC; if (dh > max_d) max_d = dh;
        if (max_d > MAP_TOLERANCE) {
            printf("[FAIL] cam16_forward_map: pixel %zu: diff=%.16e\n", i, (double)max_d);
            goto fail;
        }
    }

    /* Inverse map */
    alwan_cam16_inverse_map(map_xyz, ref_corr, &vc, CAM_COUNT, stride);
    for (size_t i = 0; i < CAM_COUNT; i++) {
        alwan_xyz xyz; alwan_cam16_inverse(&xyz, &ref_corr[i], &vc);
        ref_xyz[i*3+0] = xyz.x; ref_xyz[i*3+1] = xyz.y; ref_xyz[i*3+2] = xyz.z;
    }
    if (compare_arrays_tol(map_xyz, ref_xyz, CAM_COUNT, stride, "cam16_inverse_map", MAP_TOLERANCE)) goto fail;

    free(grid); free(map_corr); free(ref_corr); free(map_xyz); free(ref_xyz);
    TEST_PASS_MSG(); return 0;
fail:
    free(grid); free(map_corr); free(ref_corr); free(map_xyz); free(ref_xyz); return 1;
}

/* ----------------------------------------------------------------
 * Group H: _map_ex typed format validation
 *
 * Tests that _map_ex functions (typed pixel I/O) produce results
 * matching the scalar _map functions across all pixel formats.
 * For integer formats, the reference uses the same quantized input
 * to isolate output quantization error only.
 * ---------------------------------------------------------------- */

#define MAPEX_STEP 15
#define MAPEX_COUNT_1D ((255 / MAPEX_STEP) + 1)  /* 18 */
#define MAPEX_COUNT (MAPEX_COUNT_1D * MAPEX_COUNT_1D * MAPEX_COUNT_1D)  /* 5832 */

static const alwan_pixel_format MAPEX_FMTS[4] = {
    ALWAN_PIXEL_U8, ALWAN_PIXEL_U16, ALWAN_PIXEL_F32, ALWAN_PIXEL_F64
};

static const char *mapex_fmt_name(alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8:  return "U8";
    case ALWAN_PIXEL_U16: return "U16";
    case ALWAN_PIXEL_F32: return "F32";
    case ALWAN_PIXEL_F64: return "F64";
    }
    return "?";
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

static alwan_scalar mapex_tol(alwan_pixel_format fmt) {
    (void)fmt;
    return MAP_TOLERANCE;
}

static void generate_mapex_grid(alwan_scalar *out) {
    size_t idx = 0;
    for (int r = 0; r <= 255; r += MAPEX_STEP) {
        for (int g = 0; g <= 255; g += MAPEX_STEP) {
            for (int b = 0; b <= 255; b += MAPEX_STEP) {
                out[idx * 3 + 0] = (alwan_scalar)r / ALWAN_LITERAL(255.0);
                out[idx * 3 + 1] = (alwan_scalar)g / ALWAN_LITERAL(255.0);
                out[idx * 3 + 2] = (alwan_scalar)b / ALWAN_LITERAL(255.0);
                idx++;
            }
        }
    }
}

/* Compare _map_ex output vs _map reference.
 * For integer formats, skip pixels where reference is outside [0,1]
 * because clamping makes comparison meaningless. */
static int compare_mapex(alwan_scalar const *ex_out, alwan_scalar const *ref_out,
                          size_t count, size_t stride, char const *name,
                          alwan_scalar tol, alwan_pixel_format fmt) {
    int is_int = (fmt == ALWAN_PIXEL_U8 || fmt == ALWAN_PIXEL_U16);
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *e = (alwan_scalar const *)((char const *)ex_out + i * stride);
        alwan_scalar const *r = (alwan_scalar const *)((char const *)ref_out + i * stride);
        for (int c = 0; c < 3; c++) {
            if (is_int && (r[c] < ALWAN_LITERAL(0.0) || r[c] > ALWAN_LITERAL(1.0)))
                continue;
            alwan_scalar diff = ALWAN_ABS(e[c] - r[c]);
            if (diff > tol) {
                printf("[FAIL] %s [%s]: pixel %zu ch %d: ex=%.16e ref=%.16e diff=%.16e\n",
                       name, mapex_fmt_name(fmt), i, c, (double)e[c], (double)r[c], (double)diff);
                return 1;
            }
        }
    }
    return 0;
}

/* ---- Table-driven runner for simple 3->3 pattern ---- */

typedef int (*mapex_fn3)(alwan_scalar *, alwan_scalar const *, size_t, size_t, size_t);
typedef int (*mapex_fn3_ex)(void *, alwan_pixel_format, void const *, alwan_pixel_format,
                             size_t, size_t, size_t);

typedef struct { char const *name; mapex_fn3 map; mapex_fn3_ex map_ex; } mapex_entry3;

static int run_mapex3(mapex_entry3 const *entries, size_t n,
                       alwan_scalar const *grid) {
    size_t const ss = 3 * sizeof(alwan_scalar);
    size_t const mt = 3 * sizeof(double);
    alwan_scalar *ref   = (alwan_scalar *)malloc(MAPEX_COUNT * ss);
    alwan_scalar *ex    = (alwan_scalar *)malloc(MAPEX_COUNT * ss);
    alwan_scalar *qgrid = (alwan_scalar *)malloc(MAPEX_COUNT * ss);
    void *tin  = malloc(MAPEX_COUNT * mt);
    void *tout = malloc(MAPEX_COUNT * mt);
    if (!ref || !ex || !qgrid || !tin || !tout) {
        free(ref); free(ex); free(qgrid); free(tin); free(tout);
        return 1;
    }
    for (size_t e = 0; e < n; e++) {
        for (int f = 0; f < 4; f++) {
            alwan_pixel_format fmt = MAPEX_FMTS[f];
            size_t ts = mapex_typed_stride(fmt);
            alwan_scalar tol = mapex_tol(fmt);

            alwan_scatter3(tin, fmt, grid, MAPEX_COUNT, ss, ts);

            /* For integer formats, run reference on quantized input
             * to isolate output quantization error only */
            if (fmt == ALWAN_PIXEL_U8 || fmt == ALWAN_PIXEL_U16) {
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
    TEST_START("_map_ex: Simple 3->3 (OkLab, Lab/LCh, xyY, JzAzBz, IPT)");
    alwan_scalar *grid = (alwan_scalar *)malloc(MAPEX_COUNT * 3 * sizeof(alwan_scalar));
    if (!grid) { TEST_FAIL("malloc"); }
    generate_mapex_grid(grid);

    static const mapex_entry3 entries[] = {
        {"xyz_to_oklab",      alwan_xyz_to_oklab_map,      alwan_xyz_to_oklab_map_ex},
        {"oklab_to_xyz",      alwan_oklab_to_xyz_map,      alwan_oklab_to_xyz_map_ex},
        {"oklab_to_oklch",    alwan_oklab_to_oklch_map,    alwan_oklab_to_oklch_map_ex},
        {"oklch_to_oklab",    alwan_oklch_to_oklab_map,    alwan_oklch_to_oklab_map_ex},
        {"lab_to_lch",        alwan_lab_to_lch_map,        alwan_lab_to_lch_map_ex},
        {"lch_to_lab",        alwan_lch_to_lab_map,        alwan_lch_to_lab_map_ex},
        {"luv_to_lchuv",      alwan_luv_to_lchuv_map,      alwan_luv_to_lchuv_map_ex},
        {"lchuv_to_luv",      alwan_lchuv_to_luv_map,      alwan_lchuv_to_luv_map_ex},
        {"xyz_to_xyy",        alwan_xyz_to_xyy_map,        alwan_xyz_to_xyy_map_ex},
        {"xyy_to_xyz",        alwan_xyy_to_xyz_map,        alwan_xyy_to_xyz_map_ex},
        {"xyz_to_jzazbz",    alwan_xyz_to_jzazbz_map,    alwan_xyz_to_jzazbz_map_ex},
        {"jzazbz_to_xyz",    alwan_jzazbz_to_xyz_map,    alwan_jzazbz_to_xyz_map_ex},
        {"jzazbz_to_jzczhz", alwan_jzazbz_to_jzczhz_map, alwan_jzazbz_to_jzczhz_map_ex},
        {"jzczhz_to_jzazbz", alwan_jzczhz_to_jzazbz_map, alwan_jzczhz_to_jzazbz_map_ex},
        {"xyz_to_ipt",        alwan_xyz_to_ipt_map,        alwan_xyz_to_ipt_map_ex},
        {"ipt_to_xyz",        alwan_ipt_to_xyz_map,        alwan_ipt_to_xyz_map_ex},
    };

    int r = run_mapex3(entries, sizeof(entries) / sizeof(entries[0]), grid);
    free(grid);
    if (r) return 1;
    TEST_PASS_MSG(); return 0;
}

/* ---- H2: sRGB convenience maps ---- */

static int test_ex_srgb_maps(void) {
    TEST_START("_map_ex: sRGB convenience (srgb<->xyz, srgb<->lab, srgb<->oklab)");
    alwan_scalar *grid = (alwan_scalar *)malloc(MAPEX_COUNT * 3 * sizeof(alwan_scalar));
    if (!grid) { TEST_FAIL("malloc"); }
    generate_mapex_grid(grid);

    static const mapex_entry3 entries[] = {
        {"srgb_to_xyz",   alwan_srgb_to_xyz_map,   alwan_srgb_to_xyz_map_ex},
        {"xyz_to_srgb",   alwan_xyz_to_srgb_map,   alwan_xyz_to_srgb_map_ex},
        {"srgb_to_lab",   alwan_srgb_to_lab_map,   alwan_srgb_to_lab_map_ex},
        {"lab_to_srgb",   alwan_lab_to_srgb_map,   alwan_lab_to_srgb_map_ex},
        {"srgb_to_oklab", alwan_srgb_to_oklab_map, alwan_srgb_to_oklab_map_ex},
        {"oklab_to_srgb", alwan_oklab_to_srgb_map, alwan_oklab_to_srgb_map_ex},
    };

    int r = run_mapex3(entries, sizeof(entries) / sizeof(entries[0]), grid);
    free(grid);
    if (r) return 1;
    TEST_PASS_MSG(); return 0;
}

/* ---- H3: HSV/HSL maps ---- */

static int test_ex_hsv_hsl_maps(void) {
    TEST_START("_map_ex: HSV/HSL (rgb<->hsv, rgb<->hsl)");
    alwan_scalar *grid = (alwan_scalar *)malloc(MAPEX_COUNT * 3 * sizeof(alwan_scalar));
    if (!grid) { TEST_FAIL("malloc"); }
    generate_mapex_grid(grid);

    static const mapex_entry3 entries[] = {
        {"rgb_to_hsv", alwan_rgb_to_hsv_map, alwan_rgb_to_hsv_map_ex},
        {"hsv_to_rgb", alwan_hsv_to_rgb_map, alwan_hsv_to_rgb_map_ex},
        {"rgb_to_hsl", alwan_rgb_to_hsl_map, alwan_rgb_to_hsl_map_ex},
        {"hsl_to_rgb", alwan_hsl_to_rgb_map, alwan_hsl_to_rgb_map_ex},
    };

    int r = run_mapex3(entries, sizeof(entries) / sizeof(entries[0]), grid);
    free(grid);
    if (r) return 1;
    TEST_PASS_MSG(); return 0;
}

/* ---- H4: White point maps ---- */

static int test_ex_white_point_maps(void) {
    TEST_START("_map_ex: White point (xyz<->lab, xyz<->luv with D65)");

    size_t const ss = 3 * sizeof(alwan_scalar);
    size_t const mt = 3 * sizeof(double);
    alwan_scalar *grid  = (alwan_scalar *)malloc(MAPEX_COUNT * ss);
    alwan_scalar *ref   = (alwan_scalar *)malloc(MAPEX_COUNT * ss);
    alwan_scalar *ex    = (alwan_scalar *)malloc(MAPEX_COUNT * ss);
    alwan_scalar *qgrid = (alwan_scalar *)malloc(MAPEX_COUNT * ss);
    void *tin  = malloc(MAPEX_COUNT * mt);
    void *tout = malloc(MAPEX_COUNT * mt);
    if (!grid || !ref || !ex || !qgrid || !tin || !tout) {
        free(grid); free(ref); free(ex); free(qgrid); free(tin); free(tout);
        TEST_FAIL("malloc");
    }

    generate_mapex_grid(grid);
    alwan_xyz d65; d65.x = ALWAN_LITERAL(0.95047); d65.y = ALWAN_LITERAL(1.0); d65.z = ALWAN_LITERAL(1.08883);

    typedef int (*fn_w)(alwan_scalar *, alwan_scalar const *, alwan_xyz const *, size_t, size_t, size_t);
    typedef int (*fn_w_ex)(void *, alwan_pixel_format, void const *, alwan_pixel_format,
                            alwan_xyz const *, size_t, size_t, size_t);

    struct { char const *name; fn_w map; fn_w_ex map_ex; } entries[] = {
        {"xyz_to_lab", alwan_xyz_to_lab_map, alwan_xyz_to_lab_map_ex},
        {"lab_to_xyz", alwan_lab_to_xyz_map, alwan_lab_to_xyz_map_ex},
        {"xyz_to_luv", alwan_xyz_to_luv_map, alwan_xyz_to_luv_map_ex},
        {"luv_to_xyz", alwan_luv_to_xyz_map, alwan_luv_to_xyz_map_ex},
    };

    for (int e = 0; e < 4; e++) {
        for (int f = 0; f < 4; f++) {
            alwan_pixel_format fmt = MAPEX_FMTS[f];
            size_t ts = mapex_typed_stride(fmt);
            alwan_scalar tol = mapex_tol(fmt);

            alwan_scatter3(tin, fmt, grid, MAPEX_COUNT, ss, ts);

            if (fmt == ALWAN_PIXEL_U8 || fmt == ALWAN_PIXEL_U16) {
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
    TEST_START("_map_ex: ICtCp (rgb/xyz<->ictcp, PQ+HLG)");

    size_t const ss = 3 * sizeof(alwan_scalar);
    size_t const mt = 3 * sizeof(double);
    alwan_scalar *grid  = (alwan_scalar *)malloc(MAPEX_COUNT * ss);
    alwan_scalar *ref   = (alwan_scalar *)malloc(MAPEX_COUNT * ss);
    alwan_scalar *ex    = (alwan_scalar *)malloc(MAPEX_COUNT * ss);
    alwan_scalar *qgrid = (alwan_scalar *)malloc(MAPEX_COUNT * ss);
    void *tin  = malloc(MAPEX_COUNT * mt);
    void *tout = malloc(MAPEX_COUNT * mt);
    if (!grid || !ref || !ex || !qgrid || !tin || !tout) {
        free(grid); free(ref); free(ex); free(qgrid); free(tin); free(tout);
        TEST_FAIL("malloc");
    }

    generate_mapex_grid(grid);

    typedef int (*fn_pq)(alwan_scalar *, alwan_scalar const *, int, size_t, size_t, size_t);
    typedef int (*fn_pq_ex)(void *, alwan_pixel_format, void const *, alwan_pixel_format,
                             int, size_t, size_t, size_t);

    struct { char const *name; fn_pq map; fn_pq_ex map_ex; } entries[] = {
        {"rgb_to_ictcp",  alwan_rgb_to_ictcp_map,  alwan_rgb_to_ictcp_map_ex},
        {"ictcp_to_rgb",  alwan_ictcp_to_rgb_map,  alwan_ictcp_to_rgb_map_ex},
        {"xyz_to_ictcp",  alwan_xyz_to_ictcp_map,  alwan_xyz_to_ictcp_map_ex},
        {"ictcp_to_xyz",  alwan_ictcp_to_xyz_map,  alwan_ictcp_to_xyz_map_ex},
    };

    for (int pq = 0; pq <= 1; pq++) {
        for (int e = 0; e < 4; e++) {
            for (int f = 0; f < 4; f++) {
                alwan_pixel_format fmt = MAPEX_FMTS[f];
                size_t ts = mapex_typed_stride(fmt);
                alwan_scalar tol = mapex_tol(fmt);

                alwan_scatter3(tin, fmt, grid, MAPEX_COUNT, ss, ts);

                if (fmt == ALWAN_PIXEL_U8 || fmt == ALWAN_PIXEL_U16) {
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
    TEST_START("_map_ex: CAM (CIECAM02/CAM16 forward/inverse)");

    size_t const ss = 3 * sizeof(alwan_scalar);
    size_t const mt = 3 * sizeof(double);
    alwan_scalar *grid     = (alwan_scalar *)malloc(MAPEX_COUNT * ss);
    alwan_scalar *ref_xyz  = (alwan_scalar *)malloc(MAPEX_COUNT * ss);
    alwan_scalar *ex_xyz   = (alwan_scalar *)malloc(MAPEX_COUNT * ss);
    alwan_scalar *qgrid    = (alwan_scalar *)malloc(MAPEX_COUNT * ss);
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
        alwan_pixel_format fmt = MAPEX_FMTS[f];
        size_t ts = mapex_typed_stride(fmt);
        alwan_scalar tol = mapex_tol(fmt);

        alwan_scalar cam_fwd_tol = MAP_TOLERANCE;

        alwan_scatter3(typed_xyz, fmt, grid, MAPEX_COUNT, ss, ts);

        /* --- CIECAM02 --- */

        /* Forward reference (on quantized input for int formats) */
        if (fmt == ALWAN_PIXEL_U8 || fmt == ALWAN_PIXEL_U16) {
            alwan_collect3(qgrid, typed_xyz, fmt, MAPEX_COUNT, ts, ss);
            alwan_ciecam02_forward_map(c02_ref, qgrid, &vc02, MAPEX_COUNT, ss);
        } else {
            alwan_ciecam02_forward_map(c02_ref, grid, &vc02, MAPEX_COUNT, ss);
        }

        /* Forward _map_ex */
        alwan_ciecam02_forward_map_ex(c02_ex, typed_xyz, fmt, &vc02, MAPEX_COUNT, ts);
        for (size_t i = 0; i < MAPEX_COUNT; i++) {
            alwan_scalar dJ = ALWAN_ABS(c02_ex[i].J - c02_ref[i].J);
            alwan_scalar dC = ALWAN_ABS(c02_ex[i].C - c02_ref[i].C);
            alwan_scalar dh = ALWAN_ABS(c02_ex[i].h - c02_ref[i].h);
            alwan_scalar max_d = dJ; if (dC > max_d) max_d = dC; if (dh > max_d) max_d = dh;
            if (max_d > cam_fwd_tol) {
                printf("[FAIL] ciecam02_forward_map_ex [%s]: pixel %zu: diff=%.6e (tol=%.6e)\n",
                       mapex_fmt_name(fmt), i, (double)max_d, (double)cam_fwd_tol);
                goto fail;
            }
        }

        /* Inverse reference */
        alwan_ciecam02_inverse_map(ref_xyz, c02_ref, &vc02, MAPEX_COUNT, ss);

        /* Inverse _map_ex (correlates in, typed XYZ out) */
        alwan_ciecam02_inverse_map_ex(typed_xyz, fmt, c02_ref, &vc02, MAPEX_COUNT, ts);
        alwan_collect3(ex_xyz, typed_xyz, fmt, MAPEX_COUNT, ts, ss);
        if (compare_mapex(ex_xyz, ref_xyz, MAPEX_COUNT, ss,
                          "ciecam02_inverse", tol, fmt)) goto fail;

        /* --- CAM16 --- */

        alwan_scatter3(typed_xyz, fmt, grid, MAPEX_COUNT, ss, ts);

        if (fmt == ALWAN_PIXEL_U8 || fmt == ALWAN_PIXEL_U16) {
            alwan_cam16_forward_map(c16_ref, qgrid, &vc16, MAPEX_COUNT, ss);
        } else {
            alwan_cam16_forward_map(c16_ref, grid, &vc16, MAPEX_COUNT, ss);
        }

        alwan_cam16_forward_map_ex(c16_ex, typed_xyz, fmt, &vc16, MAPEX_COUNT, ts);
        for (size_t i = 0; i < MAPEX_COUNT; i++) {
            alwan_scalar dJ = ALWAN_ABS(c16_ex[i].J - c16_ref[i].J);
            alwan_scalar dC = ALWAN_ABS(c16_ex[i].C - c16_ref[i].C);
            alwan_scalar dh = ALWAN_ABS(c16_ex[i].h - c16_ref[i].h);
            alwan_scalar max_d = dJ; if (dC > max_d) max_d = dC; if (dh > max_d) max_d = dh;
            if (max_d > cam_fwd_tol) {
                printf("[FAIL] cam16_forward_map_ex [%s]: pixel %zu: diff=%.6e (tol=%.6e)\n",
                       mapex_fmt_name(fmt), i, (double)max_d, (double)cam_fwd_tol);
                goto fail;
            }
        }

        alwan_cam16_inverse_map(ref_xyz, c16_ref, &vc16, MAPEX_COUNT, ss);

        alwan_cam16_inverse_map_ex(typed_xyz, fmt, c16_ref, &vc16, MAPEX_COUNT, ts);
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

    /* Group H: _map_ex typed format validation */
    printf("\nGroup H: _map_ex typed format validation\n");
    printf("--------------------------------\n");
    if (test_ex_simple_maps()) return 1;
    if (test_ex_srgb_maps()) return 1;
    if (test_ex_hsv_hsl_maps()) return 1;
    if (test_ex_white_point_maps()) return 1;
    if (test_ex_ictcp_maps()) return 1;
    if (test_ex_cam_maps()) return 1;

    printf("\n========================================\n");
    printf("Test Results: %d/%d passed\n", test_passed, test_count);
    printf("========================================\n");

    return (test_passed == test_count) ? 0 : 1;
}
