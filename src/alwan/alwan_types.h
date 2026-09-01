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

/* Kim, Weyrich and Kautz (2009) media parameter E. The four values the paper
 * publishes; the model has no continuum, so this is an enum rather than a
 * float. Ordered by increasing E. */
typedef enum {
    ALWAN_KIM2009_MEDIA_HIGH_LUMINANCE_LCD = 0,  /* E = 1.0    */
    ALWAN_KIM2009_MEDIA_TRANSPARENT_AD     = 1,  /* E = 1.2175 */
    ALWAN_KIM2009_MEDIA_CRT                = 2,  /* E = 1.4572 */
    ALWAN_KIM2009_MEDIA_REFLECTIVE_PAPER   = 3   /* E = 1.7526 */
} alwan_kim2009_media;

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

/* --- Table addressing modes --------------------------------------------
 *
 * How a float coordinate becomes a table position. This is ADDRESSING, not
 * 1-d signal reconstruction: alwan_interp_method and alwan_resample_method
 * (Sprague, Lagrange, Akima, PCHIP) are shipped, pinned reconstruction enums
 * with their own call sites, and renumbering one of those to merge them here
 * would be an ABI event that buys no safety.
 *
 * Values are pinned from the first commit: never renamed, never renumbered,
 * never reused. New modes append. */
typedef enum {
    /* LINEAR is 0 so a zero-initialized or default-constructed mode selects
     * interpolation, not nearest-neighbour. A struct that is memset to zero
     * then sampled would otherwise band silently, which is the failure this
     * enum exists to prevent. Renumbered before the first public release;
     * 1.0.0 was a private review. */
    ALWAN_SAMPLE_LINEAR      = 0,
    ALWAN_SAMPLE_NEAREST     = 1,
    ALWAN_SAMPLE_BILINEAR    = 2,
    ALWAN_SAMPLE_TRILINEAR   = 3,
    ALWAN_SAMPLE_TETRAHEDRAL = 4,
    ALWAN_SAMPLE_CATMULL_ROM = 5,
    /* OR into the mode argument of a SCALAR reader. A non-finite or
     * out-of-[0,1] coordinate then returns ALWAN_E_RANGE and leaves *result
     * untouched instead of addressing the clamped edge. An enumerator rather
     * than a bare #define so the parameter stays typed in C++. */
    ALWAN_SAMPLE_STRICT      = 0x100
} alwan_sample_mode;

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
/* Unsigned integer types for the normalize core (bit-depth scaling). GPU
 * shading languages have no 8/16-bit integer scalars by default -- a 32-bit
 * uint holds every value the [0, 2^depth-1] domain produces. */
/* `uint` is a SHADING-LANGUAGE spelling. HLSL and GLSL have it; C++ does not,
 * and the Halide backend is a C++ eDSL rather than a GPU language -- it only
 * shares this branch because it, too, is single-precision. Spelling it
 * `unsigned int` there is the same 32-bit type the comment above describes, so
 * every backend keeps the width the [0, 2^depth-1] domain needs. */
#if ALWAN_BACKEND == ALWAN_BACKEND_HALIDE
ALWAN_TYPE_DEF unsigned int alwan_uint16;
ALWAN_TYPE_DEF unsigned int alwan_uint8;
#else
ALWAN_TYPE_DEF uint   alwan_uint16;
ALWAN_TYPE_DEF uint   alwan_uint8;

/* The <stddef.h> and <stdint.h> spellings, for a target that has neither.
 * alwan_config.h and alwan_half_core.h include those headers only on the CPU
 * backends; on HLSL and GLSL these stand in, so table/lut/vision addressing and
 * the half bit layout keep one spelling across all four backends.
 *
 * #define rather than typedef because GLSL has no typedef, the same reason
 * alwan_sample_mode below is a #define. Both languages spell a 32-bit unsigned
 * `uint`, which is wide enough for every index and bit pattern the cores form:
 * table addressing is bounded by the embedded table sizes, and the half
 * conversions work on 32-bit float bits. Halide is C++ and takes the real
 * headers, so it never reaches this branch. */
# define size_t   uint
# define uint32_t uint
# define uint16_t uint
# define uint8_t  uint
# define int32_t  int
# define int16_t  int
# define int8_t   int
#endif

/* Table addressing modes. GLSL has no enum, so the mode is a plain int and
 * the values are #defines; HLSL and Halide accept the same spelling. Values
 * match the C enum above exactly -- see its comment for the pinning rule. */
/* Hunt viewing surround, same treatment: the C branch spells it as an enum,
 * and hunt_surround_Nc_v / _Nb_v take it by value on every backend. Values
 * match the C enum exactly. */
#define alwan_hunt_surround int
#define ALWAN_HUNT_SURROUND_NORMAL 0
#define ALWAN_HUNT_SURROUND_DIM    1
#define ALWAN_HUNT_SURROUND_DARK   2

#define alwan_sample_mode int
#define ALWAN_SAMPLE_LINEAR      0
#define ALWAN_SAMPLE_NEAREST     1
#define ALWAN_SAMPLE_BILINEAR    2
#define ALWAN_SAMPLE_TRILINEAR   3
#define ALWAN_SAMPLE_TETRAHEDRAL 4
#define ALWAN_SAMPLE_CATMULL_ROM 5
#define ALWAN_SAMPLE_STRICT      256

/* Math Types */
ALWAN_TYPE_DEF struct {
    alwan_scalar v[2];
} alwan_vec2;

/* Resolved table cell -- see the .inc twin for the invariant it carries. */
ALWAN_TYPE_DEF struct {
    int i0;
    int i1;
    alwan_scalar frac;
} alwan_table_cell;

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

/* Split the STRICT flag from the base mode. Outside the backend branch: both
 * branches define the same values, so these macros are backend-independent. */
#define ALWAN_SAMPLE_BASE(m) ((int)(m) & 0xFF)
#define ALWAN_SAMPLE_IS_STRICT(m) (((int)(m) & ALWAN_SAMPLE_STRICT) != 0)

#endif /* ALWAN_TYPES_H */
