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
 *
 * alwan_reconstruct_{T} is that reconstruction in general, by dilation or erosion from a
 * caller's seed under a caller's mask through the element, as scikit-image's
 * reconstruction (suite 250). H_MAXIMA and H_MINIMA build on it as scikit-image's h_maxima
 * and h_minima; LOCAL_MAXIMA and LOCAL_MINIMA are its plateau test, local_maxima and
 * local_minima.
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
                double const v = (double)c * ALWAN_SQRT_F64((double)(r * r - dy * dy) * inv_r2);
                double const fl = ALWAN_FLOOR_F64(v), d = v - fl;   /* saturate_cast<int> is cvRound: half to even */
                long const dx = (long)((d > 0.5 || (d == 0.5 && ALWAN_FMOD_F64(fl, 2.0) != 0.0)) ? fl + 1.0 : fl);
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

/* The element's offsets for reconstruction and the extrema: (row, column) of every non-zero
 * cell less the anchor, the anchor itself left out. Returns their count. */
static size_t alwan_mo_offsets(long *off, unsigned char const *el, size_t kw, size_t kh) {
    long const ax = (long)(kw / 2), ay = (long)(kh / 2);
    size_t i, j, k = 0;
    for (i = 0; i < kh; i++)
        for (j = 0; j < kw; j++) {
            if (!el[i * kw + j] || ((long)i == ay && (long)j == ax)) continue;
            off[2 * k] = (long)i - ay;
            off[2 * k + 1] = (long)j - ax;
            k++;
        }
    return k;
}

/* Grey reconstruction by dilation of one w x h plane: rec (the seed on entry, at or below
 * mask everywhere) grows to the least fixed point of rec = min(mask, max(rec, max over the
 * offsets d of rec(q - d))), values flowing from p to p + d as scikit-image's
 * reconstruction moves them. Vincent's hybrid (1993): a raster pass taking the offsets that
 * point forward, an anti-raster pass the ones that point back, then a FIFO from every pixel
 * that can still raise a neighbour. The fixed point is unique, whatever the order;
 * scikit-image reaches it through a sorted linked list. q (w h entries) and inq (w h bytes)
 * are workspace. */
static void alwan_mo_reconstruct_plane(double *rec, double const *mask, size_t w, size_t h, long const *off, size_t no,
                                       size_t *q, unsigned char *inq) {
    size_t const n = w * h;
    size_t head = 0, tail = 0, used = 0, k, x, y;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            size_t const p = y * w + x;
            double v = rec[p];
            for (k = 0; k < no; k++) {
                long const dr = off[2 * k], dc = off[2 * k + 1];
                long sy, sx;
                if (!(dr > 0 || (dr == 0 && dc > 0))) continue;   /* the source q - d comes earlier */
                sy = (long)y - dr;
                sx = (long)x - dc;
                if (sy < 0 || sx < 0 || sy >= (long)h || sx >= (long)w) continue;
                if (rec[(size_t)sy * w + (size_t)sx] > v) v = rec[(size_t)sy * w + (size_t)sx];
            }
            rec[p] = v < mask[p] ? v : mask[p];
        }
    for (y = h; y-- > 0;)
        for (x = w; x-- > 0;) {
            size_t const p = y * w + x;
            double v = rec[p];
            for (k = 0; k < no; k++) {
                long const dr = off[2 * k], dc = off[2 * k + 1];
                long sy, sx;
                if (!(dr < 0 || (dr == 0 && dc < 0))) continue;   /* the source comes later */
                sy = (long)y - dr;
                sx = (long)x - dc;
                if (sy < 0 || sx < 0 || sy >= (long)h || sx >= (long)w) continue;
                if (rec[(size_t)sy * w + (size_t)sx] > v) v = rec[(size_t)sy * w + (size_t)sx];
            }
            rec[p] = v < mask[p] ? v : mask[p];
        }
    memset(inq, 0, n);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            size_t const p = y * w + x;
            for (k = 0; k < no; k++) {
                long const sy = (long)y + off[2 * k], sx = (long)x + off[2 * k + 1];
                size_t r;
                if (sy < 0 || sx < 0 || sy >= (long)h || sx >= (long)w) continue;
                r = (size_t)sy * w + (size_t)sx;
                if (rec[r] < rec[p] && rec[r] < mask[r]) {
                    q[tail] = p;
                    tail = (tail + 1) % n;
                    used++;
                    inq[p] = 1;
                    break;
                }
            }
        }
    while (used) {
        size_t const p = q[head], py = p / w, px = p % w;
        head = (head + 1) % n;
        used--;
        inq[p] = 0;
        for (k = 0; k < no; k++) {
            long const sy = (long)py + off[2 * k], sx = (long)px + off[2 * k + 1];
            size_t r;
            if (sy < 0 || sx < 0 || sy >= (long)h || sx >= (long)w) continue;
            r = (size_t)sy * w + (size_t)sx;
            if (rec[r] < rec[p] && rec[r] < mask[r]) {
                rec[r] = rec[p] < mask[r] ? rec[p] : mask[r];
                if (!inq[r]) {
                    q[tail] = r;
                    tail = (tail + 1) % n;
                    used++;
                    inq[r] = 1;
                }
            }
        }
    }
}

