/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Internal helpers and utilities
 */

#ifndef ALWAN_INTERNAL_H
#define ALWAN_INTERNAL_H

#include "alwan.h"  /* For alwan_alloc_fn, alwan_free_fn, alwan_f64 */
#include "alwan_math.h"  /* ALWAN_POW / ALWAN_FMA / ... routing */

/* ----------------------------------------------------------------
 * Internal context structure (shared across modules)
 * ---------------------------------------------------------------- */

struct alwan_ctx {
    /* Allocation callbacks */
    alwan_alloc_fn alloc_fn;
    alwan_free_fn  free_fn;

    /* Configuration */
    char *runtime_data_root;  /* Owned copy (if non-NULL) */
    uint32_t flags;
    alwan_aces_interp aces_interp;  /* the ACES 1.x tone curve method the view transform runs; alwan_ctx_set_aces_interp */

    /* Future: data cache, registry, etc. */
};

/* api/alwan_planckian_table.c: a Planckian table whose CMF sums stop at wl_max nm (0: all).
 * colour-science builds its tables from CMFs reshaped to 360-780 nm; CQS and CIE 2017 follow it. */
alwan_status alwan__planckian_table_create_range(alwan_planckian_table **out, alwan_observer_type observer,
                                                 double start, double end, double spacing, double wl_max,
                                                 alwan_ctx *ctx);
/* api/alwan_quality_colour.c: CQS and CIE 2017 / TM-30 as colour-science computes them. */
alwan_status alwan__cqs_compute(alwan_cqs_f64 *spec, alwan_spd_f64 const *test_spd, alwan_cqs_version version, alwan_ctx *ctx);
alwan_status alwan__cie2017_compute(alwan_tm30_f64 *spec, alwan_spd_f64 const *test_spd, alwan_ctx *ctx);

/* Edge-aware filter workers (api/alwan_guided_filter.c, alwan_bilateral.c,
 * alwan_domain_transform.c, alwan_fast_global_smoother.c), behind alwan_edge_filter_{T}.
 * Pixels are f32 when is_f32 is non-zero, f64 otherwise; strides in bytes. */
alwan_status alwan__gf_run(void *out, size_t out_row_stride, void const *src, size_t src_row_stride, size_t src_channels,
                           void const *guide, size_t guide_row_stride, size_t guide_channels, size_t w, size_t h,
                           size_t radius, double eps, int is_f32);
alwan_status alwan__bf_run(void *out, size_t out_row_stride, void const *src, size_t src_row_stride, size_t src_ch,
                           void const *joint, size_t joint_row_stride, size_t joint_ch, size_t w, size_t h,
                           size_t radius, double sigma_color, double sigma_space, int is_f32);
alwan_status alwan__rgf_run(void *out, size_t out_row_stride, void const *src, size_t src_row_stride, size_t ch,
                            size_t w, size_t h, size_t radius, double sigma_color, double sigma_space,
                            size_t iterations, int from_gaussian, int is_f32);
alwan_status alwan__dt_run(void *out, size_t out_row_stride, void const *src, size_t src_row_stride, size_t sch,
                           void const *guide, size_t guide_row_stride, size_t gch, size_t w, size_t h,
                           double sigma_s, double sigma_r, int mode, size_t iterations, int is_f32);
alwan_status alwan__fgs_run(void *out, size_t out_row_stride, void const *src, size_t src_row_stride, size_t sch,
                            void const *guide, size_t guide_row_stride, size_t gch, size_t w, size_t h,
                            double lambda, double sigma_color, double attenuation, size_t iterations, int is_f32);
/* alwan_am_filter.c: ALWAN_EDGE_FILTER_ADAPTIVE_MANIFOLD */
alwan_status alwan__amf_run(void *out, size_t out_row_stride, void const *src, size_t src_row_stride, size_t sch,
                            void const *guide, size_t guide_row_stride, size_t gch, size_t w, size_t h,
                            double sigma_s, double sigma_r, int adjust_outliers, int is_f32);
