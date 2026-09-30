/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Gradient-domain image editing: Poisson image editing and its relatives.
 *
 * A port of OpenCV 5.0.0's photo module (modules/photo/src/seamless_cloning.cpp,
 * seamless_cloning_impl.cpp and seamless_cloning.hpp: cv::seamlessClone, cv::colorChange,
 * cv::illuminationChange, cv::textureFlattening and the Cloning class behind them), after
 * P. Perez, M. Gangnet and A. Blake, "Poisson Image Editing", SIGGRAPH 2003. OpenCV is
 * Copyright the OpenCV authors, Apache License 2.0 (https://github.com/opencv/opencv); its
 * notice is kept here and the licence text is data/opencv/LICENSE-opencv.txt. The OpenCV
 * calls the Cloning class makes are reproduced from the same sources, so that the 8-bit
 * result is OpenCV's to the bit (suite 272, cv2 5.0.0 with IPP off):
 *
 *   - filter2D with the 2-tap difference kernels (gradients of the 8-bit images, reflect-101
 *     border; a view reads its parent image beyond its edge, as OpenCV's filters do on a
 *     region of interest) and the Laplacian of the boundary image;
 *   - erode with a 3 x 3 square, three iterations (one 7 x 7 pass, the border ignored);
 *   - BGR2GRAY on 8 bits, ((b 3735 + g 19235 + r 9798 + 2^14) >> 15);
 *   - the discrete sine transform of Cloning::dst, built on cv::dft of complex rows: the
 *     mixed-radix transform of core/src/dxt.cpp (DFTFactorize, DFTInit's permutation and
 *     twiddle recurrence, the radix-2, -3, -5 and odd-factor passes, and the SSE3 radix-4
 *     pass the x64 build takes, whose operation order is followed in scalar code; the
 *     port is api/alwan_cv_dxt.c, shared with ALWAN_DENOISE_DCT_OPENCV's cv::dct);
 *   - magnitude (sqrt of fma(x, x, y y), the AVX2 build's), cv::pow on floats (logf, the
 *     product in double, expf, as the MSVC build of mathfuncs_core computes it; the integer
 *     and half powers by their own paths) and patchNaNs;
 *   - Canny on the 3-channel masked image (imgproc/src/canny.cpp: Sobel of aperture 3, 5 or
 *     7 with a replicated border, the L1 magnitude, the channel of largest magnitude,
 *     non-maximum suppression with OpenCV's integer tangent test, and hysteresis).
 *
 * The images are 8-bit, R first; internally the channels are kept in OpenCV's B, G, R order,
 * which decides the grey weights, which channel wins a tie in Canny and which gain goes with
 * which channel. Every other step works channel by channel.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * cv::dft on complex float rows: the shared port in api/alwan_cv_dxt.c
 * ================================================================ */

typedef alwan__cv_cf ge_cf;
typedef alwan__cv_dft_plan ge_dft_plan;
#define ge_dft_plan_create alwan__cv_dft_plan_create
#define ge_dft_plan_free alwan__cv_dft_plan_free
#define ge_dft_row alwan__cv_dft_row

#define GE_PI 3.1415926535897932384626433832795

/* the offset of (x, y) in a row-major grid w wide; all three are non-negative ints */
static size_t ge_at(int y, int w, int x) {
    return (size_t)y * (size_t)w + (size_t)x;
}

/* ================================================================
 * Cloning::dst and the Poisson solve
 * ================================================================ */

typedef struct {
    ge_dft_plan row;   /* length 2 cols + 2 */
    ge_dft_plan col;   /* length 2 rows + 2 */
    ge_cf *in;         /* one row of either length */
    ge_cf *out;
    float *im1;        /* rows x (2 cols + 2) */
    float *im2;        /* cols x (2 rows + 2) */
    float *md;         /* rows x cols */
    float *res;
    float *fx, *fy;
} ge_solver;

static void ge_solver_free(ge_solver *s) {
    ge_dft_plan_free(&s->row);
    ge_dft_plan_free(&s->col);
    free(s->in);
    free(s->out);
    free(s->im1);
    free(s->im2);
    free(s->md);
    free(s->res);
    free(s->fx);
    free(s->fy);
    memset(s, 0, sizeof *s);
}

/* w x h is the image; the transform runs on its interior, (w - 2) x (h - 2). */
static int ge_solver_create(ge_solver *s, int w, int h) {
    int const cols = w - 2, rows = h - 2;
    int const l1 = 2 * cols + 2, l2 = 2 * rows + 2;
    int const lmax = l1 > l2 ? l1 : l2;
    int i;
    double scale;

    memset(s, 0, sizeof *s);
    if (!ge_dft_plan_create(&s->row, l1) || !ge_dft_plan_create(&s->col, l2)) return 0;
    s->in = (ge_cf *)malloc((size_t)lmax * sizeof(ge_cf));
    s->out = (ge_cf *)malloc((size_t)lmax * sizeof(ge_cf));
    s->im1 = (float *)malloc((size_t)rows * (size_t)l1 * sizeof(float));
    s->im2 = (float *)malloc((size_t)cols * (size_t)l2 * sizeof(float));
    s->md = (float *)malloc((size_t)rows * (size_t)cols * sizeof(float));
    s->res = (float *)malloc((size_t)rows * (size_t)cols * sizeof(float));
    s->fx = (float *)malloc((size_t)cols * sizeof(float));
    s->fy = (float *)malloc((size_t)rows * sizeof(float));
    if (!s->in || !s->out || !s->im1 || !s->im2 || !s->md || !s->res || !s->fx || !s->fy) return 0;

    scale = GE_PI / (w - 1);
    for (i = 0; i < cols; ++i) s->fx[i] = 2.0f * (float)ALWAN_COS_F64(scale * (i + 1));
    scale = GE_PI / (h - 1);
    for (i = 0; i < rows; ++i) s->fy[i] = 2.0f * (float)ALWAN_COS_F64(scale * (i + 1));
    return 1;
}

/* Cloning::dst: rows x cols in, rows x cols out (dest may be src) */
static void ge_dst(ge_solver *s, float const *src, float *dest, int rows, int cols, int invert) {
    int const l1 = 2 * cols + 2, l2 = 2 * rows + 2;
    int i, j;

    for (j = 0; j < rows; ++j) {
        float const *line = src + (size_t)j * (size_t)cols;
        for (i = 0; i < l1; ++i) {
            s->in[i].re = 0.f;
            s->in[i].im = 0.f;
        }
        for (i = 0; i < cols; ++i) s->in[i + 1].re = line[i];
        for (i = 0; i < cols; ++i) s->in[cols + 2 + i].re = -line[cols - 1 - i];
        ge_dft_row(&s->row, s->in, s->out, invert);
        for (i = 0; i < l1; ++i) s->im1[(size_t)j * (size_t)l1 + (size_t)i] = s->out[i].im;
    }

    for (j = 0; j < cols; ++j) {
        for (i = 0; i < l2; ++i) {
            s->in[i].re = 0.f;
            s->in[i].im = 0.f;
        }
        for (i = 0; i < rows; ++i) {
            float const val = s->im1[(size_t)i * (size_t)l1 + (size_t)(j + 1)];
            s->in[i + 1].re = val;
            s->in[l2 - 1 - i].re = -val;
        }
        ge_dft_row(&s->col, s->in, s->out, invert);
        for (i = 0; i < l2; ++i) s->im2[(size_t)j * (size_t)l2 + (size_t)i] = s->out[i].im;
    }

    for (j = 0; j < rows; ++j) {
        for (i = 0; i < cols; ++i) {
            dest[(size_t)j * (size_t)cols + (size_t)i] = s->im2[(size_t)i * (size_t)l2 + (size_t)(j + 1)];
        }
    }
}

/* Cloning::poissonSolver + solve for one channel. img is w x h (the destination channel),
 * lx and ly the two Laplacians of the guidance field; out gets w x h bytes. */
static void ge_poisson(ge_solver *s, unsigned char const *img, float const *lx, float const *ly,
                       int w, int h, unsigned char *out) {
    int const cols = w - 2, rows = h - 2;
    int x, y;

    for (y = 1; y < h - 1; ++y) {
        for (x = 1; x < w - 1; ++x) {
            size_t const k = ge_at(y, w, x);
            /* the Laplacian of the boundary image: img on the frame, 0 inside */
            int const up = (y - 1 == 0) ? img[k - (size_t)w] : 0;
            int const dn = (y + 1 == h - 1) ? img[k + (size_t)w] : 0;
            int const lf = (x - 1 == 0) ? img[k - 1] : 0;
            int const rt = (x + 1 == w - 1) ? img[k + 1] : 0;
            float const bl = (float)(up + lf + rt + dn);
            float const lap = lx[k] + ly[k];
            s->md[(size_t)(y - 1) * (size_t)cols + (size_t)(x - 1)] = lap - bl;
        }
    }

    ge_dst(s, s->md, s->res, rows, cols, 0);
    for (y = 0; y < rows; ++y) {
        for (x = 0; x < cols; ++x) {
            float t = s->fx[x] + s->fy[y];
            t = t - 4.0f;
            s->res[(size_t)y * (size_t)cols + (size_t)x] /= t;
        }
    }
    ge_dst(s, s->res, s->md, rows, cols, 1);

    memcpy(out, img, (size_t)w * (size_t)h);
    for (y = 1; y < h - 1; ++y) {
        for (x = 1; x < w - 1; ++x) {
            float const v = s->md[(size_t)(y - 1) * (size_t)cols + (size_t)(x - 1)];
            unsigned char c;
            /* OpenCV truncates, not rounds; a NaN falls through its tests to 0 on x64 */
            if (v < 0.f) c = 0;
            else if (v > 255.0f) c = 255;
            else if (v == v) c = (unsigned char)v;
            else c = 0;
            out[(size_t)y * (size_t)w + (size_t)x] = c;
        }
    }
}

/* ================================================================
 * Canny (imgproc/src/canny.cpp), 3 channels, L1 magnitude
 * ================================================================ */

static int ge_round_half_even(double v) {
    double r = ALWAN_FLOOR_F64(v + 0.5);
    if (r - v == 0.5 && ALWAN_FMOD_F64(r, 2.0) != 0.0) r -= 1.0;
    return (int)r;
}

static int ge_clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* img: w x h x 3 bytes (OpenCV channel order). edges: w x h, 255 on an edge. */
static int ge_canny(unsigned char const *img, int w, int h, double low_thresh, double high_thresh,
                    int aperture, unsigned char *edges) {
    static int const smooth3[3] = {1, 2, 1}, deriv3[3] = {-1, 0, 1};
    static int const smooth5[5] = {1, 4, 6, 4, 1}, deriv5[5] = {-1, -2, 0, 2, 1};
    static int const smooth7[7] = {1, 6, 15, 20, 15, 6, 1}, deriv7[7] = {-1, -4, -5, 0, 5, 4, 1};
    int const *sm = aperture == 3 ? smooth3 : (aperture == 5 ? smooth5 : smooth7);
    int const *dv = aperture == 3 ? deriv3 : (aperture == 5 ? deriv5 : deriv7);
    int const r = aperture / 2;
    size_t const npx = (size_t)w * (size_t)h;
    short *dx = NULL, *dy = NULL;
    int *mag = NULL;
    unsigned char *map = NULL;
    int *stack = NULL;
    size_t sp = 0;
    int low, high, x, y, c, i, j;
    int const mw = w + 2;

    if (aperture == 7) {
        low_thresh = low_thresh / 16.0;
        high_thresh = high_thresh / 16.0;
    }
    if (low_thresh > high_thresh) {
        double t = low_thresh;
        low_thresh = high_thresh;
        high_thresh = t;
    }
    low = (int)ALWAN_FLOOR_F64(low_thresh);
    high = (int)ALWAN_FLOOR_F64(high_thresh);

    dx = (short *)malloc(npx * sizeof(short));
    dy = (short *)malloc(npx * sizeof(short));
    mag = (int *)malloc(npx * sizeof(int));
    map = (unsigned char *)malloc((size_t)mw * (size_t)(h + 2));
    stack = (int *)malloc((size_t)mw * (size_t)(h + 2) * sizeof(int));
    if (!dx || !dy || !mag || !map || !stack) {
        free(dx); free(dy); free(mag); free(map); free(stack);
        return 0;
    }

    /* Sobel per channel, replicated border, keeping the channel of largest |dx| + |dy| */
    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            size_t const k = ge_at(y, w, x);
            int best = -1;
            short bx = 0, by = 0;
            for (c = 0; c < 3; ++c) {
                long sx = 0, sy = 0;
                int gx, gy, m;
                for (j = -r; j <= r; ++j) {
                    int const yy = ge_clampi(y + j, 0, h - 1);
                    long rowx = 0, rowy = 0;
                    for (i = -r; i <= r; ++i) {
                        int const xx = ge_clampi(x + i, 0, w - 1);
                        int const v = img[((size_t)yy * (size_t)w + (size_t)xx) * 3 + (size_t)c];
                        rowx += (long)dv[i + r] * v;
                        rowy += (long)sm[i + r] * v;
                    }
                    sx += (long)sm[j + r] * rowx;
                    sy += (long)dv[j + r] * rowy;
                }
                if (aperture == 7) {
                    gx = ge_round_half_even((double)sx / 16.0);
                    gy = ge_round_half_even((double)sy / 16.0);
                } else {
                    gx = (int)sx;
                    gy = (int)sy;
                }
                gx = ge_clampi(gx, -32768, 32767);
                gy = ge_clampi(gy, -32768, 32767);
                m = (gx < 0 ? -gx : gx) + (gy < 0 ? -gy : gy);
                if (m > best) {
                    best = m;
                    bx = (short)gx;
                    by = (short)gy;
                }
            }
            mag[k] = best;
            dx[k] = bx;
            dy[k] = by;
        }
    }

    /* map: 1 cannot be an edge, 0 might, 2 is; a frame of 1 around the image */
    memset(map, 1, (size_t)mw * (size_t)(h + 2));
    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            size_t const k = ge_at(y, w, x);
            int const m = mag[k];
            size_t const mk = ge_at(y + 1, mw, x + 1);
            int ok = 0;
            if (m > low) {
                int const xs = dx[k], ys = dy[k];
                int const ax = xs < 0 ? -xs : xs;
                int const ay = (ys < 0 ? -ys : ys) << 15;
                int const tg22x = ax * 13573;
                int const mp = x > 0 ? mag[k - 1] : 0;
                int const mn = x < w - 1 ? mag[k + 1] : 0;
                if (ay < tg22x) {
                    ok = m > mp && m >= mn;
                } else {
                    int const tg67x = tg22x + (ax << 16);
                    if (ay > tg67x) {
                        int const mu = y > 0 ? mag[k - (size_t)w] : 0;
                        int const md = y < h - 1 ? mag[k + (size_t)w] : 0;
                        ok = m > mu && m >= md;
                    } else {
                        int const s = (xs ^ ys) < 0 ? -1 : 1;
                        int const xu = x - s, xd = x + s;
                        int const mu = (y > 0 && xu >= 0 && xu < w) ? mag[(size_t)(y - 1) * (size_t)w + (size_t)xu] : 0;
                        int const md = (y < h - 1 && xd >= 0 && xd < w) ? mag[(size_t)(y + 1) * (size_t)w + (size_t)xd] : 0;
                        ok = m > mu && m > md;
                    }
                }
            }
            if (ok) {
                if (m > high) {
                    map[mk] = 2;
                    stack[sp++] = (int)mk;
                } else {
                    map[mk] = 0;
                }
            } else {
                map[mk] = 1;
            }
        }
    }

    while (sp > 0) {
        int const mk = stack[--sp];
        static int const off_y[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
        static int const off_x[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
        for (i = 0; i < 8; ++i) {
            int const nk = mk + off_y[i] * mw + off_x[i];
            if (!map[nk]) {
                map[nk] = 2;
                stack[sp++] = nk;
            }
        }
    }

    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            edges[(size_t)y * (size_t)w + (size_t)x] =
                map[(size_t)(y + 1) * (size_t)mw + (size_t)(x + 1)] == 2 ? 255 : 0;
        }
    }

    free(dx); free(dy); free(mag); free(map); free(stack);
    return 1;
}

