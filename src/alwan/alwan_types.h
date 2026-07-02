/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Cross-platform type definitions
 * Standalone header: includes only alwan_platform.h
 */

#ifndef ALWAN_TYPES_H
#define ALWAN_TYPES_H

#include "alwan_platform.h"
#include "alwan_build_config.h"  /* ALWAN_WITH_F32 / ALWAN_WITH_F64 precision gates */

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * C Backend: Dual-Precision Types
 *
 * Both f32 and f64 variants are always available.
 * Use explicit _f32 or _f64 suffixed type names.
 * ================================================================ */

/* --- Scalar precision aliases --- */
typedef float  alwan_f32;
typedef double alwan_f64;

/* --- CAM surround enums (needed by precision-typed viewing condition structs) --- */
typedef enum {
    ALWAN_CIECAM02_SURROUND_AVERAGE = 0,
    ALWAN_CIECAM02_SURROUND_DIM = 1,
    ALWAN_CIECAM02_SURROUND_DARK = 2
} alwan_ciecam02_surround;

typedef enum {
    ALWAN_CAM16_SURROUND_AVERAGE = 0,
    ALWAN_CAM16_SURROUND_DIM = 1,
    ALWAN_CAM16_SURROUND_DARK = 2
} alwan_cam16_surround;

typedef enum {
    ALWAN_ZCAM_SURROUND_AVERAGE = 0,
    ALWAN_ZCAM_SURROUND_DIM = 1,
    ALWAN_ZCAM_SURROUND_DARK = 2
} alwan_zcam_surround;

typedef enum {
    ALWAN_RLAB_SURROUND_AVERAGE = 0,
    ALWAN_RLAB_SURROUND_DIM = 1,
    ALWAN_RLAB_SURROUND_DARK = 2
} alwan_rlab_surround;

typedef enum {
    ALWAN_HUNT_SURROUND_NORMAL = 0,
    ALWAN_HUNT_SURROUND_DIM = 1,
    ALWAN_HUNT_SURROUND_DARK = 2
} alwan_hunt_surround;

typedef enum {
    ALWAN_HELLWIG2022_SURROUND_AVERAGE = 0,
    ALWAN_HELLWIG2022_SURROUND_DIM = 1,
    ALWAN_HELLWIG2022_SURROUND_DARK = 2
} alwan_hellwig2022_surround;

typedef enum {
    ALWAN_LLAB_SURROUND_AVERAGE = 0,
    ALWAN_LLAB_SURROUND_DIM = 1,
    ALWAN_LLAB_SURROUND_DARK = 2
} alwan_llab_surround;

/* --- f32 types --- */
#define ALWAN_T float
#define ALWAN_SUFFIX _f32
#include "alwan_types_gen.inc"
#undef ALWAN_T
#undef ALWAN_SUFFIX

/* --- f64 types --- */
#define ALWAN_T double
#define ALWAN_SUFFIX _f64
#include "alwan_types_gen.inc"
#undef ALWAN_T
#undef ALWAN_SUFFIX

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-Precision Types Only
 * ================================================================ */

/* alwan_f32 so the shared f32 deterministic coeff tables (static const alwan_f32[],
 * reused by the GPU det polynomials for bit-exactness) compile on GPU -- same
 * definition C uses. alwan_f64 is deliberately NOT defined here: the f64 coeff
 * tables are guarded C-only in the coeff headers (GPU runs single precision), so
 * nothing on the GPU path references it. */
ALWAN_TYPE_DEF float  alwan_f32;

/* Math Types */
ALWAN_TYPE_DEF struct {
    alwan_scalar v[2];
} alwan_vec2;

ALWAN_TYPE_DEF struct {
    alwan_scalar v[3];
} alwan_vec3;

ALWAN_TYPE_DEF struct {
    alwan_scalar m[9];
} alwan_mat3x3;

ALWAN_TYPE_DEF struct {
    alwan_scalar m[16];
} alwan_mat4x4;

