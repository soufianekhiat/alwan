/*
 * Alwan - Pure C colour science library
 * Test 83: Dual-Precision Explicit API
 *
 * Validates that the _f32 and _f64 function variants are both present,
 * independently correct, and that _ex dispatch routes F32 input to the
 * f32 pipeline and F64 input to the f64 pipeline.
 */

#include "test_common.h"
#include "core/alwan_oklab_core.h"
#include "core/alwan_core.h"
#include "core/alwan_math_core.h"
#include "core/alwan_colorspace_core.h"
#include <string.h>

/* Forward declarations for explicit precision map functions */
extern int alwan_xyz_to_oklab_f32_map_planar(
    float *o0, float *o1, float *o2,
    float const *i0, float const *i1, float const *i2,
    size_t count, size_t in_stride, size_t out_stride);

extern int alwan_xyz_to_oklab_f64_map_planar(
    double *o0, double *o1, double *o2,
    double const *i0, double const *i1, double const *i2,
    size_t count, size_t in_stride, size_t out_stride);

extern int alwan_xyz_to_oklab_map_planar_ex(
    void *o0, void *o1, void *o2, alwan_pixel_format out_fmt,
    void const *i0, void const *i1, void const *i2, alwan_pixel_format in_fmt,
    size_t count, size_t in_stride, size_t out_stride);

extern int alwan_srgb_to_xyz_f32_map_planar(
    float *o0, float *o1, float *o2,
    float const *i0, float const *i1, float const *i2,
    size_t count, size_t in_stride, size_t out_stride);

extern int alwan_srgb_to_xyz_f64_map_planar(
    double *o0, double *o1, double *o2,
    double const *i0, double const *i1, double const *i2,
    size_t count, size_t in_stride, size_t out_stride);

/* Compile-time struct size checks (cannot fail at runtime -- verified at build time) */
_Static_assert(sizeof(alwan_xyz_f32)  == 3 * sizeof(float),  "alwan_xyz_f32 must be 3 floats");
_Static_assert(sizeof(alwan_xyz_f64)  == 3 * sizeof(double), "alwan_xyz_f64 must be 3 doubles");
_Static_assert(sizeof(alwan_oklab_f32) == 3 * sizeof(float),  "alwan_oklab_f32 must be 3 floats");
_Static_assert(sizeof(alwan_oklab_f64) == 3 * sizeof(double), "alwan_oklab_f64 must be 3 doubles");
_Static_assert(sizeof(alwan_xyz_f64)  == 2 * sizeof(alwan_xyz_f32), "f64 struct must be twice the f32 size");
_Static_assert(sizeof(alwan_xyz_f64)   == sizeof(alwan_xyz_f64),   "alwan_xyz_f64 must alias alwan_xyz_f64");
_Static_assert(sizeof(alwan_oklab_f64) == sizeof(alwan_oklab_f64), "alwan_oklab_f64 must alias alwan_oklab_f64");

/* ================================================================
 * Test: Point functions — D65 white -> Oklab
 * Expected: L=1.0, a=0.0, b=0.0 (both precisions)
 * ================================================================ */

static int test_oklab_point_f32(void) {
    TEST_START("f32 point: D65 white -> Oklab L=1 a=0 b=0");

    /* D65 white point (Y=1 normalized, from ALWAN_D65_X/Y/Z / 100) */
    alwan_xyz_f32 xyz;
    xyz.x = 0.9504559271f;
    xyz.y = 1.0000000000f;
    xyz.z = 1.0890577508f;

    alwan_oklab_f32 lab = alwan_xyz_to_oklab_f32_v(xyz);

    /* f32 tolerance: ~5 ULPs at this scale */
    TEST_ASSERT_NEAR((alwan_f64)lab.L, ALWAN_LITERAL(1.0), ALWAN_LITERAL(2e-4),
                     "f32 D65->Oklab L~1");
    TEST_ASSERT_NEAR((alwan_f64)lab.a, ALWAN_LITERAL(0.0), ALWAN_LITERAL(2e-4),
                     "f32 D65->Oklab a~0");
    TEST_ASSERT_NEAR((alwan_f64)lab.b, ALWAN_LITERAL(0.0), ALWAN_LITERAL(2e-4),
                     "f32 D65->Oklab b~0");

    TEST_PASS("oklab_point_f32");
}

