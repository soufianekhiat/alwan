/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * FLIP, the perceptual image difference of Andersson, Nilsson, Akenine-Moller, Oskarsson,
 * Astrom and Fairchild ("FLIP: A Difference Evaluator for Alternating Images", HPG 2020)
 * and its HDR extension ("Visualizing Errors in Rendered High Dynamic Range Images",
 * Eurographics 2021).
 *
 * A port of the CPU path of NVIDIA's reference implementation, src/cpp/FLIP.h of
 * https://github.com/NVlabs/flip (commit b475eb4, 2025-11-07), kept to its float
 * arithmetic and its order of operations so the error map agrees with the
 * flip-evaluator package to float rounding (suite 304). Its notice:
 *
 *   Copyright (c) 2020-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without modification,
 *   are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list
 *      of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice, this
 *      list of conditions and the following disclaimer in the documentation and/or
 *      other materials provided with the distribution.
 *   3. Neither the name of the copyright holder nor the names of its contributors may
 *      be used to endorse or promote products derived from this software without
 *      specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 *   EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 *   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 *   SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *   TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 *   DAMAGE.
 *
 * The spatial (contrast sensitivity) and feature filters are FLIP's own separable
 * kernels with the edge sample repeated at the borders, as the reference does; they
 * are private to this file. Image metrics stay in alwan, and alwan does not call suwar.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    float x, y, z;
} alwan__fl3;

#define ALWAN__FLIP_PI 3.14159265358979f

/* FLIP's constants (FLIPConstants and GaussianConstants in FLIP.h). */
#define ALWAN__FLIP_GQC 0.7f
#define ALWAN__FLIP_GPC 0.4f
#define ALWAN__FLIP_GPT 0.95f
#define ALWAN__FLIP_GW 0.082f
#define ALWAN__FLIP_GQF 0.5f

static float const alwan__flip_tm[3][6] = {
    {0.6f * 0.6f * 2.51f, 0.6f * 0.03f, 0.0f, 0.6f * 0.6f * 2.43f, 0.6f * 0.59f, 0.14f}, /* ACES (0.6 cancels the pre-exposure) */
    {0.231683f, 0.013791f, 0.0f, 0.18f, 0.3f, 0.018f},                                   /* Hable */
    {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f},                                                /* Reinhard (luminance form, see below) */
};

static alwan__fl3 alwan__flip_mk(float x, float y, float z) {
    alwan__fl3 c;
    c.x = x;
    c.y = y;
    c.z = z;
    return c;
}

static float alwan__flip_srgb_decode(float s) {
    if (s <= 0.04045f) return s / 12.92f;
    return ALWAN_POW_F32((s + 0.055f) / 1.055f, 2.4f);
}

static alwan__fl3 alwan__flip_rgb_to_xyz(alwan__fl3 c) {
    float const a11 = 10135552.0f / 24577794.0f, a12 = 8788810.0f / 24577794.0f, a13 = 4435075.0f / 24577794.0f;
    float const a21 = 2613072.0f / 12288897.0f, a22 = 8788810.0f / 12288897.0f, a23 = 887015.0f / 12288897.0f;
    float const a31 = 1425312.0f / 73733382.0f, a32 = 8788810.0f / 73733382.0f, a33 = 70074185.0f / 73733382.0f;
    return alwan__flip_mk(a11 * c.x + a12 * c.y + a13 * c.z, a21 * c.x + a22 * c.y + a23 * c.z,
                          a31 * c.x + a32 * c.y + a33 * c.z);
}

static alwan__fl3 alwan__flip_xyz_to_rgb(alwan__fl3 c) {
    return alwan__flip_mk(3.241003275f * c.x + -1.537398934f * c.y + -0.498615861f * c.z,
                          -0.969224334f * c.x + 1.875930071f * c.y + 0.041554224f * c.z,
                          0.055639423f * c.x + -0.204011202f * c.y + 1.057148933f * c.z);
}

