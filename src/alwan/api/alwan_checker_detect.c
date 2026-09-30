/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Colour checker detection by segmentation.
 *
 * A port of colour-checker-detection 0.2.3's segmentation method
 * (detect_colour_checkers_segmentation, segmenter_default, extractor_segmentation and the
 * helpers in detection/common.py). colour-checker-detection is Copyright 2018 Colour
 * Developers, BSD-3-Clause (https://github.com/colour-science/colour-checker-detection);
 * its notice is kept here as that licence asks. The OpenCV calls it makes are reproduced
 * from OpenCV 5.x's sources (Apache-2.0): resize (INTER_CUBIC, float), bilateralFilter
 * (8-bit, one channel), adaptiveThreshold (mean), erode and dilate, findContours
 * (Suzuki and Abe, the tree and external modes), approxPolyDP, arcLength, contourArea,
 * moments, matchShapes, convexHull, minAreaRect, boxPoints, pointPolygonTest, the filled
 * drawContours, getPerspectiveTransform and the bicubic warpPerspective.
 *
 * The pipeline, at the package's defaults:
 *   - the image, float32, rotated a quarter turn clockwise if it is taller than wide, is
 *     resized to 1440 wide by OpenCV's cubic resize;
 *   - segmentation: sRGB-encoded, the maximum channel, stretched to 0..255 and truncated to
 *     8 bits, five bilateral passes (sigma 5 and 5), a mean adaptive threshold (block 21,
 *     C 3), a 3 x 3 erosion then dilation, every contour; each contour brought to four
 *     points by bisecting approxPolyDP's tolerance; the ones between 1/4800 and 1/24 of the
 *     image and within 0.015 of a square by matchShapes (I2) become their minimum-area
 *     rectangles; nested ones keep the smallest; each is grown by 4/3 about its centroid and
 *     drawn filled, the outer contours of that drawing become cluster rectangles, kept when
 *     their aspect ratio is within 10% of 1.5 and they hold 12 to 36 swatch centroids;
 *   - extraction: each cluster is warped (bicubic) onto a 1440 x 960 chart, 32 x 32 windows
 *     at the 24 swatch centres are averaged, and of the four corner orders the one whose
 *     swatches are nearest (mean squared) the reference ColorChecker is kept.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* The orientation reference: colour-checker-detection's _COLOURCHECKER_VALUES. */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static double const ccd_reference_default[24 * 3] = {
#include "../data/colorchecker/classic_post2014_srgb_linear.csv"
};
ALWAN_DIAG_POP

/* ------------------------------------------------------------------------------------ */
/* Small helpers                                                                        */
/* ------------------------------------------------------------------------------------ */

typedef struct { int x, y; } ccd_pt;
typedef struct { float x, y; } ccd_ptf;

typedef struct {
    ccd_pt *p;
    int n, cap;
} ccd_poly;

static int ccd_poly_push(ccd_poly *c, int x, int y) {
    if (c->n == c->cap) {
        int ncap = c->cap ? c->cap * 2 : 16;
        ccd_pt *np = (ccd_pt *)ALWAN_ALLOC(alwan_safe_array_size((size_t)ncap, sizeof(ccd_pt)), 16);
        if (!np) return 0;
        if (c->n) memcpy(np, c->p, (size_t)c->n * sizeof(ccd_pt));
        if (c->p) ALWAN_FREE(c->p);
        c->p = np;
        c->cap = ncap;
    }
    c->p[c->n].x = x;
    c->p[c->n].y = y;
    c->n++;
    return 1;
}

static void ccd_poly_free(ccd_poly *c) {
    if (c->p) ALWAN_FREE(c->p);
    c->p = NULL;
    c->n = c->cap = 0;
}

/* A list of polygons, each owning its points. */
typedef struct {
    ccd_poly *v;
    int n, cap;
} ccd_polys;

static ccd_poly *ccd_polys_add(ccd_polys *l) {
    if (l->n == l->cap) {
        int ncap = l->cap ? l->cap * 2 : 64;
        ccd_poly *nv = (ccd_poly *)ALWAN_ALLOC(alwan_safe_array_size((size_t)ncap, sizeof(ccd_poly)), 16);
        if (!nv) return NULL;
        if (l->n) memcpy(nv, l->v, (size_t)l->n * sizeof(ccd_poly));
        if (l->v) ALWAN_FREE(l->v);
        l->v = nv;
        l->cap = ncap;
    }
    memset(&l->v[l->n], 0, sizeof(ccd_poly));
    return &l->v[l->n++];
}

static void ccd_polys_free(ccd_polys *l) {
    int i;
    for (i = 0; i < l->n; i++) ccd_poly_free(&l->v[i]);
    if (l->v) ALWAN_FREE(l->v);
    l->v = NULL;
    l->n = l->cap = 0;
}

/* cvRound: to nearest, ties to even (lrint in the default rounding mode). */
static int ccd_round(double v) {
    double r = ALWAN_FLOOR(v + 0.5);
    if (r - v == 0.5 && ALWAN_FMOD(r, 2.0) != 0.0) r -= 1.0;
    if (!(r > (double)INT_MIN && r < (double)INT_MAX)) return 0;
    return (int)r;
}

static int ccd_floor(double v) {
    double r = ALWAN_FLOOR(v);
    if (!(r > (double)INT_MIN && r < (double)INT_MAX)) return 0;
    return (int)r;
}

/* numpy's float to int32 cast: toward zero; out of range or NaN read as 0 here. */
static int ccd_trunc(double v) {
    double r = ALWAN_TRUNC(v);
    if (!(r > (double)INT_MIN && r < (double)INT_MAX)) return 0;
    return (int)r;
}

static int ccd_clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* ------------------------------------------------------------------------------------ */
/* cv::resize, INTER_CUBIC, float32, n channels                                           */
/* ------------------------------------------------------------------------------------ */

static void ccd_cubic_coeffs(float x, float *c) {
    float const A = -0.75f;
    c[0] = ((A * (x + 1) - 5 * A) * (x + 1) + 8 * A) * (x + 1) - 4 * A;
    c[1] = ((A + 2) * x - (A + 3)) * x * x + 1;
    c[2] = ((A + 2) * (1 - x) - (A + 3)) * (1 - x) * (1 - x) + 1;
    c[3] = 1.f - c[0] - c[1] - c[2];
}

#define CCD_RESIZE_LANES 4

static int ccd_resize_cubic(float *dst, int dw, int dh, float const *src, int sw, int sh, int cn) {
    double inv_sx = (double)dw / sw, inv_sy = (double)dh / sh;
    double scale_x = 1. / inv_sx, scale_y = 1. / inv_sy;
    int width = dw * cn, swidth = sw * cn;
    int xmin = 0, xmax = dw, dx, dy, k;
    int *xofs = NULL, *yofs = NULL;
    float *alpha = NULL, *beta = NULL, *rows = NULL;
    size_t nalpha = alwan_safe_array_size((size_t)width, 4 * sizeof(float));
    if (dw == sw && dh == sh) {
        memcpy(dst, src, (size_t)sw * (size_t)sh * (size_t)cn * sizeof(float));
        return 1;
    }
    xofs = (int *)ALWAN_ALLOC(alwan_safe_array_size((size_t)width, sizeof(int)), 16);
    yofs = (int *)ALWAN_ALLOC(alwan_safe_array_size((size_t)dh, sizeof(int)), 16);
    alpha = (float *)ALWAN_ALLOC(nalpha, 16);
    beta = (float *)ALWAN_ALLOC(alwan_safe_array_size((size_t)dh, 4 * sizeof(float)), 16);
    rows = (float *)ALWAN_ALLOC(alwan_safe_array_size((size_t)width, 4 * sizeof(float)), 16);
    if (!xofs || !yofs || !alpha || !beta || !rows) {
        if (xofs) ALWAN_FREE(xofs);
        if (yofs) ALWAN_FREE(yofs);
        if (alpha) ALWAN_FREE(alpha);
        if (beta) ALWAN_FREE(beta);
        if (rows) ALWAN_FREE(rows);
        return 0;
    }
    for (dx = 0; dx < dw; dx++) {
        float fx = (float)((dx + 0.5) * scale_x - 0.5);
        int sx = ccd_floor(fx);
        float cb[4];
        fx -= (float)sx;
        if (sx < 1) xmin = dx + 1;
        if (sx + 2 >= sw && dx < xmax) xmax = dx;
        ccd_cubic_coeffs(fx, cb);
        for (k = 0; k < cn; k++) {
            int j;
            xofs[dx * cn + k] = sx * cn + k;
            for (j = 0; j < 4; j++) alpha[(dx * cn + k) * 4 + j] = cb[j];
        }
    }
    for (dy = 0; dy < dh; dy++) {
        float fy = (float)((dy + 0.5) * scale_y - 0.5);
        int sy = ccd_floor(fy);
        fy -= (float)sy;
        yofs[dy] = sy;
        ccd_cubic_coeffs(fy, beta + dy * 4);
    }
    xmin *= cn;
    xmax *= cn;
    for (dy = 0; dy < dh; dy++) {
        float const *b = beta + dy * 4;
        float *out = dst + (size_t)dy * (size_t)width;
        int x;
        for (k = 0; k < 4; k++) {
            int sy = yofs[dy] - 1 + k;
            float const *S;
            float *D = rows + (size_t)k * (size_t)width;
            int limit = xmin;
            sy = sy < 0 ? 0 : (sy >= sh ? sh - 1 : sy);
            S = src + (size_t)sy * (size_t)swidth;
            dx = 0;
            for (;;) {
                for (; dx < limit; dx++) {
                    int j, sx = xofs[dx] - cn;
                    float v = 0;
                    for (j = 0; j < 4; j++) {
                        int sxj = sx + j * cn;
                        if ((unsigned)sxj >= (unsigned)swidth) {
                            while (sxj < 0) sxj += cn;
                            while (sxj >= swidth) sxj -= cn;
                        }
                        v += S[sxj] * alpha[dx * 4 + j];
                    }
                    D[dx] = v;
                }
                if (limit == width) break;
                for (; dx < xmax; dx++) {
                    int sx = xofs[dx] - cn;
                    float const *a = alpha + dx * 4;
                    float v = S[sx] * a[0];
                    v += S[sx + cn] * a[1];
                    v += S[sx + 2 * cn] * a[2];
                    v += S[sx + 3 * cn] * a[3];
                    D[dx] = v;
                }
                limit = width;
            }
        }
        /* the vertical pass: OpenCV's vector chain S0 b0 + (S1 b1 + (S2 b2 + S3 b3)), unfused
         * at its SSE3 baseline, over whole vectors, then a left to right tail */
        {
            int xv = width - width % CCD_RESIZE_LANES;
            for (x = 0; x < xv; x++) {
                float t = rows[3 * (size_t)width + x] * b[3];
                t = rows[2 * (size_t)width + x] * b[2] + t;
                t = rows[1 * (size_t)width + x] * b[1] + t;
                t = rows[x] * b[0] + t;
                out[x] = t;
            }
            for (x = xv; x < width; x++) {
                out[x] = rows[x] * b[0] + rows[(size_t)width + x] * b[1] + rows[2 * (size_t)width + x] * b[2] +
                         rows[3 * (size_t)width + x] * b[3];
            }
        }
    }
    ALWAN_FREE(xofs);
    ALWAN_FREE(yofs);
    ALWAN_FREE(alpha);
    ALWAN_FREE(beta);
    ALWAN_FREE(rows);
    return 1;
}

/* ------------------------------------------------------------------------------------ */
/* Segmentation filters on 8-bit images                                                  */
/* ------------------------------------------------------------------------------------ */

static int ccd_reflect101(int p, int len) {
    if (len == 1) return 0;
    while (p < 0 || p >= len) {
        if (p < 0) p = -p;
        if (p >= len) p = 2 * len - 2 - p;
    }
    return p;
}

/* OpenCV's v_exp_default_32f (a Cephes polynomial), at its SSE3 baseline where v_fma is a
 * multiply then an add. Below about -87 it returns 0, not a subnormal. */
static float ccd_v_exp(float x0) {
    float x = x0, e, xx, y, scale;
    int mm;
    unsigned int bits;
    if (x < -88.3762626647949f) x = -88.3762626647949f;
    if (x > 89.f) x = 89.f;
    e = x * 1.44269504088896341f + 0.5f;
    mm = ccd_floor(e);
    e = (float)mm;
    bits = (unsigned int)(mm + 0x7f) << 23;
    memcpy(&scale, &bits, sizeof(scale));
    x = e * -6.93359375E-1f + x;
    x = e * 2.12194440E-4f + x;
    xx = x * x;
    y = x * 1.9875691500E-4f + 1.3981999507E-3f;
    y = y * x + 8.3334519073E-3f;
    y = y * x + 4.1665795894E-2f;
    y = y * x + 1.6666665459E-1f;
    y = y * x + 5.0000001201E-1f;
    y = y * xx + x;
    y = y + 1.f;
    return y * scale;
}

/* cv::bilateralFilter on one 8-bit channel, d = -1 (radius cvRound(1.5 sigma_space)),
 * BORDER_REFLECT_101. The colour weights come from v_exp in fours below 252 and from expf
 * above; the filter runs in AVX2 blocks of 32 pixels with fused sums, and a scalar tail
 * with unfused ones, as OpenCV's dispatched kernel does. */
