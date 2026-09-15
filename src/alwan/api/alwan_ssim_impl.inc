/* No include guard -- included twice (f32 and f64 passes) */

/*
 * SSIM, Wang, Bovik, Sheikh and Simoncelli 2004, as scikit-image's
 * structural_similarity computes it with the paper's settings (gaussian_weights=True,
 * sigma=1.5, use_sample_covariance=False), which the tests hold this to:
 *
 *   mu, sigma^2 and sigma_xy from an 11-tap Gaussian window of sigma 1.5 (scipy's
 *   gaussian_filter, truncate 3.5), borders reflected with the edge sample repeated;
 *   S = (2 mu_x mu_y + C1)(2 sigma_xy + C2) / ((mu_x^2 + mu_y^2 + C1)(sigma_x^2 + sigma_y^2 + C2)),
 *   C1 = (0.01 L)^2, C2 = (0.03 L)^2 with L the data range;
 *   the index is the mean of S with a 5-pixel strip dropped at every edge.
 *
 * MATLAB's ssim, which pu21_metric.m calls, pads and averages differently at the
 * borders, so the two agree in the interior and not at the edges.
 */

#ifndef ALWAN__SSIM_ONCE
#define ALWAN__SSIM_ONCE

#define ALWAN__SSIM_RADIUS 5          /* int(3.5 sigma + 0.5): scipy's truncate 3.5 */
#define ALWAN__SSIM_TAPS   11
#define ALWAN__SSIM_SIGMA  1.5
#define ALWAN__SSIM_K1     0.01
#define ALWAN__SSIM_K2     0.03

/* pu21_metric.m's get_luminance. */
#define ALWAN__PU21_Y_R 0.212656
#define ALWAN__PU21_Y_G 0.715158
#define ALWAN__PU21_Y_B 0.072186

/* scipy.ndimage 'reflect': d c b a | a b c d | d c b a */
static size_t alwan__ssim_fold(ptrdiff_t i, size_t n) {
    ptrdiff_t const period = (ptrdiff_t)(2u * n);
    i %= period;
    if (i < 0) i += period;
    return i < (ptrdiff_t)n ? (size_t)i : (size_t)(period - 1 - i);
}

/* The window along one axis, normalised as scipy's _gaussian_kernel1d. */
static void alwan__ssim_taps(double k[ALWAN__SSIM_TAPS]) {
    double sum = 0.0;
    int i;
    for (i = 0; i < ALWAN__SSIM_TAPS; i++) {
        double const x = (double)(i - ALWAN__SSIM_RADIUS);
        k[i] = ALWAN_EXP(-0.5 * (x * x) / (ALWAN__SSIM_SIGMA * ALWAN__SSIM_SIGMA));
        sum += k[i];
    }
    for (i = 0; i < ALWAN__SSIM_TAPS; i++) k[i] /= sum;
}

/* Separable Gaussian, down the columns then along the rows, as gaussian_filter
 * does axis 0 then axis 1. tmp holds w x h. */
static void alwan__ssim_blur(double *dst, double const *src, size_t w, size_t h, double const k[ALWAN__SSIM_TAPS],
                             double *tmp) {
    size_t x, y;
    int t;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            double acc = 0.0;
            for (t = 0; t < ALWAN__SSIM_TAPS; t++) {
                acc += k[t] * src[alwan__ssim_fold((ptrdiff_t)y + t - ALWAN__SSIM_RADIUS, h) * w + x];
            }
            tmp[y * w + x] = acc;
        }
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            double acc = 0.0;
            for (t = 0; t < ALWAN__SSIM_TAPS; t++) {
                acc += k[t] * tmp[y * w + alwan__ssim_fold((ptrdiff_t)x + t - ALWAN__SSIM_RADIUS, w)];
            }
            dst[y * w + x] = acc;
        }
    }
}

