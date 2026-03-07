/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test suite 70: Comprehensive Planar Map Validation
 *
 * Validates planar map functions across 4 test categories:
 *   A: _v (per-pixel API) vs _map_planar (batch planar)
 *   B: _map_interleave vs _map_planar (layout consistency)
 *   C: _v (per-pixel API) vs _map_interleave (batch interleaved)
 *   D: _map_planar_ex typed format (U8, U16, F32, F64)
 */

#include "test_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* D65 white point from CSV (Y=1 normalized) */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const g_d65_xyz_y1[] = {
#include "reference_values/test_d65_white.csv"
};
ALWAN_DIAG_POP

/* ================================================================
 * Grid generation (format-independent)
 *
 * Uses division by (N-1) so grid values are NOT multiples of 1/255.
 * This exercises quantization for all typed formats realistically.
 * ================================================================ */

#define PM_GRID_1D 20
#define PM_COUNT (PM_GRID_1D * PM_GRID_1D * PM_GRID_1D) /* 8000 */

static void generate_unit_grid(alwan_scalar *out) {
    alwan_scalar const div = (alwan_scalar)(PM_GRID_1D - 1);
    size_t idx = 0;
    for (int r = 0; r < PM_GRID_1D; r++)
        for (int g = 0; g < PM_GRID_1D; g++)
            for (int b = 0; b < PM_GRID_1D; b++, idx++) {
                out[idx*3+0] = (alwan_scalar)r / div;
                out[idx*3+1] = (alwan_scalar)g / div;
                out[idx*3+2] = (alwan_scalar)b / div;
            }
}

static void generate_lab_grid(alwan_scalar *out) {
    alwan_scalar const div = (alwan_scalar)(PM_GRID_1D - 1);
    size_t idx = 0;
    for (int r = 0; r < PM_GRID_1D; r++)
        for (int g = 0; g < PM_GRID_1D; g++)
            for (int b = 0; b < PM_GRID_1D; b++, idx++) {
                out[idx*3+0] = (alwan_scalar)r / div * ALWAN_LITERAL(100.0);
                out[idx*3+1] = ((alwan_scalar)g / div - ALWAN_LITERAL(0.5)) * ALWAN_LITERAL(256.0);
                out[idx*3+2] = ((alwan_scalar)b / div - ALWAN_LITERAL(0.5)) * ALWAN_LITERAL(256.0);
            }
}

/* ================================================================
 * Tolerances (type-dependent)
 *
 * _v vs _map_planar: both scalar per-pixel, should be exact → ALWAN_TEST_TOLERANCE
 * _v vs _map_interleave / interleave vs planar: SIMD fmadd rounding → TOL_SIMD
 * _ex tests: quantization + precision per pixel format
 * ================================================================ */

#ifdef ALWAN_SCALAR_IS_FLOAT
#define TOL_SIMD ALWAN_LITERAL(1e-4)
#else
#define TOL_SIMD ALWAN_LITERAL(1e-8)
#endif

#define TOL_U8   ALWAN_LITERAL(5e-3)
#define TOL_U16  ALWAN_LITERAL(3e-5)
#define TOL_F32  ALWAN_LITERAL(5e-5)
#define TOL_F64  ALWAN_LITERAL(1e-4)

static alwan_scalar tol_for_fmt(alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8:  return TOL_U8;
    case ALWAN_PIXEL_U16: return TOL_U16;
    case ALWAN_PIXEL_F32: return TOL_F32;
    case ALWAN_PIXEL_F64: return TOL_F64;
    }
    return TOL_SIMD;
}

static char const *fmt_name(alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8:  return "U8";
    case ALWAN_PIXEL_U16: return "U16";
    case ALWAN_PIXEL_F32: return "F32";
    case ALWAN_PIXEL_F64: return "F64";
    }
    return "?";
}

static size_t fmt_elem_size(alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8:  return sizeof(uint8_t);
    case ALWAN_PIXEL_U16: return sizeof(uint16_t);
    case ALWAN_PIXEL_F32: return sizeof(float);
    case ALWAN_PIXEL_F64: return sizeof(double);
    }
    return 0;
}

static alwan_pixel_format const PM_FMTS[4] = {
    ALWAN_PIXEL_U8, ALWAN_PIXEL_U16, ALWAN_PIXEL_F32, ALWAN_PIXEL_F64
};

/* ================================================================
 * Comparison helpers
 * ================================================================ */

static int cmp3(alwan_scalar const *a0, alwan_scalar const *a1, alwan_scalar const *a2,
                alwan_scalar const *b0, alwan_scalar const *b1, alwan_scalar const *b2,
                size_t count, char const *name, alwan_scalar tol) {
    for (size_t i = 0; i < count; i++) {
        alwan_scalar d0 = ALWAN_ABS(a0[i] - b0[i]);
        alwan_scalar d1 = ALWAN_ABS(a1[i] - b1[i]);
        alwan_scalar d2 = ALWAN_ABS(a2[i] - b2[i]);
        alwan_scalar mx = d0; if (d1 > mx) mx = d1; if (d2 > mx) mx = d2;
        if (mx > tol) {
            printf("[FAIL] %s: pixel %zu: diff=%.6e (tol=%.6e)\n", name, i, (double)mx, (double)tol);
            printf("  got: [%.10e, %.10e, %.10e]\n", (double)a0[i], (double)a1[i], (double)a2[i]);
            printf("  ref: [%.10e, %.10e, %.10e]\n", (double)b0[i], (double)b1[i], (double)b2[i]);
            return 1;
        }
    }
    return 0;
}

static int cmp_planar_vs_aos(alwan_scalar const *p0, alwan_scalar const *p1, alwan_scalar const *p2,
                              alwan_scalar const *aos, size_t count, char const *name,
                              alwan_scalar tol) {
    for (size_t i = 0; i < count; i++) {
        alwan_scalar d0 = ALWAN_ABS(p0[i] - aos[i*3+0]);
        alwan_scalar d1 = ALWAN_ABS(p1[i] - aos[i*3+1]);
        alwan_scalar d2 = ALWAN_ABS(p2[i] - aos[i*3+2]);
        alwan_scalar mx = d0; if (d1 > mx) mx = d1; if (d2 > mx) mx = d2;
        if (mx > tol) {
            printf("[FAIL] %s: pixel %zu: diff=%.6e (tol=%.6e)\n", name, i, (double)mx, (double)tol);
            return 1;
        }
    }
    return 0;
}

/* ================================================================
 * Buffer management
 * ================================================================ */

static void deinterleave(alwan_scalar *ch0, alwan_scalar *ch1, alwan_scalar *ch2,
                          alwan_scalar const *aos, size_t count) {
    for (size_t i = 0; i < count; i++) {
        ch0[i] = aos[i*3+0];
        ch1[i] = aos[i*3+1];
        ch2[i] = aos[i*3+2];
    }
}

#define ASTRIDE (3 * sizeof(alwan_scalar))
#define PSTRIDE (sizeof(alwan_scalar))

typedef struct {
    alwan_scalar *grid;
    alwan_scalar *aos;
    alwan_scalar *pi0, *pi1, *pi2;
    alwan_scalar *po0, *po1, *po2;
    alwan_scalar *r0, *r1, *r2;
} pm_buf;

static int pm_alloc(pm_buf *b, size_t count) {
    size_t n3 = count * 3 * sizeof(alwan_scalar);
    size_t n1 = count * sizeof(alwan_scalar);
    b->grid = (alwan_scalar *)malloc(n3);
    b->aos  = (alwan_scalar *)malloc(n3);
    b->pi0  = (alwan_scalar *)malloc(n1);
    b->pi1  = (alwan_scalar *)malloc(n1);
    b->pi2  = (alwan_scalar *)malloc(n1);
    b->po0  = (alwan_scalar *)malloc(n1);
    b->po1  = (alwan_scalar *)malloc(n1);
    b->po2  = (alwan_scalar *)malloc(n1);
    b->r0   = (alwan_scalar *)malloc(n1);
    b->r1   = (alwan_scalar *)malloc(n1);
    b->r2   = (alwan_scalar *)malloc(n1);
    return (b->grid && b->aos && b->pi0 && b->pi1 && b->pi2 &&
            b->po0 && b->po1 && b->po2 && b->r0 && b->r1 && b->r2) ? 0 : 1;
}

static void pm_free(pm_buf *b) {
    free(b->grid); free(b->aos);
    free(b->pi0); free(b->pi1); free(b->pi2);
    free(b->po0); free(b->po1); free(b->po2);
    free(b->r0); free(b->r1); free(b->r2);
}

#define RELOAD_UNIT() do { \
    generate_unit_grid(b.grid); \
    deinterleave(b.pi0, b.pi1, b.pi2, b.grid, PM_COUNT); \
} while(0)

#define RELOAD_LAB() do { \
    generate_lab_grid(b.grid); \
    deinterleave(b.pi0, b.pi1, b.pi2, b.grid, PM_COUNT); \
} while(0)

/* Use AoS output from _map_interleave as next input */
#define CHAIN_AOS() do { \
    memcpy(b.grid, b.aos, PM_COUNT * ASTRIDE); \
    deinterleave(b.pi0, b.pi1, b.pi2, b.grid, PM_COUNT); \
} while(0)

/* Use planar reference output as next input (for inverse tests) */
#define CHAIN_REF() do { \
    memcpy(b.pi0, b.r0, PM_COUNT * PSTRIDE); \
    memcpy(b.pi1, b.r1, PM_COUNT * PSTRIDE); \
    memcpy(b.pi2, b.r2, PM_COUNT * PSTRIDE); \
    for (size_t i_ = 0; i_ < PM_COUNT; i_++) { \
        b.grid[i_*3+0] = b.pi0[i_]; b.grid[i_*3+1] = b.pi1[i_]; b.grid[i_*3+2] = b.pi2[i_]; \
    } \
} while(0)

/* ================================================================
 * Typed planar scatter/collect (for _ex tests)
 * ================================================================ */

static void scatter1(void *out, alwan_pixel_format fmt,
                      alwan_scalar const *in, size_t count) {
    for (size_t i = 0; i < count; i++) {
        alwan_scalar v = in[i];
        switch (fmt) {
        case ALWAN_PIXEL_U8:  ((uint8_t *)out)[i]  = (v < 0) ? 0 : (v > 1) ? 255
                                : (uint8_t)(v * 255 + 0.5); break;
        case ALWAN_PIXEL_U16: ((uint16_t *)out)[i] = (v < 0) ? 0 : (v > 1) ? 65535
                                : (uint16_t)(v * 65535 + 0.5); break;
        case ALWAN_PIXEL_F32: ((float *)out)[i]    = (float)v; break;
        case ALWAN_PIXEL_F64: ((double *)out)[i]   = (double)v; break;
        }
    }
}