/* alwan_wmf.c: ALWAN_EDGE_FILTER_WEIGHTED_MEDIAN */
alwan_status alwan__wmf_run(void *out, size_t out_row_stride, void const *src, size_t src_row_stride, size_t sch,
                            void const *guide, size_t guide_row_stride, size_t gch, size_t w, size_t h,
                            size_t radius, double sigma, alwan_wmf_weight type, int is_f32);
/* alwan_segment_ext.c: ALWAN_SEGMENT_FELZENSZWALB to _RANDOM_WALKER; v is h x w x ch doubles
 * (the data's values, 8-bit as they come), kind 0 f64, 1 f32, 2 u8 */
alwan_status alwan__segment_ext(uint32_t *labels, size_t labels_rs, size_t *count_out, double *v, size_t w, size_t h, size_t ch,
                                alwan_segment_method method, alwan_segment_params const *p, int kind);
/* alwan_l0_smooth.c: ALWAN_EDGE_FILTER_L0_SMOOTH */
alwan_status alwan__l0_run(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                           double lambda, double kappa, int is_f32);
/* alwan_warp.c: alwan_warp's engine for alwan_resize's integration; kind 0 f64, 1 f32, 2 u8 */
alwan_status alwan__warp_run(void *out, size_t out_rs, size_t ow, size_t oh, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                            alwan_warp_method method, alwan_warp_params const *params, int kind);
/* alwan_resize_opencv.c: cv::resize INTER_CUBIC (area 0) or INTER_AREA (area 1), the whole
 * image; kind 0 f64, 1 f32, 2 u8 */
alwan_status alwan__cv_resize(void *out, size_t out_rs, size_t ow, size_t oh, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                              int area, int kind);
/* alwan_denoise_nlm.c: ALWAN_DENOISE_NL_MEANS_DARBON (buades 0) and _BUADES (1); kind 0 f64,
 * 1 f32, 2 u8 (through double in 0..1, rounded back) */
alwan_status alwan__denoise_nlm(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h, int buades,
                                double hh, size_t s, size_t d, double sigma, int fast_exp, int kind);
/* alwan_fft.c: a DFT of any length in double, in place; the inverse is not scaled */
/* OpenCV 5.0.0's float sRGB to CIELAB (cvtColor COLOR_BGR2Lab on a float image: clip,
 * 14-bit round, trilinear on its 33-node int16 table), shared by alwan_decolor and
 * alwan_stylize; lab receives L, a, b for sRGB-encoded r, g, b. api/alwan_decolor.c. */
void alwan__cv_rgb_to_lab_f32(float *lab, float r, float g, float b);
/* cv::inpaint's TELEA and NS (OpenCV 5.0.0) on U8, U16, F32 or F64 pixels, the entry behind
 * alwan_inpaint and alwan_inpaint_{T} for those methods. api/alwan_inpaint_opencv.c. */
alwan_status alwan__inpaint_opencv(void *out, size_t out_rs, void const *src, size_t src_rs, alwan_pixel_format format,
                                   size_t ch, size_t w, size_t h, unsigned char const *mask, size_t mask_rs,
                                   alwan_inpaint_method method, alwan_inpaint_params const *params);

/* OpenCV 5.0.0's float DFT and DCT (core/src/dxt.cpp), api/alwan_cv_dxt.c. A complex row
 * plan for cv::dft (alwan_gradient_edit's sine transform): forward, or inverse scaled by
 * 1 / n. A square-block plan for cv::dct / cv::idct (ALWAN_DENOISE_DCT_OPENCV): n even,
 * rows then columns, row strides in floats, dst may be src. */
