/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Label overlays, as scikit-image computes them: label2rgb (skimage.color), and
 * find_boundaries and mark_boundaries (skimage.segmentation), on the uint32 label
 * images alwan_segment writes.
 *
 * label2rgb OVERLAY colours each region by its rank among the distinct labels that
 * are not the background: the smallest takes the first colour, the next the second,
 * and the colours cycle. The background takes bg_color. The image, when there is
 * one, first loses saturation (through scikit-image's rgb2hsv and hsv2rgb, which are
 * transcribed here), is brightened by image_alpha, and is then blended under the
 * colours at alpha. AVG paints each region with its mean colour instead.
 *
 * find_boundaries marks the pixels whose neighbourhood holds more than one label
 * (a grey dilation differs from a grey erosion), with scikit-image's inner, outer and
 * subpixel variants; mark_boundaries paints them onto an image.
 *
 * 8-bit images are read as value * (1 / 255), which is how img_as_float reads them;
 * value / 255 differs from it in the last bit for some values.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* scikit-image's DEFAULT_COLORS: red, blue, yellow, magenta, green, indigo, darkorange,
 * cyan, pink, yellowgreen, from skimage.color.rgb_colors. */
static double const g_label_colors[10][3] = {
    {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {1.0, 1.0, 0.0}, {1.0, 0.0, 1.0}, {0.0, 0.502, 0.0},
    {0.294, 0.0, 0.51}, {1.0, 0.549, 0.0}, {0.0, 1.0, 1.0}, {1.0, 0.753, 0.796}, {0.604, 0.804, 0.196}};

#define ALWAN__LABEL_MAX_PIXELS ((size_t)1 << 30)

static int alwan__lab_size_ok(size_t w, size_t h) {
    return w > 0 && h > 0 && w <= ALWAN__LABEL_MAX_PIXELS / h;
}

static uint32_t alwan__lab_at(uint32_t const *labels, size_t rs, size_t x, size_t y) {
    return ((uint32_t const *)((char const *)labels + y * rs))[x];
}

static int alwan__lab_cmp(void const *a, void const *b) {
    uint32_t const x = *(uint32_t const *)a, y = *(uint32_t const *)b;
    return x < y ? -1 : x > y ? 1 : 0;
}

/* The distinct labels, ascending; *n receives their number. Caller frees. */
static uint32_t *alwan__lab_unique(uint32_t const *labels, size_t rs, size_t w, size_t h, size_t *n) {
    size_t const total = w * h;
    uint32_t *u = (uint32_t *)ALWAN_ALLOC(total * sizeof(uint32_t), sizeof(uint32_t));
    size_t x, y, k = 0, m = 0;
    if (!u) return NULL;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) u[k++] = alwan__lab_at(labels, rs, x, y);
    qsort(u, total, sizeof(uint32_t), alwan__lab_cmp);
    for (k = 0; k < total; k++)
        if (k == 0 || u[k] != u[m - 1]) u[m++] = u[k];
    *n = m;
    return u;
}

/* numpy's pairwise summation of a contiguous run, in double and in float. */
static double alwan__lab_pairwise(double const *a, size_t n) {
    if (n < 8) {
        double r = 0.0;
        size_t i;
        for (i = 0; i < n; i++) r += a[i];
        return r;
    }
    if (n <= 128) {
        double r[8], res;
        size_t i, j;
        for (j = 0; j < 8; j++) r[j] = a[j];
        for (i = 8; i < n - (n % 8); i += 8)
            for (j = 0; j < 8; j++) r[j] += a[i + j];
        res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
        for (; i < n; i++) res += a[i];
        return res;
    }
    {
        size_t n2 = n / 2;
        n2 -= n2 % 8;
        return alwan__lab_pairwise(a, n2) + alwan__lab_pairwise(a + n2, n - n2);
    }
}

static float alwan__lab_pairwise_f(float const *a, size_t n) {
    if (n < 8) {
        float r = 0.0f;
        size_t i;
        for (i = 0; i < n; i++) r += a[i];
        return r;
    }
    if (n <= 128) {
        float r[8], res;
        size_t i, j;
        for (j = 0; j < 8; j++) r[j] = a[j];
        for (i = 8; i < n - (n % 8); i += 8)
            for (j = 0; j < 8; j++) r[j] += a[i + j];
        res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
        for (; i < n; i++) res += a[i];
        return res;
    }
    {
        size_t n2 = n / 2;
        n2 -= n2 % 8;
        return alwan__lab_pairwise_f(a, n2) + alwan__lab_pairwise_f(a + n2, n - n2);
    }
}

