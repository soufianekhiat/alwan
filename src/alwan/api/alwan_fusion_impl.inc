/* No include guard -- included twice (f32 and f64 passes) */

/*
 * Exposure fusion, Mertens, Kautz and Van Reeth 2007, after OpenCV's MergeMertens,
 * which the tests hold this to.
 *
 * Every pixel of every exposure gets a weight, the product of three measures, each
 * raised to its exponent:
 *
 *   contrast          |Laplacian| of the BT.601 luma, 3 x 3
 *   saturation        the root of the summed squared deviations of R, G and B from
 *                     their mean
 *   well-exposedness  prod_c exp(-(v_c - 0.5)^2 / (2 sigma^2))
 *
 * OpenCV adds 1e-12 to each weight, so a pixel that no exposure favours still gets
 * their plain average. The weights are normalised per pixel. The images then blend
 * level by level as Laplacian pyramids, each level weighted by that level of the
 * weights' Gaussian pyramid, and the blended pyramid collapses to the result.
 *
 * The borders are OpenCV's: the Laplacian and pyrDown reflect without repeating the
 * edge sample; pyrUp does the same at the top and left and repeats the edge sample at
 * the bottom and right.
 */

#ifndef ALWAN__FUSION_ONCE
#define ALWAN__FUSION_ONCE

#define ALWAN__FUSION_MAX_LEVELS 64

/* OpenCV BORDER_REFLECT_101: c b | a b c | b a */
static size_t alwan__fu_r101(ptrdiff_t i, size_t n) {
    ptrdiff_t period;
    if (n == 1) return 0;
    period = (ptrdiff_t)(2u * n - 2u);
    i %= period;
    if (i < 0) i += period;
    return i < (ptrdiff_t)n ? (size_t)i : (size_t)(period - i);
}

/* Finite and not negative. */
static int alwan__fu_exponent_ok(double v) {
    return v >= 0.0 && v - v == 0.0;
}

#endif /* ALWAN__FUSION_ONCE */

/* pyrDown: [1 4 6 4 1] / 16 along each axis, keeping the even samples. src is w x h
 * with ch channels, dst (w + 1) / 2 x (h + 1) / 2; tmp holds h x (w + 1) / 2 x ch. */
static void ALWAN_CORE_FN(fu_down)(ALWAN_CORE_T *dst, ALWAN_CORE_T const *src, size_t w, size_t h, size_t ch,
                                   ALWAN_CORE_T *tmp) {
    ALWAN_CORE_T const four = ALWAN_CORE_LITERAL(4.0), six = ALWAN_CORE_LITERAL(6.0);
    ALWAN_CORE_T const sixteenth = ALWAN_CORE_LITERAL(0.0625);
    size_t const W = (w + 1) / 2, H = (h + 1) / 2;
    size_t x, y, c;
    for (y = 0; y < h; y++) {
        ALWAN_CORE_T const *s = src + y * w * ch;
        ALWAN_CORE_T *t = tmp + y * W * ch;
        for (x = 0; x < W; x++) {
            ptrdiff_t const m = (ptrdiff_t)(2 * x);
            size_t const a = alwan__fu_r101(m - 2, w) * ch, b = alwan__fu_r101(m - 1, w) * ch;
            size_t const d = alwan__fu_r101(m + 1, w) * ch, e = alwan__fu_r101(m + 2, w) * ch;
            size_t const k = 2 * x * ch;
            for (c = 0; c < ch; c++) {
                t[x * ch + c] = ((s[a + c] + s[e + c]) + (s[b + c] + s[d + c]) * four + s[k + c] * six) * sixteenth;
            }
        }
    }
    for (y = 0; y < H; y++) {
        ptrdiff_t const m = (ptrdiff_t)(2 * y);
        ALWAN_CORE_T const *ra = tmp + alwan__fu_r101(m - 2, h) * W * ch;
        ALWAN_CORE_T const *rb = tmp + alwan__fu_r101(m - 1, h) * W * ch;
        ALWAN_CORE_T const *rk = tmp + (size_t)m * W * ch;
        ALWAN_CORE_T const *rd = tmp + alwan__fu_r101(m + 1, h) * W * ch;
        ALWAN_CORE_T const *re = tmp + alwan__fu_r101(m + 2, h) * W * ch;
        ALWAN_CORE_T *o = dst + y * W * ch;
        for (x = 0; x < W * ch; x++) {
            o[x] = ((ra[x] + re[x]) + (rb[x] + rd[x]) * four + rk[x] * six) * sixteenth;
        }
    }
}