static alwan__fl3 alwan__flip_xyz_to_lab(alwan__fl3 c) {
    float const delta = 6.0f / 29.0f, delta2 = delta * delta, delta3 = delta * delta2;
    float const factor = 1.0f / (3.0f * delta2), term = 4.0f / 29.0f;
    c.x = c.x * 1.052156925f;
    c.y = c.y * 1.000000000f;
    c.z = c.z * 0.918357670f;
    c.x = c.x > delta3 ? ALWAN_POW_F32(c.x, 1.0f / 3.0f) : factor * c.x + term;
    c.y = c.y > delta3 ? ALWAN_POW_F32(c.y, 1.0f / 3.0f) : factor * c.y + term;
    c.z = c.z > delta3 ? ALWAN_POW_F32(c.z, 1.0f / 3.0f) : factor * c.z + term;
    return alwan__flip_mk(116.0f * c.y - 16.0f, 500.0f * (c.x - c.y), 200.0f * (c.y - c.z));
}

static alwan__fl3 alwan__flip_xyz_to_ycxcz(alwan__fl3 c) {
    c.x = c.x * 1.052156925f;
    c.y = c.y * 1.000000000f;
    c.z = c.z * 0.918357670f;
    return alwan__flip_mk(116.0f * c.y - 16.0f, 500.0f * (c.x - c.y), 200.0f * (c.y - c.z));
}

static alwan__fl3 alwan__flip_ycxcz_to_xyz(alwan__fl3 c) {
    float const y = (c.x + 16.0f) / 116.0f, cx = c.y / 500.0f, cz = c.z / 200.0f;
    return alwan__flip_mk((y + cx) * 0.950428545f, y * 1.000000000f, (y - cz) * 1.088900371f);
}

