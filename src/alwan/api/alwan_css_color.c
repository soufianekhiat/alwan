/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * CSS Color 4/5 colours: conversions between the CSS colour spaces, color-mix(), steps,
 * the L* and DeltaPhi* contrasts, and Okhwb. Suite 303.
 *
 * The spaces, their matrices and transfer functions, the tree of base spaces, the
 * treatment of missing components and the interpolation are ported from color.js 0.5.2
 * (src/spaces/, src/space.js, src/interpolation.js, src/angles.js, src/adapt.js,
 * src/contrast/Lstar.js, src/contrast/deltaPhi.js), under its MIT licence:
 *
 *   Copyright (c) 2021 Lea Verou, Chris Lilley
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a copy of this
 *   software and associated documentation files (the "Software"), to deal in the Software
 *   without restriction, including without limitation the rights to use, copy, modify,
 *   merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 *   permit persons to whom the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall be included in all copies
 *   or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 *   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 *   PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 *   CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 *   THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Arithmetic is written in color.js's order (matrix rows summed left to right, the same
 * constant expressions), so a conversion agrees with it to the libm's last bits.
 * Premultiplied interpolation leaves the hue alone, as CSS Color 4 sec. 12.3 says, where
 * color.js 0.5.2 multiplies it too.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <string.h>

static double cs_nan(void) {
    static uint64_t const bits = 0x7ff8000000000000ULL;
    double d;
    memcpy(&d, &bits, sizeof d);
    return d;
}

static int cs_isnan(double v) { return v != v; }

/* ---- the space tree ---- */

enum {
    N_XYZ_D65, N_XYZ_D50, N_SRGB_LIN, N_SRGB, N_HSL, N_HSV, N_HWB, N_P3_LIN, N_P3, N_A98_LIN, N_A98,
    N_PP_LIN, N_PP, N_R2020_LIN, N_R2020, N_LAB, N_LCH, N_OKLAB, N_OKLCH, N_LAB_D65, N_COUNT
};

static int const cs_parent[N_COUNT] = {
    -1, N_XYZ_D65, N_XYZ_D65, N_SRGB_LIN, N_SRGB, N_HSL, N_HSV, N_XYZ_D65, N_P3_LIN, N_XYZ_D65, N_A98_LIN,
    N_XYZ_D50, N_PP_LIN, N_XYZ_D65, N_R2020_LIN, N_XYZ_D50, N_LAB, N_XYZ_D65, N_OKLAB, N_XYZ_D65
};

static int cs_node(alwan_css_space s) {
    switch (s) {
    case ALWAN_CSS_SPACE_OKLAB: return N_OKLAB;
    case ALWAN_CSS_SPACE_OKLCH: return N_OKLCH;
    case ALWAN_CSS_SPACE_SRGB: return N_SRGB;
    case ALWAN_CSS_SPACE_SRGB_LINEAR: return N_SRGB_LIN;
    case ALWAN_CSS_SPACE_DISPLAY_P3: return N_P3;
    case ALWAN_CSS_SPACE_A98_RGB: return N_A98;
    case ALWAN_CSS_SPACE_PROPHOTO_RGB: return N_PP;
    case ALWAN_CSS_SPACE_REC2020: return N_R2020;
    case ALWAN_CSS_SPACE_LAB: return N_LAB;
    case ALWAN_CSS_SPACE_LCH: return N_LCH;
    case ALWAN_CSS_SPACE_XYZ_D50: return N_XYZ_D50;
    case ALWAN_CSS_SPACE_XYZ_D65: return N_XYZ_D65;
    case ALWAN_CSS_SPACE_HSL: return N_HSL;
    case ALWAN_CSS_SPACE_HWB: return N_HWB;
    default: return -1;
    }
}

/* which coordinate is a hue, -1 for none */
static int cs_hue_index(int node) {
    switch (node) {
    case N_LCH: case N_OKLCH: return 2;
    case N_HSL: case N_HSV: case N_HWB: return 0;
    default: return -1;
    }
}

static void cs_mul(double o[3], double const m[3][3], double const v[3]) {
    double x = v[0], y = v[1], z = v[2];
    o[0] = m[0][0] * x + m[0][1] * y + m[0][2] * z;
    o[1] = m[1][0] * x + m[1][1] * y + m[1][2] * z;
    o[2] = m[2][0] * x + m[2][1] * y + m[2][2] * z;
}

