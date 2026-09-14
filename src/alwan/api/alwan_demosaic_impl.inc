/* No include guard -- included twice (f32 and f64 passes) */

/*
 * Bayer mosaicing and demosaicing, after colour-demosaicing (colour_demosaicing.bayer),
 * which the tests hold this to.
 *
 *   bilinear     3 x 3 averaging of each channel's own sites
 *   Malvar 2004  Malvar, He and Cutler's gradient-corrected 5 x 5 linear filters
 *   Menon 2007   Menon, Andriani and Calvagno's directional filtering with a
 *                posteriori decision (DDFAPD), with or without its refining step
 *
 * The borders follow the reference exactly, since that is where two implementations
 * first disagree: bilinear and Malvar extend the image by scipy's 'reflect' (the edge
 * sample repeated), Menon's one-dimensional filters by 'mirror' (not repeated), its
 * direction decision by zero. The sums run in scipy's order: the centre tap then
 * outer pairs inwards for symmetric 1-D filters, the flipped kernel's non-zero taps
 * row by row for 2-D ones.
 */

#ifndef ALWAN__DEMOSAIC_ONCE
#define ALWAN__DEMOSAIC_ONCE

enum { ALWAN__FOLD_REFLECT = 0, ALWAN__FOLD_MIRROR = 1, ALWAN__FOLD_ZERO = 2 };

/* The channel a Bayer site records: 0 R, 1 G, 2 B. */
static int alwan__cfa_channel(alwan_cfa_pattern pattern, size_t x, size_t y) {
    static char const *const layouts[4] = { "RGGB", "BGGR", "GRBG", "GBRG" };
    char const ch = layouts[(int)pattern][(y & 1u) * 2u + (x & 1u)];
    return ch == 'R' ? 0 : (ch == 'G' ? 1 : 2);
}

/* The row and column parity of a pattern's R site (channel 0) or B site (channel 2). */
static void alwan__cfa_site(alwan_cfa_pattern pattern, int channel, size_t *row, size_t *col) {
    size_t y, x;
    for (y = 0; y < 2; y++) {
        for (x = 0; x < 2; x++) {
            if (alwan__cfa_channel(pattern, x, y) == channel) {
                *row = y;
                *col = x;
                return;
            }
        }
    }
}

/* scipy.ndimage 'reflect': d c b a | a b c d | d c b a */
static size_t alwan__fold_reflect(ptrdiff_t i, size_t n) {
    ptrdiff_t const period = (ptrdiff_t)(2u * n);
    i %= period;
    if (i < 0) i += period;
    return i < (ptrdiff_t)n ? (size_t)i : (size_t)(period - 1 - i);
}

/* scipy.ndimage 'mirror' and numpy.pad 'reflect': d c b | a b c d | c b a */
static size_t alwan__fold_mirror(ptrdiff_t i, size_t n) {
    ptrdiff_t period;
    if (n == 1) return 0;
    period = (ptrdiff_t)(2u * n - 2u);
    i %= period;
    if (i < 0) i += period;
    return i < (ptrdiff_t)n ? (size_t)i : (size_t)(period - i);
}