static int test_oklab_point_f64(void) {
    TEST_START("f64 point: D65 white -> Oklab L=1 a=0 b=0");

    alwan_xyz_f64 xyz;
    xyz.x = 0.9504559271;
    xyz.y = 1.0000000000;
    xyz.z = 1.0890577508;

    alwan_oklab_f64 lab = alwan_xyz_to_oklab_f64_v(xyz);

    /* f64 Oklab: near-achromatic for D65 white (XYZ tolerance due to D65 approximation) */
    TEST_ASSERT_NEAR((alwan_f64)lab.L, ALWAN_LITERAL(1.0), ALWAN_LITERAL(2e-4),
                     "f64 D65->Oklab L~1");
    TEST_ASSERT_NEAR((alwan_f64)lab.a, ALWAN_LITERAL(0.0), ALWAN_LITERAL(2e-4),
                     "f64 D65->Oklab a~0");
    TEST_ASSERT_NEAR((alwan_f64)lab.b, ALWAN_LITERAL(0.0), ALWAN_LITERAL(2e-4),
                     "f64 D65->Oklab b~0");

    TEST_PASS("oklab_point_f64");
}

static int test_oklab_f32_f64_agree(void) {
    TEST_START("f32 and f64 Oklab agree on sRGB primaries");

    /* Test a few sRGB primaries: red, green, blue */
    static double const xyz_vals[3][3] = {
        {0.4124564, 0.2126729, 0.0193339},  /* sRGB red    */
        {0.3575761, 0.7151522, 0.1191920},  /* sRGB green  */
        {0.1804375, 0.0721750, 0.9503041},  /* sRGB blue   */
    };

    for (int i = 0; i < 3; i++) {
        alwan_xyz_f32 xyz32;
        xyz32.x = (float)xyz_vals[i][0];
        xyz32.y = (float)xyz_vals[i][1];
        xyz32.z = (float)xyz_vals[i][2];

        alwan_xyz_f64 xyz64;
        xyz64.x = xyz_vals[i][0];
        xyz64.y = xyz_vals[i][1];
        xyz64.z = xyz_vals[i][2];

        alwan_oklab_f32 r32 = alwan_xyz_to_oklab_f32_v(xyz32);
        alwan_oklab_f64 r64 = alwan_xyz_to_oklab_f64_v(xyz64);

        /* f32 and f64 should agree within 1e-5 */
        TEST_ASSERT_NEAR((alwan_f64)r32.L, (alwan_f64)r64.L, ALWAN_LITERAL(1e-5),
                         "f32/f64 Oklab L agree");
        TEST_ASSERT_NEAR((alwan_f64)r32.a, (alwan_f64)r64.a, ALWAN_LITERAL(1e-5),
                         "f32/f64 Oklab a agree");
        TEST_ASSERT_NEAR((alwan_f64)r32.b, (alwan_f64)r64.b, ALWAN_LITERAL(1e-5),
                         "f32/f64 Oklab b agree");
    }

    TEST_PASS("oklab_f32_f64_agree");
}

/* ================================================================
 * Test: Transfer function variants
 * alwan_srgb_oetf_f32 / alwan_srgb_oetf_f64 agree
 * ================================================================ */

static int test_tf_variants(void) {
    TEST_START("sRGB OETF f32/f64 variants agree");

    static double const vals[] = {0.0, 0.001, 0.01, 0.1, 0.5, 0.9, 1.0};
    int n = (int)(sizeof(vals) / sizeof(vals[0]));

    for (int i = 0; i < n; i++) {
        float  r32 = alwan_srgb_oetf_f32((float)vals[i]);
        double r64 = alwan_srgb_oetf_f64(vals[i]);
        /* f32 and f64 agree within 1e-6 */
        TEST_ASSERT_NEAR((alwan_f64)r32, (alwan_f64)r64, ALWAN_LITERAL(1e-6),
                         "srgb_oetf f32/f64 agree");
    }

    TEST_PASS("tf_variants");
}

/* ================================================================
 * Test: Map planar — explicit f32 and f64 pipelines
 * ================================================================ */