static double const M_D65_TO_D50[3][3] = {
    { 1.0479297925449969, 0.022946870601609652, -0.05019226628920524 },
    { 0.02962780877005599, 0.9904344267538799, -0.017073799063418826 },
    { -0.009243040646204504, 0.015055191490298152, 0.7518742814281371 },
};
static double const M_D50_TO_D65[3][3] = {
    { 0.955473421488075, -0.02309845494876471, 0.06325924320057072 },
    { -0.0283697093338637, 1.0099953980813041, 0.021041441191917323 },
    { 0.012314014864481998, -0.020507649298898964, 1.330365926242124 },
};
static double const M_SRGB_TO_XYZ[3][3] = {
    { 0.41239079926595934, 0.357584339383878, 0.1804807884018343 },
    { 0.21263900587151027, 0.715168678767756, 0.07219231536073371 },
    { 0.01933081871559182, 0.11919477979462598, 0.9505321522496607 },
};
static double const M_XYZ_TO_SRGB[3][3] = {
    { 3.2409699419045226, -1.537383177570094, -0.4986107602930034 },
    { -0.9692436362808796, 1.8759675015077202, 0.04155505740717559 },
    { 0.05563007969699366, -0.20397695888897652, 1.0569715142428786 },
};
static double const M_P3_TO_XYZ[3][3] = {
    { 0.4865709486482162, 0.26566769316909306, 0.1982172852343625 },
    { 0.2289745640697488, 0.6917385218365064, 0.079286914093745 },
    { 0.0000000000000000, 0.04511338185890264, 1.043944368900976 },
};
static double const M_XYZ_TO_P3[3][3] = {
    { 2.493496911941425, -0.9313836179191239, -0.40271078445071684 },
    { -0.8294889695615747, 1.7626640603183463, 0.023624685841943577 },
    { 0.03584583024378447, -0.07617238926804182, 0.9568845240076872 },
};
static double const M_A98_TO_XYZ[3][3] = {
    { 0.5766690429101305, 0.1855582379065463, 0.1882286462349947 },
    { 0.29734497525053605, 0.6273635662554661, 0.07529145849399788 },
    { 0.02703136138641234, 0.07068885253582723, 0.9913375368376388 },
};
static double const M_XYZ_TO_A98[3][3] = {
    { 2.0415879038107465, -0.5650069742788596, -0.34473135077832956 },
    { -0.9692436362808795, 1.8759675015077202, 0.04155505740717557 },
    { 0.013444280632031142, -0.11836239223101838, 1.0151749943912054 },
};
static double const M_PP_TO_XYZ[3][3] = {
    { 0.79776664490064230, 0.13518129740053308, 0.03134773412839220 },
    { 0.28807482881940130, 0.71183523424187300, 0.00008993693872564 },
    { 0.00000000000000000, 0.00000000000000000, 0.82510460251046020 },
};
static double const M_XYZ_TO_PP[3][3] = {
    { 1.34578688164715830, -0.25557208737979464, -0.05110186497554526 },
    { -0.54463070512490190, 1.50824774284514680, 0.02052744743642139 },
    { 0.00000000000000000, 0.00000000000000000, 1.21196754563894520 },
};
static double const M_R2020_TO_XYZ[3][3] = {
    { 0.6369580483012914, 0.14461690358620832, 0.1688809751641721 },
    { 0.2627002120112671, 0.6779980715188708, 0.05930171646986196 },
    { 0.000000000000000, 0.028072693049087428, 1.060985057710791 },
};
static double const M_XYZ_TO_R2020[3][3] = {
    { 1.716651187971268, -0.355670783776392, -0.253366281373660 },
    { -0.666684351832489, 1.616481236634939, 0.0157685458139111 },
    { 0.017639857445311, -0.042770613257809, 0.942103121235474 },
};
static double const M_XYZ_TO_LMS[3][3] = {
    { 0.8190224379967030, 0.3619062600528904, -0.1288737815209879 },
    { 0.0329836539323885, 0.9292868615863434, 0.0361446663506424 },
    { 0.0481771893596242, 0.2642395317527308, 0.6335478284694309 },
};
static double const M_LMS_TO_XYZ[3][3] = {
    { 1.2268798758459243, -0.5578149944602171, 0.2813910456659647 },
    { -0.0405757452148008, 1.1122868032803170, -0.0717110580655164 },
    { -0.0763729366746601, -0.4214933324022432, 1.5869240198367816 },
};
static double const M_LMS_TO_OKLAB[3][3] = {
    { 0.2104542683093140, 0.7936177747023054, -0.0040720430116193 },
    { 1.9779985324311684, -2.4285922420485799, 0.4505937096174110 },
    { 0.0259040424655478, 0.7827717124575296, -0.8086757549230774 },
};
static double const M_OKLAB_TO_LMS[3][3] = {
    { 1.0000000000000000, 0.3963377773761749, 0.2158037573099136 },
    { 1.0000000000000000, -0.1055613458156586, -0.0638541728258133 },
    { 1.0000000000000000, -0.0894841775298119, -1.2914855480194092 },
};

static double cs_sign(double v) { return v > 0.0 ? 1.0 : (v < 0.0 ? -1.0 : v); }
static double cs_constrain(double a) { return ALWAN_FMOD(ALWAN_FMOD(a, 360.0) + 360.0, 360.0); }
static double cs_min(double a, double b) { return a < b ? a : b; }
static double cs_max(double a, double b) { return a > b ? a : b; }