/* The kernels, as colour-demosaicing writes them. */
static double const alwan__k_h_g[9] = { 0.0, 0.25, 0.0, 0.25, 1.0, 0.25, 0.0, 0.25, 0.0 };
static double const alwan__k_h_rb[9] = { 0.25, 0.5, 0.25, 0.5, 1.0, 0.5, 0.25, 0.5, 0.25 };
static double const alwan__k_gr_gb[25] = {
    0.0, 0.0, -1.0 / 8, 0.0, 0.0,
    0.0, 0.0, 2.0 / 8, 0.0, 0.0,
    -1.0 / 8, 2.0 / 8, 4.0 / 8, 2.0 / 8, -1.0 / 8,
    0.0, 0.0, 2.0 / 8, 0.0, 0.0,
    0.0, 0.0, -1.0 / 8, 0.0, 0.0,
};
static double const alwan__k_rg_rb[25] = {
    0.0, 0.0, 0.5 / 8, 0.0, 0.0,
    0.0, -1.0 / 8, 0.0, -1.0 / 8, 0.0,
    -1.0 / 8, 4.0 / 8, 5.0 / 8, 4.0 / 8, -1.0 / 8,
    0.0, -1.0 / 8, 0.0, -1.0 / 8, 0.0,
    0.0, 0.0, 0.5 / 8, 0.0, 0.0,
};
static double const alwan__k_rg_br[25] = { /* the transpose of the one above */
    0.0, 0.0, -1.0 / 8, 0.0, 0.0,
    0.0, -1.0 / 8, 4.0 / 8, -1.0 / 8, 0.0,
    0.5 / 8, 0.0, 5.0 / 8, 0.0, 0.5 / 8,
    0.0, -1.0 / 8, 4.0 / 8, -1.0 / 8, 0.0,
    0.0, 0.0, -1.0 / 8, 0.0, 0.0,
};
static double const alwan__k_rb_bb[25] = {
    0.0, 0.0, -1.5 / 8, 0.0, 0.0,
    0.0, 2.0 / 8, 0.0, 2.0 / 8, 0.0,
    -1.5 / 8, 0.0, 6.0 / 8, 0.0, -1.5 / 8,
    0.0, 2.0 / 8, 0.0, 2.0 / 8, 0.0,
    0.0, 0.0, -1.5 / 8, 0.0, 0.0,
};
static double const alwan__k_menon[25] = {
    0.0, 0.0, 1.0, 0.0, 1.0,
    0.0, 0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 3.0, 0.0, 3.0,
    0.0, 0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 1.0, 0.0, 1.0,
};
static double const alwan__k_menon_t[25] = {
    0.0, 0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0, 0.0,
    1.0, 0.0, 3.0, 0.0, 1.0,
    0.0, 1.0, 0.0, 1.0, 0.0,
    1.0, 0.0, 3.0, 0.0, 1.0,
};
/* Symmetric 1-D filters, centre tap first: h_0, h_1, k_b, and the 3-tap mean. */
static double const alwan__h0[3] = { 0.0, 0.5, 0.0 };
static double const alwan__h1[3] = { 0.5, 0.0, -0.25 };
static double const alwan__kb[2] = { 0.0, 0.5 };
static double const alwan__fir[2] = { 1.0 / 3.0, 1.0 / 3.0 };

#endif /* ALWAN__DEMOSAIC_ONCE */

/* 2-D convolution at one pixel, as scipy.ndimage.convolve sums it. */
static ALWAN_CORE_T ALWAN_CORE_FN(dm_conv2)(ALWAN_CORE_T const *img, size_t w, size_t h, size_t x, size_t y,
                                            double const *k, int ksize, int fold) {
    int const c = ksize / 2;
    ALWAN_CORE_T acc = ALWAN_CORE_ZERO;
    int a, b;
    for (a = 0; a < ksize; a++) {
        for (b = 0; b < ksize; b++) {
            double const wv = k[(ksize - 1 - a) * ksize + (ksize - 1 - b)];
            ptrdiff_t const yy = (ptrdiff_t)y + a - c;
            ptrdiff_t const xx = (ptrdiff_t)x + b - c;
            ALWAN_CORE_T v;
            if (wv == 0.0) continue;
            if (fold == ALWAN__FOLD_ZERO) {
                if (yy < 0 || xx < 0 || yy >= (ptrdiff_t)h || xx >= (ptrdiff_t)w) continue;
                v = img[(size_t)yy * w + (size_t)xx];
            } else {
                v = img[alwan__fold_reflect(yy, h) * w + alwan__fold_reflect(xx, w)];
            }
            acc += v * (ALWAN_CORE_T)wv;
        }
    }
    return acc;
}

