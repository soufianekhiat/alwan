/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Forward declarations for all _f32/_f64 map_planar and map_interleave variants.
 * Included by alwan_map_internal.h to suppress C4013 in typed_map.c /
 * typed_planar_map.c before Phase 5 adds them to alwan.h.
 */

#ifndef ALWAN_MAP_FWD_H
#define ALWAN_MAP_FWD_H

/* Helper macros ---------------------------------------------------------- */

/* Basic 3-channel planar pair (v2: out_stride after o0, in_stride after i0) */
#define ALWAN_PFWD(n) \
    int n##_f32_map_planar(float *o0,size_t out_stride,float *o1,float *o2,float const *i0,size_t in_stride,float const *i1,float const *i2,size_t count); \
    int n##_f64_map_planar(double *o0,size_t out_stride,double *o1,double *o2,double const *i0,size_t in_stride,double const *i1,double const *i2,size_t count)

/* Basic 3-channel interleave pair (v2: stride after each buffer) */
#define ALWAN_IFWD(n) \
    int n##_f32_map_interleave(float *out,size_t out_stride,float const *in,size_t in_stride,size_t count); \
    int n##_f64_map_interleave(double *out,size_t out_stride,double const *in,size_t in_stride,size_t count)

/* Both planar + interleave, basic */
#define ALWAN_PFWD_IFWD(n) ALWAN_PFWD(n); ALWAN_IFWD(n)

/* Planar with white_xyz (v2) */
#define ALWAN_PFWD_W(n) \
    int n##_f32_map_planar(float *o0,size_t out_stride,float *o1,float *o2,float const *i0,size_t in_stride,float const *i1,float const *i2,size_t count,alwan_xyz_f32 const *w); \
    int n##_f64_map_planar(double *o0,size_t out_stride,double *o1,double *o2,double const *i0,size_t in_stride,double const *i1,double const *i2,size_t count,alwan_xyz_f64 const *w)

/* Interleave with white_xyz (v2) */
#define ALWAN_IFWD_W(n) \
    int n##_f32_map_interleave(float *out,size_t out_stride,float const *in,size_t in_stride,size_t count,alwan_xyz_f32 const *w); \
    int n##_f64_map_interleave(double *out,size_t out_stride,double const *in,size_t in_stride,size_t count,alwan_xyz_f64 const *w)

/* Both with white_xyz */
#define ALWAN_PFWD_IFWD_W(n) ALWAN_PFWD_W(n); ALWAN_IFWD_W(n)

/* Planar with int (v2) */
#define ALWAN_PFWD_I(n) \
    int n##_f32_map_planar(float *o0,size_t out_stride,float *o1,float *o2,float const *i0,size_t in_stride,float const *i1,float const *i2,size_t count,int v); \
    int n##_f64_map_planar(double *o0,size_t out_stride,double *o1,double *o2,double const *i0,size_t in_stride,double const *i1,double const *i2,size_t count,int v)

/* Interleave with int (v2) */
#define ALWAN_IFWD_I(n) \
    int n##_f32_map_interleave(float *out,size_t out_stride,float const *in,size_t in_stride,size_t count,int v); \
    int n##_f64_map_interleave(double *out,size_t out_stride,double const *in,size_t in_stride,size_t count,int v)

/* Both with int */
#define ALWAN_PFWD_IFWD_I(n) ALWAN_PFWD_I(n); ALWAN_IFWD_I(n)

/* Both with alwan_ycbcr_standard (enum) -- avoids C4028 vs int mismatch (v2) */
#define ALWAN_PFWD_IFWD_YCBCR(n) \
    int n##_f32_map_planar(float *o0,size_t out_stride,float *o1,float *o2,float const *i0,size_t in_stride,float const *i1,float const *i2,size_t count,alwan_ycbcr_standard v); \
    int n##_f64_map_planar(double *o0,size_t out_stride,double *o1,double *o2,double const *i0,size_t in_stride,double const *i1,double const *i2,size_t count,alwan_ycbcr_standard v); \
    int n##_f32_map_interleave(float *out,size_t out_stride,float const *in,size_t in_stride,size_t count,alwan_ycbcr_standard v); \
    int n##_f64_map_interleave(double *out,size_t out_stride,double const *in,size_t in_stride,size_t count,alwan_ycbcr_standard v)