static float alwan__flip_clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static float alwan__flip_hyab(alwan__fl3 a, alwan__fl3 b) {
    float const dl = ALWAN_ABS_F32(a.x - b.x);
    return dl + ALWAN_SQRT_F32((a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

static float alwan__flip_cmax(void) {
    alwan__fl3 g = alwan__flip_xyz_to_lab(alwan__flip_rgb_to_xyz(alwan__flip_mk(0.0f, 1.0f, 0.0f)));
    alwan__fl3 b = alwan__flip_xyz_to_lab(alwan__flip_rgb_to_xyz(alwan__flip_mk(0.0f, 0.0f, 1.0f)));
    g = alwan__flip_mk(g.x, 0.01f * g.x * g.y, 0.01f * g.x * g.z);
    b = alwan__flip_mk(b.x, 0.01f * b.x * b.y, 0.01f * b.x * b.z);
    return ALWAN_POW_F32(alwan__flip_hyab(g, b), ALWAN__FLIP_GQC);
}

/* The contrast sensitivity filters, separated: YCx for the achromatic and red-green
 * channels, the two Cz parts (a sum of two Gaussians) for blue-yellow. */
static int alwan__flip_spatial_radius(float ppd) {
    float const pi_sq = ALWAN__FLIP_PI * ALWAN__FLIP_PI;
    float const scale = 0.04f;
    return (int)ALWAN_CEIL_F32(3.0f * ALWAN_SQRT_F32(scale / (2.0f * pi_sq)) * ppd);
}

static float alwan__flip_gauss_ab(float x2, float a, float b) {
    float const pi = ALWAN__FLIP_PI, pi_sq = ALWAN__FLIP_PI * ALWAN__FLIP_PI;
    return a * ALWAN_SQRT_F32(pi / b) * ALWAN_EXP_F32(-pi_sq * x2 / b);
}

static float alwan__flip_gauss_ab_sqrt(float x2, float a, float b) {
    float const pi = ALWAN__FLIP_PI, pi_sq = ALWAN__FLIP_PI * ALWAN__FLIP_PI;
    return ALWAN_SQRT_F32(a * ALWAN_SQRT_F32(pi / b)) * ALWAN_EXP_F32(-pi_sq * x2 / b);
}

static void alwan__flip_spatial_filters(alwan__fl3 *fycx, alwan__fl3 *fcz, float ppd, int radius) {
    float const dx = 1.0f / ppd;
    int const width = 2 * radius + 1;
    float sy = 0.0f, scx = 0.0f, sz1 = 0.0f, sz2 = 0.0f, ny, ncx, ncz;
    int x;
    for (x = 0; x < width; x++) {
        float const ix = (float)(x - radius) * dx;
        float const ix2 = ix * ix;
        float const gy = alwan__flip_gauss_ab(ix2, 1.0f, 0.0047f);
        float const gcx = alwan__flip_gauss_ab(ix2, 1.0f, 0.0053f);
        float const gcz1 = alwan__flip_gauss_ab_sqrt(ix2, 34.1f, 0.04f);
        float const gcz2 = alwan__flip_gauss_ab_sqrt(ix2, 13.5f, 0.025f);
        fycx[x] = alwan__flip_mk(gy, gcx, 0.0f);
        fcz[x] = alwan__flip_mk(gcz1, gcz2, 0.0f);
        sy += gy;
        scx += gcx;
        sz1 += gcz1;
        sz2 += gcz2;
    }
    ny = 1.0f / sy;
    ncx = 1.0f / scx;
    ncz = 1.0f / ALWAN_SQRT_F32(sz1 * sz1 + sz2 * sz2);
    for (x = 0; x < width; x++) {
        fycx[x] = alwan__flip_mk(fycx[x].x * ny, fycx[x].y * ncx, 0.0f);
        fcz[x] = alwan__flip_mk(fcz[x].x * ncz, fcz[x].y * ncz, 0.0f);
    }
}

/* The edge and point detectors: a Gaussian and its first and second derivatives,
 * the derivatives' positive and negative lobes each summing to 1 and -1. */
static void alwan__flip_feature_filter(alwan__fl3 *f, float std_dev, int radius) {
    int const width = 2 * radius + 1;
    float gs = 0.0f, dneg = 0.0f, dpos = 0.0f, ddneg = 0.0f, ddpos = 0.0f;
    int x;
    for (x = 0; x < width; x++) {
        float const xx = (float)(x - radius);
        float const g = ALWAN_EXP_F32(-(xx * xx) / (2.0f * std_dev * std_dev));
        float const dg = -xx * g;
        float const ddg = (xx * xx / (std_dev * std_dev) - 1.0f) * g;
        gs += g;
        if (dg > 0.0f) dpos += dg;
        else dneg -= dg;
        if (ddg > 0.0f) ddpos += ddg;
        else ddneg -= ddg;
        f[x] = alwan__flip_mk(g, dg, ddg);
    }
    for (x = 0; x < width; x++) {
        alwan__fl3 const p = f[x];
        f[x] = alwan__flip_mk(p.x / gs, p.y / (p.y > 0.0f ? dpos : dneg), p.z / (p.z > 0.0f ? ddpos : ddneg));
    }
}

static int alwan__flip_clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* LDR-FLIP of two linear RGB images already limited to [0, 1]; ref and tst are
 * overwritten with their YCxCz values, err receives the error map. scratch holds
 * 6 w h colour triplets. */
static void alwan__flip_ldr(float *err, alwan__fl3 *ref, alwan__fl3 *tst, int w, int h, float ppd, alwan__fl3 *scratch,
                            alwan__fl3 *filters) {
    float const cmax = alwan__flip_cmax();
    float const pccmax = ALWAN__FLIP_GPC * cmax;
    int const sr = alwan__flip_spatial_radius(ppd);
    float const std_dev = 0.5f * ALWAN__FLIP_GW * ppd;
    int const fr = (int)ALWAN_CEIL_F32(3.0f * std_dev);
    size_t const n = (size_t)w * (size_t)h;
    alwan__fl3 *iycx_r = scratch, *iycx_t = scratch + n, *icz_r = scratch + 2 * n, *icz_t = scratch + 3 * n;
    alwan__fl3 *fycx = filters, *fcz = filters + (2 * sr + 1), *ff = fcz + (2 * sr + 1);
    float const one_over_116 = 1.0f / 116.0f, sixteen_over_116 = 16.0f / 116.0f;
    float const norm = 1.0f / ALWAN_SQRT_F32(2.0f);
    size_t i;
    int x, y, k;

    for (i = 0; i < n; i++) {
        ref[i] = alwan__flip_xyz_to_ycxcz(alwan__flip_rgb_to_xyz(ref[i]));
        tst[i] = alwan__flip_xyz_to_ycxcz(alwan__flip_rgb_to_xyz(tst[i]));
    }
    alwan__flip_spatial_filters(fycx, fcz, ppd, sr);
    alwan__flip_feature_filter(ff, std_dev, fr);

    /* Colour difference: CSF filtering along x, then y, then back to linear RGB (limited
     * to [0, 1]), CIELAB, the Hunt adjustment, HyAB and FLIP's remapping. */
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            alwan__fl3 ar = {0.0f, 0.0f, 0.0f}, at = {0.0f, 0.0f, 0.0f}, cr = {0.0f, 0.0f, 0.0f}, ct = {0.0f, 0.0f, 0.0f};
            for (k = -sr; k <= sr; k++) {
                int const xx = alwan__flip_clampi(x + k, 0, w - 1);
                alwan__fl3 const wy = fycx[k + sr], wz = fcz[k + sr];
                alwan__fl3 const rc = ref[(size_t)y * (size_t)w + (size_t)xx], tc = tst[(size_t)y * (size_t)w + (size_t)xx];
                ar.x += wy.x * rc.x;
                ar.y += wy.y * rc.y;
                at.x += wy.x * tc.x;
                at.y += wy.y * tc.y;
                cr.x += wz.x * rc.z;
                cr.y += wz.y * rc.z;
                ct.x += wz.x * tc.z;
                ct.y += wz.y * tc.z;
            }
            i = (size_t)y * (size_t)w + (size_t)x;
            iycx_r[i] = ar;
            iycx_t[i] = at;
            icz_r[i] = cr;
            icz_t[i] = ct;
        }
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            alwan__fl3 ar = {0.0f, 0.0f, 0.0f}, at = {0.0f, 0.0f, 0.0f}, cr = {0.0f, 0.0f, 0.0f}, ct = {0.0f, 0.0f, 0.0f};
            alwan__fl3 fr_, ft;
            float cd;
            for (k = -sr; k <= sr; k++) {
                int const yy = alwan__flip_clampi(y + k, 0, h - 1);
                alwan__fl3 const wy = fycx[k + sr], wz = fcz[k + sr];
                size_t const j = (size_t)yy * (size_t)w + (size_t)x;
                ar.x += wy.x * iycx_r[j].x;
                ar.y += wy.y * iycx_r[j].y;
                at.x += wy.x * iycx_t[j].x;
                at.y += wy.y * iycx_t[j].y;
                cr.x += wz.x * icz_r[j].x;
                cr.y += wz.y * icz_r[j].y;
                ct.x += wz.x * icz_t[j].x;
                ct.y += wz.y * icz_t[j].y;
            }
            fr_ = alwan__flip_xyz_to_rgb(alwan__flip_ycxcz_to_xyz(alwan__flip_mk(ar.x, ar.y, cr.x + cr.y)));
            ft = alwan__flip_xyz_to_rgb(alwan__flip_ycxcz_to_xyz(alwan__flip_mk(at.x, at.y, ct.x + ct.y)));
            fr_ = alwan__flip_mk(alwan__flip_clamp01(fr_.x), alwan__flip_clamp01(fr_.y), alwan__flip_clamp01(fr_.z));
            ft = alwan__flip_mk(alwan__flip_clamp01(ft.x), alwan__flip_clamp01(ft.y), alwan__flip_clamp01(ft.z));
            fr_ = alwan__flip_xyz_to_lab(alwan__flip_rgb_to_xyz(fr_));
            ft = alwan__flip_xyz_to_lab(alwan__flip_rgb_to_xyz(ft));
            fr_.y = 0.01f * fr_.x * fr_.y;
            fr_.z = 0.01f * fr_.x * fr_.z;
            ft.y = 0.01f * ft.x * ft.y;
            ft.z = 0.01f * ft.x * ft.z;
            cd = ALWAN_POW_F32(alwan__flip_hyab(fr_, ft), ALWAN__FLIP_GQC);
            if (cd < pccmax) cd *= ALWAN__FLIP_GPT / pccmax;
            else cd = ALWAN__FLIP_GPT + ((cd - pccmax) / (cmax - pccmax)) * (1.0f - ALWAN__FLIP_GPT);
            err[(size_t)y * (size_t)w + (size_t)x] = cd;
        }
    }

    /* Feature difference on the achromatic channel, normalised to [0, 1]: derivatives
     * of the Gaussian along x (Gaussian along y), then the other way, edges and points,
     * and the final error colour_difference ^ (1 - feature_difference). */
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            float dxr = 0.0f, dxt = 0.0f, ddxr = 0.0f, ddxt = 0.0f, gr = 0.0f, gt = 0.0f;
            for (k = -fr; k <= fr; k++) {
                int const xx = alwan__flip_clampi(x + k, 0, w - 1);
                alwan__fl3 const fw = ff[k + fr];
                float const yr = ref[(size_t)y * (size_t)w + (size_t)xx].x * one_over_116 + sixteen_over_116;
                float const yt = tst[(size_t)y * (size_t)w + (size_t)xx].x * one_over_116 + sixteen_over_116;
                dxr += fw.y * yr;
                dxt += fw.y * yt;
                ddxr += fw.z * yr;
                ddxt += fw.z * yt;
                gr += fw.x * yr;
                gt += fw.x * yt;
            }
            i = (size_t)y * (size_t)w + (size_t)x;
            iycx_r[i] = alwan__flip_mk(dxr, ddxr, gr);
            iycx_t[i] = alwan__flip_mk(dxt, ddxt, gt);
        }
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            float dxr = 0.0f, dxt = 0.0f, ddxr = 0.0f, ddxt = 0.0f, dyr = 0.0f, dyt = 0.0f, ddyr = 0.0f, ddyt = 0.0f;
            float er, et, pr, pt, ed, pd, fd, cd;
            for (k = -fr; k <= fr; k++) {
                int const yy = alwan__flip_clampi(y + k, 0, h - 1);
                alwan__fl3 const fw = ff[k + fr];
                alwan__fl3 const ir = iycx_r[(size_t)yy * (size_t)w + (size_t)x], it = iycx_t[(size_t)yy * (size_t)w + (size_t)x];
                dxr += fw.x * ir.x;
                dxt += fw.x * it.x;
                ddxr += fw.x * ir.y;
                ddxt += fw.x * it.y;
                dyr += fw.y * ir.z;
                dyt += fw.y * it.z;
                ddyr += fw.z * ir.z;
                ddyt += fw.z * it.z;
            }
            er = ALWAN_SQRT_F32(dxr * dxr + dyr * dyr);
            et = ALWAN_SQRT_F32(dxt * dxt + dyt * dyt);
            pr = ALWAN_SQRT_F32(ddxr * ddxr + ddyr * ddyr);
            pt = ALWAN_SQRT_F32(ddxt * ddxt + ddyt * ddyt);
            ed = ALWAN_ABS_F32(er - et);
            pd = ALWAN_ABS_F32(pr - pt);
            fd = ALWAN_POW_F32(norm * (ed > pd ? ed : pd), ALWAN__FLIP_GQF);
            i = (size_t)y * (size_t)w + (size_t)x;
            cd = err[i];
            err[i] = ALWAN_POW_F32(cd, 1.0f - fd);
        }
    }
}