/* Reconstruction of every channel: rec holds the seed, mk the mask, both w x h x ch
 * interleaved; by erosion when erode is set (negated, reconstructed by dilation and negated
 * back, which is exact). */
static alwan_status alwan_mo_reconstruct(double *rec, double const *mk, size_t w, size_t h, size_t ch, long const *off,
                                         size_t no, int erode) {
    size_t const n = w * h;
    double *r = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, 2 * sizeof(double)), sizeof(double));
    size_t *q = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(size_t)), sizeof(size_t));
    unsigned char *inq = (unsigned char *)ALWAN_ALLOC(n, 1);
    size_t c, i;
    if (!r || !q || !inq) {
        ALWAN_FREE(r);
        ALWAN_FREE(q);
        ALWAN_FREE(inq);
        return ALWAN_E_NOMEM;
    }
    for (c = 0; c < ch; c++) {
        double *m = r + n;
        for (i = 0; i < n; i++) {
            r[i] = erode ? -rec[i * ch + c] : rec[i * ch + c];
            m[i] = erode ? -mk[i * ch + c] : mk[i * ch + c];
        }
        alwan_mo_reconstruct_plane(r, m, w, h, off, no, q, inq);
        for (i = 0; i < n; i++) rec[i * ch + c] = erode ? -r[i] : r[i];
    }
    ALWAN_FREE(r);
    ALWAN_FREE(q);
    ALWAN_FREE(inq);
    return ALWAN_OK;
}

/* H_MAXIMA (minima 0) or H_MINIMA of every channel, in place: 1 where a maximum (minimum)
 * of dynamic h or more, else 0, as scikit-image's h_maxima and h_minima. The shifted seed
 * is built in the image's own arithmetic: float32 for float32 (h rounded to float32, as NumPy
 * does with a Python float), less (plus) the resolution term 2 * finfo.resolution * |x| for
 * floats, a saturating shift for 8-bit data and an integral h; a fractional h takes 8-bit
 * data to float64. An h above the channel's range marks nothing. */
static alwan_status alwan_mo_hextrema(double *a, size_t w, size_t h, size_t ch, long const *off, size_t no, double hv,
                                      int minima, int kind) {
    size_t const n = w * h;
    double *s = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, 2 * ch * sizeof(double)), sizeof(double));
    unsigned char none[4];
    int const integral = kind == 2 && hv == ALWAN_FLOOR_F64(hv);
    float const hf = (float)hv;
    size_t c, i;
    alwan_status st;
    if (!s) return ALWAN_E_NOMEM;
    for (c = 0; c < ch; c++) {
        double lo = DBL_MAX, hi = -DBL_MAX;
        for (i = 0; i < n; i++) {
            double const x = a[i * ch + c];
            if (x < lo) lo = x;
            if (x > hi) hi = x;
        }
        none[c] = (unsigned char)(kind == 1 ? hf > (float)hi - (float)lo : hv > hi - lo);
        for (i = 0; i < n; i++) {
            double const x = a[i * ch + c];
            double sh;
            if (none[c]) {
                sh = x;   /* seed equal to the mask: nothing to reconstruct */
            } else if (integral) {
                sh = minima ? (x > 255.0 - hv ? 255.0 : x + hv) : (x < hv ? 0.0 : x - hv);
            } else if (kind == 1) {
                float const xf = (float)x, res = (2.0f * 1e-6f) * (xf < 0.0f ? -xf : xf);
                sh = minima ? (double)((float)(xf + hf) + res) : (double)((float)(xf - hf) - res);
            } else {
                double const res = 2e-15 * ALWAN_ABS_F64(x);
                sh = minima ? (x + hv) + res : (x - hv) - res;
            }
            s[i * ch + c] = sh;
            s[n * ch + i * ch + c] = x;
        }
    }
    st = alwan_mo_reconstruct(s, s + n * ch, w, h, ch, off, no, minima);
    if (st != ALWAN_OK) {
        ALWAN_FREE(s);
        return st;
    }
    for (c = 0; c < ch; c++) {
        for (i = 0; i < n; i++) {
            double const x = a[i * ch + c], r = s[i * ch + c];
            int on;
            if (none[c]) {
                on = 0;
            } else if (kind == 1) {
                float const res = minima ? (float)r - (float)x : (float)x - (float)r;
                on = res >= hf;
            } else {
                on = (minima ? r - x : x - r) >= hv;
            }
            a[i * ch + c] = on ? 1.0 : 0.0;
        }
    }
    ALWAN_FREE(s);
    return ALWAN_OK;
}