typedef struct {
    float re, im;
} alwan__cv_cf;
typedef struct {
    int n;
    int nf;
    int factors[34];
    int *itab;
    alwan__cv_cf *wave;
    alwan__cv_cf *scratch;
} alwan__cv_dft_plan;
int alwan__cv_dft_plan_create(alwan__cv_dft_plan *p, int n);
void alwan__cv_dft_plan_free(alwan__cv_dft_plan *p);
void alwan__cv_dft_row(alwan__cv_dft_plan const *p, alwan__cv_cf const *src, alwan__cv_cf *dst, int inv);
typedef struct alwan__cv_dct_plan alwan__cv_dct_plan;
alwan_status alwan__cv_dct_plan_create(alwan__cv_dct_plan **out, int n, int inverse);
void alwan__cv_dct_plan_destroy(alwan__cv_dct_plan *p);
void alwan__cv_dct2d(alwan__cv_dct_plan const *p, float const *src, size_t src_stride, float *dst, size_t dst_stride);
/* xphoto::dctDenoising (opencv_contrib 5.0.0), behind ALWAN_DENOISE_DCT_OPENCV; kind 0 f64,
 * 1 f32, 2 u8. api/alwan_dct_denoise_opencv.c. */
alwan_status alwan__dct_denoise_opencv(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch,
                                       size_t w, size_t h, double sigma, size_t ps, int kind);

typedef struct alwan__fft alwan__fft;
alwan__fft *alwan__fft_create(size_t n);
void alwan__fft_destroy(alwan__fft *f);
void alwan__fft_run(alwan__fft const *f, double *re, double *im, int inverse);

/* Local contrast workers (api/alwan_clahe.c, alwan_local_laplacian.c), behind
 * alwan_local_contrast. CLAHE: one channel of 8-bit (is16 0) or 16-bit data. The local
 * Laplacian: f32 or f64 pixels, mode 0 luminance, 1 separate channels. */
alwan_status alwan__clahe_run(void *out, size_t out_row_stride, void const *src, size_t src_row_stride, size_t w,
                              size_t h, size_t tiles_x, size_t tiles_y, double clip_limit, int is16);
alwan_status alwan__llf_run(void *out, size_t out_row_stride, void const *src, size_t src_row_stride, size_t ch,
                            size_t w, size_t h, double sigma, double alpha, double beta, size_t levels_in,
                            int mode, int is_f32);

/* Histogram matching worker (api/alwan_histogram_match.c), behind alwan_color_transfer_{T}. */
alwan_status alwan__hm_run(void *out, size_t out_stride, void const *src, size_t src_stride, size_t src_count,
                           void const *ref, size_t ref_stride, size_t ref_count, size_t channels, int is_f32);
/* alwan_quantize_octree.c: ALWAN_QUANTIZE_FAST_OCTREE of alwan_quantize_u8 */
alwan_status alwan__quantize_octree(unsigned char *palette_out, size_t *count_out, unsigned int *index_out,
                                    unsigned char const *rgb, size_t pixel_stride, size_t count, size_t max_colors);
/* alwan_quantize_max_coverage.c: ALWAN_QUANTIZE_MAX_COVERAGE of alwan_quantize_u8 */
alwan_status alwan__quantize_max_coverage(unsigned char *palette_out, size_t *count_out, unsigned int *index_out,
                                          unsigned char const *rgb, size_t pixel_stride, size_t count, size_t max_colors);
/* alwan_quantize_ext.c: ALWAN_QUANTIZE_KMEANS, _WU and _OCTREE_CLASSIC of alwan_quantize_ex_u8 */
alwan_status alwan__quantize_ext(unsigned char *palette_out, size_t *count_out, unsigned int *index_out,
                                 unsigned char const *rgb, size_t pixel_stride, size_t count, size_t max_colors,
                                 alwan_quantize_method method, alwan_quantize_params const *params);

/* Batch workers behind the CIECAM02 / CAM16 maps (api/alwan_cam_impl.inc): the
 * viewing-condition terms once, the scalar's _v core per pixel, bit-identical to
 * the scalar. The maps check count and delegate. */
