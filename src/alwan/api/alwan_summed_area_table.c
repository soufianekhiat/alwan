/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Summed-area tables (Crow 1984, "Summed-area tables for texture mapping"):
 * one pass builds S(x, y), the sum over the pixels above and left of a
 * corner; the sum over any rectangle is then four reads. The queries
 * themselves are in core/alwan_summed_area_reader.inc, so a shader runs the
 * same ones. Suite 281.
 *
 * The table is double whatever the image. 8- and 16-bit images are summed as
 * their integer codes, exactly while a sum stays below 2^53 (a 16-bit image of
 * 2^37 pixels), and scaled to [0, 1] at the query. A floating-point image has
 * its channel mean, rounded to a multiple of 1/256, taken off every pixel
 * first: the corners then hold sums of values near 0 and a rectangle's four
 * reads cancel far less (suite 281 measures it), while an image of integers
 * stays exact.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <string.h>
#include "../core/alwan_summed_area_core.h"
#include "../core/alwan_half_core.h"

struct alwan_summed_area_table_s {
    double *table;
    size_t count;
    int w, h, ch;
};

/* One channel value of a pixel, in the units the table sums: codes for the
 * integer formats, the value for the others. */
static double alwan__sat_load(void const *row, size_t i, alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8:  return (double)((unsigned char const *)row)[i];
    case ALWAN_PIXEL_U16: return (double)((unsigned short const *)row)[i];
    case ALWAN_PIXEL_F16: return (double)alwan_half_to_float_f64_v((alwan_half)((unsigned short const *)row)[i]);
    case ALWAN_PIXEL_F32: return (double)((float const *)row)[i];
    case ALWAN_PIXEL_F64: return ((double const *)row)[i];
    }
    return 0.0;
}

static size_t alwan__sat_bytes(alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8:  return 1;
    case ALWAN_PIXEL_U16: return 2;
    case ALWAN_PIXEL_F16: return 2;
    case ALWAN_PIXEL_F32: return 4;
    case ALWAN_PIXEL_F64: return 8;
    }
    return 0;
}

alwan_status alwan_summed_area_table_create(alwan_summed_area_table **out, void const *image, size_t row_stride,
                                            size_t width, size_t height, size_t channels, alwan_pixel_format format,
                                            alwan_ctx *ctx) {
    alwan_summed_area_table *t;
    size_t const bytes = alwan__sat_bytes(format);
    size_t corners, count, x, y, c;
    double mean[4] = { 0.0, 0.0, 0.0, 0.0 };
    double offset[4] = { 0.0, 0.0, 0.0, 0.0 };
    double divisor = 1.0;
    int const is_int = format == ALWAN_PIXEL_U8 || format == ALWAN_PIXEL_U16;
    int w, h, ch;
    (void)ctx;
    if (!out) return ALWAN_E_INVALID;
    *out = NULL;
    if (!image || width == 0 || height == 0 || channels == 0 || channels > 4 || bytes == 0) return ALWAN_E_INVALID;
    if (row_stride == 0) row_stride = width * channels * bytes;
    if (row_stride / bytes / channels < width) return ALWAN_E_INVALID;
    /* every offset is an int, for a shader */
    if (width >= 0x7fffffffu || height >= 0x7fffffffu) return ALWAN_E_RANGE;
    corners = (width + 1) * (height + 1);
    if (corners / (width + 1) != height + 1 || corners > (0x7fffffffu - 8u) / channels) return ALWAN_E_RANGE;
    count = corners * channels + channels + 1;
    w = (int)width; h = (int)height; ch = (int)channels;

    /* the values: finite, and the float formats' channel means */
    for (y = 0; y < height; y++) {
        void const *row = (unsigned char const *)image + y * row_stride;
        for (x = 0; x < width; x++) {
            for (c = 0; c < channels; c++) {
                double const v = alwan__sat_load(row, x * channels + c, format);
                if (!(v >= -1.7976931348623157e308 && v <= 1.7976931348623157e308)) return ALWAN_E_INVALID;
                mean[c] += v;
            }
        }
    }
    if (is_int) {
        divisor = format == ALWAN_PIXEL_U8 ? 255.0 : 65535.0;
    } else {
        for (c = 0; c < channels; c++) {
            double const m = mean[c] / ((double)width * (double)height);
            offset[c] = ALWAN_FLOOR(m * 256.0 + 0.5) / 256.0;
        }
    }

    t = (alwan_summed_area_table *)ALWAN_ALLOC(sizeof(*t), sizeof(double));
    if (!t) return ALWAN_E_NOMEM;
    t->table = (double *)ALWAN_ALLOC(alwan_safe_array_size(count, sizeof(double)), sizeof(double));
    if (!t->table) {
        ALWAN_FREE(t);
        return ALWAN_E_NOMEM;
    }
    t->count = count;
    t->w = w; t->h = h; t->ch = ch;
    memset(t->table, 0, (width + 1) * channels * sizeof(double));
    for (y = 0; y < height; y++) {
        void const *row = (unsigned char const *)image + y * row_stride;
        double *above = t->table + (y * (width + 1)) * channels;
        double *cur = t->table + ((y + 1) * (width + 1)) * channels;
        for (c = 0; c < channels; c++) {
            double run = 0.0;
            cur[c] = 0.0;
            for (x = 0; x < width; x++) {
                run += alwan__sat_load(row, x * channels + c, format) - offset[c];
                cur[(x + 1) * channels + c] = above[(x + 1) * channels + c] + run;
            }
        }
    }
    for (c = 0; c < channels; c++) t->table[corners * channels + c] = offset[c];
    t->table[corners * channels + channels] = divisor;
    *out = t;
    return ALWAN_OK;
}