/* LOCAL_MAXIMA (minima 0) or LOCAL_MINIMA of every channel, in place: 1 on every plateau,
 * a set of equal pixels joined through the element, whose neighbours outside it are all
 * lower (higher), else 0, as scikit-image's local_maxima and local_minima. With borders
 * allowed a plateau may touch the edge unless it is at the channel's lowest (highest)
 * value, which scikit-image's padding equals; with exclude_borders a plateau touching the
 * edge is never one, and a channel under 3 pixels a side has none. */
static alwan_status alwan_mo_local_extrema(double *a, size_t w, size_t h, size_t ch, long const *off, size_t no, int minima,
                                           int exclude) {
    size_t const n = w * h;
    size_t *q = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(size_t)), sizeof(size_t));
    unsigned char *seen = (unsigned char *)ALWAN_ALLOC(alwan_safe_array_size(n, 2), 1);
    double *f = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
    size_t c, i, k;
    if (!q || !seen || !f) {
        ALWAN_FREE(q);
        ALWAN_FREE(seen);
        ALWAN_FREE(f);
        return ALWAN_E_NOMEM;
    }
    for (c = 0; c < ch; c++) {
        unsigned char *res = seen + n;
        double lo = DBL_MAX;
        for (i = 0; i < n; i++) {
            f[i] = minima ? -a[i * ch + c] : a[i * ch + c];
            if (f[i] < lo) lo = f[i];
        }
        memset(seen, 0, 2 * n);
        if (!(exclude && (w < 3 || h < 3))) {
            for (i = 0; i < n; i++) {
                size_t len = 0, at = 0;
                int higher = 0, border = 0;
                double const v = f[i];
                if (seen[i]) continue;
                seen[i] = 1;
                q[len++] = i;
                while (at < len) {
                    size_t const p = q[at++], py = p / w, px = p % w;
                    if (py == 0 || px == 0 || py + 1 == h || px + 1 == w) border = 1;
                    for (k = 0; k < no; k++) {
                        long const sy = (long)py + off[2 * k], sx = (long)px + off[2 * k + 1];
                        size_t r;
                        if (sy < 0 || sx < 0 || sy >= (long)h || sx >= (long)w) continue;
                        r = (size_t)sy * w + (size_t)sx;
                        if (f[r] > v) higher = 1;
                        else if (f[r] == v && !seen[r]) {
                            seen[r] = 1;
                            q[len++] = r;
                        }
                    }
                }
                if (!higher && !(border && (exclude || v <= lo)))
                    for (k = 0; k < len; k++) res[q[k]] = 1;
            }
        }
        for (i = 0; i < n; i++) a[i * ch + c] = res[i] ? 1.0 : 0.0;
    }
    ALWAN_FREE(q);
    ALWAN_FREE(seen);
    ALWAN_FREE(f);
    return ALWAN_OK;
}

/* ---- MEDIAL_AXIS, CONVEX_HULL, CONVEX_HULL_OBJECT ------------------------------------
 * Ported from scikit-image 0.26 (morphology/_skeletonize.py medial_axis and
 * _skeletonize_various_cy.pyx _skeletonize_loop; morphology/convex_hull.py and
 * _convex_hull.pyx possible_hull; _shared/geometry.pyx point_in_polygon; BSD-3-Clause,
 * Copyright the scikit-image team). */

/* The 3 x 3 configuration index of p in m (w x h, 0/1), bit k for row k / 3 - 1, column
 * k % 3 - 1, pixels outside the image 0, as scikit-image's tables number it. */
