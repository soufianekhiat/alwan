/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2026 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Internal declarations shared by alwan's image-processing files: the edge-aware filter
 * workers, segmentation, warping and resizing, the OpenCV ports and their DFT / DCT, the
 * DFT, local contrast, histogram matching and the quantisers. They are image processing,
 * bound for suwar, and are kept in this one header so they move as a unit. Included by
 * alwan_internal.h; nothing outside alwan reads it.
 */

#ifndef ALWAN_INTERNAL_IMAGE_H
#define ALWAN_INTERNAL_IMAGE_H

#include "alwan.h"

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
/* api/alwan_bilateral_texture.c: ximgproc::bilateralTextureFilter, 1 or 3 channels */
alwan_status alwan__btf_run(void *out, size_t out_rs, void const *src, size_t src_rs, size_t cn, size_t w, size_t h,
                            size_t fr, size_t iterations, double sigma_alpha, double sigma_avg, int is_f32);
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
/* alwan_am_filter.c: cv::resize INTER_LINEAR on one packed float plane (OpenCV 5.0.0, the
 * fast 2x2 area path at a factor of exactly 2, a copy at the same size); 0 on no memory */
int alwan__cv_resize_f32_plane(float *dst, int dw, int dh, float const *src, int sw, int sh);
/* alwan_filter.c: alwan_filter's engine whatever precisions the build holds; kind 0 f64, 1 f32 */
alwan_status alwan__filter_run(void *out, size_t out_rs, void const *src, size_t src_rs, size_t channels, size_t width,
                               size_t height, alwan_filter_method method, alwan_filter_params const *params, int kind);
/* alwan_optical_flow.c: ALWAN_REGISTER_ECC (cv::findTransformECC, OpenCV 5.0.0) on two
 * packed float images of the same size, behind alwan_register */
alwan_status alwan__register_ecc(alwan_register_result *out, float const *ref, float const *mov, size_t w, size_t h,
                                 alwan_register_params const *params);
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
/* api/alwan_denoise_bilateral.c: BILATERAL_OPENCV, BILATERAL_SKIMAGE and WIENER_LOCAL; kind 0 f64,
 * 1 f32, 2 u8 (the u8 skimage path takes sigma_color and cval in 0..1). */
alwan_status alwan__denoise_bilateral_opencv(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w,
                                             size_t h, size_t diameter, double sigma_color, double sigma_space, int kind);
alwan_status alwan__denoise_bilateral_skimage(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w,
                                              size_t h, size_t win, double sigma_color, double sigma_spatial, size_t bins,
                                              int border, double cval, int kind);
alwan_status alwan__denoise_wiener(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                   size_t size, double noise, int kind);

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

#endif /* ALWAN_INTERNAL_IMAGE_H */
