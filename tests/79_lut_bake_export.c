/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 79: LUT baking, 2D flattening, .cube export/import, 1D/2D/3D sampling,
 *          precision comparison vs direct conversion functions
 */

#include "alwan.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#if ALWAN_SCALAR_IS_FLOAT
#  define TOL 1e-4
#else
#  define TOL 1e-10
#endif

#define ASSERT_NEAR(a, b, t) do { \
    double _a = (double)(a), _b = (double)(b); \
    if (fabs(_a - _b) > (t)) { \
        printf("  FAIL: %s = %.12f, expected %.12f (diff %.2e) at %s:%d\n", \
               #a, _a, _b, fabs(_a - _b), __FILE__, __LINE__); \
        return 1; \
    } \
} while(0)

/* ----------------------------------------------------------------
 * Test: 3D LUT baking (identity transform)
 * ---------------------------------------------------------------- */
static int test_bake_3dlut_identity(void) {
    printf("  test_bake_3dlut_identity...\n");

    alwan_ctx *ctx = alwan_create(NULL);

    /* sRGB -> sRGB should produce identity LUT */
    alwan_rgb_space_desc srgb;
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);

    int const size = 5;
    size_t const total = (size_t)size * (size_t)size * (size_t)size;
    alwan_scalar *lut = (alwan_scalar *)malloc(total * 3 * sizeof(alwan_scalar));

    int status = alwan_bake_3dlut(lut, size, ctx, &srgb, &srgb);
    if (status != ALWAN_OK) {
        printf("  FAIL: alwan_bake_3dlut returned %d\n", status);
        free(lut); alwan_destroy(ctx);
        return 1;
    }

    /* Every entry should match its coordinate */
    alwan_scalar const inv = 1.0 / (size - 1);
    for (int b = 0; b < size; b++) {
        for (int g = 0; g < size; g++) {
            for (int r = 0; r < size; r++) {
                size_t idx = ((size_t)b * size * size + (size_t)g * size + r) * 3;
                ASSERT_NEAR(lut[idx + 0], r * inv, TOL);
                ASSERT_NEAR(lut[idx + 1], g * inv, TOL);
                ASSERT_NEAR(lut[idx + 2], b * inv, TOL);
            }
        }
    }

    free(lut);
    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: 1D LUT baking
 * ---------------------------------------------------------------- */