static unsigned alwan_mo_index9(unsigned char const *m, size_t w, size_t h, size_t y, size_t x) {
    unsigned acc = 0, bit = 1;
    size_t dy, dx;   /* the neighbour at (y + dy - 1, x + dx - 1) */
    for (dy = 0; dy < 3; dy++)
        for (dx = 0; dx < 3; dx++, bit <<= 1) {
            if (y + dy < 1 || x + dx < 1 || y + dy - 1 >= h || x + dx - 1 >= w) continue;
            if (m[(y + dy - 1) * w + (x + dx - 1)]) acc |= bit;
        }
    return acc;
}

/* 8-connected components of a 3 x 3 pattern (bit k at row k / 3, column k % 3). */
static int alwan_mo_comp9(unsigned idx) {
    int seen = 0, comps = 0, k;
    for (k = 0; k < 9; k++) {
        int stack[9], sp = 0;
        if (!(idx >> k & 1) || (seen >> k & 1)) continue;
        comps++;
        stack[sp++] = k;
        seen |= 1 << k;
        while (sp) {
            int const c = stack[--sp], r = c / 3, q = c % 3;
            int dr, dq;
            for (dr = -1; dr <= 1; dr++)
                for (dq = -1; dq <= 1; dq++) {
                    int const rr = r + dr, qq = q + dq, n = rr * 3 + qq;
                    if (rr < 0 || rr > 2 || qq < 0 || qq > 2) continue;
                    if ((idx >> n & 1) && !(seen >> n & 1)) {
                        seen |= 1 << n;
                        stack[sp++] = n;
                    }
                }
        }
    }
    return comps;
}

static int alwan_mo_popcount9(unsigned idx) {
    int c = 0;
    while (idx) c += (int)(idx & 1u), idx >>= 1;
    return c;
}

typedef struct {
    double dist;
    int corner;
    size_t pos;
} alwan_mo_ma;

static int alwan_mo_ma_cmp(void const *pa, void const *pb) {
    alwan_mo_ma const *a = (alwan_mo_ma const *)pa, *b = (alwan_mo_ma const *)pb;
    if (a->dist != b->dist) return a->dist < b->dist ? -1 : 1;
    if (a->corner != b->corner) return a->corner < b->corner ? -1 : 1;
    return a->pos < b->pos ? -1 : a->pos > b->pos ? 1 : 0;
}

/* MEDIAL_AXIS on every channel of a: kept pixels keep their value (or their distance). */
static alwan_status alwan_mo_medial(double *a, size_t w, size_t h, size_t ch, int want_distance) {
    size_t const n = w * h;
    unsigned char table[512];
    unsigned char *m = (unsigned char *)ALWAN_ALLOC(alwan_safe_array_size(n, 2), 1), *res;
    double *dist = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
    alwan_mo_ma *ord = (alwan_mo_ma *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(alwan_mo_ma)), sizeof(double));
    alwan_distance_params dp;
    size_t c, i, x, y;
    unsigned idx;
    if (!m || !dist || !ord) {
        ALWAN_FREE(ord);
        ALWAN_FREE(dist);
        ALWAN_FREE(m);
        return ALWAN_E_NOMEM;
    }
    res = m + n;
    for (idx = 0; idx < 512; idx++) {
        int const centre = (idx & 16u) != 0;
        int const split = alwan_mo_comp9(idx) != alwan_mo_comp9(idx & ~16u);
        table[idx] = (unsigned char)(centre && (split || alwan_mo_popcount9(idx) < 3));
    }
    memset(&dp, 0, sizeof(dp));
    for (c = 0; c < ch; c++) {
        size_t nf = 0, k;
        alwan_status st;
        for (i = 0; i < n; i++) m[i] = (unsigned char)(a[i * ch + c] != 0.0);
        st = alwan_distance_transform_u8(dist, w * sizeof(double), m, w, 1, w, h, ALWAN_DISTANCE_EUCLIDEAN, &dp);
        if (st != ALWAN_OK) {
            ALWAN_FREE(ord);
            ALWAN_FREE(dist);
            ALWAN_FREE(m);
            return st;
        }
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                size_t const pos = y * w + x;
                if (!m[pos]) continue;
                ord[nf].dist = dist[pos];
                ord[nf].corner = 9 - alwan_mo_popcount9(alwan_mo_index9(m, w, h, y, x));
                ord[nf].pos = pos;
                nf++;
            }
        qsort(ord, nf, sizeof(alwan_mo_ma), alwan_mo_ma_cmp);
        memcpy(res, m, n);
        for (k = 0; k < nf; k++) {
            size_t const pos = ord[k].pos;
            res[pos] = table[alwan_mo_index9(res, w, h, pos / w, pos % w)];
        }
        for (i = 0; i < n; i++) {
            double *v = &a[i * ch + c];
            if (!res[i]) *v = 0.0;
            else if (want_distance) *v = dist[i];
        }
    }
    ALWAN_FREE(ord);
    ALWAN_FREE(dist);
    ALWAN_FREE(m);
    return ALWAN_OK;
}