/* A symmetric 1-D filter with 'mirror' borders, as scipy.ndimage.convolve1d sums it. */
static ALWAN_CORE_T ALWAN_CORE_FN(dm_conv1)(ALWAN_CORE_T const *img, size_t w, size_t h, size_t x, size_t y,
                                            double const *half, int r, int vertical) {
    ALWAN_CORE_T acc = img[y * w + x] * (ALWAN_CORE_T)half[0];
    int j;
    for (j = r; j >= 1; j--) {
        ALWAN_CORE_T lo, hi;
        if (vertical) {
            lo = img[alwan__fold_mirror((ptrdiff_t)y - j, h) * w + x];
            hi = img[alwan__fold_mirror((ptrdiff_t)y + j, h) * w + x];
        } else {
            lo = img[y * w + alwan__fold_mirror((ptrdiff_t)x - j, w)];
            hi = img[y * w + alwan__fold_mirror((ptrdiff_t)x + j, w)];
        }
        acc += (lo + hi) * (ALWAN_CORE_T)half[j];
    }
    return acc;
}

#ifndef ALWAN_DEMOSAIC_NAME
#  define ALWAN_DEMOSAIC_NAME_(base, sfx) base##sfx
#  define ALWAN_DEMOSAIC_NAME(base, sfx)  ALWAN_DEMOSAIC_NAME_(base, sfx)
#endif

alwan_status ALWAN_CORE_FNLIT(alwan_cfa_bayer_mosaic)(ALWAN_CORE_T *cfa_out, size_t cfa_row_stride,
                                                      ALWAN_CORE_T const *rgb, size_t rgb_row_stride,
                                                      size_t width, size_t height, alwan_cfa_pattern pattern) {
    size_t x, y;
    if (!cfa_out || !rgb || width == 0 || height == 0 || (int)pattern < 0 || (int)pattern > 3) {
        return ALWAN_E_INVALID;
    }
    for (y = 0; y < height; y++) {
        ALWAN_CORE_T const *s = (ALWAN_CORE_T const *)((char const *)rgb + y * rgb_row_stride);
        ALWAN_CORE_T *o = (ALWAN_CORE_T *)((char *)cfa_out + y * cfa_row_stride);
        for (x = 0; x < width; x++) {
            o[x] = s[3 * x + (size_t)alwan__cfa_channel(pattern, x, y)];
        }
    }
    return ALWAN_OK;
}