static int ccd_bilateral(unsigned char *dst, unsigned char const *src, int w, int h, double sigma_color,
                         double sigma_space) {
    float gcc = (float)(-0.5 / (sigma_color * sigma_color));
    float gsc = (float)(-0.5 / (sigma_space * sigma_space));
    int radius = ccd_round(sigma_space * 1.5), i, j, maxk = 0, x, y;
    int d, tw;
    float color_weight[256];
    float *space_weight;
    int *space_dx, *space_dy;
    unsigned char *tmp;
    if (radius < 1) radius = 1;
    d = radius * 2 + 1;
    tw = w + 2 * radius;
    space_weight = (float *)ALWAN_ALLOC(alwan_safe_array_size((size_t)d * (size_t)d, sizeof(float)), 16);
    space_dx = (int *)ALWAN_ALLOC(alwan_safe_array_size((size_t)d * (size_t)d, sizeof(int)), 16);
    space_dy = (int *)ALWAN_ALLOC(alwan_safe_array_size((size_t)d * (size_t)d, sizeof(int)), 16);
    tmp = (unsigned char *)ALWAN_ALLOC(alwan_safe_array_size((size_t)tw, (size_t)(h + 2 * radius)), 16);
    if (!space_weight || !space_dx || !space_dy || !tmp) {
        if (space_weight) ALWAN_FREE(space_weight);
        if (space_dx) ALWAN_FREE(space_dx);
        if (space_dy) ALWAN_FREE(space_dy);
        if (tmp) ALWAN_FREE(tmp);
        return 0;
    }
    for (i = 0; i < 256 - 4; i++) color_weight[i] = ccd_v_exp(((float)i * (float)i) * gcc);
    for (; i < 256; i++) color_weight[i] = (float)ALWAN_EXP((double)((float)(i * i) * gcc));
    for (i = -radius; i <= radius; i++) {
        for (j = -radius; j <= radius; j++) {
            double r = ALWAN_SQRT((double)i * i + (double)j * j);
            if (r > radius) continue;
            space_weight[maxk] = (float)ALWAN_EXP(r * r * gsc);
            space_dy[maxk] = i;
            space_dx[maxk++] = j;
        }
    }
    for (y = 0; y < h + 2 * radius; y++) {
        int sy = ccd_reflect101(y - radius, h);
        for (x = 0; x < tw; x++) tmp[(size_t)y * tw + x] = src[(size_t)sy * w + ccd_reflect101(x - radius, w)];
    }
    for (y = 0; y < h; y++) {
        int xv = w - w % 32;
        for (x = 0; x < w; x++) {
            unsigned char const *c = tmp + (size_t)(y + radius) * tw + (x + radius);
            int v0 = *c, k;
            float wsum = 0.f, sum = 0.f;
            if (x < xv) {
                for (k = 0; k < maxk; k++) {
                    int v = c[space_dy[k] * tw + space_dx[k]];
                    int ad = v > v0 ? v - v0 : v0 - v;
                    float wk = space_weight[k] * color_weight[ad];
                    wsum += wk;
                    sum = ALWAN_FMAF((float)v, wk, sum);
                }
            } else {
                for (k = 0; k < maxk; k++) {
                    int v = c[space_dy[k] * tw + space_dx[k]];
                    int ad = v > v0 ? v - v0 : v0 - v;
                    float wk = space_weight[k] * color_weight[ad];
                    wsum += wk;
                    sum += (float)v * wk;
                }
            }
            dst[(size_t)y * w + x] = (unsigned char)ccd_clampi(ccd_round((double)(sum / wsum)), 0, 255);
        }
    }
    ALWAN_FREE(space_weight);
    ALWAN_FREE(space_dx);
    ALWAN_FREE(space_dy);
    ALWAN_FREE(tmp);
    return 1;
}

/* cv::adaptiveThreshold, ADAPTIVE_THRESH_MEAN_C, THRESH_BINARY: the block mean by a
 * normalised box filter (BORDER_REPLICATE), rounded to 8 bits, then src > mean - C. */
static int ccd_adaptive_threshold(unsigned char *dst, unsigned char const *src, int w, int h, int block, double C,
                                  unsigned char maxval) {
    int r = block / 2, x, y, idelta = (int)ALWAN_CEIL(C);
    int *col = (int *)ALWAN_ALLOC(alwan_safe_array_size((size_t)w, sizeof(int)), 16);
    float scale = (float)(1.0 / ((double)block * block));
    if (!col) return 0;
    for (y = 0; y < h; y++) {
        int k;
        memset(col, 0, (size_t)w * sizeof(int));
        for (k = -r; k <= r; k++) {
            unsigned char const *row = src + (size_t)ccd_clampi(y + k, 0, h - 1) * w;
            for (x = 0; x < w; x++) col[x] += row[x];
        }
        for (x = 0; x < w; x++) {
            int s = 0, mean;
            for (k = -r; k <= r; k++) s += col[ccd_clampi(x + k, 0, w - 1)];
            mean = ccd_clampi(ccd_round((double)((float)s * scale)), 0, 255);
            dst[(size_t)y * w + x] = (unsigned char)(((int)src[(size_t)y * w + x] - mean > -idelta) ? maxval : 0);
        }
    }
    ALWAN_FREE(col);
    return 1;
}

/* 3 x 3 erosion (minimum) or dilation (maximum); the border ignored, as OpenCV's
 * morphologyDefaultBorderValue makes it. */
static void ccd_morph3(unsigned char *dst, unsigned char const *src, int w, int h, int dilate) {
    int x, y, i, j;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int v = dilate ? 0 : 255;
            for (i = -1; i <= 1; i++) {
                int yy = y + i;
                if (yy < 0 || yy >= h) continue;
                for (j = -1; j <= 1; j++) {
                    int xx = x + j, s;
                    if (xx < 0 || xx >= w) continue;
                    s = src[(size_t)yy * w + xx];
                    if (dilate ? s > v : s < v) v = s;
                }
            }
            dst[(size_t)y * w + x] = (unsigned char)v;
        }
    }
}

/* ------------------------------------------------------------------------------------ */
/* cv::findContours (OpenCV 5.x contours_new.cpp), CHAIN_APPROX_NONE                     */
/* ------------------------------------------------------------------------------------ */

typedef struct {
    int parent, first_child, prev, next, ctable_next;
    int is_hole;
    int bx, by, bw, bh; /* brect */
    int ox, oy;         /* origin */
    int pstart, pcount; /* points in the scanner's pool */
} ccd_cnode;

typedef struct {
    signed char *img;
    int W, H; /* padded */
    int tree_mode;
    ccd_cnode *nodes;
    int nn, ncap;
    ccd_pt *pool;
    int np, pcap;
    int ctable[128];
    int ptx, pty, lnx, lny;
    signed char nbd;
    int oom;
} ccd_scanner;

static int const ccd_dx8[8] = {1, 1, 0, -1, -1, -1, 0, 1};
static int const ccd_dy8[8] = {0, -1, -1, -1, 0, 1, 1, 1};

static int ccd_delta(ccd_scanner const *s, int dir) {
    int d = dir & 7;
    return ccd_dx8[d] + ccd_dy8[d] * s->W;
}

static int ccd_scan_new_node(ccd_scanner *s) {
    ccd_cnode *n;
    if (s->nn == s->ncap) {
        int ncap = s->ncap ? s->ncap * 2 : 256;
        ccd_cnode *nv = (ccd_cnode *)ALWAN_ALLOC(alwan_safe_array_size((size_t)ncap, sizeof(ccd_cnode)), 16);
        if (!nv) {
            s->oom = 1;
            return -1;
        }
        if (s->nn) memcpy(nv, s->nodes, (size_t)s->nn * sizeof(ccd_cnode));
        if (s->nodes) ALWAN_FREE(s->nodes);
        s->nodes = nv;
        s->ncap = ncap;
    }
    n = &s->nodes[s->nn];
    memset(n, 0, sizeof(*n));
    n->parent = n->first_child = n->prev = n->next = n->ctable_next = -1;
    n->pstart = s->np;
    return s->nn++;
}

static void ccd_scan_add_point(ccd_scanner *s, int node, int x, int y) {
    if (s->np == s->pcap) {
        int ncap = s->pcap ? s->pcap * 2 : 4096;
        ccd_pt *nv = (ccd_pt *)ALWAN_ALLOC(alwan_safe_array_size((size_t)ncap, sizeof(ccd_pt)), 16);
        if (!nv) {
            s->oom = 1;
            return;
        }
        if (s->np) memcpy(nv, s->pool, (size_t)s->np * sizeof(ccd_pt));
        if (s->pool) ALWAN_FREE(s->pool);
        s->pool = nv;
        s->pcap = ncap;
    }
    s->pool[s->np].x = x;
    s->pool[s->np].y = y;
    s->np++;
    s->nodes[node].pcount++;
}

/* icvFetchContourEx<schar>, CHAIN_APPROX_NONE (isDirect) */
static void ccd_fetch_contour(ccd_scanner *s, int node, int sx, int sy, signed char nbd) {
    signed char *img = s->img;
    int i0 = sy * s->W + sx, i1 = 0, i3, i4 = 0;
    ccd_cnode *n = &s->nodes[node];
    int ptx = n->ox, pty = n->oy;
    int rx = ptx, ry = pty, rw = ptx, rh = pty;
    int s_end = n->is_hole ? 0 : 4, dir = s_end;
    do {
        dir = (dir - 1) & 7;
        i1 = i0 + ccd_delta(s, dir);
    } while (img[i1] == 0 && dir != s_end);
    if (dir == s_end) {
        img[i0] = (signed char)(nbd | 0x80);
        ccd_scan_add_point(s, node, ptx, pty);
    } else {
        int prev_s = dir ^ 4;
        i3 = i0;
        for (;;) {
            s_end = dir;
            if (dir > 15) dir = 15;
            while (dir < 15) {
                ++dir;
                i4 = i3 + ccd_delta(s, dir);
                if (img[i4] != 0) break;
            }
            dir &= 7;
            if ((unsigned)(dir - 1) < (unsigned)s_end) {
                img[i3] = (signed char)(nbd | 0x80);
            } else if (img[i3] == 1) {
                img[i3] = nbd;
            }
            ccd_scan_add_point(s, node, ptx, pty);
            if (dir != prev_s) {
                if (ptx < rx) rx = ptx;
                else if (ptx > rw) rw = ptx;
                if (pty < ry) ry = pty;
                else if (pty > rh) rh = pty;
            }
            prev_s = dir;
            ptx += ccd_dx8[dir];
            pty += ccd_dy8[dir];
            if (i4 == i0 && i3 == i1) break;
            i3 = i4;
            dir = (dir + 4) & 7;
        }
    }
    n = &s->nodes[node];
    n->bx = rx;
    n->by = ry;
    n->bw = rw - (rx - 1);
    n->bh = rh - (ry - 1);
}

/* icvTraceContour<schar> */
static int ccd_trace_contour(ccd_scanner *s, int sx, int sy, int ex, int ey, int is_hole) {
    signed char const *img = s->img;
    int stop = ey * s->W + ex, i0 = sy * s->W + sx, i1 = 0, i3, i4 = 0;
    int s_end = is_hole ? 0 : 4, dir = s_end;
    do {
        dir = (dir - 1) & 7;
        i1 = i0 + ccd_delta(s, dir);
    } while (img[i1] == 0 && dir != s_end);
    i3 = i0;
    if (dir != s_end) {
        for (;;) {
            if (dir > 15) dir = 15;
            while (dir < 15) {
                ++dir;
                i4 = i3 + ccd_delta(s, dir);
                if (img[i4] != 0) break;
            }
            if (i3 == stop) {
                int t = dir;
                if ((img[i3] & 0x80) == 0) return 1;
                for (;;) {
                    t = (t - 1) & 7;
                    if (img[i3 + ccd_delta(s, t)] != 0) break;
                    if (t == 0) return 1;
                }
            }
            if (i4 == i0 && i3 == i1) break;
            i3 = i4;
            dir = (dir + 4) & 7;
        }
    } else {
        return i3 == stop;
    }
    return 0;
}

static int ccd_first_bounding(ccd_scanner *s, int lx, int ly, int y, int lval, int par) {
    int res = par, cur = s->ctable[lval];
    while (cur != -1) {
        ccd_cnode const *c = &s->nodes[cur];
        if ((lx - c->bx) < c->bw && (ly - c->by) < c->bh) {
            if (res != -1) {
                ccd_cnode const *r = &s->nodes[res];
                if (ccd_trace_contour(s, r->ox, r->oy, lx, y, r->is_hole)) break;
            }
            res = cur;
        }
        cur = c->ctable_next;
    }
    return res;
}

static void ccd_add_child(ccd_scanner *s, int parent, int child) {
    ccd_cnode *p = &s->nodes[parent], *c = &s->nodes[child];
    if (p->first_child != -1) {
        s->nodes[p->first_child].prev = child;
        c->next = p->first_child;
    }
    p->first_child = child;
    c->parent = parent;
    c->prev = -1;
}

