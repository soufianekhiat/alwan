/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Munsell Renotation System: specification to xyY and back.
 *
 * A transcription of colour-science's colour.notation.munsell (BSD-3-Clause, Colour
 * Developers and Paul Centore, after Centore's MATLAB Munsell and Kubelka-Munk Toolbox),
 * step for step, so that it can serve as the reference (suite 259):
 *
 *   - a specification's xy by interpolating the 1943 renotation data: between the two
 *     bounding renotation hues along the hue angle, linearly or radially about
 *     illuminant C as colour's table of ovoids decides, then between the two bounding
 *     even chromas, then between the two bounding integer values against their
 *     ASTM D1535 luminances;
 *   - xyY to a specification by colour's iteration: the value from Y, a first guess from
 *     CIELAB's hue and chroma, then alternating corrections of the hue angle and of the
 *     chroma until the specification's xy is within 1e-7 of the target.
 *
 * The one deliberate difference: colour takes the Munsell value of Y by interpolating a
 * 0.001-step table of the ASTM D1535 quintic, alwan by inverting the quintic exactly
 * (alwan_munsell_value_f64); the two differ by up to 2.1e-7 in value.
 *
 * alwan names a hue by one number on [0, 100): 10 per family from R (0) through YR, Y,
 * GY, G, BG, B, PB and P to RP (90), 5R = 5, 10R = 10 = 0YR. colour names it by a step in
 * (0, 10] and a family code: 1 B, 2 BG, 3 G, 4 GY, 5 Y, 6 YR, 7 R, 8 RP, 9 P, 10 PB.
 * Y is on the 0 to 1 scale of colour's xyY.
 */

#include "../alwan.h"
#include "../alwan_internal.h"

#define MUNSELL_HUES 4
#define MUNSELL_CODES 10
#define MUNSELL_VALUES 10
#define MUNSELL_CHROMAS 25
#define MUNSELL_THRESHOLD_INTEGER ALWAN_LITERAL(1e-3)

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
/* x, y by [hue 2.5, 5, 7.5, 10][code 1..10][value 1..10][chroma 2..50]; -1, -1 where the
 * renotation has no entry. gendata/data/munsell_tables.py, from colour. */
static alwan_f64 const g_munsell_xy[] = {
#include "../data/munsell/renotation_xy.csv"
};
/* The largest chroma the renotation holds by [hue][code][value]; -1 where none. */
static alwan_f64 const g_munsell_max_chroma[] = {
#include "../data/munsell/max_chroma.csv"
};
static alwan_f64 const g_munsell_c[] = {
#include "../data/munsell/illuminant_c.csv"
};
ALWAN_DIAG_POP

typedef struct {
    int grey;
    alwan_f64 hue, value, chroma;
    int code;
} munsell_spec;

#define MUNSELL_FAIL ALWAN_E_RANGE

/* The ASTM D1535 quintic and its inverse, in the arithmetic of alwan_lightness_impl.inc's
 * alwan_luminance_from_munsell_value and alwan_munsell_value. They are repeated here rather
 * than called because this file is an f64 facade that links in an f32-only build, where the
 * f64 public functions do not exist. Suite 259 holds both to colour. */
static alwan_f64 mun_spow(alwan_f64 x, alwan_f64 p) {
    alwan_f64 const m = ALWAN_ABS(x);
    alwan_f64 const r = m == ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : ALWAN_POW(m, p);
    return x < ALWAN_LITERAL(0.0) ? -r : r;
}

static alwan_f64 mun_lum(alwan_f64 V) {
    return ALWAN_LITERAL(1.1914) * V - ALWAN_LITERAL(0.22533) * mun_spow(V, ALWAN_LITERAL(2.0)) +
           ALWAN_LITERAL(0.23352) * mun_spow(V, ALWAN_LITERAL(3.0)) -
           ALWAN_LITERAL(0.020484) * mun_spow(V, ALWAN_LITERAL(4.0)) +
           ALWAN_LITERAL(0.00081939) * mun_spow(V, ALWAN_LITERAL(5.0));
}

/* Newton from Priest's value; the quintic rises on the whole of its use. */
static alwan_f64 mun_value(alwan_f64 Y) {
    alwan_f64 V = ALWAN_LITERAL(10.0) * ALWAN_SQRT(ALWAN_ABS(Y) / ALWAN_LITERAL(100.0));
    int i;
    for (i = 0; i < 40; i++) {
        alwan_f64 const d = ALWAN_LITERAL(1.1914) - ALWAN_LITERAL(2.0) * ALWAN_LITERAL(0.22533) * V +
                            ALWAN_LITERAL(3.0) * ALWAN_LITERAL(0.23352) * V * V -
                            ALWAN_LITERAL(4.0) * ALWAN_LITERAL(0.020484) * V * V * V +
                            ALWAN_LITERAL(5.0) * ALWAN_LITERAL(0.00081939) * V * V * V * V;
        V = V - (mun_lum(V) - Y) / d;
    }
    return V;
}