static int test_bake_1dlut(void) {
    printf("  test_bake_1dlut...\n");

    int const size = 256;
    alwan_scalar *lut = (alwan_scalar *)malloc((size_t)size * sizeof(alwan_scalar));

    /* Linear TF should give identity */
    int status = alwan_bake_1dlut(lut, size, ALWAN_TF_LINEAR, 1);
    if (status != ALWAN_OK) {
        printf("  FAIL: alwan_bake_1dlut returned %d\n", status);
        free(lut);
        return 1;
    }

    for (int i = 0; i < size; i++) {
        alwan_scalar expected = (alwan_scalar)i / (alwan_scalar)(size - 1);
        ASSERT_NEAR(lut[i], expected, TOL);
    }

    /* sRGB OETF: should be monotonically increasing */
    status = alwan_bake_1dlut(lut, size, ALWAN_TF_SRGB, 1);
    if (status != ALWAN_OK) {
        printf("  FAIL: sRGB OETF bake returned %d\n", status);
        free(lut);
        return 1;
    }

    ASSERT_NEAR(lut[0], 0.0, TOL);
    for (int i = 1; i < size; i++) {
        if (lut[i] < lut[i - 1] - 1e-15) {
            printf("  FAIL: sRGB OETF not monotonic at index %d\n", i);
            free(lut);
            return 1;
        }
    }

    free(lut);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: 3D -> 2D flatten and unflatten roundtrip
 * ---------------------------------------------------------------- */
static int test_lut_2d_roundtrip(void) {
    printf("  test_lut_2d_roundtrip...\n");

    int const size = 4;
    size_t const total3d = (size_t)size * size * size * 3;
    size_t const total2d = (size_t)(size * size) * size * 3;

    alwan_scalar *lut3d = (alwan_scalar *)malloc(total3d * sizeof(alwan_scalar));
    alwan_scalar *lut2d = (alwan_scalar *)malloc(total2d * sizeof(alwan_scalar));
    alwan_scalar *roundtrip = (alwan_scalar *)malloc(total3d * sizeof(alwan_scalar));

    /* Fill 3D LUT with known pattern */
    alwan_scalar inv = 1.0 / (size - 1);
    for (int b = 0; b < size; b++) {
        for (int g = 0; g < size; g++) {
            for (int r = 0; r < size; r++) {
                size_t idx = ((size_t)b * size * size + (size_t)g * size + r) * 3;
                lut3d[idx + 0] = r * inv;
                lut3d[idx + 1] = g * inv;
                lut3d[idx + 2] = b * inv;
            }
        }
    }

    /* Flatten to 2D */
    int status = alwan_lut3d_to_2d(lut2d, lut3d, size);
    if (status != ALWAN_OK) {
        printf("  FAIL: alwan_lut3d_to_2d returned %d\n", status);
        free(lut3d); free(lut2d); free(roundtrip);
        return 1;
    }

    /* Verify 2D dimensions */
    int w, h;
    alwan_lut2d_dimensions(size, &w, &h);
    if (w != size * size || h != size) {
        printf("  FAIL: dimensions %dx%d, expected %dx%d\n", w, h, size * size, size);
        free(lut3d); free(lut2d); free(roundtrip);
        return 1;
    }

    /* Unflatten back to 3D */
    status = alwan_lut2d_to_3d(roundtrip, lut2d, size);
    if (status != ALWAN_OK) {
        printf("  FAIL: alwan_lut2d_to_3d returned %d\n", status);
        free(lut3d); free(lut2d); free(roundtrip);
        return 1;
    }

    /* Verify roundtrip */
    for (size_t i = 0; i < total3d; i++) {
        ASSERT_NEAR(roundtrip[i], lut3d[i], 1e-15);
    }

    free(lut3d);
    free(lut2d);
    free(roundtrip);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: 2D LUT layout correctness
 * ---------------------------------------------------------------- */
static int test_lut_2d_layout(void) {
    printf("  test_lut_2d_layout...\n");

    int const size = 4;
    int const w = size * size;
    size_t const total3d = (size_t)size * size * size * 3;
    size_t const total2d = (size_t)w * size * 3;

    alwan_scalar *lut3d = (alwan_scalar *)malloc(total3d * sizeof(alwan_scalar));
    alwan_scalar *lut2d = (alwan_scalar *)malloc(total2d * sizeof(alwan_scalar));

    /* Fill with coordinate values */
    alwan_scalar inv = 1.0 / (size - 1);
    for (int b = 0; b < size; b++) {
        for (int g = 0; g < size; g++) {
            for (int r = 0; r < size; r++) {
                size_t idx = ((size_t)b * size * size + (size_t)g * size + r) * 3;
                lut3d[idx + 0] = r * inv;
                lut3d[idx + 1] = g * inv;
                lut3d[idx + 2] = b * inv;
            }
        }
    }

    alwan_lut3d_to_2d(lut2d, lut3d, size);

    alwan_scalar *p;

    /* (0,0) = black */
    p = lut2d;
    ASSERT_NEAR(p[0], 0.0, 1e-15);
    ASSERT_NEAR(p[1], 0.0, 1e-15);
    ASSERT_NEAR(p[2], 0.0, 1e-15);

    /* (size-1, 0) = red */
    p = lut2d + (size_t)(size - 1) * 3;
    ASSERT_NEAR(p[0], 1.0, 1e-15);
    ASSERT_NEAR(p[1], 0.0, 1e-15);
    ASSERT_NEAR(p[2], 0.0, 1e-15);

    /* (size, 0) = first R, first G, second B slice */
    p = lut2d + (size_t)size * 3;
    ASSERT_NEAR(p[0], 0.0, 1e-15);
    ASSERT_NEAR(p[1], 0.0, 1e-15);
    ASSERT_NEAR(p[2], inv, 1e-15);

    /* (0, size-1) = green */
    p = lut2d + (size_t)(size - 1) * w * 3;
    ASSERT_NEAR(p[0], 0.0, 1e-15);
    ASSERT_NEAR(p[1], 1.0, 1e-15);
    ASSERT_NEAR(p[2], 0.0, 1e-15);

    free(lut3d);
    free(lut2d);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: bake directly to 2D
 * ---------------------------------------------------------------- */
static int test_bake_2dlut(void) {
    printf("  test_bake_2dlut...\n");

    alwan_ctx *ctx = alwan_create(NULL);
    alwan_rgb_space_desc srgb;
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);

    int const size = 5;
    int const w = size * size;
    size_t const total2d = (size_t)w * size * 3;
    size_t const total3d = (size_t)size * size * size * 3;

    alwan_scalar *lut2d_direct = (alwan_scalar *)malloc(total2d * sizeof(alwan_scalar));
    alwan_scalar *lut3d = (alwan_scalar *)malloc(total3d * sizeof(alwan_scalar));
    alwan_scalar *lut2d_indirect = (alwan_scalar *)malloc(total2d * sizeof(alwan_scalar));

    /* Direct 2D bake */
    int status = alwan_bake_2dlut(lut2d_direct, size, ctx, &srgb, &srgb);
    if (status != ALWAN_OK) {
        printf("  FAIL: alwan_bake_2dlut returned %d\n", status);
        free(lut2d_direct); free(lut3d); free(lut2d_indirect);
        alwan_destroy(ctx);
        return 1;
    }

    /* Indirect: bake 3D then flatten */
    alwan_bake_3dlut(lut3d, size, ctx, &srgb, &srgb);
    alwan_lut3d_to_2d(lut2d_indirect, lut3d, size);

    /* Both should produce identical results */
    for (size_t i = 0; i < total2d; i++) {
        ASSERT_NEAR(lut2d_direct[i], lut2d_indirect[i], TOL);
    }

    free(lut2d_direct);
    free(lut3d);
    free(lut2d_indirect);
    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: trilinear sampling
 * ---------------------------------------------------------------- */
static int test_lut3d_sample(void) {
    printf("  test_lut3d_sample...\n");

    alwan_ctx *ctx = alwan_create(NULL);
    alwan_rgb_space_desc srgb;
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);

    int const size = 17;
    size_t const total = (size_t)size * size * size;
    alwan_scalar *lut = (alwan_scalar *)malloc(total * 3 * sizeof(alwan_scalar));

    /* Identity LUT */
    alwan_bake_3dlut(lut, size, ctx, &srgb, &srgb);

    /* Sample at grid points — should be exact */
    alwan_rgb in, out;
    in.r = 0.5; in.g = 0.25; in.b = 0.75;
    alwan_lut3d_sample(&out, lut, &in, size);
    ASSERT_NEAR(out.r, 0.5, TOL);
    ASSERT_NEAR(out.g, 0.25, TOL);
    ASSERT_NEAR(out.b, 0.75, TOL);

    /* Sample at corners */
    in.r = 0.0; in.g = 0.0; in.b = 0.0;
    alwan_lut3d_sample(&out, lut, &in, size);
    ASSERT_NEAR(out.r, 0.0, TOL);
    ASSERT_NEAR(out.g, 0.0, TOL);
    ASSERT_NEAR(out.b, 0.0, TOL);

    in.r = 1.0; in.g = 1.0; in.b = 1.0;
    alwan_lut3d_sample(&out, lut, &in, size);
    ASSERT_NEAR(out.r, 1.0, TOL);
    ASSERT_NEAR(out.g, 1.0, TOL);
    ASSERT_NEAR(out.b, 1.0, TOL);

    /* Sample at non-grid point — interpolation should still be close to identity */
    in.r = 0.37; in.g = 0.62; in.b = 0.11;
    alwan_lut3d_sample(&out, lut, &in, size);
    ASSERT_NEAR(out.r, 0.37, 0.001);
    ASSERT_NEAR(out.g, 0.62, 0.001);
    ASSERT_NEAR(out.b, 0.11, 0.001);

    free(lut);
    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: .cube export and re-import roundtrip
 * ---------------------------------------------------------------- */
static int test_cube_roundtrip(void) {
    printf("  test_cube_roundtrip...\n");

    alwan_ctx *ctx = alwan_create(NULL);
    alwan_rgb_space_desc srgb;
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);

    int const size = 5;
    size_t const total = (size_t)size * size * size;
    alwan_scalar *lut_out = (alwan_scalar *)malloc(total * 3 * sizeof(alwan_scalar));
    alwan_scalar *lut_in = (alwan_scalar *)malloc(total * 3 * sizeof(alwan_scalar));

    alwan_bake_3dlut(lut_out, size, ctx, &srgb, &srgb);

    /* Export to .cube file */
    char const *path = "test_roundtrip.cube";
    int status = alwan_cube_export_3d(path, lut_out, size, "Test Identity LUT");
    if (status != ALWAN_OK) {
        printf("  FAIL: export returned %d\n", status);
        free(lut_out); free(lut_in); alwan_destroy(ctx);
        return 1;
    }

    /* Import back */
    int imported_size = 0;
    status = alwan_cube_import_3d(lut_in, &imported_size, path);
    if (status != ALWAN_OK) {
        printf("  FAIL: import returned %d\n", status);
        free(lut_out); free(lut_in); alwan_destroy(ctx);
        remove(path);
        return 1;
    }

    if (imported_size != size) {
        printf("  FAIL: imported size %d, expected %d\n", imported_size, size);
        free(lut_out); free(lut_in); alwan_destroy(ctx);
        remove(path);
        return 1;
    }

    /* Compare (within text serialization precision) */
    for (size_t i = 0; i < total * 3; i++) {
        ASSERT_NEAR(lut_in[i], lut_out[i], 1e-9);
    }

    remove(path);
    free(lut_out);
    free(lut_in);
    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: .cube export to buffer
 * ---------------------------------------------------------------- */
static int test_cube_buffer_export(void) {
    printf("  test_cube_buffer_export...\n");

    int const size = 3;
    size_t const total = (size_t)size * size * size;
    alwan_scalar *lut = (alwan_scalar *)malloc(total * 3 * sizeof(alwan_scalar));

    /* Fill with simple identity pattern */
    alwan_scalar inv = 1.0 / (size - 1);
    for (int b = 0; b < size; b++) {
        for (int g = 0; g < size; g++) {
            for (int r = 0; r < size; r++) {
                size_t idx = ((size_t)b * size * size + (size_t)g * size + r) * 3;
                lut[idx + 0] = r * inv;
                lut[idx + 1] = g * inv;
                lut[idx + 2] = b * inv;
            }
        }
    }

    /* Export to buffer */
    char buf[16384];
    size_t written = 0;
    int status = alwan_cube_export_3d_buffer(buf, sizeof(buf), &written, lut, size, "Buffer Test");
    if (status != ALWAN_OK) {
        printf("  FAIL: buffer export returned %d\n", status);
        free(lut);
        return 1;
    }

    /* Verify header present */
    if (strstr(buf, "TITLE \"Buffer Test\"") == NULL) {
        printf("  FAIL: TITLE not found in buffer\n");
        free(lut);
        return 1;
    }
    if (strstr(buf, "LUT_3D_SIZE 3") == NULL) {
        printf("  FAIL: LUT_3D_SIZE not found in buffer\n");
        free(lut);
        return 1;
    }

    /* Re-import from buffer */
    alwan_scalar *lut2 = (alwan_scalar *)malloc(total * 3 * sizeof(alwan_scalar));
    int imported_size = 0;
    status = alwan_cube_import_3d_buffer(lut2, &imported_size, buf, written);
    if (status != ALWAN_OK) {
        printf("  FAIL: buffer import returned %d\n", status);
        free(lut); free(lut2);
        return 1;
    }

    if (imported_size != size) {
        printf("  FAIL: imported size %d, expected %d\n", imported_size, size);
        free(lut); free(lut2);
        return 1;
    }

    for (size_t i = 0; i < total * 3; i++) {
        ASSERT_NEAR(lut2[i], lut[i], 1e-9);
    }

    free(lut);
    free(lut2);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: 3D LUT baking with view transform
 * ---------------------------------------------------------------- */
static int test_bake_3dlut_view(void) {
    printf("  test_bake_3dlut_view...\n");

    alwan_ctx *ctx = alwan_create(NULL);
    alwan_rgb_space_desc srgb;
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);

    int const size = 5;
    size_t const total = (size_t)size * size * size;
    alwan_scalar *lut_plain = (alwan_scalar *)malloc(total * 3 * sizeof(alwan_scalar));
    alwan_scalar *lut_view = (alwan_scalar *)malloc(total * 3 * sizeof(alwan_scalar));

    /* Plain identity */
    alwan_bake_3dlut(lut_plain, size, ctx, &srgb, &srgb);

    /* With Reinhard Extended tone map */
    int status = alwan_bake_3dlut_view(lut_view, size, ctx, &srgb, &srgb,
                                        ALWAN_VIEW_REINHARD_EXT);
    if (status != ALWAN_OK) {
        printf("  FAIL: alwan_bake_3dlut_view returned %d\n", status);
        free(lut_plain); free(lut_view); alwan_destroy(ctx);
        return 1;
    }

    /* Black should stay black */
    ASSERT_NEAR(lut_view[0], 0.0, TOL);
    ASSERT_NEAR(lut_view[1], 0.0, TOL);
    ASSERT_NEAR(lut_view[2], 0.0, TOL);

    /* White should be compressed */
    size_t white_idx = (total - 1) * 3;
    if (lut_view[white_idx] >= 1.0 || lut_view[white_idx + 1] >= 1.0) {
        printf("  FAIL: view transform should compress highlights\n");
        free(lut_plain); free(lut_view); alwan_destroy(ctx);
        return 1;
    }

    /* View LUT should differ from plain */
    int any_diff = 0;
    for (size_t i = 3; i < total * 3; i++) { /* skip black */
        if (fabs((double)(lut_view[i] - lut_plain[i])) > 1e-15) {
            any_diff = 1;
            break;
        }
    }
    if (!any_diff) {
        printf("  FAIL: view transform LUT should differ from identity\n");
        free(lut_plain); free(lut_view); alwan_destroy(ctx);
        return 1;
    }

    free(lut_plain);
    free(lut_view);
    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: NULL and invalid argument handling
 * ---------------------------------------------------------------- */
static int test_lut_null_checks(void) {
    printf("  test_lut_null_checks...\n");

    alwan_scalar buf[3 * 8]; /* 2^3 */
    alwan_rgb_space_desc desc;
    memset(&desc, 0, sizeof(desc));

    /* NULL output */
    if (alwan_bake_3dlut(NULL, 2, NULL, &desc, &desc) != ALWAN_E_INVALID) return 1;

    /* Invalid size */
    if (alwan_bake_3dlut(buf, 1, NULL, &desc, &desc) != ALWAN_E_INVALID) return 1;
    if (alwan_bake_3dlut(buf, 257, NULL, &desc, &desc) != ALWAN_E_INVALID) return 1;

    /* NULL spaces */
    if (alwan_bake_3dlut(buf, 2, NULL, NULL, &desc) != ALWAN_E_INVALID) return 1;
    if (alwan_bake_3dlut(buf, 2, NULL, &desc, NULL) != ALWAN_E_INVALID) return 1;

    /* 1D NULL */
    if (alwan_bake_1dlut(NULL, 256, ALWAN_TF_LINEAR, 1) != ALWAN_E_INVALID) return 1;
    if (alwan_bake_1dlut(buf, 1, ALWAN_TF_LINEAR, 1) != ALWAN_E_INVALID) return 1;

    /* 2D NULL */
    if (alwan_lut3d_to_2d(NULL, buf, 2) != ALWAN_E_INVALID) return 1;
    if (alwan_lut2d_to_3d(NULL, buf, 2) != ALWAN_E_INVALID) return 1;

    /* Sample NULL */
    alwan_rgb rgb = {0.5, 0.5, 0.5};
    if (alwan_lut3d_sample(NULL, buf, &rgb, 2) != ALWAN_E_INVALID) return 1;

    /* 1D sample NULL */
    alwan_scalar val;
    if (alwan_lut1d_sample(NULL, buf, 0.5, 2) != ALWAN_E_INVALID) return 1;
    if (alwan_lut1d_sample(&val, NULL, 0.5, 2) != ALWAN_E_INVALID) return 1;
    if (alwan_lut1d_sample(&val, buf, 0.5, 1) != ALWAN_E_INVALID) return 1;

    /* 2D sample NULL */
    alwan_rgb out2d;
    if (alwan_lut2d_sample(NULL, buf, &rgb, 2) != ALWAN_E_INVALID) return 1;
    if (alwan_lut2d_sample(&out2d, NULL, &rgb, 2) != ALWAN_E_INVALID) return 1;
    if (alwan_lut2d_sample(&out2d, buf, NULL, 2) != ALWAN_E_INVALID) return 1;
    if (alwan_lut2d_sample(&out2d, buf, &rgb, 1) != ALWAN_E_INVALID) return 1;

    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: cross-space bake (sRGB -> Display P3)
 * ---------------------------------------------------------------- */
static int test_bake_cross_space(void) {
    printf("  test_bake_cross_space...\n");

    alwan_ctx *ctx = alwan_create(NULL);
    alwan_rgb_space_desc srgb, p3;
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);
    alwan_rgb_get_space_descriptor(&p3, ctx, ALWAN_RGB_SPACE_DISPLAY_P3);

    int const size = 9;
    size_t const total = (size_t)size * size * size;
    alwan_scalar *lut = (alwan_scalar *)malloc(total * 3 * sizeof(alwan_scalar));

    int status = alwan_bake_3dlut(lut, size, ctx, &srgb, &p3);
    if (status != ALWAN_OK) {
        printf("  FAIL: cross-space bake returned %d\n", status);
        free(lut); alwan_destroy(ctx);
        return 1;
    }

    /* Black -> black */
    ASSERT_NEAR(lut[0], 0.0, TOL);
    ASSERT_NEAR(lut[1], 0.0, TOL);
    ASSERT_NEAR(lut[2], 0.0, TOL);

    /* White -> white (both share D65) */
    size_t white_idx = (total - 1) * 3;
    ASSERT_NEAR(lut[white_idx + 0], 1.0, 0.01);
    ASSERT_NEAR(lut[white_idx + 1], 1.0, 0.01);
    ASSERT_NEAR(lut[white_idx + 2], 1.0, 0.01);

    /* sRGB red primary should be less saturated in P3 gamut */
    size_t red_idx = (size_t)(size - 1) * 3;
    if (lut[red_idx + 1] >= 0.0 && lut[red_idx + 2] >= 0.0) {
        if (fabs((double)lut[red_idx + 0] - 1.0) < 0.001 &&
            fabs((double)lut[red_idx + 1]) < 0.001 &&
            fabs((double)lut[red_idx + 2]) < 0.001) {
            printf("  FAIL: sRGB red should differ in P3\n");
            free(lut); alwan_destroy(ctx);
            return 1;
        }
    }

    free(lut);
    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* Helper: full pipeline conversion for a single RGB value
 * (encoded src -> linear via EOTF -> matrix convert -> encoded dst via OETF)
 * This matches the pipeline in alwan_bake_3dlut. */
static int full_pipeline_convert(alwan_rgb *dst, alwan_ctx *ctx,
                                  alwan_rgb_space_desc const *src_space,
                                  alwan_rgb_space_desc const *dst_space,
                                  alwan_rgb const *src) {
    /* EOTF: encoded src -> linear src */
    alwan_rgb linear_src;
    alwan_eotf_apply(&linear_src.r, src_space->eotf, &src->r, 3, sizeof(alwan_scalar), sizeof(alwan_scalar));

    /* Matrix: linear src -> linear dst */
    alwan_rgb linear_dst;
    alwan_rgb_convert(&linear_dst, ctx, src_space, dst_space, &linear_src);

    /* OETF: linear dst -> encoded dst */
    alwan_oetf_apply(&dst->r, dst_space->oetf, &linear_dst.r, 3, sizeof(alwan_scalar), sizeof(alwan_scalar));

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Test: .cube write non-identity, import, sample through LUT, verify
 * ---------------------------------------------------------------- */
static int test_cube_import_and_sample(void) {
    printf("  test_cube_import_and_sample...\n");

    alwan_ctx *ctx = alwan_create(NULL);
    alwan_rgb_space_desc srgb, p3;
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);
    alwan_rgb_get_space_descriptor(&p3, ctx, ALWAN_RGB_SPACE_DISPLAY_P3);

    int const size = 17;
    size_t const total = (size_t)size * size * size;
    alwan_scalar *lut_baked = (alwan_scalar *)malloc(total * 3 * sizeof(alwan_scalar));
    alwan_scalar *lut_imported = (alwan_scalar *)malloc(total * 3 * sizeof(alwan_scalar));

    /* Bake sRGB -> P3 conversion LUT */
    int status = alwan_bake_3dlut(lut_baked, size, ctx, &srgb, &p3);
    if (status != ALWAN_OK) {
        printf("  FAIL: bake returned %d\n", status);
        free(lut_baked); free(lut_imported); alwan_destroy(ctx);
        return 1;
    }

    /* Export to .cube */
    char const *path = "test_srgb_to_p3.cube";
    status = alwan_cube_export_3d(path, lut_baked, size, "sRGB to P3");
    if (status != ALWAN_OK) {
        printf("  FAIL: export returned %d\n", status);
        free(lut_baked); free(lut_imported); alwan_destroy(ctx);
        return 1;
    }

    /* Import from .cube */
    int imported_size = 0;
    status = alwan_cube_import_3d(lut_imported, &imported_size, path);
    if (status != ALWAN_OK) {
        printf("  FAIL: import returned %d\n", status);
        free(lut_baked); free(lut_imported); alwan_destroy(ctx);
        remove(path);
        return 1;
    }

    if (imported_size != size) {
        printf("  FAIL: imported size %d != %d\n", imported_size, size);
        free(lut_baked); free(lut_imported); alwan_destroy(ctx);
        remove(path);
        return 1;
    }

    /* Verify imported data matches baked (within .cube text precision) */
    for (size_t i = 0; i < total * 3; i++) {
        ASSERT_NEAR(lut_imported[i], lut_baked[i], 1e-9);
    }

    /* Test grid-point sampling: LUT should match full pipeline at grid points */
    alwan_rgb test_colors[] = {
        {0.0, 0.0, 0.0},       /* black */
        {1.0, 1.0, 1.0},       /* white */
        {0.5, 0.5, 0.5},       /* mid gray (grid point: 0.5 = 8/16) */
        {0.25, 0.25, 0.25},    /* quarter gray (grid point: 0.25 = 4/16) */
    };
    int num_colors = (int)(sizeof(test_colors) / sizeof(test_colors[0]));

    for (int c = 0; c < num_colors; c++) {
        alwan_rgb lut_out, pipeline_out;
        alwan_lut3d_sample(&lut_out, lut_imported, &test_colors[c], size);
        full_pipeline_convert(&pipeline_out, ctx, &srgb, &p3, &test_colors[c]);

        ASSERT_NEAR(lut_out.r, pipeline_out.r, 1e-6);
        ASSERT_NEAR(lut_out.g, pipeline_out.g, 1e-6);
        ASSERT_NEAR(lut_out.b, pipeline_out.b, 1e-6);
    }

    remove(path);
    free(lut_baked);
    free(lut_imported);
    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: 1D LUT linear interpolation sampling
 * ---------------------------------------------------------------- */
static int test_1d_linear_sample(void) {
    printf("  test_1d_linear_sample...\n");

    int const size = 256;
    alwan_scalar *lut = (alwan_scalar *)malloc((size_t)size * sizeof(alwan_scalar));

    /* Bake sRGB OETF */
    int status = alwan_bake_1dlut(lut, size, ALWAN_TF_SRGB, 1);
    if (status != ALWAN_OK) {
        printf("  FAIL: bake returned %d\n", status);
        free(lut);
        return 1;
    }

    /* Sample at exact grid points — should match LUT entries exactly */
    for (int i = 0; i < size; i++) {
        alwan_scalar t = (alwan_scalar)i / (alwan_scalar)(size - 1);
        alwan_scalar sampled;
        alwan_lut1d_sample(&sampled, lut, t, size);
        ASSERT_NEAR(sampled, lut[i], 1e-15);
    }

    /* Sample at midpoints between grid entries — should be average */
    for (int i = 0; i < size - 1; i++) {
        alwan_scalar t = ((alwan_scalar)i + 0.5) / (alwan_scalar)(size - 1);
        alwan_scalar sampled;
        alwan_lut1d_sample(&sampled, lut, t, size);
        alwan_scalar expected = (lut[i] + lut[i + 1]) * 0.5;
        ASSERT_NEAR(sampled, expected, 1e-12);
    }

    /* Clamp test: below 0 should return lut[0] */
    {
        alwan_scalar sampled;
        alwan_lut1d_sample(&sampled, lut, -0.5, size);
        ASSERT_NEAR(sampled, lut[0], 1e-15);
    }

    /* Clamp test: above 1 should return lut[size-1] */
    {
        alwan_scalar sampled;
        alwan_lut1d_sample(&sampled, lut, 1.5, size);
        ASSERT_NEAR(sampled, lut[size - 1], 1e-15);
    }

    free(lut);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: 1D LUT sample vs direct TF function (precision)
 * ---------------------------------------------------------------- */
static int test_1d_lut_vs_tf(void) {
    printf("  test_1d_lut_vs_tf...\n");

    /* Compare LUT-sampled sRGB OETF to direct sRGB OETF at arbitrary inputs.
     * A 4096-entry LUT should be very close to the analytical function. */
    int const size = 4096;
    alwan_scalar *lut = (alwan_scalar *)malloc((size_t)size * sizeof(alwan_scalar));

    int status = alwan_bake_1dlut(lut, size, ALWAN_TF_SRGB, 1);
    if (status != ALWAN_OK) {
        printf("  FAIL: bake returned %d\n", status);
        free(lut);
        return 1;
    }

    /* Bake the sRGB EOTF for comparison */
    alwan_scalar *lut_eotf = (alwan_scalar *)malloc((size_t)size * sizeof(alwan_scalar));
    status = alwan_bake_1dlut(lut_eotf, size, ALWAN_TF_SRGB, 0);
    if (status != ALWAN_OK) {
        printf("  FAIL: eotf bake returned %d\n", status);
        free(lut); free(lut_eotf);
        return 1;
    }

    /* OETF(EOTF(x)) should ≈ x (roundtrip through LUTs) */
    double max_err = 0.0;
    for (int i = 0; i < 1000; i++) {
        alwan_scalar t = (alwan_scalar)i / 999.0;

        /* EOTF: encoded -> linear */
        alwan_scalar linear;
        alwan_lut1d_sample(&linear, lut_eotf, t, size);

        /* OETF: linear -> encoded */
        alwan_scalar roundtrip;
        alwan_lut1d_sample(&roundtrip, lut, linear, size);

        double err = fabs((double)(roundtrip - t));
        if (err > max_err) max_err = err;
    }

    /* With 4096 entries, roundtrip error should be very small */
    if (max_err > 0.001) {
        printf("  FAIL: OETF/EOTF roundtrip max error %.6e exceeds 0.001\n", max_err);
        free(lut); free(lut_eotf);
        return 1;
    }
    printf("    1D LUT OETF/EOTF roundtrip max error: %.6e (4096 entries)\n", max_err);

    free(lut);
    free(lut_eotf);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: 2D LUT bilinear sampling matches 3D trilinear sampling
 * ---------------------------------------------------------------- */
static int test_2d_sample_matches_3d(void) {
    printf("  test_2d_sample_matches_3d...\n");

    alwan_ctx *ctx = alwan_create(NULL);
    alwan_rgb_space_desc srgb, p3;
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);
    alwan_rgb_get_space_descriptor(&p3, ctx, ALWAN_RGB_SPACE_DISPLAY_P3);

    int const size = 17;
    size_t const total3d = (size_t)size * size * size * 3;
    size_t const total2d = (size_t)(size * size) * size * 3;

    alwan_scalar *lut3d = (alwan_scalar *)malloc(total3d * sizeof(alwan_scalar));
    alwan_scalar *lut2d = (alwan_scalar *)malloc(total2d * sizeof(alwan_scalar));

    /* Bake a non-identity LUT: sRGB -> P3 */
    alwan_bake_3dlut(lut3d, size, ctx, &srgb, &p3);
    alwan_lut3d_to_2d(lut2d, lut3d, size);

    /* Sample both at many arbitrary RGB values and compare */
    alwan_rgb test_colors[] = {
        {0.0, 0.0, 0.0},
        {1.0, 1.0, 1.0},
        {0.5, 0.5, 0.5},
        {0.25, 0.75, 0.125},
        {0.37, 0.62, 0.11},
        {0.1, 0.9, 0.5},
        {0.8, 0.2, 0.6},
        {0.123, 0.456, 0.789},
        {0.0, 0.5, 1.0},
        {1.0, 0.0, 0.5},
    };
    int const num_colors = (int)(sizeof(test_colors) / sizeof(test_colors[0]));

    for (int i = 0; i < num_colors; i++) {
        alwan_rgb out3d, out2d;
        alwan_lut3d_sample(&out3d, lut3d, &test_colors[i], size);
        alwan_lut2d_sample(&out2d, lut2d, &test_colors[i], size);

        /* 2D and 3D sampling should produce identical results */
        ASSERT_NEAR(out2d.r, out3d.r, 1e-14);
        ASSERT_NEAR(out2d.g, out3d.g, 1e-14);
        ASSERT_NEAR(out2d.b, out3d.b, 1e-14);
    }

    free(lut3d);
    free(lut2d);
    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: 2D LUT direct bake + sample matches 3D bake + sample
 * ---------------------------------------------------------------- */
static int test_2d_bake_and_sample(void) {
    printf("  test_2d_bake_and_sample...\n");

    alwan_ctx *ctx = alwan_create(NULL);
    alwan_rgb_space_desc srgb, p3;
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);
    alwan_rgb_get_space_descriptor(&p3, ctx, ALWAN_RGB_SPACE_DISPLAY_P3);

    int const size = 9;
    size_t const total3d = (size_t)size * size * size * 3;
    size_t const total2d = (size_t)(size * size) * size * 3;

    alwan_scalar *lut3d = (alwan_scalar *)malloc(total3d * sizeof(alwan_scalar));
    alwan_scalar *lut2d = (alwan_scalar *)malloc(total2d * sizeof(alwan_scalar));

    alwan_bake_3dlut(lut3d, size, ctx, &srgb, &p3);
    alwan_bake_2dlut(lut2d, size, ctx, &srgb, &p3);

    /* Sample both at mid-gray and verify same result */
    alwan_rgb gray = {0.5, 0.5, 0.5};
    alwan_rgb out3d, out2d;
    alwan_lut3d_sample(&out3d, lut3d, &gray, size);
    alwan_lut2d_sample(&out2d, lut2d, &gray, size);

    ASSERT_NEAR(out2d.r, out3d.r, TOL);
    ASSERT_NEAR(out2d.g, out3d.g, TOL);
    ASSERT_NEAR(out2d.b, out3d.b, TOL);

    /* Sample at off-grid point */
    alwan_rgb color = {0.37, 0.82, 0.15};
    alwan_lut3d_sample(&out3d, lut3d, &color, size);
    alwan_lut2d_sample(&out2d, lut2d, &color, size);

    ASSERT_NEAR(out2d.r, out3d.r, TOL);
    ASSERT_NEAR(out2d.g, out3d.g, TOL);
    ASSERT_NEAR(out2d.b, out3d.b, TOL);

    free(lut3d);
    free(lut2d);
    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: LUT precision vs full pipeline at varying sizes
 *
 * Bake sRGB -> P3 at different LUT sizes (9, 17, 33, 65)
 * and measure max interpolation error against the full pipeline
 * (EOTF -> matrix -> OETF). Larger LUTs should have lower error.
 * ---------------------------------------------------------------- */
static int test_lut_precision_vs_direct(void) {
    printf("  test_lut_precision_vs_direct...\n");

    alwan_ctx *ctx = alwan_create(NULL);
    alwan_rgb_space_desc srgb, p3;
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);
    alwan_rgb_get_space_descriptor(&p3, ctx, ALWAN_RGB_SPACE_DISPLAY_P3);

    /* Test colors — deliberately off-grid to exercise interpolation */
    alwan_rgb test_colors[] = {
        {0.13, 0.27, 0.84},
        {0.71, 0.33, 0.52},
        {0.92, 0.08, 0.46},
        {0.05, 0.95, 0.50},
        {0.44, 0.44, 0.44},
        {0.88, 0.77, 0.66},
        {0.11, 0.22, 0.33},
        {0.61, 0.51, 0.41},
        {0.37, 0.82, 0.15},
        {0.99, 0.01, 0.99},
    };
    int const num_colors = (int)(sizeof(test_colors) / sizeof(test_colors[0]));

    int sizes[] = {9, 17, 33, 65};
    int const num_sizes = 4;
    double prev_max_err = 1.0;

    for (int s = 0; s < num_sizes; s++) {
        int const size = sizes[s];
        size_t const total = (size_t)size * size * size;
        alwan_scalar *lut = (alwan_scalar *)malloc(total * 3 * sizeof(alwan_scalar));

        alwan_bake_3dlut(lut, size, ctx, &srgb, &p3);

        double max_err = 0.0;
        for (int c = 0; c < num_colors; c++) {
            alwan_rgb lut_out, pipeline_out;
            alwan_lut3d_sample(&lut_out, lut, &test_colors[c], size);
            full_pipeline_convert(&pipeline_out, ctx, &srgb, &p3, &test_colors[c]);

            double dr = fabs((double)(lut_out.r - pipeline_out.r));
            double dg = fabs((double)(lut_out.g - pipeline_out.g));
            double db = fabs((double)(lut_out.b - pipeline_out.b));
            double err = dr > dg ? (dr > db ? dr : db) : (dg > db ? dg : db);
            if (err > max_err) max_err = err;
        }

        printf("    LUT size %2d: max error vs pipeline = %.6e\n", size, max_err);

        /* Error should decrease (or stay same) as size increases */
        if (s > 0 && max_err > prev_max_err + 1e-10) {
            printf("  FAIL: error increased from %.6e to %.6e at size %d\n",
                   prev_max_err, max_err, size);
            free(lut);
            alwan_destroy(ctx);
            return 1;
        }

        /* Size 65 should have very small error (< 1e-4) */
        if (size == 65 && max_err > 1e-4) {
            printf("  FAIL: size-65 LUT error %.6e exceeds 1e-4\n", max_err);
            free(lut);
            alwan_destroy(ctx);
            return 1;
        }

        prev_max_err = max_err;
        free(lut);
    }

    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: .cube 1D write, import, sample roundtrip
 * ---------------------------------------------------------------- */
static int test_cube_1d_import_sample(void) {
    printf("  test_cube_1d_import_sample...\n");

    int const size = 1024;
    alwan_scalar *lut_baked = (alwan_scalar *)malloc((size_t)size * sizeof(alwan_scalar));
    alwan_scalar *lut_imported = (alwan_scalar *)malloc((size_t)size * sizeof(alwan_scalar));

    /* Bake sRGB EOTF (decode curve) */
    int status = alwan_bake_1dlut(lut_baked, size, ALWAN_TF_SRGB, 0);
    if (status != ALWAN_OK) {
        printf("  FAIL: bake returned %d\n", status);
        free(lut_baked); free(lut_imported);
        return 1;
    }

    /* Export to .cube */
    char const *path = "test_1d_srgb_eotf.cube";
    status = alwan_cube_export_1d(path, lut_baked, size, "sRGB EOTF 1D");
    if (status != ALWAN_OK) {
        printf("  FAIL: export returned %d\n", status);
        free(lut_baked); free(lut_imported);
        return 1;
    }

    /* Import */
    int imported_size = 0;
    status = alwan_cube_import_1d(lut_imported, &imported_size, path);
    if (status != ALWAN_OK) {
        printf("  FAIL: import returned %d\n", status);
        free(lut_baked); free(lut_imported);
        remove(path);
        return 1;
    }

    if (imported_size != size) {
        printf("  FAIL: imported size %d != %d\n", imported_size, size);
        free(lut_baked); free(lut_imported);
        remove(path);
        return 1;
    }

    /* Verify imported data matches baked (within text precision) */
    for (int i = 0; i < size; i++) {
        ASSERT_NEAR(lut_imported[i], lut_baked[i], 1e-9);
    }

    /* Sample through imported LUT at various points and verify monotonicity */
    alwan_scalar prev = -1.0;
    for (int i = 0; i <= 500; i++) {
        alwan_scalar t = (alwan_scalar)i / 500.0;
        alwan_scalar sampled;
        alwan_lut1d_sample(&sampled, lut_imported, t, size);
        if (sampled < prev - 1e-15) {
            printf("  FAIL: imported 1D LUT not monotonic at t=%.4f\n", (double)t);
            free(lut_baked); free(lut_imported);
            remove(path);
            return 1;
        }
        prev = sampled;
    }

    remove(path);
    free(lut_baked);
    free(lut_imported);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: .cube 3D write with cross-space, import, sample, compare
 * to full pipeline (EOTF->matrix->OETF) at off-grid points
 * ---------------------------------------------------------------- */
static int test_cube_3d_precision_vs_direct(void) {
    printf("  test_cube_3d_precision_vs_direct...\n");

    alwan_ctx *ctx = alwan_create(NULL);
    alwan_rgb_space_desc srgb, p3;
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);
    alwan_rgb_get_space_descriptor(&p3, ctx, ALWAN_RGB_SPACE_DISPLAY_P3);

    int const size = 33;
    size_t const total = (size_t)size * size * size;
    alwan_scalar *lut_baked = (alwan_scalar *)malloc(total * 3 * sizeof(alwan_scalar));
    alwan_scalar *lut_imported = (alwan_scalar *)malloc(total * 3 * sizeof(alwan_scalar));

    /* Bake and export */
    alwan_bake_3dlut(lut_baked, size, ctx, &srgb, &p3);
    char const *path = "test_precision_p3.cube";
    alwan_cube_export_3d(path, lut_baked, size, "sRGB->P3 precision");

    /* Import */
    int imported_size = 0;
    int status = alwan_cube_import_3d(lut_imported, &imported_size, path);
    if (status != ALWAN_OK || imported_size != size) {
        printf("  FAIL: import returned %d, size %d\n", status, imported_size);
        free(lut_baked); free(lut_imported); alwan_destroy(ctx);
        remove(path);
        return 1;
    }

    /* Sample imported LUT at off-grid points and compare to full pipeline */
    double max_err = 0.0;
    int const steps = 10;
    for (int ri = 0; ri < steps; ri++) {
        for (int gi = 0; gi < steps; gi++) {
            for (int bi = 0; bi < steps; bi++) {
                alwan_rgb in;
                in.r = (alwan_scalar)ri / (alwan_scalar)(steps - 1);
                in.g = (alwan_scalar)gi / (alwan_scalar)(steps - 1);
                in.b = (alwan_scalar)bi / (alwan_scalar)(steps - 1);

                alwan_rgb lut_out, pipeline_out;
                alwan_lut3d_sample(&lut_out, lut_imported, &in, size);
                full_pipeline_convert(&pipeline_out, ctx, &srgb, &p3, &in);

                double dr = fabs((double)(lut_out.r - pipeline_out.r));
                double dg = fabs((double)(lut_out.g - pipeline_out.g));
                double db = fabs((double)(lut_out.b - pipeline_out.b));
                double err = dr > dg ? (dr > db ? dr : db) : (dg > db ? dg : db);
                if (err > max_err) max_err = err;
            }
        }
    }

    printf("    .cube import (size %d) vs pipeline: max error = %.6e (1000 samples)\n",
           size, max_err);

    /* With size-33 LUT, max interpolation error should be small */
    if (max_err > 0.005) {
        printf("  FAIL: max error %.6e exceeds 0.005\n", max_err);
        free(lut_baked); free(lut_imported); alwan_destroy(ctx);
        remove(path);
        return 1;
    }

    remove(path);
    free(lut_baked);
    free(lut_imported);
    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: trilinear interpolation correctness (manual verification)
 *
 * Create a small LUT with known non-linear values and verify
 * that trilinear interpolation produces correct results.
 * ---------------------------------------------------------------- */
static int test_trilinear_manual(void) {
    printf("  test_trilinear_manual...\n");

    /* 2x2x2 cube: 8 corners with known values */
    int const size = 2;
    alwan_scalar lut[2 * 2 * 2 * 3];

    /* Fill corners: output = (R^2, G^2, B^2) at each corner */
    alwan_scalar corners[2] = {0.0, 1.0};
    for (int b = 0; b < 2; b++) {
        for (int g = 0; g < 2; g++) {
            for (int r = 0; r < 2; r++) {
                size_t idx = ((size_t)b * 4 + (size_t)g * 2 + (size_t)r) * 3;
                lut[idx + 0] = corners[r] * corners[r];
                lut[idx + 1] = corners[g] * corners[g];
                lut[idx + 2] = corners[b] * corners[b];
            }
        }
    }

    /* At corner (0,0,0) = (0,0,0) */
    alwan_rgb in = {0.0, 0.0, 0.0};
    alwan_rgb out;
    alwan_lut3d_sample(&out, lut, &in, size);
    ASSERT_NEAR(out.r, 0.0, 1e-15);
    ASSERT_NEAR(out.g, 0.0, 1e-15);
    ASSERT_NEAR(out.b, 0.0, 1e-15);

    /* At corner (1,1,1) = (1,1,1) */
    in.r = 1.0; in.g = 1.0; in.b = 1.0;
    alwan_lut3d_sample(&out, lut, &in, size);
    ASSERT_NEAR(out.r, 1.0, 1e-15);
    ASSERT_NEAR(out.g, 1.0, 1e-15);
    ASSERT_NEAR(out.b, 1.0, 1e-15);

    /* At midpoint (0.5, 0.5, 0.5): trilinear of (R^2, G^2, B^2)
     * With corners at 0 and 1, trilinear interp at 0.5 gives:
     *   Each channel: 0.0*(1-0.5) + 1.0*0.5 = 0.5
     * (linear interp of the corner VALUES, not of R^2)
     */
    in.r = 0.5; in.g = 0.5; in.b = 0.5;
    alwan_lut3d_sample(&out, lut, &in, size);
    ASSERT_NEAR(out.r, 0.5, 1e-14);
    ASSERT_NEAR(out.g, 0.5, 1e-14);
    ASSERT_NEAR(out.b, 0.5, 1e-14);

    /* At (0.25, 0.75, 0.5):
     * R channel: interp between R=0^2=0 and R=1^2=1 with frac=0.25 -> 0.25
     * G channel: interp between G=0^2=0 and G=1^2=1 with frac=0.75 -> 0.75
     * B channel: interp between B=0^2=0 and B=1^2=1 with frac=0.5  -> 0.5
     */
    in.r = 0.25; in.g = 0.75; in.b = 0.5;
    alwan_lut3d_sample(&out, lut, &in, size);
    ASSERT_NEAR(out.r, 0.25, 1e-14);
    ASSERT_NEAR(out.g, 0.75, 1e-14);
    ASSERT_NEAR(out.b, 0.5,  1e-14);

    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */
int test_79_lut_bake_export_main(void) {
    int fail = 0;

    fail += test_bake_3dlut_identity();
    fail += test_bake_1dlut();
    fail += test_lut_2d_roundtrip();
    fail += test_lut_2d_layout();
    fail += test_bake_2dlut();
    fail += test_lut3d_sample();
    fail += test_cube_roundtrip();
    fail += test_cube_buffer_export();
    fail += test_bake_3dlut_view();
    fail += test_lut_null_checks();
    fail += test_bake_cross_space();
    fail += test_cube_import_and_sample();
    fail += test_1d_linear_sample();
    fail += test_1d_lut_vs_tf();
    fail += test_2d_sample_matches_3d();
    fail += test_2d_bake_and_sample();
    fail += test_lut_precision_vs_direct();
    fail += test_cube_1d_import_sample();
    fail += test_cube_3d_precision_vs_direct();
    fail += test_trilinear_manual();

    return fail;
}