/* ================================================================
 * The Cloning pipeline
 * ================================================================ */

/* the 8-bit difference gradients of one image of w x h x 3 (OpenCV order): gx(x) = I(x+1) -
 * I(x) with reflect-101 at the right edge, of a view (x0, y0, w, h) into a parent of pw x ph
 * whose rows are pstride bytes apart */
static void ge_gradients(unsigned char const *parent, size_t pstride, int pw, int ph, int x0, int y0,
                         int w, int h, float *gx, float *gy) {
    int x, y, c;
    for (y = 0; y < h; ++y) {
        int const py = y0 + y;
        int const pyn = py + 1 < ph ? py + 1 : (ph > 1 ? ph - 2 : 0);
        for (x = 0; x < w; ++x) {
            int const px = x0 + x;
            int const pxn = px + 1 < pw ? px + 1 : (pw > 1 ? pw - 2 : 0);
            for (c = 0; c < 3; ++c) {
                int const v = parent[(size_t)py * pstride + (size_t)px * 3 + (size_t)c];
                int const vr = parent[(size_t)py * pstride + (size_t)pxn * 3 + (size_t)c];
                int const vd = parent[(size_t)pyn * pstride + (size_t)px * 3 + (size_t)c];
                size_t const k = ((size_t)y * (size_t)w + (size_t)x) * 3 + (size_t)c;
                gx[k] = (float)(vr - v);
                gy[k] = (float)(vd - v);
            }
        }
    }
}