static void collect1(alwan_scalar *out, void const *in,
                      alwan_pixel_format fmt, size_t count) {
    for (size_t i = 0; i < count; i++) {
        switch (fmt) {
        case ALWAN_PIXEL_U8:  out[i] = (alwan_scalar)((uint8_t const *)in)[i]
                                / ALWAN_LITERAL(255.0); break;
        case ALWAN_PIXEL_U16: out[i] = (alwan_scalar)((uint16_t const *)in)[i]
                                / ALWAN_LITERAL(65535.0); break;
        case ALWAN_PIXEL_F32: out[i] = (alwan_scalar)((float const *)in)[i]; break;
        case ALWAN_PIXEL_F64: out[i] = (alwan_scalar)((double const *)in)[i]; break;
        }
    }
}

/* ================================================================
 * Macros: _v (per-pixel API) vs _map_planar
 *
 * TVP: pointer-based API  fn(&out, &in)
 * TVP_W: with white point fn(&out, &in, &wp)
 * TVP_I: with int param   fn(&out, &in, param)
 * ================================================================ */

#define TVP(v_fn, p_fn, InT, OutT, fi0,fi1,fi2, fo0,fo1,fo2, lbl) do { \
    for (size_t i_ = 0; i_ < PM_COUNT; i_++) { \
        InT si_; si_.fi0 = b.pi0[i_]; si_.fi1 = b.pi1[i_]; si_.fi2 = b.pi2[i_]; \
        OutT so_; v_fn(&so_, &si_); \
        b.r0[i_] = so_.fo0; b.r1[i_] = so_.fo1; b.r2[i_] = so_.fo2; \
    } \
    p_fn(b.po0, b.po1, b.po2, b.pi0, b.pi1, b.pi2, PM_COUNT, PSTRIDE, PSTRIDE); \
    if (cmp3(b.po0, b.po1, b.po2, b.r0, b.r1, b.r2, PM_COUNT, lbl, tol)) goto fail; \
} while(0)

#define TVP_W(v_fn, p_fn, InT, OutT, fi0,fi1,fi2, fo0,fo1,fo2, wp, lbl) do { \
    for (size_t i_ = 0; i_ < PM_COUNT; i_++) { \
        InT si_; si_.fi0 = b.pi0[i_]; si_.fi1 = b.pi1[i_]; si_.fi2 = b.pi2[i_]; \
        OutT so_; v_fn(&so_, &si_, wp); \
        b.r0[i_] = so_.fo0; b.r1[i_] = so_.fo1; b.r2[i_] = so_.fo2; \
    } \
    p_fn(b.po0, b.po1, b.po2, b.pi0, b.pi1, b.pi2, wp, PM_COUNT, PSTRIDE, PSTRIDE); \
    if (cmp3(b.po0, b.po1, b.po2, b.r0, b.r1, b.r2, PM_COUNT, lbl, tol)) goto fail; \
} while(0)

#define TVP_I(v_fn, p_fn, InT, OutT, fi0,fi1,fi2, fo0,fo1,fo2, param, lbl) do { \
    for (size_t i_ = 0; i_ < PM_COUNT; i_++) { \
        InT si_; si_.fi0 = b.pi0[i_]; si_.fi1 = b.pi1[i_]; si_.fi2 = b.pi2[i_]; \
        OutT so_; v_fn(&so_, &si_, param); \
        b.r0[i_] = so_.fo0; b.r1[i_] = so_.fo1; b.r2[i_] = so_.fo2; \
    } \
    p_fn(b.po0, b.po1, b.po2, b.pi0, b.pi1, b.pi2, param, PM_COUNT, PSTRIDE, PSTRIDE); \
    if (cmp3(b.po0, b.po1, b.po2, b.r0, b.r1, b.r2, PM_COUNT, lbl, tol)) goto fail; \
} while(0)

/* ================================================================
 * Macros: _map_interleave vs _map_planar
 * ================================================================ */

#define TIP(m_fn, p_fn, lbl) do { \
    m_fn(b.aos, b.grid, PM_COUNT, ASTRIDE, ASTRIDE); \
    p_fn(b.po0, b.po1, b.po2, b.pi0, b.pi1, b.pi2, PM_COUNT, PSTRIDE, PSTRIDE); \
    if (cmp_planar_vs_aos(b.po0, b.po1, b.po2, b.aos, PM_COUNT, lbl, tol)) goto fail; \
} while(0)

#define TIP_W(m_fn, p_fn, wp, lbl) do { \
    m_fn(b.aos, b.grid, wp, PM_COUNT, ASTRIDE, ASTRIDE); \
    p_fn(b.po0, b.po1, b.po2, b.pi0, b.pi1, b.pi2, wp, PM_COUNT, PSTRIDE, PSTRIDE); \
    if (cmp_planar_vs_aos(b.po0, b.po1, b.po2, b.aos, PM_COUNT, lbl, tol)) goto fail; \
} while(0)

#define TIP_I(m_fn, p_fn, param, lbl) do { \
    m_fn(b.aos, b.grid, param, PM_COUNT, ASTRIDE, ASTRIDE); \
    p_fn(b.po0, b.po1, b.po2, b.pi0, b.pi1, b.pi2, param, PM_COUNT, PSTRIDE, PSTRIDE); \
    if (cmp_planar_vs_aos(b.po0, b.po1, b.po2, b.aos, PM_COUNT, lbl, tol)) goto fail; \
} while(0)

/* ================================================================
 * Macros: _v (per-pixel API) vs _map_interleave
 * ================================================================ */

#define TVI(v_fn, m_fn, InT, OutT, fi0,fi1,fi2, fo0,fo1,fo2, lbl) do { \
    for (size_t i_ = 0; i_ < PM_COUNT; i_++) { \
        InT si_; si_.fi0 = b.pi0[i_]; si_.fi1 = b.pi1[i_]; si_.fi2 = b.pi2[i_]; \
        OutT so_; v_fn(&so_, &si_); \
        b.r0[i_] = so_.fo0; b.r1[i_] = so_.fo1; b.r2[i_] = so_.fo2; \
    } \
    m_fn(b.aos, b.grid, PM_COUNT, ASTRIDE, ASTRIDE); \
    if (cmp_planar_vs_aos(b.r0, b.r1, b.r2, b.aos, PM_COUNT, lbl, tol)) goto fail; \
} while(0)

/* ================================================================
 * Section A: _v vs _map_planar
 *
 * Both paths use scalar per-pixel kernels, so results should match
 * within ALWAN_TEST_TOLERANCE (essentially exact).
 * ================================================================ */

static int test_v_vs_planar_core(void) {
    TEST_START("_v vs _map_planar: core (OkLab, LCh, LChuv, xyY, JzAzBz, IPT)");
    alwan_scalar const tol = ALWAN_TEST_TOLERANCE;
    pm_buf b;
    if (pm_alloc(&b, PM_COUNT)) { pm_free(&b); TEST_FAIL("malloc"); }

    RELOAD_UNIT();
    TVP(alwan_xyz_to_oklab, alwan_xyz_to_oklab_map_planar,
        alwan_xyz, alwan_oklab, x,y,z, L,a,b, "v:xyz_to_oklab");
    CHAIN_REF();
    TVP(alwan_oklab_to_xyz, alwan_oklab_to_xyz_map_planar,
        alwan_oklab, alwan_xyz, L,a,b, x,y,z, "v:oklab_to_xyz");

    RELOAD_UNIT();
    alwan_xyz_to_oklab_map_interleave(b.grid, b.grid, PM_COUNT, ASTRIDE, ASTRIDE);
    deinterleave(b.pi0, b.pi1, b.pi2, b.grid, PM_COUNT);
    TVP(alwan_oklab_to_oklch, alwan_oklab_to_oklch_map_planar,
        alwan_oklab, alwan_oklch, L,a,b, L,C,h, "v:oklab_to_oklch");
    CHAIN_REF();
    TVP(alwan_oklch_to_oklab, alwan_oklch_to_oklab_map_planar,
        alwan_oklch, alwan_oklab, L,C,h, L,a,b, "v:oklch_to_oklab");

    RELOAD_LAB();
    TVP(alwan_lab_to_lch, alwan_lab_to_lch_map_planar,
        alwan_lab, alwan_lch, L,a,b, L,C,h, "v:lab_to_lch");
    CHAIN_REF();
    TVP(alwan_lch_to_lab, alwan_lch_to_lab_map_planar,
        alwan_lch, alwan_lab, L,C,h, L,a,b, "v:lch_to_lab");

    RELOAD_LAB();
    TVP(alwan_luv_to_lchuv, alwan_luv_to_lchuv_map_planar,
        alwan_luv, alwan_lchuv, L,u,v, L,C,h, "v:luv_to_lchuv");
    CHAIN_REF();
    TVP(alwan_lchuv_to_luv, alwan_lchuv_to_luv_map_planar,
        alwan_lchuv, alwan_luv, L,C,h, L,u,v, "v:lchuv_to_luv");

    RELOAD_UNIT();
    TVP(alwan_xyz_to_xyy, alwan_xyz_to_xyy_map_planar,
        alwan_xyz, alwan_xyy, x,y,z, x,y,Y, "v:xyz_to_xyy");
    CHAIN_REF();
    TVP(alwan_xyy_to_xyz, alwan_xyy_to_xyz_map_planar,
        alwan_xyy, alwan_xyz, x,y,Y, x,y,z, "v:xyy_to_xyz");

    RELOAD_UNIT();
    TVP(alwan_xyz_to_jzazbz, alwan_xyz_to_jzazbz_map_planar,
        alwan_xyz, alwan_jzazbz, x,y,z, Jz,az,bz, "v:xyz_to_jzazbz");
    CHAIN_REF();
    TVP(alwan_jzazbz_to_xyz, alwan_jzazbz_to_xyz_map_planar,
        alwan_jzazbz, alwan_xyz, Jz,az,bz, x,y,z, "v:jzazbz_to_xyz");

    RELOAD_UNIT();
    alwan_xyz_to_jzazbz_map_interleave(b.grid, b.grid, PM_COUNT, ASTRIDE, ASTRIDE);
    deinterleave(b.pi0, b.pi1, b.pi2, b.grid, PM_COUNT);
    TVP(alwan_jzazbz_to_jzczhz, alwan_jzazbz_to_jzczhz_map_planar,
        alwan_jzazbz, alwan_jzczhz, Jz,az,bz, Jz,Cz,hz, "v:jzazbz_to_jzczhz");
    CHAIN_REF();
    TVP(alwan_jzczhz_to_jzazbz, alwan_jzczhz_to_jzazbz_map_planar,
        alwan_jzczhz, alwan_jzazbz, Jz,Cz,hz, Jz,az,bz, "v:jzczhz_to_jzazbz");

    RELOAD_UNIT();
    TVP(alwan_xyz_to_ipt, alwan_xyz_to_ipt_map_planar,
        alwan_xyz, alwan_ipt, x,y,z, I,P,T, "v:xyz_to_ipt");
    CHAIN_REF();
    TVP(alwan_ipt_to_xyz, alwan_ipt_to_xyz_map_planar,
        alwan_ipt, alwan_xyz, I,P,T, x,y,z, "v:ipt_to_xyz");

    pm_free(&b);
    TEST_PASS_MSG(); return 0;
fail:
    pm_free(&b); return 1;
}

