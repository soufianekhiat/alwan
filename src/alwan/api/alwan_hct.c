/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * HCT, the hue, chroma and tone of Google's Material colour system. Suite 303.
 *
 * Ported from material-color-utilities 0.3.0 (hct/hct.js, hct/hct_solver.js, hct/cam16.js,
 * hct/viewing_conditions.js, utils/color_utils.js, utils/math_utils.js,
 * palettes/tonal_palette.js), under the Apache License, Version 2.0:
 *
 *   Copyright 2021 Google LLC
 *
 *   Licensed under the Apache License, Version 2.0 (the "License"); you may not use this
 *   file except in compliance with the License. You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software distributed under
 *   the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *   KIND, either express or implied. See the License for the specific language governing
 *   permissions and limitations under the License.
 *
 * Changes from the original: translated from JavaScript to C; only the parts HCT and the
 * tonal palette need (the default viewing conditions, Cam16.fromInt, HctSolver, the
 * sRGB/L* helpers). The arithmetic keeps the original's order so the 8-bit results agree.
 */

#include "../alwan.h"
#include "../alwan_internal.h"

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static double const hct_critical_planes[255] = {
#include "../data/hct/critical_planes.csv"
};
ALWAN_DIAG_POP

/* ---- utils ---- */

static double hct_signum(double x) { return x < 0 ? -1.0 : (x == 0 ? 0.0 : 1.0); }

/* JavaScript's Math.round: the nearest integer, halves upwards */
static double hct_js_round(double x) {
    double r = ALWAN_FLOOR(x);
    if (x - r >= 0.5) r += 1.0;
    return r;
}

static int hct_clamp_int(int lo, int hi, double v) {
    if (v != v) return 0;           /* JavaScript: NaN & 255 is 0 */
    if (v < lo) return lo;
    if (v > hi) return hi;
    return (int)v;
}

static double hct_lab_f(double t) {
    double const e = 216.0 / 24389.0, kappa = 24389.0 / 27.0;
    if (t > e) return ALWAN_POW(t, 1.0 / 3.0);
    return (kappa * t + 16) / 116;
}

static double hct_lab_invf(double ft) {
    double const e = 216.0 / 24389.0, kappa = 24389.0 / 27.0;
    double ft3 = ft * ft * ft;
    if (ft3 > e) return ft3;
    return (116 * ft - 16) / kappa;
}

static double hct_y_from_lstar(double lstar) { return 100.0 * hct_lab_invf((lstar + 16.0) / 116.0); }

static double hct_linearized(int c) {
    double n = c / 255.0;
    if (n <= 0.040449936) return n / 12.92 * 100.0;
    return ALWAN_POW((n + 0.055) / 1.055, 2.4) * 100.0;
}

static int hct_delinearized(double c) {
    double n = c / 100.0, d;
    if (n <= 0.0031308) d = n * 12.92;
    else d = 1.055 * ALWAN_POW(n, 1.0 / 2.4) - 0.055;
    return hct_clamp_int(0, 255, hct_js_round(d * 255.0));
}

static uint32_t hct_argb(int r, int g, int b) {
    return (uint32_t)(0xFF000000u | ((uint32_t)(r & 255) << 16) | ((uint32_t)(g & 255) << 8) | (uint32_t)(b & 255));
}

static uint32_t hct_argb_from_linrgb(double const l[3]) {
    return hct_argb(hct_delinearized(l[0]), hct_delinearized(l[1]), hct_delinearized(l[2]));
}

static uint32_t hct_argb_from_lstar(double lstar) {
    int c = hct_delinearized(hct_y_from_lstar(lstar));
    return hct_argb(c, c, c);
}

static double hct_lstar_from_argb(uint32_t argb) {
    double r = hct_linearized((int)((argb >> 16) & 255)), g = hct_linearized((int)((argb >> 8) & 255));
    double b = hct_linearized((int)(argb & 255));
    double y = r * 0.2126 + g * 0.7152 + b * 0.0722;
    return 116.0 * hct_lab_f(y / 100.0) - 16.0;
}