/* sRGB's transfer, also P3's */
static double cs_srgb_from_lin(double val) {
    double sign = val < 0 ? -1.0 : 1.0, abs = val * sign;
    if (abs > 0.0031308) return sign * (1.055 * ALWAN_POW(abs, 1.0 / 2.4) - 0.055);
    return 12.92 * val;
}
static double cs_srgb_to_lin(double val) {
    double sign = val < 0 ? -1.0 : 1.0, abs = val * sign;
    if (abs <= 0.04045) return val / 12.92;
    return sign * ALWAN_POW((abs + 0.055) / 1.055, 2.4);
}

#define CS_R2020_A 1.09929682680944
#define CS_R2020_B 0.018053968510807

static double const cs_white_d50[3] = { 0.3457 / 0.3585, 1.00000, (1.0 - 0.3457 - 0.3585) / 0.3585 };
static double const cs_white_d65[3] = { 0.3127 / 0.3290, 1.00000, (1.0 - 0.3127 - 0.3290) / 0.3290 };

static void cs_lab_from_xyz(double o[3], double const xyz[3], double const white[3]) {
    double const e = 216.0 / 24389.0, k = 24389.0 / 27.0;
    double f[3];
    int i;
    for (i = 0; i < 3; i++) {
        double v = xyz[i] / white[i];
        f[i] = v > e ? ALWAN_CBRT(v) : (k * v + 16) / 116;
    }
    o[0] = (116 * f[1]) - 16;
    o[1] = 500 * (f[0] - f[1]);
    o[2] = 200 * (f[1] - f[2]);
}
static void cs_lab_to_xyz(double o[3], double const lab[3], double const white[3]) {
    double const e3 = 24.0 / 116.0, k = 24389.0 / 27.0;
    double f0, f1, f2, x, y, z;
    f1 = (lab[0] + 16) / 116;
    f0 = lab[1] / 500 + f1;
    f2 = f1 - lab[2] / 200;
    x = f0 > e3 ? ALWAN_POW(f0, 3.0) : (116 * f0 - 16) / k;
    y = lab[0] > 8 ? ALWAN_POW((lab[0] + 16) / 116, 3.0) : lab[0] / k;
    z = f2 > e3 ? ALWAN_POW(f2, 3.0) : (116 * f2 - 16) / k;
    o[0] = x * white[0];
    o[1] = y * white[1];
    o[2] = z * white[2];
}

static void cs_polar_from(double o[3], double const v[3], double eps) {
    double L = v[0], a = v[1], b = v[2], h;
    if (ALWAN_ABS(a) < eps && ALWAN_ABS(b) < eps) h = cs_nan();
    else h = ALWAN_ATAN2(b, a) * 180 / ALWAN_PI_F64;
    o[0] = L;
    o[1] = ALWAN_SQRT(a * a + b * b);
    o[2] = cs_constrain(h);
}