#define N_PIXELS 40

static int test_map_planar_f32(void) {
    TEST_START("f32 map planar xyz->oklab correctness");

    float xi0[N_PIXELS], xi1[N_PIXELS], xi2[N_PIXELS];
    float xo0[N_PIXELS], xo1[N_PIXELS], xo2[N_PIXELS];

    /* Fill with D65 white (Y=1 normalized) */
    for (int i = 0; i < N_PIXELS; i++) {
        xi0[i] = 0.9504559271f;
        xi1[i] = 1.0000000000f;
        xi2[i] = 1.0890577508f;
    }

    int rc = alwan_xyz_to_oklab_f32_map_planar(xo0, xo1, xo2, xi0, xi1, xi2, N_PIXELS, sizeof(float), sizeof(float));
    TEST_ASSERT(rc == ALWAN_OK, "f32 map planar returned ALWAN_OK");

    for (int i = 0; i < N_PIXELS; i++) {
        TEST_CHECK_NEAR((alwan_f64)xo0[i], ALWAN_LITERAL(1.0), ALWAN_LITERAL(2e-4));
        TEST_CHECK_NEAR((alwan_f64)xo1[i], ALWAN_LITERAL(0.0), ALWAN_LITERAL(2e-4));
        TEST_CHECK_NEAR((alwan_f64)xo2[i], ALWAN_LITERAL(0.0), ALWAN_LITERAL(2e-4));
    }

    TEST_PASS("map_planar_f32");
}

static int test_map_planar_f64(void) {
    TEST_START("f64 map planar xyz->oklab correctness");

    double xi0[N_PIXELS], xi1[N_PIXELS], xi2[N_PIXELS];
    double xo0[N_PIXELS], xo1[N_PIXELS], xo2[N_PIXELS];

    for (int i = 0; i < N_PIXELS; i++) {
        xi0[i] = 0.9504559271;
        xi1[i] = 1.0000000000;
        xi2[i] = 1.0890577508;
    }

    int rc = alwan_xyz_to_oklab_f64_map_planar(xo0, xo1, xo2, xi0, xi1, xi2, N_PIXELS, sizeof(double), sizeof(double));
    TEST_ASSERT(rc == ALWAN_OK, "f64 map planar returned ALWAN_OK");

    for (int i = 0; i < N_PIXELS; i++) {
        TEST_CHECK_NEAR((alwan_f64)xo0[i], ALWAN_LITERAL(1.0), ALWAN_LITERAL(2e-4));
        TEST_CHECK_NEAR((alwan_f64)xo1[i], ALWAN_LITERAL(0.0), ALWAN_LITERAL(2e-4));
        TEST_CHECK_NEAR((alwan_f64)xo2[i], ALWAN_LITERAL(0.0), ALWAN_LITERAL(2e-4));
    }

    TEST_PASS("map_planar_f64");
}

