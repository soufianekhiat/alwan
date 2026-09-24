/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_background_{T} and _u8: the slowly varying background of an image, to subtract or
 * divide out uneven illumination.
 *
 * ALWAN_BACKGROUND_ROLLING_BALL, scikit-image's restoration.rolling_ball (after Sternberg
 * 1983), which suite 218 holds this to: a ball, or an ellipsoid, pushed up under the image
 * surface; at each pixel the background is the height the top of the kernel reaches when it
 * is centred under that pixel:
 *
 *   background(p) = min over the kernel's offsets o of  img(p + o) + k(0) - k(o)
 *
 * with k the kernel's height, k(o) = sqrt(r^2 - |o|^2) inside the ball, and pixels outside
 * the image counted as +infinity. That is a grey erosion by the kernel's surface. (ImageJ's
 * "Subtract Background" follows it with the matching dilation, an opening, which sits
 * closer to the image; this is scikit-image's.) The ball of radius r spans the offsets
 * -ceil(r) .. ceil(r) on both axes; the ellipsoid of width w, height h and intensity c has
 * semi-axes max(w / 2, 1) and max(h / 2, 1) and k(o) = c sqrt(1 - (ox / ax)^2 - (oy / ay)^2).
 * Each channel alone. 8-bit results are truncated, as scikit-image's cast back truncates.
 * The cost is about pi r^2 per pixel.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>

static alwan_status alwan_bg_run(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 alwan_background_method method, alwan_background_params const *params,
                                 int kind /* 0 f64, 1 f32, 2 u8 */) {
    alwan_background_params const zero = { 0 };
    alwan_background_params const *p = params ? params : &zero;
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    size_t const n = w * h;
    long ax, ay, kw, kh, x0, x1, y, x, i, j;
    double *img, *diff, center;
    size_t c;
    if (!out || !src || w == 0 || h == 0 || ch == 0 || ch > 4 || n / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / elem / ch < w) return ALWAN_E_INVALID;
    if (method != ALWAN_BACKGROUND_ROLLING_BALL) return ALWAN_E_INVALID;
    if (p->ellipsoid_width != 0) {
        if (!(p->ellipsoid_intensity > 0.0) || p->ellipsoid_intensity > DBL_MAX || p->ellipsoid_width > 4001 ||
            p->ellipsoid_height > 4001) {
            return ALWAN_E_RANGE;
        }
        ax = (long)(p->ellipsoid_width / 2) > 1 ? (long)(p->ellipsoid_width / 2) : 1;
        ay = (long)(p->ellipsoid_height / 2) > 1 ? (long)(p->ellipsoid_height / 2) : 1;
    } else {
        double const r = p->radius == 0.0 ? 100.0 : p->radius;
        if (!(r > 0.0) || r > 2000.0) return ALWAN_E_RANGE;
        ax = ay = (long)ceil(r);
    }
    kw = 2 * ax + 1;
    kh = 2 * ay + 1;
    img = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, 2 * sizeof(double)) + (size_t)(kw * kh) * sizeof(double),
                                sizeof(double));
    if (!img) return ALWAN_E_NOMEM;
    diff = img + 2 * n;
    /* the kernel's height, then center - height; +inf outside it */
    for (i = 0; i < kh; i++) {
        for (j = 0; j < kw; j++) {
            double const oy = (double)(i - ay), ox = (double)(j - ax);
            double k;
            if (p->ellipsoid_width != 0) {
                double const s = 1.0 - (ox / (double)ax) * (ox / (double)ax) - (oy / (double)ay) * (oy / (double)ay);
                k = s < 0.0 ? HUGE_VAL : p->ellipsoid_intensity * sqrt(s);
            } else {
                double const r = p->radius == 0.0 ? 100.0 : p->radius, ss = ox * ox + oy * oy;
                k = sqrt(ss) > r ? HUGE_VAL : sqrt(r * r - ss > 0.0 ? r * r - ss : 0.0);
            }
            diff[i * kw + j] = k;
        }
    }
    center = diff[ay * kw + ax];
    for (i = 0; i < kw * kh; i++) diff[i] = diff[i] == HUGE_VAL ? HUGE_VAL : center - diff[i];
    for (c = 0; c < ch; c++) {
        double *plane = img, *bg = img + n;
        for (y = 0; y < (long)h; y++) {
            char const *row = (char const *)src + (size_t)y * src_rs;
            for (x = 0; x < (long)w; x++) {
                double const v = kind == 0 ? ((alwan_f64 const *)row)[(size_t)x * ch + c]
                               : kind == 1 ? (double)((alwan_f32 const *)row)[(size_t)x * ch + c]
                                           : (double)((unsigned char const *)row)[(size_t)x * ch + c];
                if (!(v == v) || v > DBL_MAX || v < -DBL_MAX) {
                    ALWAN_FREE(img);
                    return ALWAN_E_INVALID;
                }
                plane[(size_t)y * w + (size_t)x] = v;
            }
        }
        for (y = 0; y < (long)h; y++) {
            for (x = 0; x < (long)w; x++) {
                double m = HUGE_VAL;
                x0 = x - ax < 0 ? ax - x : 0;
                x1 = x + ax >= (long)w ? kw - 1 - (x + ax - ((long)w - 1)) : kw - 1;
                for (i = 0; i < kh; i++) {
                    long const sy = y + i - ay;
                    double const *drow = diff + i * kw;
                    long const base = sy * (long)w + x - ax;   /* + j is the pixel under offset j */
                    if (sy < 0 || sy >= (long)h) continue;
                    for (j = x0; j <= x1; j++) {
                        double const t = plane[base + j] + drow[j];
                        if (t < m) m = t;
                    }
                }
                bg[(size_t)y * w + (size_t)x] = m;
            }
        }
        for (y = 0; y < (long)h; y++) {
            char *row = (char *)out + (size_t)y * out_rs;
            for (x = 0; x < (long)w; x++) {
                double const v = bg[(size_t)y * w + (size_t)x];
                if (kind == 0) ((alwan_f64 *)row)[(size_t)x * ch + c] = v;
                else if (kind == 1) ((alwan_f32 *)row)[(size_t)x * ch + c] = (alwan_f32)v;
                else ((unsigned char *)row)[(size_t)x * ch + c] = (unsigned char)(v < 0.0 ? 0.0 : v > 255.0 ? 255.0 : floor(v));
            }
        }
    }
    ALWAN_FREE(img);
    return ALWAN_OK;
}

alwan_status alwan_background_u8(unsigned char *out, size_t out_row_stride, unsigned char const *src, size_t src_row_stride,
                                 size_t channels, size_t width, size_t height, alwan_background_method method,
                                 alwan_background_params const *params) {
    return alwan_bg_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 2);
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_background_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride,
                                  size_t channels, size_t width, size_t height, alwan_background_method method,
                                  alwan_background_params const *params) {
    return alwan_bg_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_background_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride,
                                  size_t channels, size_t width, size_t height, alwan_background_method method,
                                  alwan_background_params const *params) {
    return alwan_bg_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 1);
}
#endif