#if ALWAN_WITH_F32
alwan_status alwan__ciecam02_forward_n_f32(alwan_ciecam02_correlates_f32 *out, alwan_f32 const *xyz_in, size_t in_stride, alwan_ciecam02_viewing_conditions_f32 const *vc, size_t count);
alwan_status alwan__ciecam02_inverse_n_f32(alwan_f32 *xyz_out, size_t out_stride, alwan_ciecam02_correlates_f32 const *in, alwan_ciecam02_viewing_conditions_f32 const *vc, size_t count);
alwan_status alwan__cam16_forward_n_f32(alwan_cam16_correlates_f32 *out, alwan_f32 const *xyz_in, size_t in_stride, alwan_cam16_viewing_conditions_f32 const *vc, size_t count);
alwan_status alwan__cam16_inverse_n_f32(alwan_f32 *xyz_out, size_t out_stride, alwan_cam16_correlates_f32 const *in, alwan_cam16_viewing_conditions_f32 const *vc, size_t count);
#endif
#if ALWAN_WITH_F64_FACADE
alwan_status alwan__ciecam02_forward_n_f64(alwan_ciecam02_correlates_f64 *out, alwan_f64 const *xyz_in, size_t in_stride, alwan_ciecam02_viewing_conditions_f64 const *vc, size_t count);
alwan_status alwan__ciecam02_inverse_n_f64(alwan_f64 *xyz_out, size_t out_stride, alwan_ciecam02_correlates_f64 const *in, alwan_ciecam02_viewing_conditions_f64 const *vc, size_t count);
alwan_status alwan__cam16_forward_n_f64(alwan_cam16_correlates_f64 *out, alwan_f64 const *xyz_in, size_t in_stride, alwan_cam16_viewing_conditions_f64 const *vc, size_t count);
alwan_status alwan__cam16_inverse_n_f64(alwan_f64 *xyz_out, size_t out_stride, alwan_cam16_correlates_f64 const *in, alwan_cam16_viewing_conditions_f64 const *vc, size_t count);
#endif

/* ----------------------------------------------------------------
 * Safe allocation helper (overflow protection)
 * ---------------------------------------------------------------- */

/* Check for multiplication overflow before allocation.
 * Returns 0 if overflow would occur, otherwise returns the safe size. */
static inline size_t alwan_safe_array_size(size_t count, size_t elem_size) {
    if (elem_size == 0) return 0;
    if (count > SIZE_MAX / elem_size) return 0;  /* Overflow */
    return count * elem_size;
}

/* ----------------------------------------------------------------
 * Embedded Data (extern declarations)
 * ---------------------------------------------------------------- */

#if ALWAN_EMBED_DATA

/* CAT matrices (3x3 = 9 elements). Dual f32/f64 twins so the templated
 * f32 path reads native float data; ALWAN_CORE_FNLIT(g_cat_NAME) selects. */
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_bradford_f32[9];
#endif
/* Compiled in every build: the documented f64-internal facades read these f64
 * tables from their f32 entry points, so the data has to exist even when the
 * f64 public surface is excluded. See ALWAN_WITH_F64_FACADE. */
#if ALWAN_WITH_F64_FACADE
extern alwan_f64 const g_cat_bradford_f64[9];
#endif
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_cat02_f32[9];
#endif
#if ALWAN_WITH_F64_FACADE
extern alwan_f64 const g_cat_cat02_f64[9];
#endif
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_cat16_f32[9];
#endif
#if ALWAN_WITH_F64_FACADE
extern alwan_f64 const g_cat_cat16_f64[9];
#endif
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_sharp_f32[9];
#endif
#if ALWAN_WITH_F64_FACADE
extern alwan_f64 const g_cat_sharp_f64[9];
#endif
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_fairchild_f32[9];
#endif
#if ALWAN_WITH_F64_FACADE
extern alwan_f64 const g_cat_fairchild_f64[9];
#endif
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_cmccat97_f32[9];
#endif
#if ALWAN_WITH_F64_FACADE
extern alwan_f64 const g_cat_cmccat97_f64[9];
#endif
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_cmccat2000_f32[9];
#endif
#if ALWAN_WITH_F64_FACADE
extern alwan_f64 const g_cat_cmccat2000_f64[9];
#endif
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_cat02_brill_2008_f32[9];
#endif
#if ALWAN_WITH_F64_FACADE
extern alwan_f64 const g_cat_cat02_brill_2008_f64[9];
#endif
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_bianco_2010_f32[9];
#endif
#if ALWAN_WITH_F64_FACADE
extern alwan_f64 const g_cat_bianco_2010_f64[9];
#endif
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_bianco_pc_2010_f32[9];
#endif
#if ALWAN_WITH_F64_FACADE
extern alwan_f64 const g_cat_bianco_pc_2010_f64[9];
#endif
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_von_kries_f32[9];
#endif
#if ALWAN_WITH_F64_FACADE
extern alwan_f64 const g_cat_von_kries_f64[9];
#endif