/* Planar with scalar (v2) */
#define ALWAN_PFWD_S(n) \
    int n##_f32_map_planar(float *o0,size_t out_stride,float *o1,float *o2,float const *i0,size_t in_stride,float const *i1,float const *i2,size_t count,float p); \
    int n##_f64_map_planar(double *o0,size_t out_stride,double *o1,double *o2,double const *i0,size_t in_stride,double const *i1,double const *i2,size_t count,double p)

/* Interleave with scalar (v2) */
#define ALWAN_IFWD_S(n) \
    int n##_f32_map_interleave(float *out,size_t out_stride,float const *in,size_t in_stride,size_t count,float p); \
    int n##_f64_map_interleave(double *out,size_t out_stride,double const *in,size_t in_stride,size_t count,double p)

/* Both with scalar */
#define ALWAN_PFWD_IFWD_S(n) ALWAN_PFWD_S(n); ALWAN_IFWD_S(n)

/* ======================================================================== */
/* sRGB convenience (planar only -- interleave is in alwan_rgb_map.c via    */
/* SIMD and already declared in alwan.h under old names)                    */
/* ======================================================================== */

ALWAN_PFWD(alwan_srgb_to_xyz);
ALWAN_PFWD(alwan_xyz_to_srgb);
ALWAN_PFWD(alwan_srgb_to_lab);
ALWAN_PFWD(alwan_lab_to_srgb);
ALWAN_PFWD(alwan_srgb_to_oklab);
ALWAN_PFWD(alwan_oklab_to_srgb);

/* ======================================================================== */
/* Core XYZ conversions (planar only for Lab/Luv, both for Oklab)           */
/* ======================================================================== */

/* xyz_to_lab / xyz_to_luv take a white_xyz pointer */
ALWAN_PFWD_W(alwan_xyz_to_lab);
ALWAN_PFWD_W(alwan_lab_to_xyz);
ALWAN_PFWD_W(alwan_xyz_to_luv);
ALWAN_PFWD_W(alwan_luv_to_xyz);
ALWAN_PFWD(alwan_xyz_to_oklab);
ALWAN_PFWD(alwan_oklab_to_xyz);

/* ======================================================================== */
/* Derived Lab/Luv conversions                                              */
/* ======================================================================== */

ALWAN_PFWD_IFWD(alwan_lab_to_lch);
ALWAN_PFWD_IFWD(alwan_lch_to_lab);
ALWAN_PFWD_IFWD(alwan_luv_to_lchuv);
ALWAN_PFWD_IFWD(alwan_lchuv_to_luv);
ALWAN_PFWD_IFWD(alwan_xyz_to_xyy);
ALWAN_PFWD_IFWD(alwan_xyy_to_xyz);
ALWAN_PFWD_IFWD(alwan_oklab_to_oklch);
ALWAN_PFWD_IFWD(alwan_oklch_to_oklab);

/* ======================================================================== */
/* Perceptual / HDR colour spaces                                           */
/* ======================================================================== */

ALWAN_PFWD_IFWD_I(alwan_rgb_to_ictcp);
ALWAN_PFWD_IFWD_I(alwan_ictcp_to_rgb);
ALWAN_PFWD_IFWD_I(alwan_xyz_to_ictcp);
ALWAN_PFWD_IFWD_I(alwan_ictcp_to_xyz);
ALWAN_PFWD_IFWD(alwan_xyz_to_jzazbz);
ALWAN_PFWD_IFWD(alwan_jzazbz_to_xyz);
ALWAN_PFWD_IFWD(alwan_jzazbz_to_jzczhz);
ALWAN_PFWD_IFWD(alwan_jzczhz_to_jzazbz);
ALWAN_PFWD_IFWD(alwan_xyz_to_ipt);
ALWAN_PFWD_IFWD(alwan_ipt_to_xyz);
ALWAN_PFWD_IFWD(alwan_xyz_to_igpgtg);
ALWAN_PFWD_IFWD(alwan_igpgtg_to_xyz);
ALWAN_PFWD_IFWD(alwan_xyz_to_icacb);
ALWAN_PFWD_IFWD(alwan_icacb_to_xyz);
ALWAN_PFWD_IFWD(alwan_xyz_to_hdr_cielab);
ALWAN_PFWD_IFWD(alwan_hdr_cielab_to_xyz);
ALWAN_PFWD_IFWD(alwan_xyz_to_hdr_ipt);
ALWAN_PFWD_IFWD(alwan_hdr_ipt_to_xyz);
ALWAN_PFWD_IFWD(alwan_xyz_to_ucs);
ALWAN_PFWD_IFWD(alwan_ucs_to_xyz);
ALWAN_PFWD_IFWD(alwan_xyz_to_osa_ucs);
ALWAN_PFWD_IFWD(alwan_osa_ucs_to_xyz);