static void alwan__flip_tonemap(alwan__fl3 *c, size_t n, alwan_flip_tonemapper tm) {
    size_t i;
    if (tm == ALWAN_FLIP_TONEMAP_REINHARD) {
        for (i = 0; i < n; i++) {
            float const lum = 0.2126f * c[i].x + 0.7152f * c[i].y + 0.0722f * c[i].z;
            float const f = 1.0f / (1.0f + lum);
            c[i] = alwan__flip_mk(c[i].x * f, c[i].y * f, c[i].z * f);
        }
        return;
    }
    {
        float const *tc = alwan__flip_tm[tm == ALWAN_FLIP_TONEMAP_HABLE ? 1 : 0];
        for (i = 0; i < n; i++) {
            float const x = c[i].x, y = c[i].y, z = c[i].z;
            c[i] = alwan__flip_mk(((x * x) * tc[0] + x * tc[1] + tc[2]) / ((x * x) * tc[3] + x * tc[4] + tc[5]),
                                  ((y * y) * tc[0] + y * tc[1] + tc[2]) / ((y * y) * tc[3] + y * tc[4] + tc[5]),
                                  ((z * z) * tc[0] + z * tc[1] + tc[2]) / ((z * z) * tc[3] + z * tc[4] + tc[5]));
        }
    }
}