static size_t alwan__lab_find(uint32_t const *u, size_t n, uint32_t v) {
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t const mid = lo + (hi - lo) / 2;
        if (u[mid] < v) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

/* ---------------------------------------------------------------- *
 * scikit-image's rgb2hsv and hsv2rgb, in the precision of the image
 * ---------------------------------------------------------------- */

/* numpy's remainder: fmod, moved into the divisor's sign. */
#define ALWAN__LAB_HSV(T, SFX, FMOD, FLOOR)                                                              \
    static void alwan__lab_rgb2hsv_##SFX(T hsv[3], T const rgb[3]) {                                     \
        T const r = rgb[0], g = rgb[1], b = rgb[2];                                                      \
        T const v = r > g ? (r > b ? r : b) : (g > b ? g : b);                                          \
        T const mn = r < g ? (r < b ? r : b) : (g < b ? g : b);                                         \
        T const delta = v - mn;                                                                          \
        T s = delta / v, h = (T)0;                                                                       \
        if (delta == (T)0) s = (T)0;                                                                     \
        if (r == v) h = (g - b) / delta;                                                                 \
        if (g == v) h = (T)2 + (b - r) / delta;                                                          \
        if (b == v) h = (T)4 + (r - g) / delta;                                                          \
        {                                                                                                \
            T mod = FMOD(h / (T)6, (T)1);                                                                \
            if (mod != (T)0 && mod < (T)0) mod += (T)1;                                                  \
            h = mod;                                                                                     \
        }                                                                                                \
        if (delta == (T)0) h = (T)0;                                                                     \
        hsv[0] = h != h ? (T)0 : h;                                                                      \
        hsv[1] = s != s ? (T)0 : s;                                                                      \
        hsv[2] = v != v ? (T)0 : v;                                                                      \
    }                                                                                                    \
    static void alwan__lab_hsv2rgb_##SFX(T rgb[3], T const hsv[3]) {                                     \
        T const hi = FLOOR(hsv[0] * (T)6);                                                               \
        T const f = hsv[0] * (T)6 - hi;                                                                  \
        T const p = hsv[2] * ((T)1 - hsv[1]);                                                            \
        T const q = hsv[2] * ((T)1 - f * hsv[1]);                                                        \
        T const t = hsv[2] * ((T)1 - ((T)1 - f) * hsv[1]);                                               \
        T const v = hsv[2];                                                                              \
        int const sector = (int)(unsigned char)(int)hi % 6;                                              \
        switch (sector) {                                                                                \
        case 0: rgb[0] = v; rgb[1] = t; rgb[2] = p; break;                                               \
        case 1: rgb[0] = q; rgb[1] = v; rgb[2] = p; break;                                               \
        case 2: rgb[0] = p; rgb[1] = v; rgb[2] = t; break;                                               \
        case 3: rgb[0] = p; rgb[1] = q; rgb[2] = v; break;                                               \
        case 4: rgb[0] = t; rgb[1] = p; rgb[2] = v; break;                                               \
        default: rgb[0] = v; rgb[1] = p; rgb[2] = q; break;                                              \
        }                                                                                                \
    }

ALWAN__LAB_HSV(double, f64, fmod, floor)
ALWAN__LAB_HSV(float, f32, fmodf, floorf)

/* ---------------------------------------------------------------- *
 * label2rgb
 * ---------------------------------------------------------------- */

typedef enum { ALWAN__LAB_F64 = 0, ALWAN__LAB_F32 = 1, ALWAN__LAB_U8 = 2 } alwan__lab_type;

/* The image pixel at (x, y) as RGB in its working precision: gray repeated, u8 as
 * value * (1 / 255). */
static void alwan__lab_pixel(double rgb[3], void const *image, size_t irs, size_t channels, alwan__lab_type t,
                             size_t x, size_t y) {
    char const *row = (char const *)image + y * irs;
    size_t c;
    for (c = 0; c < 3; c++) {
        size_t const k = channels == 1 ? x : x * channels + c;
        if (t == ALWAN__LAB_F64) rgb[c] = ((double const *)row)[k];
        else if (t == ALWAN__LAB_F32) rgb[c] = (double)((float const *)row)[k];
        else rgb[c] = (double)((unsigned char const *)row)[k] * (1.0 / 255.0);
    }
}

