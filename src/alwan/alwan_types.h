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

/* 2-component vector (for xy chromaticity coordinates) */
ALWAN_TYPE_DEF struct {
    alwan_scalar v[2];
} alwan_vec2;

/* 3-component vector */
ALWAN_TYPE_DEF struct {
    alwan_scalar v[3];
} alwan_vec3;

/* 3x3 matrix stored in row-major order: [m00 m01 m02 m10 m11 m12 m20 m21 m22] */
ALWAN_TYPE_DEF struct {
    alwan_scalar m[9];
} alwan_mat3x3;

/* 4x4 matrix stored in row-major order */
ALWAN_TYPE_DEF struct {
    alwan_scalar m[16];
} alwan_mat4x4;

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
 *
 * HLSL/GLSL NOTE:
 * In HLSL and GLSL, ALWAN_TYPE_DEF expands to nothing so these become plain
 * structs (the tag is the type name). This gives proper named member access
 * (.J, .a, .b) instead of float3 swizzle (.x, .y, .z).
 */

/* RGB color (red, green, blue) - typically linear or encoded depending on context */
ALWAN_TYPE_DEF struct {
    alwan_scalar r, g, b;
} alwan_rgb;

/* CMYK color (cyan, magenta, yellow, black) - note: uses 4 components, not compatible with vec3 */
ALWAN_TYPE_DEF struct {
    alwan_scalar c, m, y, k;
} alwan_cmyk;

/* CMY color (cyan, magenta, yellow) */
ALWAN_TYPE_DEF struct {
    alwan_scalar c, m, y;
} alwan_cmy;

/* HSV color (hue [0-360], saturation [0-1], value [0-1]) */
ALWAN_TYPE_DEF struct {
    alwan_scalar h, s, v;
} alwan_hsv;

/* HSL color (hue [0-360], saturation [0-1], lightness [0-1]) */
ALWAN_TYPE_DEF struct {
    alwan_scalar h, s, l;
} alwan_hsl;

/*
 * HSP color (hue [0-1], saturation [0-1], perceived brightness [0-1])
 * Reference: Darel Rex Finley (2006), "HSP Color Model - Alternative to
 * HSV (HSB) and HSL", http://alienryderflex.com/hsp.html
 * P = sqrt(Pr*R^2 + Pg*G^2 + Pb*B^2) with BT.601 weights (0.299, 0.587, 0.114).
 * H and S are identical to HSV. Used by DaVinci Resolve.
 */
ALWAN_TYPE_DEF struct {
    alwan_scalar h, s, p;
} alwan_hsp;

/*
 * HSPLog color (hue [0-1], saturation [0-1], perceived brightness [0-1])
 * HSP with logarithmic saturation stretching: S_log = log10(1 + 9*S).
 * Expands low saturation values — designed for log/flat-encoded footage.
 * Inspired by Nobe Color Remap (Time in Pixels) / DaVinci Resolve "HSP Log".
 *
 * NOTE: No published specification exists for this model. The formula
 * used here (log10 saturation remap) is a reasonable interpretation of
 * the "logarithmic saturation stretching of HSP" description. The actual
 * DaVinci Resolve implementation is proprietary (Blackmagic Design).
 */
ALWAN_TYPE_DEF struct {
    alwan_scalar h, s, p;
} alwan_hsplog;

/*
 * HSY color (hue [0-1], saturation [0-1], luma [0-1])
 * Reference: Kuzma Shapran, "HCY color space" (chilliant.com);
 * Krita KoColorConversions.cpp (KDE, LGPL-2.1+).
 * Y = weighted linear luma (BT.601: 0.299*R + 0.587*G + 0.114*B).
 * H identical to HSV. S uses luma-aware max_sat remapping per hue sector.
 * Used by DaVinci Resolve.
 */
ALWAN_TYPE_DEF struct {
    alwan_scalar h, s, y;
} alwan_hsy;

/* XYZ tristimulus values (CIE 1931) */
ALWAN_TYPE_DEF struct {
    alwan_scalar x, y, z;
} alwan_xyz;

/* xyY chromaticity + luminance (CIE 1931) */
ALWAN_TYPE_DEF struct {
    alwan_scalar x, y, Y;
} alwan_xyy;

/* CIE Lab color (L* [0-100], a*, b*) */
ALWAN_TYPE_DEF struct {
    alwan_scalar L, a, b;
} alwan_lab;

/* CIE Luv color (L* [0-100], u*, v*) */
ALWAN_TYPE_DEF struct {
    alwan_scalar L, u, v;
} alwan_luv;

/* CIE LCh (cylindrical Lab): Lightness, Chroma, Hue */
ALWAN_TYPE_DEF struct {
    alwan_scalar L, C, h;
} alwan_lch;

/* CIE LCh(uv) (cylindrical Luv): Lightness, Chroma, Hue */
ALWAN_TYPE_DEF struct {
    alwan_scalar L, C, h;
} alwan_lchuv;

/* Oklab color space (modern perceptual) */
ALWAN_TYPE_DEF struct {
    alwan_scalar L, a, b;
} alwan_oklab;

/* Oklch color space (cylindrical Oklab) */
ALWAN_TYPE_DEF struct {
    alwan_scalar L, C, h;
} alwan_oklch;

/* Jzazbz color space (HDR perceptual) */
ALWAN_TYPE_DEF struct {
    alwan_scalar Jz, az, bz;
} alwan_jzazbz;

/* JzCzhz (cylindrical Jzazbz) */
ALWAN_TYPE_DEF struct {
    alwan_scalar Jz, Cz, hz;
} alwan_jzczhz;