/* colour-demosaicing's Menon 2007, one full-plane step at a time, as numpy runs it. */
static void ALWAN_CORE_FN(dm_menon)(ALWAN_CORE_T *R, ALWAN_CORE_T *G, ALWAN_CORE_T *B, ALWAN_CORE_T const *cfa,
                                    ALWAN_CORE_T *t0, ALWAN_CORE_T *t1, ALWAN_CORE_T *t2, ALWAN_CORE_T *t3,
                                    unsigned char *M, size_t w, size_t h, alwan_cfa_pattern pattern, int refine) {
    size_t const n = w * h;
    size_t rr, rc, br, bc, x, y, p;
    ALWAN_CORE_T *G_H = t0, *G_V = t1, *C_H = t2, *C_V = t3;
    alwan__cfa_site(pattern, 0, &rr, &rc);
    alwan__cfa_site(pattern, 2, &br, &bc);
#define CH(x_, y_) alwan__cfa_channel(pattern, (x_), (y_))
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int const ch = CH(x, y);
            p = y * w + x;
            R[p] = ch == 0 ? cfa[p] : ALWAN_CORE_ZERO;
            G[p] = ch == 1 ? cfa[p] : ALWAN_CORE_ZERO;
            B[p] = ch == 2 ? cfa[p] : ALWAN_CORE_ZERO;
        }
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            p = y * w + x;
            if (CH(x, y) == 1) {
                G_H[p] = G[p];
                G_V[p] = G[p];
            } else {
                G_H[p] = ALWAN_CORE_FN(dm_conv1)(cfa, w, h, x, y, alwan__h0, 2, 0)
                       + ALWAN_CORE_FN(dm_conv1)(cfa, w, h, x, y, alwan__h1, 2, 0);
                G_V[p] = ALWAN_CORE_FN(dm_conv1)(cfa, w, h, x, y, alwan__h0, 2, 1)
                       + ALWAN_CORE_FN(dm_conv1)(cfa, w, h, x, y, alwan__h1, 2, 1);
            }
        }
    }
    for (p = 0; p < n; p++) {
        int const ch = CH(p % w, p / w);
        C_H[p] = ch == 0 ? R[p] - G_H[p] : (ch == 2 ? B[p] - G_H[p] : ALWAN_CORE_ZERO);
        C_V[p] = ch == 0 ? R[p] - G_V[p] : (ch == 2 ? B[p] - G_V[p] : ALWAN_CORE_ZERO);
    }
    /* D_H and D_V over the C planes, then the decision. The R and B planes are free
     * to hold them until they are rebuilt from the CFA below. */
    {
        ALWAN_CORE_T *D_H = R, *D_V = B;
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                ALWAN_CORE_T const dh = C_H[y * w + x] - C_H[y * w + alwan__fold_mirror((ptrdiff_t)x + 2, w)];
                ALWAN_CORE_T const dv = C_V[y * w + x] - C_V[alwan__fold_mirror((ptrdiff_t)y + 2, h) * w + x];
                D_H[y * w + x] = dh < ALWAN_CORE_ZERO ? -dh : dh;
                D_V[y * w + x] = dv < ALWAN_CORE_ZERO ? -dv : dv;
            }
        }
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                ALWAN_CORE_T const d_h = ALWAN_CORE_FN(dm_conv2)(D_H, w, h, x, y, alwan__k_menon, 5, ALWAN__FOLD_ZERO);
                ALWAN_CORE_T const d_v = ALWAN_CORE_FN(dm_conv2)(D_V, w, h, x, y, alwan__k_menon_t, 5, ALWAN__FOLD_ZERO);
                M[y * w + x] = (unsigned char)(d_v >= d_h);
            }
        }
    }
    for (p = 0; p < n; p++) {
        int const ch = CH(p % w, p / w);
        G[p] = M[p] ? G_H[p] : G_V[p];
        R[p] = ch == 0 ? cfa[p] : ALWAN_CORE_ZERO;
        B[p] = ch == 2 ? cfa[p] : ALWAN_CORE_ZERO;
    }