/* Semantic Color Types */
ALWAN_TYPE_DEF struct { alwan_scalar r, g, b; }     alwan_rgb;
ALWAN_TYPE_DEF struct { alwan_scalar c, m, y, k; }  alwan_cmyk;
ALWAN_TYPE_DEF struct { alwan_scalar c, m, y; }     alwan_cmy;
ALWAN_TYPE_DEF struct { alwan_scalar h, s, v; }     alwan_hsv;
ALWAN_TYPE_DEF struct { alwan_scalar h, s, l; }     alwan_hsl;
ALWAN_TYPE_DEF struct { alwan_scalar h, s, p; }     alwan_hsp;
ALWAN_TYPE_DEF struct { alwan_scalar h, s, p; }     alwan_hsplog;
ALWAN_TYPE_DEF struct { alwan_scalar h, s, y; }     alwan_hsy;
ALWAN_TYPE_DEF struct { alwan_scalar h, w, b; }     alwan_hwb;
ALWAN_TYPE_DEF struct { alwan_scalar x, y, z; }     alwan_xyz;
ALWAN_TYPE_DEF struct { alwan_scalar x, y, Y; }     alwan_xyy;
ALWAN_TYPE_DEF struct { alwan_scalar L, a, b; }     alwan_lab;
ALWAN_TYPE_DEF struct { alwan_scalar L, u, v; }     alwan_luv;
ALWAN_TYPE_DEF struct { alwan_scalar L, C, h; }     alwan_lch;
ALWAN_TYPE_DEF struct { alwan_scalar L, C, h; }     alwan_lchuv;
ALWAN_TYPE_DEF struct { alwan_scalar L, a, b; }     alwan_oklab;
ALWAN_TYPE_DEF struct { alwan_scalar L, C, h; }     alwan_oklch;
ALWAN_TYPE_DEF struct { alwan_scalar Jz, az, bz; }  alwan_jzazbz;
ALWAN_TYPE_DEF struct { alwan_scalar Jz, Cz, hz; }  alwan_jzczhz;
ALWAN_TYPE_DEF struct { alwan_scalar I, Ct, Cp; }   alwan_ictcp;
ALWAN_TYPE_DEF struct { alwan_scalar I, P, T; }     alwan_ipt;
ALWAN_TYPE_DEF struct { alwan_scalar Ig, Pg, Tg; }  alwan_igpgtg;
ALWAN_TYPE_DEF struct { alwan_scalar I, Ca, Cb; }   alwan_icacb;
ALWAN_TYPE_DEF struct { alwan_scalar Y, Cb, Cr; }   alwan_ycbcr;
ALWAN_TYPE_DEF struct { alwan_scalar Y, Co, Cg; }   alwan_ycocg;
ALWAN_TYPE_DEF struct { alwan_scalar Yc, Cbc, Crc; } alwan_yccbccrc;
ALWAN_TYPE_DEF struct { alwan_scalar U, V, W; }     alwan_uvw;
ALWAN_TYPE_DEF struct { alwan_scalar L99, a99, b99; } alwan_din99;
ALWAN_TYPE_DEF struct { alwan_scalar L, a, b; }     alwan_hunter_lab;
ALWAN_TYPE_DEF struct { alwan_scalar I, C, h; }     alwan_iptch;
ALWAN_TYPE_DEF struct { alwan_scalar L, a, b; }     alwan_prolab;
ALWAN_TYPE_DEF struct { alwan_scalar L, j, g; }     alwan_osa_ucs;
ALWAN_TYPE_DEF struct { alwan_scalar U, V, W; }     alwan_ucs;
ALWAN_TYPE_DEF struct { alwan_scalar L, s, h; }     alwan_prismatic;
ALWAN_TYPE_DEF struct { alwan_scalar H, C, L; }     alwan_hcl;
ALWAN_TYPE_DEF struct { alwan_scalar H, L, S; }     alwan_ihls;
ALWAN_TYPE_DEF struct { alwan_scalar J, a, b; }     alwan_cam_jab;
ALWAN_TYPE_DEF struct { alwan_scalar h, s, l; }     alwan_hsluv;
ALWAN_TYPE_DEF struct { alwan_scalar h, s, l; }     alwan_hpluv;
ALWAN_TYPE_DEF struct { alwan_scalar h, s, l; }     alwan_okhsl;
ALWAN_TYPE_DEF struct { alwan_scalar h, s, v; }     alwan_okhsv;
ALWAN_TYPE_DEF struct { alwan_scalar h, s, l; }     alwan_cubehelix;
ALWAN_TYPE_DEF struct { alwan_scalar H, L, C; }     alwan_hlc;
ALWAN_TYPE_DEF struct {
    alwan_scalar display_primaries_xy[6];
    alwan_scalar white_point_xy[2];
    alwan_scalar max_luminance;
    alwan_scalar min_luminance;
} alwan_st2086_metadata;
ALWAN_TYPE_DEF struct {
    alwan_scalar max_cll;
    alwan_scalar max_fall;
} alwan_content_light_level;

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_TYPES_H */