static int alwan__lab_params_ok(alwan_label2rgb_params const *p) {
    if ((int)p->kind < (int)ALWAN_LABEL2RGB_OVERLAY || (int)p->kind > (int)ALWAN_LABEL2RGB_AVG) return 0;
    if (p->colors && p->color_count == 0) return 0;
    if (p->alpha_given && !(p->alpha == p->alpha)) return 0;
    if (p->image_alpha_given && !(p->image_alpha == p->image_alpha)) return 0;
    if (!(p->saturation >= 0.0 && p->saturation <= 1.0)) return 0;
    return 1;
}

static alwan_status alwan__label2rgb(double *out, size_t ors, uint32_t const *labels, size_t lrs, void const *image,
                                     size_t irs, size_t channels, size_t w, size_t h,
                                     alwan_label2rgb_params const *params, alwan__lab_type t) {
    alwan_label2rgb_params p;
    uint32_t *u;
    size_t n = 0, x, y, c;
    int has_bg = 0;
    size_t bg_rank = 0;
    if (!out || !labels || !alwan__lab_size_ok(w, h)) return ALWAN_E_INVALID;
    if (image && channels != 1 && channels != 3) return ALWAN_E_INVALID;
    if (params) p = *params;
    else memset(&p, 0, sizeof p);
    if (!alwan__lab_params_ok(&p)) return ALWAN_E_INVALID;
    if (p.kind == ALWAN_LABEL2RGB_AVG && (!image || p.bg_color_none)) return ALWAN_E_INVALID;
    u = alwan__lab_unique(labels, lrs, w, h, &n);
    if (!u) return ALWAN_E_NOMEM;
    if (!p.no_background) {
        size_t const k = alwan__lab_find(u, n, p.bg_label);
        if (k < n && u[k] == p.bg_label) {
            has_bg = 1;
            bg_rank = k;
        }
    }

    if (p.kind == ALWAN_LABEL2RGB_OVERLAY) {
        double const alpha = image ? (p.alpha_given ? p.alpha : 0.3) : 1.0;
        double const image_alpha = p.image_alpha_given ? p.image_alpha : 1.0;
        double const *bg = p.bg_color_given ? p.bg_color : NULL;
        double const zero[3] = {0.0, 0.0, 0.0};
        size_t const ncol = p.colors ? p.color_count : 10;
        for (y = 0; y < h; y++) {
            double *orow = (double *)((char *)out + y * ors);
            for (x = 0; x < w; x++) {
                uint32_t const lab = alwan__lab_at(labels, lrs, x, y);
                size_t const k = alwan__lab_find(u, n, lab);
                size_t rank;
                double const *col;
                double img[3] = {0.0, 0.0, 0.0};
                int const is_bg = has_bg && k == bg_rank;
                if (is_bg) rank = 0;
                else rank = (has_bg && k < bg_rank) ? k + 1 : (has_bg ? k : k + 1);
                if (rank == 0) col = bg ? bg : zero;
                else col = p.colors ? p.colors + 3 * ((rank - 1) % ncol) : g_label_colors[(rank - 1) % ncol];
                if (image) {
                    alwan__lab_pixel(img, image, irs, channels, t, x, y);
                    if (t == ALWAN__LAB_F32) {
                        float f[3], hsv[3];
                        for (c = 0; c < 3; c++) f[c] = (float)img[c];
                        if (channels == 3) {
                            alwan__lab_rgb2hsv_f32(hsv, f);
                            hsv[1] *= (float)p.saturation;
                            alwan__lab_hsv2rgb_f32(f, hsv);
                        }
                        for (c = 0; c < 3; c++) f[c] = f[c] * (float)image_alpha + (float)(1.0 - image_alpha);
                        for (c = 0; c < 3; c++) img[c] = (double)f[c];
                    } else {
                        if (channels == 3) {
                            double hsv[3];
                            alwan__lab_rgb2hsv_f64(hsv, img);
                            hsv[1] *= p.saturation;
                            alwan__lab_hsv2rgb_f64(img, hsv);
                        }
                        for (c = 0; c < 3; c++) img[c] = img[c] * image_alpha + (1.0 - image_alpha);
                    }
                }
                for (c = 0; c < 3; c++) {
                    double v;
                    if (is_bg && p.bg_color_none) v = img[c];
                    else if (t == ALWAN__LAB_F32 && image)
                        v = col[c] * alpha + (double)((float)img[c] * (float)(1.0 - alpha));
                    else v = col[c] * alpha + img[c] * (1.0 - alpha);
                    orow[3 * x + c] = v;
                }
            }
        }
        ALWAN_FREE(u);
        return ALWAN_OK;
    }

    /* AVG: each region's mean, summed row by row as numpy's mean(axis=0) does; 8-bit
     * means truncated as they are stored back into a uint8 array. */
    {
        double *sum = (double *)ALWAN_ALLOC(n * 3 * sizeof(double), sizeof(double));
        float *sumf = NULL;
        size_t *cnt = (size_t *)ALWAN_ALLOC(n * sizeof(size_t), sizeof(size_t));
        if (t == ALWAN__LAB_F32) sumf = (float *)ALWAN_ALLOC(n * 3 * sizeof(float), sizeof(float));
        if (!sum || !cnt || (t == ALWAN__LAB_F32 && !sumf)) {
            ALWAN_FREE(sum);
            ALWAN_FREE(cnt);
            ALWAN_FREE(sumf);
            ALWAN_FREE(u);
            return ALWAN_E_NOMEM;
        }
        memset(sum, 0, n * 3 * sizeof(double));
        memset(cnt, 0, n * sizeof(size_t));
        if (sumf) memset(sumf, 0, n * 3 * sizeof(float));
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                size_t const k = alwan__lab_find(u, n, alwan__lab_at(labels, lrs, x, y));
                char const *row = (char const *)image + y * irs;
                cnt[k]++;
                for (c = 0; c < (channels == 1 ? 1u : 3u); c++) {
                    size_t const idx = channels == 1 ? x : x * 3 + c;
                    if (t == ALWAN__LAB_F64) sum[3 * k + c] += ((double const *)row)[idx];
                    else if (t == ALWAN__LAB_F32) sumf[3 * k + c] += ((float const *)row)[idx];
                    else sum[3 * k + c] += (double)((unsigned char const *)row)[idx];
                }
            }
        }
        /* A grey image's region is a 1-D run, which numpy's mean sums pairwise (float32
         * in float32, 8-bit in float64 buffers of 8192): regroup each region's values in
         * image order and sum them that way. */
        if (channels == 1) {
            size_t *start = (size_t *)ALWAN_ALLOC((n + 1) * sizeof(size_t), sizeof(size_t));
            double *vals = (double *)ALWAN_ALLOC(w * h * sizeof(double), sizeof(double));
            size_t k;
            if (!start || !vals) {
                ALWAN_FREE(start);
                ALWAN_FREE(vals);
                ALWAN_FREE(sum);
                ALWAN_FREE(cnt);
                ALWAN_FREE(sumf);
                ALWAN_FREE(u);
                return ALWAN_E_NOMEM;
            }
            start[0] = 0;
            for (k = 0; k < n; k++) start[k + 1] = start[k] + cnt[k];
            for (y = 0; y < h; y++) {
                char const *row = (char const *)image + y * irs;
                for (x = 0; x < w; x++) {
                    size_t const kk = alwan__lab_find(u, n, alwan__lab_at(labels, lrs, x, y));
                    double const v = t == ALWAN__LAB_F64   ? ((double const *)row)[x]
                                     : t == ALWAN__LAB_F32 ? (double)((float const *)row)[x]
                                                           : (double)((unsigned char const *)row)[x];
                    vals[start[kk]++] = v;
                }
            }
            for (k = n; k > 0; k--) start[k] = start[k - 1];
            start[0] = 0;
            for (k = 0; k < n; k++) {
                double const *run = vals + start[k];
                if (t == ALWAN__LAB_F32) {
                    float *f = (float *)(void *)(vals + start[k]);
                    size_t i;
                    for (i = 0; i < cnt[k]; i++) f[i] = (float)run[i];
                    sumf[3 * k] = 0.0f + alwan__lab_pairwise_f(f, cnt[k]);
                } else if (t == ALWAN__LAB_U8) {
                    size_t i;
                    double r = 0.0;
                    for (i = 0; i < cnt[k]; i += 8192)
                        r += alwan__lab_pairwise(run + i, cnt[k] - i < 8192 ? cnt[k] - i : 8192);
                    sum[3 * k] = r;
                } else {
                    sum[3 * k] = 0.0 + alwan__lab_pairwise(run, cnt[k]);
                }
            }
            ALWAN_FREE(start);
            ALWAN_FREE(vals);
        }
        for (y = 0; y < h; y++) {
            double *orow = (double *)((char *)out + y * ors);
            for (x = 0; x < w; x++) {
                size_t const k = alwan__lab_find(u, n, alwan__lab_at(labels, lrs, x, y));
                for (c = 0; c < 3; c++) {
                    size_t const cc = channels == 1 ? 0 : c;
                    double v;
                    if (has_bg && k == bg_rank) {
                        v = p.bg_color_given ? p.bg_color[c] : 0.0;
                        if (t == ALWAN__LAB_F32) v = (double)(float)v;
                        else if (t == ALWAN__LAB_U8) v = (double)(unsigned char)(int)v;
                    } else if (t == ALWAN__LAB_F32) {
                        v = (double)(sumf[3 * k + cc] / (float)cnt[k]);
                    } else {
                        v = sum[3 * k + cc] / (double)cnt[k];
                        if (t == ALWAN__LAB_U8) v = floor(v);
                    }
                    orow[3 * x + c] = v;
                }
            }
        }
        ALWAN_FREE(sum);
        ALWAN_FREE(cnt);
        ALWAN_FREE(sumf);
    }
    ALWAN_FREE(u);
    return ALWAN_OK;
}