/* numpy.interp for increasing xp, as its compiled_interp evaluates it. */
static alwan_f64 np_interp(alwan_f64 x, alwan_f64 const *xp, alwan_f64 const *fp, int n) {
    int j;
    if (x < xp[0]) return fp[0];
    if (x > xp[n - 1]) return fp[n - 1];
    for (j = 0; j < n - 1; j++) {
        if (xp[j] <= x && x < xp[j + 1]) break;
    }
    if (j == n - 1) return fp[n - 1];
    if (x == xp[j]) return fp[j];
    {
        alwan_f64 const slope = (fp[j + 1] - fp[j]) / (xp[j + 1] - xp[j]);
        alwan_f64 r = slope * (x - xp[j]) + fp[j];
        if (r != r) {
            r = slope * (x - xp[j + 1]) + fp[j + 1];
            if (r != r && fp[j] == fp[j + 1]) r = fp[j];
        }
        return r;
    }
}

static alwan_f64 interp2(alwan_f64 x, alwan_f64 x0, alwan_f64 x1, alwan_f64 f0, alwan_f64 f1) {
    alwan_f64 const xp[2] = { x0, x1 }, fp[2] = { f0, f1 };
    return np_interp(x, xp, fp, 2);
}

/* colour's default sdiv mode: a non-finite quotient reads as 0. */
static alwan_f64 sdiv(alwan_f64 a, alwan_f64 b) {
    alwan_f64 q;
    if (b == ALWAN_LITERAL(0.0)) return ALWAN_LITERAL(0.0);
    q = a / b;
    return (q == q && q - q == ALWAN_LITERAL(0.0)) ? q : ALWAN_LITERAL(0.0);
}

/* Extrapolator(LinearInterpolator): linear beyond either end. */
static alwan_f64 extrap_linear(alwan_f64 x, alwan_f64 const *xp, alwan_f64 const *fp, int n) {
    if (x < xp[0]) return fp[0] + (x - xp[0]) * sdiv(fp[1] - fp[0], xp[1] - xp[0]);
    if (x > xp[n - 1]) return fp[n - 1] + (x - xp[n - 1]) * sdiv(fp[n - 1] - fp[n - 2], xp[n - 1] - xp[n - 2]);
    return np_interp(x, xp, fp, n);
}

static alwan_f64 py_mod(alwan_f64 a, alwan_f64 b) {
    alwan_f64 r = ALWAN_FMOD(a, b);
    if (r != ALWAN_LITERAL(0.0) && ((r < ALWAN_LITERAL(0.0)) != (b < ALWAN_LITERAL(0.0)))) r += b;
    return r;
}

/* Python's round(): half to even. */
static alwan_f64 py_round(alwan_f64 v) {
    alwan_f64 const f = ALWAN_FLOOR(v);
    alwan_f64 const d = v - f;
    if (d > ALWAN_LITERAL(0.5)) return f + ALWAN_LITERAL(1.0);
    if (d < ALWAN_LITERAL(0.5)) return f;
    return (py_mod(f, ALWAN_LITERAL(2.0)) == ALWAN_LITERAL(0.0)) ? f : f + ALWAN_LITERAL(1.0);
}

static int is_integer(alwan_f64 v) {
    return ALWAN_ABS(v - py_round(v)) <= MUNSELL_THRESHOLD_INTEGER;
}

static int hue_index(alwan_f64 hue) {
    if (hue == ALWAN_LITERAL(2.5)) return 0;
    if (hue == ALWAN_LITERAL(5.0)) return 1;
    if (hue == ALWAN_LITERAL(7.5)) return 2;
    if (hue == ALWAN_LITERAL(10.0)) return 3;
    return -1;
}

static alwan_status renotation_xy(alwan_f64 *x, alwan_f64 *y, alwan_f64 hue, alwan_f64 value, alwan_f64 chroma, int code) {
    int const h = hue_index(hue);
    int const v = (int)value - 1, k = (int)(chroma / ALWAN_LITERAL(2.0)) - 1;
    size_t idx;
    if (h < 0 || code < 1 || code > MUNSELL_CODES || v < 0 || v >= MUNSELL_VALUES || k < 0 || k >= MUNSELL_CHROMAS) {
        return MUNSELL_FAIL;
    }
    idx = ((((size_t)h * MUNSELL_CODES + (size_t)(code - 1)) * MUNSELL_VALUES + (size_t)v) * MUNSELL_CHROMAS + (size_t)k) * 2;
    if (g_munsell_xy[idx] == ALWAN_LITERAL(-1.0) && g_munsell_xy[idx + 1] == ALWAN_LITERAL(-1.0)) return MUNSELL_FAIL;
    *x = g_munsell_xy[idx];
    *y = g_munsell_xy[idx + 1];
    return ALWAN_OK;
}

static void bounding_hues(alwan_f64 hue, int code, alwan_f64 *hue_cw, int *code_cw, alwan_f64 *hue_ccw, int *code_ccw) {
    if (py_mod(hue, ALWAN_LITERAL(2.5)) == ALWAN_LITERAL(0.0)) {
        if (hue == ALWAN_LITERAL(0.0)) {
            *hue_cw = ALWAN_LITERAL(10.0);
            *code_cw = (code + 1) % 10;
        } else {
            *hue_cw = hue;
            *code_cw = code;
        }
        *hue_ccw = *hue_cw;
        *code_ccw = *code_cw;
        return;
    }
    *hue_cw = ALWAN_LITERAL(2.5) * ALWAN_FLOOR(hue / ALWAN_LITERAL(2.5));
    *hue_ccw = py_mod(*hue_cw + ALWAN_LITERAL(2.5), ALWAN_LITERAL(10.0));
    if (*hue_ccw == ALWAN_LITERAL(0.0)) *hue_ccw = ALWAN_LITERAL(10.0);
    if (*hue_cw == ALWAN_LITERAL(0.0)) {
        *hue_cw = ALWAN_LITERAL(10.0);
        *code_cw = (code + 1) % 10;
        if (*code_cw == 0) *code_cw = 10;
    } else {
        *code_cw = code;
    }
    *code_ccw = code;
}