/* scikit-image's point_in_polygon on (row x, column y): 0 outside, 1 inside, 2 vertex, 3 edge. */
static int alwan_mo_pip(double const *xp, double const *yp, size_t nv, double x, double y) {
    double const eps = 1e-12;
    double x1 = xp[nv - 1] - x, y1 = yp[nv - 1] - y;
    unsigned l_cross = 0, r_cross = 0;
    size_t i;
    for (i = 0; i < nv; i++) {
        double const x0 = xp[i] - x, y0 = yp[i] - y;
        if (-eps < x0 && x0 < eps && -eps < y0 && y0 < eps) return 2;
        if ((y0 > 0) != (y1 > 0) && (x0 * y1 - x1 * y0) / (y1 - y0) > 0) r_cross++;
        if ((y0 < 0) != (y1 < 0) && (x0 * y1 - x1 * y0) / (y1 - y0) < 0) l_cross++;
        x1 = x0;
        y1 = y0;
    }
    if ((r_cross & 1) != (l_cross & 1)) return 3;
    return (r_cross & 1) ? 1 : 0;
}

static int alwan_mo_pt_cmp(void const *pa, void const *pb) {
    long long const *a = (long long const *)pa, *b = (long long const *)pb;
    if (a[0] != b[0]) return a[0] < b[0] ? -1 : 1;
    return a[1] < b[1] ? -1 : a[1] > b[1] ? 1 : 0;
}

static long long alwan_mo_cross(long long const *o, long long const *a, long long const *b) {
    return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
}

/* The convex hull of mask m (w x h, 0/1) into hull (1 inside or on it, else 0). With
 * P = 8 (w + h), the most candidate points: pts holds 2 P long longs, hv 2 (P + 2), vx and
 * vy P + 2 doubles each. */
static void alwan_mo_hull(unsigned char *hull, unsigned char const *m, size_t w, size_t h, long long *pts, long long *hv, double *vx,
                          double *vy) {
    size_t np = 0, nh = 0, i, k, x, y;
    long long rmin, rmax, cmin, cmax;
    memset(hull, 0, w * h);
    /* possible_hull: the first and last pixel of every row and column, each as the four
     * midpoints of its edges, in doubled coordinates */
    for (y = 0; y < h; y++) {
        size_t first = w, last = w;
        for (x = 0; x < w; x++)
            if (m[y * w + x]) {
                if (first == w) first = x;
                last = x;
            }
        if (first == w) continue;
        for (k = 0; k < 2; k++) {
            long long const r = 2 * (long long)y, q = 2 * (long long)(k ? last : first);
            long long const add[4][2] = { { r - 1, q }, { r + 1, q }, { r, q - 1 }, { r, q + 1 } };
            size_t j;
            for (j = 0; j < 4; j++) pts[2 * np] = add[j][0], pts[2 * np + 1] = add[j][1], np++;
        }
    }
    for (x = 0; x < w; x++) {
        size_t first = h, last = h;
        for (y = 0; y < h; y++)
            if (m[y * w + x]) {
                if (first == h) first = y;
                last = y;
            }
        if (first == h) continue;
        for (k = 0; k < 2; k++) {
            long long const r = 2 * (long long)(k ? last : first), q = 2 * (long long)x;
            long long const add[4][2] = { { r - 1, q }, { r + 1, q }, { r, q - 1 }, { r, q + 1 } };
            size_t j;
            for (j = 0; j < 4; j++) pts[2 * np] = add[j][0], pts[2 * np + 1] = add[j][1], np++;
        }
    }
    if (np == 0) return;
    /* Andrew's monotone chain, collinear points dropped: the same polygon Qhull returns */
    qsort(pts, np, 2 * sizeof(long long), alwan_mo_pt_cmp);
    for (i = 0; i < np; i++) {
        while (nh >= 2 && alwan_mo_cross(hv + 2 * (nh - 2), hv + 2 * (nh - 1), pts + 2 * i) <= 0) nh--;
        hv[2 * nh] = pts[2 * i], hv[2 * nh + 1] = pts[2 * i + 1], nh++;
    }
    {
        size_t const lower = nh + 1;
        for (i = np - 1; i-- > 0;) {
            while (nh >= lower && alwan_mo_cross(hv + 2 * (nh - 2), hv + 2 * (nh - 1), pts + 2 * i) <= 0) nh--;
            hv[2 * nh] = pts[2 * i], hv[2 * nh + 1] = pts[2 * i + 1], nh++;
        }
    }
    nh--;   /* the last point repeats the first */
    rmin = rmax = hv[0];
    cmin = cmax = hv[1];
    for (i = 0; i < nh; i++) {
        vx[i] = (double)hv[2 * i] / 2.0;
        vy[i] = (double)hv[2 * i + 1] / 2.0;
        if (hv[2 * i] < rmin) rmin = hv[2 * i];
        if (hv[2 * i] > rmax) rmax = hv[2 * i];
        if (hv[2 * i + 1] < cmin) cmin = hv[2 * i + 1];
        if (hv[2 * i + 1] > cmax) cmax = hv[2 * i + 1];
    }
    for (y = 0; y < h; y++) {
        if (2 * (long long)y < rmin || 2 * (long long)y > rmax) continue;   /* outside the box: outside */
        for (x = 0; x < w; x++) {
            if (2 * (long long)x < cmin || 2 * (long long)x > cmax) continue;
            hull[y * w + x] = (unsigned char)(alwan_mo_pip(vx, vy, nh, (double)y, (double)x) >= 1);
        }
    }
}

