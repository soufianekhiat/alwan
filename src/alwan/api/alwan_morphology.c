/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_morphology_{T} and _u8: grey-level mathematical morphology, as OpenCV's erode,
 * dilate and morphologyEx compute it, which suite 217 holds this to value for value.
 *
 * Erosion is the minimum and dilation the maximum over the structuring element placed with
 * its anchor, (kernel_width / 2, kernel_height / 2), on the pixel: out(x, y) = min or max of
 * src(x + j - ax, y + i - ay) over the element's non-zero (i, j), the element used as given,
 * not reflected, as OpenCV uses it. Pixels outside the image take no part, which is
 * OpenCV's default border for both. `iterations` repeats each erosion and dilation. The
 * composites:
 *
 *   OPEN      erode, then dilate          removes bright specks smaller than the element
 *   CLOSE     dilate, then erode          fills dark holes and gaps smaller than it
 *   GRADIENT  dilate - erode              the outline of every edge
 *   TOP_HAT   src - open                  the bright detail smaller than the element
 *   BLACK_HAT close - src                 the dark detail smaller than it
 *
 * The elements are OpenCV's getStructuringElement: RECT, CROSS through the anchor, ELLIPSE
 * rasterised row by row with half-width round(c sqrt((r^2 - dy^2) / r^2)), r = h / 2 and
 * c = w / 2, and DIAMOND |dx| + |dy| <= r in rows; or the caller's own mask.
 *
 * AREA_OPEN and AREA_CLOSE are connected operators, as scikit-image's area_opening and
 * area_closing (suite 219). The max-tree of the image (Berger et al. 2007: pixels taken in
 * decreasing order, each joined by union-find to the regions of its processed neighbours,
 * then made canonical so one node stands for each connected set at one level) gives every
 * region its area; a node of fewer than area_threshold pixels takes its parent's output,
 * from the root down, so a pixel ends at the highest level at which its region is large
 * enough. AREA_CLOSE is the same on the negated image. The root keeps its level whatever
 * its area; scikit-image sets it to 0 when the threshold exceeds the image. DIAMETER_OPEN
 * and DIAMETER_CLOSE are the same tree with each node's bounding box in place of its area,
 * judged by max(width, height), as scikit-image's diameter_opening (suite 220).
 *
 * FILL_HOLES is a priority flood: the border pixels enter a min-heap at their values, and
 * each pixel taken from it gives its unvisited neighbours max(their value, its own). A pixel
 * ends at the least, over paths to the border, of the highest value along the path, which
 * is the reconstruction by erosion of the image from a seed that is the image on the border
 * and its maximum inside, as scikit-image's morphology.reconstruction computes it.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void alwan_mo_element(unsigned char *el, size_t kw, size_t kh, alwan_morphology_shape shape) {
    long const r = (long)(kh / 2), c = (long)(kw / 2);
    double const inv_r2 = r ? 1.0 / ((double)r * (double)r) : 0.0;
    size_t i, j;
    if (kw == 1 && kh == 1) shape = ALWAN_MORPHOLOGY_RECT;
    for (i = 0; i < kh; i++) {
        long j1 = 0, j2 = 0;
        if (shape == ALWAN_MORPHOLOGY_RECT || (shape == ALWAN_MORPHOLOGY_CROSS && (long)i == r)) {
            j2 = (long)kw;
        } else if (shape == ALWAN_MORPHOLOGY_CROSS) {
            j1 = c;
            j2 = c + 1;
        } else if (shape == ALWAN_MORPHOLOGY_DIAMOND) {
            long const dy = labs((long)i - r);
            if (dy <= r) {
                long const dx = r - dy;
                j1 = c - dx > 0 ? c - dx : 0;
                j2 = c + dx + 1 < (long)kw ? c + dx + 1 : (long)kw;
            }
        } else {   /* ELLIPSE */
            long const dy = (long)i - r;
            if (labs(dy) <= r) {
                double const v = (double)c * sqrt((double)(r * r - dy * dy) * inv_r2);
                double const fl = floor(v), d = v - fl;   /* saturate_cast<int> is cvRound: half to even */
                long const dx = (long)((d > 0.5 || (d == 0.5 && fmod(fl, 2.0) != 0.0)) ? fl + 1.0 : fl);
                j1 = c - dx > 0 ? c - dx : 0;
                j2 = c + dx + 1 < (long)kw ? c + dx + 1 : (long)kw;
            }
        }
        for (j = 0; j < kw; j++) el[i * kw + j] = (unsigned char)((long)j >= j1 && (long)j < j2);
    }
}