static alwan_f64 const g_ha_x[9] = { 0, 2, 3, 4, 5, 6, 8, 9, 10 };
static alwan_f64 const g_ha_y[9] = { 0, 45, 70, 135, 160, 225, 255, 315, 360 };

static alwan_f64 hue_to_hue_angle(alwan_f64 hue, int code) {
    alwan_f64 const single = py_mod(py_mod(ALWAN_LITERAL(17.0) - (alwan_f64)code, ALWAN_LITERAL(10.0)) + hue / ALWAN_LITERAL(10.0) -
                                    ALWAN_LITERAL(0.5), ALWAN_LITERAL(10.0));
    return np_interp(single, g_ha_x, g_ha_y, 9);
}

static void hue_angle_to_hue(alwan_f64 angle, alwan_f64 *hue, int *code) {
    alwan_f64 const single = np_interp(angle, g_ha_y, g_ha_x, 9);
    alwan_f64 h;
    if (single <= ALWAN_LITERAL(0.5)) *code = 7;
    else if (single <= ALWAN_LITERAL(1.5)) *code = 6;
    else if (single <= ALWAN_LITERAL(2.5)) *code = 5;
    else if (single <= ALWAN_LITERAL(3.5)) *code = 4;
    else if (single <= ALWAN_LITERAL(4.5)) *code = 3;
    else if (single <= ALWAN_LITERAL(5.5)) *code = 2;
    else if (single <= ALWAN_LITERAL(6.5)) *code = 1;
    else if (single <= ALWAN_LITERAL(7.5)) *code = 10;
    else if (single <= ALWAN_LITERAL(8.5)) *code = 9;
    else if (single <= ALWAN_LITERAL(9.5)) *code = 8;
    else *code = 7;
    h = py_mod(ALWAN_LITERAL(10.0) * py_mod(single, ALWAN_LITERAL(1.0)) + ALWAN_LITERAL(5.0), ALWAN_LITERAL(10.0));
    *hue = (h == ALWAN_LITERAL(0.0)) ? ALWAN_LITERAL(10.0) : h;
}

static alwan_f64 hue_to_astm_hue(alwan_f64 hue, int code) {
    alwan_f64 const a = ALWAN_LITERAL(10.0) * (alwan_f64)(((7 - code) % 10 + 10) % 10) + hue;
    return (a == ALWAN_LITERAL(0.0)) ? ALWAN_LITERAL(100.0) : a;
}

#define IN(a, b) (A > ALWAN_LITERAL(a) && A < ALWAN_LITERAL(b))

/* colour's interpolation_method_from_renotation_ovoid: 1 linear, 2 radial, 0 none. */
static int ovoid_method(alwan_f64 hue, alwan_f64 value_in, alwan_f64 chroma_in, int code) {
    int const value = (int)py_round(value_in);
    int const chroma = (int)(ALWAN_LITERAL(2.0) * py_round(chroma_in / ALWAN_LITERAL(2.0)));
    alwan_f64 const A = hue_to_astm_hue(hue, code);
    switch (value) {
    case 1:
        if (chroma == 2) return (IN(15, 30) || IN(60, 85)) ? 2 : 1;
        if (chroma == 4) return (IN(12.5, 27.5) || IN(57.5, 80)) ? 2 : 1;
        if (chroma == 6) return IN(55, 80) ? 2 : 1;
        if (chroma == 8) return IN(67.5, 77.5) ? 2 : 1;
        if (chroma >= 10) return IN(72.5, 77.5) ? 2 : 1;
        return 1;
    case 2:
        if (chroma == 2) return (IN(15, 27.5) || IN(77.5, 80)) ? 2 : 1;
        if (chroma == 4) return (IN(12.5, 30) || IN(62.5, 80)) ? 2 : 1;
        if (chroma == 6) return (IN(7.5, 22.5) || IN(62.5, 80)) ? 2 : 1;
        if (chroma == 8) return (IN(7.5, 15) || IN(60, 80)) ? 2 : 1;
        if (chroma >= 10) return IN(65, 77.5) ? 2 : 1;
        return 1;
    case 3:
        if (chroma == 2) return (IN(10, 37.5) || IN(65, 85)) ? 2 : 1;
        if (chroma == 4) return (IN(5, 37.5) || IN(55, 72.5)) ? 2 : 1;
        if (chroma == 6 || chroma == 8 || chroma == 10) return (IN(7.5, 37.5) || IN(57.5, 82.5)) ? 2 : 1;
        if (chroma >= 12) return (IN(7.5, 42.5) || IN(57.5, 80)) ? 2 : 1;
        return 1;
    case 4:
        if (chroma == 2 || chroma == 4) return (IN(7.5, 42.5) || IN(57.5, 85)) ? 2 : 1;
        if (chroma == 6 || chroma == 8) return (IN(7.5, 40) || IN(57.5, 82.5)) ? 2 : 1;
        if (chroma >= 10) return (IN(7.5, 40) || IN(57.5, 80)) ? 2 : 1;
        return 1;
    case 5:
        if (chroma == 2) return (IN(5, 37.5) || IN(55, 85)) ? 2 : 1;
        if (chroma == 4 || chroma == 6 || chroma == 8) return (IN(2.5, 42.5) || IN(55, 85)) ? 2 : 1;
        if (chroma >= 10) return (IN(2.5, 42.5) || IN(55, 82.5)) ? 2 : 1;
        return 1;
    case 6:
        if (chroma == 2 || chroma == 4) return (IN(5, 37.5) || IN(55, 87.5)) ? 2 : 1;
        if (chroma == 6) return (IN(5, 42.5) || IN(57.5, 87.5)) ? 2 : 1;
        if (chroma == 8 || chroma == 10) return (IN(5, 42.5) || IN(60, 85)) ? 2 : 1;
        if (chroma == 12 || chroma == 14) return (IN(5, 42.5) || IN(60, 82.5)) ? 2 : 1;
        if (chroma >= 16) return (IN(5, 42.5) || IN(60, 80)) ? 2 : 1;
        return 1;
    case 7:
        if (chroma == 2 || chroma == 4 || chroma == 6) return (IN(5, 42.5) || IN(60, 85)) ? 2 : 1;
        if (chroma == 8) return (IN(5, 42.5) || IN(60, 82.5)) ? 2 : 1;
        if (chroma == 10) return (IN(30, 42.5) || IN(5, 25) || IN(60, 82.5)) ? 2 : 1;
        if (chroma == 12) return (IN(30, 42.5) || IN(7.5, 27.5) || IN(80, 82.5)) ? 2 : 1;
        if (chroma >= 14) return (IN(32.5, 40) || IN(7.5, 15) || IN(80, 82.5)) ? 2 : 1;
        return 1;
    case 8:
        if (chroma >= 2 && chroma <= 12) return (IN(5, 40) || IN(60, 85)) ? 2 : 1;
        if (chroma >= 14) return (IN(32.5, 40) || IN(5, 15) || IN(60, 85)) ? 2 : 1;
        return 1;
    case 9:
        if (chroma == 2 || chroma == 4) return (IN(5, 40) || IN(55, 80)) ? 2 : 1;
        if (chroma >= 6 && chroma <= 14) return IN(5, 42.5) ? 2 : 1;
        if (chroma >= 16) return IN(35, 42.5) ? 2 : 1;
        return 1;
    default:
        return 0;
    }
}