/* ---- the default viewing conditions ---- */

typedef struct { double n, aw, nbb, ncb, c, nc, rgbD[3], fl, fLRoot, z; } hct_vc;

static void hct_vc_default(hct_vc *v) {
    double const wp[3] = { 95.047, 100.0, 108.883 };
    double const la = (200.0 / ALWAN_PI_F64) * hct_y_from_lstar(50.0) / 100.0;
    double const background = 50.0, surround = 2.0;
    double rW = wp[0] * 0.401288 + wp[1] * 0.650173 + wp[2] * -0.051461;
    double gW = wp[0] * -0.250268 + wp[1] * 1.204414 + wp[2] * 0.045854;
    double bW = wp[0] * -0.002079 + wp[1] * 0.048952 + wp[2] * 0.953127;
    double f = 0.8 + surround / 10.0, c, d, k, k4, k4F, fl, n, rgbAF[3], rgbA[3];
    double amount;
    if (f >= 0.9) { amount = (f - 0.9) * 10.0; c = (1.0 - amount) * 0.59 + amount * 0.69; }
    else { amount = (f - 0.8) * 10.0; c = (1.0 - amount) * 0.525 + amount * 0.59; }
    d = f * (1.0 - (1.0 / 3.6) * ALWAN_EXP((-la - 42.0) / 92.0));
    d = d > 1.0 ? 1.0 : d < 0.0 ? 0.0 : d;
    v->nc = f;
    v->rgbD[0] = d * (100.0 / rW) + 1.0 - d;
    v->rgbD[1] = d * (100.0 / gW) + 1.0 - d;
    v->rgbD[2] = d * (100.0 / bW) + 1.0 - d;
    k = 1.0 / (5.0 * la + 1.0);
    k4 = k * k * k * k;
    k4F = 1.0 - k4;
    fl = k4 * la + 0.1 * k4F * k4F * ALWAN_CBRT(5.0 * la);
    n = hct_y_from_lstar(background) / wp[1];
    v->z = 1.48 + ALWAN_SQRT(n);
    v->nbb = 0.725 / ALWAN_POW(n, 0.2);
    v->ncb = v->nbb;
    rgbAF[0] = ALWAN_POW((fl * v->rgbD[0] * rW) / 100.0, 0.42);
    rgbAF[1] = ALWAN_POW((fl * v->rgbD[1] * gW) / 100.0, 0.42);
    rgbAF[2] = ALWAN_POW((fl * v->rgbD[2] * bW) / 100.0, 0.42);
    rgbA[0] = (400.0 * rgbAF[0]) / (rgbAF[0] + 27.13);
    rgbA[1] = (400.0 * rgbAF[1]) / (rgbAF[1] + 27.13);
    rgbA[2] = (400.0 * rgbAF[2]) / (rgbAF[2] + 27.13);
    v->aw = (2.0 * rgbA[0] + rgbA[1] + 0.05 * rgbA[2]) * v->nbb;
    v->n = n;
    v->c = c;
    v->fl = fl;
    v->fLRoot = ALWAN_POW(fl, 0.25);
}