static int test_v_vs_planar_srgb(void) {
    TEST_START("_v vs _map_planar: sRGB convenience");
    alwan_scalar const tol = ALWAN_TEST_TOLERANCE;
    pm_buf b;
    if (pm_alloc(&b, PM_COUNT)) { pm_free(&b); TEST_FAIL("malloc"); }

    RELOAD_UNIT();
    TVP(alwan_srgb_to_xyz, alwan_srgb_to_xyz_map_planar,
        alwan_rgb, alwan_xyz, r,g,b, x,y,z, "v:srgb_to_xyz");
    CHAIN_REF();
    TVP(alwan_xyz_to_srgb, alwan_xyz_to_srgb_map_planar,
        alwan_xyz, alwan_rgb, x,y,z, r,g,b, "v:xyz_to_srgb");

    RELOAD_UNIT();
    TVP(alwan_srgb_to_lab, alwan_srgb_to_lab_map_planar,
        alwan_rgb, alwan_lab, r,g,b, L,a,b, "v:srgb_to_lab");
    CHAIN_REF();
    TVP(alwan_lab_to_srgb, alwan_lab_to_srgb_map_planar,
        alwan_lab, alwan_rgb, L,a,b, r,g,b, "v:lab_to_srgb");

    RELOAD_UNIT();
    TVP(alwan_srgb_to_oklab, alwan_srgb_to_oklab_map_planar,
        alwan_rgb, alwan_oklab, r,g,b, L,a,b, "v:srgb_to_oklab");
    CHAIN_REF();
    TVP(alwan_oklab_to_srgb, alwan_oklab_to_srgb_map_planar,
        alwan_oklab, alwan_rgb, L,a,b, r,g,b, "v:oklab_to_srgb");

    pm_free(&b);
    TEST_PASS_MSG(); return 0;
fail:
    pm_free(&b); return 1;
}

static int test_v_vs_planar_white(void) {
    TEST_START("_v vs _map_planar: white point (Lab, Luv)");
    alwan_scalar const tol = ALWAN_TEST_TOLERANCE;
    pm_buf b;
    if (pm_alloc(&b, PM_COUNT)) { pm_free(&b); TEST_FAIL("malloc"); }

    alwan_xyz wp;
    wp.x = g_d65_xyz_y1[0]; wp.y = g_d65_xyz_y1[1]; wp.z = g_d65_xyz_y1[2];

    RELOAD_UNIT();
    TVP_W(alwan_xyz_to_lab, alwan_xyz_to_lab_map_planar,
          alwan_xyz, alwan_lab, x,y,z, L,a,b, &wp, "v:xyz_to_lab");
    CHAIN_REF();
    TVP_W(alwan_lab_to_xyz, alwan_lab_to_xyz_map_planar,
          alwan_lab, alwan_xyz, L,a,b, x,y,z, &wp, "v:lab_to_xyz");

    RELOAD_UNIT();
    TVP_W(alwan_xyz_to_luv, alwan_xyz_to_luv_map_planar,
          alwan_xyz, alwan_luv, x,y,z, L,u,v, &wp, "v:xyz_to_luv");
    CHAIN_REF();
    TVP_W(alwan_luv_to_xyz, alwan_luv_to_xyz_map_planar,
          alwan_luv, alwan_xyz, L,u,v, x,y,z, &wp, "v:luv_to_xyz");

    pm_free(&b);
    TEST_PASS_MSG(); return 0;
fail:
    pm_free(&b); return 1;
}

static int test_v_vs_planar_hsv_hsl(void) {
    TEST_START("_v vs _map_planar: HSV/HSL");
    alwan_scalar const tol = ALWAN_TEST_TOLERANCE;
    pm_buf b;
    if (pm_alloc(&b, PM_COUNT)) { pm_free(&b); TEST_FAIL("malloc"); }

    RELOAD_UNIT();
    TVP(alwan_rgb_to_hsv, alwan_rgb_to_hsv_map_planar,
        alwan_rgb, alwan_hsv, r,g,b, h,s,v, "v:rgb_to_hsv");
    CHAIN_REF();
    TVP(alwan_hsv_to_rgb, alwan_hsv_to_rgb_map_planar,
        alwan_hsv, alwan_rgb, h,s,v, r,g,b, "v:hsv_to_rgb");

    RELOAD_UNIT();
    TVP(alwan_rgb_to_hsl, alwan_rgb_to_hsl_map_planar,
        alwan_rgb, alwan_hsl, r,g,b, h,s,l, "v:rgb_to_hsl");
    CHAIN_REF();
    TVP(alwan_hsl_to_rgb, alwan_hsl_to_rgb_map_planar,
        alwan_hsl, alwan_rgb, h,s,l, r,g,b, "v:hsl_to_rgb");

    pm_free(&b);
    TEST_PASS_MSG(); return 0;
fail:
    pm_free(&b); return 1;
}

static int test_v_vs_planar_ictcp(void) {
    TEST_START("_v vs _map_planar: ICtCp (PQ)");
    alwan_scalar const tol = ALWAN_TEST_TOLERANCE;
    pm_buf b;
    if (pm_alloc(&b, PM_COUNT)) { pm_free(&b); TEST_FAIL("malloc"); }

    RELOAD_UNIT();
    TVP_I(alwan_rgb_to_ictcp, alwan_rgb_to_ictcp_map_planar,
          alwan_rgb, alwan_ictcp, r,g,b, I,Ct,Cp, 1, "v:rgb_to_ictcp_pq");
    CHAIN_REF();
    TVP_I(alwan_ictcp_to_rgb, alwan_ictcp_to_rgb_map_planar,
          alwan_ictcp, alwan_rgb, I,Ct,Cp, r,g,b, 1, "v:ictcp_to_rgb_pq");

    RELOAD_UNIT();
    TVP_I(alwan_rgb_to_ictcp, alwan_rgb_to_ictcp_map_planar,
          alwan_rgb, alwan_ictcp, r,g,b, I,Ct,Cp, 0, "v:rgb_to_ictcp_hlg");
    CHAIN_REF();
    TVP_I(alwan_ictcp_to_rgb, alwan_ictcp_to_rgb_map_planar,
          alwan_ictcp, alwan_rgb, I,Ct,Cp, r,g,b, 0, "v:ictcp_to_rgb_hlg");

    pm_free(&b);
    TEST_PASS_MSG(); return 0;
fail:
    pm_free(&b); return 1;
}

static int test_v_vs_planar_convenience(void) {
    TEST_START("_v vs _map_planar: CMY, YCoCg, HWB, YCbCr, YcCbcCrc");
    alwan_scalar const tol = ALWAN_TEST_TOLERANCE;
    pm_buf b;
    if (pm_alloc(&b, PM_COUNT)) { pm_free(&b); TEST_FAIL("malloc"); }

    RELOAD_UNIT();
    TVP(alwan_rgb_to_cmy, alwan_rgb_to_cmy_map_planar,
        alwan_rgb, alwan_cmy, r,g,b, c,m,y, "v:rgb_to_cmy");
    CHAIN_REF();
    TVP(alwan_cmy_to_rgb, alwan_cmy_to_rgb_map_planar,
        alwan_cmy, alwan_rgb, c,m,y, r,g,b, "v:cmy_to_rgb");

    RELOAD_UNIT();
    TVP(alwan_rgb_to_ycocg, alwan_rgb_to_ycocg_map_planar,
        alwan_rgb, alwan_ycocg, r,g,b, Y,Co,Cg, "v:rgb_to_ycocg");
    CHAIN_REF();
    TVP(alwan_ycocg_to_rgb, alwan_ycocg_to_rgb_map_planar,
        alwan_ycocg, alwan_rgb, Y,Co,Cg, r,g,b, "v:ycocg_to_rgb");

    /* HWB: API uses alwan_scalar[3], not struct */
    RELOAD_UNIT();
    for (size_t i_ = 0; i_ < PM_COUNT; i_++) {
        alwan_rgb rgb; rgb.r = b.pi0[i_]; rgb.g = b.pi1[i_]; rgb.b = b.pi2[i_];
        alwan_scalar hwb[3];
        alwan_rgb_to_hwb(hwb, &rgb);
        b.r0[i_] = hwb[0]; b.r1[i_] = hwb[1]; b.r2[i_] = hwb[2];
    }
    alwan_rgb_to_hwb_map_planar(b.po0, b.po1, b.po2,
                                 b.pi0, b.pi1, b.pi2, PM_COUNT, PSTRIDE, PSTRIDE);
    if (cmp3(b.po0, b.po1, b.po2, b.r0, b.r1, b.r2, PM_COUNT, "v:rgb_to_hwb", tol)) goto fail;

    CHAIN_REF();
    for (size_t i_ = 0; i_ < PM_COUNT; i_++) {
        alwan_scalar hwb[3] = {b.pi0[i_], b.pi1[i_], b.pi2[i_]};
        alwan_rgb rgb;
        alwan_hwb_to_rgb(&rgb, hwb);
        b.r0[i_] = rgb.r; b.r1[i_] = rgb.g; b.r2[i_] = rgb.b;
    }
    alwan_hwb_to_rgb_map_planar(b.po0, b.po1, b.po2,
                                 b.pi0, b.pi1, b.pi2, PM_COUNT, PSTRIDE, PSTRIDE);
    if (cmp3(b.po0, b.po1, b.po2, b.r0, b.r1, b.r2, PM_COUNT, "v:hwb_to_rgb", tol)) goto fail;

    /* YCbCr BT.709 */
    RELOAD_UNIT();
    TVP_I(alwan_rgb_to_ycbcr, alwan_rgb_to_ycbcr_map_planar,
          alwan_rgb, alwan_ycbcr, r,g,b, Y,Cb,Cr, ALWAN_YCBCR_BT709, "v:rgb_to_ycbcr");
    CHAIN_REF();
    TVP_I(alwan_ycbcr_to_rgb, alwan_ycbcr_to_rgb_map_planar,
          alwan_ycbcr, alwan_rgb, Y,Cb,Cr, r,g,b, ALWAN_YCBCR_BT709, "v:ycbcr_to_rgb");

    /* YcCbcCrc 10-bit */
    RELOAD_UNIT();
    TVP_I(alwan_rgb_to_yccbccrc, alwan_rgb_to_yccbccrc_map_planar,
          alwan_rgb, alwan_yccbccrc, r,g,b, Yc,Cbc,Crc, 10, "v:rgb_to_yccbccrc");
    CHAIN_REF();
    TVP_I(alwan_yccbccrc_to_rgb, alwan_yccbccrc_to_rgb_map_planar,
          alwan_yccbccrc, alwan_rgb, Yc,Cbc,Crc, r,g,b, 10, "v:yccbccrc_to_rgb");

    /* Full/legal 10-bit */
    RELOAD_UNIT();
    TVP_I(alwan_ycbcr_full_to_legal, alwan_ycbcr_full_to_legal_map_planar,
          alwan_ycbcr, alwan_ycbcr, Y,Cb,Cr, Y,Cb,Cr, 10, "v:ycbcr_full_to_legal");
    CHAIN_REF();
    TVP_I(alwan_ycbcr_legal_to_full, alwan_ycbcr_legal_to_full_map_planar,
          alwan_ycbcr, alwan_ycbcr, Y,Cb,Cr, Y,Cb,Cr, 10, "v:ycbcr_legal_to_full");

    pm_free(&b);
    TEST_PASS_MSG(); return 0;
fail:
    pm_free(&b); return 1;
}