static int ccd_contour_scan(ccd_scanner *s, int prev, int p, int *lx, int *ly, int x, int y) {
    int is_hole = 0, main_parent, node, sx;
    signed char lval_nbd;
    if (!(prev == 0 && p == 1)) {
        if (p != 0 || prev < 1) return 0;
        if (prev & -2) *lx = x - 1; /* MASK8_FLAGS, (schar)0xFE */
        is_hole = 1;
    }
    if (!s->tree_mode && (is_hole || s->img[*ly * s->W + *lx] > 0)) return 0;
    if (!s->tree_mode || *lx <= 0) {
        main_parent = 0;
    } else {
        int lval = s->img[*ly * s->W + *lx] & 0x7F;
        main_parent = ccd_first_bounding(s, *lx, *ly, y, lval, -1);
        if (main_parent < 0) main_parent = 0;
        if (s->nodes[main_parent].is_hole == is_hole) {
            main_parent = s->nodes[main_parent].parent != -1 ? s->nodes[main_parent].parent : 0;
        }
    }
    *lx = x - (is_hole ? 1 : 0);
    node = ccd_scan_new_node(s);
    if (node < 0) return 0;
    sx = x - (is_hole ? 1 : 0);
    s->nodes[node].is_hole = is_hole;
    s->nodes[node].ox = sx - 1;
    s->nodes[node].oy = y - 1;
    if (!s->tree_mode) {
        ccd_fetch_contour(s, node, sx, y, 2);
    } else {
        lval_nbd = s->nbd;
        s->nbd = (signed char)((s->nbd + 1) & 0x7F);
        if (s->nbd == 0) s->nbd = 3;
        ccd_fetch_contour(s, node, sx, y, lval_nbd);
        s->nodes[node].bx += 1;
        s->nodes[node].by += 1;
        s->nodes[node].ctable_next = s->ctable[(int)lval_nbd];
        s->ctable[(int)lval_nbd] = node;
    }
    s->nodes[node].ox = sx;
    s->nodes[node].oy = y;
    ccd_add_child(s, main_parent, node);
    s->ptx = x + 1;
    s->pty = y;
    return 1;
}

static int ccd_find_next(ccd_scanner *s) {
    int x = s->ptx, y = s->pty, width = s->W - 1, height = s->H - 1;
    int lx = s->lnx, ly = s->lny;
    int prev = s->img[y * s->W + x - 1];
    for (; y < height; y++) {
        int p = 0;
        for (; x < width; x++) {
            for (; x < width && (p = s->img[y * s->W + x]) == prev; x++) {
            }
            if (x >= width) break;
            if (ccd_contour_scan(s, prev, p, &lx, &ly, x, y)) {
                s->lnx = lx;
                s->lny = ly;
                return 1;
            }
            if (s->oom) return 0;
            prev = p;
            if (prev & -2) lx = x;
        }
        lx = 0;
        ly = y + 1;
        x = 1;
        prev = 0;
    }
    return 0;
}

/* Contours of a binary 8-bit image, in OpenCV's output order. tree_mode 1 is RETR_TREE,
 * 0 is RETR_EXTERNAL. Returns 0 when memory runs out. */
static int ccd_find_contours(ccd_polys *out, unsigned char const *bin, int w, int h, int tree_mode) {
    ccd_scanner s;
    int x, y, ok = 1;
    int *stack = NULL, sp = 0;
    memset(&s, 0, sizeof(s));
    s.W = w + 2;
    s.H = h + 2;
    s.tree_mode = tree_mode;
    s.img = (signed char *)ALWAN_ALLOC(alwan_safe_array_size((size_t)s.W, (size_t)s.H), 16);
    if (!s.img) return 0;
    memset(s.img, 0, (size_t)s.W * (size_t)s.H);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) s.img[(y + 1) * s.W + x + 1] = (signed char)(bin[(size_t)y * w + x] > 0 ? 1 : 0);
    for (x = 0; x < 128; x++) s.ctable[x] = -1;
    s.ptx = 1;
    s.pty = 1;
    s.lnx = 0;
    s.lny = 1;
    s.nbd = 2;
    if (ccd_scan_new_node(&s) != 0) {
        ALWAN_FREE(s.img);
        return 0;
    }
    s.nodes[0].is_hole = 1;
    s.nodes[0].bx = 0;
    s.nodes[0].by = 0;
    s.nodes[0].bw = s.W;
    s.nodes[0].bh = s.H;
    while (ccd_find_next(&s)) {
    }
    if (s.oom) ok = 0;
    /* depth first, a node's most recent child first */
    if (ok) {
        stack = (int *)ALWAN_ALLOC(alwan_safe_array_size((size_t)s.nn + 1, sizeof(int)), 16);
        if (!stack) ok = 0;
    }
    if (ok) {
        stack[sp++] = 0;
        while (sp > 0 && ok) {
            int idx = stack[--sp], cur;
            ccd_cnode const *nd = &s.nodes[idx];
            cur = nd->first_child;
            if (cur != -1) {
                while (s.nodes[cur].next != -1) cur = s.nodes[cur].next;
            }
            while (cur != -1) {
                stack[sp++] = cur;
                cur = s.nodes[cur].prev;
            }
            if (idx != 0) {
                ccd_poly *c = ccd_polys_add(out);
                int i;
                if (!c) {
                    ok = 0;
                    break;
                }
                for (i = 0; i < nd->pcount; i++) {
                    if (!ccd_poly_push(c, s.pool[nd->pstart + i].x, s.pool[nd->pstart + i].y)) {
                        ok = 0;
                        break;
                    }
                }
            }
        }
    }
    if (stack) ALWAN_FREE(stack);
    if (s.nodes) ALWAN_FREE(s.nodes);
    if (s.pool) ALWAN_FREE(s.pool);
    ALWAN_FREE(s.img);
    return ok;
}

/* ------------------------------------------------------------------------------------ */
/* Shape descriptors                                                                    */
/* ------------------------------------------------------------------------------------ */

static double ccd_contour_area(ccd_pt const *p, int n) {
    double a00 = 0;
    float px, py;
    int i;
    if (n == 0) return 0.;
    px = (float)p[n - 1].x;
    py = (float)p[n - 1].y;
    for (i = 0; i < n; i++) {
        float cx = (float)p[i].x, cy = (float)p[i].y;
        a00 += (double)px * cy - (double)py * cx;
        px = cx;
        py = cy;
    }
    a00 *= 0.5;
    return ALWAN_ABS(a00);
}

static double ccd_arc_length_closed(ccd_pt const *p, int n) {
    double per = 0;
    float px, py;
    int i;
    if (n <= 1) return 0.;
    px = (float)p[n - 1].x;
    py = (float)p[n - 1].y;
    for (i = 0; i < n; i++) {
        float cx = (float)p[i].x, cy = (float)p[i].y;
        float dx = cx - px, dy = cy - py;
        per += (double)(float)ALWAN_SQRT((double)(dx * dx + dy * dy));
        px = cx;
        py = cy;
    }
    return per;
}

/* approxPolyDP_<int>, closed; returns the number of points written to dst. */
static int ccd_approx_poly_dp(ccd_pt const *src, int count0, ccd_pt *dst, double eps, int *stack_lo, int *stack_hi) {
    int init_iters = 3, i, j, pos = 0, wpos, count = count0, new_count = 0, top = 0;
    int slice_s = 0, slice_e = 0, rs_s = 0, rs_e = 0;
    ccd_pt start_pt, end_pt, pt;
    int le_eps = 0;
    if (count == 0) return 0;
    start_pt.x = start_pt.y = -1000000;
    eps *= eps;
    rs_s = 0;
    for (i = 0; i < init_iters; i++) {
        double max_dist = 0;
        pos = (pos + rs_s) % count;
        start_pt = src[pos];
        if (++pos >= count) pos = 0;
        for (j = 1; j < count; j++) {
            double ddx, ddy, dist;
            pt = src[pos];
            if (++pos >= count) pos = 0;
            ddx = pt.x - start_pt.x;
            ddy = pt.y - start_pt.y;
            dist = ddx * ddx + ddy * ddy;
            if (dist > max_dist) {
                max_dist = dist;
                rs_s = j;
            }
        }
        le_eps = max_dist <= eps;
    }
    if (!le_eps) {
        rs_e = slice_s = pos % count;
        slice_e = rs_s = (rs_s + slice_s) % count;
        stack_lo[top] = rs_s;
        stack_hi[top++] = rs_e;
        stack_lo[top] = slice_s;
        stack_hi[top++] = slice_e;
    } else {
        dst[new_count++] = start_pt;
    }
    while (top > 0) {
        --top;
        slice_s = stack_lo[top];
        slice_e = stack_hi[top];
        end_pt = src[slice_e];
        pos = slice_s;
        start_pt = src[pos];
        if (++pos >= count) pos = 0;
        if (pos != slice_e) {
            double dx = end_pt.x - start_pt.x, dy = end_pt.y - start_pt.y, maxd = 0;
            double seg2 = dx * dx + dy * dy;
            while (pos != slice_e) {
                double proj, d2;
                pt = src[pos];
                if (++pos >= count) pos = 0;
                proj = (double)((pt.x - start_pt.x) * dx + (pt.y - start_pt.y) * dy);
                if (proj < 0) {
                    d2 = (double)((pt.x - start_pt.x) * (pt.x - start_pt.x) + (pt.y - start_pt.y) * (pt.y - start_pt.y)) *
                         seg2;
                } else if (proj > seg2) {
                    d2 = (double)((pt.x - end_pt.x) * (pt.x - end_pt.x) + (pt.y - end_pt.y) * (pt.y - end_pt.y)) * seg2;
                } else {
                    double dist = ((pt.y - start_pt.y) * dx - (pt.x - start_pt.x) * dy);
                    d2 = dist * dist;
                }
                if (d2 > maxd) {
                    maxd = d2;
                    rs_s = (pos + count - 1) % count;
                }
            }
            le_eps = maxd <= eps * seg2;
        } else {
            le_eps = 1;
            start_pt = src[slice_s];
        }
        if (le_eps) {
            dst[new_count++] = start_pt;
        } else {
            rs_e = slice_e;
            slice_e = rs_s;
            stack_lo[top] = rs_s;
            stack_hi[top++] = rs_e;
            stack_lo[top] = slice_s;
            stack_hi[top++] = slice_e;
        }
    }
    /* clean-up of points on almost straight lines, closed */
    count = new_count;
    pos = count - 1;
    start_pt = dst[pos];
    if (++pos >= count) pos = 0;
    wpos = pos;
    pt = dst[pos];
    if (++pos >= count) pos = 0;
    for (i = 0; i < count && new_count > 2; i++) {
        double dx, dy, dist, sip;
        end_pt = dst[pos];
        if (++pos >= count) pos = 0;
        dx = end_pt.x - start_pt.x;
        dy = end_pt.y - start_pt.y;
        dist = ALWAN_ABS((double)(pt.x - start_pt.x) * dy - (double)(pt.y - start_pt.y) * dx);
        sip = (double)((pt.x - start_pt.x) * (end_pt.x - pt.x) + (pt.y - start_pt.y) * (end_pt.y - pt.y));
        if (dist * dist <= 0.5 * eps * (dx * dx + dy * dy) && dx != 0 && dy != 0 && sip >= 0) {
            new_count--;
            dst[wpos] = start_pt = end_pt;
            if (++wpos >= count) wpos = 0;
            pt = dst[pos];
            if (++pos >= count) pos = 0;
            i++;
            continue;
        }
        dst[wpos] = start_pt = pt;
        if (++wpos >= count) wpos = 0;
        pt = end_pt;
    }
    return new_count;
}

/* colour-checker-detection's approximate_contour: bisect approxPolyDP's tolerance, as a
 * fraction of the perimeter, until the polygon has 4 points; the contour itself after 100
 * tries. Writes into out (capacity n). */
static int ccd_quadrilateralise(ccd_pt const *c, int n, ccd_pt *out, ccd_pt *work, int *slo, int *shi) {
    double low = 0, high = 1, arc = ccd_arc_length_closed(c, n);
    int i;
    for (i = 1; i <= 100; i++) {
        double center = (low + high) / 2;
        int m = ccd_approx_poly_dp(c, n, work, center * arc, slo, shi);
        if (m > 4) {
            low = (low + high) / 2;
        } else if (m < 4) {
            high = (low + high) / 2;
        } else {
            memcpy(out, work, 4 * sizeof(ccd_pt));
            return 4;
        }
    }
    memcpy(out, c, (size_t)n * sizeof(ccd_pt));
    return n;
}

typedef struct {
    double m00, m10, m01, m20, m11, m02, m30, m21, m12, m03;
    double mu20, mu11, mu02, mu30, mu21, mu12, mu03;
    double nu20, nu11, nu02, nu30, nu21, nu12, nu03;
} ccd_moments;