/* erode with a 3 x 3 square three times: one 7 x 7 minimum, the window clipped at the image */
static void ge_erode7(unsigned char const *m, int w, int h, unsigned char *out) {
    int x, y, i, j;
    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            int v = 255;
            for (j = y - 3; j <= y + 3; ++j) {
                if (j < 0 || j >= h) continue;
                for (i = x - 3; i <= x + 3; ++i) {
                    int t;
                    if (i < 0 || i >= w) continue;
                    t = m[(size_t)j * (size_t)w + (size_t)i];
                    if (t < v) v = t;
                }
            }
            out[(size_t)y * (size_t)w + (size_t)x] = (unsigned char)v;
        }
    }
}

static float ge_f32_bits(unsigned int bits) {
    float f;
    memcpy(&f, &bits, sizeof f);
    return f;
}

/* cv::pow(x, power) on floats, as mathfuncs.cpp computes it on the MSVC x64 build */
static void ge_pow(float const *x, float *y, size_t n, double power) {
    int const in_range = ALWAN_ABS_F64(power) < 2147483647.0;
    double const rp = in_range ? (double)ge_round_half_even(power) : 0.0;
    int const ipower = (int)rp;
    int const is_ipower = in_range && ALWAN_ABS_F64(rp - power) < 2.2204460492503131e-16;
    size_t i;

    if (is_ipower && ipower == 0) {
        for (i = 0; i < n; ++i) y[i] = 1.f;
        return;
    }
    if (is_ipower && ipower == 1) {
        for (i = 0; i < n; ++i) y[i] = x[i];
        return;
    }
    if (is_ipower && ipower == 2) {
        for (i = 0; i < n; ++i) y[i] = x[i] * x[i];
        return;
    }
    if (is_ipower) {
        int const pw = ipower < 0 ? -ipower : ipower;
        for (i = 0; i < n; ++i) {
            float a = 1, b = x[i];
            int p = pw;
            if (ipower < 0) b = 1 / b;
            while (p > 1) {
                if (p & 1) a *= b;
                b *= b;
                p >>= 1;
            }
            a *= b;
            y[i] = a;
        }
        return;
    }
    if (ALWAN_ABS_F64(ALWAN_ABS_F64(power) - 0.5) < 2.2204460492503131e-16) {
        for (i = 0; i < n; ++i) {
            y[i] = power < 0 ? 1 / ALWAN_SQRT_F32(x[i]) : ALWAN_SQRT_F32(x[i]);
        }
        return;
    }
#if defined(_MSC_VER)
#pragma loop(no_vector)
#endif
    for (i = 0; i < n; ++i) {
        float const x0 = x[i];
        float const l = ALWAN_LN_F32(x0);
        float const t = (float)(l * power);
        float r = ALWAN_EXP_F32(t);
        if (x0 <= 0) {
            if (x0 == 0.f) {
                if (power < 0) r = ge_f32_bits(0x7f800000u);
            } else {
                r = ge_f32_bits(0x7fffffffu);
            }
        }
        y[i] = r;
    }
}

