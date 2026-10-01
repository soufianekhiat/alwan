/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Dithering for quantisation: blue noise masks by Ulichney's void-and-cluster method
 * ("The void-and-cluster method for dither array generation", Proc. SPIE 1913, 1993),
 * ordered dithering with a Bayer or blue noise mask (RPDF or TPDF), and error diffusion
 * (Floyd and Steinberg 1976; Jarvis, Judice and Ninke 1976; Stucki 1981), applied to an
 * encoded float image as it is quantised to 8 or 16 bit codes. Suite 285.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_dither_core.h"
#include <stdlib.h>
#include <string.h>

/* exp(-d2 / 4.5), Ulichney's sigma of 1.5, at d2 = 0..128 (gendata/data/blue_noise_kernel.py):
 * a table, so the deterministic build ranks the same masks as the release build. */
static double const bn_kernel[129] = {
#include "../data/blue_noise/void_cluster_kernel.csv"
};

/* The built-in masks: alwan_blue_noise_mask_generate at seed 0, side 64 and 128
 * (gendata/data/blue_noise_masks.py, which calls this library). */
static uint16_t const bn_mask_64[64 * 64] = {
#include "../data/blue_noise/mask_64.csv"
};
static uint16_t const bn_mask_128[128 * 128] = {
#include "../data/blue_noise/mask_128.csv"
};

#define BN_RADIUS 8
#define BN_MAX_PIXELS 65536u

/* splitmix64 (Steele, Lea and Flood 2014): the initial pattern's positions from the seed. */
static uint64_t bn_splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

typedef struct {
    size_t w, h, n;
    int rx, ry;                 /* the energy window's half sizes, at most BN_RADIUS */
    unsigned char *bits;
    double *energy;
} bn_field;

/* Add sign times the window's weights around pixel p, wrapping round the torus. */
static void bn_toggle(bn_field *f, size_t p, double sign) {
    long const x = (long)(p % f->w), y = (long)(p / f->w);
    long const w = (long)f->w, h = (long)f->h;
    long dy, dx;
    for (dy = -f->ry; dy <= f->ry; dy++) {
        long const yy = ((y + dy) % h + h) % h;
        double *row = f->energy + (size_t)yy * f->w;
        for (dx = -f->rx; dx <= f->rx; dx++) {
            long const xx = ((x + dx) % w + w) % w;
            row[xx] += sign * bn_kernel[dx * dx + dy * dy];
        }
    }
    f->bits[p] = (unsigned char)(sign > 0.0);
}

/* The tightest cluster (the 1 of most energy) or the largest void (the 0 of least):
 * the first such pixel in raster order. */
static size_t bn_extreme(bn_field const *f, int want_one) {
    size_t best = (size_t)-1, i;
    double e = 0.0;
    for (i = 0; i < f->n; i++) {
        if (f->bits[i] != (unsigned char)want_one) continue;
        if (best == (size_t)-1 || (want_one ? f->energy[i] > e : f->energy[i] < e)) {
            best = i;
            e = f->energy[i];
        }
    }
    return best;
}