/* one step towards the root */
static void cs_to_base(int node, double c[3]) {
    double t[3];
    int i;
    switch (node) {
    case N_XYZ_D50: cs_mul(t, M_D50_TO_D65, c); break;
    case N_SRGB_LIN: cs_mul(t, M_SRGB_TO_XYZ, c); break;
    case N_P3_LIN: cs_mul(t, M_P3_TO_XYZ, c); break;
    case N_A98_LIN: cs_mul(t, M_A98_TO_XYZ, c); break;
    case N_PP_LIN: cs_mul(t, M_PP_TO_XYZ, c); break;
    case N_R2020_LIN: cs_mul(t, M_R2020_TO_XYZ, c); break;
    case N_SRGB: case N_P3:
        for (i = 0; i < 3; i++) t[i] = cs_srgb_to_lin(c[i]);
        break;
    case N_A98:
        for (i = 0; i < 3; i++) t[i] = ALWAN_POW(ALWAN_ABS(c[i]), 563.0 / 256.0) * cs_sign(c[i]);
        break;
    case N_PP:
        for (i = 0; i < 3; i++) t[i] = c[i] < 16.0 / 512.0 ? c[i] / 16 : ALWAN_POW(c[i], 1.8);
        break;
    case N_R2020:
        for (i = 0; i < 3; i++)
            t[i] = c[i] < CS_R2020_B * 4.5 ? c[i] / 4.5 : ALWAN_POW((c[i] + CS_R2020_A - 1) / CS_R2020_A, 1 / 0.45);
        break;
    case N_HSL: {
        double h = ALWAN_FMOD(c[0], 360.0), s, l, a;
        int n;
        static double const ns[3] = { 0, 8, 4 };
        if (h < 0) h += 360;
        s = c[1] / 100;
        l = c[2] / 100;
        for (n = 0; n < 3; n++) {
            double k = ALWAN_FMOD(ns[n] + h / 30, 12.0);
            a = s * cs_min(l, 1 - l);
            t[n] = l - a * cs_max(-1, cs_min(cs_min(k - 3, 9 - k), 1));
        }
        break;
    }
    case N_HSV: {   /* to HSL */
        double s = c[1] / 100, v = c[2] / 100, l = v * (1 - s / 2);
        t[0] = c[0];
        t[1] = (l == 0 || l == 1) ? 0 : ((v - l) / cs_min(l, 1 - l)) * 100;
        t[2] = l * 100;
        break;
    }
    case N_HWB: {   /* to HSV */
        double w = c[1] / 100, b = c[2] / 100, sum = w + b, v, s;
        if (sum >= 1) {
            double gray = w / sum;
            t[0] = c[0]; t[1] = 0; t[2] = gray * 100;
            break;
        }
        v = (1 - b);
        s = (v == 0) ? 0 : 1 - w / v;
        t[0] = c[0]; t[1] = s * 100; t[2] = v * 100;
        break;
    }
    case N_LAB: cs_lab_to_xyz(t, c, cs_white_d50); break;
    case N_LAB_D65: cs_lab_to_xyz(t, c, cs_white_d65); break;
    case N_LCH: {
        double C = c[1] < 0 ? 0 : c[1], H = cs_isnan(c[2]) ? 0 : c[2];
        t[0] = c[0];
        t[1] = C * ALWAN_COS(H * ALWAN_PI_F64 / 180);
        t[2] = C * ALWAN_SIN(H * ALWAN_PI_F64 / 180);
        break;
    }
    case N_OKLAB: {
        double lmsg[3], lms[3];
        cs_mul(lmsg, M_OKLAB_TO_LMS, c);
        for (i = 0; i < 3; i++) lms[i] = ALWAN_POW(lmsg[i], 3.0);
        cs_mul(t, M_LMS_TO_XYZ, lms);
        break;
    }
    case N_OKLCH:
        t[0] = c[0];
        if (cs_isnan(c[2])) { t[1] = 0; t[2] = 0; }
        else { t[1] = c[1] * ALWAN_COS(c[2] * ALWAN_PI_F64 / 180); t[2] = c[1] * ALWAN_SIN(c[2] * ALWAN_PI_F64 / 180); }
        break;
    default: return;
    }
    c[0] = t[0]; c[1] = t[1]; c[2] = t[2];
}

/* one step away from the root, into node */
static void cs_from_base(int node, double c[3]) {
    double t[3];
    int i;
    switch (node) {
    case N_XYZ_D50: cs_mul(t, M_D65_TO_D50, c); break;
    case N_SRGB_LIN: cs_mul(t, M_XYZ_TO_SRGB, c); break;
    case N_P3_LIN: cs_mul(t, M_XYZ_TO_P3, c); break;
    case N_A98_LIN: cs_mul(t, M_XYZ_TO_A98, c); break;
    case N_PP_LIN: cs_mul(t, M_XYZ_TO_PP, c); break;
    case N_R2020_LIN: cs_mul(t, M_XYZ_TO_R2020, c); break;
    case N_SRGB: case N_P3:
        for (i = 0; i < 3; i++) t[i] = cs_srgb_from_lin(c[i]);
        break;
    case N_A98:
        for (i = 0; i < 3; i++) t[i] = ALWAN_POW(ALWAN_ABS(c[i]), 256.0 / 563.0) * cs_sign(c[i]);
        break;
    case N_PP:
        for (i = 0; i < 3; i++) t[i] = c[i] >= 1.0 / 512.0 ? ALWAN_POW(c[i], 1 / 1.8) : 16 * c[i];
        break;
    case N_R2020:
        for (i = 0; i < 3; i++)
            t[i] = c[i] >= CS_R2020_B ? CS_R2020_A * ALWAN_POW(c[i], 0.45) - (CS_R2020_A - 1) : 4.5 * c[i];
        break;
    case N_HSL: {   /* from sRGB */
        double r = c[0], g = c[1], b = c[2];
        double mx = cs_max(cs_max(r, g), b), mn = cs_min(cs_min(r, g), b);
        double h = cs_nan(), s = 0, l = (mn + mx) / 2, d = mx - mn;
        if (d != 0) {
            s = (l == 0 || l == 1) ? 0 : (mx - l) / cs_min(l, 1 - l);
            if (mx == r) h = (g - b) / d + (g < b ? 6 : 0);
            else if (mx == g) h = (b - r) / d + 2;
            else h = (r - g) / d + 4;
            h = h * 60;
        }
        if (s < 0) { h += 180; s = ALWAN_ABS(s); }
        if (h >= 360) h -= 360;
        t[0] = h; t[1] = s * 100; t[2] = l * 100;
        break;
    }
    case N_HSV: {   /* from HSL */
        double s = c[1] / 100, l = c[2] / 100, v = l + s * cs_min(l, 1 - l);
        t[0] = c[0];
        t[1] = v == 0 ? 0 : 200 * (1 - l / v);
        t[2] = 100 * v;
        break;
    }
    case N_HWB:     /* from HSV */
        t[0] = c[0];
        t[1] = c[2] * (100 - c[1]) / 100;
        t[2] = 100 - c[2];
        break;
    case N_LAB: cs_lab_from_xyz(t, c, cs_white_d50); break;
    case N_LAB_D65: cs_lab_from_xyz(t, c, cs_white_d65); break;
    case N_LCH: cs_polar_from(t, c, 0.02); break;
    case N_OKLAB: {
        double lms[3], lmsg[3];
        cs_mul(lms, M_XYZ_TO_LMS, c);
        for (i = 0; i < 3; i++) lmsg[i] = ALWAN_CBRT(lms[i]);
        cs_mul(t, M_LMS_TO_OKLAB, lmsg);
        break;
    }
    case N_OKLCH: cs_polar_from(t, c, 0.0002); break;
    default: return;
    }
    c[0] = t[0]; c[1] = t[1]; c[2] = t[2];
}