/* ======================================================================== */
/* Extended colour models (Hunter Lab, ProLab)                              */
/* ======================================================================== */

ALWAN_PFWD_IFWD(alwan_xyz_to_hunter_lab);
ALWAN_PFWD_IFWD(alwan_hunter_lab_to_xyz);
ALWAN_PFWD_IFWD(alwan_xyz_to_prolab);
ALWAN_PFWD_IFWD(alwan_prolab_to_xyz);

/* With custom white point */
ALWAN_PFWD_IFWD_W(alwan_xyz_to_uvw);
ALWAN_PFWD_IFWD_W(alwan_uvw_to_xyz);
ALWAN_PFWD_IFWD_W(alwan_xyz_to_hunter_lab_custom);
ALWAN_PFWD_IFWD_W(alwan_hunter_lab_to_xyz_custom);
ALWAN_PFWD_IFWD_W(alwan_xyz_to_prolab_custom);
ALWAN_PFWD_IFWD_W(alwan_prolab_to_xyz_custom);

/* ======================================================================== */
/* HSx and perceptual cylindrical spaces                                    */
/* ======================================================================== */

ALWAN_PFWD_IFWD(alwan_rgb_to_hsv);
ALWAN_PFWD_IFWD(alwan_hsv_to_rgb);
ALWAN_PFWD_IFWD(alwan_rgb_to_hsl);
ALWAN_PFWD_IFWD(alwan_hsl_to_rgb);
ALWAN_PFWD_IFWD(alwan_rgb_to_hsp);
ALWAN_PFWD_IFWD(alwan_hsp_to_rgb);
ALWAN_PFWD_IFWD(alwan_rgb_to_hsplog);
ALWAN_PFWD_IFWD(alwan_hsplog_to_rgb);
ALWAN_PFWD_IFWD(alwan_rgb_to_hsy);
ALWAN_PFWD_IFWD(alwan_hsy_to_rgb);
ALWAN_PFWD_IFWD(alwan_linear_srgb_to_hsv);
ALWAN_PFWD_IFWD(alwan_hsv_to_linear_srgb);
ALWAN_PFWD_IFWD(alwan_linear_srgb_to_hsl);
ALWAN_PFWD_IFWD(alwan_hsl_to_linear_srgb);
ALWAN_PFWD_IFWD(alwan_rgb_to_hcl);
ALWAN_PFWD_IFWD(alwan_hcl_to_rgb);
ALWAN_PFWD_IFWD(alwan_rgb_to_ihls);
ALWAN_PFWD_IFWD(alwan_ihls_to_rgb);
ALWAN_PFWD_IFWD(alwan_rgb_to_prismatic);
ALWAN_PFWD_IFWD(alwan_prismatic_to_rgb);

/* ======================================================================== */
/* Device / printing spaces                                                 */
/* ======================================================================== */

ALWAN_PFWD_IFWD(alwan_rgb_to_cmy);
ALWAN_PFWD_IFWD(alwan_cmy_to_rgb);
ALWAN_PFWD_IFWD(alwan_rgb_to_hwb);
ALWAN_PFWD_IFWD(alwan_hwb_to_rgb);
ALWAN_PFWD_IFWD(alwan_hsv_to_hwb);
ALWAN_PFWD_IFWD(alwan_hwb_to_hsv);
ALWAN_PFWD_IFWD(alwan_rgb_to_ycocg);
ALWAN_PFWD_IFWD(alwan_ycocg_to_rgb);
/* ycbcr uses alwan_ycbcr_standard (enum) */
ALWAN_PFWD_IFWD_YCBCR(alwan_rgb_to_ycbcr);
ALWAN_PFWD_IFWD_YCBCR(alwan_ycbcr_to_rgb);
/* yccbccrc and ycbcr range conversion use int bit_depth */
ALWAN_PFWD_IFWD_I(alwan_rgb_to_yccbccrc);
ALWAN_PFWD_IFWD_I(alwan_yccbccrc_to_rgb);
ALWAN_PFWD_IFWD_I(alwan_ycbcr_full_to_legal);
ALWAN_PFWD_IFWD_I(alwan_ycbcr_legal_to_full);