typedef struct {
    int w, h;              /* the region the solve runs on */
    unsigned char *img;    /* w x h x 3, OpenCV order: the image the boundary comes from */
    float *dgx, *dgy;      /* destination gradients, w x h x 3 */
    float *pgx, *pgy;      /* patch gradients */
    unsigned char *wmask;  /* the eroded mask, w x h */
} ge_problem;

/* Cloning::evaluate + poisson: the guidance field in, the w x h x 3 result out */
static alwan_status ge_evaluate(ge_problem *pr, unsigned char *result) {
    int const w = pr->w, h = pr->h;
    size_t const n = (size_t)w * (size_t)h;
    float const k255 = (float)(1.0 / 255.0);
    float *sx = NULL, *sy = NULL, *lx = NULL, *ly = NULL;
    unsigned char *chan = NULL, *outc = NULL;
    ge_solver s;
    size_t i;
    int x, y, c;
    alwan_status st = ALWAN_E_NOMEM;

    memset(&s, 0, sizeof s);
    sx = (float *)malloc(n * 3 * sizeof(float));
    sy = (float *)malloc(n * 3 * sizeof(float));
    lx = (float *)malloc(n * sizeof(float));
    ly = (float *)malloc(n * sizeof(float));
    chan = (unsigned char *)malloc(n);
    outc = (unsigned char *)malloc(n);
    if (!sx || !sy || !lx || !ly || !chan || !outc || !ge_solver_create(&s, w, h)) goto done;

    for (i = 0; i < n; ++i) {
        float const inv = (float)(255 - pr->wmask[i]) * k255;
        for (c = 0; c < 3; ++c) {
            /* arrayProduct, then the sum: two roundings, never one fused */
            float const tx = pr->dgx[i * 3 + (size_t)c] * inv;
            float const ty = pr->dgy[i * 3 + (size_t)c] * inv;
            sx[i * 3 + (size_t)c] = tx + pr->pgx[i * 3 + (size_t)c];
            sy[i * 3 + (size_t)c] = ty + pr->pgy[i * 3 + (size_t)c];
        }
    }

    for (c = 0; c < 3; ++c) {
        /* computeLaplacianX/Y: G(x) - G(x - 1), reflect-101 (the edges are not read) */
        for (y = 0; y < h; ++y) {
            for (x = 0; x < w; ++x) {
                size_t const k = ge_at(y, w, x);
                int const xm = x > 0 ? x - 1 : 1;
                int const ym = y > 0 ? y - 1 : 1;
                lx[k] = sx[k * 3 + (size_t)c] - sx[((size_t)y * (size_t)w + (size_t)xm) * 3 + (size_t)c];
                ly[k] = sy[k * 3 + (size_t)c] - sy[((size_t)ym * (size_t)w + (size_t)x) * 3 + (size_t)c];
            }
        }
        for (i = 0; i < n; ++i) chan[i] = pr->img[i * 3 + (size_t)c];
        ge_poisson(&s, chan, lx, ly, w, h, outc);
        for (i = 0; i < n; ++i) result[i * 3 + (size_t)c] = outc[i];
    }
    st = ALWAN_OK;

done:
    ge_solver_free(&s);
    free(sx); free(sy); free(lx); free(ly); free(chan); free(outc);
    return st;
}