#undef IN

/* colour's xy_from_renotation_ovoid: value an integer in [1, 9], chroma even. */
static alwan_status xy_ovoid(alwan_f64 *x_out, alwan_f64 *y_out, alwan_f64 hue, alwan_f64 value, alwan_f64 chroma, int code) {
    alwan_f64 const xg = g_munsell_c[0], yg = g_munsell_c[1];
    alwan_f64 hm, hp, xm, ym, xp, yp, rho_m, phi_m, rho_p, phi_p, lo, ha, up;
    int cm, cp, method;
    alwan_status st;
    static alwan_f64 const grid[5] = { 0.0, 2.5, 5.0, 7.5, 10.0 };
    int q;
    if (!(value >= ALWAN_LITERAL(1.0) && value <= ALWAN_LITERAL(9.0))) return MUNSELL_FAIL;
    value = py_round(value);
    chroma = ALWAN_LITERAL(2.0) * py_round(chroma / ALWAN_LITERAL(2.0));
    if (!(chroma >= ALWAN_LITERAL(2.0) && chroma <= ALWAN_LITERAL(50.0))) return MUNSELL_FAIL;
    for (q = 0; q < 5; q++) {
        if (ALWAN_ABS(hue - grid[q]) < MUNSELL_THRESHOLD_INTEGER) {
            hue = ALWAN_LITERAL(2.5) * py_round(hue / ALWAN_LITERAL(2.5));
            if (hue == ALWAN_LITERAL(0.0)) {
                hue = ALWAN_LITERAL(10.0);
                code = (code + 1) % 10;
            }
            return renotation_xy(x_out, y_out, hue, value, chroma, code);
        }
    }
    bounding_hues(hue, code, &hm, &cm, &hp, &cp);
    st = renotation_xy(&xm, &ym, hm, value, chroma, cm);
    if (st != ALWAN_OK) return st;
    st = renotation_xy(&xp, &yp, hp, value, chroma, cp);
    if (st != ALWAN_OK) return st;
    rho_m = ALWAN_SQRT((xm - xg) * (xm - xg) + (ym - yg) * (ym - yg));
    phi_m = ALWAN_ATAN2(ym - yg, xm - xg) * (ALWAN_LITERAL(180.0) / ALWAN_PI);
    rho_p = ALWAN_SQRT((xp - xg) * (xp - xg) + (yp - yg) * (yp - yg));
    phi_p = ALWAN_ATAN2(yp - yg, xp - xg) * (ALWAN_LITERAL(180.0) / ALWAN_PI);
    lo = hue_to_hue_angle(hm, cm);
    ha = hue_to_hue_angle(hue, code);
    up = hue_to_hue_angle(hp, cp);
    if (phi_m - phi_p > ALWAN_LITERAL(180.0)) phi_p += ALWAN_LITERAL(360.0);
    if (lo == ALWAN_LITERAL(0.0)) lo = ALWAN_LITERAL(360.0);
    if (lo > up) {
        if (lo > ha) {
            lo -= ALWAN_LITERAL(360.0);
        } else {
            lo -= ALWAN_LITERAL(360.0);
            ha -= ALWAN_LITERAL(360.0);
        }
    }
    method = ovoid_method(hue, value, chroma, code);
    if (method == 1) {
        *x_out = interp2(ha, lo, up, xm, xp);
        *y_out = interp2(ha, lo, up, ym, yp);
        return ALWAN_OK;
    }
    if (method == 2) {
        alwan_f64 const rho = interp2(ha, lo, up, rho_m, rho_p);
        alwan_f64 const phi = interp2(ha, lo, up, phi_m, phi_p) * (ALWAN_PI / ALWAN_LITERAL(180.0));
        *x_out = rho * ALWAN_COS(phi) + xg;
        *y_out = rho * ALWAN_SIN(phi) + yg;
        return ALWAN_OK;
    }
    return MUNSELL_FAIL;
}