static void ccd_complete_moments(ccd_moments *m) {
    double cx = 0, cy = 0, mu20, mu11, mu02, inv_m00 = 0.0, inv_sqrt, s2, s3;
    if (ALWAN_ABS(m->m00) > DBL_EPSILON) {
        inv_m00 = 1. / m->m00;
        cx = m->m10 * inv_m00;
        cy = m->m01 * inv_m00;
    }
    mu20 = m->m20 - m->m10 * cx;
    mu11 = m->m11 - m->m10 * cy;
    mu02 = m->m02 - m->m01 * cy;
    m->mu20 = mu20;
    m->mu11 = mu11;
    m->mu02 = mu02;
    m->mu30 = m->m30 - cx * (3 * mu20 + cx * m->m10);
    mu11 += mu11;
    m->mu21 = m->m21 - cx * (mu11 + cx * m->m01) - cy * mu20;
    m->mu12 = m->m12 - cy * (mu11 + cy * m->m10) - cx * mu02;
    m->mu03 = m->m03 - cy * (3 * mu02 + cy * m->m01);
    inv_sqrt = ALWAN_SQRT(ALWAN_ABS(inv_m00));
    s2 = inv_m00 * inv_m00;
    s3 = s2 * inv_sqrt;
    m->nu20 = m->mu20 * s2;
    m->nu11 = m->mu11 * s2;
    m->nu02 = m->mu02 * s2;
    m->nu30 = m->mu30 * s3;
    m->nu21 = m->mu21 * s3;
    m->nu12 = m->mu12 * s3;
    m->nu03 = m->mu03 * s3;
}

/* contourMoments on points (xs, ys) given as doubles */
static void ccd_contour_moments(ccd_moments *m, double const *xs, double const *ys, int lpt) {
    double a00 = 0, a10 = 0, a01 = 0, a20 = 0, a11 = 0, a02 = 0, a30 = 0, a21 = 0, a12 = 0, a03 = 0;
    double xi_1, yi_1, xi_12, yi_12;
    int i;
    memset(m, 0, sizeof(*m));
    if (lpt == 0) return;
    xi_1 = xs[lpt - 1];
    yi_1 = ys[lpt - 1];
    xi_12 = xi_1 * xi_1;
    yi_12 = yi_1 * yi_1;
    for (i = 0; i < lpt; i++) {
        double xi = xs[i], yi = ys[i];
        double xi2 = xi * xi, yi2 = yi * yi;
        double dxy = xi_1 * yi - xi * yi_1;
        double xii_1 = xi_1 + xi, yii_1 = yi_1 + yi;
        a00 += dxy;
        a10 += dxy * xii_1;
        a01 += dxy * yii_1;
        a20 += dxy * (xi_1 * xii_1 + xi2);
        a11 += dxy * (xi_1 * (yii_1 + yi_1) + xi * (yii_1 + yi));
        a02 += dxy * (yi_1 * yii_1 + yi2);
        a30 += dxy * xii_1 * (xi_12 + xi2);
        a03 += dxy * yii_1 * (yi_12 + yi2);
        a21 += dxy * (xi_12 * (3 * yi_1 + yi) + 2 * xi * xi_1 * yii_1 + xi2 * (yi_1 + 3 * yi));
        a12 += dxy * (yi_12 * (3 * xi_1 + xi) + 2 * yi * yi_1 * xii_1 + yi2 * (xi_1 + 3 * xi));
        xi_1 = xi;
        yi_1 = yi;
        xi_12 = xi2;
        yi_12 = yi2;
    }
    if (ALWAN_ABS(a00) > FLT_EPSILON) {
        double s = a00 > 0 ? 1.0 : -1.0;
        m->m00 = a00 * (s * 0.5);
        m->m10 = a10 * (s * 0.16666666666666666666666666666667);
        m->m01 = a01 * (s * 0.16666666666666666666666666666667);
        m->m20 = a20 * (s * 0.083333333333333333333333333333333);
        m->m11 = a11 * (s * 0.041666666666666666666666666666667);
        m->m02 = a02 * (s * 0.083333333333333333333333333333333);
        m->m30 = a30 * (s * 0.05);
        m->m21 = a21 * (s * 0.016666666666666666666666666666667);
        m->m12 = a12 * (s * 0.016666666666666666666666666666667);
        m->m03 = a03 * (s * 0.05);
        ccd_complete_moments(m);
    }
}

static void ccd_moments_of(ccd_moments *m, ccd_pt const *p, int n) {
    double xs[64], ys[64];
    double *hx = xs, *hy = ys;
    int i;
    if (n > 64) {
        hx = (double *)ALWAN_ALLOC(alwan_safe_array_size((size_t)n, 2 * sizeof(double)), 16);
        if (!hx) {
            memset(m, 0, sizeof(*m));
            return;
        }
        hy = hx + n;
    }
    for (i = 0; i < n; i++) {
        hx[i] = p[i].x;
        hy[i] = p[i].y;
    }
    ccd_contour_moments(m, hx, hy, n);
    if (hx != xs) ALWAN_FREE(hx);
}

static void ccd_hu(ccd_moments const *m, double hu[7]) {
    double t0 = m->nu30 + m->nu12, t1 = m->nu21 + m->nu03;
    double q0 = t0 * t0, q1 = t1 * t1;
    double n4 = 4 * m->nu11, s = m->nu20 + m->nu02, d = m->nu20 - m->nu02;
    hu[0] = s;
    hu[1] = d * d + n4 * m->nu11;
    hu[3] = q0 + q1;
    hu[5] = d * (q0 - q1) + n4 * t0 * t1;
    t0 *= q0 - 3 * q1;
    t1 *= 3 * q0 - q1;
    q0 = m->nu30 - 3 * m->nu12;
    q1 = 3 * m->nu21 - m->nu03;
    hu[2] = q0 * q0 + q1 * q1;
    hu[4] = q0 * t0 + q1 * t1;
    hu[6] = q1 * t0 - q0 * t1;
}

/* matchShapes, CONTOURS_MATCH_I2 */
static double ccd_match_shapes_i2(double const ma[7], double const mb[7]) {
    double result = 0, eps = 1.e-5;
    int any_a = 0, any_b = 0, i;
    for (i = 0; i < 7; i++) {
        double ama = ALWAN_ABS(ma[i]), amb = ALWAN_ABS(mb[i]);
        int sma = ma[i] > 0 ? 1 : (ma[i] < 0 ? -1 : 0), smb = mb[i] > 0 ? 1 : (mb[i] < 0 ? -1 : 0);
        if (ama > 0) any_a = 1;
        if (amb > 0) any_b = 1;
        if (ama > eps && amb > eps) {
            ama = sma * ALWAN_LOG10(ama);
            amb = smb * ALWAN_LOG10(amb);
            result += ALWAN_ABS(-ama + amb);
        }
    }
    if (any_a != any_b) result = DBL_MAX;
    return result;
}

/* ------------------------------------------------------------------------------------ */
/* convexHull (Sklansky), minAreaRect (rotating calipers), boxPoints                     */
/* ------------------------------------------------------------------------------------ */

/* OpenCV sorts pointers by (x, y, address); the address is the index here */
typedef struct {
    int x, y, idx;
} ccd_hull_key;

static int ccd_hull_cmp(void const *a, void const *b) {
    ccd_hull_key const *p1 = (ccd_hull_key const *)a, *p2 = (ccd_hull_key const *)b;
    if (p1->x != p2->x) return p1->x < p2->x ? -1 : 1;
    if (p1->y != p2->y) return p1->y < p2->y ? -1 : 1;
    return p1->idx < p2->idx ? -1 : (p1->idx > p2->idx ? 1 : 0);
}

static int ccd_sign64(long long v) { return (v > 0) - (v < 0); }

static int ccd_sklansky(ccd_pt const *d, int const *ptr, int start, int end, int *stack, int nsign, int sign2) {
    int incr = end > start ? 1 : -1;
    int pprev = start, pcur = pprev + incr, pnext = pcur + incr;
    int stacksize = 3;
    if (start == end || (d[ptr[start]].x == d[ptr[end]].x && d[ptr[start]].y == d[ptr[end]].y)) {
        stack[0] = start;
        return 1;
    }
    stack[0] = pprev;
    stack[1] = pcur;
    stack[2] = pnext;
    end += incr;
    while (pnext != end) {
        int cury = d[ptr[pcur]].y, nexty = d[ptr[pnext]].y;
        int by = nexty - cury;
        if (ccd_sign64(by) != nsign) {
            int ax = d[ptr[pcur]].x - d[ptr[pprev]].x, ay = cury - d[ptr[pprev]].y;
            int bx = d[ptr[pnext]].x - d[ptr[pcur]].x;
            long long convexity = (long long)ay * bx - (long long)ax * by;
            if (ccd_sign64(convexity) == sign2 && (ax != 0 || ay != 0)) {
                pprev = pcur;
                pcur = pnext;
                pnext += incr;
                stack[stacksize] = pnext;
                stacksize++;
            } else {
                if (pprev == start) {
                    pcur = pnext;
                    stack[1] = pcur;
                    pnext += incr;
                    stack[2] = pnext;
                } else {
                    stack[stacksize - 2] = pnext;
                    pcur = pprev;
                    pprev = stack[stacksize - 4];
                    stacksize--;
                }
            }
        } else {
            pnext += incr;
            stack[stacksize - 1] = pnext;
        }
    }
    return --stacksize;
}

/* convexHull_<int>, counter-clockwise (clockwise = false), returning points; writes the
 * hull's indices into data0 order in hullbuf. Returns the count, 0 on no memory. */
static int ccd_convex_hull(ccd_pt const *data0, int total, int *hullbuf) {
    int i, nout = 0, miny_ind = 0, maxy_ind = 0;
    int *pointer = (int *)ALWAN_ALLOC(alwan_safe_array_size((size_t)total * 2 + 2, sizeof(int)), 16);
    ccd_hull_key *keys = (ccd_hull_key *)ALWAN_ALLOC(alwan_safe_array_size((size_t)total, sizeof(ccd_hull_key)), 16);
    int *stack;
    if (!pointer || !keys) {
        if (pointer) ALWAN_FREE(pointer);
        if (keys) ALWAN_FREE(keys);
        return 0;
    }
    stack = pointer + total;
    for (i = 0; i < total; i++) {
        keys[i].x = data0[i].x;
        keys[i].y = data0[i].y;
        keys[i].idx = i;
    }
    qsort(keys, (size_t)total, sizeof(ccd_hull_key), ccd_hull_cmp);
    for (i = 0; i < total; i++) pointer[i] = keys[i].idx;
    ALWAN_FREE(keys);
    for (i = 1; i < total; i++) {
        int y = data0[pointer[i]].y;
        if (data0[pointer[miny_ind]].y > y) miny_ind = i;
        if (data0[pointer[maxy_ind]].y < y) maxy_ind = i;
    }
    if (data0[pointer[0]].x == data0[pointer[total - 1]].x && data0[pointer[0]].y == data0[pointer[total - 1]].y) {
        hullbuf[nout++] = 0;
    } else {
        int *tl_stack = stack, *tr_stack, *bl_stack, *br_stack, *tmp;
        int tl_count = ccd_sklansky(data0, pointer, 0, maxy_ind, tl_stack, -1, 1), tr_count, bl_count, br_count, t;
        int stop_idx;
        tr_stack = stack + tl_count;
        tr_count = ccd_sklansky(data0, pointer, total - 1, maxy_ind, tr_stack, -1, -1);
        /* not clockwise: swap */
        tmp = tl_stack;
        tl_stack = tr_stack;
        tr_stack = tmp;
        t = tl_count;
        tl_count = tr_count;
        tr_count = t;
        for (i = 0; i < tl_count - 1; i++) hullbuf[nout++] = tl_stack[i];
        for (i = tr_count - 1; i > 0; i--) hullbuf[nout++] = tr_stack[i];
        stop_idx = tr_count > 2 ? tr_stack[1] : tl_count > 2 ? tl_stack[tl_count - 2] : -1;
        bl_stack = stack;
        bl_count = ccd_sklansky(data0, pointer, 0, miny_ind, bl_stack, 1, -1);
        br_stack = stack + bl_count;
        br_count = ccd_sklansky(data0, pointer, total - 1, miny_ind, br_stack, 1, 1);
        if (stop_idx >= 0) {
            int check_idx = bl_count > 2 ? bl_stack[1] : bl_count + br_count > 2 ? br_stack[2 - bl_count] : -1;
            if (check_idx == stop_idx ||
                (check_idx >= 0 && data0[pointer[check_idx]].x == data0[pointer[stop_idx]].x &&
                 data0[pointer[check_idx]].y == data0[pointer[stop_idx]].y)) {
                bl_count = bl_count < 2 ? bl_count : 2;
                br_count = br_count < 2 ? br_count : 2;
            }
        }
        for (i = 0; i < bl_count - 1; i++) hullbuf[nout++] = bl_stack[i];
        for (i = br_count - 1; i > 0; i--) hullbuf[nout++] = br_stack[i];
        for (i = 0; i < nout; ++i) hullbuf[i] = pointer[hullbuf[i]];
        if (nout >= 3) {
            int min_idx = 0, max_idx = 0, lt = 0;
            for (i = 1; i < nout; i++) {
                int idx = hullbuf[i];
                lt += hullbuf[i - 1] < idx;
                if (lt > 1 && lt <= i - 2) break;
                if (idx < hullbuf[min_idx]) min_idx = i;
                if (idx > hullbuf[max_idx]) max_idx = i;
            }
            {
                int mmdist = max_idx - min_idx < 0 ? min_idx - max_idx : max_idx - min_idx;
                if ((mmdist == 1 || mmdist == nout - 1) && (lt <= 1 || lt >= nout - 2)) {
                    int ascending = (max_idx + 1) % nout == min_idx;
                    int i0 = ascending ? min_idx : max_idx, j = i0;
                    if (i0 > 0) {
                        for (i = 0; i < nout; i++) {
                            int curr_idx = stack[i] = hullbuf[j];
                            int next_j = j + 1 < nout ? j + 1 : 0;
                            int next_idx = hullbuf[next_j];
                            if (i < nout - 1 && (ascending != (curr_idx < next_idx))) break;
                            j = next_j;
                        }
                        if (i == nout) memcpy(hullbuf, stack, (size_t)nout * sizeof(hullbuf[0]));
                    }
                }
            }
        }
        ALWAN_FREE(pointer);
        return nout;
    }
    hullbuf[0] = pointer[hullbuf[0]];
    ALWAN_FREE(pointer);
    return nout;
}