static int alwan__flip_cmp_float(void const *a, void const *b) {
    float const x = *(float const *)a, y = *(float const *)b;
    return (x > y) - (x < y);
}

/* HDR-FLIP's automatic exposure range (computeExposures in FLIP.h): from the reference's
 * largest and median luminance and where the tone mapper reaches 0.85. */
static void alwan__flip_exposures(float *start, float *stop, alwan__fl3 const *ref, size_t n, alwan_flip_tonemapper tm,
                                  float *lum) {
    float const *tc = alwan__flip_tm[tm == ALWAN_FLIP_TONEMAP_REINHARD ? 2 : (tm == ALWAN_FLIP_TONEMAP_HABLE ? 1 : 0)];
    float const t = 0.85f;
    float const a = tc[0] - t * tc[3], b = tc[1] - t * tc[4], c = tc[2] - t * tc[5];
    float xmax, ymax = -1e30f, ymed;
    size_t i;
    if (a == 0.0f) {
        xmax = -c / b;
    } else {
        float const d1 = -0.5f * (b / a);
        float const d2 = ALWAN_SQRT_F32((d1 * d1) - (c / a));
        xmax = d1 + d2;
    }
    for (i = 0; i < n; i++) {
        lum[i] = 0.2126f * ref[i].x + 0.7152f * ref[i].y + 0.0722f * ref[i].z;
        if (lum[i] > ymax) ymax = lum[i];
    }
    qsort(lum, n, sizeof(float), alwan__flip_cmp_float);
    ymed = lum[n / 2];
    if (ymed < FLT_EPSILON) ymed = FLT_EPSILON;
    *start = (float)ALWAN_LOG2_F64((double)(xmax / ymax));
    *stop = (float)ALWAN_LOG2_F64((double)(xmax / ymed));
}

