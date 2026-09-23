/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * A discrete Fourier transform of any length, in double, for the methods that solve in
 * the frequency domain. A power of two runs the iterative radix-2 Cooley-Tukey transform;
 * any other length n runs Bluestein's chirp-z algorithm (1970), which writes the DFT as
 * a convolution with the chirp c_k = exp(-i pi k^2 / n), done by radix-2 transforms of a
 * power-of-two length m >= 2n - 1:
 *
 *   X_k = c_k sum_j (x_j c_j) conj(c_{k-j})     since  jk = (j^2 + k^2 - (k - j)^2) / 2
 *
 * The chirp's angle is taken from k^2 mod 2n, so it stays accurate for large k, and every
 * twiddle factor is its own cos and sin rather than a product of others. The forward
 * transform is X_k = sum_j x_j exp(-2 pi i jk / n); the inverse has the opposite sign and
 * is not scaled, so a round trip multiplies by n.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <string.h>

struct alwan__fft {
    size_t n, m;              /* the length, and the power of two the transforms run at */
    double *tw_re, *tw_im;    /* exp(-2 pi i t / m), t < m / 2 */
    double *ch_re, *ch_im;    /* Bluestein: the chirp c_k, k < n */
    double *b_re, *b_im;      /* Bluestein: the transformed conjugate chirp, m values */
    double *w_re, *w_im;      /* Bluestein: scratch, m values */
};

static int alwan__fft_pow2(size_t n) {
    return n != 0 && (n & (n - 1)) == 0;
}

/* In place, forward (exp(-...)), length m a power of two. */
static void alwan__fft_radix2(double *re, double *im, size_t m, double const *tw_re, double const *tw_im) {
    size_t i, j, len;
    for (i = 1, j = 0; i < m; i++) {    /* bit reversal */
        size_t bit = m >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re[i];
            re[i] = re[j];
            re[j] = t;
            t = im[i];
            im[i] = im[j];
            im[j] = t;
        }
    }
    for (len = 2; len <= m; len <<= 1) {
        size_t const half = len >> 1, step = m / len;
        for (i = 0; i < m; i += len) {
            for (j = 0; j < half; j++) {
                double const wr = tw_re[j * step], wi = tw_im[j * step];
                double const xr = re[i + j + half], xi = im[i + j + half];
                double const tr = xr * wr - xi * wi, ti = xr * wi + xi * wr;
                re[i + j + half] = re[i + j] - tr;
                im[i + j + half] = im[i + j] - ti;
                re[i + j] += tr;
                im[i + j] += ti;
            }
        }
    }
}

void alwan__fft_destroy(alwan__fft *f) {
    if (!f) return;
    if (f->tw_re) ALWAN_FREE(f->tw_re);
    ALWAN_FREE(f);
}

alwan__fft *alwan__fft_create(size_t n) {
    alwan__fft *f;
    size_t m = 1, k, total;
    double const pi = 3.14159265358979323846;
    if (n == 0 || n > ((size_t)1 << 28)) return NULL;
    if (alwan__fft_pow2(n)) {
        m = n;
    } else {
        while (m < 2 * n - 1) m <<= 1;
    }
    f = (alwan__fft *)ALWAN_ALLOC(sizeof(alwan__fft), sizeof(double));
    if (!f) return NULL;
    memset(f, 0, sizeof(*f));
    f->n = n;
    f->m = m;
    total = m + (m == n ? 0 : 2 * n + 4 * m);
    f->tw_re = (double *)ALWAN_ALLOC(total * sizeof(double), sizeof(double));
    if (!f->tw_re) {
        ALWAN_FREE(f);
        return NULL;
    }
    f->tw_im = f->tw_re + m / 2;
    for (k = 0; k < m / 2; k++) {
        double const a = -2.0 * pi * (double)k / (double)m;
        f->tw_re[k] = cos(a);
        f->tw_im[k] = sin(a);
    }
    if (m != n) {
        f->ch_re = f->tw_re + m;
        f->ch_im = f->ch_re + n;
        f->b_re = f->ch_im + n;
        f->b_im = f->b_re + m;
        f->w_re = f->b_im + m;
        f->w_im = f->w_re + m;
        for (k = 0; k < n; k++) {
            unsigned long long const k2 = ((unsigned long long)k * (unsigned long long)k) % (2ull * n);
            double const a = -pi * (double)k2 / (double)n;
            f->ch_re[k] = cos(a);
            f->ch_im[k] = sin(a);
        }
        for (k = 0; k < m; k++) f->b_re[k] = f->b_im[k] = 0.0;
        for (k = 0; k < n; k++) {  /* conj(c_k) at k and at m - k */
            f->b_re[k] = f->ch_re[k];
            f->b_im[k] = -f->ch_im[k];
            if (k) {
                f->b_re[m - k] = f->ch_re[k];
                f->b_im[m - k] = -f->ch_im[k];
            }
        }
        alwan__fft_radix2(f->b_re, f->b_im, m, f->tw_re, f->tw_im);
    }
    return f;
}

/* The forward transform in place. */
static void alwan__fft_forward(alwan__fft const *f, double *re, double *im) {
    size_t const n = f->n, m = f->m;
    size_t k;
    if (m == n) {
        alwan__fft_radix2(re, im, m, f->tw_re, f->tw_im);
        return;
    }
    for (k = 0; k < n; k++) {
        f->w_re[k] = re[k] * f->ch_re[k] - im[k] * f->ch_im[k];
        f->w_im[k] = re[k] * f->ch_im[k] + im[k] * f->ch_re[k];
    }
    for (; k < m; k++) f->w_re[k] = f->w_im[k] = 0.0;
    alwan__fft_radix2(f->w_re, f->w_im, m, f->tw_re, f->tw_im);
    for (k = 0; k < m; k++) {  /* times the chirp's transform, then the inverse by conjugation */
        double const r = f->w_re[k] * f->b_re[k] - f->w_im[k] * f->b_im[k];
        double const i = f->w_re[k] * f->b_im[k] + f->w_im[k] * f->b_re[k];
        f->w_re[k] = r;
        f->w_im[k] = -i;
    }
    alwan__fft_radix2(f->w_re, f->w_im, m, f->tw_re, f->tw_im);
    for (k = 0; k < n; k++) {  /* conj, / m, times c_k */
        double const cr = f->w_re[k] / (double)m, ci = -f->w_im[k] / (double)m;
        re[k] = cr * f->ch_re[k] - ci * f->ch_im[k];
        im[k] = cr * f->ch_im[k] + ci * f->ch_re[k];
    }
}

void alwan__fft_run(alwan__fft const *f, double *re, double *im, int inverse) {
    size_t k;
    if (!inverse) {
        alwan__fft_forward(f, re, im);
        return;
    }
    for (k = 0; k < f->n; k++) im[k] = -im[k];
    alwan__fft_forward(f, re, im);
    for (k = 0; k < f->n; k++) im[k] = -im[k];
}