/* ICtCp color space (ITU-R BT.2100 HDR) */
ALWAN_TYPE_DEF struct {
    alwan_scalar I, Ct, Cp;
} alwan_ictcp;

/* IPT color space */
ALWAN_TYPE_DEF struct {
    alwan_scalar I, P, T;
} alwan_ipt;

/* IgPgTg color space (Ebner & Fairchild 1998) */
ALWAN_TYPE_DEF struct {
    alwan_scalar Ig, Pg, Tg;
} alwan_igpgtg;

/* ICaCb color space (Zhang & Wandell 1996, 1997) */
ALWAN_TYPE_DEF struct {
    alwan_scalar I, Ca, Cb;
} alwan_icacb;

/* YCbCr color (luma + chroma) */
ALWAN_TYPE_DEF struct {
    alwan_scalar Y, Cb, Cr;
} alwan_ycbcr;

/* YCoCg color space (luma + orange-cyan + green-magenta) */
ALWAN_TYPE_DEF struct {
    alwan_scalar Y, Co, Cg;
} alwan_ycocg;

/* Y'Cb'Cr'c' color space (luma + chroma with reduced range) */
ALWAN_TYPE_DEF struct {
    alwan_scalar Yc, Cbc, Crc;
} alwan_yccbccrc;

/* UVW color space (CIE 1964) */
ALWAN_TYPE_DEF struct {
    alwan_scalar U, V, W;
} alwan_uvw;

/* DIN99 color space */
ALWAN_TYPE_DEF struct {
    alwan_scalar L99, a99, b99;
} alwan_din99;

/* Hunter Lab color space */
ALWAN_TYPE_DEF struct {
    alwan_scalar L, a, b;
} alwan_hunter_lab;

/* IPT cylindrical form (I, Chroma, Hue) */
ALWAN_TYPE_DEF struct {
    alwan_scalar I, C, h;
} alwan_iptch;

/* ProLab color space */
ALWAN_TYPE_DEF struct {
    alwan_scalar L, a, b;
} alwan_prolab;

/* OSA-UCS color space (Ljg) */
ALWAN_TYPE_DEF struct {
    alwan_scalar L, j, g;
} alwan_osa_ucs;

/* CIE 1960 UCS color space */
ALWAN_TYPE_DEF struct {
    alwan_scalar U, V, W;
} alwan_ucs;

/* Prismatic color space */
ALWAN_TYPE_DEF struct {
    alwan_scalar L, s, h;
} alwan_prismatic;

/* HCL color space (Hue, Chroma, Luminance - in RGB context) */
ALWAN_TYPE_DEF struct {
    alwan_scalar H, C, L;
} alwan_hcl;

/* IHLS color space (Improved HLS) */
ALWAN_TYPE_DEF struct {
    alwan_scalar H, L, S;
} alwan_ihls;

/* CAM Jab color (J, a, b - for CAM02/CAM16) */
ALWAN_TYPE_DEF struct {
    alwan_scalar J, a, b;
} alwan_cam_jab;

/* HSLuv color (hue [0-360], saturation [0-100], lightness [0-100])
 * Human-friendly HSL mapped through CIE LCHuv (Boronine) */
ALWAN_TYPE_DEF struct {
    alwan_scalar h, s, l;
} alwan_hsluv;

/* HPLuv color (hue [0-360], saturation [0-100], lightness [0-100])
 * Pastel variant of HSLuv — all (h,s,l) triples are in sRGB gamut */
ALWAN_TYPE_DEF struct {
    alwan_scalar h, s, l;
} alwan_hpluv;

/* Okhsl color (hue [0-1], saturation [0-1], lightness [0-1])
 * Perceptually uniform HSL in Oklab space (Ottosson 2021) */
ALWAN_TYPE_DEF struct {
    alwan_scalar h, s, l;
} alwan_okhsl;

/* Okhsv color (hue [0-1], saturation [0-1], value [0-1])
 * Perceptually uniform HSV in Oklab space (Ottosson 2021) */
ALWAN_TYPE_DEF struct {
    alwan_scalar h, s, v;
} alwan_okhsv;

/* Cubehelix color (hue [degrees], saturation [0-inf], lightness [0-1])
 * Monotonic-luminance helical scheme (Green 2011) */
ALWAN_TYPE_DEF struct {
    alwan_scalar h, s, l;
} alwan_cubehelix;

/* HLC color (hue [0-360], lightness [0-100], chroma)
 * Cylindrical CIELAB with (H, L, C) ordering */
ALWAN_TYPE_DEF struct {
    alwan_scalar H, L, C;
} alwan_hlc;

/* ST.2086 Mastering Display Color Volume Metadata
 * Reference: SMPTE ST 2086:2018
 * Contains mastering display primaries, white point, and luminance range */
ALWAN_TYPE_DEF struct {
    alwan_scalar display_primaries_xy[6]; /* rx, ry, gx, gy, bx, by */
    alwan_scalar white_point_xy[2];       /* wx, wy */
    alwan_scalar max_luminance;           /* cd/m2 (nits) */
    alwan_scalar min_luminance;           /* cd/m2 (nits) */
} alwan_st2086_metadata;

/* Content Light Level Info (CTA-861-H / ST.2086 companion)
 * MaxCLL:  maximum content light level (peak pixel luminance, cd/m2)
 * MaxFALL: maximum frame-average light level (cd/m2) */
ALWAN_TYPE_DEF struct {
    alwan_scalar max_cll;
    alwan_scalar max_fall;
} alwan_content_light_level;

#endif /* ALWAN_TYPES_H */