static int cs_path(int node, int path[8]) {
    int tmp[8], n = 0, i;
    for (; node >= 0; node = cs_parent[node]) tmp[n++] = node;
    for (i = 0; i < n; i++) path[i] = tmp[n - 1 - i];
    return n;
}

/* color.js's ColorSpace.to: copy within a space, otherwise missing components become 0 and
 * the colour goes up to the common base and down. */
static void cs_convert(double o[3], int from, int to, double const in[3]) {
    int pf[8], pt[8], nf, nt, lca = 0, i;
    double c[3];
    if (from == to) { o[0] = in[0]; o[1] = in[1]; o[2] = in[2]; return; }
    for (i = 0; i < 3; i++) c[i] = cs_isnan(in[i]) ? 0.0 : in[i];
    nf = cs_path(from, pf);
    nt = cs_path(to, pt);
    for (i = 0; i < nf && i < nt && pf[i] == pt[i]; i++) lca = i;
    for (i = nf - 1; i > lca; i--) cs_to_base(pf[i], c);
    for (i = lca + 1; i < nt; i++) cs_from_base(pt[i], c);
    o[0] = c[0]; o[1] = c[1]; o[2] = c[2];
}

/* ---- color-mix ---- */

typedef struct { int node; double c[3]; double alpha; } cs_color;

static double cs_interp(double s, double e, double p) {
    if (cs_isnan(s)) return e;
    if (cs_isnan(e)) return s;
    return s + (e - s) * p;
}

static void cs_adjust(alwan_css_hue_method arc, double *h1, double *h2) {
    double a1 = cs_constrain(*h1), a2 = cs_constrain(*h2), d = a2 - a1;
    if (arc == ALWAN_CSS_HUE_INCREASING) {
        if (d < 0) a2 += 360;
    } else if (arc == ALWAN_CSS_HUE_DECREASING) {
        if (d > 0) a1 += 360;
    } else if (arc == ALWAN_CSS_HUE_LONGER) {
        if (-180 < d && d < 180) {
            if (d > 0) a1 += 360;
            else a2 += 360;
        }
    } else {
        if (d > 180) a1 += 360;
        else if (d < -180) a2 += 360;
    }
    *h1 = a1;
    *h2 = a2;
}

typedef struct {
    int space, out;
    alwan_css_hue_method hue;
    int premultiplied;
    cs_color a, b;   /* in the interpolation space, hues fixed, premultiplied if asked */
} cs_range;

static alwan_status cs_range_init(cs_range *r, cs_color const *a, cs_color const *b, alwan_css_mix_params const *params) {
    alwan_css_space sp = params ? params->space : ALWAN_CSS_SPACE_DEFAULT;
    alwan_css_space os = params ? params->output_space : ALWAN_CSS_SPACE_DEFAULT;
    int hi, i;
    r->space = cs_node(sp == ALWAN_CSS_SPACE_DEFAULT ? ALWAN_CSS_SPACE_OKLAB : sp);
    r->out = os == ALWAN_CSS_SPACE_DEFAULT ? r->space : cs_node(os);
    r->hue = params ? params->hue : ALWAN_CSS_HUE_SHORTER;
    r->premultiplied = params ? params->premultiplied : 0;
    if (r->space < 0 || r->out < 0 || (unsigned)r->hue > (unsigned)ALWAN_CSS_HUE_DECREASING) return ALWAN_E_INVALID;
    r->a.node = r->b.node = r->space;
    cs_convert(r->a.c, a->node, r->space, a->c);
    cs_convert(r->b.c, b->node, r->space, b->c);
    r->a.alpha = a->alpha;
    r->b.alpha = b->alpha;
    hi = cs_hue_index(r->space);
    if (hi >= 0) {
        double h1 = r->a.c[hi], h2 = r->b.c[hi];
        if (cs_isnan(h1) && !cs_isnan(h2)) h1 = h2;
        else if (cs_isnan(h2) && !cs_isnan(h1)) h2 = h1;
        cs_adjust(r->hue, &h1, &h2);
        r->a.c[hi] = h1;
        r->b.c[hi] = h2;
    }
    if (r->premultiplied) {
        for (i = 0; i < 3; i++) {
            if (i == hi) continue;
            r->a.c[i] = r->a.c[i] * r->a.alpha;
            r->b.c[i] = r->b.c[i] * r->b.alpha;
        }
    }
    return ALWAN_OK;
}