/* CAM matrices (Hunt-Pointer-Estevez) */
extern alwan_f64 const g_hpe[9];
extern alwan_f64 const g_hpe_inv[9];

/* ICtCp matrices */
extern alwan_f64 const g_ictcp_rgb_to_lms[9];
extern alwan_f64 const g_ictcp_lms_to_rgb[9];
extern alwan_f64 const g_ictcp_lms_p_to_ictcp_pq[9];
extern alwan_f64 const g_ictcp_ictcp_to_lms_p_pq[9];
extern alwan_f64 const g_ictcp_lms_p_to_ictcp_hlg[9];
extern alwan_f64 const g_ictcp_ictcp_to_lms_p_hlg[9];
extern alwan_f64 const g_ictcp_xyz_to_bt2020[9];
extern alwan_f64 const g_ictcp_bt2020_to_xyz[9];

/* IPT matrices and constants */
extern alwan_f64 const g_ipt_exponent;
extern alwan_f64 const g_ipt_xyz_to_lms[9];
extern alwan_f64 const g_ipt_lms_to_xyz[9];
extern alwan_f64 const g_ipt_lms_p_to_ipt[9];
extern alwan_f64 const g_ipt_ipt_to_lms_p[9];

#endif /* ALWAN_EMBED_DATA */

/* ----------------------------------------------------------------
 * YCbCr coefficient resolution (shared between api and map)
 * ---------------------------------------------------------------- */

static inline void alwan__get_ycbcr_coeffs(alwan_ycbcr_standard standard,
                                            alwan_f64 *kr, alwan_f64 *kb) {
    switch (standard) {
        case ALWAN_YCBCR_BT601:
            *kr = ALWAN_LUMA_KR_BT601;
            *kb = ALWAN_LUMA_KB_BT601;
            break;
        case ALWAN_YCBCR_BT709:
            *kr = ALWAN_LUMA_KR_BT709;
            *kb = ALWAN_LUMA_KB_BT709;
            break;
        case ALWAN_YCBCR_BT2020:
            *kr = ALWAN_LUMA_KR_BT2020;
            *kb = ALWAN_LUMA_KB_BT2020;
            break;
        default:
            *kr = ALWAN_LUMA_KR_BT709;
            *kb = ALWAN_LUMA_KB_BT709;
            break;
    }
}

/* ----------------------------------------------------------------
 * Luma coefficient resolution (alwan_luma_standard -> kr, kg, kb)
 * ---------------------------------------------------------------- */