#define ROW_R(y_) (((y_) & 1u) == rr)
#define ROW_B(y_) (((y_) & 1u) == br)
#define COL_R(x_) (((x_) & 1u) == rc)
#define COL_B(x_) (((x_) & 1u) == bc)
#define CNV(img_, vert_, k_, r_) ALWAN_CORE_FN(dm_conv1)((img_), w, h, x, y, (k_), (r_), (vert_))
    /* Each step reads the planes as they stood before it, like numpy.where. */
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        p = y * w + x;
        t0[p] = (CH(x, y) == 1 && ROW_R(y)) ? G[p] + CNV(R, 0, alwan__kb, 1) - CNV(G, 0, alwan__kb, 1) : R[p];
    }
    for (p = 0; p < n; p++) R[p] = t0[p];
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        p = y * w + x;
        t0[p] = (CH(x, y) == 1 && ROW_B(y)) ? G[p] + CNV(R, 1, alwan__kb, 1) - CNV(G, 1, alwan__kb, 1) : R[p];
    }
    for (p = 0; p < n; p++) R[p] = t0[p];
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        p = y * w + x;
        t0[p] = (CH(x, y) == 1 && ROW_B(y)) ? G[p] + CNV(B, 0, alwan__kb, 1) - CNV(G, 0, alwan__kb, 1) : B[p];
    }
    for (p = 0; p < n; p++) B[p] = t0[p];
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        p = y * w + x;
        t0[p] = (CH(x, y) == 1 && ROW_R(y)) ? G[p] + CNV(B, 1, alwan__kb, 1) - CNV(G, 1, alwan__kb, 1) : B[p];
    }
    for (p = 0; p < n; p++) B[p] = t0[p];
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        p = y * w + x;
        if (ROW_B(y) && CH(x, y) == 2) {
            t0[p] = M[p] ? B[p] + CNV(R, 0, alwan__kb, 1) - CNV(B, 0, alwan__kb, 1)
                         : B[p] + CNV(R, 1, alwan__kb, 1) - CNV(B, 1, alwan__kb, 1);
        } else {
            t0[p] = R[p];
        }
    }
    for (p = 0; p < n; p++) R[p] = t0[p];
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        p = y * w + x;
        if (ROW_R(y) && CH(x, y) == 0) {
            t0[p] = M[p] ? R[p] + CNV(B, 0, alwan__kb, 1) - CNV(R, 0, alwan__kb, 1)
                         : R[p] + CNV(B, 1, alwan__kb, 1) - CNV(R, 1, alwan__kb, 1);
        } else {
            t0[p] = B[p];
        }
    }
    for (p = 0; p < n; p++) B[p] = t0[p];

    if (refine) {
        ALWAN_CORE_T *R_G = t0, *B_G = t1, *R_G_m = t2, *B_G_m = t3;
        /* Green at the red and blue sites. */
        for (p = 0; p < n; p++) { R_G[p] = R[p] - G[p]; B_G[p] = B[p] - G[p]; }
        for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
            int const ch = CH(x, y);
            p = y * w + x;
            B_G_m[p] = ch == 2 ? (M[p] ? CNV(B_G, 0, alwan__fir, 1) : CNV(B_G, 1, alwan__fir, 1)) : ALWAN_CORE_ZERO;
            R_G_m[p] = ch == 0 ? (M[p] ? CNV(R_G, 0, alwan__fir, 1) : CNV(R_G, 1, alwan__fir, 1)) : ALWAN_CORE_ZERO;
        }
        for (p = 0; p < n; p++) {
            int const ch = CH(p % w, p / w);
            if (ch == 0) G[p] = R[p] - R_G_m[p];
            if (ch == 2) G[p] = B[p] - B_G_m[p];
        }
        /* Red and blue at the green sites. */
        for (p = 0; p < n; p++) { R_G[p] = R[p] - G[p]; B_G[p] = B[p] - G[p]; }
        for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
            p = y * w + x;
            if (CH(x, y) == 1 && ROW_B(y)) R_G_m[p] = CNV(R_G, 1, alwan__kb, 1);
        }
        for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
            p = y * w + x;
            if (CH(x, y) == 1 && ROW_B(y)) R[p] = G[p] + R_G_m[p];
        }
        for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
            p = y * w + x;
            if (CH(x, y) == 1 && COL_B(x)) R_G_m[p] = CNV(R_G, 0, alwan__kb, 1);
        }
        for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
            p = y * w + x;
            if (CH(x, y) == 1 && COL_B(x)) R[p] = G[p] + R_G_m[p];
        }
        for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
            p = y * w + x;
            if (CH(x, y) == 1 && ROW_R(y)) B_G_m[p] = CNV(B_G, 1, alwan__kb, 1);
        }
        for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
            p = y * w + x;
            if (CH(x, y) == 1 && ROW_R(y)) B[p] = G[p] + B_G_m[p];
        }
        for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
            p = y * w + x;
            if (CH(x, y) == 1 && COL_R(x)) B_G_m[p] = CNV(B_G, 0, alwan__kb, 1);
        }
        for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
            p = y * w + x;
            if (CH(x, y) == 1 && COL_R(x)) B[p] = G[p] + B_G_m[p];
        }
        /* Red at the blue sites and blue at the red sites, from one R - B plane. */
        {
            ALWAN_CORE_T *R_B = t0, *R_B_m = t1;
            for (p = 0; p < n; p++) R_B[p] = R[p] - B[p];
            for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
                p = y * w + x;
                R_B_m[p] = CH(x, y) == 2 ? (M[p] ? CNV(R_B, 0, alwan__fir, 1) : CNV(R_B, 1, alwan__fir, 1))
                                         : ALWAN_CORE_ZERO;
            }
            for (p = 0; p < n; p++) if (CH(p % w, p / w) == 2) R[p] = B[p] + R_B_m[p];
            for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
                p = y * w + x;
                R_B_m[p] = CH(x, y) == 0 ? (M[p] ? CNV(R_B, 0, alwan__fir, 1) : CNV(R_B, 1, alwan__fir, 1))
                                         : ALWAN_CORE_ZERO;
            }
            for (p = 0; p < n; p++) if (CH(p % w, p / w) == 0) B[p] = R[p] - R_B_m[p];
        }
    }