static int test_v_vs_planar_extended(void) {
    TEST_START("_v vs _map_planar: extended colorspaces");
    alwan_scalar const tol = ALWAN_TEST_TOLERANCE;
    pm_buf b;
    if (pm_alloc(&b, PM_COUNT)) { pm_free(&b); TEST_FAIL("malloc"); }

    alwan_xyz wp;
    wp.x = g_d65_xyz_y1[0]; wp.y = g_d65_xyz_y1[1]; wp.z = g_d65_xyz_y1[2];

    /* Simple XYZ-based */
    RELOAD_UNIT();
    TVP(alwan_xyz_to_igpgtg, alwan_xyz_to_igpgtg_map_planar,
        alwan_xyz, alwan_igpgtg, x,y,z, Ig,Pg,Tg, "v:xyz_to_igpgtg");
    CHAIN_REF();
    TVP(alwan_igpgtg_to_xyz, alwan_igpgtg_to_xyz_map_planar,
        alwan_igpgtg, alwan_xyz, Ig,Pg,Tg, x,y,z, "v:igpgtg_to_xyz");

    RELOAD_UNIT();
    TVP(alwan_xyz_to_icacb, alwan_xyz_to_icacb_map_planar,
        alwan_xyz, alwan_icacb, x,y,z, I,Ca,Cb, "v:xyz_to_icacb");
    CHAIN_REF();
    TVP(alwan_icacb_to_xyz, alwan_icacb_to_xyz_map_planar,
        alwan_icacb, alwan_xyz, I,Ca,Cb, x,y,z, "v:icacb_to_xyz");

    RELOAD_UNIT();
    TVP(alwan_xyz_to_hdr_cielab, alwan_xyz_to_hdr_cielab_map_planar,
        alwan_xyz, alwan_lab, x,y,z, L,a,b, "v:xyz_to_hdr_cielab");
    CHAIN_REF();
    TVP(alwan_hdr_cielab_to_xyz, alwan_hdr_cielab_to_xyz_map_planar,
        alwan_lab, alwan_xyz, L,a,b, x,y,z, "v:hdr_cielab_to_xyz");

    RELOAD_UNIT();
    TVP(alwan_xyz_to_hdr_ipt, alwan_xyz_to_hdr_ipt_map_planar,
        alwan_xyz, alwan_ipt, x,y,z, I,P,T, "v:xyz_to_hdr_ipt");
    CHAIN_REF();
    TVP(alwan_hdr_ipt_to_xyz, alwan_hdr_ipt_to_xyz_map_planar,
        alwan_ipt, alwan_xyz, I,P,T, x,y,z, "v:hdr_ipt_to_xyz");

    RELOAD_UNIT();
    TVP(alwan_xyz_to_ucs, alwan_xyz_to_ucs_map_planar,
        alwan_xyz, alwan_ucs, x,y,z, U,V,W, "v:xyz_to_ucs");
    CHAIN_REF();
    TVP(alwan_ucs_to_xyz, alwan_ucs_to_xyz_map_planar,
        alwan_ucs, alwan_xyz, U,V,W, x,y,z, "v:ucs_to_xyz");

    RELOAD_UNIT();
    TVP(alwan_xyz_to_osa_ucs, alwan_xyz_to_osa_ucs_map_planar,
        alwan_xyz, alwan_osa_ucs, x,y,z, L,j,g, "v:xyz_to_osa_ucs");

    RELOAD_UNIT();
    TVP(alwan_xyz_to_hunter_lab, alwan_xyz_to_hunter_lab_map_planar,
        alwan_xyz, alwan_hunter_lab, x,y,z, L,a,b, "v:xyz_to_hunter_lab");
    CHAIN_REF();
    TVP(alwan_hunter_lab_to_xyz, alwan_hunter_lab_to_xyz_map_planar,
        alwan_hunter_lab, alwan_xyz, L,a,b, x,y,z, "v:hunter_lab_to_xyz");

    RELOAD_UNIT();
    TVP(alwan_xyz_to_prolab, alwan_xyz_to_prolab_map_planar,
        alwan_xyz, alwan_prolab, x,y,z, L,a,b, "v:xyz_to_prolab");
    CHAIN_REF();
    TVP(alwan_prolab_to_xyz, alwan_prolab_to_xyz_map_planar,
        alwan_prolab, alwan_xyz, L,a,b, x,y,z, "v:prolab_to_xyz");

    /* RGB-based */
    RELOAD_UNIT();
    TVP(alwan_rgb_to_prismatic, alwan_rgb_to_prismatic_map_planar,
        alwan_rgb, alwan_prismatic, r,g,b, L,s,h, "v:rgb_to_prismatic");
    CHAIN_REF();
    TVP(alwan_prismatic_to_rgb, alwan_prismatic_to_rgb_map_planar,
        alwan_prismatic, alwan_rgb, L,s,h, r,g,b, "v:prismatic_to_rgb");

    RELOAD_UNIT();
    TVP(alwan_rgb_to_hcl, alwan_rgb_to_hcl_map_planar,
        alwan_rgb, alwan_hcl, r,g,b, H,C,L, "v:rgb_to_hcl");
    CHAIN_REF();
    TVP(alwan_hcl_to_rgb, alwan_hcl_to_rgb_map_planar,
        alwan_hcl, alwan_rgb, H,C,L, r,g,b, "v:hcl_to_rgb");

    RELOAD_UNIT();
    TVP(alwan_rgb_to_ihls, alwan_rgb_to_ihls_map_planar,
        alwan_rgb, alwan_ihls, r,g,b, H,L,S, "v:rgb_to_ihls");
    CHAIN_REF();
    TVP(alwan_ihls_to_rgb, alwan_ihls_to_rgb_map_planar,
        alwan_ihls, alwan_rgb, H,L,S, r,g,b, "v:ihls_to_rgb");

    /* With white point */
    RELOAD_UNIT();
    TVP_W(alwan_xyz_to_uvw, alwan_xyz_to_uvw_map_planar,
          alwan_xyz, alwan_uvw, x,y,z, U,V,W, &wp, "v:xyz_to_uvw");
    CHAIN_REF();
    TVP_W(alwan_uvw_to_xyz, alwan_uvw_to_xyz_map_planar,
          alwan_uvw, alwan_xyz, U,V,W, x,y,z, &wp, "v:uvw_to_xyz");

    RELOAD_UNIT();
    TVP_W(alwan_xyz_to_hunter_lab_custom, alwan_xyz_to_hunter_lab_custom_map_planar,
          alwan_xyz, alwan_hunter_lab, x,y,z, L,a,b, &wp, "v:xyz_to_hunter_lab_custom");
    CHAIN_REF();
    TVP_W(alwan_hunter_lab_to_xyz_custom, alwan_hunter_lab_to_xyz_custom_map_planar,
          alwan_hunter_lab, alwan_xyz, L,a,b, x,y,z, &wp, "v:hunter_lab_custom_to_xyz");

    RELOAD_UNIT();
    TVP_W(alwan_xyz_to_prolab_custom, alwan_xyz_to_prolab_custom_map_planar,
          alwan_xyz, alwan_prolab, x,y,z, L,a,b, &wp, "v:xyz_to_prolab_custom");
    CHAIN_REF();
    TVP_W(alwan_prolab_to_xyz_custom, alwan_prolab_to_xyz_custom_map_planar,
          alwan_prolab, alwan_xyz, L,a,b, x,y,z, &wp, "v:prolab_custom_to_xyz");

    /* DIN99 (variant 0) */
    RELOAD_LAB();
    TVP_I(alwan_lab_to_din99, alwan_lab_to_din99_map_planar,
          alwan_lab, alwan_din99, L,a,b, L99,a99,b99, 0, "v:lab_to_din99");
    CHAIN_REF();
    TVP_I(alwan_din99_to_lab, alwan_din99_to_lab_map_planar,
          alwan_din99, alwan_lab, L99,a99,b99, L,a,b, 0, "v:din99_to_lab");

    pm_free(&b);
    TEST_PASS_MSG(); return 0;
fail:
    pm_free(&b); return 1;
}