/* 4-channel CMY<->CMYK -- different channel counts, explicit declarations */
int alwan_cmy_to_cmyk_f32_map_planar(float *out_c, size_t out_stride, float *out_m, float *out_y, float *out_k, float const *in_c, size_t in_stride, float const *in_m, float const *in_y, size_t count);
int alwan_cmy_to_cmyk_f64_map_planar(double *out_c, size_t out_stride, double *out_m, double *out_y, double *out_k, double const *in_c, size_t in_stride, double const *in_m, double const *in_y, size_t count);
int alwan_cmy_to_cmyk_f32_map_interleave(float *out, size_t out_stride, float const *in, size_t in_stride, size_t count);
int alwan_cmy_to_cmyk_f64_map_interleave(double *out, size_t out_stride, double const *in, size_t in_stride, size_t count);
int alwan_cmyk_to_cmy_f32_map_planar(float *out_c, size_t out_stride, float *out_m, float *out_y, float const *in_c, size_t in_stride, float const *in_m, float const *in_y, float const *in_k, size_t count);
int alwan_cmyk_to_cmy_f64_map_planar(double *out_c, size_t out_stride, double *out_m, double *out_y, double const *in_c, size_t in_stride, double const *in_m, double const *in_y, double const *in_k, size_t count);
int alwan_cmyk_to_cmy_f32_map_interleave(float *out, size_t out_stride, float const *in, size_t in_stride, size_t count);
int alwan_cmyk_to_cmy_f64_map_interleave(double *out, size_t out_stride, double const *in, size_t in_stride, size_t count);

/* ======================================================================== */
/* DIN99 (with int variant)                                                 */
/* ======================================================================== */

ALWAN_PFWD_IFWD_I(alwan_lab_to_din99);
ALWAN_PFWD_IFWD_I(alwan_din99_to_lab);

/* ======================================================================== */
/* CVD -- simple severity (scalar)                                          */
/* ======================================================================== */

ALWAN_PFWD_IFWD_S(alwan_simulate_protanopia);
ALWAN_PFWD_IFWD_S(alwan_simulate_deuteranopia);
ALWAN_PFWD_IFWD_S(alwan_simulate_tritanopia);

/* CVD with cvd_type enum + scalar severity (manual -- no helper macro) */
int alwan_simulate_cvd_f32_map_planar(float *o0, size_t out_stride, float *o1, float *o2, float const *i0, size_t in_stride, float const *i1, float const *i2, size_t count, alwan_cvd_type cvd_type, float severity);
int alwan_simulate_cvd_f64_map_planar(double *o0, size_t out_stride, double *o1, double *o2, double const *i0, size_t in_stride, double const *i1, double const *i2, size_t count, alwan_cvd_type cvd_type, double severity);
int alwan_simulate_cvd_f32_map_interleave(float *out, size_t out_stride, float const *in, size_t in_stride, size_t count, alwan_cvd_type t, float s);
int alwan_simulate_cvd_f64_map_interleave(double *out, size_t out_stride, double const *in, size_t in_stride, size_t count, alwan_cvd_type t, double s);
int alwan_simulate_cvd_machado_f32_map_planar(float *o0, size_t out_stride, float *o1, float *o2, float const *i0, size_t in_stride, float const *i1, float const *i2, size_t count, alwan_cvd_type cvd_type, float severity);
int alwan_simulate_cvd_machado_f64_map_planar(double *o0, size_t out_stride, double *o1, double *o2, double const *i0, size_t in_stride, double const *i1, double const *i2, size_t count, alwan_cvd_type cvd_type, double severity);
int alwan_simulate_cvd_machado_f32_map_interleave(float *out, size_t out_stride, float const *in, size_t in_stride, size_t count, alwan_cvd_type t, float s);
int alwan_simulate_cvd_machado_f64_map_interleave(double *out, size_t out_stride, double const *in, size_t in_stride, size_t count, alwan_cvd_type t, double s);

/* ======================================================================== */
/* Color correction (complex signatures)                                    */
/* ======================================================================== */