static void cs_range_at(cs_color *o, cs_range const *r, double p) {
    int i, hi = cs_hue_index(r->space);
    double c[3], alpha;
    for (i = 0; i < 3; i++) c[i] = cs_interp(r->a.c[i], r->b.c[i], p);
    alpha = cs_interp(r->a.alpha, r->b.alpha, p);
    if (r->premultiplied) {
        for (i = 0; i < 3; i++)
            if (i != hi) c[i] = c[i] / alpha;
    }
    o->node = r->out;
    o->alpha = alpha;
    cs_convert(o->c, r->space, r->out, c);
}

/* ---- public forms ---- */

static int cs_in(cs_color *o, alwan_css_space s, double const c[3], double alpha) {
    o->node = cs_node(s);
    o->c[0] = c[0]; o->c[1] = c[1]; o->c[2] = c[2];
    o->alpha = alpha;
    return o->node >= 0;
}

static alwan_css_space cs_space_of(int node) {
    alwan_css_space s;
    for (s = ALWAN_CSS_SPACE_OKLAB; s <= ALWAN_CSS_SPACE_HWB; s = (alwan_css_space)(s + 1))
        if (cs_node(s) == node) return s;
    return ALWAN_CSS_SPACE_DEFAULT;
}

static alwan_status cs_mix(cs_color *o, cs_color const *a, cs_color const *b, double p, alwan_css_mix_params const *params) {
    cs_range r;
    alwan_status st = cs_range_init(&r, a, b, params);
    if (st != ALWAN_OK) return st;
    cs_range_at(o, &r, p);
    return ALWAN_OK;
}

static alwan_status cs_mix_percent(cs_color *o, cs_color const *a, double pa, cs_color const *b, double pb,
                                   alwan_css_mix_params const *params) {
    double sum, mult = 1.0;
    alwan_status st;
    if (cs_isnan(pa) && cs_isnan(pb)) { pa = 50; pb = 50; }
    else if (cs_isnan(pb)) pb = 100 - pa;
    else if (cs_isnan(pa)) pa = 100 - pb;
    if (!(pa >= 0 && pa <= 100 && pb >= 0 && pb <= 100)) return ALWAN_E_INVALID;
    sum = pa + pb;
    if (sum == 0) return ALWAN_E_INVALID;
    if (sum < 100) mult = sum / 100;
    st = cs_mix(o, a, b, pb / sum, params);
    if (st == ALWAN_OK) o->alpha = o->alpha * mult;
    return st;
}

static double cs_lightness(cs_color const *c, int lab_node) {
    double o[3];
    cs_convert(o, c->node, lab_node, c->c);
    return o[0];
}

static double cs_delta_phi(cs_color const *a, cs_color const *b) {
    double const phi = ALWAN_POW(5.0, 0.5) * 0.5 + 0.5;
    double l1 = cs_lightness(a, N_LAB_D65), l2 = cs_lightness(b, N_LAB_D65);
    double dps = ALWAN_ABS(ALWAN_POW(l1, phi) - ALWAN_POW(l2, phi));
    double contrast = ALWAN_POW(dps, (1 / phi)) * 1.4142135623730951 - 40;
    return (contrast < 7.5) ? 0.0 : contrast;
}