#undef CNV
#undef ROW_R
#undef ROW_B
#undef COL_R
#undef COL_B
#undef CH
}

alwan_status ALWAN_CORE_FNLIT(alwan_cfa_bayer_demosaic)(ALWAN_CORE_T *rgb_out, size_t rgb_row_stride,
                                                        ALWAN_CORE_T const *cfa, size_t cfa_row_stride,
                                                        size_t width, size_t height, alwan_cfa_pattern pattern,
                                                        alwan_demosaic_method method) {
    size_t const n = width * height;
    size_t rr, rc, br, bc, x, y;
    ALWAN_CORE_T *buf, *plane, *R0, *G0, *B0, *t[4];
    unsigned char *M = NULL;
    if (!rgb_out || !cfa || width < 2 || height < 2 || (int)pattern < 0 || (int)pattern > 3
        || (int)method < 0 || (int)method > (int)ALWAN_DEMOSAIC_MENON2007_NO_REFINE) {
        return ALWAN_E_INVALID;
    }
    if (n / width != height) {
        return ALWAN_E_INVALID;
    }
    buf = (ALWAN_CORE_T *)ALWAN_ALLOC(8 * n * sizeof(ALWAN_CORE_T), sizeof(ALWAN_CORE_T));
    if (!buf) return ALWAN_E_NOMEM;
    plane = buf;
    R0 = buf + n; G0 = R0 + n; B0 = G0 + n;
    t[0] = B0 + n; t[1] = t[0] + n; t[2] = t[1] + n; t[3] = t[2] + n;
    for (y = 0; y < height; y++) {
        ALWAN_CORE_T const *s = (ALWAN_CORE_T const *)((char const *)cfa + y * cfa_row_stride);
        for (x = 0; x < width; x++) plane[y * width + x] = s[x];
    }
    alwan__cfa_site(pattern, 0, &rr, &rc);
    alwan__cfa_site(pattern, 2, &br, &bc);

    if (method == ALWAN_DEMOSAIC_BILINEAR) {
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                int const ch = alwan__cfa_channel(pattern, x, y);
                size_t const p = y * width + x;
                R0[p] = ch == 0 ? plane[p] : ALWAN_CORE_ZERO;
                G0[p] = ch == 1 ? plane[p] : ALWAN_CORE_ZERO;
                B0[p] = ch == 2 ? plane[p] : ALWAN_CORE_ZERO;
            }
        }
        for (y = 0; y < height; y++) {
            ALWAN_CORE_T *o = (ALWAN_CORE_T *)((char *)rgb_out + y * rgb_row_stride);
            for (x = 0; x < width; x++) {
                o[3 * x + 0] = ALWAN_CORE_FN(dm_conv2)(R0, width, height, x, y, alwan__k_h_rb, 3, ALWAN__FOLD_REFLECT);
                o[3 * x + 1] = ALWAN_CORE_FN(dm_conv2)(G0, width, height, x, y, alwan__k_h_g, 3, ALWAN__FOLD_REFLECT);
                o[3 * x + 2] = ALWAN_CORE_FN(dm_conv2)(B0, width, height, x, y, alwan__k_h_rb, 3, ALWAN__FOLD_REFLECT);
            }
        }
    } else if (method == ALWAN_DEMOSAIC_MALVAR2004) {
        for (y = 0; y < height; y++) {
            ALWAN_CORE_T *o = (ALWAN_CORE_T *)((char *)rgb_out + y * rgb_row_stride);
            int const row_r = (y & 1u) == rr, row_b = (y & 1u) == br;
            for (x = 0; x < width; x++) {
                int const ch = alwan__cfa_channel(pattern, x, y);
                int const col_r = (x & 1u) == rc, col_b = (x & 1u) == bc;
                ALWAN_CORE_T const v = plane[y * width + x];
                ALWAN_CORE_T r = ch == 0 ? v : ALWAN_CORE_ZERO;
                ALWAN_CORE_T g = ch == 1 ? v : ALWAN_CORE_ZERO;
                ALWAN_CORE_T b = ch == 2 ? v : ALWAN_CORE_ZERO;
                if (ch != 1) {
                    g = ALWAN_CORE_FN(dm_conv2)(plane, width, height, x, y, alwan__k_gr_gb, 5, ALWAN__FOLD_REFLECT);
                }
                if (row_r && col_b) {
                    r = ALWAN_CORE_FN(dm_conv2)(plane, width, height, x, y, alwan__k_rg_rb, 5, ALWAN__FOLD_REFLECT);
                    b = ALWAN_CORE_FN(dm_conv2)(plane, width, height, x, y, alwan__k_rg_br, 5, ALWAN__FOLD_REFLECT);
                }
                if (row_b && col_r) {
                    r = ALWAN_CORE_FN(dm_conv2)(plane, width, height, x, y, alwan__k_rg_br, 5, ALWAN__FOLD_REFLECT);
                    b = ALWAN_CORE_FN(dm_conv2)(plane, width, height, x, y, alwan__k_rg_rb, 5, ALWAN__FOLD_REFLECT);
                }
                if (row_b && col_b) {
                    r = ALWAN_CORE_FN(dm_conv2)(plane, width, height, x, y, alwan__k_rb_bb, 5, ALWAN__FOLD_REFLECT);
                }
                if (row_r && col_r) {
                    b = ALWAN_CORE_FN(dm_conv2)(plane, width, height, x, y, alwan__k_rb_bb, 5, ALWAN__FOLD_REFLECT);
                }
                o[3 * x + 0] = r;
                o[3 * x + 1] = g;
                o[3 * x + 2] = b;
            }
        }
    } else {
        M = (unsigned char *)ALWAN_ALLOC(n, 1);
        if (!M) {
            ALWAN_FREE(buf);
            return ALWAN_E_NOMEM;
        }
        ALWAN_CORE_FN(dm_menon)(R0, G0, B0, plane, t[0], t[1], t[2], t[3], M, width, height, pattern,
                                method == ALWAN_DEMOSAIC_MENON2007);
        for (y = 0; y < height; y++) {
            ALWAN_CORE_T *o = (ALWAN_CORE_T *)((char *)rgb_out + y * rgb_row_stride);
            for (x = 0; x < width; x++) {
                size_t const p = y * width + x;
                o[3 * x + 0] = R0[p];
                o[3 * x + 1] = G0[p];
                o[3 * x + 2] = B0[p];
            }
        }
        ALWAN_FREE(M);
    }
    ALWAN_FREE(buf);
    return ALWAN_OK;
}