alwan_status alwan_blue_noise_mask_generate(uint32_t *ranks_out, size_t width, size_t height, uint64_t seed) {
    bn_field proto, work;
    size_t n, ones, i, r, guard;
    uint64_t state = seed;
    size_t *order;
    if (!ranks_out || width < 4 || height < 4) return ALWAN_E_INVALID;
    if (width > BN_MAX_PIXELS || height > BN_MAX_PIXELS || width * height > BN_MAX_PIXELS) return ALWAN_E_RANGE;
    n = width * height;
    proto.w = work.w = width;
    proto.h = work.h = height;
    proto.n = work.n = n;
    /* a window wider than the torus would count a pixel twice */
    proto.rx = work.rx = (int)((width - 1) / 2 < BN_RADIUS ? (width - 1) / 2 : BN_RADIUS);
    proto.ry = work.ry = (int)((height - 1) / 2 < BN_RADIUS ? (height - 1) / 2 : BN_RADIUS);
    proto.bits = (unsigned char *)calloc(n, 1);
    work.bits = (unsigned char *)malloc(n);
    proto.energy = (double *)calloc(n, sizeof(double));
    work.energy = (double *)malloc(n * sizeof(double));
    order = (size_t *)malloc(n * sizeof(size_t));
    if (!proto.bits || !work.bits || !proto.energy || !work.energy || !order) {
        free(proto.bits); free(work.bits); free(proto.energy); free(work.energy); free(order);
        return ALWAN_E_NOMEM;
    }
    /* The initial pattern: a tenth of the pixels, drawn without replacement. */
    ones = n / 10 ? n / 10 : 1;
    for (i = 0; i < n; i++) order[i] = i;
    for (i = 0; i < ones; i++) {
        size_t const j = i + (size_t)(bn_splitmix64(&state) % (uint64_t)(n - i));
        size_t const t = order[i];
        order[i] = order[j];
        order[j] = t;
        bn_toggle(&proto, order[i], 1.0);
    }
    /* Relax it: move the tightest cluster to the largest void until the void is where the
     * cluster was. Ulichney's loop ends; the guard only bounds a pathological pattern. */
    for (guard = 0; guard < n; guard++) {
        size_t const c = bn_extreme(&proto, 1);
        size_t v;
        bn_toggle(&proto, c, -1.0);
        v = bn_extreme(&proto, 0);
        if (v == c) {
            bn_toggle(&proto, c, 1.0);
            break;
        }
        bn_toggle(&proto, v, 1.0);
    }
    /* Phase 1: from the prototype, take the tightest cluster away, ranks ones-1 down to 0. */
    memcpy(work.bits, proto.bits, n);
    memcpy(work.energy, proto.energy, n * sizeof(double));
    for (r = ones; r-- > 0;) {
        size_t const c = bn_extreme(&work, 1);
        ranks_out[c] = (uint32_t)r;
        bn_toggle(&work, c, -1.0);
    }
    /* Phases 2 and 3: from the prototype, fill the largest void, ranks ones to n-1. Phase 3's
     * tightest cluster of zeros is the zero of least energy here: the window sums to the
     * same everywhere on the torus, so the zeros' energy is that sum less the ones'. */
    for (r = ones; r < n; r++) {
        size_t const v = bn_extreme(&proto, 0);
        ranks_out[v] = (uint32_t)r;
        bn_toggle(&proto, v, 1.0);
    }
    free(proto.bits); free(work.bits); free(proto.energy); free(work.energy); free(order);
    return ALWAN_OK;
}