#if ALWAN_WITH_F64
static int cs_load_f64(cs_color *o, alwan_css_color_f64 const *c) {
    double v[3];
    v[0] = (double)c->coords[0]; v[1] = (double)c->coords[1]; v[2] = (double)c->coords[2];
    return cs_in(o, c->space, v, (double)c->alpha);
}
static void cs_store_f64(alwan_css_color_f64 *o, cs_color const *c) {
    o->space = cs_space_of(c->node);
    o->coords[0] = (alwan_f64)c->c[0]; o->coords[1] = (alwan_f64)c->c[1]; o->coords[2] = (alwan_f64)c->c[2];
    o->alpha = (alwan_f64)c->alpha;
}
alwan_status alwan_css_color_convert_f64(alwan_css_color_f64 *out, alwan_css_space space,
                                           alwan_css_color_f64 const *in) {
    cs_color a, o;
    int to;
    if (!out || !in || !cs_load_f64(&a, in)) return ALWAN_E_INVALID;
    to = cs_node(space);
    if (to < 0) return ALWAN_E_INVALID;
    o.node = to;
    o.alpha = a.alpha;
    cs_convert(o.c, a.node, to, a.c);
    cs_store_f64(out, &o);
    return ALWAN_OK;
}
alwan_status alwan_css_color_mix_f64(alwan_css_color_f64 *out, alwan_css_color_f64 const *a,
                                       alwan_css_color_f64 const *b, alwan_f64 p, alwan_css_mix_params const *params) {
    cs_color ca, cb, o;
    alwan_status st;
    if (!out || !a || !b || !cs_load_f64(&ca, a) || !cs_load_f64(&cb, b)) return ALWAN_E_INVALID;
    st = cs_mix(&o, &ca, &cb, (double)p, params);
    if (st == ALWAN_OK) cs_store_f64(out, &o);
    return st;
}
alwan_status alwan_css_color_mix_percent_f64(alwan_css_color_f64 *out, alwan_css_color_f64 const *a,
                                               alwan_f64 percent_a, alwan_css_color_f64 const *b, alwan_f64 percent_b,
                                               alwan_css_mix_params const *params) {
    cs_color ca, cb, o;
    alwan_status st;
    if (!out || !a || !b || !cs_load_f64(&ca, a) || !cs_load_f64(&cb, b)) return ALWAN_E_INVALID;
    st = cs_mix_percent(&o, &ca, (double)percent_a, &cb, (double)percent_b, params);
    if (st == ALWAN_OK) cs_store_f64(out, &o);
    return st;
}
alwan_status alwan_css_color_steps_f64(alwan_css_color_f64 *out, size_t count, alwan_css_color_f64 const *a,
                                         alwan_css_color_f64 const *b, alwan_css_mix_params const *params) {
    cs_color ca, cb, o;
    cs_range r;
    alwan_status st;
    size_t i;
    if (!out || count == 0 || !a || !b || !cs_load_f64(&ca, a) || !cs_load_f64(&cb, b))
        return ALWAN_E_INVALID;
    st = cs_range_init(&r, &ca, &cb, params);
    if (st != ALWAN_OK) return st;
    if (count == 1) {
        cs_range_at(&o, &r, 0.5);
        cs_store_f64(out, &o);
        return ALWAN_OK;
    }
    for (i = 0; i < count; i++) {
        double step = 1.0 / (double)(count - 1);
        cs_range_at(&o, &r, (double)i * step);
        cs_store_f64(&out[i], &o);
    }
    return ALWAN_OK;
}
alwan_status alwan_css_contrast_lstar_f64(alwan_f64 *out, alwan_css_color_f64 const *a, alwan_css_color_f64 const *b) {
    cs_color ca, cb;
    if (!out || !a || !b || !cs_load_f64(&ca, a) || !cs_load_f64(&cb, b)) return ALWAN_E_INVALID;
    *out = (alwan_f64)ALWAN_ABS(cs_lightness(&ca, N_LAB) - cs_lightness(&cb, N_LAB));
    return ALWAN_OK;
}
alwan_status alwan_css_contrast_delta_phi_f64(alwan_f64 *out, alwan_css_color_f64 const *a,
                                                alwan_css_color_f64 const *b) {
    cs_color ca, cb;
    if (!out || !a || !b || !cs_load_f64(&ca, a) || !cs_load_f64(&cb, b)) return ALWAN_E_INVALID;
    *out = (alwan_f64)cs_delta_phi(&ca, &cb);
    return ALWAN_OK;
}

void alwan_srgb_to_okhwb_f64(alwan_okhwb_f64 *out, alwan_rgb_f64 const *srgb) {
    alwan_okhsv_f64 hsv;
    if (!out || !srgb) return;
    alwan_srgb_to_okhsv_f64(&hsv, srgb);
    out->h = hsv.h;
    out->w = (1 - hsv.s) * hsv.v;
    out->b = 1 - hsv.v;
}

void alwan_okhwb_to_srgb_f64(alwan_rgb_f64 *out, alwan_okhwb_f64 const *okhwb) {
    alwan_okhsv_f64 hsv;
    double w, b, sum;
    if (!out || !okhwb) return;
    w = okhwb->w;
    b = okhwb->b;
    sum = w + b;
    hsv.h = okhwb->h;
    if (sum >= 1) {
        hsv.s = 0;
        hsv.v = w / sum;
    } else {
        hsv.v = 1 - b;
        hsv.s = hsv.v == 0 ? 0 : 1 - w / hsv.v;
    }
    alwan_okhsv_to_srgb_f64(out, &hsv);
}
#endif

