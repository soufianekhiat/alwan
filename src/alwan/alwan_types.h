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

/* ----------------------------------------------------------------
 * Math Types
 * ---------------------------------------------------------------- */

#if ALWAN_BACKEND == ALWAN_BACKEND_C || ALWAN_BACKEND == ALWAN_BACKEND_HALIDE
  /* C/C++ and Halide: struct-based types */

/* 2-component vector (for xy chromaticity coordinates) */
typedef struct {
    alwan_scalar v[2];
} alwan_vec2;

/* 3-component vector */
typedef struct {
    alwan_scalar v[3];
} alwan_vec3;

/* 3x3 matrix stored in row-major order: [m00 m01 m02 m10 m11 m12 m20 m21 m22] */
typedef struct {
    alwan_scalar m[9];
} alwan_mat3x3;

#elif ALWAN_BACKEND == ALWAN_BACKEND_HLSL
  /* HLSL: use native vector/matrix types */
typedef float2   alwan_vec2;
typedef float3   alwan_vec3;
typedef float3x3 alwan_mat3x3;

#endif

/* ----------------------------------------------------------------
 * Semantic Color Types (for type-safe I/O)
 * ---------------------------------------------------------------- */

/*
 * Semantic color types provide type safety while maintaining API versatility.
 * All types are layout-compatible with alwan_vec3, allowing:
 *   - Safe copy via ALWAN_MEMCPY: ALWAN_MEMCPY(&vec, &rgb, sizeof(alwan_vec3))
 *   - Member pointer passing: alwan_function(&rgb.r, &out.r)
 *   - Array interpretation: alwan_scalar* ptr = &rgb.r;
 *
 * STRICT ALIASING NOTE:
 * Direct casting between semantic types (e.g., (alwan_vec3*)&rgb) may violate
 * C strict aliasing rules. Use ALWAN_MEMCPY for guaranteed safety. The library
 * internally uses ALWAN_MEMCPY which can be overridden in alwan_config.h.
 */

#if ALWAN_BACKEND == ALWAN_BACKEND_C || ALWAN_BACKEND == ALWAN_BACKEND_HALIDE
  /* C/C++ and Halide: struct-based color types */

/* RGB color (red, green, blue) - typically linear or encoded depending on context */
typedef struct {
    alwan_scalar r, g, b;
} alwan_rgb;

/* CMYK color (cyan, magenta, yellow, black) - note: uses 4 components, not compatible with vec3 */
typedef struct {
    alwan_scalar c, m, y, k;
} alwan_cmyk;

/* CMY color (cyan, magenta, yellow) */
typedef struct {
    alwan_scalar c, m, y;
} alwan_cmy;

/* HSV color (hue [0-360], saturation [0-1], value [0-1]) */
typedef struct {
    alwan_scalar h, s, v;
} alwan_hsv;

/* HSL color (hue [0-360], saturation [0-1], lightness [0-1]) */
typedef struct {
    alwan_scalar h, s, l;
} alwan_hsl;

/* XYZ tristimulus values (CIE 1931) */
typedef struct {
    alwan_scalar x, y, z;
} alwan_xyz;

/* xyY chromaticity + luminance (CIE 1931) */
typedef struct {
    alwan_scalar x, y, Y;
} alwan_xyy;

/* CIE Lab color (L* [0-100], a*, b*) */
typedef struct {
    alwan_scalar L, a, b;
} alwan_lab;

/* CIE Luv color (L* [0-100], u*, v*) */
typedef struct {
    alwan_scalar L, u, v;
} alwan_luv;

/* CIE LCh (cylindrical Lab): Lightness, Chroma, Hue */
typedef struct {
    alwan_scalar L, C, h;
} alwan_lch;

/* CIE LCh(uv) (cylindrical Luv): Lightness, Chroma, Hue */
typedef struct {
    alwan_scalar L, C, h;
} alwan_lchuv;

/* Oklab color space (modern perceptual) */
typedef struct {
    alwan_scalar L, a, b;
} alwan_oklab;

/* Oklch color space (cylindrical Oklab) */
typedef struct {
    alwan_scalar L, C, h;
} alwan_oklch;