/* One erosion (dilate 0) or dilation (1) of an h x w x ch plane set, in -> out. */
static void alwan_mo_pass(double *out, double const *in, size_t w, size_t h, size_t ch, unsigned char const *el, size_t kw,
                          size_t kh, int dilate) {
    long const ax = (long)(kw / 2), ay = (long)(kh / 2);
    size_t x, y, c, i, j;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            for (c = 0; c < ch; c++) {
                double v = dilate ? -DBL_MAX : DBL_MAX;
                int any = 0;
                for (i = 0; i < kh; i++) {
                    long const sy = (long)y + (long)i - ay;
                    if (sy < 0 || sy >= (long)h) continue;
                    for (j = 0; j < kw; j++) {
                        long const sx = (long)x + (long)j - ax;
                        double s;
                        if (!el[i * kw + j] || sx < 0 || sx >= (long)w) continue;
                        s = in[((size_t)sy * w + (size_t)sx) * ch + c];
                        if (dilate ? s > v : s < v) v = s;
                        any = 1;
                    }
                }
                /* an element with no pixel inside the image leaves the value (OpenCV's border
                 * value is the operation's identity) */
                out[(y * w + x) * ch + c] = any ? v : in[(y * w + x) * ch + c];
            }
        }
    }
}

/* iterations passes, in place through tmp. */
static void alwan_mo_repeat(double *img, double *tmp, size_t w, size_t h, size_t ch, unsigned char const *el, size_t kw,
                            size_t kh, int dilate, size_t iterations) {
    size_t k;
    for (k = 0; k < iterations; k++) {
        alwan_mo_pass(tmp, img, w, h, ch, el, kw, kh, dilate);
        memcpy(img, tmp, w * h * ch * sizeof(double));
    }
}

typedef struct {
    double v;
    size_t i;
} alwan_mo_px;

/* Decreasing value; ties by index, so the order (and the tree) does not depend on qsort. */
static int alwan_mo_cmp(void const *a, void const *b) {
    alwan_mo_px const *p = (alwan_mo_px const *)a, *q = (alwan_mo_px const *)b;
    if (p->v != q->v) return p->v > q->v ? -1 : 1;
    return p->i < q->i ? -1 : p->i > q->i ? 1 : 0;
}

static size_t alwan_mo_find(size_t *zpar, size_t p) {
    size_t r = p, t;
    while (zpar[r] != r) r = zpar[r];
    while (zpar[p] != r) {   /* path compression */
        t = zpar[p];
        zpar[p] = r;
        p = t;
    }
    return r;
}

/* Area (diameter 0) or diameter (1) opening of one w x h plane f (row-major, contiguous)
 * into out. work holds 7 w h size_t and px w h entries. */
static void alwan_mo_area_plane(double *out, double const *f, size_t w, size_t h, size_t threshold, int eight,
                                int diameter, size_t *work, alwan_mo_px *px) {
    size_t const n = w * h;
    size_t *parent = work, *zpar = work + n, *area = work + 2 * n;
    size_t *x0 = work + 3 * n, *x1 = work + 4 * n, *y0 = work + 5 * n, *y1 = work + 6 * n;
    size_t k;
    static int const dx[8] = { -1, 1, 0, 0, -1, 1, -1, 1 }, dy[8] = { 0, 0, -1, 1, -1, -1, 1, 1 };
    int const nn = eight ? 8 : 4;
    for (k = 0; k < n; k++) {
        px[k].v = f[k];
        px[k].i = k;
        zpar[k] = (size_t)-1;   /* not yet processed */
    }
    qsort(px, n, sizeof(*px), alwan_mo_cmp);
    for (k = 0; k < n; k++) {
        size_t const p = px[k].i, x = p % w, y = p / w;
        int d;
        parent[p] = p;
        zpar[p] = p;
        area[p] = 1;
        x0[p] = x1[p] = x;
        y0[p] = y1[p] = y;
        for (d = 0; d < nn; d++) {
            long const sx = (long)x + dx[d], sy = (long)y + dy[d];
            size_t q, r;
            if (sx < 0 || sy < 0 || sx >= (long)w || sy >= (long)h) continue;
            q = (size_t)sy * w + (size_t)sx;
            if (zpar[q] == (size_t)-1) continue;
            r = alwan_mo_find(zpar, q);
            if (r != p) {
                parent[r] = p;
                zpar[r] = p;
            }
        }
    }
    /* canonical: every pixel's parent is the node that stands for its level set */
    for (k = n; k-- > 0;) {
        size_t const p = px[k].i, q = parent[p];
        if (f[parent[q]] == f[q]) parent[p] = parent[q];
    }
    /* areas and bounding boxes, children before parents */
    for (k = 0; k + 1 < n; k++) {
        size_t const p = px[k].i, q = parent[p];
        area[q] += area[p];
        if (x0[p] < x0[q]) x0[q] = x0[p];
        if (x1[p] > x1[q]) x1[q] = x1[p];
        if (y0[p] < y0[q]) y0[q] = y0[p];
        if (y1[p] > y1[q]) y1[q] = y1[p];
    }
    if (diameter) {
        for (k = 0; k < n; k++) {
            size_t const bw = x1[k] - x0[k], bh = y1[k] - y0[k];
            area[k] = (bw > bh ? bw : bh) + 1;
        }
    }
    /* filter, root first: the root keeps its level */
    for (k = n; k-- > 0;) {
        size_t const p = px[k].i, q = parent[p];
        if (q == p) out[p] = f[p];
        else if (f[q] == f[p]) out[p] = out[q];   /* a pixel of its node's level */
        else out[p] = area[p] >= threshold ? f[p] : out[q];
    }
}