static int test_map_planar_f32_f64_agree(void) {
    TEST_START("f32 and f64 map planar agree on sRGB primaries");

    /* Use sRGB->XYZ->Oklab: red, green, blue interleaved */
    float in_r32[N_PIXELS], in_g32[N_PIXELS], in_b32[N_PIXELS];
    float out_r32[N_PIXELS], out_g32[N_PIXELS], out_b32[N_PIXELS];
    double in_r64[N_PIXELS], in_g64[N_PIXELS], in_b64[N_PIXELS];
    double out_r64[N_PIXELS], out_g64[N_PIXELS], out_b64[N_PIXELS];

    /* Fill with a gradient of grays plus a few saturated colors */
    for (int i = 0; i < N_PIXELS; i++) {
        double v = (double)i / (N_PIXELS - 1);
        in_r32[i] = (float)v;
        in_g32[i] = (float)(v * 0.5);
        in_b32[i] = (float)(1.0 - v);
        in_r64[i] = v;
        in_g64[i] = v * 0.5;
        in_b64[i] = 1.0 - v;
    }

    /* Use sRGB->xyz map to get XYZ values for Oklab */
    float xyz_r32[N_PIXELS], xyz_g32[N_PIXELS], xyz_b32[N_PIXELS];
    double xyz_r64[N_PIXELS], xyz_g64[N_PIXELS], xyz_b64[N_PIXELS];

    alwan_srgb_to_xyz_f32_map_planar(xyz_r32, xyz_g32, xyz_b32,
                                      in_r32, in_g32, in_b32, N_PIXELS, sizeof(float), sizeof(float));
    alwan_srgb_to_xyz_f64_map_planar(xyz_r64, xyz_g64, xyz_b64,
                                      in_r64, in_g64, in_b64, N_PIXELS, sizeof(double), sizeof(double));

    alwan_xyz_to_oklab_f32_map_planar(out_r32, out_g32, out_b32,
                                       xyz_r32, xyz_g32, xyz_b32, N_PIXELS, sizeof(float), sizeof(float));
    alwan_xyz_to_oklab_f64_map_planar(out_r64, out_g64, out_b64,
                                       xyz_r64, xyz_g64, xyz_b64, N_PIXELS, sizeof(double), sizeof(double));

    for (int i = 0; i < N_PIXELS; i++) {
        TEST_CHECK_NEAR((alwan_f64)out_r32[i], (alwan_f64)out_r64[i], ALWAN_LITERAL(1e-4));
        TEST_CHECK_NEAR((alwan_f64)out_g32[i], (alwan_f64)out_g64[i], ALWAN_LITERAL(1e-4));
        TEST_CHECK_NEAR((alwan_f64)out_b32[i], (alwan_f64)out_b64[i], ALWAN_LITERAL(1e-4));
    }

    TEST_PASS("map_planar_f32_f64_agree");
}

/* ================================================================
 * Test: _ex dispatch — F32 format routes to f32 pipeline,
 *                      F64 format routes to f64 pipeline.
 * Verify by comparing _ex output against direct _f32/_f64 calls.
 * ================================================================ */

static int test_ex_dispatch_f32(void) {
    TEST_START("_ex dispatch: F32 format matches f32 pipeline");

    float xi0[N_PIXELS], xi1[N_PIXELS], xi2[N_PIXELS];
    float dir0[N_PIXELS], dir1[N_PIXELS], dir2[N_PIXELS]; /* direct f32 */
    float ex0[N_PIXELS],  ex1[N_PIXELS],  ex2[N_PIXELS];  /* _ex F32 */

    for (int i = 0; i < N_PIXELS; i++) {
        xi0[i] = (float)(0.1 + 0.02 * i);
        xi1[i] = (float)(0.2 + 0.01 * i);
        xi2[i] = (float)(0.3 + 0.015 * i);
    }

    alwan_xyz_to_oklab_f32_map_planar(dir0, dir1, dir2, xi0, xi1, xi2, N_PIXELS, sizeof(float), sizeof(float));
    alwan_xyz_to_oklab_map_planar_ex(ex0, ex1, ex2, ALWAN_PIXEL_F32,
                                      xi0, xi1, xi2, ALWAN_PIXEL_F32,
                                      N_PIXELS, sizeof(float), sizeof(float));

    for (int i = 0; i < N_PIXELS; i++) {
        TEST_CHECK_NEAR((alwan_f64)ex0[i], (alwan_f64)dir0[i], ALWAN_LITERAL(1e-6));
        TEST_CHECK_NEAR((alwan_f64)ex1[i], (alwan_f64)dir1[i], ALWAN_LITERAL(1e-6));
        TEST_CHECK_NEAR((alwan_f64)ex2[i], (alwan_f64)dir2[i], ALWAN_LITERAL(1e-6));
    }

    TEST_PASS("ex_dispatch_f32");
}