static int test_v_vs_planar_cvd(void) {
    TEST_START("_v vs _map_planar: CVD simulation");
    alwan_scalar const tol = ALWAN_TEST_TOLERANCE;
    pm_buf b;
    if (pm_alloc(&b, PM_COUNT)) { pm_free(&b); TEST_FAIL("malloc"); }

    alwan_scalar const sev = ALWAN_LITERAL(0.8);

    RELOAD_UNIT();
    for (size_t i_ = 0; i_ < PM_COUNT; i_++) {
        alwan_rgb si; si.r = b.pi0[i_]; si.g = b.pi1[i_]; si.b = b.pi2[i_];
        alwan_rgb so; alwan_simulate_cvd(&so, &si, ALWAN_CVD_PROTANOPIA, sev);
        b.r0[i_] = so.r; b.r1[i_] = so.g; b.r2[i_] = so.b;
    }
    alwan_simulate_cvd_map_planar(b.po0, b.po1, b.po2, b.pi0, b.pi1, b.pi2,
                                   ALWAN_CVD_PROTANOPIA, sev, PM_COUNT, PSTRIDE, PSTRIDE);
    if (cmp3(b.po0, b.po1, b.po2, b.r0, b.r1, b.r2, PM_COUNT, "v:cvd_protan", tol)) goto fail;

    RELOAD_UNIT();
    for (size_t i_ = 0; i_ < PM_COUNT; i_++) {
        alwan_rgb si; si.r = b.pi0[i_]; si.g = b.pi1[i_]; si.b = b.pi2[i_];
        alwan_rgb so; alwan_simulate_cvd(&so, &si, ALWAN_CVD_DEUTERANOPIA, sev);
        b.r0[i_] = so.r; b.r1[i_] = so.g; b.r2[i_] = so.b;
    }
    alwan_simulate_deuteranopia_map_planar(b.po0, b.po1, b.po2, b.pi0, b.pi1, b.pi2,
                                            sev, PM_COUNT, PSTRIDE, PSTRIDE);
    if (cmp3(b.po0, b.po1, b.po2, b.r0, b.r1, b.r2, PM_COUNT, "v:cvd_deutan", tol)) goto fail;

    RELOAD_UNIT();
    for (size_t i_ = 0; i_ < PM_COUNT; i_++) {
        alwan_rgb si; si.r = b.pi0[i_]; si.g = b.pi1[i_]; si.b = b.pi2[i_];
        alwan_rgb so; alwan_simulate_cvd(&so, &si, ALWAN_CVD_TRITANOPIA, sev);
        b.r0[i_] = so.r; b.r1[i_] = so.g; b.r2[i_] = so.b;
    }
    alwan_simulate_tritanopia_map_planar(b.po0, b.po1, b.po2, b.pi0, b.pi1, b.pi2,
                                          sev, PM_COUNT, PSTRIDE, PSTRIDE);
    if (cmp3(b.po0, b.po1, b.po2, b.r0, b.r1, b.r2, PM_COUNT, "v:cvd_tritan", tol)) goto fail;

    pm_free(&b);
    TEST_PASS_MSG(); return 0;
fail:
    pm_free(&b); return 1;
}

static int test_v_vs_planar_colorcorr(void) {
    TEST_START("_v vs _map_planar: color correction");
    alwan_scalar const tol = ALWAN_TEST_TOLERANCE;
    pm_buf b;
    if (pm_alloc(&b, PM_COUNT)) { pm_free(&b); TEST_FAIL("malloc"); }

    alwan_rgb lift  = {ALWAN_LITERAL(0.02), ALWAN_LITERAL(0.01), ALWAN_LITERAL(0.03)};
    alwan_rgb gamma = {ALWAN_LITERAL(1.1),  ALWAN_LITERAL(0.95), ALWAN_LITERAL(1.05)};
    alwan_rgb gain  = {ALWAN_LITERAL(1.2),  ALWAN_LITERAL(1.0),  ALWAN_LITERAL(0.9)};

    RELOAD_UNIT();
    for (size_t i_ = 0; i_ < PM_COUNT; i_++) {
        alwan_rgb si; si.r = b.pi0[i_]; si.g = b.pi1[i_]; si.b = b.pi2[i_];
        alwan_rgb so;
        alwan_lgg_apply(&so, &si, &lift, &gamma, &gain);
        b.r0[i_] = so.r; b.r1[i_] = so.g; b.r2[i_] = so.b;
    }
    alwan_lgg_apply_map_planar(b.po0, b.po1, b.po2, b.pi0, b.pi1, b.pi2,
                                &lift, &gamma, &gain, PM_COUNT, PSTRIDE, PSTRIDE);
    if (cmp3(b.po0, b.po1, b.po2, b.r0, b.r1, b.r2, PM_COUNT, "v:lgg_apply", tol)) goto fail;

    alwan_mat3x3 matrix;
    matrix.m[0] = ALWAN_LITERAL(1.2);   matrix.m[1] = ALWAN_LITERAL(-0.1);  matrix.m[2] = ALWAN_LITERAL(-0.1);
    matrix.m[3] = ALWAN_LITERAL(-0.05); matrix.m[4] = ALWAN_LITERAL(1.1);   matrix.m[5] = ALWAN_LITERAL(-0.05);
    matrix.m[6] = ALWAN_LITERAL(-0.02); matrix.m[7] = ALWAN_LITERAL(-0.08); matrix.m[8] = ALWAN_LITERAL(1.1);

    RELOAD_UNIT();
    for (size_t i_ = 0; i_ < PM_COUNT; i_++) {
        alwan_rgb si; si.r = b.pi0[i_]; si.g = b.pi1[i_]; si.b = b.pi2[i_];
        alwan_rgb so;
        alwan_color_matrix_apply(&so, &si, &matrix);
        b.r0[i_] = so.r; b.r1[i_] = so.g; b.r2[i_] = so.b;
    }
    alwan_color_matrix_apply_map_planar(b.po0, b.po1, b.po2, b.pi0, b.pi1, b.pi2,
                                         &matrix, PM_COUNT, PSTRIDE, PSTRIDE);
    if (cmp3(b.po0, b.po1, b.po2, b.r0, b.r1, b.r2, PM_COUNT, "v:color_matrix", tol)) goto fail;

    RELOAD_UNIT();
    for (size_t i_ = 0; i_ < PM_COUNT; i_++) {
        alwan_rgb si; si.r = b.pi0[i_]; si.g = b.pi1[i_]; si.b = b.pi2[i_];
        alwan_rgb so;
        alwan_printer_lights_apply(&so, &si, ALWAN_LITERAL(1.1), ALWAN_LITERAL(0.95), ALWAN_LITERAL(1.05));
        b.r0[i_] = so.r; b.r1[i_] = so.g; b.r2[i_] = so.b;
    }
    alwan_printer_lights_apply_map_planar(b.po0, b.po1, b.po2, b.pi0, b.pi1, b.pi2,
                                           ALWAN_LITERAL(1.1), ALWAN_LITERAL(0.95), ALWAN_LITERAL(1.05),
                                           PM_COUNT, PSTRIDE, PSTRIDE);
    if (cmp3(b.po0, b.po1, b.po2, b.r0, b.r1, b.r2, PM_COUNT, "v:printer_lights", tol)) goto fail;

    alwan_rgb wb_mul = {ALWAN_LITERAL(1.05), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.92)};
    RELOAD_UNIT();
    for (size_t i_ = 0; i_ < PM_COUNT; i_++) {
        alwan_rgb si; si.r = b.pi0[i_]; si.g = b.pi1[i_]; si.b = b.pi2[i_];
        alwan_rgb so;
        alwan_white_balance_apply(&so, &si, &wb_mul);
        b.r0[i_] = so.r; b.r1[i_] = so.g; b.r2[i_] = so.b;
    }
    alwan_white_balance_apply_map_planar(b.po0, b.po1, b.po2, b.pi0, b.pi1, b.pi2,
                                          &wb_mul, PM_COUNT, PSTRIDE, PSTRIDE);
    if (cmp3(b.po0, b.po1, b.po2, b.r0, b.r1, b.r2, PM_COUNT, "v:white_balance", tol)) goto fail;

    pm_free(&b);
    TEST_PASS_MSG(); return 0;
fail:
    pm_free(&b); return 1;
}

/* ================================================================
 * Section B: _map_interleave vs _map_planar
 *
 * SIMD interleave may differ from scalar planar by fmadd rounding.
 * ================================================================ */