/* One line of pyrUp: n samples, step apart, to m = 2n or 2n - 1 samples. Zeros are
 * inserted and [1 4 6 4 1] / 8 applied: even outputs (lo + 6 s + hi) / 8, odd ones
 * (s + hi) / 2, with lo reflected at the start and hi repeated at the end. */
static void ALWAN_CORE_FN(fu_up_line)(ALWAN_CORE_T *dst, size_t dstep, ALWAN_CORE_T const *src, size_t sstep,
                                      size_t n, size_t m, size_t ch) {
    ALWAN_CORE_T const six = ALWAN_CORE_LITERAL(6.0), eighth = ALWAN_CORE_LITERAL(0.125);
    ALWAN_CORE_T const half = ALWAN_CORE_LITERAL(0.5);
    size_t i, c;
    for (i = 0; i < n; i++) {
        ALWAN_CORE_T const *s = src + i * sstep;
        ALWAN_CORE_T const *lo = n == 1 ? s : (i > 0 ? s - sstep : s + sstep);
        ALWAN_CORE_T const *hi = i + 1 < n ? s + sstep : s;
        for (c = 0; c < ch; c++) {
            ALWAN_CORE_T even, odd;
            if (n == 1) {
                even = s[c];
                odd = s[c];
            } else {
                even = (lo[c] + s[c] * six + hi[c]) * eighth;
                odd = (s[c] + hi[c]) * half;
            }
            if (2 * i < m) dst[2 * i * dstep + c] = even;
            if (2 * i + 1 < m) dst[(2 * i + 1) * dstep + c] = odd;
        }
    }
}

/* pyrUp from sw x sh to dw x dh, rows first. tmp holds sh x dw x ch. */
static void ALWAN_CORE_FN(fu_up)(ALWAN_CORE_T *dst, size_t dw, size_t dh, ALWAN_CORE_T const *src, size_t sw,
                                 size_t sh, size_t ch, ALWAN_CORE_T *tmp) {
    size_t x, y;
    for (y = 0; y < sh; y++) {
        ALWAN_CORE_FN(fu_up_line)(tmp + y * dw * ch, ch, src + y * sw * ch, ch, sw, dw, ch);
    }
    for (x = 0; x < dw; x++) {
        ALWAN_CORE_FN(fu_up_line)(dst + x * ch, dw * ch, tmp + x * ch, dw * ch, sh, dh, ch);
    }
}

/* A measure raised to its exponent; 1 is left exact. */
static ALWAN_CORE_T ALWAN_CORE_FN(fu_raise)(ALWAN_CORE_T v, ALWAN_CORE_T e, int ignored) {
    if (ignored) return ALWAN_CORE_LITERAL(1.0);
    if (e == ALWAN_CORE_LITERAL(1.0)) return v;
    return ALWAN_CORE_POW(v, e);
}