/* colour's munsell_specification_to_xy: value an integer, chroma any. */
static alwan_status spec_to_xy(alwan_f64 *x, alwan_f64 *y, alwan_f64 hue, alwan_f64 value, alwan_f64 chroma, int code) {
    alwan_f64 cm, cp, xm, ym, xp, yp;
    alwan_status st;
    value = py_round(value);
    if (py_mod(chroma, ALWAN_LITERAL(2.0)) == ALWAN_LITERAL(0.0)) {
        cm = cp = chroma;
    } else {
        cm = ALWAN_LITERAL(2.0) * ALWAN_FLOOR(chroma / ALWAN_LITERAL(2.0));
        cp = cm + ALWAN_LITERAL(2.0);
    }
    if (cm == ALWAN_LITERAL(0.0)) {
        xm = g_munsell_c[0];
        ym = g_munsell_c[1];
    } else {
        st = xy_ovoid(&xm, &ym, hue, value, cm, code);
        if (st != ALWAN_OK) return st;
    }
    st = xy_ovoid(&xp, &yp, hue, value, cp, code);
    if (st != ALWAN_OK) return st;
    if (cm == cp) {
        *x = xm;
        *y = ym;
    } else {
        *x = interp2(chroma, cm, cp, xm, xp);
        *y = interp2(chroma, cm, cp, ym, yp);
    }
    return ALWAN_OK;
}

/* colour's _munsell_specification_to_xyY, Y on the 0 to 1 scale. */
static alwan_status spec_to_xyY(alwan_f64 *x, alwan_f64 *y, alwan_f64 *Y, munsell_spec s) {
    alwan_f64 const Yv = mun_lum(s.value);
    alwan_f64 vm, vp, xm, ym, xp, yp;
    alwan_status st;
    if (!s.grey && s.hue == ALWAN_LITERAL(0.0)) {
        s.hue = ALWAN_LITERAL(10.0);
        s.code = (s.code + 1) % 10;
    }
    if (!s.grey && !(s.hue >= ALWAN_LITERAL(0.0) && s.hue <= ALWAN_LITERAL(10.0))) return MUNSELL_FAIL;
    if (!(s.value >= ALWAN_LITERAL(0.0) && s.value <= ALWAN_LITERAL(10.0))) return MUNSELL_FAIL;
    if (is_integer(s.value)) {
        vm = vp = py_round(s.value);
    } else {
        vm = ALWAN_FLOOR(s.value);
        vp = vm + ALWAN_LITERAL(1.0);
    }
    if (s.grey) {
        xm = g_munsell_c[0];
        ym = g_munsell_c[1];
    } else {
        st = spec_to_xy(&xm, &ym, s.hue, vm, s.chroma, s.code);
        if (st != ALWAN_OK) return st;
    }
    if (s.grey || vp == ALWAN_LITERAL(10.0)) {
        xp = g_munsell_c[0];
        yp = g_munsell_c[1];
    } else {
        st = spec_to_xy(&xp, &yp, s.hue, vp, s.chroma, s.code);
        if (st != ALWAN_OK) return st;
    }
    if (vm == vp) {
        *x = xm;
        *y = ym;
    } else {
        alwan_f64 const Ym = mun_lum(vm), Yp = mun_lum(vp);
        *x = interp2(Yv, Ym, Yp, xm, xp);
        *y = interp2(Yv, Ym, Yp, ym, yp);
    }
    *Y = Yv / ALWAN_LITERAL(100.0);
    return ALWAN_OK;
}

static alwan_status max_chroma_lookup(alwan_f64 *out, alwan_f64 hue, alwan_f64 value, int code) {
    int const h = hue_index(hue), v = (int)value - 1;
    size_t idx;
    if (h < 0 || code < 1 || code > MUNSELL_CODES || v < 0 || v >= MUNSELL_VALUES) return MUNSELL_FAIL;
    idx = ((size_t)h * MUNSELL_CODES + (size_t)(code - 1)) * MUNSELL_VALUES + (size_t)v;
    if (g_munsell_max_chroma[idx] == ALWAN_LITERAL(-1.0)) return MUNSELL_FAIL;
    *out = g_munsell_max_chroma[idx];
    return ALWAN_OK;
}