static alwan_status alwan_mo_area(double *a, size_t w, size_t h, size_t ch, size_t threshold, int eight, int close,
                                  int diameter) {
    size_t const n = w * h;
    size_t *work = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(n, 7 * sizeof(size_t)), sizeof(size_t));
    alwan_mo_px *px = (alwan_mo_px *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(alwan_mo_px)), sizeof(double));
    double *f = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, 2 * sizeof(double)), sizeof(double));
    size_t c, i;
    if (!work || !px || !f) {
        ALWAN_FREE(work);
        ALWAN_FREE(px);
        ALWAN_FREE(f);
        return ALWAN_E_NOMEM;
    }
    for (c = 0; c < ch; c++) {
        double *o = f + n;
        for (i = 0; i < n; i++) f[i] = close ? -a[i * ch + c] : a[i * ch + c];
        alwan_mo_area_plane(o, f, w, h, threshold, eight, diameter, work, px);
        for (i = 0; i < n; i++) a[i * ch + c] = close ? -o[i] : o[i];
    }
    ALWAN_FREE(work);
    ALWAN_FREE(px);
    ALWAN_FREE(f);
    return ALWAN_OK;
}

/* Min-heap of pixels by value, ties by index. */
static void alwan_mo_heap_push(alwan_mo_px *hp, size_t *len, alwan_mo_px e) {
    size_t i = (*len)++;
    while (i > 0) {
        size_t const up = (i - 1) / 2;
        if (hp[up].v < e.v || (hp[up].v == e.v && hp[up].i < e.i)) break;
        hp[i] = hp[up];
        i = up;
    }
    hp[i] = e;
}

static alwan_mo_px alwan_mo_heap_pop(alwan_mo_px *hp, size_t *len) {
    alwan_mo_px const top = hp[0], e = hp[--(*len)];
    size_t i = 0;
    for (;;) {
        size_t c = 2 * i + 1;
        if (c >= *len) break;
        if (c + 1 < *len && (hp[c + 1].v < hp[c].v || (hp[c + 1].v == hp[c].v && hp[c + 1].i < hp[c].i))) c++;
        if (e.v < hp[c].v || (e.v == hp[c].v && e.i < hp[c].i)) break;
        hp[i] = hp[c];
        i = c;
    }
    hp[i] = e;
    return top;
}