#if ALWAN_WITH_F64
alwan_status alwan_label2rgb_f64(double *out, size_t out_row_stride, uint32_t const *labels, size_t labels_row_stride,
                                 alwan_f64 const *image, size_t image_row_stride, size_t channels, size_t width,
                                 size_t height, alwan_label2rgb_params const *params) {
    return alwan__label2rgb(out, out_row_stride, labels, labels_row_stride, image, image_row_stride, channels, width,
                            height, params, ALWAN__LAB_F64);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_label2rgb_f32(double *out, size_t out_row_stride, uint32_t const *labels, size_t labels_row_stride,
                                 alwan_f32 const *image, size_t image_row_stride, size_t channels, size_t width,
                                 size_t height, alwan_label2rgb_params const *params) {
    return alwan__label2rgb(out, out_row_stride, labels, labels_row_stride, image, image_row_stride, channels, width,
                            height, params, ALWAN__LAB_F32);
}
#endif

alwan_status alwan_label2rgb_u8(double *out, size_t out_row_stride, uint32_t const *labels, size_t labels_row_stride,
                                unsigned char const *image, size_t image_row_stride, size_t channels, size_t width,
                                size_t height, alwan_label2rgb_params const *params) {
    return alwan__label2rgb(out, out_row_stride, labels, labels_row_stride, image, image_row_stride, channels, width,
                            height, params, ALWAN__LAB_U8);
}

/* ---------------------------------------------------------------- *
 * find_boundaries
 * ---------------------------------------------------------------- */

/* Grey dilation (max) or erosion (min) of the labels at (x, y) over a 3 x 3
 * footprint, the cross or the full square, edges repeated ('reflect' with radius 1). */
static uint32_t alwan__lab_morph(uint32_t const *labels, size_t rs, size_t w, size_t h, size_t x, size_t y, int full,
                                 int dilate, uint32_t const *override_bg, uint32_t bg) {
    uint32_t best = 0;
    int first = 1, dx, dy;
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            size_t xx, yy;
            uint32_t v;
            if (!full && dx != 0 && dy != 0) continue;
            xx = (dx < 0 && x == 0) ? 0 : (dx > 0 && x + 1 >= w) ? x : x + (size_t)(ptrdiff_t)dx;
            yy = (dy < 0 && y == 0) ? 0 : (dy > 0 && y + 1 >= h) ? y : y + (size_t)(ptrdiff_t)dy;
            v = alwan__lab_at(labels, rs, xx, yy);
            if (override_bg && v == bg) v = *override_bg;
            if (first || (dilate ? v > best : v < best)) best = v;
            first = 0;
        }
    }
    return best;
}