static int test_interleave_vs_planar(void) {
    TEST_START("_map_interleave vs _map_planar: all functions");
    alwan_scalar const tol = TOL_SIMD;
    pm_buf b;
    if (pm_alloc(&b, PM_COUNT)) { pm_free(&b); TEST_FAIL("malloc"); }

    alwan_xyz wp;
    wp.x = g_d65_xyz_y1[0]; wp.y = g_d65_xyz_y1[1]; wp.z = g_d65_xyz_y1[2];

    /* OkLab */
    RELOAD_UNIT();
    TIP(alwan_xyz_to_oklab_map_interleave, alwan_xyz_to_oklab_map_planar, "i_vs_p:xyz_to_oklab");
    RELOAD_UNIT();
    alwan_xyz_to_oklab_map_interleave(b.grid, b.grid, PM_COUNT, ASTRIDE, ASTRIDE);
    deinterleave(b.pi0, b.pi1, b.pi2, b.grid, PM_COUNT);
    TIP(alwan_oklab_to_xyz_map_interleave, alwan_oklab_to_xyz_map_planar, "i_vs_p:oklab_to_xyz");

    RELOAD_UNIT();
    alwan_xyz_to_oklab_map_interleave(b.grid, b.grid, PM_COUNT, ASTRIDE, ASTRIDE);
    deinterleave(b.pi0, b.pi1, b.pi2, b.grid, PM_COUNT);
    TIP(alwan_oklab_to_oklch_map_interleave, alwan_oklab_to_oklch_map_planar, "i_vs_p:oklab_to_oklch");
    CHAIN_AOS();
    TIP(alwan_oklch_to_oklab_map_interleave, alwan_oklch_to_oklab_map_planar, "i_vs_p:oklch_to_oklab");

    /* LCh, LChuv, xyY */
    RELOAD_LAB();
    TIP(alwan_lab_to_lch_map_interleave, alwan_lab_to_lch_map_planar, "i_vs_p:lab_to_lch");
    CHAIN_AOS();
    TIP(alwan_lch_to_lab_map_interleave, alwan_lch_to_lab_map_planar, "i_vs_p:lch_to_lab");

    RELOAD_LAB();
    TIP(alwan_luv_to_lchuv_map_interleave, alwan_luv_to_lchuv_map_planar, "i_vs_p:luv_to_lchuv");
    CHAIN_AOS();
    TIP(alwan_lchuv_to_luv_map_interleave, alwan_lchuv_to_luv_map_planar, "i_vs_p:lchuv_to_luv");

    RELOAD_UNIT();
    TIP(alwan_xyz_to_xyy_map_interleave, alwan_xyz_to_xyy_map_planar, "i_vs_p:xyz_to_xyy");
    CHAIN_AOS();
    TIP(alwan_xyy_to_xyz_map_interleave, alwan_xyy_to_xyz_map_planar, "i_vs_p:xyy_to_xyz");

    /* White point */
    RELOAD_UNIT();
    TIP_W(alwan_xyz_to_lab_map_interleave, alwan_xyz_to_lab_map_planar, &wp, "i_vs_p:xyz_to_lab");
    CHAIN_AOS();
    TIP_W(alwan_lab_to_xyz_map_interleave, alwan_lab_to_xyz_map_planar, &wp, "i_vs_p:lab_to_xyz");

    RELOAD_UNIT();
    TIP_W(alwan_xyz_to_luv_map_interleave, alwan_xyz_to_luv_map_planar, &wp, "i_vs_p:xyz_to_luv");
    CHAIN_AOS();
    TIP_W(alwan_luv_to_xyz_map_interleave, alwan_luv_to_xyz_map_planar, &wp, "i_vs_p:luv_to_xyz");

    /* JzAzBz, IPT */
    RELOAD_UNIT();
    TIP(alwan_xyz_to_jzazbz_map_interleave, alwan_xyz_to_jzazbz_map_planar, "i_vs_p:xyz_to_jzazbz");
    CHAIN_AOS();
    TIP(alwan_jzazbz_to_xyz_map_interleave, alwan_jzazbz_to_xyz_map_planar, "i_vs_p:jzazbz_to_xyz");

    RELOAD_UNIT();
    TIP(alwan_xyz_to_ipt_map_interleave, alwan_xyz_to_ipt_map_planar, "i_vs_p:xyz_to_ipt");
    CHAIN_AOS();
    TIP(alwan_ipt_to_xyz_map_interleave, alwan_ipt_to_xyz_map_planar, "i_vs_p:ipt_to_xyz");

    /* sRGB convenience */
    RELOAD_UNIT();
    TIP(alwan_srgb_to_xyz_map_interleave, alwan_srgb_to_xyz_map_planar, "i_vs_p:srgb_to_xyz");
    CHAIN_AOS();
    TIP(alwan_xyz_to_srgb_map_interleave, alwan_xyz_to_srgb_map_planar, "i_vs_p:xyz_to_srgb");

    RELOAD_UNIT();
    TIP(alwan_srgb_to_oklab_map_interleave, alwan_srgb_to_oklab_map_planar, "i_vs_p:srgb_to_oklab");
    CHAIN_AOS();
    TIP(alwan_oklab_to_srgb_map_interleave, alwan_oklab_to_srgb_map_planar, "i_vs_p:oklab_to_srgb");

    /* HSV/HSL */
    RELOAD_UNIT();
    TIP(alwan_rgb_to_hsv_map_interleave, alwan_rgb_to_hsv_map_planar, "i_vs_p:rgb_to_hsv");
    CHAIN_AOS();
    TIP(alwan_hsv_to_rgb_map_interleave, alwan_hsv_to_rgb_map_planar, "i_vs_p:hsv_to_rgb");

    RELOAD_UNIT();
    TIP(alwan_rgb_to_hsl_map_interleave, alwan_rgb_to_hsl_map_planar, "i_vs_p:rgb_to_hsl");
    CHAIN_AOS();
    TIP(alwan_hsl_to_rgb_map_interleave, alwan_hsl_to_rgb_map_planar, "i_vs_p:hsl_to_rgb");

    /* Convenience */
    RELOAD_UNIT();
    TIP(alwan_rgb_to_cmy_map_interleave, alwan_rgb_to_cmy_map_planar, "i_vs_p:rgb_to_cmy");
    CHAIN_AOS();
    TIP(alwan_cmy_to_rgb_map_interleave, alwan_cmy_to_rgb_map_planar, "i_vs_p:cmy_to_rgb");

    RELOAD_UNIT();
    TIP(alwan_rgb_to_ycocg_map_interleave, alwan_rgb_to_ycocg_map_planar, "i_vs_p:rgb_to_ycocg");
    CHAIN_AOS();
    TIP(alwan_ycocg_to_rgb_map_interleave, alwan_ycocg_to_rgb_map_planar, "i_vs_p:ycocg_to_rgb");

    RELOAD_UNIT();
    TIP(alwan_rgb_to_hwb_map_interleave, alwan_rgb_to_hwb_map_planar, "i_vs_p:rgb_to_hwb");
    CHAIN_AOS();
    TIP(alwan_hwb_to_rgb_map_interleave, alwan_hwb_to_rgb_map_planar, "i_vs_p:hwb_to_rgb");

    /* ICtCp PQ */
    RELOAD_UNIT();
    alwan_rgb_to_ictcp_map_interleave(b.aos, b.grid, 1, PM_COUNT, ASTRIDE, ASTRIDE);
    alwan_rgb_to_ictcp_map_planar(b.po0, b.po1, b.po2, b.pi0, b.pi1, b.pi2,
                                   1, PM_COUNT, PSTRIDE, PSTRIDE);
    if (cmp_planar_vs_aos(b.po0, b.po1, b.po2, b.aos, PM_COUNT, "i_vs_p:rgb_to_ictcp", tol)) goto fail;

    /* YCbCr BT.709 */
    RELOAD_UNIT();
    TIP_I(alwan_rgb_to_ycbcr_map_interleave, alwan_rgb_to_ycbcr_map_planar,
          ALWAN_YCBCR_BT709, "i_vs_p:rgb_to_ycbcr");

    /* Extended (representative) */
    RELOAD_UNIT();
    TIP(alwan_xyz_to_igpgtg_map_interleave, alwan_xyz_to_igpgtg_map_planar, "i_vs_p:xyz_to_igpgtg");
    RELOAD_UNIT();
    TIP(alwan_xyz_to_icacb_map_interleave, alwan_xyz_to_icacb_map_planar, "i_vs_p:xyz_to_icacb");
    RELOAD_UNIT();
    TIP(alwan_xyz_to_hdr_cielab_map_interleave, alwan_xyz_to_hdr_cielab_map_planar, "i_vs_p:xyz_to_hdr_cielab");
    RELOAD_UNIT();
    TIP(alwan_xyz_to_ucs_map_interleave, alwan_xyz_to_ucs_map_planar, "i_vs_p:xyz_to_ucs");
    RELOAD_UNIT();
    TIP(alwan_xyz_to_hunter_lab_map_interleave, alwan_xyz_to_hunter_lab_map_planar, "i_vs_p:xyz_to_hunter_lab");
    RELOAD_UNIT();
    TIP(alwan_xyz_to_prolab_map_interleave, alwan_xyz_to_prolab_map_planar, "i_vs_p:xyz_to_prolab");
    RELOAD_UNIT();
    TIP(alwan_rgb_to_prismatic_map_interleave, alwan_rgb_to_prismatic_map_planar, "i_vs_p:rgb_to_prismatic");
    RELOAD_UNIT();
    TIP(alwan_rgb_to_hcl_map_interleave, alwan_rgb_to_hcl_map_planar, "i_vs_p:rgb_to_hcl");
    RELOAD_UNIT();
    TIP(alwan_rgb_to_ihls_map_interleave, alwan_rgb_to_ihls_map_planar, "i_vs_p:rgb_to_ihls");

    /* Extended with white */
    RELOAD_UNIT();
    TIP_W(alwan_xyz_to_uvw_map_interleave, alwan_xyz_to_uvw_map_planar, &wp, "i_vs_p:xyz_to_uvw");
    RELOAD_UNIT();
    TIP_W(alwan_xyz_to_hunter_lab_custom_map_interleave, alwan_xyz_to_hunter_lab_custom_map_planar, &wp, "i_vs_p:xyz_to_hunter_lab_custom");
    RELOAD_UNIT();
    TIP_W(alwan_xyz_to_prolab_custom_map_interleave, alwan_xyz_to_prolab_custom_map_planar, &wp, "i_vs_p:xyz_to_prolab_custom");

    /* DIN99 */
    RELOAD_LAB();
    TIP_I(alwan_lab_to_din99_map_interleave, alwan_lab_to_din99_map_planar, 0, "i_vs_p:lab_to_din99");

    /* CVD */
    {
        alwan_scalar const sev = ALWAN_LITERAL(0.8);
        RELOAD_UNIT();
        alwan_simulate_cvd_map_interleave(b.aos, b.grid, ALWAN_CVD_PROTANOPIA, sev, PM_COUNT, ASTRIDE, ASTRIDE);
        alwan_simulate_cvd_map_planar(b.po0, b.po1, b.po2, b.pi0, b.pi1, b.pi2,
                                       ALWAN_CVD_PROTANOPIA, sev, PM_COUNT, PSTRIDE, PSTRIDE);
        if (cmp_planar_vs_aos(b.po0, b.po1, b.po2, b.aos, PM_COUNT, "i_vs_p:cvd_protan", tol)) goto fail;
    }

    /* Color correction */
    {
        alwan_rgb lft  = {ALWAN_LITERAL(0.02), ALWAN_LITERAL(0.01), ALWAN_LITERAL(0.03)};
        alwan_rgb gam  = {ALWAN_LITERAL(1.1),  ALWAN_LITERAL(0.95), ALWAN_LITERAL(1.05)};
        alwan_rgb gan  = {ALWAN_LITERAL(1.2),  ALWAN_LITERAL(1.0),  ALWAN_LITERAL(0.9)};
        RELOAD_UNIT();
        alwan_lgg_apply_map_interleave(b.aos, b.grid, &lft, &gam, &gan, PM_COUNT, ASTRIDE, ASTRIDE);
        alwan_lgg_apply_map_planar(b.po0, b.po1, b.po2, b.pi0, b.pi1, b.pi2,
                                    &lft, &gam, &gan, PM_COUNT, PSTRIDE, PSTRIDE);
        if (cmp_planar_vs_aos(b.po0, b.po1, b.po2, b.aos, PM_COUNT, "i_vs_p:lgg_apply", tol)) goto fail;
    }

    pm_free(&b);
    TEST_PASS_MSG(); return 0;
fail:
    pm_free(&b); return 1;
}

/* ================================================================
 * Section C: _v vs _map_interleave (representative subset)
 *
 * Validates that SIMD batch matches per-pixel API.
 * Full coverage is in test 69; this is a sanity check.
 * ================================================================ */

static int test_v_vs_interleave(void) {
    TEST_START("_v vs _map_interleave: representative subset");
    alwan_scalar const tol = TOL_SIMD;
    pm_buf b;
    if (pm_alloc(&b, PM_COUNT)) { pm_free(&b); TEST_FAIL("malloc"); }

    RELOAD_UNIT();
    TVI(alwan_xyz_to_oklab, alwan_xyz_to_oklab_map_interleave,
        alwan_xyz, alwan_oklab, x,y,z, L,a,b, "v_vs_i:xyz_to_oklab");

    RELOAD_UNIT();
    TVI(alwan_xyz_to_jzazbz, alwan_xyz_to_jzazbz_map_interleave,
        alwan_xyz, alwan_jzazbz, x,y,z, Jz,az,bz, "v_vs_i:xyz_to_jzazbz");

    RELOAD_UNIT();
    TVI(alwan_rgb_to_hsv, alwan_rgb_to_hsv_map_interleave,
        alwan_rgb, alwan_hsv, r,g,b, h,s,v, "v_vs_i:rgb_to_hsv");

    RELOAD_UNIT();
    TVI(alwan_rgb_to_cmy, alwan_rgb_to_cmy_map_interleave,
        alwan_rgb, alwan_cmy, r,g,b, c,m,y, "v_vs_i:rgb_to_cmy");

    RELOAD_UNIT();
    TVI(alwan_xyz_to_igpgtg, alwan_xyz_to_igpgtg_map_interleave,
        alwan_xyz, alwan_igpgtg, x,y,z, Ig,Pg,Tg, "v_vs_i:xyz_to_igpgtg");

    RELOAD_UNIT();
    TVI(alwan_srgb_to_xyz, alwan_srgb_to_xyz_map_interleave,
        alwan_rgb, alwan_xyz, r,g,b, x,y,z, "v_vs_i:srgb_to_xyz");

    pm_free(&b);
    TEST_PASS_MSG(); return 0;
fail:
    pm_free(&b); return 1;
}