/* The weight of every pixel of one image. img is w x h RGB, contiguous; gray is scratch. */
static void ALWAN_CORE_FN(fu_weights)(ALWAN_CORE_T *wt, ALWAN_CORE_T const *img, size_t w, size_t h,
                                      ALWAN_CORE_T *gray, ALWAN_CORE_T const expo[3], unsigned ignore,
                                      ALWAN_CORE_T two_sigma2) {
    ALWAN_CORE_T const half = ALWAN_CORE_LITERAL(0.5), third = ALWAN_CORE_LITERAL(3.0);
    size_t const n = w * h;
    size_t x, y, p;
    for (p = 0; p < n; p++) {
        /* OpenCV's COLOR_RGB2GRAY: the BT.601 luma weights. */
        gray[p] = img[3 * p] * ALWAN_CORE_LITERAL(0.299) + img[3 * p + 1] * ALWAN_CORE_LITERAL(0.587)
                + img[3 * p + 2] * ALWAN_CORE_LITERAL(0.114);
    }
    for (y = 0; y < h; y++) {
        size_t const yu = alwan__fu_r101((ptrdiff_t)y - 1, h), yd = alwan__fu_r101((ptrdiff_t)y + 1, h);
        for (x = 0; x < w; x++) {
            size_t const xl = alwan__fu_r101((ptrdiff_t)x - 1, w), xr = alwan__fu_r101((ptrdiff_t)x + 1, w);
            ALWAN_CORE_T const *v = img + 3 * (y * w + x);
            ALWAN_CORE_T const g = gray[y * w + x];
            ALWAN_CORE_T const lap = gray[yu * w + x] + gray[y * w + xl] + gray[y * w + xr] + gray[yd * w + x]
                                   - g * ALWAN_CORE_LITERAL(4.0);
            ALWAN_CORE_T const mean = (v[0] + v[1] + v[2]) / third;
            ALWAN_CORE_T const d0 = v[0] - mean, d1 = v[1] - mean, d2 = v[2] - mean;
            ALWAN_CORE_T const e0 = v[0] - half, e1 = v[1] - half, e2 = v[2] - half;
            ALWAN_CORE_T const contrast = ALWAN_CORE_ABS(lap);
            ALWAN_CORE_T const saturation = ALWAN_CORE_SQRT(d0 * d0 + d1 * d1 + d2 * d2);
            ALWAN_CORE_T const well = ALWAN_CORE_EXP(-(e0 * e0) / two_sigma2) * ALWAN_CORE_EXP(-(e1 * e1) / two_sigma2)
                                    * ALWAN_CORE_EXP(-(e2 * e2) / two_sigma2);
            wt[y * w + x] = ALWAN_CORE_FN(fu_raise)(contrast, expo[0], (ignore & ALWAN_FUSION_IGNORE_CONTRAST) != 0u)
                          * ALWAN_CORE_FN(fu_raise)(saturation, expo[1], (ignore & ALWAN_FUSION_IGNORE_SATURATION) != 0u)
                          * ALWAN_CORE_FN(fu_raise)(well, expo[2], (ignore & ALWAN_FUSION_IGNORE_EXPOSURE) != 0u)
                          + ALWAN_CORE_LITERAL(1e-12);
        }
    }
}

static void ALWAN_CORE_FN(fu_load)(ALWAN_CORE_T *dst, ALWAN_CORE_T const *src, size_t row_stride, size_t w, size_t h) {
    size_t x, y;
    for (y = 0; y < h; y++) {
        ALWAN_CORE_T const *s = (ALWAN_CORE_T const *)((char const *)src + y * row_stride);
        for (x = 0; x < 3 * w; x++) dst[y * 3 * w + x] = s[x];
    }
}