static inline void alwan__get_luma_coeffs(alwan_luma_standard standard,
                                           alwan_f64 *kr, alwan_f64 *kg,
                                           alwan_f64 *kb) {
    switch (standard) {
        case ALWAN_LUMA_BT601:
            *kr = ALWAN_LUMA_KR_BT601;
            *kg = ALWAN_LUMA_KG_BT601;
            *kb = ALWAN_LUMA_KB_BT601;
            break;
        case ALWAN_LUMA_BT709:
            *kr = ALWAN_LUMA_KR_BT709;
            *kg = ALWAN_LUMA_KG_BT709;
            *kb = ALWAN_LUMA_KB_BT709;
            break;
        case ALWAN_LUMA_BT2020:
            *kr = ALWAN_LUMA_KR_BT2020;
            *kg = ALWAN_LUMA_KG_BT2020;
            *kb = ALWAN_LUMA_KB_BT2020;
            break;
        case ALWAN_LUMA_ACES_AP1:
            *kr = ALWAN_LUMA_KR_AP1;
            *kg = ALWAN_LUMA_KG_AP1;
            *kb = ALWAN_LUMA_KB_AP1;
            break;
        case ALWAN_LUMA_ACES_AP0:
            *kr = ALWAN_LUMA_KR_AP0;
            *kg = ALWAN_LUMA_KG_AP0;
            *kb = ALWAN_LUMA_KB_AP0;
            break;
        case ALWAN_LUMA_DISPLAY_P3:
            *kr = ALWAN_LUMA_KR_P3;
            *kg = ALWAN_LUMA_KG_P3;
            *kb = ALWAN_LUMA_KB_P3;
            break;
        case ALWAN_LUMA_DCI_P3:
            *kr = ALWAN_LUMA_KR_DCIP3;
            *kg = ALWAN_LUMA_KG_DCIP3;
            *kb = ALWAN_LUMA_KB_DCIP3;
            break;
        case ALWAN_LUMA_ADOBE_RGB:
            *kr = ALWAN_LUMA_KR_ADOBE;
            *kg = ALWAN_LUMA_KG_ADOBE;
            *kb = ALWAN_LUMA_KB_ADOBE;
            break;
        case ALWAN_LUMA_PROPHOTO_RGB:
            *kr = ALWAN_LUMA_KR_PROPHOTO;
            *kg = ALWAN_LUMA_KG_PROPHOTO;
            *kb = ALWAN_LUMA_KB_PROPHOTO;
            break;
        default:
            *kr = ALWAN_LUMA_KR_BT709;
            *kg = ALWAN_LUMA_KG_BT709;
            *kb = ALWAN_LUMA_KB_BT709;
            break;
    }
}

/* TF inline definitions are exposed transitively to consumers that call
 * alwan_*_oetf/eotf_* directly (alwan_view.c, alwan_convenience_core.h, ...). */
#include "core/alwan_rgb_core.h"

/* ----------------------------------------------------------------
 * Transfer function resolution (enum -> function pointer)
 *
 * Single-source-of-truth X-macro tables drive every TF dispatch in the
 * library. Adding a TF means adding one row -- the four scalar resolvers
 * (defined in alwan_tf_resolve.c) and the SIMD dispatch sites are all
 * regenerated from these tables.
 *
 *   TF_TABLE_BODY(X)      X(ENUM_SUFFIX, OETF_BASE, EOTF_BASE)
 *   TF_SIMD_TABLE_BODY(X) X(ENUM_SUFFIX, OETF_SIMD_FN, EOTF_SIMD_FN)
 *
 * Function names are alwan_<base>_oetf_<prec> / alwan_<base>_eotf_<prec>.
 * LINEAR is intentionally excluded from TF_TABLE_BODY -- its identity is
 * not named *_oetf or *_eotf so the resolvers handle it as a separate case.
 * SIMD entries omit LINEAR (identity = NULL) and asymmetric pairs are
 * encoded explicitly (e.g. HLG uses the "_full" EOTF variant).
 * ---------------------------------------------------------------- */