typedef struct {
    float cx, cy, w, h, angle;
} ccd_rrect;

static void ccd_rotating_calipers(ccd_ptf const *points, int n, float *out) {
    float minarea = FLT_MAX;
    int i, k, left = 0, bottom = 0, right = 0, top = 0, seq[4];
    float base_a, base_b = 0, left_x, right_x, top_y, bottom_y, orientation = 1.f;
    float buf_a = 0, buf_w = 0, buf_b = 0, buf_h = 0;
    int buf_left = 0, buf_bottom = 0;
    float *inv_len = (float *)ALWAN_ALLOC(alwan_safe_array_size((size_t)n, 3 * sizeof(float)), 16);
    ccd_ptf *vect;
    ccd_ptf pt0 = points[0];
    if (!inv_len) {
        out[0] = out[1] = out[2] = out[3] = out[4] = out[5] = 0;
        return;
    }
    vect = (ccd_ptf *)(inv_len + n);
    left_x = right_x = pt0.x;
    top_y = bottom_y = pt0.y;
    for (i = 0; i < n; i++) {
        double dx, dy;
        ccd_ptf pt;
        if (pt0.x < left_x) left_x = pt0.x, left = i;
        if (pt0.x > right_x) right_x = pt0.x, right = i;
        if (pt0.y > top_y) top_y = pt0.y, top = i;
        if (pt0.y < bottom_y) bottom_y = pt0.y, bottom = i;
        pt = points[(i + 1) < n ? i + 1 : 0];
        dx = (double)(pt.x - pt0.x);
        dy = (double)(pt.y - pt0.y);
        vect[i].x = (float)dx;
        vect[i].y = (float)dy;
        inv_len[i] = (float)(1. / ALWAN_SQRT(dx * dx + dy * dy));
        pt0 = pt;
    }
    base_a = orientation;
    seq[0] = bottom;
    seq[1] = right;
    seq[2] = top;
    seq[3] = left;
    for (k = 0; k < n; k++) {
        int main_element = 0;
        ccd_ptf rv[4];
        rv[0] = vect[seq[0]];
        rv[1].x = vect[seq[1]].y;
        rv[1].y = -vect[seq[1]].x;
        rv[2].x = -vect[seq[2]].x;
        rv[2].y = -vect[seq[2]].y;
        rv[3].x = -vect[seq[3]].y;
        rv[3].y = vect[seq[3]].x;
        for (i = 1; i < 4; i++) {
            /* firstVecIsRight(rv[i], rv[main]): rotate90CW(v1) . v2 < 0 */
            float tx = rv[i].y, ty = -rv[i].x;
            if (tx * rv[main_element].x + ty * rv[main_element].y < 0) main_element = i;
        }
        {
            int pindex = seq[main_element];
            float lead_x = vect[pindex].x * inv_len[pindex];
            float lead_y = vect[pindex].y * inv_len[pindex];
            switch (main_element) {
            case 0: base_a = lead_x; base_b = lead_y; break;
            case 1: base_a = lead_y; base_b = -lead_x; break;
            case 2: base_a = -lead_x; base_b = -lead_y; break;
            default: base_a = -lead_y; base_b = lead_x; break;
            }
        }
        seq[main_element] += 1;
        seq[main_element] = (seq[main_element] == n) ? 0 : seq[main_element];
        {
            float dx = points[seq[1]].x - points[seq[3]].x;
            float dy = points[seq[1]].y - points[seq[3]].y;
            float width = dx * base_a + dy * base_b, height, area;
            dx = points[seq[2]].x - points[seq[0]].x;
            dy = points[seq[2]].y - points[seq[0]].y;
            height = -dx * base_b + dy * base_a;
            area = width * height;
            if (area <= minarea) {
                minarea = area;
                buf_left = seq[3];
                buf_a = base_a;
                buf_w = width;
                buf_b = base_b;
                buf_h = height;
                buf_bottom = seq[0];
            }
        }
    }
    {
        float A1 = buf_a, B1 = buf_b, A2 = -buf_b, B2 = buf_a;
        float C1 = A1 * points[buf_left].x + points[buf_left].y * B1;
        float C2 = A2 * points[buf_bottom].x + points[buf_bottom].y * B2;
        float idet = 1.f / (A1 * B2 - A2 * B1);
        float px = (C1 * B2 - C2 * B1) * idet;
        float py = (A1 * C2 - A2 * C1) * idet;
        out[0] = px;
        out[1] = py;
        out[2] = A1 * buf_w;
        out[3] = B1 * buf_w;
        out[4] = A2 * buf_h;
        out[5] = B2 * buf_h;
    }
    ALWAN_FREE(inv_len);
}

static int ccd_min_area_rect(ccd_rrect *box, ccd_pt const *p, int n) {
    double angle = -ALWAN_PI / 2;
    int *hull = (int *)ALWAN_ALLOC(alwan_safe_array_size((size_t)n + 1, sizeof(int)), 16);
    ccd_ptf *hp;
    int nh, i;
    memset(box, 0, sizeof(*box));
    if (n == 0) {
        box->angle = (float)(angle * 180 / ALWAN_PI);
        if (hull) ALWAN_FREE(hull);
        return 1;
    }
    hp = (ccd_ptf *)ALWAN_ALLOC(alwan_safe_array_size((size_t)n + 1, sizeof(ccd_ptf)), 16);
    if (!hull || !hp) {
        if (hull) ALWAN_FREE(hull);
        if (hp) ALWAN_FREE(hp);
        return 0;
    }
    nh = ccd_convex_hull(p, n, hull);
    for (i = 0; i < nh; i++) {
        hp[i].x = (float)p[hull[i]].x;
        hp[i].y = (float)p[hull[i]].y;
    }
    if (nh > 2) {
        float out[6];
        ccd_rotating_calipers(hp, nh, out);
        box->cx = out[0] + (out[2] + out[4]) * 0.5f;
        box->cy = out[1] + (out[3] + out[5]) * 0.5f;
        box->w = (float)ALWAN_SQRT((double)out[4] * out[4] + (double)out[5] * out[5]);
        box->h = (float)ALWAN_SQRT((double)out[2] * out[2] + (double)out[3] * out[3]);
        if (out[2] == 0.f && out[3] > 0.f) {
            float t = box->w;
            box->w = box->h;
            box->h = t;
        } else {
            angle = -ALWAN_ATAN2((double)out[2], (double)out[3]);
        }
    } else if (nh == 2) {
        double dx = (double)hp[0].x - hp[1].x, dy = (double)hp[0].y - hp[1].y;
        box->cx = (hp[0].x + hp[1].x) * 0.5f;
        box->cy = (hp[0].y + hp[1].y) * 0.5f;
        box->w = 0;
        box->h = (float)ALWAN_SQRT(dx * dx + dy * dy);
        if (dx == 0) {
            float t = box->w;
            box->w = box->h;
            box->h = t;
        } else if (dy < 0) {
            float t = box->w;
            angle = ALWAN_ATAN2(dy, dx);
            box->w = box->h;
            box->h = t;
        } else if (dy > 0) {
            angle = -ALWAN_ATAN2(dx, dy);
        }
    } else if (nh == 1) {
        box->cx = hp[0].x;
        box->cy = hp[0].y;
    }
    box->angle = (float)(angle * 180 / ALWAN_PI);
    ALWAN_FREE(hull);
    ALWAN_FREE(hp);
    return 1;
}

static void ccd_box_points(ccd_rrect const *b, ccd_ptf pt[4]) {
    double a_ = b->angle * ALWAN_PI / 180.;
    float bb = (float)ALWAN_COS(a_) * 0.5f;
    float aa = (float)ALWAN_SIN(a_) * 0.5f;
    float ah = aa * b->h, aw = aa * b->w, bh = bb * b->h, bw = bb * b->w;
    pt[0].x = b->cx - ah - bw;
    pt[0].y = b->cy + bh - aw;
    pt[1].x = b->cx + ah - bw;
    pt[1].y = b->cy - bh - aw;
    pt[2].x = b->cx + ah + bw;
    pt[2].y = b->cy - bh + aw;
    pt[3].x = b->cx - ah + bw;
    pt[3].y = b->cy + bh + aw;
}

/* int32(boxPoints(minAreaRect(contour))), numpy's truncating cast */
static int ccd_box_of(ccd_pt out[4], ccd_pt const *p, int n) {
    ccd_rrect r;
    ccd_ptf f[4];
    int i;
    if (!ccd_min_area_rect(&r, p, n)) return 0;
    ccd_box_points(&r, f);
    for (i = 0; i < 4; i++) {
        out[i].x = ccd_trunc(f[i].x);
        out[i].y = ccd_trunc(f[i].y);
    }
    return 1;
}

/* pointPolygonTest(contour, pt, measureDist = false): 1 inside, 0 on the edge, -1 outside */
static int ccd_point_in_polygon(ccd_pt const *c, int total, float ptx, float pty) {
    int i, counter = 0;
    int ipx = ccd_round(ptx), ipy = ccd_round(pty);
    if (total == 0) return -1;
    if ((float)ipx == ptx && (float)ipy == pty) {
        ccd_pt v0, v = c[total - 1];
        for (i = 0; i < total; i++) {
            long long dist;
            v0 = v;
            v = c[i];
            if ((v0.y <= ipy && v.y <= ipy) || (v0.y > ipy && v.y > ipy) || (v0.x < ipx && v.x < ipx)) {
                if (ipy == v.y &&
                    (ipx == v.x || (ipy == v0.y && ((v0.x <= ipx && ipx <= v.x) || (v.x <= ipx && ipx <= v0.x)))))
                    return 0;
                continue;
            }
            dist = (long long)(ipy - v0.y) * (v.x - v0.x) - (long long)(ipx - v0.x) * (v.y - v0.y);
            if (dist == 0) return 0;
            if (v.y < v0.y) dist = -dist;
            counter += dist > 0;
        }
    } else {
        float v0x, v0y, vx = (float)c[total - 1].x, vy = (float)c[total - 1].y;
        for (i = 0; i < total; i++) {
            double dist;
            v0x = vx;
            v0y = vy;
            vx = (float)c[i].x;
            vy = (float)c[i].y;
            if ((v0y <= pty && vy <= pty) || (v0y > pty && vy > pty) || (v0x < ptx && vx < ptx)) {
                if (pty == vy && (ptx == vx || (pty == v0y && ((v0x <= ptx && ptx <= vx) || (vx <= ptx && ptx <= v0x)))))
                    return 0;
                continue;
            }
            dist = (double)(pty - v0y) * (vx - v0x) - (double)(ptx - v0x) * (vy - v0y);
            if (dist == 0) return 0;
            if (vy < v0y) dist = -dist;
            counter += dist > 0;
        }
    }
    return counter % 2 == 0 ? -1 : 1;
}

/* colour-checker-detection's contour_centroid: moments of the float32 contour */
static void ccd_centroid(ccd_pt const *c, int n, double *cx, double *cy) {
    ccd_moments m;
    ccd_moments_of(&m, c, n);
    *cx = m.m10 / m.m00;
    *cy = m.m01 / m.m00;
}

/* ------------------------------------------------------------------------------------ */
/* Filled drawContours: OpenCV's fillPoly edge collection with LINE_8 outlines            */
/* ------------------------------------------------------------------------------------ */

#define CCD_XY_SHIFT 16
#define CCD_XY_ONE (1 << CCD_XY_SHIFT)

typedef struct ccd_edge_s {
    int y0, y1;
    long long x, dx;
    struct ccd_edge_s *next;
} ccd_edge;

static int ccd_clip_line(long long w, long long h, long long *x1, long long *y1, long long *x2, long long *y2) {
    int c1, c2;
    long long right = w - 1, bottom = h - 1;
    if (w <= 0 || h <= 0) return 0;
    c1 = (*x1 < 0) + (*x1 > right) * 2 + (*y1 < 0) * 4 + (*y1 > bottom) * 8;
    c2 = (*x2 < 0) + (*x2 > right) * 2 + (*y2 < 0) * 4 + (*y2 > bottom) * 8;
    if ((c1 & c2) == 0 && (c1 | c2) != 0) {
        long long a;
        if (c1 & 12) {
            a = c1 < 8 ? 0 : bottom;
            *x1 += (long long)((double)(a - *y1) * (double)(*x2 - *x1) / (double)(*y2 - *y1));
            *y1 = a;
            c1 = (*x1 < 0) + (*x1 > right) * 2;
        }
        if (c2 & 12) {
            a = c2 < 8 ? 0 : bottom;
            *x2 += (long long)((double)(a - *y2) * (double)(*x2 - *x1) / (double)(*y2 - *y1));
            *y2 = a;
            c2 = (*x2 < 0) + (*x2 > right) * 2;
        }
        if ((c1 & c2) == 0 && (c1 | c2) != 0) {
            if (c1) {
                a = c1 == 1 ? 0 : right;
                *y1 += (long long)((double)(a - *x1) * (double)(*y2 - *y1) / (double)(*x2 - *x1));
                *x1 = a;
                c1 = 0;
            }
            if (c2) {
                a = c2 == 1 ? 0 : right;
                *y2 += (long long)((double)(a - *x2) * (double)(*y2 - *y1) / (double)(*x2 - *x1));
                *x2 = a;
                c2 = 0;
            }
        }
    }
    return (c1 | c2) == 0;
}