/* colour's maximum_chroma_from_renotation. */
static alwan_status max_chroma(alwan_f64 *out, alwan_f64 hue, alwan_f64 value, int code) {
    alwan_f64 vm, vp, hc, hw, a, b, c, d;
    int cc, cw;
    alwan_status st;
    if (value >= ALWAN_LITERAL(9.99)) {
        *out = ALWAN_LITERAL(0.0);
        return ALWAN_OK;
    }
    if (!(value >= ALWAN_LITERAL(1.0) && value <= ALWAN_LITERAL(10.0))) return MUNSELL_FAIL;
    if (py_mod(value, ALWAN_LITERAL(1.0)) == ALWAN_LITERAL(0.0)) {
        vm = vp = value;
    } else {
        vm = ALWAN_FLOOR(value);
        vp = vm + ALWAN_LITERAL(1.0);
    }
    bounding_hues(hue, code, &hc, &cc, &hw, &cw);
    if ((st = max_chroma_lookup(&a, hc, vm, cc)) != ALWAN_OK) return st;
    if ((st = max_chroma_lookup(&b, hw, vm, cw)) != ALWAN_OK) return st;
    if (vp <= ALWAN_LITERAL(9.0)) {
        alwan_f64 m;
        if ((st = max_chroma_lookup(&c, hc, vp, cc)) != ALWAN_OK) return st;
        if ((st = max_chroma_lookup(&d, hw, vp, cw)) != ALWAN_OK) return st;
        m = a;
        if (b < m) m = b;
        if (c < m) m = c;
        if (d < m) m = d;
        *out = m;
    } else {
        alwan_f64 const L = mun_lum(value), L9 = mun_lum(ALWAN_LITERAL(9.0)), L10 = mun_lum(ALWAN_LITERAL(10.0));
        alwan_f64 const p = interp2(L, L9, L10, a, ALWAN_LITERAL(0.0)), r = interp2(L, L9, L10, b, ALWAN_LITERAL(0.0));
        *out = p < r ? p : r;
    }
    return ALWAN_OK;
}

static alwan_f64 lab_f(alwan_f64 t) {
    alwan_f64 const e = (ALWAN_LITERAL(24.0) / ALWAN_LITERAL(116.0)) * (ALWAN_LITERAL(24.0) / ALWAN_LITERAL(116.0)) *
                        (ALWAN_LITERAL(24.0) / ALWAN_LITERAL(116.0));
    return t > e ? ALWAN_POW(t, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0))
                 : (ALWAN_LITERAL(841.0) / ALWAN_LITERAL(108.0)) * t + ALWAN_LITERAL(16.0) / ALWAN_LITERAL(116.0);
}

static int fsign(alwan_f64 v) {
    return (v > ALWAN_LITERAL(0.0)) - (v < ALWAN_LITERAL(0.0));
}

/* Stable insertion sort of k keys carrying values, as numpy's argsort on these few. */
static void sort_pairs(alwan_f64 *keys, alwan_f64 *vals, int n) {
    int i, j;
    for (i = 1; i < n; i++) {
        alwan_f64 const k = keys[i], v = vals[i];
        for (j = i - 1; j >= 0 && keys[j] > k; j--) {
            keys[j + 1] = keys[j];
            vals[j + 1] = vals[j];
        }
        keys[j + 1] = k;
        vals[j + 1] = v;
    }
}

static alwan_f64 phi_diff(alwan_f64 phi_in, alwan_f64 phi) {
    alwan_f64 d = py_mod(ALWAN_LITERAL(360.0) - phi_in + phi, ALWAN_LITERAL(360.0));
    if (d > ALWAN_LITERAL(180.0)) d -= ALWAN_LITERAL(360.0);
    return d;
}