#define TF_TABLE_BODY(X)                                  \
    X(SRGB,       srgb,         srgb)                     \
    X(BT709,      bt2020,       bt2020)                   \
    X(BT2020,     bt2020,       bt2020)                   \
    X(PQ,         pq,           pq)                       \
    X(HLG,        hlg,          hlg)                      \
    X(BT1886,     gamma24,      bt1886)                   \
    X(ACESPROXY,  acesproxy,    acesproxy)                \
    X(ACESCC,     acescc,       acescc)                   \
    X(ACESCCT,    acescct,      acescct)                  \
    X(SLOG,       slog,         slog)                     \
    X(SLOG2,      slog2,        slog2)                    \
    X(SLOG3,      slog3,        slog3)                    \
    X(CLOG,       clog,         clog)                     \
    X(CLOG2,      clog2,        clog2)                    \
    X(CLOG3,      clog3,        clog3)                    \
    X(VLOG,       vlog,         vlog)                     \
    X(LOGC3,      logc3,        logc3)                    \
    X(LOGC4,      logc4,        logc4)                    \
    X(REDLOG,     redlog,       redlog)                   \
    X(REDLOGFILM, redlogfilm,   redlogfilm)               \
    X(LOG3G10,    log3g10,      log3g10)                  \
    X(BMDFILM,    bmdfilm,      bmdfilm)                  \
    X(BMDFILM4,   bmdfilm4,     bmdfilm4)                 \
    X(TLOG,       tlog,         tlog)                     \
    X(ELOG,       elog,         elog)                     \
    X(PROTUNE,    protune,      protune)                  \
    X(GAMMA22,    gamma22,      gamma22)                  \
    X(GAMMA24,    gamma24,      gamma24)                  \
    X(GAMMA26,    gamma26,      gamma26)                  \
    X(GAMMA28,    gamma28,      gamma28)                  \
    X(NLOG,       nlog,         nlog)                     \
    X(CINEON,     cineon,       cineon)                   \
    X(APPLE_LOG,  apple_log,    apple_log)                \
    X(FLOG,       flog,         flog)                     \
    X(FLOG2,      flog2,        flog2)                    \
    X(LLOG,       llog,         llog)                     \
    X(DLOG,       dlog,         dlog)                     \
    X(DCDM,       dcdm,         dcdm)                     \
    X(ADX10,      adx10,        adx10)                    \
    X(ADX16,      adx16,        adx16)                    \
    X(GAMMA18,    gamma18,      gamma18)                  \
    X(ROMM,       romm,         romm)                     \
    X(RIMM,       rimm,         rimm)                     \
    X(ERIMM,      erimm,        erimm)                    \
    X(LSTAR,      lstar,        lstar)                    \
    X(SMPTE240M,  smpte240m,    smpte240m)                \
    X(ADOBE_RGB,  adobergb,     adobergb)                 \
    X(DAVINCI_INTERMEDIATE, davinci_intermediate, davinci_intermediate) \
    X(H273_LOG,      h273_log,      h273_log)             \
    X(H273_LOG_SQRT, h273_log_sqrt, h273_log_sqrt)        \
    X(XVYCC,         xvycc,         xvycc)                \
    X(BT1361,        bt1361,        bt1361)               \
    X(SYCC,          sycc,          sycc)                 \
    X(BT2020_12BIT,  bt2020_12bit,  bt2020_12bit)         \
    X(LOG3G12,       log3g12,       log3g12)              \
    X(PANALOG,       panalog,       panalog)              \
    X(VIPERLOG,      viperlog,      viperlog)             \
    X(PLOG,          plog,          plog)                 \
    X(FILMIC_PRO6,   filmic_pro6,   filmic_pro6)          \
    X(MILOG,         milog,         milog)                \
    X(LOG2,          log2_shaper,   log2_shaper)          \
    X(DICOM_GSDF,    dicom_gsdf,    dicom_gsdf)           \
    X(ARIB_STD_B67,  arib_std_b67,  arib_std_b67)

#define TF_SIMD_TABLE_BODY(X)                             \
    X(SRGB, srgb_oetf_simd, srgb_eotf_simd)               \
    X(PQ,   pq_oetf_simd,   pq_eotf_simd)                 \
    X(HLG,  hlg_oetf_simd,  hlg_eotf_full_simd)

typedef float  (*alwan_tf_fn_f32)(float);
typedef double (*alwan_tf_fn_f64)(double);

alwan_tf_fn_f32 alwan__resolve_oetf_f32(alwan_transfer_function tf);
alwan_tf_fn_f32 alwan__resolve_eotf_f32(alwan_transfer_function tf);
alwan_tf_fn_f64 alwan__resolve_oetf_f64(alwan_transfer_function tf);
alwan_tf_fn_f64 alwan__resolve_eotf_f64(alwan_transfer_function tf);

#endif /* ALWAN_INTERNAL_H */