static int alwan__lab_bmode_ok(alwan_boundary_mode m) {
    return (int)m >= (int)ALWAN_BOUNDARY_THICK && (int)m <= (int)ALWAN_BOUNDARY_SUBPIXEL;
}

/* One pixel's boundary flag for THICK, INNER and OUTER. */
static int alwan__lab_is_boundary(uint32_t const *labels, size_t rs, size_t w, size_t h, size_t x, size_t y,
                                  alwan_boundary_mode mode, int full, uint32_t background) {
    uint32_t const lab = alwan__lab_at(labels, rs, x, y);
    int b = alwan__lab_morph(labels, rs, w, h, x, y, full, 1, NULL, 0) !=
            alwan__lab_morph(labels, rs, w, h, x, y, full, 0, NULL, 0);
    if (mode == ALWAN_BOUNDARY_INNER) {
        b = b && lab != background;
    } else if (mode == ALWAN_BOUNDARY_OUTER) {
        uint32_t const max_label = 0xFFFFFFFFu;
        int const is_bg = lab == background;
        int const adjacent = !is_bg && (alwan__lab_morph(labels, rs, w, h, x, y, 1, 1, NULL, 0) !=
                                        alwan__lab_morph(labels, rs, w, h, x, y, 1, 0, &max_label, background));
        b = b && (is_bg || adjacent);
    }
    return b;
}