void alwan_summed_area_table_destroy(alwan_summed_area_table *table, alwan_ctx *ctx) {
    (void)ctx;
    if (!table) return;
    ALWAN_FREE(table->table);
    ALWAN_FREE(table);
}

alwan_status alwan_summed_area_table_sum(alwan_f64 *sum_out, size_t x0, size_t y0, size_t x1, size_t y1,
                                         alwan_summed_area_table const *table) {
    int c;
    if (!sum_out || !table) return ALWAN_E_INVALID;
    if (x0 > x1 || y0 > y1 || x1 > (size_t)table->w || y1 > (size_t)table->h) return ALWAN_E_RANGE;
    for (c = 0; c < table->ch; c++) {
        sum_out[c] = alwan_sat_sum_f64_v(table->table, table->w, table->h, table->ch, c, (int)x0, (int)y0, (int)x1, (int)y1);
    }
    return ALWAN_OK;
}

alwan_status alwan_summed_area_table_mean(alwan_f64 *mean_out, size_t x0, size_t y0, size_t x1, size_t y1,
                                          alwan_summed_area_table const *table) {
    alwan_status st;
    int c;
    if (!mean_out || !table) return ALWAN_E_INVALID;
    if (x0 >= x1 || y0 >= y1) return ALWAN_E_RANGE;
    st = alwan_summed_area_table_sum(mean_out, x0, y0, x1, y1, table);
    if (st != ALWAN_OK) return st;
    for (c = 0; c < table->ch; c++) mean_out[c] /= (double)(x1 - x0) * (double)(y1 - y0);
    return ALWAN_OK;
}

alwan_status alwan_summed_area_table_integrate(alwan_f64 *integral_out, alwan_f64 x0, alwan_f64 y0, alwan_f64 x1,
                                               alwan_f64 y1, alwan_summed_area_table const *table) {
    int c;
    if (!integral_out || !table) return ALWAN_E_INVALID;
    if (!(x0 >= 0.0 && y0 >= 0.0 && x1 <= (double)table->w && y1 <= (double)table->h && x0 <= x1 && y0 <= y1)) {
        return ALWAN_E_RANGE;
    }
    for (c = 0; c < table->ch; c++) {
        integral_out[c] = alwan_sat_integral_f64_v(table->table, table->w, table->h, table->ch, c, x0, y0, x1, y1);
    }
    return ALWAN_OK;
}

alwan_status alwan_summed_area_table_get_layout(alwan_summed_area_table_layout *out, alwan_summed_area_table const *table) {
    if (!out || !table) return ALWAN_E_INVALID;
    out->table = table->table;
    out->table_count = table->count;
    out->width = table->w;
    out->height = table->h;
    out->channels = table->ch;
    return ALWAN_OK;
}

/* The box mean: each pixel the mean of the (2 rx + 1) x (2 ry + 1) window around
 * it, the window cut at the image's edges and the mean taken over what is left. */
static alwan_status alwan__sat_box(void *out, size_t out_row_stride, int is_f32, size_t radius_x, size_t radius_y,
                                   alwan_summed_area_table const *table) {
    size_t x, y, w, h, ch, elem;
    int c;
    if (!out || !table) return ALWAN_E_INVALID;
    w = (size_t)table->w; h = (size_t)table->h; ch = (size_t)table->ch;
    elem = is_f32 ? sizeof(float) : sizeof(double);
    if (out_row_stride == 0) out_row_stride = w * ch * elem;
    if (out_row_stride < w * ch * elem) return ALWAN_E_INVALID;
    for (y = 0; y < h; y++) {
        unsigned char *row = (unsigned char *)out + y * out_row_stride;
        size_t const ya = y > radius_y ? y - radius_y : 0;
        size_t const yb = (h - 1 - y) > radius_y ? y + radius_y + 1 : h;
        for (x = 0; x < w; x++) {
            size_t const xa = x > radius_x ? x - radius_x : 0;
            size_t const xb = (w - 1 - x) > radius_x ? x + radius_x + 1 : w;
            double const area = (double)(xb - xa) * (double)(yb - ya);
            for (c = 0; c < table->ch; c++) {
                double const m = alwan_sat_sum_f64_v(table->table, table->w, table->h, table->ch, c,
                                                     (int)xa, (int)ya, (int)xb, (int)yb) / area;
                if (is_f32) ((float *)row)[x * ch + (size_t)c] = (float)m;
                else ((double *)row)[x * ch + (size_t)c] = m;
            }
        }
    }
    return ALWAN_OK;
}

#if ALWAN_WITH_F32
alwan_status alwan_summed_area_table_box_mean_f32(alwan_f32 *out, size_t out_row_stride, size_t radius_x, size_t radius_y,
                                                  alwan_summed_area_table const *table) {
    return alwan__sat_box(out, out_row_stride, 1, radius_x, radius_y, table);
}
#endif
#if ALWAN_WITH_F64
alwan_status alwan_summed_area_table_box_mean_f64(alwan_f64 *out, size_t out_row_stride, size_t radius_x, size_t radius_y,
                                                  alwan_summed_area_table const *table) {
    return alwan__sat_box(out, out_row_stride, 0, radius_x, radius_y, table);
}
#endif