/* The index of two w x h planes, both at least 11 x 11. */
static alwan_status alwan__ssim_planes(double *out, double const *a, double const *b, size_t w, size_t h,
                                       double range) {
    size_t const n = w * h;
    double k[ALWAN__SSIM_TAPS];
    double *buf, *prod, *tmp, *ux, *uy, *uxx, *uyy, *uxy;
    double const c1 = (ALWAN__SSIM_K1 * range) * (ALWAN__SSIM_K1 * range);
    double const c2 = (ALWAN__SSIM_K2 * range) * (ALWAN__SSIM_K2 * range);
    double sum = 0.0;
    size_t x, y, p;

    buf = (double *)ALWAN_ALLOC(7 * n * sizeof(double), sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    prod = buf; tmp = prod + n; ux = tmp + n; uy = ux + n; uxx = uy + n; uyy = uxx + n; uxy = uyy + n;
    alwan__ssim_taps(k);
    alwan__ssim_blur(ux, a, w, h, k, tmp);
    alwan__ssim_blur(uy, b, w, h, k, tmp);
    for (p = 0; p < n; p++) prod[p] = a[p] * a[p];
    alwan__ssim_blur(uxx, prod, w, h, k, tmp);
    for (p = 0; p < n; p++) prod[p] = b[p] * b[p];
    alwan__ssim_blur(uyy, prod, w, h, k, tmp);
    for (p = 0; p < n; p++) prod[p] = a[p] * b[p];
    alwan__ssim_blur(uxy, prod, w, h, k, tmp);
    for (y = ALWAN__SSIM_RADIUS; y < h - ALWAN__SSIM_RADIUS; y++) {
        for (x = ALWAN__SSIM_RADIUS; x < w - ALWAN__SSIM_RADIUS; x++) {
            size_t const q = y * w + x;
            double const vx = uxx[q] - ux[q] * ux[q];
            double const vy = uyy[q] - uy[q] * uy[q];
            double const vxy = uxy[q] - ux[q] * uy[q];
            double const a1 = 2.0 * ux[q] * uy[q] + c1, a2 = 2.0 * vxy + c2;
            double const b1 = ux[q] * ux[q] + uy[q] * uy[q] + c1, b2 = vx + vy + c2;
            sum += (a1 * a2) / (b1 * b2);
        }
    }
    *out = sum / ((double)(w - 2 * ALWAN__SSIM_RADIUS) * (double)(h - 2 * ALWAN__SSIM_RADIUS));
    ALWAN_FREE(buf);
    return ALWAN_OK;
}

static int alwan__ssim_size_ok(size_t w, size_t h) {
    size_t const n = w * h;
    return w >= ALWAN__SSIM_TAPS && h >= ALWAN__SSIM_TAPS && n / w == h
        && n <= ((size_t)-1) / (8u * sizeof(double));
}

#endif /* ALWAN__SSIM_ONCE */

alwan_status ALWAN_CORE_FNLIT(alwan_ssim)(ALWAN_CORE_T *ssim_out, ALWAN_CORE_T const *test, size_t test_row_stride,
                                          ALWAN_CORE_T const *ref, size_t ref_row_stride, size_t width, size_t height,
                                          ALWAN_CORE_T data_range) {
    double *a, *b, v = 0.0;
    alwan_status st;
    size_t x, y;
    double const range = (double)data_range;
    if (!ssim_out || !test || !ref || !alwan__ssim_size_ok(width, height) || !(range > 0.0) || range - range != 0.0) {
        return ALWAN_E_INVALID;
    }
    a = (double *)ALWAN_ALLOC(2 * width * height * sizeof(double), sizeof(double));
    if (!a) return ALWAN_E_NOMEM;
    b = a + width * height;
    for (y = 0; y < height; y++) {
        ALWAN_CORE_T const *t = (ALWAN_CORE_T const *)((char const *)test + y * test_row_stride);
        ALWAN_CORE_T const *r = (ALWAN_CORE_T const *)((char const *)ref + y * ref_row_stride);
        for (x = 0; x < width; x++) {
            a[y * width + x] = (double)t[x];
            b[y * width + x] = (double)r[x];
        }
    }
    st = alwan__ssim_planes(&v, a, b, width, height, range);
    if (st == ALWAN_OK) *ssim_out = (ALWAN_CORE_T)v;
    ALWAN_FREE(a);
    return st;
}

alwan_status ALWAN_CORE_FNLIT(alwan_pu21_ssim)(ALWAN_CORE_T *ssim_out, ALWAN_CORE_T const *test,
                                               size_t test_row_stride, ALWAN_CORE_T const *ref, size_t ref_row_stride,
                                               size_t width, size_t height, size_t channels,
                                               alwan_pu21_variant variant) {
    double *a, *b, v = 0.0;
    alwan_status st = ALWAN_OK;
    size_t x, y, i;
    if (!ssim_out || !test || !ref || !alwan__ssim_size_ok(width, height) || (channels != 1 && channels != 3)
        || (int)variant < (int)ALWAN_PU21_BANDING_GLARE || (int)variant > (int)ALWAN_PU21_PEAKS_GLARE) {
        return ALWAN_E_INVALID;
    }
    a = (double *)ALWAN_ALLOC(2 * width * height * sizeof(double), sizeof(double));
    if (!a) return ALWAN_E_NOMEM;
    b = a + width * height;
    for (y = 0; y < height && st == ALWAN_OK; y++) {
        for (i = 0; i < 2 && st == ALWAN_OK; i++) {
            ALWAN_CORE_T const *row = (ALWAN_CORE_T const *)(i == 0
                ? (char const *)test + y * test_row_stride : (char const *)ref + y * ref_row_stride);
            double *dst = (i == 0 ? a : b) + y * width;
            for (x = 0; x < width && st == ALWAN_OK; x++) {
                ALWAN_CORE_T const *px = row + x * channels;
                double lum = channels == 3
                    ? (double)px[0] * ALWAN__PU21_Y_R + (double)px[1] * ALWAN__PU21_Y_G + (double)px[2] * ALWAN__PU21_Y_B
                    : (double)px[0];
                ALWAN_CORE_T enc = ALWAN_CORE_ZERO;
                /* pu21_encoder.m's range, which pu21_metric enforces through encode. */
                lum = lum < 0.005 ? 0.005 : (lum > 10000.0 ? 10000.0 : lum);
                st = ALWAN_CORE_FNLIT(alwan_pu21_encode)(&enc, (ALWAN_CORE_T)lum, variant);
                dst[x] = (double)enc;
            }
        }
    }
    if (st == ALWAN_OK) st = alwan__ssim_planes(&v, a, b, width, height, 256.0);
    if (st == ALWAN_OK) *ssim_out = (ALWAN_CORE_T)v;
    ALWAN_FREE(a);
    return st;
}