/* ================================================================
 * Section D: _map_planar_ex typed format validation
 *
 * For each pixel format (U8, U16, F32, F64):
 *   1. Scatter scalar input to typed planar channels
 *   2. For integer formats, collect back (quantized) and run _v as reference
 *   3. Run _map_planar_ex
 *   4. Collect _ex output to scalar and compare
 * ================================================================ */

/* Compare _ex planar output against reference, skipping out-of-range for int */
static int cmp_ex(alwan_scalar const *ex0, alwan_scalar const *ex1, alwan_scalar const *ex2,
                   alwan_scalar const *r0, alwan_scalar const *r1, alwan_scalar const *r2,
                   size_t count, char const *name, alwan_scalar tol, alwan_pixel_format fmt) {
    int is_int = (fmt == ALWAN_PIXEL_U8 || fmt == ALWAN_PIXEL_U16);
    int is_fp  = (fmt == ALWAN_PIXEL_F32 || fmt == ALWAN_PIXEL_F64);
    for (size_t i = 0; i < count; i++) {
        alwan_scalar rv[3] = {r0[i], r1[i], r2[i]};
        if (is_int) {
            int skip = 0;
            for (int c = 0; c < 3; c++)
                if (rv[c] < ALWAN_LITERAL(0.0) || rv[c] > ALWAN_LITERAL(1.0)) skip = 1;
            if (skip) continue;
        }
        alwan_scalar ev[3] = {ex0[i], ex1[i], ex2[i]};
        alwan_scalar mx = ALWAN_LITERAL(0.0);
        for (int c = 0; c < 3; c++) {
            alwan_scalar d = ALWAN_ABS(ev[c] - rv[c]);
            /* For float formats, use relative error to handle large output magnitudes */
            if (is_fp) {
                alwan_scalar ref_mag = ALWAN_ABS(rv[c]);
                if (ref_mag > ALWAN_LITERAL(1.0))
                    d /= ref_mag;
            }
            if (d > mx) mx = d;
        }
        if (mx > tol) {
            printf("[FAIL] %s [%s]: pixel %zu ch max: err=%.6e (tol=%.6e)\n",
                   name, fmt_name(fmt), i, (double)mx, (double)tol);
            return 1;
        }
    }
    return 0;
}

/* Simple 3->3 _map_planar_ex runner */
typedef int (*pex_fn3)(void *, void *, void *, alwan_pixel_format,
                        void const *, void const *, void const *, alwan_pixel_format,
                        size_t, size_t, size_t);
typedef int (*pm_fn3)(alwan_scalar *, alwan_scalar const *, size_t, size_t, size_t);

typedef struct { char const *name; pm_fn3 map_interleave; pex_fn3 planar_ex; } pex_entry3;

static int run_pex3(pex_entry3 const *entries, size_t n, alwan_scalar const *grid, size_t count) {
    size_t const ss = 3 * sizeof(alwan_scalar);
    size_t const max_elem = sizeof(double);
    alwan_scalar *ref0  = (alwan_scalar *)malloc(count * sizeof(alwan_scalar));
    alwan_scalar *ref1  = (alwan_scalar *)malloc(count * sizeof(alwan_scalar));
    alwan_scalar *ref2  = (alwan_scalar *)malloc(count * sizeof(alwan_scalar));
    alwan_scalar *ex0   = (alwan_scalar *)malloc(count * sizeof(alwan_scalar));
    alwan_scalar *ex1   = (alwan_scalar *)malloc(count * sizeof(alwan_scalar));
    alwan_scalar *ex2   = (alwan_scalar *)malloc(count * sizeof(alwan_scalar));
    alwan_scalar *qi0   = (alwan_scalar *)malloc(count * sizeof(alwan_scalar));
    alwan_scalar *qi1   = (alwan_scalar *)malloc(count * sizeof(alwan_scalar));
    alwan_scalar *qi2   = (alwan_scalar *)malloc(count * sizeof(alwan_scalar));
    alwan_scalar *ref_aos = (alwan_scalar *)malloc(count * ss);
    alwan_scalar *qgrid = (alwan_scalar *)malloc(count * ss);
    void *ti0 = malloc(count * max_elem);
    void *ti1 = malloc(count * max_elem);
    void *ti2 = malloc(count * max_elem);
    void *to0 = malloc(count * max_elem);
    void *to1 = malloc(count * max_elem);
    void *to2 = malloc(count * max_elem);
    if (!ref0 || !ref1 || !ref2 || !ex0 || !ex1 || !ex2 ||
        !qi0 || !qi1 || !qi2 || !ref_aos || !qgrid ||
        !ti0 || !ti1 || !ti2 || !to0 || !to1 || !to2) {
        free(ref0); free(ref1); free(ref2); free(ex0); free(ex1); free(ex2);
        free(qi0); free(qi1); free(qi2); free(ref_aos); free(qgrid);
        free(ti0); free(ti1); free(ti2); free(to0); free(to1); free(to2);
        return 1;
    }

    /* Deinterleave source grid */
    for (size_t i = 0; i < count; i++) {
        qi0[i] = grid[i*3+0]; qi1[i] = grid[i*3+1]; qi2[i] = grid[i*3+2];
    }

    for (size_t e = 0; e < n; e++) {
        for (int f = 0; f < 4; f++) {
            alwan_pixel_format fmt = PM_FMTS[f];
            size_t es = fmt_elem_size(fmt);
            alwan_scalar tol = tol_for_fmt(fmt);

            /* Scatter planar input to typed */
            scatter1(ti0, fmt, qi0, count);
            scatter1(ti1, fmt, qi1, count);
            scatter1(ti2, fmt, qi2, count);

            /* Collect quantized input and run reference on that.
             * For U8/U16 this compensates for integer quantization; for F32
             * (when alwan_scalar is double) it compensates for float32
             * precision loss — critical for nonlinear functions like PQ. */
            collect1(ref0, ti0, fmt, count);
            collect1(ref1, ti1, fmt, count);
            collect1(ref2, ti2, fmt, count);
            for (size_t i = 0; i < count; i++) {
                qgrid[i*3+0] = ref0[i]; qgrid[i*3+1] = ref1[i]; qgrid[i*3+2] = ref2[i];
            }
            entries[e].map_interleave(ref_aos, qgrid, count, ss, ss);
            /* Deinterleave reference output */
            for (size_t i = 0; i < count; i++) {
                ref0[i] = ref_aos[i*3+0]; ref1[i] = ref_aos[i*3+1]; ref2[i] = ref_aos[i*3+2];
            }

            /* Run _map_planar_ex */
            entries[e].planar_ex(to0, to1, to2, fmt, ti0, ti1, ti2, fmt, count, es, es);

            /* Collect typed output to scalar */
            collect1(ex0, to0, fmt, count);
            collect1(ex1, to1, fmt, count);
            collect1(ex2, to2, fmt, count);

            if (cmp_ex(ex0, ex1, ex2, ref0, ref1, ref2, count, entries[e].name, tol, fmt)) {
                free(ref0); free(ref1); free(ref2); free(ex0); free(ex1); free(ex2);
                free(qi0); free(qi1); free(qi2); free(ref_aos); free(qgrid);
                free(ti0); free(ti1); free(ti2); free(to0); free(to1); free(to2);
                return 1;
            }
        }
    }

    free(ref0); free(ref1); free(ref2); free(ex0); free(ex1); free(ex2);
    free(qi0); free(qi1); free(qi2); free(ref_aos); free(qgrid);
    free(ti0); free(ti1); free(ti2); free(to0); free(to1); free(to2);
    return 0;
}

static int test_planar_ex_simple(void) {
    TEST_START("_map_planar_ex: simple 3->3 (U8, U16, F32, F64)");
    alwan_scalar *grid = (alwan_scalar *)malloc(PM_COUNT * 3 * sizeof(alwan_scalar));
    if (!grid) TEST_FAIL("malloc");
    generate_unit_grid(grid);

    static pex_entry3 const entries[] = {
        {"xyz_to_oklab",      alwan_xyz_to_oklab_map_interleave,      alwan_xyz_to_oklab_map_planar_ex},
        {"oklab_to_xyz",      alwan_oklab_to_xyz_map_interleave,      alwan_oklab_to_xyz_map_planar_ex},
        {"oklab_to_oklch",    alwan_oklab_to_oklch_map_interleave,    alwan_oklab_to_oklch_map_planar_ex},
        {"oklch_to_oklab",    alwan_oklch_to_oklab_map_interleave,    alwan_oklch_to_oklab_map_planar_ex},
        {"lab_to_lch",        alwan_lab_to_lch_map_interleave,        alwan_lab_to_lch_map_planar_ex},
        {"lch_to_lab",        alwan_lch_to_lab_map_interleave,        alwan_lch_to_lab_map_planar_ex},
        {"luv_to_lchuv",      alwan_luv_to_lchuv_map_interleave,      alwan_luv_to_lchuv_map_planar_ex},
        {"lchuv_to_luv",      alwan_lchuv_to_luv_map_interleave,      alwan_lchuv_to_luv_map_planar_ex},
        {"xyz_to_xyy",        alwan_xyz_to_xyy_map_interleave,        alwan_xyz_to_xyy_map_planar_ex},
        {"xyy_to_xyz",        alwan_xyy_to_xyz_map_interleave,        alwan_xyy_to_xyz_map_planar_ex},
        {"xyz_to_jzazbz",     alwan_xyz_to_jzazbz_map_interleave,    alwan_xyz_to_jzazbz_map_planar_ex},
        {"jzazbz_to_xyz",     alwan_jzazbz_to_xyz_map_interleave,    alwan_jzazbz_to_xyz_map_planar_ex},
        {"jzazbz_to_jzczhz",  alwan_jzazbz_to_jzczhz_map_interleave, alwan_jzazbz_to_jzczhz_map_planar_ex},
        {"jzczhz_to_jzazbz",  alwan_jzczhz_to_jzazbz_map_interleave, alwan_jzczhz_to_jzazbz_map_planar_ex},
        {"xyz_to_ipt",        alwan_xyz_to_ipt_map_interleave,        alwan_xyz_to_ipt_map_planar_ex},
        {"ipt_to_xyz",        alwan_ipt_to_xyz_map_interleave,        alwan_ipt_to_xyz_map_planar_ex},
    };

    int r = run_pex3(entries, sizeof(entries)/sizeof(entries[0]), grid, PM_COUNT);
    free(grid);
    if (r) return 1;
    TEST_PASS_MSG(); return 0;
}