/* cv::Line through LineIterator, 8-connected, left to right */
static void ccd_line8(unsigned char *img, int w, int h, int x1i, int y1i, int x2i, int y2i, unsigned char color) {
    long long x1 = x1i, y1 = y1i, x2 = x2i, y2 = y2i;
    int dx, dy, delta_x = 1, delta_y = 1, err, plus_delta, minus_delta, plus_step, minus_step, plus_shift,
        minus_shift, count, i, px, py, vert, t;
    if ((unsigned long long)x1 >= (unsigned long long)w || (unsigned long long)x2 >= (unsigned long long)w ||
        (unsigned long long)y1 >= (unsigned long long)h || (unsigned long long)y2 >= (unsigned long long)h) {
        if (!ccd_clip_line(w, h, &x1, &y1, &x2, &y2)) return;
    }
    dx = (int)(x2 - x1);
    dy = (int)(y2 - y1);
    if (dx < 0) {
        dx = -dx;
        dy = -dy;
        x1 = x2;
        y1 = y2;
    }
    if (dy < 0) {
        dy = -dy;
        delta_y = -1;
    }
    vert = dy > dx;
    if (vert) {
        t = dx; dx = dy; dy = t;
        t = delta_x; delta_x = delta_y; delta_y = t;
    }
    err = dx - (dy + dy);
    plus_delta = dx + dx;
    minus_delta = -(dy + dy);
    minus_shift = delta_x;
    plus_shift = 0;
    minus_step = 0;
    plus_step = delta_y;
    count = dx + 1;
    if (vert) {
        t = plus_step; plus_step = plus_shift; plus_shift = t;
        t = minus_step; minus_step = minus_shift; minus_shift = t;
    }
    px = (int)x1;
    py = (int)y1;
    for (i = 0; i < count; i++) {
        int mask;
        if (px >= 0 && px < w && py >= 0 && py < h) img[(size_t)py * w + px] = color;
        mask = err < 0 ? -1 : 0;
        err += minus_delta + (plus_delta & mask);
        px += minus_shift + (plus_shift & mask);
        py += minus_step + (plus_step & mask);
    }
}

static int ccd_edge_cmp(void const *a, void const *b) {
    ccd_edge const *e1 = (ccd_edge const *)a, *e2 = (ccd_edge const *)b;
    if (e1->y0 != e2->y0) return e1->y0 < e2->y0 ? -1 : 1;
    if (e1->x != e2->x) return e1->x < e2->x ? -1 : 1;
    if (e1->dx != e2->dx) return e1->dx < e2->dx ? -1 : 1;
    return 0;
}

static int ccd_fill_polys(unsigned char *img, int w, int h, ccd_pt const *const *polys, int const *npts, int npoly,
                          unsigned char color) {
    int total = 0, i, k, y, ne = 0;
    ccd_edge *edges, tmp, *e;
    int y_max = INT_MIN, y_min = INT_MAX;
    long long x_max = LLONG_MIN, x_min = LLONG_MAX;
    long long delta = CCD_XY_ONE - 1;
    for (i = 0; i < npoly; i++) total += npts[i];
    edges = (ccd_edge *)ALWAN_ALLOC(alwan_safe_array_size((size_t)total + 1, sizeof(ccd_edge)), 16);
    if (!edges) return 0;
    for (k = 0; k < npoly; k++) {
        ccd_pt const *v = polys[k];
        int count = npts[k];
        long long p0x, p0y;
        if (count <= 0) continue;
        p0x = (long long)v[count - 1].x << CCD_XY_SHIFT;
        p0y = v[count - 1].y;
        for (i = 0; i < count; i++) {
            long long p1x = (long long)v[i].x << CCD_XY_SHIFT, p1y = v[i].y;
            long long t0x = (p0x + (CCD_XY_ONE >> 1)) >> CCD_XY_SHIFT, t0y = p0y;
            long long t1x = (p1x + (CCD_XY_ONE >> 1)) >> CCD_XY_SHIFT, t1y = p1y;
            long long p0cx = p0x, p0cy = p0y, p1cx = p1x, p1cy = p1y;
            ccd_line8(img, w, h, (int)t0x, (int)t0y, (int)t1x, (int)t1y, color);
            if ((unsigned long long)t0x >= (unsigned long long)w || (unsigned long long)t1x >= (unsigned long long)w ||
                (unsigned long long)t0y >= (unsigned long long)h || (unsigned long long)t1y >= (unsigned long long)h) {
                /* clipLine(Size, Point&, Point&) works in int */
                long long cx0 = (int)t0x, cy0 = (int)t0y, cx1 = (int)t1x, cy1 = (int)t1y;
                ccd_clip_line(w, h, &cx0, &cy0, &cx1, &cy1);
                t0x = (int)cx0;
                t0y = (int)cy0;
                t1x = (int)cx1;
                t1y = (int)cy1;
                if (t0y != t1y) {
                    p0cy = t0y;
                    p1cy = t1y;
                }
            }
            p0cx = t0x << CCD_XY_SHIFT;
            p1cx = t1x << CCD_XY_SHIFT;
            if (p0y != p1y) {
                ccd_edge *ed = &edges[ne++];
                ed->dx = (p1cx - p0cx) / (p1cy - p0cy);
                if (p0y < p1y) {
                    ed->y0 = (int)p0y;
                    ed->y1 = (int)p1y;
                    ed->x = p0cx + (p0y - p0cy) * ed->dx;
                } else {
                    ed->y0 = (int)p1y;
                    ed->y1 = (int)p0y;
                    ed->x = p1cx + (p1y - p1cy) * ed->dx;
                }
                ed->next = NULL;
            }
            p0x = p1x;
            p0y = p1y;
        }
    }
    total = ne;
    if (total < 2) {
        ALWAN_FREE(edges);
        return 1;
    }
    for (i = 0; i < total; i++) {
        ccd_edge *e1 = &edges[i];
        long long x1 = e1->x + (e1->y1 - e1->y0) * e1->dx;
        if (e1->y0 < y_min) y_min = e1->y0;
        if (e1->y1 > y_max) y_max = e1->y1;
        if (e1->x < x_min) x_min = e1->x;
        if (e1->x > x_max) x_max = e1->x;
        if (x1 < x_min) x_min = x1;
        if (x1 > x_max) x_max = x1;
    }
    if (y_max < 0 || y_min >= h || x_max < 0 || x_min >= ((long long)w << CCD_XY_SHIFT)) {
        ALWAN_FREE(edges);
        return 1;
    }
    qsort(edges, (size_t)total, sizeof(ccd_edge), ccd_edge_cmp);
    memset(&tmp, 0, sizeof(tmp));
    edges[total].y0 = INT_MAX;
    edges[total].next = NULL;
    i = 0;
    tmp.next = NULL;
    e = &edges[i];
    if (y_max > h) y_max = h;
    for (y = e->y0; y < y_max; y++) {
        ccd_edge *last, *prelast, *keep_prelast;
        int draw = 0, clipline = y < 0;
        prelast = &tmp;
        last = tmp.next;
        while (last || e->y0 == y) {
            if (last && last->y1 == y) {
                prelast->next = last->next;
                last = last->next;
                continue;
            }
            keep_prelast = prelast;
            if (last && (e->y0 > y || last->x < e->x)) {
                prelast = last;
                last = last->next;
            } else if (i < total) {
                prelast->next = e;
                e->next = last;
                prelast = e;
                e = &edges[++i];
            } else {
                break;
            }
            if (draw) {
                if (!clipline) {
                    int x1, x2, xx;
                    if (keep_prelast->x > prelast->x) {
                        x1 = (int)((prelast->x + delta) >> CCD_XY_SHIFT);
                        x2 = (int)(keep_prelast->x >> CCD_XY_SHIFT);
                    } else {
                        x1 = (int)((keep_prelast->x + delta) >> CCD_XY_SHIFT);
                        x2 = (int)(prelast->x >> CCD_XY_SHIFT);
                    }
                    if (x1 < w && x2 >= 0) {
                        if (x1 < 0) x1 = 0;
                        if (x2 >= w) x2 = w - 1;
                        for (xx = x1; xx <= x2; xx++) img[(size_t)y * w + xx] = color;
                    }
                }
                keep_prelast->x += keep_prelast->dx;
                prelast->x += prelast->dx;
            }
            draw ^= 1;
        }
        keep_prelast = NULL;
        do {
            ccd_edge *last_exchange = NULL;
            prelast = &tmp;
            last = tmp.next;
            while (last != keep_prelast && last->next != NULL) {
                ccd_edge *te = last->next;
                if (last->x > te->x) {
                    prelast->next = te;
                    last->next = te->next;
                    te->next = last;
                    prelast = te;
                    last_exchange = prelast;
                } else {
                    prelast = last;
                    last = te;
                }
            }
            if (last_exchange == NULL) break;
            keep_prelast = last_exchange;
        } while (keep_prelast != tmp.next && keep_prelast != &tmp);
    }
    ALWAN_FREE(edges);
    return 1;
}

/* ------------------------------------------------------------------------------------ */
/* Perspective                                                                          */
/* ------------------------------------------------------------------------------------ */

/* cv::solve DECOMP_LU on an 8 x 8 system (partial pivoting), in place; 0 when singular */
static int ccd_lu_solve8(double A[8][8], double b[8]) {
    int i, j, k, m = 8;
    for (i = 0; i < m; i++) {
        double d;
        k = i;
        for (j = i + 1; j < m; j++)
            if (ALWAN_ABS(A[j][i]) > ALWAN_ABS(A[k][i])) k = j;
        if (ALWAN_ABS(A[k][i]) < DBL_EPSILON * 100) return 0;
        if (k != i) {
            double t;
            for (j = i; j < m; j++) {
                t = A[i][j];
                A[i][j] = A[k][j];
                A[k][j] = t;
            }
            t = b[i];
            b[i] = b[k];
            b[k] = t;
        }
        d = -1 / A[i][i];
        for (j = i + 1; j < m; j++) {
            double alpha = A[j][i] * d;
            for (k = i + 1; k < m; k++) A[j][k] += alpha * A[i][k];
            b[j] += alpha * b[i];
        }
        A[i][i] = -d;
    }
    for (i = m - 1; i >= 0; i--) {
        double s = b[i];
        for (k = i + 1; k < m; k++) s -= A[i][k] * b[k];
        b[i] = s * A[i][i];
    }
    return 1;
}

/* getPerspectiveTransform(src, dst) for float32 points */
static int ccd_perspective(double M[9], ccd_ptf const src[4], ccd_ptf const dst[4]) {
    double a[8][8], b[8];
    int i;
    for (i = 0; i < 4; ++i) {
        a[i][0] = a[i + 4][3] = src[i].x;
        a[i][1] = a[i + 4][4] = src[i].y;
        a[i][2] = a[i + 4][5] = 1;
        a[i][3] = a[i][4] = a[i][5] = a[i + 4][0] = a[i + 4][1] = a[i + 4][2] = 0;
        a[i][6] = -(double)(src[i].x * dst[i].x);
        a[i][7] = -(double)(src[i].y * dst[i].x);
        a[i + 4][6] = -(double)(src[i].x * dst[i].y);
        a[i + 4][7] = -(double)(src[i].y * dst[i].y);
        b[i] = dst[i].x;
        b[i + 4] = dst[i].y;
    }
    if (!ccd_lu_solve8(a, b)) return 0;
    for (i = 0; i < 8; i++) M[i] = b[i];
    M[8] = 1.;
    return 1;
}

/* cv::invert DECOMP_LU on 3 x 3 doubles (the closed form OpenCV takes for n <= 3) */
static int ccd_invert3(double r[9], double const s[9]) {
    double d = s[0] * (s[4] * s[8] - s[5] * s[7]) - s[1] * (s[3] * s[8] - s[5] * s[6]) + s[2] * (s[3] * s[7] - s[4] * s[6]);
    if (d == 0.) return 0;
    d = 1. / d;
    r[0] = (s[4] * s[8] - s[5] * s[7]) * d;
    r[1] = (s[2] * s[7] - s[1] * s[8]) * d;
    r[2] = (s[1] * s[5] - s[2] * s[4]) * d;
    r[3] = (s[5] * s[6] - s[3] * s[8]) * d;
    r[4] = (s[0] * s[8] - s[2] * s[6]) * d;
    r[5] = (s[2] * s[3] - s[0] * s[5]) * d;
    r[6] = (s[3] * s[7] - s[4] * s[6]) * d;
    r[7] = (s[1] * s[6] - s[0] * s[7]) * d;
    r[8] = (s[0] * s[4] - s[1] * s[3]) * d;
    return 1;
}

/* One pixel of OpenCV 5's bicubic warpPerspective (float32, 3 channels, BORDER_CONSTANT 0),
 * the fused AVX2 kernel: Mf is the float inverse map. */
