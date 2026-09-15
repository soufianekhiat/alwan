/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test patterns rendered at any resolution: the colour bar signals of ITU-R BT.471-1,
 * the ARIB STD-B28 multiformat colour bar and the EBU Tech 3325 monitor test patterns,
 * as their native R'G'B' signal in fractions of white, interleaved or planar.
 *
 * Every stripe edge is the pattern's own fraction of the width, rounded to the nearest
 * sample, and every band edge the same fraction of the height, so a pattern keeps its
 * proportions at any size. Levels below black (ARIB's -2 %) stay negative: they are
 * the signal, and a caller who clips them loses the PLUGE.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <stddef.h>
#include <string.h>

/* ---------------------------------------------------------------- levels */

/* R'G'B' of the eight BT.471 bars, white to black, from the four numbers of the
 * signal's name: white, black, coloured maximum, coloured minimum. */
static void alwan__bt471_bar(double *rgb, int bar, double white, double black, double hi, double lo) {
    static int const k_on[8][3] = { { 1, 1, 1 }, { 1, 1, 0 }, { 0, 1, 1 }, { 0, 1, 0 },
                                    { 1, 0, 1 }, { 1, 0, 0 }, { 0, 0, 1 }, { 0, 0, 0 } };
    int c;
    for (c = 0; c < 3; c++) {
        rgb[c] = bar == 0 ? white : bar == 7 ? black : (k_on[bar][c] ? hi : lo);
    }
}

/* ARIB STD-B28 levels as R'G'B'. The +I signal is the standard's own component values,
 * R = 41.2545, G = 16.6946, B = 0 IRE. */
enum {
    ALWAN__B28_GREY40, ALWAN__B28_W75, ALWAN__B28_Y75, ALWAN__B28_C75, ALWAN__B28_G75, ALWAN__B28_M75,
    ALWAN__B28_R75, ALWAN__B28_B75, ALWAN__B28_C100, ALWAN__B28_B100, ALWAN__B28_W100, ALWAN__B28_PLUS_I,
    ALWAN__B28_Y100, ALWAN__B28_R100, ALWAN__B28_GREY15, ALWAN__B28_BLACK, ALWAN__B28_M2, ALWAN__B28_P2,
    ALWAN__B28_P4, ALWAN__B28_RAMP, ALWAN__B28_LEVELS
};

static double const k_b28[ALWAN__B28_LEVELS][3] = {
    { 0.40, 0.40, 0.40 }, { 0.75, 0.75, 0.75 }, { 0.75, 0.75, 0.0 }, { 0.0, 0.75, 0.75 },
    { 0.0, 0.75, 0.0 }, { 0.75, 0.0, 0.75 }, { 0.75, 0.0, 0.0 }, { 0.0, 0.0, 0.75 },
    { 0.0, 1.0, 1.0 }, { 0.0, 0.0, 1.0 }, { 1.0, 1.0, 1.0 }, { 0.412545, 0.166946, 0.0 },
    { 1.0, 1.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 0.15, 0.15, 0.15 }, { 0.0, 0.0, 0.0 },
    { -0.02, -0.02, -0.02 }, { 0.02, 0.02, 0.02 }, { 0.04, 0.04, 0.04 }, { 0.0, 0.0, 0.0 },
};

/* EBU Tech 3325 Table 5: luma codes of the grey-scale patches EBU_4-1 to EBU_4-20. */
static unsigned short const k_ebu_steps[20] = { 64,  86,  138, 190, 242, 294, 346, 398, 450, 502,
                                                554, 606, 658, 710, 762, 814, 866, 918, 940, 1019 };

/* Tables 7 and 6: D'Y, D'CB, D'CR of the 15 EBU test colours, then red, green and blue. */
static unsigned short const k_ebu_colours[18][3] = {
    { 381, 470, 578 }, { 636, 457, 599 }, { 582, 478, 592 }, { 577, 340, 480 }, { 579, 544, 411 },
    { 586, 597, 543 }, { 433, 443, 487 }, { 460, 465, 703 }, { 658, 380, 370 }, { 470, 639, 468 },
    { 319, 490, 616 }, { 487, 422, 396 }, { 321, 617, 491 }, { 655, 349, 673 }, { 494, 601, 593 },
    { 250, 409, 960 }, { 691, 167, 105 }, { 127, 960, 471 },
};

/* A 10-bit narrow-range luma code as R'G'B': 64 is black, 940 white. */
static double alwan__ebu_level(unsigned code) {
    return ((double)code - 64.0) / 876.0;
}

/* The R'G'B' a D'Y, D'CB, D'CR triple decodes to, through the four-digit BT.709
 * coefficients Tech 3325 Annex 3 encodes with. The codes are quantised, so a primary's
 * other two channels land a little off 0 (red's blue is -0.00098) and stay there. */
static void alwan__ebu_colour(double *rgb, unsigned short const *cv) {
    double const y = ((double)cv[0] - 64.0) / 876.0;
    double const cb = ((double)cv[1] - 512.0) / 896.0, cr = ((double)cv[2] - 512.0) / 896.0;
    rgb[0] = y + 1.5748 * cr;
    rgb[2] = y + 1.8556 * cb;
    rgb[1] = (y - 0.2126 * rgb[0] - 0.0722 * rgb[2]) / 0.7152;
}

/* ---------------------------------------------------------------- geometry */

/* The sample an edge at fraction f of the width falls on, rounded to the nearest. */
static size_t alwan__edge(double f, size_t w) {
    return (size_t)ALWAN_FLOOR(f * (double)w + 0.5);
}

/* One band of stripes: n cumulative edge fractions (the last is 1) and n levels. */
static void alwan__stripes(double *row, size_t w, double const *edges, int const *levels, int n, double ramp_x0,
                           double ramp_len) {
    size_t x = 0;
    int k;
    for (k = 0; k < n; k++) {
        size_t const end = alwan__edge(edges[k], w);
        for (; x < end && x < w; x++) {
            double *px = row + 3 * x;
            if (levels[k] == ALWAN__B28_RAMP) {
                /* ARIB's ramp: black to white at one 10-bit code per sample at 1920,
                 * centred on the picture, black before it and white after. */
                double v = ((double)x - ramp_x0) / ramp_len;
                v = v < 0.0 ? 0.0 : v > 1.0 ? 1.0 : v;
                px[0] = px[1] = px[2] = v;
            } else {
                px[0] = k_b28[levels[k]][0];
                px[1] = k_b28[levels[k]][1];
                px[2] = k_b28[levels[k]][2];
            }
        }
    }
}

static void alwan__fill_row(double *row, size_t w, double v) {
    size_t x;
    for (x = 0; x < 3 * w; x++) row[x] = v;
}

/* Paint, in row y, the rectangle centred on (cx, cy) samples with half sides hw and hh.
 * Each edge rounds to the nearest sample and the rectangle is cut to the picture. */
static void alwan__rect_row(double *row, size_t w, size_t y, double cx, double cy, double hw, double hh,
                            double const *rgb) {
    double const x0 = ALWAN_FLOOR(cx - hw + 0.5), x1 = ALWAN_FLOOR(cx + hw + 0.5);
    double const y0 = ALWAN_FLOOR(cy - hh + 0.5), y1 = ALWAN_FLOOR(cy + hh + 0.5);
    size_t x, xa, xb;
    if ((double)y < y0 || (double)y >= y1) return;
    xa = x0 <= 0.0 ? 0 : x0 >= (double)w ? w : (size_t)x0;
    xb = x1 <= 0.0 ? 0 : x1 >= (double)w ? w : (size_t)x1;
    for (x = xa; x < xb; x++) {
        row[3 * x + 0] = rgb[0];
        row[3 * x + 1] = rgb[1];
        row[3 * x + 2] = rgb[2];
    }
}

/* A Tech 3325 patch, an H/7.5 square, at measurement point p (1 to 13, Figure 7):
 * w and h from the centre in fractions of the width and height, h upwards. */
static void alwan__ebu_patch(double *row, size_t w, size_t h, size_t y, unsigned p, double const *rgb) {
    static double const k_points[13][2] = {
        { 0.0, 0.0 },  { 0.0, 0.4 },  { 0.2, 0.2 },   { 0.2, -0.2 }, { 0.0, -0.4 }, { -0.2, -0.2 }, { -0.2, 0.2 },
        { 0.4, 0.4 },  { 0.4, 0.0 },  { 0.4, -0.4 },  { -0.4, 0.4 }, { -0.4, 0.0 }, { -0.4, -0.4 },
    };
    double const half = (double)h / 15.0;
    double const cx = (double)w * 0.5 + k_points[p - 1][0] * (double)w;
    double const cy = (double)h * 0.5 - k_points[p - 1][1] * (double)h;
    alwan__rect_row(row, w, y, cx, cy, half, half, rgb);
}

/* One row of an EBU Tech 3325 pattern. */
static void alwan__ebu_row(double *row, alwan_pattern pattern, alwan_pattern_params const *pr, size_t y, size_t w,
                           size_t h) {
    static double const k_black[3] = { 0.0, 0.0, 0.0 }, k_white[3] = { 1.0, 1.0, 1.0 };
    double rgb[3];
    switch (pattern) {
    case ALWAN_PATTERN_EBU_1:
    case ALWAN_PATTERN_EBU_2:
        rgb[0] = rgb[1] = rgb[2] = alwan__ebu_level(1019);
        alwan__fill_row(row, w, alwan__ebu_level(502));
        alwan__ebu_patch(row, w, h, y, 2, k_black);
        alwan__ebu_patch(row, w, h, y, 5, k_black);
        alwan__ebu_patch(row, w, h, y, 9, k_black);
        alwan__ebu_patch(row, w, h, y, 12, k_black);
        alwan__ebu_patch(row, w, h, y, 1, pattern == ALWAN_PATTERN_EBU_2 ? rgb : k_white);
        break;
    case ALWAN_PATTERN_EBU_3:
        alwan__fill_row(row, w, 0.0);
        alwan__ebu_patch(row, w, h, y, pr && pr->ebu_point ? pr->ebu_point : 1, k_white);
        break;
    case ALWAN_PATTERN_EBU_3_WINDOW: {
        /* A window of p % of the area keeps the picture's aspect: sqrt(p %) of each side. */
        double const side = ALWAN_SQRT((double)(pr && pr->ebu_area ? pr->ebu_area : 4) / 100.0);
        alwan__fill_row(row, w, 0.0);
        alwan__rect_row(row, w, y, (double)w * 0.5, (double)h * 0.5, side * (double)w * 0.5, side * (double)h * 0.5,
                        k_white);
        break;
    }
    case ALWAN_PATTERN_EBU_3_WHITE:
        alwan__fill_row(row, w, 1.0);
        break;
    case ALWAN_PATTERN_EBU_4:
        rgb[0] = rgb[1] = rgb[2] = alwan__ebu_level(k_ebu_steps[(pr && pr->ebu_step ? pr->ebu_step : 1) - 1]);
        alwan__fill_row(row, w, 0.0);
        alwan__ebu_patch(row, w, h, y, 1, rgb);
        break;
    case ALWAN_PATTERN_EBU_5:
        alwan__ebu_colour(rgb, k_ebu_colours[(pr && pr->ebu_colour ? pr->ebu_colour : 1) - 1]);
        alwan__fill_row(row, w, 0.0);
        alwan__ebu_patch(row, w, h, y, 1, rgb);
        break;
    case ALWAN_PATTERN_EBU_12_GREY:
        alwan__fill_row(row, w, alwan__ebu_level(502));
        break;
    default: /* ALWAN_PATTERN_EBU_3_BLACK */
        alwan__fill_row(row, w, 0.0);
        break;
    }
}

/* One row of a pattern, R'G'B' per sample. */
static void alwan__pattern_row(double *row, alwan_pattern pattern, alwan_pattern_params const *pr, size_t y, size_t w,
                               size_t h) {
    if (pattern >= ALWAN_PATTERN_EBU_1) {
        alwan__ebu_row(row, pattern, pr, y, w, h);
    } else if (pattern <= ALWAN_PATTERN_BARS_75_7_5_75_7_5) {
        static double const k_sig[4][4] = { { 1.0, 0.0, 1.0, 0.0 }, { 1.0, 0.0, 0.75, 0.0 },
                                            { 1.0, 0.0, 1.0, 0.25 }, { 0.75, 0.075, 0.75, 0.075 } };
        double const *s = k_sig[pattern];
        size_t x = 0;
        int bar;
        for (bar = 0; bar < 8; bar++) {
            size_t const end = alwan__edge((double)(bar + 1) / 8.0, w);
            double rgb[3];
            alwan__bt471_bar(rgb, bar, s[0], s[1], s[2], s[3]);
            for (; x < end && x < w; x++) {
                row[3 * x + 0] = rgb[0];
                row[3 * x + 1] = rgb[1];
                row[3 * x + 2] = rgb[2];
            }
        }
    } else {
        double const d = 1.0 / 8.0, c = 3.0 / 28.0;
        double const ramp_len = 876.0 / 1920.0 * (double)w;
        double const ramp_x0 = (double)w / 2.0 - ramp_len / 2.0;
        if (y < alwan__edge(7.0 / 12.0, h)) {
            double const e[9] = { d, d + c, d + 2 * c, d + 3 * c, d + 4 * c, d + 5 * c, d + 6 * c, d + 7 * c, 1.0 };
            int const lv[9] = { ALWAN__B28_GREY40, ALWAN__B28_W75, ALWAN__B28_Y75, ALWAN__B28_C75, ALWAN__B28_G75,
                                ALWAN__B28_M75, ALWAN__B28_R75, ALWAN__B28_B75, ALWAN__B28_GREY40 };
            alwan__stripes(row, w, e, lv, 9, 0.0, 1.0);
        } else if (y < alwan__edge(8.0 / 12.0, h)) {
            int const choice = pr ? (int)pr->b28_choice : 0;
            int const first = choice == ALWAN_PATTERN_B28_100_WHITE ? ALWAN__B28_W100
                            : choice == ALWAN_PATTERN_B28_PLUS_I     ? ALWAN__B28_PLUS_I
                                                                     : ALWAN__B28_W75;
            double const e[4] = { d, d + c, d + 7 * c, 1.0 };
            int const lv[4] = { ALWAN__B28_C100, first, ALWAN__B28_W75, ALWAN__B28_B100 };
            alwan__stripes(row, w, e, lv, 4, 0.0, 1.0);
        } else if (y < alwan__edge(9.0 / 12.0, h)) {
            double const e[3] = { d, d + 7 * c, 1.0 };
            int const lv[3] = { ALWAN__B28_Y100, ALWAN__B28_RAMP, ALWAN__B28_R100 };
            alwan__stripes(row, w, e, lv, 3, ramp_x0, ramp_len);
        } else {
            double const i = c / 3.0;
            double const e0 = d + 1.5 * c, e1 = e0 + 2.0 * c, e2 = e1 + 5.0 / 6.0 * c;
            double const e[11] = { d, e0, e1, e2, e2 + i, e2 + 2 * i, e2 + 3 * i, e2 + 4 * i, e2 + 5 * i, d + 7 * c, 1.0 };
            int const lv[11] = { ALWAN__B28_GREY15, ALWAN__B28_BLACK, ALWAN__B28_W100, ALWAN__B28_BLACK, ALWAN__B28_M2,
                                 ALWAN__B28_BLACK, ALWAN__B28_P2, ALWAN__B28_BLACK, ALWAN__B28_P4, ALWAN__B28_BLACK,
                                 ALWAN__B28_GREY15 };
            alwan__stripes(row, w, e, lv, 11, 0.0, 1.0);
        }
    }
}

/* ---------------------------------------------------------------- render */

static alwan_status alwan__pattern_check(alwan_pattern pattern, alwan_pattern_params const *params, size_t w, size_t h) {
    if ((int)pattern < 0 || (int)pattern >= (int)ALWAN_PATTERN_COUNT || w == 0 || h == 0) return ALWAN_E_INVALID;
    if (params) {
        unsigned const a = params->ebu_area;
        if ((int)params->b28_choice < 0 || (int)params->b28_choice > (int)ALWAN_PATTERN_B28_PLUS_I) {
            return ALWAN_E_INVALID;
        }
        if (params->ebu_point > 13 || params->ebu_step > 20 || params->ebu_colour > 18) return ALWAN_E_INVALID;
        if (a != 0 && a != 4 && a != 10 && a != 25 && a != 81) return ALWAN_E_INVALID;
    }
    return ALWAN_OK;
}

/* Shared by the four entry points: rows of doubles, stored as the caller asked. */
static alwan_status alwan__pattern_render(void *p0, void *p1, void *p2, size_t row_stride, int is_f32, int planar,
                                          size_t w, size_t h, alwan_pattern pattern, alwan_pattern_params const *params) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t y, x;
    double *row;
    alwan_status st = alwan__pattern_check(pattern, params, w, h);
    if (st != ALWAN_OK) return st;
    if (!p0 || (planar && (!p1 || !p2))) return ALWAN_E_INVALID;
    if (row_stride < (planar ? w * elem : w * 3 * elem)) return ALWAN_E_INVALID;
    row = (double *)ALWAN_ALLOC(alwan_safe_array_size(w * 3, sizeof(double)), sizeof(double));
    if (!row) return ALWAN_E_NOMEM;
    for (y = 0; y < h; y++) {
        alwan__pattern_row(row, pattern, params, y, w, h);
        for (x = 0; x < w; x++) {
            if (planar) {
                unsigned char *const r0 = (unsigned char *)p0 + y * row_stride;
                unsigned char *const r1 = (unsigned char *)p1 + y * row_stride;
                unsigned char *const r2 = (unsigned char *)p2 + y * row_stride;
                if (is_f32) {
                    ((alwan_f32 *)r0)[x] = (alwan_f32)row[3 * x + 0];
                    ((alwan_f32 *)r1)[x] = (alwan_f32)row[3 * x + 1];
                    ((alwan_f32 *)r2)[x] = (alwan_f32)row[3 * x + 2];
                } else {
                    ((alwan_f64 *)r0)[x] = row[3 * x + 0];
                    ((alwan_f64 *)r1)[x] = row[3 * x + 1];
                    ((alwan_f64 *)r2)[x] = row[3 * x + 2];
                }
            } else {
                unsigned char *const r = (unsigned char *)p0 + y * row_stride;
                if (is_f32) {
                    alwan_f32 *const o = (alwan_f32 *)r + 3 * x;
                    o[0] = (alwan_f32)row[3 * x + 0];
                    o[1] = (alwan_f32)row[3 * x + 1];
                    o[2] = (alwan_f32)row[3 * x + 2];
                } else {
                    alwan_f64 *const o = (alwan_f64 *)r + 3 * x;
                    o[0] = row[3 * x + 0];
                    o[1] = row[3 * x + 1];
                    o[2] = row[3 * x + 2];
                }
            }
        }
    }
    ALWAN_FREE(row);
    return ALWAN_OK;
}