/* Cam16.fromIntInViewingConditions: hue and chroma */
static void hct_cam16_from_int(double *hue_out, double *chroma_out, uint32_t argb, hct_vc const *v) {
    double redL = hct_linearized((int)((argb & 0x00ff0000u) >> 16));
    double greenL = hct_linearized((int)((argb & 0x0000ff00u) >> 8));
    double blueL = hct_linearized((int)(argb & 0x000000ffu));
    double x = 0.41233895 * redL + 0.35762064 * greenL + 0.18051042 * blueL;
    double y = 0.2126 * redL + 0.7152 * greenL + 0.0722 * blueL;
    double z = 0.01932141 * redL + 0.11916382 * greenL + 0.95034478 * blueL;
    double rC = 0.401288 * x + 0.650173 * y - 0.051461 * z;
    double gC = -0.250268 * x + 1.204414 * y + 0.045854 * z;
    double bC = -0.002079 * x + 0.048952 * y + 0.953127 * z;
    double rD = v->rgbD[0] * rC, gD = v->rgbD[1] * gC, bD = v->rgbD[2] * bC;
    double rAF = ALWAN_POW((v->fl * ALWAN_ABS(rD)) / 100.0, 0.42);
    double gAF = ALWAN_POW((v->fl * ALWAN_ABS(gD)) / 100.0, 0.42);
    double bAF = ALWAN_POW((v->fl * ALWAN_ABS(bD)) / 100.0, 0.42);
    double rA = (hct_signum(rD) * 400.0 * rAF) / (rAF + 27.13);
    double gA = (hct_signum(gD) * 400.0 * gAF) / (gAF + 27.13);
    double bA = (hct_signum(bD) * 400.0 * bAF) / (bAF + 27.13);
    double a = (11.0 * rA + -12.0 * gA + bA) / 11.0;
    double b = (rA + gA - 2.0 * bA) / 9.0;
    double u = (20.0 * rA + 20.0 * gA + 21.0 * bA) / 20.0;
    double p2 = (40.0 * rA + 20.0 * gA + bA) / 20.0;
    double atanDegrees = (ALWAN_ATAN2(b, a) * 180.0) / ALWAN_PI_F64;
    double hue = atanDegrees < 0 ? atanDegrees + 360.0 : atanDegrees >= 360 ? atanDegrees - 360.0 : atanDegrees;
    double ac = p2 * v->nbb;
    double j = 100.0 * ALWAN_POW(ac / v->aw, v->c * v->z);
    double huePrime = hue < 20.14 ? hue + 360 : hue;
    double eHue = 0.25 * (ALWAN_COS((huePrime * ALWAN_PI_F64) / 180.0 + 2.0) + 3.8);
    double p1 = (50000.0 / 13.0) * eHue * v->nc * v->ncb;
    double t = (p1 * ALWAN_SQRT(a * a + b * b)) / (u + 0.305);
    double alpha = ALWAN_POW(t, 0.9) * ALWAN_POW(1.64 - ALWAN_POW(0.29, v->n), 0.73);
    *hue_out = hue;
    *chroma_out = alpha * ALWAN_SQRT(j / 100.0);
}

/* ---- HctSolver ---- */

static double const SCALED_DISCOUNT_FROM_LINRGB[3][3] = {
    { 0.001200833568784504, 0.002389694492170889, 0.0002795742885861124 },
    { 0.0005891086651375999, 0.0029785502573438758, 0.0003270666104008398 },
    { 0.00010146692491640572, 0.0005364214359186694, 0.0032979401770712076 },
};
static double const LINRGB_FROM_SCALED_DISCOUNT[3][3] = {
    { 1373.2198709594231, -1100.4251190754821, -7.278681089101213 },
    { -271.815969077903, 559.6580465940733, -32.46047482791194 },
    { 1.9622899599665666, -57.173814538844006, 308.7233197812385 },
};
static double const Y_FROM_LINRGB[3] = { 0.2126, 0.7152, 0.0722 };

static void hct_mat(double o[3], double const row[3], double const m[3][3]) {
    double a = row[0] * m[0][0] + row[1] * m[0][1] + row[2] * m[0][2];
    double b = row[0] * m[1][0] + row[1] * m[1][1] + row[2] * m[1][2];
    double c = row[0] * m[2][0] + row[1] * m[2][1] + row[2] * m[2][2];
    o[0] = a; o[1] = b; o[2] = c;
}

static double hct_sanitize_radians(double angle) {
    return ALWAN_FMOD(angle + ALWAN_PI_F64 * 8, ALWAN_PI_F64 * 2);
}

static double hct_true_delinearized(double c) {
    double n = c / 100.0, d;
    if (n <= 0.0031308) d = n * 12.92;
    else d = 1.055 * ALWAN_POW(n, 1.0 / 2.4) - 0.055;
    return d * 255.0;
}

static double hct_chromatic_adaptation(double component) {
    double af = ALWAN_POW(ALWAN_ABS(component), 0.42);
    return hct_signum(component) * 400.0 * af / (af + 27.13);
}