alwan_status alwan_find_boundaries(unsigned char *out, size_t out_row_stride, uint32_t const *labels,
                                   size_t labels_row_stride, size_t width, size_t height,
                                   alwan_boundary_params const *params) {
    alwan_boundary_params p;
    size_t x, y;
    int full;
    if (!out || !labels || !alwan__lab_size_ok(width, height)) return ALWAN_E_INVALID;
    if (params) p = *params;
    else memset(&p, 0, sizeof p);
    if (!alwan__lab_bmode_ok(p.mode) || (p.connectivity != 0 && p.connectivity != 1 && p.connectivity != 2))
        return ALWAN_E_INVALID;
    full = p.connectivity == 2;
    if (p.mode != ALWAN_BOUNDARY_SUBPIXEL) {
        for (y = 0; y < height; y++) {
            unsigned char *orow = out + y * out_row_stride;
            for (x = 0; x < width; x++)
                orow[x] = (unsigned char)alwan__lab_is_boundary(labels, labels_row_stride, width, height, x, y,
                                                                p.mode, full, p.background);
        }
        return ALWAN_OK;
    }
    /* SUBPIXEL: the labels spread onto a (2w - 1) x (2h - 1) grid with the in-between
     * cells set to the largest label value; an in-between cell is a boundary when its
     * 3 x 3 window (edges repeated) holds more than two distinct values. */
    {
        size_t const ew = 2 * width - 1, eh = 2 * height - 1;
        uint32_t const max_label = 0xFFFFFFFFu;
        for (y = 0; y < eh; y++) {
            unsigned char *orow = out + y * out_row_stride;
            for (x = 0; x < ew; x++) {
                uint32_t vals[9];
                int nv = 0, dx, dy;
                if ((x % 2 == 0) && (y % 2 == 0)) {
                    orow[x] = 0;
                    continue;
                }
                for (dy = -1; dy <= 1; dy++) {
                    for (dx = -1; dx <= 1; dx++) {
                        long xx = (long)x + dx, yy = (long)y + dy;
                        uint32_t v;
                        int k, seen = 0;
                        if (xx < 0) xx = 0;
                        if (yy < 0) yy = 0;
                        if (xx >= (long)ew) xx = (long)ew - 1;
                        if (yy >= (long)eh) yy = (long)eh - 1;
                        v = ((xx % 2 == 0) && (yy % 2 == 0))
                                ? alwan__lab_at(labels, labels_row_stride, (size_t)xx / 2, (size_t)yy / 2)
                                : max_label;
                        for (k = 0; k < nv; k++)
                            if (vals[k] == v) seen = 1;
                        if (!seen) vals[nv++] = v;
                    }
                }
                orow[x] = (unsigned char)(nv > 2);
            }
        }
    }
    return ALWAN_OK;
}

/* ---------------------------------------------------------------- *
 * mark_boundaries
 * ---------------------------------------------------------------- */

