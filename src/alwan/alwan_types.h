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

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * C Backend: Dual-Precision Types
 *
 * Both f32 and f64 variants are always available.
 * Unsuffixed names alias to double (f64).
 * ================================================================ */

/* --- Scalar precision aliases --- */
typedef float  alwan_f32;
typedef double alwan_f64;

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

/* --- Backward-compat aliases (unsuffixed = f64; prefer explicit _f32/_f64) --- */
#define alwan_vec2              alwan_vec2_f64
#define alwan_vec3              alwan_vec3_f64
#define alwan_mat3x3            alwan_mat3x3_f64
#define alwan_mat4x4            alwan_mat4x4_f64
#define alwan_rgb               alwan_rgb_f64
#define alwan_cmyk              alwan_cmyk_f64
#define alwan_cmy               alwan_cmy_f64
#define alwan_hsv               alwan_hsv_f64
#define alwan_hsl               alwan_hsl_f64
#define alwan_hsp               alwan_hsp_f64
#define alwan_hsplog            alwan_hsplog_f64
#define alwan_hsy               alwan_hsy_f64
#define alwan_xyz               alwan_xyz_f64
#define alwan_xyy               alwan_xyy_f64
#define alwan_lab               alwan_lab_f64
#define alwan_luv               alwan_luv_f64
#define alwan_lch               alwan_lch_f64
#define alwan_lchuv             alwan_lchuv_f64
#define alwan_oklab             alwan_oklab_f64
#define alwan_oklch             alwan_oklch_f64
#define alwan_jzazbz            alwan_jzazbz_f64
#define alwan_jzczhz            alwan_jzczhz_f64
#define alwan_ictcp             alwan_ictcp_f64
#define alwan_ipt               alwan_ipt_f64
#define alwan_igpgtg            alwan_igpgtg_f64
#define alwan_icacb             alwan_icacb_f64
#define alwan_ycbcr             alwan_ycbcr_f64
#define alwan_ycocg             alwan_ycocg_f64
#define alwan_yccbccrc          alwan_yccbccrc_f64
#define alwan_uvw               alwan_uvw_f64
#define alwan_din99             alwan_din99_f64
#define alwan_hunter_lab        alwan_hunter_lab_f64
#define alwan_iptch             alwan_iptch_f64
#define alwan_prolab            alwan_prolab_f64
#define alwan_osa_ucs           alwan_osa_ucs_f64
#define alwan_ucs               alwan_ucs_f64
#define alwan_prismatic         alwan_prismatic_f64
#define alwan_hcl               alwan_hcl_f64
#define alwan_ihls              alwan_ihls_f64
#define alwan_cam_jab           alwan_cam_jab_f64
#define alwan_hsluv             alwan_hsluv_f64
#define alwan_hpluv             alwan_hpluv_f64
#define alwan_okhsl             alwan_okhsl_f64
#define alwan_okhsv             alwan_okhsv_f64
#define alwan_cubehelix         alwan_cubehelix_f64
#define alwan_hlc               alwan_hlc_f64
#define alwan_st2086_metadata   alwan_st2086_metadata_f64
#define alwan_content_light_level alwan_content_light_level_f64

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-Precision Types Only
 * ================================================================ */

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