alwan_status ALWAN_CORE_FNLIT(alwan_exposure_fusion_mertens2007)(ALWAN_CORE_T *rgb_out, size_t out_row_stride,
                                                                 ALWAN_CORE_T const *const *images,
                                                                 size_t in_row_stride, size_t image_count,
                                                                 size_t width, size_t height,
                                                                 alwan_exposure_fusion_params const *params) {
    alwan_exposure_fusion_params const zero = { 0 };
    alwan_exposure_fusion_params const *pr = params ? params : &zero;
    size_t const n = width * height;
    size_t lw[ALWAN__FUSION_MAX_LEVELS], lh[ALWAN__FUSION_MAX_LEVELS], off[ALWAN__FUSION_MAX_LEVELS];
    size_t levels, total = 0, l, i, p, x, y;
    ALWAN_CORE_T expo[3], two_sigma2, sigma;
    ALWAN_CORE_T *buf, *res, *pyr, *wpyr, *wsum, *wcur, *gray, *ta, *tb;

    if (!rgb_out || !images || image_count == 0 || width == 0 || height == 0) return ALWAN_E_INVALID;
    if (n / width != height || n > ((size_t)-1) / (32u * sizeof(ALWAN_CORE_T))) return ALWAN_E_INVALID;
    for (i = 0; i < image_count; i++) {
        if (!images[i]) return ALWAN_E_INVALID;
    }
    if (!alwan__fu_exponent_ok(pr->contrast) || !alwan__fu_exponent_ok(pr->saturation)
        || !alwan__fu_exponent_ok(pr->exposure) || !alwan__fu_exponent_ok(pr->sigma)
        || (pr->ignore & ~7u) != 0u || pr->levels < 0 || pr->levels > ALWAN__FUSION_MAX_LEVELS) {
        return ALWAN_E_INVALID;
    }
    expo[0] = pr->contrast == 0.0 ? ALWAN_CORE_LITERAL(1.0) : (ALWAN_CORE_T)pr->contrast;
    expo[1] = pr->saturation == 0.0 ? ALWAN_CORE_LITERAL(1.0) : (ALWAN_CORE_T)pr->saturation;
    expo[2] = pr->exposure == 0.0 ? ALWAN_CORE_LITERAL(1.0) : (ALWAN_CORE_T)pr->exposure;
    sigma = pr->sigma == 0.0 ? ALWAN_CORE_LITERAL(0.2) : (ALWAN_CORE_T)pr->sigma;
    two_sigma2 = ALWAN_CORE_LITERAL(2.0) * sigma * sigma;

    /* OpenCV: floor(log2(min(width, height))) levels below the image. */
    if (pr->levels == 0) {
        size_t m = width < height ? width : height;
        levels = 1;
        while (m >>= 1) levels++;
    } else {
        levels = (size_t)pr->levels;
    }
    lw[0] = width;
    lh[0] = height;
    for (l = 0; l < levels; l++) {
        if (l > 0) {
            lw[l] = (lw[l - 1] + 1) / 2;
            lh[l] = (lh[l - 1] + 1) / 2;
        }
        off[l] = total;
        total += lw[l] * lh[l];
    }

    buf = (ALWAN_CORE_T *)ALWAN_ALLOC((7 * total + 9 * n) * sizeof(ALWAN_CORE_T), sizeof(ALWAN_CORE_T));
    if (!buf) return ALWAN_E_NOMEM;
    res = buf;
    pyr = res + 3 * total;
    wpyr = pyr + 3 * total;
    wsum = wpyr + total;
    wcur = wsum + n;
    gray = wcur + n;
    ta = gray + n;
    tb = ta + 3 * n;

    /* The weights' sum, in image order. */
    for (i = 0; i < image_count; i++) {
        ALWAN_CORE_FN(fu_load)(pyr, images[i], in_row_stride, width, height);
        ALWAN_CORE_FN(fu_weights)(wcur, pyr, width, height, gray, expo, pr->ignore, two_sigma2);
        for (p = 0; p < n; p++) wsum[p] = i == 0 ? wcur[p] : wsum[p] + wcur[p];
    }

    for (p = 0; p < 3 * total; p++) res[p] = ALWAN_CORE_ZERO;
    for (i = 0; i < image_count; i++) {
        ALWAN_CORE_FN(fu_load)(pyr, images[i], in_row_stride, width, height);
        ALWAN_CORE_FN(fu_weights)(wcur, pyr, width, height, gray, expo, pr->ignore, two_sigma2);
        for (p = 0; p < n; p++) wpyr[p] = wcur[p] / wsum[p];
        for (l = 1; l < levels; l++) {
            ALWAN_CORE_FN(fu_down)(pyr + 3 * off[l], pyr + 3 * off[l - 1], lw[l - 1], lh[l - 1], 3, tb);
            ALWAN_CORE_FN(fu_down)(wpyr + off[l], wpyr + off[l - 1], lw[l - 1], lh[l - 1], 1, tb);
        }
        for (l = 0; l < levels; l++) {
            size_t const m = lw[l] * lh[l];
            ALWAN_CORE_T *g = pyr + 3 * off[l];
            ALWAN_CORE_T const *wl = wpyr + off[l];
            ALWAN_CORE_T *r = res + 3 * off[l];
            if (l + 1 < levels) {
                ALWAN_CORE_FN(fu_up)(ta, lw[l], lh[l], pyr + 3 * off[l + 1], lw[l + 1], lh[l + 1], 3, tb);
                for (p = 0; p < 3 * m; p++) g[p] -= ta[p];
            }
            for (p = 0; p < m; p++) {
                r[3 * p + 0] += g[3 * p + 0] * wl[p];
                r[3 * p + 1] += g[3 * p + 1] * wl[p];
                r[3 * p + 2] += g[3 * p + 2] * wl[p];
            }
        }
    }

    for (l = levels - 1; l > 0; l--) {
        size_t const m = lw[l - 1] * lh[l - 1];
        ALWAN_CORE_T *r = res + 3 * off[l - 1];
        ALWAN_CORE_FN(fu_up)(ta, lw[l - 1], lh[l - 1], res + 3 * off[l], lw[l], lh[l], 3, tb);
        for (p = 0; p < 3 * m; p++) r[p] += ta[p];
    }
    for (y = 0; y < height; y++) {
        ALWAN_CORE_T *o = (ALWAN_CORE_T *)((char *)rgb_out + y * out_row_stride);
        for (x = 0; x < 3 * width; x++) o[x] = res[y * 3 * width + x];
    }
    ALWAN_FREE(buf);
    return ALWAN_OK;
}