static void ccd_warp_pixel(float out[3], float const Mf[9], int x, int y, float const *src, int sw, int sh) {
    float M_x = (float)y * Mf[1] + Mf[2];
    float M_y = (float)y * Mf[4] + Mf[5];
    float M_z = (float)y * Mf[7] + Mf[8];
    double xf = (double)x;
    double invz = 1. / ((double)M_z + (double)Mf[6] * xf);
    float xs = (float)(((double)M_x + (double)Mf[0] * xf) * invz);
    float ys = (float)(((double)M_y + (double)Mf[3] * xf) * invz);
    int bigw = sw > 16 ? sw : 16, bigh = sh > 16 ? sh : 16;
    float vx0 = xs, vy0 = ys, alpha, beta, wx[4], wy[4];
    float const A = -0.75f;
    float acc[3] = {0.f, 0.f, 0.f};
    int ix0, iy0, r, c, ch;
    if (!(vx0 >= (float)-bigw)) vx0 = (float)-bigw;
    if (vx0 > (float)(bigw * 2)) vx0 = (float)(bigw * 2);
    if (!(vy0 >= (float)-bigh)) vy0 = (float)-bigh;
    if (vy0 > (float)(bigh * 2)) vy0 = (float)(bigh * 2);
    ix0 = ccd_floor(vx0);
    iy0 = ccd_floor(vy0);
    alpha = vx0 - (float)ix0;
    beta = vy0 - (float)iy0;
    ix0--;
    iy0--;
    if ((unsigned)(ix0 + 4) >= (unsigned)(sw + 4) || (unsigned)(iy0 + 4) >= (unsigned)(sh + 4)) {
        out[0] = out[1] = out[2] = 0.f;
        return;
    }
    {
        float a2 = alpha * alpha, b = 1.f - alpha, b2 = b * b;
        wx[0] = A * (alpha * b2);
        wx[3] = A * (a2 * b);
        wx[1] = ALWAN_FMAF(a2, ALWAN_FMAF(A + 2.0f, alpha, -(A + 3.0f)), 1.0f);
        wx[2] = ((1.0f - wx[0]) - wx[1]) - wx[3];
        a2 = beta * beta;
        b = 1.f - beta;
        b2 = b * b;
        wy[0] = A * (beta * b2);
        wy[3] = A * (a2 * b);
        wy[1] = ALWAN_FMAF(a2, ALWAN_FMAF(A + 2.0f, beta, -(A + 3.0f)), 1.0f);
        wy[2] = ((1.0f - wy[0]) - wy[1]) - wy[3];
    }
    for (r = 0; r < 4; r++) {
        int yy = iy0 + r;
        for (ch = 0; ch < 3; ch++) {
            float v[4], sumwx;
            for (c = 0; c < 4; c++) {
                int xx = ix0 + c;
                v[c] = ((unsigned)yy < (unsigned)sh && (unsigned)xx < (unsigned)sw) ? src[((size_t)yy * sw + xx) * 3 + ch]
                                                                                    : 0.f;
            }
            sumwx = ALWAN_FMAF(v[1], wx[1], v[0] * wx[0]);
            sumwx = ALWAN_FMAF(v[2], wx[2], sumwx);
            sumwx = ALWAN_FMAF(v[3], wx[3], sumwx);
            acc[ch] = ALWAN_FMAF(sumwx, wy[r], acc[ch]);
        }
    }
    out[0] = acc[0];
    out[1] = acc[1];
    out[2] = acc[2];
}

/* ------------------------------------------------------------------------------------ */
/* The pipeline                                                                         */
/* ------------------------------------------------------------------------------------ */

typedef struct {
    int working_width, sh, sv, samples, count_min, count_max, bilateral_iterations, block, no_encoding;
    double aspect, aspect_min, aspect_max, area_factor, contour_scale, sigma_color, sigma_space, C;
    double const *reference;
} ccd_settings;

static alwan_status ccd_settings_from(ccd_settings *s, alwan_checker_detect_params const *p) {
    alwan_checker_detect_params z;
    int sw;
    if (!p) {
        memset(&z, 0, sizeof(z));
        p = &z;
    }
    s->working_width = p->working_width > 0 ? (int)p->working_width : 1440;
    s->sh = p->swatches_horizontal > 0 ? (int)p->swatches_horizontal : 6;
    s->sv = p->swatches_vertical > 0 ? (int)p->swatches_vertical : 4;
    if ((size_t)s->sh * (size_t)s->sv > ALWAN_CHECKER_MAX_SWATCHES) return ALWAN_E_INVALID;
    sw = s->sh * s->sv;
    s->aspect = p->aspect_ratio > 0 ? p->aspect_ratio : (double)s->sh / s->sv;
    s->aspect_min = p->aspect_ratio_minimum > 0 ? p->aspect_ratio_minimum : s->aspect * 0.9;
    s->aspect_max = p->aspect_ratio_maximum > 0 ? p->aspect_ratio_maximum : s->aspect * 1.1;
    s->count_min = p->swatches_count_minimum > 0 ? (int)p->swatches_count_minimum : (int)(sw * 0.5);
    s->count_max = p->swatches_count_maximum > 0 ? (int)p->swatches_count_maximum : (int)(sw * 1.5);
    s->area_factor = p->swatch_minimum_area_factor > 0 ? p->swatch_minimum_area_factor : 200;
    s->contour_scale = p->swatch_contour_scale > 0 ? p->swatch_contour_scale : 1 + 1. / 3;
    s->samples = p->samples > 0 ? (int)p->samples : 32;
    s->bilateral_iterations = p->bilateral_iterations > 0 ? (int)p->bilateral_iterations : 5;
    s->sigma_color = p->bilateral_sigma_color > 0 ? p->bilateral_sigma_color : 5;
    s->sigma_space = p->bilateral_sigma_space > 0 ? p->bilateral_sigma_space : 5;
    s->block = p->threshold_block_size > 0 ? (int)p->threshold_block_size
                                           : (int)(s->working_width * 0.015) - (int)(s->working_width * 0.015) % 2 + 1;
    s->C = p->threshold_constant != 0 ? p->threshold_constant : 3;
    s->no_encoding = p->skip_srgb_encoding != 0;
    s->reference = p->reference_values;
    if (!s->reference) {
        if (sw != 24) return ALWAN_E_INVALID;
        s->reference = ccd_reference_default;
    }
    if (s->block < 3 || s->block % 2 == 0 || s->working_width < 16) return ALWAN_E_INVALID;
    return ALWAN_OK;
}

/* colour-checker-detection's remove_stacked_contours (keep_smallest) on 4-point boxes */
static int ccd_remove_stacked(ccd_pt (*sq)[4], int n, ccd_pt (*out)[4]) {
    int nf = 0, i, j;
    int *stacked = (int *)ALWAN_ALLOC(alwan_safe_array_size((size_t)n + 1, sizeof(int)), 16);
    if (!stacked) return -1;
    for (i = 0; i < n; i++) {
        double cx, cy;
        int ns = 0;
        ccd_centroid(sq[i], 4, &cx, &cy);
        for (j = 0; j < nf; j++)
            if (ccd_point_in_polygon(out[j], 4, (float)cx, (float)cy) > 0) stacked[ns++] = j;
        if (ns == 0) {
            memcpy(out[nf++], sq[i], sizeof(out[0]));
        } else {
            double area = ccd_contour_area(sq[i], 4);
            int all_smaller = 1, best = -1;
            float best_area = 0;
            for (j = 0; j < ns; j++) {
                float aj = (float)ccd_contour_area(out[stacked[j]], 4);
                if (!(area < (double)aj)) all_smaller = 0;
                if (best < 0 || aj < best_area) {
                    best = j;
                    best_area = aj;
                }
            }
            if (all_smaller) {
                /* the first filtered box equal to the smallest stacked one */
                int target = stacked[best], k;
                for (k = 0; k < nf; k++) {
                    if (memcmp(out[k], out[target], sizeof(out[0])) == 0) break;
                }
                memcpy(out[k], sq[i], sizeof(out[0]));
            }
        }
    }
    ALWAN_FREE(stacked);
    return nf;
}

/* numpy.linspace(start, stop, num)[i]: start + i * step, the last one exactly stop */
static double ccd_linspace(double start, double stop, int num, int i) {
    double step;
    if (num <= 1) return start;
    if (i == num - 1) return stop;
    step = (stop - start) / (num - 1);
    return start + i * step;
}

static double ccd_mse(double const *ref, float const *s, int n) {
    double acc = 0;
    int i;
    for (i = 0; i < n; i++) {
        double d = ref[i] - (double)s[i];
        acc += d * d;
    }
    return acc / n;
}

/* The swatches under a quadrilateral: warp and average each window (colour-checker-detection's
 * swatch_masks and swatch_colours). numpy's mean over the window's two axes keeps the channel
 * innermost, so each channel is a float32 running sum in row order, divided in float32. */
static void ccd_sample(float *colours, float const *img, int w, int h, ccd_ptf const quad[4], ccd_settings const *s,
                       int ww, int wh) {
    double M[9], Mi[9];
    float Mf[9];
    ccd_ptf rect[4];
    int i, j, sx, sy, half = s->samples / 2 > 1 ? s->samples / 2 : 1;
    double offh = (double)ww / s->sh / 2, offv = (double)wh / s->sv / 2;
    rect[0].x = (float)ww;
    rect[0].y = 0;
    rect[1].x = (float)ww;
    rect[1].y = (float)(int)(ww / s->aspect);
    rect[2].x = 0;
    rect[2].y = (float)(int)(ww / s->aspect);
    rect[3].x = 0;
    rect[3].y = 0;
    if (!ccd_perspective(M, quad, rect) || !ccd_invert3(Mi, M)) {
        memset(colours, 0, (size_t)s->sh * s->sv * 3 * sizeof(float));
        return;
    }
    for (i = 0; i < 9; i++) Mf[i] = (float)Mi[i];
    for (sy = 0; sy < s->sv; sy++) {
        double jc = ccd_linspace(offv, wh - offv, s->sv, sy);
        for (sx = 0; sx < s->sh; sx++) {
            double ic = ccd_linspace(offh, ww - offh, s->sh, sx);
            int r0 = ccd_trunc(jc - half), r1 = ccd_trunc(jc + half), c0 = ccd_trunc(ic - half), c1 = ccd_trunc(ic + half);
            float acc[3] = {0.f, 0.f, 0.f};
            int cnt = 0;
            if (r0 < 0) r0 = 0;
            if (c0 < 0) c0 = 0;
            if (r1 > wh) r1 = wh;
            if (c1 > ww) c1 = ww;
            for (i = r0; i < r1; i++) {
                for (j = c0; j < c1; j++) {
                    float px[3];
                    ccd_warp_pixel(px, Mf, j, i, img, w, h);
                    acc[0] += px[0];
                    acc[1] += px[1];
                    acc[2] += px[2];
                    cnt++;
                }
            }
            for (i = 0; i < 3; i++)
                colours[(sy * s->sh + sx) * 3 + i] = cnt ? acc[i] / (float)cnt : 0.f;
        }
    }
}