alwan_status alwan_pattern_render_f32(alwan_f32 *rgb_out, size_t row_stride, size_t width, size_t height,
                                      alwan_pattern pattern, alwan_pattern_params const *params) {
    return alwan__pattern_render(rgb_out, NULL, NULL, row_stride, 1, 0, width, height, pattern, params);
}

alwan_status alwan_pattern_render_f64(alwan_f64 *rgb_out, size_t row_stride, size_t width, size_t height,
                                      alwan_pattern pattern, alwan_pattern_params const *params) {
    return alwan__pattern_render(rgb_out, NULL, NULL, row_stride, 0, 0, width, height, pattern, params);
}

alwan_status alwan_pattern_render_planar_f32(alwan_f32 *r_out, size_t row_stride, alwan_f32 *g_out, alwan_f32 *b_out,
                                             size_t width, size_t height, alwan_pattern pattern,
                                             alwan_pattern_params const *params) {
    return alwan__pattern_render(r_out, g_out, b_out, row_stride, 1, 1, width, height, pattern, params);
}

alwan_status alwan_pattern_render_planar_f64(alwan_f64 *r_out, size_t row_stride, alwan_f64 *g_out, alwan_f64 *b_out,
                                             size_t width, size_t height, alwan_pattern pattern,
                                             alwan_pattern_params const *params) {
    return alwan__pattern_render(r_out, g_out, b_out, row_stride, 0, 1, width, height, pattern, params);
}