/* Fill holes of every channel of a (w x h x ch) in place: a priority flood from the border. */
static alwan_status alwan_mo_fill(double *a, size_t w, size_t h, size_t ch, int eight) {
    size_t const n = w * h;
    alwan_mo_px *hp = (alwan_mo_px *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(alwan_mo_px)), sizeof(double));
    unsigned char *seen = (unsigned char *)ALWAN_ALLOC(n, sizeof(double));
    static int const dx[8] = { -1, 1, 0, 0, -1, 1, -1, 1 }, dy[8] = { 0, 0, -1, 1, -1, -1, 1, 1 };
    int const nn = eight ? 8 : 4;
    size_t c, i;
    if (!hp || !seen) {
        ALWAN_FREE(hp);
        ALWAN_FREE(seen);
        return ALWAN_E_NOMEM;
    }
    for (c = 0; c < ch; c++) {
        size_t len = 0;
        memset(seen, 0, n);
        for (i = 0; i < n; i++) {
            size_t const x = i % w, y = i / w;
            if (x == 0 || y == 0 || x + 1 == w || y + 1 == h) {
                alwan_mo_px e;
                e.v = a[i * ch + c];
                e.i = i;
                seen[i] = 1;
                alwan_mo_heap_push(hp, &len, e);
            }
        }
        while (len) {
            alwan_mo_px const e = alwan_mo_heap_pop(hp, &len);
            size_t const x = e.i % w, y = e.i / w;
            int d;
            for (d = 0; d < nn; d++) {
                long const sx = (long)x + dx[d], sy = (long)y + dy[d];
                size_t q;
                alwan_mo_px o;
                if (sx < 0 || sy < 0 || sx >= (long)w || sy >= (long)h) continue;
                q = (size_t)sy * w + (size_t)sx;
                if (seen[q]) continue;
                seen[q] = 1;
                if (a[q * ch + c] < e.v) a[q * ch + c] = e.v;
                o.v = a[q * ch + c];
                o.i = q;
                alwan_mo_heap_push(hp, &len, o);
            }
        }
    }
    ALWAN_FREE(hp);
    ALWAN_FREE(seen);
    return ALWAN_OK;
}

/* Zhang and Suen's thinning ("A fast parallel algorithm for thinning digital patterns",
 * CACM 27(3), 1984) as scikit-image's _fast_skeletonize (BSD) runs it: the neighbourhood
 * number NW 1, N 2, NE 4, E 8, SE 16, S 32, SW 64, W 128, its class from this table (1 and
 * 3 removed in the first pass, 2 and 3 in the second), every pass reading the image as the
 * pass began. Transcribed from skimage/morphology/_skeletonize_various_cy.pyx at v0.26.0. */
static unsigned char const alwan_mo_zhang_lut[256] = {
    0, 0, 0, 1, 0, 0, 1, 3, 0, 0, 3, 1, 1, 0, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 3, 0, 3, 3,
    0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 3, 0, 2, 2,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    2, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 3, 0, 2, 0,
    0, 0, 3, 1, 0, 0, 1, 3, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    2, 3, 1, 3, 0, 0, 1, 3, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    2, 3, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 1, 0, 0, 0, 0, 2, 2, 0, 0, 2, 0, 0, 0
};

/* Guo and Hall's two-subiteration thinning ("Parallel thinning with two-subiteration
 * algorithms", CACM 32(3), 1989), as scikit-image's thin: the tables built from the paper's
 * conditions G1, G2 and G3 (G3' for the second), the neighbourhood numbered E 1, NE 2, N 4,
 * NW 8, W 16, SW 32, S 64, SE 128. */
static void alwan_mo_guo_hall_luts(unsigned char *g123, unsigned char *g123p) {
    int nb;
    for (nb = 0; nb < 256; nb++) {
        int b[8], i, s = 0, n1 = 0, n2 = 0, g1, g2, g3, g3p;
        for (i = 0; i < 8; i++) b[i] = (nb >> i) & 1;
        for (i = 0; i < 8; i += 2)
            if (!b[i] && (b[i + 1] || b[(i + 2) % 8])) s++;
        g1 = s == 1;
        for (i = 1; i < 8; i += 2) {
            n1 += b[i] || b[i - 1];
            n2 += b[i] || b[(i + 1) % 8];
        }
        g2 = (n1 < n2 ? n1 : n2) == 2 || (n1 < n2 ? n1 : n2) == 3;
        g3 = !((b[1] || b[2] || !b[7]) && b[0]);
        g3p = !((b[5] || b[6] || !b[3]) && b[4]);
        g123[nb] = (unsigned char)(g1 && g2 && g3);
        g123p[nb] = (unsigned char)(g1 && g2 && g3p);
    }
}

/* SKELETONIZE (thin = 0) or THIN, every channel of a (w x h x ch) on its own: a non-zero
 * value is foreground, a removed pixel becomes 0 and a kept one keeps its value. THIN stops
 * after max_iter iterations when that is not 0. */