alwan_status alwan_blue_noise_mask_builtin(uint32_t *ranks_out, size_t side) {
    uint16_t const *src;
    size_t i;
    if (!ranks_out) return ALWAN_E_INVALID;
    if (side == 64) src = bn_mask_64;
    else if (side == 128) src = bn_mask_128;
    else return ALWAN_E_RANGE;
    for (i = 0; i < side * side; i++) ranks_out[i] = src[i];
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Quantisation
 * ---------------------------------------------------------------- */

typedef struct {
    alwan_dither_method method;
    int bits, store_codes, log2_bayer, tpdf, decorrelate, serpentine;
    uint32_t frame;
    double levels, scale;             /* code range, and code to stored value */
    unsigned fmt_max;
    uint32_t const *mask32;
    uint16_t const *mask16;
    size_t mw, mh;
} dq_setup;

static alwan_status dq_prepare(dq_setup *s, alwan_pixel_format fmt, size_t channels, size_t width, size_t height,
                               alwan_dither_method method, alwan_dither_params const *params) {
    alwan_dither_params p;
    int fmt_bits;
    if (params) p = *params; else memset(&p, 0, sizeof p);
    if (channels < 1 || channels > 4 || width == 0 || height == 0) return ALWAN_E_INVALID;
    if (fmt == ALWAN_PIXEL_U8) fmt_bits = 8;
    else if (fmt == ALWAN_PIXEL_U16) fmt_bits = 16;
    else return ALWAN_E_INVALID;
    if ((int)method < (int)ALWAN_DITHER_NONE || (int)method > (int)ALWAN_DITHER_STUCKI) return ALWAN_E_INVALID;
    memset(s, 0, sizeof *s);
    s->method = method;
    s->bits = p.bits ? p.bits : fmt_bits;
    if (s->bits < 1 || s->bits > fmt_bits) return ALWAN_E_INVALID;
    s->store_codes = p.store_codes != 0;
    s->log2_bayer = p.bayer_log2_size ? p.bayer_log2_size : 3;
    if (s->log2_bayer < 1 || s->log2_bayer > 8) return ALWAN_E_INVALID;
    s->tpdf = p.tpdf != 0;
    s->decorrelate = p.decorrelate_channels != 0;
    s->serpentine = p.serpentine != 0;
    s->frame = p.frame;
    s->fmt_max = (1u << fmt_bits) - 1u;
    s->levels = (double)((1u << s->bits) - 1u);
    s->scale = s->store_codes ? 1.0 : (double)s->fmt_max / s->levels;
    if (method == ALWAN_DITHER_ORDERED_BLUE_NOISE) {
        if (p.mask) {
            if (p.mask_width == 0 || p.mask_height == 0) return ALWAN_E_INVALID;
            s->mask32 = p.mask;
            s->mw = p.mask_width;
            s->mh = p.mask_height;
        } else {
            s->mask16 = bn_mask_64;
            s->mw = s->mh = 64;
        }
    }
    return ALWAN_OK;
}

static void dq_store(void *row, alwan_pixel_format fmt, size_t i, double code, dq_setup const *s) {
    double v = s->store_codes ? code : ALWAN_FLOOR_F64(code * s->scale + 0.5);
    if (v > (double)s->fmt_max) v = (double)s->fmt_max;
    if (fmt == ALWAN_PIXEL_U8) ((uint8_t *)row)[i] = (uint8_t)v;
    else ((uint16_t *)row)[i] = (uint16_t)v;
}

/* The R2 sequence's fractions (Roberts 2018), shifting the mask per frame and per channel:
 * offsets spread evenly whatever the count. */
static void dq_offset(dq_setup const *s, size_t c, size_t *ox, size_t *oy) {
    double const k = (double)s->frame * 4.0 + (double)(s->decorrelate ? c : 0);
    double fx = k * 0.7548776662466927, fy = k * 0.5698402909980532;
    fx -= ALWAN_FLOOR_F64(fx);
    fy -= ALWAN_FLOOR_F64(fy);
    *ox = (size_t)(fx * (double)s->mw);
    *oy = (size_t)(fy * (double)s->mh);
    if (*ox >= s->mw) *ox = s->mw - 1;
    if (*oy >= s->mh) *oy = s->mh - 1;
}

/* The ordered offset for pixel (x, y) and channel c: the threshold, or 0.5 plus TPDF noise. */
static double dq_ordered_offset(dq_setup const *s, size_t x, size_t y, size_t c) {
    double u;
    if (s->method == ALWAN_DITHER_ORDERED_BAYER) {
        int const side = 1 << s->log2_bayer;
        size_t ox = 0, oy = 0;
        int rank;
        if (s->decorrelate || s->frame) {
            dq_setup t = *s;
            t.mw = t.mh = (size_t)side;
            dq_offset(&t, c, &ox, &oy);
        }
        rank = alwan_dither_bayer_rank_f64_v((int)((x + ox) & (size_t)(side - 1)),
                                             (int)((y + oy) & (size_t)(side - 1)), s->log2_bayer);
        u = alwan_dither_threshold_f64_v(rank, side * side);
    } else {
        size_t ox, oy, mx, my;
        uint32_t rank;
        dq_offset(s, c, &ox, &oy);
        mx = (x + ox) % s->mw;
        my = (y + oy) % s->mh;
        rank = s->mask32 ? s->mask32[my * s->mw + mx] : s->mask16[my * s->mw + mx];
        u = alwan_dither_threshold_f64_v((int)rank, (int)(s->mw * s->mh));
    }
    return s->tpdf ? 0.5 + alwan_dither_tpdf_f64_v(u) : u;
}

/* Error diffusion weights, row by row from the current one, columns -2..+2, and the divisor. */
static int const ed_fs[3][5] = {{0, 0, 0, 7, 0}, {0, 3, 5, 1, 0}, {0, 0, 0, 0, 0}};
static int const ed_jjn[3][5] = {{0, 0, 0, 7, 5}, {3, 5, 7, 5, 3}, {1, 3, 5, 3, 1}};
static int const ed_stucki[3][5] = {{0, 0, 0, 8, 4}, {2, 4, 8, 4, 2}, {1, 2, 4, 2, 1}};

/* A quantiser fed one row at a time, in order: the setup, the carried error of the current
 * row and the two below it (two columns of margin each side), and the row's index. */
typedef struct {
    dq_setup s;
    alwan_pixel_format fmt;
    size_t channels, width;
    double *err;
} dq_state;

static alwan_status dq_begin(dq_state *q, alwan_pixel_format fmt, size_t channels, size_t width, size_t height,
                             alwan_dither_method method, alwan_dither_params const *params) {
    alwan_status st = dq_prepare(&q->s, fmt, channels, width, height, method, params);
    if (st != ALWAN_OK) return st;
    q->fmt = fmt;
    q->channels = channels;
    q->width = width;
    q->err = NULL;
    if (method >= ALWAN_DITHER_FLOYD_STEINBERG) {
        q->err = (double *)calloc(3 * (width + 4) * channels, sizeof(double));
        if (!q->err) return ALWAN_E_NOMEM;
    }
    return ALWAN_OK;
}

/* Row y of in (width x channels values, already in double) into out. */
static alwan_status dq_row(dq_state *q, void *out, double const *in, size_t y) {
    dq_setup const *s = &q->s;
    size_t const ch = q->channels, w = q->width;
    size_t x, c;
    for (x = 0; x < w * ch; x++) {
        if (!(in[x] == in[x]) || in[x] > 1e300 || in[x] < -1e300) return ALWAN_E_INVALID;
    }
    if (!q->err) {
        for (x = 0; x < w; x++) {
            for (c = 0; c < ch; c++) {
                double const off = s->method == ALWAN_DITHER_NONE ? 0.5 : dq_ordered_offset(s, x, y, c);
                dq_store(out, q->fmt, x * ch + c, alwan_dither_quantize_f64_v(in[x * ch + c], s->levels, off), s);
            }
        }
        return ALWAN_OK;
    } else {
        int const (*k)[5] = s->method == ALWAN_DITHER_FLOYD_STEINBERG ? ed_fs
                          : s->method == ALWAN_DITHER_JARVIS_JUDICE_NINKE ? ed_jjn : ed_stucki;
        double const div = s->method == ALWAN_DITHER_FLOYD_STEINBERG ? 16.0
                         : s->method == ALWAN_DITHER_JARVIS_JUDICE_NINKE ? 48.0 : 42.0;
        size_t const rw = (w + 4) * ch;
        double *rows[3];
        int const rev = s->serpentine && (y & 1);
        size_t i;
        rows[0] = q->err + (y % 3) * rw;
        rows[1] = q->err + ((y + 1) % 3) * rw;
        rows[2] = q->err + ((y + 2) % 3) * rw;
        for (i = 0; i < w; i++) {
            x = rev ? w - 1 - i : i;
            for (c = 0; c < ch; c++) {
                double const t = in[x * ch + c] * s->levels + rows[0][(x + 2) * ch + c];
                double qv = ALWAN_FLOOR_F64(t + 0.5), e;
                int dy, dx;
                if (qv < 0.0) qv = 0.0;
                if (qv > s->levels) qv = s->levels;
                e = t - qv;
                dq_store(out, q->fmt, x * ch + c, qv, s);
                for (dy = 0; dy < 3; dy++) {
                    for (dx = -2; dx <= 2; dx++) {
                        int const wgt = k[dy][dx + 2];
                        /* a right-to-left row mirrors the kernel */
                        long const xx = rev ? (long)x - dx : (long)x + dx;
                        if (!wgt || xx < 0 || xx >= (long)w) continue;   /* error off the edge is dropped */
                        rows[dy][((size_t)xx + 2) * ch + c] += e * (double)wgt / div;
                    }
                }
            }
        }
        memset(rows[0], 0, rw * sizeof(double));             /* becomes the row two below */
        return ALWAN_OK;
    }
}

static void dq_end(dq_state *q) { free(q->err); q->err = NULL; }

/* A whole float image, f32 or f64, through the row quantiser. */
static alwan_status dq_quantize(void *dst, size_t dst_row_stride, alwan_pixel_format dst_fmt, void const *src,
                                int src_is_f32, size_t src_row_stride, size_t channels, size_t width, size_t height,
                                alwan_dither_method method, alwan_dither_params const *params) {
    dq_state q;
    alwan_status st;
    double *buf;
    size_t y, i;
    size_t const elem = src_is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    if (!dst || !src) return ALWAN_E_INVALID;
    st = dq_begin(&q, dst_fmt, channels, width, height, method, params);
    if (st != ALWAN_OK) return st;
    if (src_row_stride < width * channels * elem
        || dst_row_stride < width * channels * (dst_fmt == ALWAN_PIXEL_U8 ? 1u : 2u)) {
        dq_end(&q);
        return ALWAN_E_INVALID;
    }
    buf = (double *)malloc(width * channels * sizeof(double));
    if (!buf) {
        dq_end(&q);
        return ALWAN_E_NOMEM;
    }
    for (y = 0; y < height && st == ALWAN_OK; y++) {
        void const *in = (char const *)src + y * src_row_stride;
        for (i = 0; i < width * channels; i++) {
            buf[i] = src_is_f32 ? (double)((alwan_f32 const *)in)[i] : ((alwan_f64 const *)in)[i];
        }
        st = dq_row(&q, (char *)dst + y * dst_row_stride, buf, y);
    }
    free(buf);
    dq_end(&q);
    return st;
}

#if ALWAN_WITH_F64
alwan_status alwan_dither_quantize_f64(void *dst, size_t dst_row_stride, alwan_pixel_format dst_fmt,
                                       alwan_f64 const *src, size_t src_row_stride, size_t channels,
                                       size_t width, size_t height, alwan_dither_method method,
                                       alwan_dither_params const *params) {
    return dq_quantize(dst, dst_row_stride, dst_fmt, src, 0, src_row_stride, channels, width, height, method, params);
}

/* alwan_image_convert of each row into F64 in dst_space's encoding, then that row dithered into
 * dst at its own index, so masks tile and error diffusion carries down the image as
 * alwan_dither_quantize does on a whole float image. */
alwan_status alwan_image_convert_dithered_f64(void *dst, size_t dst_row_stride, void const *src, size_t src_row_stride,
                                              size_t width, size_t height, alwan_pixel_format dst_fmt,
                                              alwan_pixel_format src_fmt, alwan_rgb_space_desc_f64 const *src_space,
                                              alwan_rgb_space_desc_f64 const *dst_space, alwan_dither_method method,
                                              alwan_dither_params const *params, alwan_ctx *ctx) {
    dq_state q;
    alwan_status st;
    alwan_f64 *row;
    size_t y;
    if (!dst || !src || !src_space || !dst_space) return ALWAN_E_INVALID;
    st = dq_begin(&q, dst_fmt, 3, width, height, method, params);
    if (st != ALWAN_OK) return st;
    if (dst_row_stride < width * 3 * (dst_fmt == ALWAN_PIXEL_U8 ? 1u : 2u)) {
        dq_end(&q);
        return ALWAN_E_INVALID;
    }
    row = (alwan_f64 *)malloc(width * 3 * sizeof(alwan_f64));
    if (!row) {
        dq_end(&q);
        return ALWAN_E_NOMEM;
    }
    for (y = 0; y < height && st == ALWAN_OK; y++) {
        st = alwan_image_convert_f64(row, width * 3 * sizeof(alwan_f64), (char const *)src + y * src_row_stride,
                                     src_row_stride, width, 1, ALWAN_PIXEL_F64, src_fmt, src_space, dst_space, ctx);
        if (st == ALWAN_OK) st = dq_row(&q, (char *)dst + y * dst_row_stride, row, y);
    }
    free(row);
    dq_end(&q);
    return st;
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_dither_quantize_f32(void *dst, size_t dst_row_stride, alwan_pixel_format dst_fmt,
                                       alwan_f32 const *src, size_t src_row_stride, size_t channels,
                                       size_t width, size_t height, alwan_dither_method method,
                                       alwan_dither_params const *params) {
    return dq_quantize(dst, dst_row_stride, dst_fmt, src, 1, src_row_stride, channels, width, height, method, params);
}

/* As alwan_image_convert_dithered_f64, through an F32 row. */
alwan_status alwan_image_convert_dithered_f32(void *dst, size_t dst_row_stride, void const *src, size_t src_row_stride,
                                              size_t width, size_t height, alwan_pixel_format dst_fmt,
                                              alwan_pixel_format src_fmt, alwan_rgb_space_desc_f32 const *src_space,
                                              alwan_rgb_space_desc_f32 const *dst_space, alwan_dither_method method,
                                              alwan_dither_params const *params, alwan_ctx *ctx) {
    dq_state q;
    alwan_status st;
    alwan_f32 *row;
    double *buf;
    size_t y, i;
    if (!dst || !src || !src_space || !dst_space) return ALWAN_E_INVALID;
    st = dq_begin(&q, dst_fmt, 3, width, height, method, params);
    if (st != ALWAN_OK) return st;
    if (dst_row_stride < width * 3 * (dst_fmt == ALWAN_PIXEL_U8 ? 1u : 2u)) {
        dq_end(&q);
        return ALWAN_E_INVALID;
    }
    row = (alwan_f32 *)malloc(width * 3 * sizeof(alwan_f32));
    buf = (double *)malloc(width * 3 * sizeof(double));
    if (!row || !buf) {
        free(row);
        free(buf);
        dq_end(&q);
        return ALWAN_E_NOMEM;
    }
    for (y = 0; y < height && st == ALWAN_OK; y++) {
        st = alwan_image_convert_f32(row, width * 3 * sizeof(alwan_f32), (char const *)src + y * src_row_stride,
                                     src_row_stride, width, 1, ALWAN_PIXEL_F32, src_fmt, src_space, dst_space, ctx);
        if (st != ALWAN_OK) break;
        for (i = 0; i < width * 3; i++) buf[i] = (double)row[i];
        st = dq_row(&q, (char *)dst + y * dst_row_stride, buf, y);
    }
    free(row);
    free(buf);
    dq_end(&q);
    return st;
}
#endif
