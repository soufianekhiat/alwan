/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Parameterized type definitions - included twice (f32 and f64)
 * No include guard.
 *
 * Expects before inclusion:
 *   ALWAN_T       -- float or double
 *   ALWAN_SUFFIX  -- _f32 or _f64
 */

/* Token-pasting helpers (safe to redefine identically on second pass) */
#define ALWAN_TN2_(base, sfx) base##sfx
#define ALWAN_TN_(base, sfx)  ALWAN_TN2_(base, sfx)
#define ALWAN_TN(base)        ALWAN_TN_(base, ALWAN_SUFFIX)

/* ----------------------------------------------------------------
 * Math Types
 * ---------------------------------------------------------------- */

typedef struct { ALWAN_T v[2]; }  ALWAN_TN(alwan_vec2);
typedef struct { ALWAN_T v[3]; }  ALWAN_TN(alwan_vec3);
typedef struct { ALWAN_T m[9]; }  ALWAN_TN(alwan_mat3x3);
typedef struct { ALWAN_T m[16]; } ALWAN_TN(alwan_mat4x4);

/* ----------------------------------------------------------------
 * Semantic Color Types (layout-compatible with suffixed alwan_vec3)
 * ---------------------------------------------------------------- */

typedef struct { ALWAN_T r, g, b; }     ALWAN_TN(alwan_rgb);
typedef struct { ALWAN_T c, m, y, k; }  ALWAN_TN(alwan_cmyk);
typedef struct { ALWAN_T c, m, y; }     ALWAN_TN(alwan_cmy);
typedef struct { ALWAN_T h, s, v; }     ALWAN_TN(alwan_hsv);
typedef struct { ALWAN_T h, s, l; }     ALWAN_TN(alwan_hsl);
typedef struct { ALWAN_T h, s, p; }     ALWAN_TN(alwan_hsp);
typedef struct { ALWAN_T h, s, p; }     ALWAN_TN(alwan_hsplog);
typedef struct { ALWAN_T h, s, y; }     ALWAN_TN(alwan_hsy);
typedef struct { ALWAN_T h, w, b; }     ALWAN_TN(alwan_hwb);
typedef struct { ALWAN_T x, y, z; }     ALWAN_TN(alwan_xyz);
typedef struct { ALWAN_T x, y, Y; }     ALWAN_TN(alwan_xyy);
typedef struct { ALWAN_T L, a, b; }     ALWAN_TN(alwan_lab);
typedef struct { ALWAN_T L, u, v; }     ALWAN_TN(alwan_luv);
typedef struct { ALWAN_T L, C, h; }     ALWAN_TN(alwan_lch);
typedef struct { ALWAN_T L, C, h; }     ALWAN_TN(alwan_lchuv);
typedef struct { ALWAN_T L, a, b; }     ALWAN_TN(alwan_oklab);
typedef struct { ALWAN_T L, C, h; }     ALWAN_TN(alwan_oklch);
typedef struct { ALWAN_T Jz, az, bz; }  ALWAN_TN(alwan_jzazbz);
typedef struct { ALWAN_T Jz, Cz, hz; }  ALWAN_TN(alwan_jzczhz);
typedef struct { ALWAN_T I, Ct, Cp; }   ALWAN_TN(alwan_ictcp);
typedef struct { ALWAN_T I, P, T; }     ALWAN_TN(alwan_ipt);
typedef struct { ALWAN_T Ig, Pg, Tg; }  ALWAN_TN(alwan_igpgtg);
typedef struct { ALWAN_T I, Ca, Cb; }   ALWAN_TN(alwan_icacb);
typedef struct { ALWAN_T Y, Cb, Cr; }   ALWAN_TN(alwan_ycbcr);
typedef struct { ALWAN_T Y, Co, Cg; }   ALWAN_TN(alwan_ycocg);
typedef struct { ALWAN_T Yc, Cbc, Crc; } ALWAN_TN(alwan_yccbccrc);
typedef struct { ALWAN_T U, V, W; }     ALWAN_TN(alwan_uvw);
typedef struct { ALWAN_T L99, a99, b99; } ALWAN_TN(alwan_din99);
typedef struct { ALWAN_T L, a, b; }     ALWAN_TN(alwan_hunter_lab);
typedef struct { ALWAN_T I, C, h; }     ALWAN_TN(alwan_iptch);
typedef struct { ALWAN_T L, a, b; }     ALWAN_TN(alwan_prolab);
typedef struct { ALWAN_T L, j, g; }     ALWAN_TN(alwan_osa_ucs);
typedef struct { ALWAN_T U, V, W; }     ALWAN_TN(alwan_ucs);
typedef struct { ALWAN_T L, s, h; }     ALWAN_TN(alwan_prismatic);
typedef struct { ALWAN_T H, C, L; }     ALWAN_TN(alwan_hcl);
typedef struct { ALWAN_T H, L, S; }     ALWAN_TN(alwan_ihls);
typedef struct { ALWAN_T J, a, b; }     ALWAN_TN(alwan_cam_jab);
typedef struct { ALWAN_T h, s, l; }     ALWAN_TN(alwan_hsluv);
typedef struct { ALWAN_T h, s, l; }     ALWAN_TN(alwan_hpluv);
typedef struct { ALWAN_T h, s, l; }     ALWAN_TN(alwan_okhsl);
typedef struct { ALWAN_T h, s, v; }     ALWAN_TN(alwan_okhsv);
typedef struct { ALWAN_T h, s, l; }     ALWAN_TN(alwan_cubehelix);
typedef struct { ALWAN_T H, L, C; }     ALWAN_TN(alwan_hlc);

/* ST.2086 Mastering Display Color Volume Metadata */
typedef struct {
    ALWAN_T display_primaries_xy[6]; /* rx, ry, gx, gy, bx, by */
    ALWAN_T white_point_xy[2];       /* wx, wy */
    ALWAN_T max_luminance;           /* cd/m2 (nits) */
    ALWAN_T min_luminance;           /* cd/m2 (nits) */
} ALWAN_TN(alwan_st2086_metadata);

/* Content Light Level Info */
typedef struct {
    ALWAN_T max_cll;
    ALWAN_T max_fall;
} ALWAN_TN(alwan_content_light_level);

#undef ALWAN_TN
#undef ALWAN_TN_
#undef ALWAN_TN2_