static alwan_status alwan_mo_skeleton(double *a, size_t w, size_t h, size_t ch, int thin, size_t max_iter) {
    size_t const W = w + 2, H = h + 2;
    unsigned char *s = (unsigned char *)ALWAN_ALLOC(alwan_safe_array_size(W * H, 2), 1), *d;
    unsigned char g123[256], g123p[256];
    size_t c, x, y;
    if (!s) return ALWAN_E_NOMEM;
    d = s + W * H;
    if (thin) alwan_mo_guo_hall_luts(g123, g123p);
    for (c = 0; c < ch; c++) {
        size_t iter = 0;
        int changed = 1;
        /* a one-pixel border of background, as both do */
        memset(s, 0, W * H);
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) s[(y + 1) * W + x + 1] = (unsigned char)(a[(y * w + x) * ch + c] != 0.0);
        while (changed && (!thin || max_iter == 0 || iter < max_iter)) {
            int pass;
            changed = 0;
            for (pass = 0; pass < 2; pass++) {
                memcpy(d, s, W * H);
                for (y = 1; y + 1 < H; y++)
                    for (x = 1; x + 1 < W; x++) {
                        unsigned char const *q = s + y * W + x;
                        if (!*q) continue;
                        if (thin) {
                            int const nb = q[1] | q[-(long)W + 1] << 1 | q[-(long)W] << 2 | q[-(long)W - 1] << 3 | q[-1] << 4 |
                                           q[W - 1] << 5 | q[W] << 6 | q[W + 1] << 7;
                            if ((pass == 0 ? g123 : g123p)[nb]) d[y * W + x] = 0, changed = 1;
                        } else {
                            int const k = alwan_mo_zhang_lut[q[-(long)W - 1] | q[-(long)W] << 1 | q[-(long)W + 1] << 2 | q[1] << 3 |
                                                             q[W + 1] << 4 | q[W] << 5 | q[W - 1] << 6 | q[-1] << 7];
                            if (k == 3 || (k == 1 && pass == 0) || (k == 2 && pass == 1)) d[y * W + x] = 0, changed = 1;
                        }
                    }
                memcpy(s, d, W * H);
            }
            iter++;
        }
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
                if (!s[(y + 1) * W + x + 1]) a[(y * w + x) * ch + c] = 0.0;
    }
    ALWAN_FREE(s);
    return ALWAN_OK;
}