alwan_f32 alwan_flip_ppd(alwan_f32 distance_m, alwan_f32 display_width_px, alwan_f32 display_width_m) {
    return distance_m * (display_width_px / display_width_m) * (ALWAN__FLIP_PI / 180.0f);
}

static alwan_status alwan__flip_run(float *err_out, size_t err_rs, float *exp_out, size_t exp_rs, alwan_flip_result *result,
                                    void const *refp, size_t ref_rs, void const *tstp, size_t tst_rs, int is_f64, size_t width,
                                    size_t height, alwan_flip_params const *params, alwan_ctx *ctx) {
    alwan_flip_params p;
    alwan__fl3 *img, *ref, *tst, *rw, *tw, *scratch, *filters;
    float *err, *best, *lum;
    float ppd, start = 0.0f, stop = 0.0f;
    int num = 0, w, h, sr, fr;
    size_t n, i, x, y, nfilt, bytes;
    if (!refp || !tstp || width == 0 || height == 0) return ALWAN_E_INVALID;
    if (!err_out && !exp_out && !result) return ALWAN_E_INVALID;
    if (width > (size_t)1 << 20 || height > (size_t)1 << 20) return ALWAN_E_RANGE;
    if (params) p = *params;
    else memset(&p, 0, sizeof p);
    if ((unsigned)p.range > (unsigned)ALWAN_FLIP_HDR || (unsigned)p.input > (unsigned)ALWAN_FLIP_INPUT_LINEAR ||
        (unsigned)p.tonemapper > (unsigned)ALWAN_FLIP_TONEMAP_REINHARD)
        return ALWAN_E_INVALID;
    /* a ppd below 0.01 underflows the filters (1 / ppd in float) */
    if (!(p.ppd == 0.0 || (p.ppd >= 0.01 && p.ppd <= 10000.0))) return ALWAN_E_RANGE;
    ppd = p.ppd > 0.0 ? (float)p.ppd : alwan_flip_ppd(0.7f, 3840.0f, 0.7f);
    if (p.num_exposures < 0 || p.num_exposures > 1000) return ALWAN_E_RANGE;
    w = (int)width;
    h = (int)height;
    n = width * height;
    /* a row stride of 0 is packed rows */
    if (ref_rs == 0) ref_rs = width * 3 * (is_f64 ? sizeof(alwan_f64) : sizeof(alwan_f32));
    if (tst_rs == 0) tst_rs = width * 3 * (is_f64 ? sizeof(alwan_f64) : sizeof(alwan_f32));
    if (err_rs == 0) err_rs = width * sizeof(float);
    if (exp_rs == 0) exp_rs = width * sizeof(float);
    sr = alwan__flip_spatial_radius(ppd);
    fr = (int)ALWAN_CEIL_F32(3.0f * (0.5f * ALWAN__FLIP_GW * ppd));
    if (sr > 1 << 16 || fr > 1 << 16) return ALWAN_E_RANGE;
    nfilt = (size_t)(2 * (2 * sr + 1) + (2 * fr + 1));
    bytes = alwan_safe_array_size(n, 10 * sizeof(alwan__fl3) + 3 * sizeof(float));
    if (bytes == 0) return ALWAN_E_NOMEM;
    bytes += nfilt * sizeof(alwan__fl3);
    img = (alwan__fl3 *)alwan_ctx_alloc(bytes, sizeof(double), ctx);
    if (!img) return ALWAN_E_NOMEM;
    ref = img;
    tst = ref + n;
    rw = tst + n;
    tw = rw + n;
    scratch = tw + n;
    filters = scratch + 6 * n;
    err = (float *)(filters + nfilt);
    best = err + n;
    lum = best + n;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            size_t const k = y * width + x;
            float r[3], t[3];
            int c;
            for (c = 0; c < 3; c++) {
                if (is_f64) {
                    r[c] = (float)((alwan_f64 const *)((char const *)refp + y * ref_rs))[x * 3 + (size_t)c];
                    t[c] = (float)((alwan_f64 const *)((char const *)tstp + y * tst_rs))[x * 3 + (size_t)c];
                } else {
                    r[c] = ((alwan_f32 const *)((char const *)refp + y * ref_rs))[x * 3 + (size_t)c];
                    t[c] = ((alwan_f32 const *)((char const *)tstp + y * tst_rs))[x * 3 + (size_t)c];
                }
                if (!(r[c] - r[c] == 0.0f) || !(t[c] - t[c] == 0.0f)) {
                    alwan_ctx_free(img, ctx);
                    return ALWAN_E_INVALID;
                }
                if (p.range == ALWAN_FLIP_LDR && p.input == ALWAN_FLIP_INPUT_SRGB) {
                    r[c] = alwan__flip_srgb_decode(r[c]);
                    t[c] = alwan__flip_srgb_decode(t[c]);
                }
            }
            ref[k] = alwan__flip_mk(r[0], r[1], r[2]);
            tst[k] = alwan__flip_mk(t[0], t[1], t[2]);
        }
    }

    if (p.range == ALWAN_FLIP_LDR) {
        for (i = 0; i < n; i++) {
            ref[i] = alwan__flip_mk(alwan__flip_clamp01(ref[i].x), alwan__flip_clamp01(ref[i].y), alwan__flip_clamp01(ref[i].z));
            tst[i] = alwan__flip_mk(alwan__flip_clamp01(tst[i].x), alwan__flip_clamp01(tst[i].y), alwan__flip_clamp01(tst[i].z));
        }
        alwan__flip_ldr(best, ref, tst, w, h, ppd, scratch, filters);
        if (exp_out) {
            for (y = 0; y < height; y++) memset((char *)exp_out + y * exp_rs, 0, width * sizeof(float));
        }
    } else {
        float step;
        int e;
        if (p.exposures_given) {
            if (!(p.start_exposure == p.start_exposure) || !(p.stop_exposure == p.stop_exposure) ||
                p.start_exposure > p.stop_exposure) {
                alwan_ctx_free(img, ctx);
                return ALWAN_E_RANGE;
            }
            start = (float)p.start_exposure;
            stop = (float)p.stop_exposure;
        } else {
            alwan__flip_exposures(&start, &stop, ref, n, p.tonemapper, lum);
            if (!(start - start == 0.0f) || !(stop - stop == 0.0f) || start > stop) {
                /* An all-black reference has no exposure range to search. */
                alwan_ctx_free(img, ctx);
                return ALWAN_E_RANGE;
            }
        }
        num = p.num_exposures > 0 ? p.num_exposures : 0;
        if (num == 0) {
            float const span = ALWAN_CEIL_F32(stop - start);
            num = (int)(span > 2.0f ? span : 2.0f);
        }
        if (num < 2) num = 2;
        if (num > 1000) {
            alwan_ctx_free(img, ctx);
            return ALWAN_E_RANGE;
        }
        for (i = 0; i < n; i++) best[i] = 0.0f;
        if (exp_out) {
            for (y = 0; y < height; y++) memset((char *)exp_out + y * exp_rs, 0, width * sizeof(float));
        }
        step = (stop - start) / (float)(num - 1);
        for (e = 0; e < num; e++) {
            float const exposure = start + (float)e * step;
            float const m = ALWAN_POW_F32(2.0f, exposure);
            float const level = (float)e / (float)(num - 1);
            for (i = 0; i < n; i++) {
                rw[i] = alwan__flip_mk(ref[i].x * m, ref[i].y * m, ref[i].z * m);
                tw[i] = alwan__flip_mk(tst[i].x * m, tst[i].y * m, tst[i].z * m);
            }
            alwan__flip_tonemap(rw, n, p.tonemapper);
            alwan__flip_tonemap(tw, n, p.tonemapper);
            for (i = 0; i < n; i++) {
                rw[i] = alwan__flip_mk(alwan__flip_clamp01(rw[i].x), alwan__flip_clamp01(rw[i].y), alwan__flip_clamp01(rw[i].z));
                tw[i] = alwan__flip_mk(alwan__flip_clamp01(tw[i].x), alwan__flip_clamp01(tw[i].y), alwan__flip_clamp01(tw[i].z));
            }
            alwan__flip_ldr(err, rw, tw, w, h, ppd, scratch, filters);
            for (y = 0; y < height; y++) {
                for (x = 0; x < width; x++) {
                    size_t const k = y * width + x;
                    if (err[k] > best[k]) {
                        best[k] = err[k];
                        if (exp_out) ((float *)((char *)exp_out + y * exp_rs))[x] = level;
                    }
                }
            }
        }
    }

    if (err_out) {
        for (y = 0; y < height; y++) memcpy((char *)err_out + y * err_rs, best + y * width, width * sizeof(float));
    }
    if (result) {
        float sum = 0.0f;
        for (i = 0; i < n; i++) sum += best[i];
        result->mean = (double)(sum / (float)(w * h));
        result->ppd = (double)ppd;
        result->start_exposure = (double)start;
        result->stop_exposure = (double)stop;
        result->num_exposures = num;
    }
    alwan_ctx_free(img, ctx);
    return ALWAN_OK;
}