static alwan_status ccd_detect(alwan_checker_detection *out, size_t capacity, size_t *count, float const *in0, int w0,
                               int h0, ccd_settings const *s) {
    alwan_status st = ALWAN_OK;
    int rotated = w0 < h0, w1 = rotated ? h0 : w0, h1 = rotated ? w0 : h0;
    int W = s->working_width, H, x, y, i, j;
    int nsw = s->sh * s->sv, wh;
    float *rot = NULL, *img = NULL;
    unsigned char *g8 = NULL, *g8b = NULL;
    ccd_polys contours, clusters;
    ccd_pt(*squares)[4] = NULL, (*swatches)[4] = NULL, (*rects)[4] = NULL;
    ccd_pt *qbuf = NULL, *work = NULL, **scaled_ptrs = NULL;
    ccd_pt(*scaled)[4] = NULL;
    int *slo = NULL, *shi = NULL, *npts = NULL;
    int nsq = 0, nswt = 0, nrect = 0, maxn = 0;
    double ratio, minimum_area, maximum_area;
    memset(&contours, 0, sizeof(contours));
    memset(&clusters, 0, sizeof(clusters));
    *count = 0;
    ratio = (double)w1 / W;
    H = (int)(h1 / ratio);
    if (H < 1) return ALWAN_E_RANGE;
    wh = (int)(W / s->aspect);
    rot = (float *)ALWAN_ALLOC(alwan_safe_array_size((size_t)w1 * (size_t)h1, 3 * sizeof(float)), 64);
    img = (float *)ALWAN_ALLOC(alwan_safe_array_size((size_t)W * (size_t)H, 3 * sizeof(float)), 64);
    g8 = (unsigned char *)ALWAN_ALLOC(alwan_safe_array_size((size_t)W, (size_t)H), 64);
    g8b = (unsigned char *)ALWAN_ALLOC(alwan_safe_array_size((size_t)W, (size_t)H), 64);
    if (!rot || !img || !g8 || !g8b) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    /* cv::rotate ROTATE_90_CLOCKWISE when taller than wide */
    for (y = 0; y < h1; y++) {
        for (x = 0; x < w1; x++) {
            float const *p = rotated ? in0 + ((size_t)(h0 - 1 - x) * w0 + y) * 3 : in0 + ((size_t)y * w0 + x) * 3;
            float *q = rot + ((size_t)y * w1 + x) * 3;
            q[0] = p[0];
            q[1] = p[1];
            q[2] = p[2];
        }
    }
    if (!ccd_resize_cubic(img, W, H, rot, w1, h1, 3)) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    ALWAN_FREE(rot);
    rot = NULL;

    /* segmenter_default: sRGB encode (double), maximum channel, stretched, truncated to 8 bits */
    {
        double *gd = (double *)ALWAN_ALLOC(alwan_safe_array_size((size_t)W * (size_t)H, sizeof(double)), 64);
        double gmin = DBL_MAX, gmax = -DBL_MAX;
        size_t n = (size_t)W * (size_t)H, k;
        if (!gd) {
            st = ALWAN_E_NOMEM;
            goto done;
        }
        for (k = 0; k < n; k++) {
            double m = -DBL_MAX;
            int c;
            for (c = 0; c < 3; c++) {
                double L = (double)img[k * 3 + c], V;
                if (s->no_encoding) V = L;
                else V = L <= 0.0031308 ? L * 12.92 : 1.055 * ALWAN_POW(L, 1. / 2.4) - 0.055;
                if (V > m) m = V;
            }
            gd[k] = m;
            if (m < gmin) gmin = m;
            if (m > gmax) gmax = m;
        }
        for (k = 0; k < n; k++) {
            double v = ((gd[k] - gmin) / (gmax - gmin)) * (1.0 - 0.0) + 0.0;
            v *= 255;
            g8[k] = (unsigned char)(v >= 0 && v < 256 ? (int)v : 0);
        }
        ALWAN_FREE(gd);
    }
    for (i = 0; i < s->bilateral_iterations; i++) {
        if (!ccd_bilateral(g8b, g8, W, H, s->sigma_color, s->sigma_space)) {
            st = ALWAN_E_NOMEM;
            goto done;
        }
        memcpy(g8, g8b, (size_t)W * H);
    }
    if (!ccd_adaptive_threshold(g8b, g8, W, H, s->block, s->C, 255)) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    ccd_morph3(g8, g8b, W, H, 0);
    ccd_morph3(g8b, g8, W, H, 1);
    if (!ccd_find_contours(&contours, g8b, W, H, 1)) {
        st = ALWAN_E_NOMEM;
        goto done;
    }

    /* quadrilaterals: area and squareness, then their minimum-area boxes */
    minimum_area = (double)W * H / nsw / s->area_factor;
    maximum_area = (double)W * H / nsw;
    for (i = 0; i < contours.n; i++)
        if (contours.v[i].n > maxn) maxn = contours.v[i].n;
    squares = (ccd_pt(*)[4])ALWAN_ALLOC(alwan_safe_array_size((size_t)contours.n + 1, sizeof(*squares)), 16);
    qbuf = (ccd_pt *)ALWAN_ALLOC(alwan_safe_array_size((size_t)maxn + 4, sizeof(ccd_pt)), 16);
    work = (ccd_pt *)ALWAN_ALLOC(alwan_safe_array_size((size_t)maxn + 4, sizeof(ccd_pt)), 16);
    slo = (int *)ALWAN_ALLOC(alwan_safe_array_size((size_t)maxn + 4, 2 * sizeof(int)), 16);
    if (!squares || !qbuf || !work || !slo) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    shi = slo + maxn + 4;
    {
        static ccd_pt const unit[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        double hu_unit[7];
        ccd_moments mu;
        ccd_moments_of(&mu, unit, 4);
        ccd_hu(&mu, hu_unit);
        for (i = 0; i < contours.n; i++) {
            ccd_poly const *c = &contours.v[i];
            int nq = ccd_quadrilateralise(c->p, c->n, qbuf, work, slo, shi);
            double area = ccd_contour_area(qbuf, nq);
            if (minimum_area < area && area < maximum_area) {
                ccd_moments m;
                double hu[7];
                ccd_moments_of(&m, qbuf, nq);
                ccd_hu(&m, hu);
                if (ccd_match_shapes_i2(hu, hu_unit) < 0.015) {
                    if (!ccd_box_of(squares[nsq], qbuf, nq)) {
                        st = ALWAN_E_NOMEM;
                        goto done;
                    }
                    nsq++;
                }
            }
        }
    }
    swatches = (ccd_pt(*)[4])ALWAN_ALLOC(alwan_safe_array_size((size_t)nsq + 1, sizeof(*swatches)), 16);
    if (!swatches) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    nswt = ccd_remove_stacked(squares, nsq, swatches);
    if (nswt < 0) {
        st = ALWAN_E_NOMEM;
        goto done;
    }

    /* cluster_swatches: grown boxes drawn filled, outer contours, their boxes */
    memset(g8, 0, (size_t)W * H);
    scaled = (ccd_pt(*)[4])ALWAN_ALLOC(alwan_safe_array_size((size_t)nswt + 1, sizeof(*scaled)), 16);
    scaled_ptrs = (ccd_pt **)ALWAN_ALLOC(alwan_safe_array_size((size_t)nswt + 1, sizeof(ccd_pt *)), 16);
    npts = (int *)ALWAN_ALLOC(alwan_safe_array_size((size_t)nswt + 1, sizeof(int)), 16);
    if (!scaled || !scaled_ptrs || !npts) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    {
        /* numpy: the float32 contour minus a tuple of doubles is float64, and the factor
         * comes in as float32 */
        double f = (double)(float)s->contour_scale;
        for (i = 0; i < nswt; i++) {
            double cx, cy;
            ccd_centroid(swatches[i], 4, &cx, &cy);
            for (j = 0; j < 4; j++) {
                double px = ((double)swatches[i][j].x - cx) * f + cx;
                double py = ((double)swatches[i][j].y - cy) * f + cy;
                scaled[i][j].x = ccd_trunc(px);
                scaled[i][j].y = ccd_trunc(py);
            }
            scaled_ptrs[i] = scaled[i];
            npts[i] = 4;
        }
    }
    if (!ccd_fill_polys(g8, W, H, (ccd_pt const *const *)scaled_ptrs, npts, nswt, 255) ||
        !ccd_find_contours(&clusters, g8, W, H, 0)) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    rects = (ccd_pt(*)[4])ALWAN_ALLOC(alwan_safe_array_size((size_t)clusters.n + 1, sizeof(*rects)), 16);
    if (!rects) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    for (i = 0; i < clusters.n; i++) {
        ccd_pt box[4];
        ccd_rrect r;
        double wd, ht, ar;
        int cnt = 0, k;
        if (!ccd_box_of(box, clusters.v[i].p, clusters.v[i].n) || !ccd_min_area_rect(&r, box, 4)) {
            st = ALWAN_E_NOMEM;
            goto done;
        }
        wd = r.w > r.h ? r.w : r.h;
        ht = r.w > r.h ? r.h : r.w;
        ar = wd / ht;
        if (!(s->aspect_min < ar && ar < s->aspect_max)) continue;
        for (k = 0; k < nswt; k++) {
            double cx, cy;
            ccd_centroid(swatches[k], 4, &cx, &cy);
            if (ccd_point_in_polygon(box, 4, (float)cx, (float)cy) >= 0) cnt++;
        }
        if (cnt < s->count_min || cnt > s->count_max) continue;
        memcpy(rects[nrect++], box, sizeof(box));
    }

    /* extractor_segmentation: sample each, keep the corner order nearest the reference */
    for (i = 0; i < nrect; i++) {
        alwan_checker_detection *d;
        ccd_ptf quad[4];
        float cols[ALWAN_CHECKER_MAX_SWATCHES * 3], cand[ALWAN_CHECKER_MAX_SWATCHES * 3];
        double best;
        int k, roll;
        if ((size_t)*count >= capacity) {
            (*count)++;
            continue;
        }
        d = &out[*count];
        for (k = 0; k < 4; k++) {
            quad[k].x = (float)rects[i][k].x;
            quad[k].y = (float)rects[i][k].y;
        }
        ccd_sample(cols, img, W, H, quad, s, W, wh);
        best = ccd_mse(s->reference, cols, nsw * 3);
        {
            ccd_ptf cq[4], bq[4];
            memcpy(cq, quad, sizeof(cq));
            memcpy(bq, quad, sizeof(bq));
            for (roll = 0; roll < 3; roll++) {
                ccd_ptf t = cq[3];
                double e;
                cq[3] = cq[2];
                cq[2] = cq[1];
                cq[1] = cq[0];
                cq[0] = t;
                ccd_sample(cand, img, W, H, cq, s, W, wh);
                e = ccd_mse(s->reference, cand, nsw * 3);
                if (e < best) {
                    best = e;
                    memcpy(cols, cand, (size_t)nsw * 3 * sizeof(float));
                    memcpy(bq, cq, sizeof(bq));
                }
            }
            memcpy(quad, bq, sizeof(quad));
        }
        memset(d, 0, sizeof(*d));
        d->swatch_count = (size_t)nsw;
        for (k = 0; k < nsw * 3; k++) d->swatches[k / 3][k % 3] = cols[k];
        d->mse = best;
        for (k = 0; k < 4; k++) {
            double wx = quad[k].x, wy = quad[k].y, ix, iy, sc = (double)w1 / W, scy = (double)h1 / H;
            d->quad[k][0] = wx;
            d->quad[k][1] = wy;
            /* back to the input: undo the resize (pixel centres), then the rotation */
            ix = (wx + 0.5) * sc - 0.5;
            iy = (wy + 0.5) * scy - 0.5;
            if (rotated) {
                d->quad_image[k][0] = iy;
                d->quad_image[k][1] = (double)(h0 - 1) - ix;
            } else {
                d->quad_image[k][0] = ix;
                d->quad_image[k][1] = iy;
            }
        }
        d->working_width = (size_t)W;
        d->working_height = (size_t)H;
        (*count)++;
    }
    if ((size_t)*count > capacity) st = ALWAN_E_RANGE;

done:
    if (rot) ALWAN_FREE(rot);
    if (img) ALWAN_FREE(img);
    if (g8) ALWAN_FREE(g8);
    if (g8b) ALWAN_FREE(g8b);
    if (squares) ALWAN_FREE(squares);
    if (swatches) ALWAN_FREE(swatches);
    if (rects) ALWAN_FREE(rects);
    if (qbuf) ALWAN_FREE(qbuf);
    if (work) ALWAN_FREE(work);
    if (slo) ALWAN_FREE(slo);
    if (scaled) ALWAN_FREE(scaled);
    if (scaled_ptrs) ALWAN_FREE(scaled_ptrs);
    if (npts) ALWAN_FREE(npts);
    ccd_polys_free(&contours);
    ccd_polys_free(&clusters);
    return st;
}

static alwan_status ccd_entry(alwan_checker_detection *out, size_t capacity, size_t *count, void const *image,
                              size_t row_stride, size_t width, size_t height, size_t channels, int is_f64,
                              alwan_checker_detect_method method, alwan_checker_detect_params const *params) {
    ccd_settings s;
    alwan_status st;
    float *buf;
    size_t x, y;
    if (!count || !image || (capacity > 0 && !out)) return ALWAN_E_INVALID;
    *count = 0;
    if (method != ALWAN_CHECKER_DETECT_SEGMENTATION) return ALWAN_E_INVALID;
    if (width < 2 || height < 2 || channels < 3 || width > (size_t)INT_MAX / 4 || height > (size_t)INT_MAX / 4)
        return ALWAN_E_INVALID;
    if (row_stride == 0) row_stride = width * channels * (is_f64 ? sizeof(alwan_f64) : sizeof(alwan_f32));
    if (row_stride < width * channels * (is_f64 ? sizeof(alwan_f64) : sizeof(alwan_f32))) return ALWAN_E_INVALID;
    st = ccd_settings_from(&s, params);
    if (st != ALWAN_OK) return st;
    buf = (float *)ALWAN_ALLOC(alwan_safe_array_size(width * height, 3 * sizeof(float)), 64);
    if (!buf) return ALWAN_E_NOMEM;
    for (y = 0; y < height; y++) {
        unsigned char const *row = (unsigned char const *)image + y * row_stride;
        for (x = 0; x < width; x++) {
            size_t c;
            for (c = 0; c < 3; c++) {
                float v = is_f64 ? (float)((alwan_f64 const *)row)[x * channels + c] : ((alwan_f32 const *)row)[x * channels + c];
                buf[(y * width + x) * 3 + c] = v;
            }
        }
    }
    st = ccd_detect(out, capacity, count, buf, (int)width, (int)height, &s);
    ALWAN_FREE(buf);
    return st;
}

alwan_status alwan_color_checker_detect_f32(alwan_checker_detection *out, size_t capacity, size_t *count,
                                            alwan_f32 const *image, size_t row_stride, size_t width, size_t height,
                                            size_t channels, alwan_checker_detect_method method,
                                            alwan_checker_detect_params const *params) {
    return ccd_entry(out, capacity, count, image, row_stride, width, height, channels, 0, method, params);
}

alwan_status alwan_color_checker_detect_f64(alwan_checker_detection *out, size_t capacity, size_t *count,
                                            alwan_f64 const *image, size_t row_stride, size_t width, size_t height,
                                            size_t channels, alwan_checker_detect_method method,
                                            alwan_checker_detect_params const *params) {
    return ccd_entry(out, capacity, count, image, row_stride, width, height, channels, 1, method, params);
}