static double hct_hue_of(double const linrgb[3]) {
    double sd[3], rA, gA, bA, a, b;
    hct_mat(sd, linrgb, SCALED_DISCOUNT_FROM_LINRGB);
    rA = hct_chromatic_adaptation(sd[0]);
    gA = hct_chromatic_adaptation(sd[1]);
    bA = hct_chromatic_adaptation(sd[2]);
    a = (11.0 * rA + -12.0 * gA + bA) / 11.0;
    b = (rA + gA - 2.0 * bA) / 9.0;
    return ALWAN_ATAN2(b, a);
}

static int hct_cyclic(double a, double b, double c) {
    return hct_sanitize_radians(b - a) < hct_sanitize_radians(c - a);
}

static void hct_set_coordinate(double o[3], double const src[3], double coordinate, double const tgt[3], int axis) {
    double t = (coordinate - src[axis]) / (tgt[axis] - src[axis]);
    o[0] = src[0] + (tgt[0] - src[0]) * t;
    o[1] = src[1] + (tgt[1] - src[1]) * t;
    o[2] = src[2] + (tgt[2] - src[2]) * t;
}

static int hct_bounded(double x) { return 0.0 <= x && x <= 100.0; }

static void hct_nth_vertex(double o[3], double y, int n) {
    double kR = Y_FROM_LINRGB[0], kG = Y_FROM_LINRGB[1], kB = Y_FROM_LINRGB[2];
    double coordA = n % 4 <= 1 ? 0.0 : 100.0, coordB = n % 2 == 0 ? 0.0 : 100.0;
    if (n < 4) {
        double g = coordA, b = coordB, r = (y - g * kG - b * kB) / kR;
        if (hct_bounded(r)) { o[0] = r; o[1] = g; o[2] = b; return; }
    } else if (n < 8) {
        double b = coordA, r = coordB, g = (y - r * kR - b * kB) / kG;
        if (hct_bounded(g)) { o[0] = r; o[1] = g; o[2] = b; return; }
    } else {
        double r = coordA, g = coordB, b = (y - r * kR - g * kG) / kB;
        if (hct_bounded(b)) { o[0] = r; o[1] = g; o[2] = b; return; }
    }
    o[0] = o[1] = o[2] = -1.0;
}

static void hct_bisect_to_segment(double left[3], double right[3], double y, double target_hue) {
    double left_hue = 0.0, right_hue = 0.0;
    int initialized = 0, uncut = 1, n, i;
    for (i = 0; i < 3; i++) left[i] = right[i] = -1.0;
    for (n = 0; n < 12; n++) {
        double mid[3], mid_hue;
        hct_nth_vertex(mid, y, n);
        if (mid[0] < 0) continue;
        mid_hue = hct_hue_of(mid);
        if (!initialized) {
            for (i = 0; i < 3; i++) left[i] = right[i] = mid[i];
            left_hue = right_hue = mid_hue;
            initialized = 1;
            continue;
        }
        if (uncut || hct_cyclic(left_hue, mid_hue, right_hue)) {
            uncut = 0;
            if (hct_cyclic(left_hue, target_hue, mid_hue)) {
                for (i = 0; i < 3; i++) right[i] = mid[i];
                right_hue = mid_hue;
            } else {
                for (i = 0; i < 3; i++) left[i] = mid[i];
                left_hue = mid_hue;
            }
        }
    }
}