#if ALWAN_WITH_F32
static int cs_load_f32(cs_color *o, alwan_css_color_f32 const *c) {
    double v[3];
    v[0] = (double)c->coords[0]; v[1] = (double)c->coords[1]; v[2] = (double)c->coords[2];
    return cs_in(o, c->space, v, (double)c->alpha);
}
static void cs_store_f32(alwan_css_color_f32 *o, cs_color const *c) {
    o->space = cs_space_of(c->node);
    o->coords[0] = (alwan_f32)c->c[0]; o->coords[1] = (alwan_f32)c->c[1]; o->coords[2] = (alwan_f32)c->c[2];
    o->alpha = (alwan_f32)c->alpha;
}
alwan_status alwan_css_color_convert_f32(alwan_css_color_f32 *out, alwan_css_space space,
                                           alwan_css_color_f32 const *in) {
    cs_color a, o;
    int to;
    if (!out || !in || !cs_load_f32(&a, in)) return ALWAN_E_INVALID;
    to = cs_node(space);
    if (to < 0) return ALWAN_E_INVALID;
    o.node = to;
    o.alpha = a.alpha;
    cs_convert(o.c, a.node, to, a.c);
    cs_store_f32(out, &o);
    return ALWAN_OK;
}
alwan_status alwan_css_color_mix_f32(alwan_css_color_f32 *out, alwan_css_color_f32 const *a,
                                       alwan_css_color_f32 const *b, alwan_f32 p, alwan_css_mix_params const *params) {
    cs_color ca, cb, o;
    alwan_status st;
    if (!out || !a || !b || !cs_load_f32(&ca, a) || !cs_load_f32(&cb, b)) return ALWAN_E_INVALID;
    st = cs_mix(&o, &ca, &cb, (double)p, params);
    if (st == ALWAN_OK) cs_store_f32(out, &o);
    return st;
}
alwan_status alwan_css_color_mix_percent_f32(alwan_css_color_f32 *out, alwan_css_color_f32 const *a,
                                               alwan_f32 percent_a, alwan_css_color_f32 const *b, alwan_f32 percent_b,
                                               alwan_css_mix_params const *params) {
    cs_color ca, cb, o;
    alwan_status st;
    if (!out || !a || !b || !cs_load_f32(&ca, a) || !cs_load_f32(&cb, b)) return ALWAN_E_INVALID;
    st = cs_mix_percent(&o, &ca, (double)percent_a, &cb, (double)percent_b, params);
    if (st == ALWAN_OK) cs_store_f32(out, &o);
    return st;
}
alwan_status alwan_css_color_steps_f32(alwan_css_color_f32 *out, size_t count, alwan_css_color_f32 const *a,
                                         alwan_css_color_f32 const *b, alwan_css_mix_params const *params) {
    cs_color ca, cb, o;
    cs_range r;
    alwan_status st;
    size_t i;
    if (!out || count == 0 || !a || !b || !cs_load_f32(&ca, a) || !cs_load_f32(&cb, b))
        return ALWAN_E_INVALID;
    st = cs_range_init(&r, &ca, &cb, params);
    if (st != ALWAN_OK) return st;
    if (count == 1) {
        cs_range_at(&o, &r, 0.5);
        cs_store_f32(out, &o);
        return ALWAN_OK;
    }
    for (i = 0; i < count; i++) {
        double step = 1.0 / (double)(count - 1);
        cs_range_at(&o, &r, (double)i * step);
        cs_store_f32(&out[i], &o);
    }
    return ALWAN_OK;
}
alwan_status alwan_css_contrast_lstar_f32(alwan_f32 *out, alwan_css_color_f32 const *a, alwan_css_color_f32 const *b) {
    cs_color ca, cb;
    if (!out || !a || !b || !cs_load_f32(&ca, a) || !cs_load_f32(&cb, b)) return ALWAN_E_INVALID;
    *out = (alwan_f32)ALWAN_ABS(cs_lightness(&ca, N_LAB) - cs_lightness(&cb, N_LAB));
    return ALWAN_OK;
}
alwan_status alwan_css_contrast_delta_phi_f32(alwan_f32 *out, alwan_css_color_f32 const *a,
                                                alwan_css_color_f32 const *b) {
    cs_color ca, cb;
    if (!out || !a || !b || !cs_load_f32(&ca, a) || !cs_load_f32(&cb, b)) return ALWAN_E_INVALID;
    *out = (alwan_f32)cs_delta_phi(&ca, &cb);
    return ALWAN_OK;
}

void alwan_srgb_to_okhwb_f32(alwan_okhwb_f32 *out, alwan_rgb_f32 const *srgb) {
    alwan_okhsv_f32 hsv;
    if (!out || !srgb) return;
    alwan_srgb_to_okhsv_f32(&hsv, srgb);
    out->h = hsv.h;
    out->w = (1 - hsv.s) * hsv.v;
    out->b = 1 - hsv.v;
}

void alwan_okhwb_to_srgb_f32(alwan_rgb_f32 *out, alwan_okhwb_f32 const *okhwb) {
    alwan_okhsv_f32 hsv;
    float w, b, sum;
    if (!out || !okhwb) return;
    w = okhwb->w;
    b = okhwb->b;
    sum = w + b;
    hsv.h = okhwb->h;
    if (sum >= 1) {
        hsv.s = 0;
        hsv.v = w / sum;
    } else {
        hsv.v = 1 - b;
        hsv.s = hsv.v == 0 ? 0.0f : 1 - w / hsv.v;
    }
    alwan_okhsv_to_srgb_f32(out, &hsv);
}
#endif