static int test_planar_ex_srgb(void) {
    TEST_START("_map_planar_ex: sRGB convenience (U8, U16, F32, F64)");
    alwan_scalar *grid = (alwan_scalar *)malloc(PM_COUNT * 3 * sizeof(alwan_scalar));
    if (!grid) TEST_FAIL("malloc");
    generate_unit_grid(grid);

    static pex_entry3 const entries[] = {
        {"srgb_to_xyz",   alwan_srgb_to_xyz_map_interleave,   alwan_srgb_to_xyz_map_planar_ex},
        {"xyz_to_srgb",   alwan_xyz_to_srgb_map_interleave,   alwan_xyz_to_srgb_map_planar_ex},
        {"srgb_to_lab",   alwan_srgb_to_lab_map_interleave,   alwan_srgb_to_lab_map_planar_ex},
        {"lab_to_srgb",   alwan_lab_to_srgb_map_interleave,   alwan_lab_to_srgb_map_planar_ex},
        {"srgb_to_oklab", alwan_srgb_to_oklab_map_interleave, alwan_srgb_to_oklab_map_planar_ex},
        {"oklab_to_srgb", alwan_oklab_to_srgb_map_interleave, alwan_oklab_to_srgb_map_planar_ex},
    };

    int r = run_pex3(entries, sizeof(entries)/sizeof(entries[0]), grid, PM_COUNT);
    free(grid);
    if (r) return 1;
    TEST_PASS_MSG(); return 0;
}

static int test_planar_ex_hsv_hsl(void) {
    TEST_START("_map_planar_ex: HSV/HSL (U8, U16, F32, F64)");
    alwan_scalar *grid = (alwan_scalar *)malloc(PM_COUNT * 3 * sizeof(alwan_scalar));
    if (!grid) TEST_FAIL("malloc");
    generate_unit_grid(grid);

    static pex_entry3 const entries[] = {
        {"rgb_to_hsv", alwan_rgb_to_hsv_map_interleave, alwan_rgb_to_hsv_map_planar_ex},
        {"hsv_to_rgb", alwan_hsv_to_rgb_map_interleave, alwan_hsv_to_rgb_map_planar_ex},
        {"rgb_to_hsl", alwan_rgb_to_hsl_map_interleave, alwan_rgb_to_hsl_map_planar_ex},
        {"hsl_to_rgb", alwan_hsl_to_rgb_map_interleave, alwan_hsl_to_rgb_map_planar_ex},
    };

    int r = run_pex3(entries, sizeof(entries)/sizeof(entries[0]), grid, PM_COUNT);
    free(grid);
    if (r) return 1;
    TEST_PASS_MSG(); return 0;
}

static int test_planar_ex_convenience(void) {
    TEST_START("_map_planar_ex: convenience (U8, U16, F32, F64)");
    alwan_scalar *grid = (alwan_scalar *)malloc(PM_COUNT * 3 * sizeof(alwan_scalar));
    if (!grid) TEST_FAIL("malloc");
    generate_unit_grid(grid);

    static pex_entry3 const entries[] = {
        {"rgb_to_cmy",   alwan_rgb_to_cmy_map_interleave,   alwan_rgb_to_cmy_map_planar_ex},
        {"cmy_to_rgb",   alwan_cmy_to_rgb_map_interleave,   alwan_cmy_to_rgb_map_planar_ex},
        {"rgb_to_ycocg", alwan_rgb_to_ycocg_map_interleave, alwan_rgb_to_ycocg_map_planar_ex},
        {"ycocg_to_rgb", alwan_ycocg_to_rgb_map_interleave, alwan_ycocg_to_rgb_map_planar_ex},
        {"rgb_to_hwb",   alwan_rgb_to_hwb_map_interleave,   alwan_rgb_to_hwb_map_planar_ex},
        {"hwb_to_rgb",   alwan_hwb_to_rgb_map_interleave,   alwan_hwb_to_rgb_map_planar_ex},
        {"hsv_to_hwb",   alwan_hsv_to_hwb_map_interleave,   alwan_hsv_to_hwb_map_planar_ex},
        {"hwb_to_hsv",   alwan_hwb_to_hsv_map_interleave,   alwan_hwb_to_hsv_map_planar_ex},
    };

    int r = run_pex3(entries, sizeof(entries)/sizeof(entries[0]), grid, PM_COUNT);
    free(grid);
    if (r) return 1;
    TEST_PASS_MSG(); return 0;
}

static int test_planar_ex_extended(void) {
    TEST_START("_map_planar_ex: extended (U8, U16, F32, F64)");
    alwan_scalar *grid = (alwan_scalar *)malloc(PM_COUNT * 3 * sizeof(alwan_scalar));
    if (!grid) TEST_FAIL("malloc");
    generate_unit_grid(grid);

    static pex_entry3 const entries[] = {
        {"xyz_to_igpgtg",     alwan_xyz_to_igpgtg_map_interleave,     alwan_xyz_to_igpgtg_map_planar_ex},
        {"igpgtg_to_xyz",     alwan_igpgtg_to_xyz_map_interleave,     alwan_igpgtg_to_xyz_map_planar_ex},
        {"xyz_to_icacb",      alwan_xyz_to_icacb_map_interleave,      alwan_xyz_to_icacb_map_planar_ex},
        {"icacb_to_xyz",      alwan_icacb_to_xyz_map_interleave,      alwan_icacb_to_xyz_map_planar_ex},
        {"xyz_to_hdr_cielab", alwan_xyz_to_hdr_cielab_map_interleave, alwan_xyz_to_hdr_cielab_map_planar_ex},
        {"hdr_cielab_to_xyz", alwan_hdr_cielab_to_xyz_map_interleave, alwan_hdr_cielab_to_xyz_map_planar_ex},
        {"xyz_to_hdr_ipt",    alwan_xyz_to_hdr_ipt_map_interleave,    alwan_xyz_to_hdr_ipt_map_planar_ex},
        {"hdr_ipt_to_xyz",    alwan_hdr_ipt_to_xyz_map_interleave,    alwan_hdr_ipt_to_xyz_map_planar_ex},
        {"xyz_to_ucs",        alwan_xyz_to_ucs_map_interleave,        alwan_xyz_to_ucs_map_planar_ex},
        {"ucs_to_xyz",        alwan_ucs_to_xyz_map_interleave,        alwan_ucs_to_xyz_map_planar_ex},
        {"xyz_to_osa_ucs",    alwan_xyz_to_osa_ucs_map_interleave,    alwan_xyz_to_osa_ucs_map_planar_ex},
        {"osa_ucs_to_xyz",    alwan_osa_ucs_to_xyz_map_interleave,    alwan_osa_ucs_to_xyz_map_planar_ex},
        {"xyz_to_hunter_lab", alwan_xyz_to_hunter_lab_map_interleave,  alwan_xyz_to_hunter_lab_map_planar_ex},
        {"hunter_lab_to_xyz", alwan_hunter_lab_to_xyz_map_interleave,  alwan_hunter_lab_to_xyz_map_planar_ex},
        {"xyz_to_prolab",     alwan_xyz_to_prolab_map_interleave,      alwan_xyz_to_prolab_map_planar_ex},
        {"prolab_to_xyz",     alwan_prolab_to_xyz_map_interleave,      alwan_prolab_to_xyz_map_planar_ex},
        {"rgb_to_prismatic",  alwan_rgb_to_prismatic_map_interleave,   alwan_rgb_to_prismatic_map_planar_ex},
        {"prismatic_to_rgb",  alwan_prismatic_to_rgb_map_interleave,   alwan_prismatic_to_rgb_map_planar_ex},
        {"rgb_to_hcl",        alwan_rgb_to_hcl_map_interleave,         alwan_rgb_to_hcl_map_planar_ex},
        {"hcl_to_rgb",        alwan_hcl_to_rgb_map_interleave,         alwan_hcl_to_rgb_map_planar_ex},
        {"rgb_to_ihls",       alwan_rgb_to_ihls_map_interleave,        alwan_rgb_to_ihls_map_planar_ex},
        {"ihls_to_rgb",       alwan_ihls_to_rgb_map_interleave,        alwan_ihls_to_rgb_map_planar_ex},
    };

    int r = run_pex3(entries, sizeof(entries)/sizeof(entries[0]), grid, PM_COUNT);
    free(grid);
    if (r) return 1;
    TEST_PASS_MSG(); return 0;
}

/* ================================================================
 * Main Test Runner
 * ================================================================ */

int test_70_planar_map_main(void) {
    printf("========================================\n");
    printf("Test Suite 70: Comprehensive Planar Map Validation\n");
    printf("========================================\n\n");

    test_count = 0;
    test_passed = 0;

    /* Section A: _v vs _map_planar */
    printf("Section A: _v vs _map_planar\n");
    printf("--------------------------------\n");
    if (test_v_vs_planar_core()) return 1;
    if (test_v_vs_planar_srgb()) return 1;
    if (test_v_vs_planar_white()) return 1;
    if (test_v_vs_planar_hsv_hsl()) return 1;
    if (test_v_vs_planar_ictcp()) return 1;
    if (test_v_vs_planar_convenience()) return 1;
    if (test_v_vs_planar_extended()) return 1;
    if (test_v_vs_planar_cvd()) return 1;
    if (test_v_vs_planar_colorcorr()) return 1;

    /* Section B: _map_interleave vs _map_planar */
    printf("\nSection B: _map_interleave vs _map_planar\n");
    printf("--------------------------------\n");
    if (test_interleave_vs_planar()) return 1;

    /* Section C: _v vs _map_interleave (sanity check) */
    printf("\nSection C: _v vs _map_interleave\n");
    printf("--------------------------------\n");
    if (test_v_vs_interleave()) return 1;

    /* Section D: _map_planar_ex typed format */
    printf("\nSection D: _map_planar_ex typed format validation\n");
    printf("--------------------------------\n");
    if (test_planar_ex_simple()) return 1;
    if (test_planar_ex_srgb()) return 1;
    if (test_planar_ex_hsv_hsl()) return 1;
    if (test_planar_ex_convenience()) return 1;
    if (test_planar_ex_extended()) return 1;

    printf("\n========================================\n");
    printf("Test Suite 70 Summary: %d/%d passed\n", test_passed, test_count);
    printf("========================================\n");

    return (test_passed == test_count) ? 0 : 1;
}