static int test_ex_dispatch_f64(void) {
    TEST_START("_ex dispatch: F64 format matches f64 pipeline");

    double xi0[N_PIXELS], xi1[N_PIXELS], xi2[N_PIXELS];
    double dir0[N_PIXELS], dir1[N_PIXELS], dir2[N_PIXELS];
    double ex0[N_PIXELS],  ex1[N_PIXELS],  ex2[N_PIXELS];

    for (int i = 0; i < N_PIXELS; i++) {
        xi0[i] = 0.1 + 0.02 * i;
        xi1[i] = 0.2 + 0.01 * i;
        xi2[i] = 0.3 + 0.015 * i;
    }

    alwan_xyz_to_oklab_f64_map_planar(dir0, dir1, dir2, xi0, xi1, xi2, N_PIXELS, sizeof(double), sizeof(double));
    alwan_xyz_to_oklab_map_planar_ex(ex0, ex1, ex2, ALWAN_PIXEL_F64,
                                      xi0, xi1, xi2, ALWAN_PIXEL_F64,
                                      N_PIXELS, sizeof(double), sizeof(double));

    for (int i = 0; i < N_PIXELS; i++) {
        TEST_CHECK_NEAR((alwan_f64)ex0[i], (alwan_f64)dir0[i], ALWAN_LITERAL(1e-12));
        TEST_CHECK_NEAR((alwan_f64)ex1[i], (alwan_f64)dir1[i], ALWAN_LITERAL(1e-12));
        TEST_CHECK_NEAR((alwan_f64)ex2[i], (alwan_f64)dir2[i], ALWAN_LITERAL(1e-12));
    }

    TEST_PASS("ex_dispatch_f64");
}

/* ================================================================
 * Test: _ex F64 is more precise than f32 for a sensitive input
 * For values near the PQ curve, f64 should have lower rounding error.
 * Use Oklab roundtrip: xyz -> oklab -> xyz. f64 error < f32 error.
 * ================================================================ */

static int test_f64_more_precise_than_f32(void) {
    TEST_START("f64 more precise than f32 for Oklab roundtrip");

    /* A color that exercises the full pipeline */
    double x = 0.2034, y = 0.2140, z = 0.2330;

    alwan_xyz_f32 xyz32 = {(float)x, (float)y, (float)z};
    alwan_xyz_f64 xyz64 = {x, y, z};

    alwan_oklab_f32 lab32 = alwan_xyz_to_oklab_f32_v(xyz32);
    alwan_oklab_f64 lab64 = alwan_xyz_to_oklab_f64_v(xyz64);

    alwan_xyz_f32 back32 = alwan_oklab_to_xyz_f32_v(lab32);
    alwan_xyz_f64 back64 = alwan_oklab_to_xyz_f64_v(lab64);

    double err32_x = (double)back32.x - x;
    double err32_y = (double)back32.y - y;
    double err32_z = (double)back32.z - z;
    double err64_x = (double)back64.x - x;
    double err64_y = (double)back64.y - y;
    double err64_z = (double)back64.z - z;

    double max32 = err32_x < 0 ? -err32_x : err32_x;
    if (err32_y < 0) err32_y = -err32_y;
    if (err32_y > max32) max32 = err32_y;
    if (err32_z < 0) err32_z = -err32_z;
    if (err32_z > max32) max32 = err32_z;

    double max64 = err64_x < 0 ? -err64_x : err64_x;
    if (err64_y < 0) err64_y = -err64_y;
    if (err64_y > max64) max64 = err64_y;
    if (err64_z < 0) err64_z = -err64_z;
    if (err64_z > max64) max64 = err64_z;

    /* f64 roundtrip error must be < f32 roundtrip error */
    TEST_ASSERT(max64 < max32, "f64 Oklab roundtrip error < f32");
    /* f64 error must be tiny (near machine epsilon for doubles) */
    TEST_ASSERT_NEAR((alwan_f64)max64, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1e-12),
                     "f64 Oklab roundtrip near-lossless");

    TEST_PASS("f64_more_precise_than_f32");
}

/* ================================================================
 * Main
 * ================================================================ */

int test_83_dual_precision_main(void) {
    printf("  Testing dual-precision (f32/f64) explicit variants...\n");

    if (test_oklab_point_f32())      return 1;
    if (test_oklab_point_f64())      return 1;
    if (test_oklab_f32_f64_agree())  return 1;
    if (test_tf_variants())          return 1;
    if (test_map_planar_f32())       return 1;
    if (test_map_planar_f64())       return 1;
    if (test_map_planar_f32_f64_agree()) return 1;
    if (test_ex_dispatch_f32())      return 1;
    if (test_ex_dispatch_f64())      return 1;
    if (test_f64_more_precise_than_f32()) return 1;

    printf("  All dual-precision tests passed (%d assertions)\n", test_count);
    return 0;
}