#if ALWAN_WITH_F32
alwan_status alwan_flip_f32(alwan_f32 *error_map, size_t error_row_stride, alwan_f32 *exposure_map, size_t exposure_row_stride,
                            alwan_flip_result *result, alwan_f32 const *reference, size_t reference_row_stride,
                            alwan_f32 const *test, size_t test_row_stride, size_t width, size_t height,
                            alwan_flip_params const *params, alwan_ctx *ctx) {
    return alwan__flip_run(error_map, error_row_stride, exposure_map, exposure_row_stride, result, reference,
                           reference_row_stride, test, test_row_stride, 0, width, height, params, ctx);
}

#endif

#if ALWAN_WITH_F64
alwan_status alwan_flip_f64(alwan_f32 *error_map, size_t error_row_stride, alwan_f32 *exposure_map, size_t exposure_row_stride,
                            alwan_flip_result *result, alwan_f64 const *reference, size_t reference_row_stride,
                            alwan_f64 const *test, size_t test_row_stride, size_t width, size_t height,
                            alwan_flip_params const *params, alwan_ctx *ctx) {
    return alwan__flip_run(error_map, error_row_stride, exposure_map, exposure_row_stride, result, reference,
                           reference_row_stride, test, test_row_stride, 1, width, height, params, ctx);
}
#endif