static alwan_status alwan__mark_boundaries(double *out, size_t ors, void const *image, size_t irs, size_t channels,
                                          uint32_t const *labels, size_t lrs, size_t w, size_t h,
                                          alwan_mark_boundaries_params const *params, alwan__lab_type t) {
    alwan_mark_boundaries_params p;
    alwan_boundary_mode mode;
    unsigned char *bnd;
    double color[3] = {1.0, 1.0, 0.0}, outline[3] = {0.0, 0.0, 0.0};
    size_t x, y, c;
    if (!out || !image || !labels || !alwan__lab_size_ok(w, h) || (channels != 1 && channels != 3))
        return ALWAN_E_INVALID;
    if (params) p = *params;
    else memset(&p, 0, sizeof p);
    mode = p.mode_given ? p.mode : ALWAN_BOUNDARY_OUTER;
    if (!alwan__lab_bmode_ok(mode) || mode == ALWAN_BOUNDARY_SUBPIXEL) return ALWAN_E_INVALID;
    if (p.color_given)
        for (c = 0; c < 3; c++) color[c] = p.color[c];
    if (p.outline_given)
        for (c = 0; c < 3; c++) outline[c] = p.outline_color[c];
    if (t == ALWAN__LAB_F32)
        for (c = 0; c < 3; c++) {
            color[c] = (double)(float)color[c];
            outline[c] = (double)(float)outline[c];
        }
    bnd = (unsigned char *)ALWAN_ALLOC(w * h, 1);
    if (!bnd) return ALWAN_E_NOMEM;
    {
        alwan_boundary_params bp;
        memset(&bp, 0, sizeof bp);
        bp.mode = mode;
        bp.background = p.background;
        alwan_find_boundaries(bnd, w, labels, lrs, w, h, &bp);
    }
    for (y = 0; y < h; y++) {
        double *orow = (double *)((char *)out + y * ors);
        for (x = 0; x < w; x++) {
            double px[3];
            alwan__lab_pixel(px, image, irs, channels, t, x, y);
            if (bnd[y * w + x]) {
                for (c = 0; c < 3; c++) px[c] = color[c];
            } else if (p.outline_given) {
                int any = 0, dx, dy;
                for (dy = -1; dy <= 1 && !any; dy++)
                    for (dx = -1; dx <= 1 && !any; dx++) {
                        long const xx = (long)x + dx, yy = (long)y + dy;
                        if (xx >= 0 && yy >= 0 && xx < (long)w && yy < (long)h && bnd[(size_t)yy * w + (size_t)xx])
                            any = 1;
                    }
                if (any)
                    for (c = 0; c < 3; c++) px[c] = outline[c];
            }
            for (c = 0; c < 3; c++) orow[3 * x + c] = px[c];
        }
    }
    ALWAN_FREE(bnd);
    return ALWAN_OK;
}

#if ALWAN_WITH_F64
alwan_status alwan_mark_boundaries_f64(double *out, size_t out_row_stride, alwan_f64 const *image,
                                       size_t image_row_stride, size_t channels, uint32_t const *labels,
                                       size_t labels_row_stride, size_t width, size_t height,
                                       alwan_mark_boundaries_params const *params) {
    return alwan__mark_boundaries(out, out_row_stride, image, image_row_stride, channels, labels, labels_row_stride,
                                  width, height, params, ALWAN__LAB_F64);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_mark_boundaries_f32(double *out, size_t out_row_stride, alwan_f32 const *image,
                                       size_t image_row_stride, size_t channels, uint32_t const *labels,
                                       size_t labels_row_stride, size_t width, size_t height,
                                       alwan_mark_boundaries_params const *params) {
    return alwan__mark_boundaries(out, out_row_stride, image, image_row_stride, channels, labels, labels_row_stride,
                                  width, height, params, ALWAN__LAB_F32);
}
#endif

alwan_status alwan_mark_boundaries_u8(double *out, size_t out_row_stride, unsigned char const *image,
                                      size_t image_row_stride, size_t channels, uint32_t const *labels,
                                      size_t labels_row_stride, size_t width, size_t height,
                                      alwan_mark_boundaries_params const *params) {
    return alwan__mark_boundaries(out, out_row_stride, image, image_row_stride, channels, labels, labels_row_stride,
                                  width, height, params, ALWAN__LAB_U8);
}