int alwan_lgg_apply_f32_map_planar(float *o0, size_t out_stride, float *o1, float *o2, float const *i0, size_t in_stride, float const *i1, float const *i2, size_t count, alwan_rgb_f32 const *lift, alwan_rgb_f32 const *gamma, alwan_rgb_f32 const *gain);
int alwan_lgg_apply_f64_map_planar(double *o0, size_t out_stride, double *o1, double *o2, double const *i0, size_t in_stride, double const *i1, double const *i2, size_t count, alwan_rgb_f64 const *lift, alwan_rgb_f64 const *gamma, alwan_rgb_f64 const *gain);
int alwan_lgg_apply_f32_map_interleave(float *out, size_t out_stride, float const *in, size_t in_stride, size_t count, alwan_rgb_f32 const *lift, alwan_rgb_f32 const *gamma, alwan_rgb_f32 const *gain);
int alwan_lgg_apply_f64_map_interleave(double *out, size_t out_stride, double const *in, size_t in_stride, size_t count, alwan_rgb_f64 const *lift, alwan_rgb_f64 const *gamma, alwan_rgb_f64 const *gain);

int alwan_color_matrix_apply_f32_map_planar(float *o0, size_t out_stride, float *o1, float *o2, float const *i0, size_t in_stride, float const *i1, float const *i2, size_t count, alwan_mat3x3_f32 const *matrix);
int alwan_color_matrix_apply_f64_map_planar(double *o0, size_t out_stride, double *o1, double *o2, double const *i0, size_t in_stride, double const *i1, double const *i2, size_t count, alwan_mat3x3_f64 const *matrix);
int alwan_color_matrix_apply_f32_map_interleave(float *out, size_t out_stride, float const *in, size_t in_stride, size_t count, alwan_mat3x3_f32 const *matrix);
int alwan_color_matrix_apply_f64_map_interleave(double *out, size_t out_stride, double const *in, size_t in_stride, size_t count, alwan_mat3x3_f64 const *matrix);

int alwan_printer_lights_apply_f32_map_planar(float *o0, size_t out_stride, float *o1, float *o2, float const *i0, size_t in_stride, float const *i1, float const *i2, size_t count, float r, float g, float b);
int alwan_printer_lights_apply_f64_map_planar(double *o0, size_t out_stride, double *o1, double *o2, double const *i0, size_t in_stride, double const *i1, double const *i2, size_t count, double r, double g, double b);
int alwan_printer_lights_apply_f32_map_interleave(float *out, size_t out_stride, float const *in, size_t in_stride, size_t count, float r, float g, float b);
int alwan_printer_lights_apply_f64_map_interleave(double *out, size_t out_stride, double const *in, size_t in_stride, size_t count, double r, double g, double b);

int alwan_white_balance_apply_f32_map_planar(float *o0, size_t out_stride, float *o1, float *o2, float const *i0, size_t in_stride, float const *i1, float const *i2, size_t count, alwan_rgb_f32 const *multipliers);
int alwan_white_balance_apply_f64_map_planar(double *o0, size_t out_stride, double *o1, double *o2, double const *i0, size_t in_stride, double const *i1, double const *i2, size_t count, alwan_rgb_f64 const *multipliers);
int alwan_white_balance_apply_f32_map_interleave(float *out, size_t out_stride, float const *in, size_t in_stride, size_t count, alwan_rgb_f32 const *multipliers);
int alwan_white_balance_apply_f64_map_interleave(double *out, size_t out_stride, double const *in, size_t in_stride, size_t count, alwan_rgb_f64 const *multipliers);

/* ======================================================================== */
/* Gamut operations (no extra param)                                        */
/* ======================================================================== */

ALWAN_PFWD_IFWD(alwan_gamut_clip);
ALWAN_PFWD_IFWD(alwan_css_gamut_map);

/* Undef helper macros */
#undef ALWAN_PFWD
#undef ALWAN_IFWD
#undef ALWAN_PFWD_IFWD
#undef ALWAN_PFWD_W
#undef ALWAN_IFWD_W
#undef ALWAN_PFWD_IFWD_W
#undef ALWAN_PFWD_I
#undef ALWAN_IFWD_I
#undef ALWAN_PFWD_IFWD_I
#undef ALWAN_PFWD_S
#undef ALWAN_IFWD_S
#undef ALWAN_PFWD_IFWD_S

#endif /* ALWAN_MAP_FWD_H */