static void hct_bisect_to_limit(double o[3], double y, double target_hue) {
    double left[3], right[3], left_hue;
    int axis, i;
    hct_bisect_to_segment(left, right, y, target_hue);
    left_hue = hct_hue_of(left);
    for (axis = 0; axis < 3; axis++) {
        if (left[axis] != right[axis]) {
            double l_plane, r_plane;
            if (left[axis] < right[axis]) {
                l_plane = ALWAN_FLOOR(hct_true_delinearized(left[axis]) - 0.5);
                r_plane = ALWAN_CEIL(hct_true_delinearized(right[axis]) - 0.5);
            } else {
                l_plane = ALWAN_CEIL(hct_true_delinearized(left[axis]) - 0.5);
                r_plane = ALWAN_FLOOR(hct_true_delinearized(right[axis]) - 0.5);
            }
            for (i = 0; i < 8; i++) {
                if (ALWAN_ABS(r_plane - l_plane) <= 1) break;
                {
                    double m_plane = ALWAN_FLOOR((l_plane + r_plane) / 2.0), mid[3], mid_hue;
                    /* both planes lie in [-1, 255] (a coordinate in [0, 100] delinearizes to
                     * [0, 255]) and are more than 1 apart, so m_plane is in [0, 254] */
                    int m = (int)m_plane;
                    hct_set_coordinate(mid, left, hct_critical_planes[m], right, axis);
                    mid_hue = hct_hue_of(mid);
                    if (hct_cyclic(left_hue, target_hue, mid_hue)) {
                        right[0] = mid[0]; right[1] = mid[1]; right[2] = mid[2];
                        r_plane = m_plane;
                    } else {
                        left[0] = mid[0]; left[1] = mid[1]; left[2] = mid[2];
                        left_hue = mid_hue;
                        l_plane = m_plane;
                    }
                }
            }
        }
    }
    o[0] = (left[0] + right[0]) / 2;
    o[1] = (left[1] + right[1]) / 2;
    o[2] = (left[2] + right[2]) / 2;
}

static double hct_inverse_chromatic_adaptation(double adapted) {
    double adapted_abs = ALWAN_ABS(adapted);
    double base = 27.13 * adapted_abs / (400.0 - adapted_abs);
    if (base < 0) base = 0;
    return hct_signum(adapted) * ALWAN_POW(base, 1.0 / 0.42);
}

static uint32_t hct_find_result_by_j(double hue_radians, double chroma, double y, hct_vc const *v) {
    double j = ALWAN_SQRT(y) * 11.0;
    double tInnerCoeff = 1 / ALWAN_POW(1.64 - ALWAN_POW(0.29, v->n), 0.73);
    double eHue = 0.25 * (ALWAN_COS(hue_radians + 2.0) + 3.8);
    double p1 = eHue * (50000.0 / 13.0) * v->nc * v->ncb;
    double hSin = ALWAN_SIN(hue_radians), hCos = ALWAN_COS(hue_radians);
    int round;
    for (round = 0; round < 5; round++) {
        double jN = j / 100.0;
        double alpha = chroma == 0.0 || j == 0.0 ? 0.0 : chroma / ALWAN_SQRT(jN);
        double t = ALWAN_POW(alpha * tInnerCoeff, 1.0 / 0.9);
        double ac = v->aw * ALWAN_POW(jN, 1.0 / v->c / v->z);
        double p2 = ac / v->nbb;
        double gamma = 23.0 * (p2 + 0.305) * t / (23.0 * p1 + 11 * t * hCos + 108.0 * t * hSin);
        double a = gamma * hCos, b = gamma * hSin;
        double rA = (460.0 * p2 + 451.0 * a + 288.0 * b) / 1403.0;
        double gA = (460.0 * p2 - 891.0 * a - 261.0 * b) / 1403.0;
        double bA = (460.0 * p2 - 220.0 * a - 6300.0 * b) / 1403.0;
        double scaled[3], linrgb[3], fnj;
        scaled[0] = hct_inverse_chromatic_adaptation(rA);
        scaled[1] = hct_inverse_chromatic_adaptation(gA);
        scaled[2] = hct_inverse_chromatic_adaptation(bA);
        hct_mat(linrgb, scaled, LINRGB_FROM_SCALED_DISCOUNT);
        if (linrgb[0] < 0 || linrgb[1] < 0 || linrgb[2] < 0) return 0;
        fnj = Y_FROM_LINRGB[0] * linrgb[0] + Y_FROM_LINRGB[1] * linrgb[1] + Y_FROM_LINRGB[2] * linrgb[2];
        if (fnj <= 0) return 0;
        if (round == 4 || ALWAN_ABS(fnj - y) < 0.002) {
            if (linrgb[0] > 100.01 || linrgb[1] > 100.01 || linrgb[2] > 100.01) return 0;
            return hct_argb_from_linrgb(linrgb);
        }
        j = j - (fnj - y) * j / (2 * fnj);
    }
    return 0;
}