/* ================================================================
 * The public entry point
 * ================================================================ */

static int ge_finite(double v) {
    return v == v && v > -1e308 && v < 1e308;
}

/* an RGB(A) row-major image to w x h x 3 bytes in OpenCV's B, G, R order */
static void ge_to_bgr(unsigned char const *src, size_t stride, size_t channels, int w, int h, unsigned char *out) {
    int x, y;
    for (y = 0; y < h; ++y) {
        unsigned char const *row = src + (size_t)y * stride;
        for (x = 0; x < w; ++x) {
            unsigned char const *px = row + (size_t)x * channels;
            size_t const k = ((size_t)y * (size_t)w + (size_t)x) * 3;
            out[k + 0] = px[2];
            out[k + 1] = px[1];
            out[k + 2] = px[0];
        }
    }
}

alwan_status alwan_gradient_edit(unsigned char *out, size_t out_row_stride,
                                 unsigned char const *src, size_t src_row_stride, size_t src_width, size_t src_height,
                                 unsigned char const *mask, size_t mask_row_stride,
                                 unsigned char const *dst, size_t dst_row_stride, size_t dst_width, size_t dst_height,
                                 size_t channels, alwan_gradient_edit_method method,
                                 alwan_gradient_edit_params const *params) {
    alwan_gradient_edit_params prm;
    int const is_clone = method == ALWAN_GRADIENT_EDIT_CLONE_NORMAL || method == ALWAN_GRADIENT_EDIT_CLONE_MIXED ||
                         method == ALWAN_GRADIENT_EDIT_CLONE_MONOCHROME;
    int sw, sh, dw = 0, dh = 0;
    size_t snpx, i;
    unsigned char *sbgr = NULL, *dbgr = NULL, *m = NULL, *em = NULL, *patch = NULL, *res = NULL, *edges = NULL;
    float *buf = NULL;
    ge_problem pr;
    alwan_status st = ALWAN_E_NOMEM;
    int x, y, c;

    if (!out || !src) return ALWAN_E_INVALID;
    if (method < ALWAN_GRADIENT_EDIT_CLONE_NORMAL || method > ALWAN_GRADIENT_EDIT_TEXTURE_FLATTENING) return ALWAN_E_INVALID;
    if (channels != 3 && channels != 4) return ALWAN_E_INVALID;
    if (src_width < 3 || src_height < 3 || src_width > (size_t)INT_MAX / 8 || src_height > (size_t)INT_MAX / 8) {
        return ALWAN_E_INVALID;
    }
    if (src_width > ((size_t)-1) / 64 / src_height) return ALWAN_E_INVALID;
    if (src_row_stride < src_width * channels) return ALWAN_E_INVALID;
    if (mask && mask_row_stride < src_width) return ALWAN_E_INVALID;
    if (is_clone) {
        if (!dst) return ALWAN_E_INVALID;
        if (dst_width < 3 || dst_height < 3 || dst_width > (size_t)INT_MAX / 8 || dst_height > (size_t)INT_MAX / 8) {
            return ALWAN_E_INVALID;
        }
        if (dst_width > ((size_t)-1) / 64 / dst_height) return ALWAN_E_INVALID;
        if (dst_row_stride < dst_width * channels || out_row_stride < dst_width * channels) return ALWAN_E_INVALID;
    } else if (out_row_stride < src_width * channels) {
        return ALWAN_E_INVALID;
    }

    if (params) prm = *params;
    else memset(&prm, 0, sizeof prm);
    if (!ge_finite(prm.red_mul) || !ge_finite(prm.green_mul) || !ge_finite(prm.blue_mul) || !ge_finite(prm.alpha) ||
        !ge_finite(prm.beta) || !ge_finite(prm.low_threshold) || !ge_finite(prm.high_threshold)) {
        return ALWAN_E_INVALID;
    }
    if (prm.kernel_size != 0 && prm.kernel_size != 3 && prm.kernel_size != 5 && prm.kernel_size != 7) return ALWAN_E_INVALID;
    if (prm.red_mul == 0) prm.red_mul = 1.0;
    if (prm.green_mul == 0) prm.green_mul = 1.0;
    if (prm.blue_mul == 0) prm.blue_mul = 1.0;
    if (prm.alpha == 0) prm.alpha = 0.2;
    if (prm.beta == 0) prm.beta = 0.4;
    if (prm.low_threshold == 0) prm.low_threshold = 30;
    if (prm.high_threshold == 0) prm.high_threshold = 45;
    if (prm.kernel_size == 0) prm.kernel_size = 3;

    sw = (int)src_width;
    sh = (int)src_height;
    snpx = (size_t)sw * (size_t)sh;
    memset(&pr, 0, sizeof pr);

    sbgr = (unsigned char *)malloc(snpx * 3);
    m = (unsigned char *)malloc(snpx);
    em = (unsigned char *)malloc(snpx);
    if (!sbgr || !m || !em) goto done;
    ge_to_bgr(src, src_row_stride, channels, sw, sh, sbgr);
    for (y = 0; y < sh; ++y) {
        for (x = 0; x < sw; ++x) {
            m[(size_t)y * (size_t)sw + (size_t)x] = mask ? mask[(size_t)y * mask_row_stride + (size_t)x] : 255;
        }
    }

    if (is_clone) {
        int rx0 = sw, ry0 = sh, rx1 = -1, ry1 = -1, rw, rh, l, t, cx, cy;

        dw = (int)dst_width;
        dh = (int)dst_height;
        /* seamlessClone clears a one-pixel frame of the mask */
        for (x = 0; x < sw; ++x) {
            m[x] = 0;
            m[(size_t)(sh - 1) * (size_t)sw + (size_t)x] = 0;
        }
        for (y = 0; y < sh; ++y) {
            m[(size_t)y * (size_t)sw] = 0;
            m[(size_t)y * (size_t)sw + (size_t)(sw - 1)] = 0;
        }
        for (y = 0; y < sh; ++y) {
            for (x = 0; x < sw; ++x) {
                if (m[(size_t)y * (size_t)sw + (size_t)x]) {
                    if (x < rx0) rx0 = x;
                    if (x > rx1) rx1 = x;
                    if (y < ry0) ry0 = y;
                    if (y > ry1) ry1 = y;
                }
            }
        }

        if (rx1 < 0) {
            /* nothing to clone: dst as it was */
            for (y = 0; y < dh; ++y) memmove(out + (size_t)y * out_row_stride, dst + (size_t)y * dst_row_stride, (size_t)dw * channels);
            st = ALWAN_OK;
            goto done;
        }
        rw = rx1 - rx0 + 1;
        rh = ry1 - ry0 + 1;
        cx = prm.center_x;
        cy = prm.center_y;
        if (cx == 0 && cy == 0) {
            cx = dw / 2;
            cy = dh / 2;
        }
        if (prm.centre_on_mask_image) {
            l = cx - (sw / 2 - rx0);
            t = cy - (sh / 2 - ry0);
        } else {
            l = cx - rw / 2;
            t = cy - rh / 2;
        }
        if (l < 0 || t < 0 || l > dw - rw || t > dh - rh || rw < 3 || rh < 3) {
            st = ALWAN_E_RANGE;
            goto done;
        }

        dbgr = (unsigned char *)malloc((size_t)dw * (size_t)dh * 3);
        if (!dbgr) goto done;
        ge_to_bgr(dst, dst_row_stride, channels, dw, dh, dbgr);
        ge_erode7(m, sw, sh, em);

        pr.w = rw;
        pr.h = rh;
        {
            size_t const n = (size_t)rw * (size_t)rh;
            pr.img = (unsigned char *)malloc(n * 3);
            patch = (unsigned char *)malloc(n * 3);
            pr.wmask = (unsigned char *)malloc(n);
            buf = (float *)malloc(n * 12 * sizeof(float));
            res = (unsigned char *)malloc(n * 3);
            if (!pr.img || !patch || !pr.wmask || !buf || !res) goto done;
            pr.dgx = buf;
            pr.dgy = buf + n * 3;
            pr.pgx = buf + n * 6;
            pr.pgy = buf + n * 9;
        }
        for (y = 0; y < rh; ++y) {
            for (x = 0; x < rw; ++x) {
                size_t const k = ge_at(y, rw, x);
                size_t const sk = ge_at(ry0 + y, sw, rx0 + x);
                size_t const dk = ge_at(t + y, dw, l + x);
                pr.wmask[k] = em[sk];
                for (c = 0; c < 3; ++c) {
                    pr.img[k * 3 + (size_t)c] = dbgr[dk * 3 + (size_t)c];
                    patch[k * 3 + (size_t)c] = m[sk] ? sbgr[sk * 3 + (size_t)c] : 0;
                }
            }
        }

        ge_gradients(dbgr, (size_t)dw * 3, dw, dh, l, t, rw, rh, pr.dgx, pr.dgy);
        if (method == ALWAN_GRADIENT_EDIT_CLONE_MONOCHROME) {
            size_t const n = (size_t)rw * (size_t)rh;
            unsigned char *grey = res;  /* scratch, three planes wide */
            for (i = 0; i < n; ++i) {
                int const b = patch[i * 3], g = patch[i * 3 + 1], r = patch[i * 3 + 2];
                unsigned char const v = (unsigned char)((b * 3735 + g * 19235 + r * 9798 + (1 << 14)) >> 15);
                grey[i * 3] = grey[i * 3 + 1] = grey[i * 3 + 2] = v;
            }
            ge_gradients(grey, (size_t)rw * 3, rw, rh, 0, 0, rw, rh, pr.pgx, pr.pgy);
        } else {
            ge_gradients(patch, (size_t)rw * 3, rw, rh, 0, 0, rw, rh, pr.pgx, pr.pgy);
        }

        {
            size_t const n = (size_t)rw * (size_t)rh;
            float const k255 = (float)(1.0 / 255.0);
            for (i = 0; i < n; ++i) {
                float const mf = (float)pr.wmask[i] * k255;
                for (c = 0; c < 3; ++c) {
                    size_t const k = i * 3 + (size_t)c;
                    if (method == ALWAN_GRADIENT_EDIT_CLONE_MIXED) {
                        float const pd = pr.pgx[k] - pr.pgy[k];
                        float const dd = pr.dgx[k] - pr.dgy[k];
                        if (ALWAN_ABS_F32(pd) > ALWAN_ABS_F32(dd)) {
                            pr.pgx[k] *= mf;
                            pr.pgy[k] *= mf;
                        } else {
                            pr.pgx[k] = pr.dgx[k] * mf;
                            pr.pgy[k] = pr.dgy[k] * mf;
                        }
                    } else {
                        pr.pgx[k] *= mf;
                        pr.pgy[k] *= mf;
                    }
                }
            }
        }

        st = ge_evaluate(&pr, res);
        if (st != ALWAN_OK) goto done;

        /* out: dst with the region replaced; a fourth channel stays dst's */
        for (y = 0; y < dh; ++y) memmove(out + (size_t)y * out_row_stride, dst + (size_t)y * dst_row_stride, (size_t)dw * channels);
        for (y = 0; y < rh; ++y) {
            for (x = 0; x < rw; ++x) {
                size_t const k = ((size_t)y * (size_t)rw + (size_t)x) * 3;
                unsigned char *px = out + (size_t)(t + y) * out_row_stride + (size_t)(l + x) * channels;
                px[0] = res[k + 2];
                px[1] = res[k + 1];
                px[2] = res[k + 0];
            }
        }
        goto done;
    }

    /* colorChange, illuminationChange, textureFlattening: the whole image */
    {
        size_t const n = snpx;
        float const k255 = (float)(1.0 / 255.0);

        pr.w = sw;
        pr.h = sh;
        pr.img = sbgr;
        pr.wmask = em;
        patch = (unsigned char *)malloc(n * 3);
        buf = (float *)malloc(n * 12 * sizeof(float));
        res = (unsigned char *)malloc(n * 3);
        if (!patch || !buf || !res) goto done;
        pr.dgx = buf;
        pr.dgy = buf + n * 3;
        pr.pgx = buf + n * 6;
        pr.pgy = buf + n * 9;

        for (i = 0; i < n; ++i) {
            for (c = 0; c < 3; ++c) patch[i * 3 + (size_t)c] = m[i] ? sbgr[i * 3 + (size_t)c] : 0;
        }
        ge_gradients(sbgr, (size_t)sw * 3, sw, sh, 0, 0, sw, sh, pr.dgx, pr.dgy);
        ge_gradients(patch, (size_t)sw * 3, sw, sh, 0, 0, sw, sh, pr.pgx, pr.pgy);
        ge_erode7(m, sw, sh, em);

        if (method == ALWAN_GRADIENT_EDIT_TEXTURE_FLATTENING) {
            edges = (unsigned char *)malloc(n);
            if (!edges) goto done;
            if (!ge_canny(patch, sw, sh, (double)(float)prm.low_threshold, (double)(float)prm.high_threshold,
                          prm.kernel_size, edges)) {
                goto done;
            }
            for (i = 0; i < n; ++i) {
                if (edges[i] != 255) {
                    for (c = 0; c < 3; ++c) {
                        pr.pgx[i * 3 + (size_t)c] = 0.f;
                        pr.pgy[i * 3 + (size_t)c] = 0.f;
                    }
                }
            }
        }

        for (i = 0; i < n; ++i) {
            float const mf = (float)em[i] * k255;
            for (c = 0; c < 3; ++c) {
                pr.pgx[i * 3 + (size_t)c] *= mf;
                pr.pgy[i * 3 + (size_t)c] *= mf;
            }
        }

        if (method == ALWAN_GRADIENT_EDIT_COLOR_CHANGE) {
            /* scalarProduct: B by blue_mul, G by green_mul, R by red_mul */
            float const g3[3] = {(float)prm.blue_mul, (float)prm.green_mul, (float)prm.red_mul};
            for (i = 0; i < n; ++i) {
                for (c = 0; c < 3; ++c) {
                    pr.pgx[i * 3 + (size_t)c] *= g3[c];
                    pr.pgy[i * 3 + (size_t)c] *= g3[c];
                }
            }
        } else if (method == ALWAN_GRADIENT_EDIT_ILLUMINATION_CHANGE) {
            float const alpha = (float)prm.alpha, beta = (float)prm.beta;
            float const ab = ALWAN_POW_F32(alpha, beta);
            size_t const n3 = n * 3;
            float *mag = (float *)malloc(n3 * sizeof(float));
            float *pw = (float *)malloc(n3 * sizeof(float));
            if (!mag || !pw) {
                free(mag);
                free(pw);
                goto done;
            }
            for (i = 0; i < n3; ++i) {
                float const gx = pr.pgx[i], gy = pr.pgy[i];
                mag[i] = ALWAN_SQRT_F32(ALWAN_FMA_CR_F32(gx, gx, gy * gy));
            }
            ge_pow(mag, pw, n3, (double)(-1 * beta));
            for (i = 0; i < n3; ++i) {
                float vx = (pr.pgx[i] * ab) * pw[i];
                float vy = (pr.pgy[i] * ab) * pw[i];
                pr.pgx[i] = vx == vx ? vx : 0.f;
                pr.pgy[i] = vy == vy ? vy : 0.f;
            }
            free(mag);
            free(pw);
        }

        st = ge_evaluate(&pr, res);
        if (st != ALWAN_OK) goto done;

        for (y = 0; y < sh; ++y) {
            unsigned char *orow = out + (size_t)y * out_row_stride;
            unsigned char const *srow = src + (size_t)y * src_row_stride;
            for (x = 0; x < sw; ++x) {
                size_t const k = ((size_t)y * (size_t)sw + (size_t)x) * 3;
                unsigned char const a = channels == 4 ? srow[(size_t)x * 4 + 3] : 0;
                unsigned char *px = orow + (size_t)x * channels;
                px[0] = res[k + 2];
                px[1] = res[k + 1];
                px[2] = res[k + 0];
                if (channels == 4) px[3] = a;
            }
        }
        pr.img = NULL;
        pr.wmask = NULL;
    }

done:
    free(sbgr);
    free(dbgr);
    free(m);
    free(em);
    free(patch);
    free(res);
    free(edges);
    free(buf);
    if (is_clone) {
        free(pr.img);
        free(pr.wmask);
    }
    return st;
}