/* CONVEX_HULL (per_object 0) or CONVEX_HULL_OBJECT on every channel of a. */
static alwan_status alwan_mo_convex(double *a, size_t w, size_t h, size_t ch, int per_object, int conn8) {
    size_t const n = w * h, P = 8 * (w + h);
    unsigned char *m = (unsigned char *)ALWAN_ALLOC(alwan_safe_array_size(n, 4), 1), *hull, *obj, *acc;
    long long *pts = (long long *)ALWAN_ALLOC(alwan_safe_array_size(4 * P + 4, sizeof(long long)), sizeof(long long)), *hv;
    double *vx = (double *)ALWAN_ALLOC(alwan_safe_array_size(2 * P + 4, sizeof(double)), sizeof(double)), *vy;
    size_t *lab = per_object ? (size_t *)ALWAN_ALLOC(alwan_safe_array_size(n, 2 * sizeof(size_t)), sizeof(size_t)) : NULL, *stack;
    size_t c, i;
    if (!m || !pts || !vx || (per_object && !lab)) {
        ALWAN_FREE(lab);
        ALWAN_FREE(vx);
        ALWAN_FREE(pts);
        ALWAN_FREE(m);
        return ALWAN_E_NOMEM;
    }
    hull = m + n;
    obj = hull + n;
    acc = obj + n;
    hv = pts + 2 * P;
    vy = vx + P + 2;
    stack = lab ? lab + n : NULL;
    for (c = 0; c < ch; c++) {
        for (i = 0; i < n; i++) m[i] = (unsigned char)(a[i * ch + c] != 0.0);
        if (!per_object) {
            alwan_mo_hull(acc, m, w, h, pts, hv, vx, vy);
        } else {
            size_t nl = 0;
            memset(acc, 0, n);
            for (i = 0; i < n; i++) lab[i] = 0;
            for (i = 0; i < n; i++) {
                size_t sp = 0, j;
                if (!m[i] || lab[i]) continue;
                nl++;
                memset(obj, 0, n);
                lab[i] = nl;
                stack[sp++] = i;
                while (sp) {
                    size_t const q = stack[--sp], qy = q / w, qx = q % w;
                    long dy, dx;
                    obj[q] = 1;
                    for (dy = -1; dy <= 1; dy++)
                        for (dx = -1; dx <= 1; dx++) {
                            long const yy = (long)qy + dy, xx = (long)qx + dx;
                            size_t r;
                            if ((dy == 0 && dx == 0) || (!conn8 && dy != 0 && dx != 0)) continue;
                            if (yy < 0 || xx < 0 || yy >= (long)h || xx >= (long)w) continue;
                            r = (size_t)yy * w + (size_t)xx;
                            if (m[r] && !lab[r]) lab[r] = nl, stack[sp++] = r;
                        }
                }
                alwan_mo_hull(hull, obj, w, h, pts, hv, vx, vy);
                for (j = 0; j < n; j++) acc[j] |= hull[j];
            }
        }
        for (i = 0; i < n; i++) a[i * ch + c] = acc[i] ? 1.0 : 0.0;
    }
    ALWAN_FREE(lab);
    ALWAN_FREE(vx);
    ALWAN_FREE(pts);
    ALWAN_FREE(m);
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
    if ((unsigned)method > (unsigned)ALWAN_MORPHOLOGY_CONVEX_HULL_OBJECT) return ALWAN_E_INVALID;
    if ((method == ALWAN_MORPHOLOGY_H_MAXIMA || method == ALWAN_MORPHOLOGY_H_MINIMA) &&
        (p->h == 0.0 || !(p->h == p->h))) return ALWAN_E_INVALID;   /* h = 0 is ambiguous, as scikit-image says */
    if ((method == ALWAN_MORPHOLOGY_H_MAXIMA || method == ALWAN_MORPHOLOGY_H_MINIMA) && p->h < 0.0) return ALWAN_E_RANGE;
    if ((method == ALWAN_MORPHOLOGY_LOCAL_MAXIMA || method == ALWAN_MORPHOLOGY_LOCAL_MINIMA) && (kw != 3 || kh != 3))
        return ALWAN_E_INVALID;   /* scikit-image centres the footprint at (1, 1) */
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
    case ALWAN_MORPHOLOGY_SKELETONIZE:
    case ALWAN_MORPHOLOGY_THIN: {
        alwan_status const st = alwan_mo_skeleton(a, w, h, ch, method == ALWAN_MORPHOLOGY_THIN, p->iterations);
        if (st != ALWAN_OK) {
            ALWAN_FREE(a);
            return st;
        }
        break;
    }
    case ALWAN_MORPHOLOGY_H_MAXIMA:
    case ALWAN_MORPHOLOGY_H_MINIMA:
    case ALWAN_MORPHOLOGY_LOCAL_MAXIMA:
    case ALWAN_MORPHOLOGY_LOCAL_MINIMA: {
        long *off = (long *)ALWAN_ALLOC(alwan_safe_array_size(kw * kh, 2 * sizeof(long)), sizeof(long));
        size_t no;
        alwan_status st;
        if (!off) {
            ALWAN_FREE(a);
            return ALWAN_E_NOMEM;
        }
        no = alwan_mo_offsets(off, el, kw, kh);
        st = method == ALWAN_MORPHOLOGY_H_MAXIMA || method == ALWAN_MORPHOLOGY_H_MINIMA
                 ? alwan_mo_hextrema(a, w, h, ch, off, no, p->h, method == ALWAN_MORPHOLOGY_H_MINIMA, kind)
                 : alwan_mo_local_extrema(a, w, h, ch, off, no, method == ALWAN_MORPHOLOGY_LOCAL_MINIMA, p->exclude_borders);
        ALWAN_FREE(off);
        if (st != ALWAN_OK) {
            ALWAN_FREE(a);
            return st;
        }
        break;
    }
    case ALWAN_MORPHOLOGY_MEDIAL_AXIS:
    case ALWAN_MORPHOLOGY_CONVEX_HULL:
    case ALWAN_MORPHOLOGY_CONVEX_HULL_OBJECT: {
        alwan_status const st = method == ALWAN_MORPHOLOGY_MEDIAL_AXIS
                                    ? alwan_mo_medial(a, w, h, ch, p->medial_axis_distance)
                                    : alwan_mo_convex(a, w, h, ch, method == ALWAN_MORPHOLOGY_CONVEX_HULL_OBJECT, p->connectivity != 4);
        if (st != ALWAN_OK) {
            ALWAN_FREE(a);
            return st;
        }
        break;
    }
    case ALWAN_MORPHOLOGY_AREA_OPEN:
    case ALWAN_MORPHOLOGY_AREA_CLOSE:
    case ALWAN_MORPHOLOGY_DIAMETER_OPEN:
    case ALWAN_MORPHOLOGY_DIAMETER_CLOSE:
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

static alwan_status alwan_rc_run(void *out, size_t out_rs, void const *seed, size_t seed_rs, void const *mask, size_t mask_rs,
                                 size_t ch, size_t w, size_t h, alwan_reconstruct_method method,
                                 alwan_morphology_params const *params, int kind /* 0 f64, 1 f32, 2 u8 */) {
    alwan_morphology_params const zero = { 0 };
    alwan_morphology_params const *p = params ? params : &zero;
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    size_t const kw = p->kernel_width ? p->kernel_width : 3, kh = p->kernel_height ? p->kernel_height : 3;
    size_t const n = w * h * ch;
    double *a, *b;
    unsigned char *el;
    long *off;
    size_t x, y, i, no;
    alwan_status st;
    if (!out || !seed || !mask || w == 0 || h == 0 || ch == 0 || ch > 4 || n / ch / w != h) return ALWAN_E_INVALID;
    if (seed_rs / elem / ch < w || mask_rs / elem / ch < w || out_rs / elem / ch < w) return ALWAN_E_INVALID;
    if ((unsigned)method > (unsigned)ALWAN_RECONSTRUCT_EROSION) return ALWAN_E_INVALID;
    if (!p->kernel && (unsigned)p->shape > (unsigned)ALWAN_MORPHOLOGY_DIAMOND) return ALWAN_E_INVALID;
    if (kw > 255 || kh > 255) return ALWAN_E_RANGE;
    a = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, 2 * sizeof(double)) + kw * kh * (1 + 2 * sizeof(long)), sizeof(double));
    if (!a) return ALWAN_E_NOMEM;
    b = a + n;
    off = (long *)(b + n);
    el = (unsigned char *)(off + 2 * kw * kh);
    if (p->kernel) {
        for (i = 0; i < kw * kh; i++) el[i] = (unsigned char)(p->kernel[i] != 0);
    } else {
        alwan_mo_element(el, kw, kh, p->shape);
    }
    for (y = 0; y < h; y++) {
        char const *rs = (char const *)seed + y * seed_rs, *rm = (char const *)mask + y * mask_rs;
        for (x = 0; x < w * ch; x++) {
            double const s = kind == 0 ? ((alwan_f64 const *)rs)[x] : kind == 1 ? (double)((alwan_f32 const *)rs)[x]
                                                                                : (double)((unsigned char const *)rs)[x];
            double const m = kind == 0 ? ((alwan_f64 const *)rm)[x] : kind == 1 ? (double)((alwan_f32 const *)rm)[x]
                                                                                : (double)((unsigned char const *)rm)[x];
            if (!(s == s) || !(m == m)) {
                ALWAN_FREE(a);
                return ALWAN_E_INVALID;
            }
            /* by dilation the seed may not rise above the mask, by erosion not fall below it */
            if (method == ALWAN_RECONSTRUCT_DILATION ? s > m : s < m) {
                ALWAN_FREE(a);
                return ALWAN_E_RANGE;
            }
            a[y * w * ch + x] = s;
            b[y * w * ch + x] = m;
        }
    }
    no = alwan_mo_offsets(off, el, kw, kh);
    st = alwan_mo_reconstruct(a, b, w, h, ch, off, no, method == ALWAN_RECONSTRUCT_EROSION);
    if (st == ALWAN_OK) {
        for (y = 0; y < h; y++) {
            char *row = (char *)out + y * out_rs;
            for (x = 0; x < w * ch; x++) {
                double const v = a[y * w * ch + x];   /* a value of the seed or the mask: exact in their type */
                if (kind == 0) ((alwan_f64 *)row)[x] = v;
                else if (kind == 1) ((alwan_f32 *)row)[x] = (alwan_f32)v;
                else ((unsigned char *)row)[x] = (unsigned char)v;
            }
        }
    }
    ALWAN_FREE(a);
    return st;
}

alwan_status alwan_reconstruct_u8(unsigned char *out, size_t out_row_stride, unsigned char const *seed, size_t seed_row_stride,
                                  unsigned char const *mask, size_t mask_row_stride, size_t channels, size_t width, size_t height,
                                  alwan_reconstruct_method method, alwan_morphology_params const *params) {
    return alwan_rc_run(out, out_row_stride, seed, seed_row_stride, mask, mask_row_stride, channels, width, height, method,
                        params, 2);
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_reconstruct_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *seed, size_t seed_row_stride,
                                   alwan_f64 const *mask, size_t mask_row_stride, size_t channels, size_t width, size_t height,
                                   alwan_reconstruct_method method, alwan_morphology_params const *params) {
    return alwan_rc_run(out, out_row_stride, seed, seed_row_stride, mask, mask_row_stride, channels, width, height, method,
                        params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_reconstruct_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *seed, size_t seed_row_stride,
                                   alwan_f32 const *mask, size_t mask_row_stride, size_t channels, size_t width, size_t height,
                                   alwan_reconstruct_method method, alwan_morphology_params const *params) {
    return alwan_rc_run(out, out_row_stride, seed, seed_row_stride, mask, mask_row_stride, channels, width, height, method,
                        params, 1);
}
#endif