static alwan_status alwan_mo_run(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 alwan_morphology_method method, alwan_morphology_params const *params,
                                 int kind /* 0 f64, 1 f32, 2 u8 */) {
    alwan_morphology_params const zero = { 0 };
    alwan_morphology_params const *p = params ? params : &zero;
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    size_t const kw = p->kernel_width ? p->kernel_width : 3, kh = p->kernel_height ? p->kernel_height : 3;
    size_t const iterations = p->iterations ? p->iterations : 1;
    size_t const n = w * h * ch;
    double *a, *b, *t;
    unsigned char *el;
    size_t x, y, i;
    if (!out || !src || w == 0 || h == 0 || ch == 0 || ch > 4 || n / ch / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / elem / ch < w) return ALWAN_E_INVALID;
    if ((unsigned)method > (unsigned)ALWAN_MORPHOLOGY_THIN) return ALWAN_E_INVALID;
    if (p->connectivity != 0 && p->connectivity != 4 && p->connectivity != 8) return ALWAN_E_INVALID;
    if (!p->kernel && (unsigned)p->shape > (unsigned)ALWAN_MORPHOLOGY_DIAMOND) return ALWAN_E_INVALID;
    if (kw > 255 || kh > 255 || iterations > 1000) return ALWAN_E_RANGE;
    a = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, 3 * sizeof(double)) + kw * kh, sizeof(double));
    if (!a) return ALWAN_E_NOMEM;
    b = a + n;
    t = b + n;
    el = (unsigned char *)(t + n);
    if (p->kernel) {
        for (i = 0; i < kw * kh; i++) el[i] = (unsigned char)(p->kernel[i] != 0);
    } else {
        alwan_mo_element(el, kw, kh, p->shape);
    }
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        for (x = 0; x < w * ch; x++) {
            double const v = kind == 0 ? ((alwan_f64 const *)row)[x] : kind == 1 ? (double)((alwan_f32 const *)row)[x]
                                                                                 : (double)((unsigned char const *)row)[x];
            if (!(v == v)) {
                ALWAN_FREE(a);
                return ALWAN_E_INVALID;
            }
            a[y * w * ch + x] = v;
        }
    }
    memcpy(b, a, n * sizeof(double));   /* b keeps the source */
    switch (method) {
    case ALWAN_MORPHOLOGY_ERODE:
        alwan_mo_repeat(a, t, w, h, ch, el, kw, kh, 0, iterations);
        break;
    case ALWAN_MORPHOLOGY_DILATE:
        alwan_mo_repeat(a, t, w, h, ch, el, kw, kh, 1, iterations);
        break;
    case ALWAN_MORPHOLOGY_OPEN:
    case ALWAN_MORPHOLOGY_TOP_HAT:
        alwan_mo_repeat(a, t, w, h, ch, el, kw, kh, 0, iterations);
        alwan_mo_repeat(a, t, w, h, ch, el, kw, kh, 1, iterations);
        if (method == ALWAN_MORPHOLOGY_TOP_HAT) for (i = 0; i < n; i++) a[i] = b[i] - a[i];
        break;
    case ALWAN_MORPHOLOGY_CLOSE:
    case ALWAN_MORPHOLOGY_BLACK_HAT:
        alwan_mo_repeat(a, t, w, h, ch, el, kw, kh, 1, iterations);
        alwan_mo_repeat(a, t, w, h, ch, el, kw, kh, 0, iterations);
        if (method == ALWAN_MORPHOLOGY_BLACK_HAT) for (i = 0; i < n; i++) a[i] = a[i] - b[i];
        break;
    case ALWAN_MORPHOLOGY_GRADIENT:
        alwan_mo_repeat(a, t, w, h, ch, el, kw, kh, 1, iterations);   /* a: dilated */
        alwan_mo_repeat(b, t, w, h, ch, el, kw, kh, 0, iterations);   /* b: eroded */
        for (i = 0; i < n; i++) a[i] = a[i] - b[i];
        break;
    case ALWAN_MORPHOLOGY_AREA_OPEN:
    case ALWAN_MORPHOLOGY_AREA_CLOSE:
    case ALWAN_MORPHOLOGY_DIAMETER_OPEN:
    case ALWAN_MORPHOLOGY_DIAMETER_CLOSE:
    case ALWAN_MORPHOLOGY_SKELETONIZE:
    case ALWAN_MORPHOLOGY_THIN: {
        alwan_status const st = alwan_mo_skeleton(a, w, h, ch, method == ALWAN_MORPHOLOGY_THIN, p->iterations);
        if (st != ALWAN_OK) {
            ALWAN_FREE(a);
            return st;
        }
        break;
    }
    case ALWAN_MORPHOLOGY_FILL_HOLES: {
        int const diameter = method == ALWAN_MORPHOLOGY_DIAMETER_OPEN || method == ALWAN_MORPHOLOGY_DIAMETER_CLOSE;
        alwan_status const st =
            method == ALWAN_MORPHOLOGY_FILL_HOLES
                ? alwan_mo_fill(a, w, h, ch, p->connectivity == 8)
                : alwan_mo_area(a, w, h, ch,
                                diameter ? (p->diameter_threshold ? p->diameter_threshold : 8)
                                         : (p->area_threshold ? p->area_threshold : 64),
                                p->connectivity == 8,
                                method == ALWAN_MORPHOLOGY_AREA_CLOSE || method == ALWAN_MORPHOLOGY_DIAMETER_CLOSE, diameter);
        if (st != ALWAN_OK) {
            ALWAN_FREE(a);
            return st;
        }
        break;
    }
    }
    for (y = 0; y < h; y++) {
        char *row = (char *)out + y * out_rs;
        for (x = 0; x < w * ch; x++) {
            double const v = a[y * w * ch + x];
            if (kind == 0) ((alwan_f64 *)row)[x] = v;
            else if (kind == 1) ((alwan_f32 *)row)[x] = (alwan_f32)v;
            else ((unsigned char *)row)[x] = (unsigned char)(v < 0.0 ? 0.0 : v > 255.0 ? 255.0 : v);   /* saturating */
        }
    }
    ALWAN_FREE(a);
    return ALWAN_OK;
}

alwan_status alwan_morphology_u8(unsigned char *out, size_t out_row_stride, unsigned char const *src, size_t src_row_stride,
                                 size_t channels, size_t width, size_t height, alwan_morphology_method method,
                                 alwan_morphology_params const *params) {
    return alwan_mo_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 2);
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_morphology_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride,
                                  size_t channels, size_t width, size_t height, alwan_morphology_method method,
                                  alwan_morphology_params const *params) {
    return alwan_mo_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_morphology_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride,
                                  size_t channels, size_t width, size_t height, alwan_morphology_method method,
                                  alwan_morphology_params const *params) {
    return alwan_mo_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 1);
}
#endif