/* Jzazbz color space (HDR perceptual) */
typedef struct {
    alwan_scalar Jz, az, bz;
} alwan_jzazbz;

/* JzCzhz (cylindrical Jzazbz) */
typedef struct {
    alwan_scalar Jz, Cz, hz;
} alwan_jzczhz;

/* ICtCp color space (ITU-R BT.2100 HDR) */
typedef struct {
    alwan_scalar I, Ct, Cp;
} alwan_ictcp;

/* IPT color space */
typedef struct {
    alwan_scalar I, P, T;
} alwan_ipt;

/* IgPgTg color space (Ebner & Fairchild 1998) */
typedef struct {
    alwan_scalar Ig, Pg, Tg;
} alwan_igpgtg;

/* ICaCb color space (Zhang & Wandell 1996, 1997) */
typedef struct {
    alwan_scalar I, Ca, Cb;
} alwan_icacb;

/* YCbCr color (luma + chroma) */
typedef struct {
    alwan_scalar Y, Cb, Cr;
} alwan_ycbcr;

/* YCoCg color space (luma + orange-cyan + green-magenta) */
typedef struct {
    alwan_scalar Y, Co, Cg;
} alwan_ycocg;

/* Y'Cb'Cr'c' color space (luma + chroma with reduced range) */
typedef struct {
    alwan_scalar Yc, Cbc, Crc;
} alwan_yccbccrc;

/* UVW color space (CIE 1964) */
typedef struct {
    alwan_scalar U, V, W;
} alwan_uvw;

/* DIN99 color space */
typedef struct {
    alwan_scalar L99, a99, b99;
} alwan_din99;

/* Hunter Lab color space */
typedef struct {
    alwan_scalar L, a, b;
} alwan_hunter_lab;

/* IPT cylindrical form (I, Chroma, Hue) */
typedef struct {
    alwan_scalar I, C, h;
} alwan_iptch;

/* ProLab color space */
typedef struct {
    alwan_scalar L, a, b;
} alwan_prolab;

/* OSA-UCS color space (Ljg) */
typedef struct {
    alwan_scalar L, j, g;
} alwan_osa_ucs;

/* CIE 1960 UCS color space */
typedef struct {
    alwan_scalar U, V, W;
} alwan_ucs;

/* Prismatic color space */
typedef struct {
    alwan_scalar L, s, h;
} alwan_prismatic;

/* HCL color space (Hue, Chroma, Luminance - in RGB context) */
typedef struct {
    alwan_scalar H, C, L;
} alwan_hcl;

/* IHLS color space (Improved HLS) */
typedef struct {
    alwan_scalar H, L, S;
} alwan_ihls;

/* CAM Jab color (J, a, b - for CAM02/CAM16) */
typedef struct {
    alwan_scalar J, a, b;
} alwan_cam_jab;

#elif ALWAN_BACKEND == ALWAN_BACKEND_HLSL
  /* HLSL: typedef to float3 */
typedef float3 alwan_rgb;
typedef float3 alwan_cmy;
typedef float3 alwan_hsv;
typedef float3 alwan_hsl;
typedef float3 alwan_xyz;
typedef float3 alwan_xyy;
typedef float3 alwan_lab;
typedef float3 alwan_luv;
typedef float3 alwan_lch;
typedef float3 alwan_lchuv;
typedef float3 alwan_oklab;
typedef float3 alwan_oklch;
typedef float3 alwan_jzazbz;
typedef float3 alwan_jzczhz;
typedef float3 alwan_ictcp;
typedef float3 alwan_ipt;
typedef float3 alwan_igpgtg;
typedef float3 alwan_icacb;
typedef float3 alwan_ycbcr;
typedef float3 alwan_ycocg;
typedef float3 alwan_yccbccrc;
typedef float3 alwan_uvw;
typedef float3 alwan_din99;
typedef float3 alwan_hunter_lab;
typedef float3 alwan_iptch;
typedef float3 alwan_prolab;
typedef float3 alwan_osa_ucs;
typedef float3 alwan_ucs;
typedef float3 alwan_prismatic;
typedef float3 alwan_hcl;
typedef float3 alwan_ihls;
typedef float3 alwan_cam_jab;
typedef float4 alwan_cmyk;

#endif

#endif /* ALWAN_TYPES_H */