/* colour's _xyY_to_munsell_specification, the value from alwan_munsell_value. */
static alwan_status xyY_to_spec(munsell_spec *out, alwan_f64 x, alwan_f64 y, alwan_f64 Y) {
    alwan_f64 const rad = ALWAN_LITERAL(180.0) / ALWAN_PI;
    alwan_f64 value, xc, yc, Yc, rho_in, phi_in, L, a, b, C, H, conv;
    munsell_spec s;
    alwan_status st;
    int it;
    value = mun_value(Y * ALWAN_LITERAL(100.0));
    if (is_integer(value)) value = py_round(value);
    s.grey = 1;
    s.value = value;
    s.hue = s.chroma = ALWAN_LITERAL(0.0);
    s.code = 0;
    st = spec_to_xyY(&xc, &yc, &Yc, s);
    if (st != ALWAN_OK) return st;
    rho_in = ALWAN_SQRT((x - xc) * (x - xc) + (y - yc) * (y - yc));
    phi_in = ALWAN_ATAN2(y - yc, x - xc) * rad;
    if (rho_in < MUNSELL_THRESHOLD_INTEGER) {
        *out = s;
        return ALWAN_OK;
    }
    {
        alwan_f64 const X = x * Y / y, Z = (ALWAN_LITERAL(1.0) - x - y) * Y / y;
        alwan_f64 const Xr = g_munsell_c[0] / g_munsell_c[1];
        alwan_f64 const Zr = (ALWAN_LITERAL(1.0) - g_munsell_c[0] - g_munsell_c[1]) / g_munsell_c[1];
        alwan_f64 const fx = lab_f(X / Xr), fy = lab_f(Y), fz = lab_f(Z / Zr);
        L = ALWAN_LITERAL(116.0) * fy - ALWAN_LITERAL(16.0);
        a = ALWAN_LITERAL(500.0) * (fx - fy);
        b = ALWAN_LITERAL(200.0) * (fy - fz);
    }
    C = ALWAN_SQRT(a * a + b * b);
    H = py_mod(ALWAN_ATAN2(b, a) * rad, ALWAN_LITERAL(360.0));
    {
        static alwan_f64 const lims[9] = { 36, 72, 108, 144, 180, 216, 252, 288, 324 };
        static int const codes[9] = { 7, 6, 5, 4, 3, 2, 1, 10, 9 };
        int code = 8, k;
        alwan_f64 hue0;
        if (H != ALWAN_LITERAL(0.0)) {
            for (k = 0; k < 9; k++) {
                if (H <= lims[k]) {
                    code = codes[k];
                    break;
                }
            }
        }
        hue0 = interp2(py_mod(H, ALWAN_LITERAL(36.0)), ALWAN_LITERAL(0.0), ALWAN_LITERAL(36.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(10.0));
        if (hue0 == ALWAN_LITERAL(0.0)) hue0 = ALWAN_LITERAL(10.0);
        s.grey = 0;
        s.hue = hue0;
        s.value = value;
        s.chroma = ALWAN_LITERAL(5.0) / ALWAN_LITERAL(5.5) * (C / ALWAN_LITERAL(5.0));
        s.code = code;
        (void)L;
    }
    conv = MUNSELL_THRESHOLD_INTEGER / ALWAN_LITERAL(10000.0);
    for (it = 1; it <= 65; it++) {
        alwan_f64 ha_c, cmax, xcur, ycur, Ycur, phi_c, phis[16 + 2], hads[16 + 2], had_new, ha_new, rho_c, chroma_c;
        alwan_f64 rhos[16 + 2], chs[16 + 2];
        int np_, inner, extrapolate, k;
        ha_c = hue_to_hue_angle(s.hue, s.code);
        if ((st = max_chroma(&cmax, s.hue, value, s.code)) != ALWAN_OK) return st;
        if (s.chroma > cmax) s.chroma = cmax;
        if ((st = spec_to_xyY(&xcur, &ycur, &Ycur, s)) != ALWAN_OK) return st;
        phi_c = ALWAN_ATAN2(ycur - yc, xcur - xc) * rad;
        phis[0] = phi_diff(phi_in, phi_c);
        hads[0] = ALWAN_LITERAL(0.0);
        np_ = 1;
        inner = 0;
        extrapolate = 0;
        for (;;) {
            alwan_f64 mn = phis[0], mx = phis[0];
            for (k = 1; k < np_; k++) {
                if (phis[k] < mn) mn = phis[k];
                if (phis[k] > mx) mx = phis[k];
            }
            if (!(fsign(mn) == fsign(mx) && !extrapolate)) break;
            inner++;
            if (inner > 16) return MUNSELL_FAIL;
            {
                alwan_f64 const ha_i = py_mod(ha_c + (alwan_f64)inner * (phi_in - phi_c), ALWAN_LITERAL(360.0));
                alwan_f64 had_i = py_mod((alwan_f64)inner * (phi_in - phi_c), ALWAN_LITERAL(360.0));
                munsell_spec si;
                alwan_f64 xi, yi, Yi;
                if (had_i > ALWAN_LITERAL(180.0)) had_i -= ALWAN_LITERAL(360.0);
                si.grey = 0;
                si.value = value;
                si.chroma = s.chroma;
                hue_angle_to_hue(ha_i, &si.hue, &si.code);
                if ((st = spec_to_xyY(&xi, &yi, &Yi, si)) != ALWAN_OK) return st;
                if (np_ >= 2) extrapolate = 1;
                if (!extrapolate) {
                    phis[np_] = phi_diff(phi_in, ALWAN_ATAN2(yi - yc, xi - xc) * rad);
                    hads[np_] = had_i;
                    np_++;
                }
            }
        }
        sort_pairs(phis, hads, np_);
        had_new = py_mod(extrap_linear(ALWAN_LITERAL(0.0), phis, hads, np_), ALWAN_LITERAL(360.0));
        ha_new = py_mod(ha_c + had_new, ALWAN_LITERAL(360.0));
        hue_angle_to_hue(ha_new, &s.hue, &s.code);
        if ((st = spec_to_xyY(&xcur, &ycur, &Ycur, s)) != ALWAN_OK) return st;
        if (ALWAN_SQRT((x - xcur) * (x - xcur) + (y - ycur) * (y - ycur)) < conv) {
            *out = s;
            return ALWAN_OK;
        }
        if ((st = max_chroma(&cmax, s.hue, value, s.code)) != ALWAN_OK) return st;
        if (s.chroma > cmax) s.chroma = cmax;
        if ((st = spec_to_xyY(&xcur, &ycur, &Ycur, s)) != ALWAN_OK) return st;
        rho_c = ALWAN_SQRT((xcur - xc) * (xcur - xc) + (ycur - yc) * (ycur - yc));
        chroma_c = s.chroma;   /* colour scales from this local; its clamp below writes only the spec */
        rhos[0] = rho_c;
        chs[0] = s.chroma;
        np_ = 1;
        inner = 0;
        for (;;) {
            alwan_f64 mn = rhos[0], mx = rhos[0];
            for (k = 1; k < np_; k++) {
                if (rhos[k] < mn) mn = rhos[k];
                if (rhos[k] > mx) mx = rhos[k];
            }
            if (mn < rho_in && rho_in < mx) break;
            inner++;
            if (inner > 16) return MUNSELL_FAIL;
            {
                alwan_f64 ch_i = ALWAN_POW(rho_in / rho_c, (alwan_f64)inner) * chroma_c;
                munsell_spec si;
                alwan_f64 xi, yi, Yi;
                if (ch_i > cmax) ch_i = cmax;
                si.grey = 0;
                si.hue = s.hue;
                si.value = value;
                si.chroma = ch_i;
                si.code = s.code;
                if ((st = spec_to_xyY(&xi, &yi, &Yi, si)) != ALWAN_OK) return st;
                rhos[np_] = ALWAN_SQRT((xi - xc) * (xi - xc) + (yi - yc) * (yi - yc));
                chs[np_] = ch_i;
                np_++;
            }
        }
        sort_pairs(rhos, chs, np_);
        s.chroma = np_interp(rho_in, rhos, chs, np_);
        if ((st = spec_to_xyY(&xcur, &ycur, &Ycur, s)) != ALWAN_OK) return st;
        if (ALWAN_SQRT((x - xcur) * (x - xcur) + (y - ycur) * (y - ycur)) < conv) {
            *out = s;
            return ALWAN_OK;
        }
    }
    return MUNSELL_FAIL;
}

/* alwan's hue number to colour's (step, code), the step in (0, 10]. */
static void hue_from_number(alwan_f64 h, alwan_f64 *step, int *code) {
    static int const codes[10] = { 7, 6, 5, 4, 3, 2, 1, 10, 9, 8 };
    int f;
    h = py_mod(h, ALWAN_LITERAL(100.0));
    f = (int)ALWAN_FLOOR(h / ALWAN_LITERAL(10.0));
    *step = h - ALWAN_LITERAL(10.0) * (alwan_f64)f;
    if (*step == ALWAN_LITERAL(0.0)) {
        *step = ALWAN_LITERAL(10.0);
        f = (f + 9) % 10;
    }
    *code = codes[f];
}

static alwan_status adapt(alwan_xyz_f64 *xyz, alwan_illuminant from, alwan_illuminant to) {
    alwan_xyz_f64 w_from, w_to;
    alwan_mat3x3_f64 m;
    alwan_vec3_f64 in, out;
    alwan_status st;
    if (from == to) return ALWAN_OK;
    if ((st = alwan_illuminant_white_point_f64(&w_from, from, ALWAN_OBSERVER_CIE_1931_2DEG)) != ALWAN_OK) return st;
    if ((st = alwan_illuminant_white_point_f64(&w_to, to, ALWAN_OBSERVER_CIE_1931_2DEG)) != ALWAN_OK) return st;
    if ((st = alwan_cat_matrix_f64(&m, &w_from, &w_to, ALWAN_CAT_BRADFORD)) != ALWAN_OK) return st;
    in.v[0] = xyz->x;
    in.v[1] = xyz->y;
    in.v[2] = xyz->z;
    alwan_mat3_mulv_f64(&out, &m, &in);
    xyz->x = out.v[0];
    xyz->y = out.v[1];
    xyz->z = out.v[2];
    return ALWAN_OK;
}

alwan_status alwan_munsell_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_f64 hue, alwan_f64 value, alwan_f64 chroma,
                                      alwan_illuminant illuminant) {
    munsell_spec s;
    alwan_f64 x, y, Y;
    alwan_status st;
    if (!xyz) return ALWAN_E_INVALID;
    if (!(value >= ALWAN_LITERAL(0.0) && value <= ALWAN_LITERAL(10.0)) || !(chroma >= ALWAN_LITERAL(0.0)) || hue != hue) {
        return ALWAN_E_RANGE;
    }
    s.value = value;
    s.chroma = chroma;
    s.grey = chroma == ALWAN_LITERAL(0.0);
    if (s.grey) {
        s.hue = ALWAN_LITERAL(0.0);
        s.code = 0;
    } else {
        hue_from_number(hue, &s.hue, &s.code);
    }
    st = spec_to_xyY(&x, &y, &Y, s);
    if (st != ALWAN_OK) return st;
    if (y == ALWAN_LITERAL(0.0)) return ALWAN_E_RANGE;
    xyz->x = x * Y / y;
    xyz->y = Y;
    xyz->z = (ALWAN_LITERAL(1.0) - x - y) * Y / y;
    return adapt(xyz, ALWAN_ILLUMINANT_C, illuminant);
}

alwan_status alwan_xyz_to_munsell_f64(alwan_f64 *hue, alwan_f64 *value, alwan_f64 *chroma, alwan_xyz_f64 const *xyz,
                                      alwan_illuminant illuminant) {
    alwan_xyz_f64 c;
    alwan_f64 sum;
    munsell_spec s;
    alwan_status st;
    if (!xyz || !hue || !value || !chroma) return ALWAN_E_INVALID;
    c = *xyz;
    if ((st = adapt(&c, illuminant, ALWAN_ILLUMINANT_C)) != ALWAN_OK) return st;
    sum = c.x + c.y + c.z;
    if (!(sum > ALWAN_LITERAL(0.0)) || !(c.y > ALWAN_LITERAL(0.0))) return ALWAN_E_RANGE;
    st = xyY_to_spec(&s, c.x / sum, c.y / sum, c.y);
    if (st != ALWAN_OK) return st;
    *value = s.value;
    if (s.grey) {
        *hue = ALWAN_LITERAL(0.0);
        *chroma = ALWAN_LITERAL(0.0);
    } else {
        *hue = hue_to_astm_hue(s.hue, s.code);
        *chroma = s.chroma;
    }
    return ALWAN_OK;
}