static uint32_t hct_solve_to_int(double hue_degrees, double chroma, double lstar) {
    hct_vc v;
    double hue_radians, y, linrgb[3];
    uint32_t exact;
    if (chroma < 0.0001 || lstar < 0.0001 || lstar > 99.9999) return hct_argb_from_lstar(lstar);
    hue_degrees = ALWAN_FMOD(hue_degrees, 360.0);
    if (hue_degrees < 0) hue_degrees = hue_degrees + 360.0;
    hue_radians = hue_degrees / 180 * ALWAN_PI_F64;
    y = hct_y_from_lstar(lstar);
    hct_vc_default(&v);
    exact = hct_find_result_by_j(hue_radians, chroma, y, &v);
    if (exact != 0) return exact;
    hct_bisect_to_limit(linrgb, y, hue_radians);
    return hct_argb_from_linrgb(linrgb);
}

static int hct_finite(double x) { return x - x == 0.0; }

#if ALWAN_WITH_F64
alwan_status alwan_hct_from_argb_f64(alwan_hct_f64 *out, uint32_t argb) {
    hct_vc v;
    if (!out) return ALWAN_E_INVALID;
    hct_vc_default(&v);
    hct_cam16_from_int(&out->hue, &out->chroma, argb, &v);
    out->tone = hct_lstar_from_argb(argb);
    return ALWAN_OK;
}

alwan_status alwan_hct_to_argb_f64(uint32_t *out, alwan_hct_f64 const *hct) {
    if (!out || !hct || !hct_finite(hct->hue) || !hct_finite(hct->chroma) || !hct_finite(hct->tone)) return ALWAN_E_INVALID;
    *out = hct_solve_to_int(hct->hue, hct->chroma, hct->tone);
    return ALWAN_OK;
}

alwan_status alwan_hct_tonal_palette_f64(uint32_t *out, alwan_f64 const *tones, size_t count, alwan_f64 hue, alwan_f64 chroma) {
    size_t i;
    if (!out || !tones || count == 0 || !hct_finite(hue) || !hct_finite(chroma)) return ALWAN_E_INVALID;
    for (i = 0; i < count; i++) {
        if (!hct_finite(tones[i])) return ALWAN_E_INVALID;
        out[i] = hct_solve_to_int(hue, chroma, tones[i]);
    }
    return ALWAN_OK;
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_hct_from_argb_f32(alwan_hct_f32 *out, uint32_t argb) {
    hct_vc v;
    double h, c;
    if (!out) return ALWAN_E_INVALID;
    hct_vc_default(&v);
    hct_cam16_from_int(&h, &c, argb, &v);
    out->hue = (alwan_f32)h;
    out->chroma = (alwan_f32)c;
    out->tone = (alwan_f32)hct_lstar_from_argb(argb);
    return ALWAN_OK;
}

alwan_status alwan_hct_to_argb_f32(uint32_t *out, alwan_hct_f32 const *hct) {
    if (!out || !hct || !hct_finite(hct->hue) || !hct_finite(hct->chroma) || !hct_finite(hct->tone)) return ALWAN_E_INVALID;
    *out = hct_solve_to_int((double)hct->hue, (double)hct->chroma, (double)hct->tone);
    return ALWAN_OK;
}

alwan_status alwan_hct_tonal_palette_f32(uint32_t *out, alwan_f32 const *tones, size_t count, alwan_f32 hue, alwan_f32 chroma) {
    size_t i;
    if (!out || !tones || count == 0 || !hct_finite(hue) || !hct_finite(chroma)) return ALWAN_E_INVALID;
    for (i = 0; i < count; i++) {
        if (!hct_finite(tones[i])) return ALWAN_E_INVALID;
        out[i] = hct_solve_to_int((double)hue, (double)chroma, (double)tones[i]);
    }
    return ALWAN_OK;
}
#endif
