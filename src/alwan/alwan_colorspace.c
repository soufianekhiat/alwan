/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <math.h>

/* ================================================================
 * XYZ <-> xyY Conversions
 * ================================================================ */

void alwan_xyz_to_xyy(alwan_xyz const *xyz, alwan_xyy *xyy) {
    alwan_scalar const X = xyz->x;
    alwan_scalar const Y = xyz->y;
    alwan_scalar const Z = xyz->z;

    alwan_scalar const sum = X + Y + Z;

    if (sum < ALWAN_EPSILON) {
        /* Black point: return [0,0,0] for reversibility */
        xyy->x = ALWAN_LITERAL(0.0);
        xyy->y = ALWAN_LITERAL(0.0);
        xyy->Y = ALWAN_LITERAL(0.0);
    } else {
        xyy->x = X / sum;  /* x */
        xyy->y = Y / sum;  /* y */
        xyy->Y = Y;        /* Y */
    }
}

void alwan_xyy_to_xyz(alwan_xyy const *xyy, alwan_xyz *xyz) {
    alwan_scalar const x = xyy->x;
    alwan_scalar const y = xyy->y;
    alwan_scalar const Y = xyy->Y;

    if (y < ALWAN_EPSILON) {
        /* Degenerate case */
        xyz->x = ALWAN_LITERAL(0.0);
        xyz->y = ALWAN_LITERAL(0.0);
        xyz->z = ALWAN_LITERAL(0.0);
    } else {
        xyz->x = (x * Y) / y;           /* X */
        xyz->y = Y;                     /* Y */
        xyz->z = ((ALWAN_LITERAL(1.0) - x - y) * Y) / y;  /* Z */
    }
}

/* ================================================================
 * XYZ <-> Lab Conversions
 * ================================================================ */

/* CIE Lab f(t) function with δ = 6/29 */
static inline alwan_scalar lab_f(alwan_scalar t) {
    alwan_scalar const delta = ALWAN_LITERAL(6.0) / ALWAN_LITERAL(29.0);
    alwan_scalar const delta3 = delta * delta * delta;
    alwan_scalar const kappa = ALWAN_LITERAL(1.0) / (ALWAN_LITERAL(3.0) * delta * delta);
    alwan_scalar const offset = ALWAN_LITERAL(16.0) / ALWAN_LITERAL(116.0);

    if (t > delta3) {
        return ALWAN_POW(t, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));
    } else {
        return kappa * t + offset;
    }
}

/* Inverse of CIE Lab f(t) function */
static inline alwan_scalar lab_f_inv(alwan_scalar t) {
    alwan_scalar const delta = ALWAN_LITERAL(6.0) / ALWAN_LITERAL(29.0);
    alwan_scalar const threshold = delta;
    alwan_scalar const kappa = ALWAN_LITERAL(3.0) * delta * delta;
    alwan_scalar const offset = ALWAN_LITERAL(16.0) / ALWAN_LITERAL(116.0);

    if (t > threshold) {
        return t * t * t;
    } else {
        return kappa * (t - offset);
    }
}

void alwan_xyz_to_lab(alwan_xyz const *xyz, alwan_xyz const *white_xyz, alwan_lab *lab) {
    alwan_scalar const xr = xyz->x / white_xyz->x;
    alwan_scalar const yr = xyz->y / white_xyz->y;
    alwan_scalar const zr = xyz->z / white_xyz->z;

    alwan_scalar const fx = lab_f(xr);
    alwan_scalar const fy = lab_f(yr);
    alwan_scalar const fz = lab_f(zr);

    lab->L = ALWAN_LITERAL(116.0) * fy - ALWAN_LITERAL(16.0);  /* L* */
    lab->a = ALWAN_LITERAL(500.0) * (fx - fy);                 /* a* */
    lab->b = ALWAN_LITERAL(200.0) * (fy - fz);                 /* b* */
}

void alwan_lab_to_xyz(alwan_lab const *lab, alwan_xyz const *white_xyz, alwan_xyz *xyz) {
    alwan_scalar const L = lab->L;
    alwan_scalar const a = lab->a;
    alwan_scalar const b = lab->b;

    alwan_scalar const fy = (L + ALWAN_LITERAL(16.0)) / ALWAN_LITERAL(116.0);
    alwan_scalar const fx = a / ALWAN_LITERAL(500.0) + fy;
    alwan_scalar const fz = fy - b / ALWAN_LITERAL(200.0);

    xyz->x = white_xyz->x * lab_f_inv(fx);  /* X */
    xyz->y = white_xyz->y * lab_f_inv(fy);  /* Y */
    xyz->z = white_xyz->z * lab_f_inv(fz);  /* Z */
}

/* ================================================================
 * XYZ <-> Luv Conversions
 * ================================================================ */

/* Calculate u', v' chromaticity coordinates */
static inline void xyz_to_uv_prime(alwan_scalar X, alwan_scalar Y, alwan_scalar Z, alwan_scalar *u_prime, alwan_scalar *v_prime) {
    alwan_scalar const denom = X + ALWAN_LITERAL(15.0) * Y + ALWAN_LITERAL(3.0) * Z;

    if (denom < ALWAN_EPSILON) {
        *u_prime = ALWAN_LITERAL(0.0);
        *v_prime = ALWAN_LITERAL(0.0);
    } else {
        *u_prime = (ALWAN_LITERAL(4.0) * X) / denom;
        *v_prime = (ALWAN_LITERAL(9.0) * Y) / denom;
    }
}

void alwan_xyz_to_luv(alwan_xyz const *xyz, alwan_xyz const *white_xyz, alwan_luv *luv) {
    alwan_scalar const yr = xyz->y / white_xyz->y;

    /* Calculate L* (same formula as Lab) */
    alwan_scalar const fy = lab_f(yr);
    alwan_scalar const L = ALWAN_LITERAL(116.0) * fy - ALWAN_LITERAL(16.0);

    /* Calculate u', v' for sample and white point */
    alwan_scalar u_prime, v_prime;
    alwan_scalar un_prime, vn_prime;
    xyz_to_uv_prime(xyz->x, xyz->y, xyz->z, &u_prime, &v_prime);
    xyz_to_uv_prime(white_xyz->x, white_xyz->y, white_xyz->z, &un_prime, &vn_prime);

    luv->L = L;  /* L* */
    luv->u = ALWAN_LITERAL(13.0) * L * (u_prime - un_prime);  /* u* */
    luv->v = ALWAN_LITERAL(13.0) * L * (v_prime - vn_prime);  /* v* */
}

void alwan_luv_to_xyz(alwan_luv const *luv, alwan_xyz const *white_xyz, alwan_xyz *xyz) {
    alwan_scalar const L = luv->L;
    alwan_scalar const u = luv->u;
    alwan_scalar const v = luv->v;

    /* Calculate white point u', v' */
    alwan_scalar un_prime, vn_prime;
    xyz_to_uv_prime(white_xyz->x, white_xyz->y, white_xyz->z, &un_prime, &vn_prime);

    /* Recover Y from L* */
    alwan_scalar const fy = (L + ALWAN_LITERAL(16.0)) / ALWAN_LITERAL(116.0);
    alwan_scalar const Y = white_xyz->y * lab_f_inv(fy);

    /* Recover u', v' */
    alwan_scalar u_prime, v_prime;
    if (ALWAN_FABS(L) < ALWAN_EPSILON) {
        u_prime = un_prime;
        v_prime = vn_prime;
    } else {
        u_prime = u / (ALWAN_LITERAL(13.0) * L) + un_prime;
        v_prime = v / (ALWAN_LITERAL(13.0) * L) + vn_prime;
    }

    /* Recover X, Z from u', v', Y */
    if (ALWAN_FABS(v_prime) < ALWAN_EPSILON) {
        xyz->x = ALWAN_LITERAL(0.0);
        xyz->y = ALWAN_LITERAL(0.0);
        xyz->z = ALWAN_LITERAL(0.0);
    } else {
        xyz->x = Y * (ALWAN_LITERAL(9.0) * u_prime) / (ALWAN_LITERAL(4.0) * v_prime);  /* X */
        xyz->y = Y;  /* Y */
        xyz->z = Y * (ALWAN_LITERAL(12.0) - ALWAN_LITERAL(3.0) * u_prime - ALWAN_LITERAL(20.0) * v_prime) / (ALWAN_LITERAL(4.0) * v_prime);  /* Z */
    }
}

/* ================================================================
 * XYZ <-> U*V*W* Conversions (CIE 1964 for CRI)
 * ================================================================ */

/* XYZ to U*V*W* (CIE 1964 uniform color space)
 * Based on CIE 1960 UCS chromaticity coordinates
 * U* = 13*W*(u - un), V* = 13*W*(v - vn), W* = 25*Y^(1/3) - 17 */
void alwan_xyz_to_uvw(alwan_vec3 const *xyz, alwan_vec3 const *white_xyz, alwan_vec3 *uvw) {
    alwan_scalar const X = xyz->v[0];
    alwan_scalar const Y = xyz->v[1];
    alwan_scalar const Z = xyz->v[2];

    /* Calculate CIE 1960 UCS chromaticity coordinates (u, v) */
    alwan_scalar const sum = X + ALWAN_LITERAL(15.0) * Y + ALWAN_LITERAL(3.0) * Z;
    alwan_scalar u, v;
    if (ALWAN_FABS(sum) < ALWAN_EPSILON) {
        u = ALWAN_LITERAL(0.0);
        v = ALWAN_LITERAL(0.0);
    } else {
        u = (ALWAN_LITERAL(4.0) * X) / sum;
        v = (ALWAN_LITERAL(6.0) * Y) / sum;
    }

    /* Calculate white point chromaticity */
    alwan_scalar const Xn = white_xyz->v[0];
    alwan_scalar const Yn = white_xyz->v[1];
    alwan_scalar const Zn = white_xyz->v[2];
    alwan_scalar const sum_n = Xn + ALWAN_LITERAL(15.0) * Yn + ALWAN_LITERAL(3.0) * Zn;
    alwan_scalar un, vn;
    if (ALWAN_FABS(sum_n) < ALWAN_EPSILON) {
        un = ALWAN_LITERAL(0.0);
        vn = ALWAN_LITERAL(0.0);
    } else {
        un = (ALWAN_LITERAL(4.0) * Xn) / sum_n;
        vn = (ALWAN_LITERAL(6.0) * Yn) / sum_n;
    }

    /* Calculate W* (1964 lightness)
     * W* = 25 * (Y/Yn)^(1/3) - 17
     * where Y/Yn is the relative luminance */
    alwan_scalar W;
    if (Yn < ALWAN_EPSILON || Y < ALWAN_EPSILON) {
        W = ALWAN_LITERAL(-17.0);
    } else {
        alwan_scalar Y_ratio = Y / Yn;
        W = ALWAN_LITERAL(25.0) * ALWAN_CBRT(Y_ratio) - 17;
    }

    /* Calculate U* and V* */
    uvw->v[0] = ALWAN_LITERAL(13.0) * W * (u - un);   /* U* */
    uvw->v[1] = ALWAN_LITERAL(13.0) * W * (v - vn);   /* V* */
    uvw->v[2] = W;                                      /* W* */
}

/* U*V*W* to XYZ (inverse transform) */
void alwan_uvw_to_xyz(alwan_vec3 const *uvw, alwan_vec3 const *white_xyz, alwan_vec3 *xyz) {
    alwan_scalar const U_star = uvw->v[0];
    alwan_scalar const V_star = uvw->v[1];
    alwan_scalar const W = uvw->v[2];

    /* Calculate Y from W* */
    alwan_scalar Y;
    alwan_scalar const W_plus_17 = W + ALWAN_LITERAL(17.0);
    if (W_plus_17 < ALWAN_EPSILON) {
        Y = ALWAN_LITERAL(0.0);
    } else {
        alwan_scalar const Y_cbrt = W_plus_17 / ALWAN_LITERAL(25.0);
        Y = Y_cbrt * Y_cbrt * Y_cbrt;
    }

    /* Calculate white point chromaticity */
    alwan_scalar const Xn = white_xyz->v[0];
    alwan_scalar const Yn = white_xyz->v[1];
    alwan_scalar const Zn = white_xyz->v[2];
    alwan_scalar const sum_n = Xn + ALWAN_LITERAL(15.0) * Yn + ALWAN_LITERAL(3.0) * Zn;
    alwan_scalar un, vn;
    if (ALWAN_FABS(sum_n) < ALWAN_EPSILON) {
        un = ALWAN_LITERAL(0.0);
        vn = ALWAN_LITERAL(0.0);
    } else {
        un = (ALWAN_LITERAL(4.0) * Xn) / sum_n;
        vn = (ALWAN_LITERAL(6.0) * Yn) / sum_n;
    }

    /* Recover u, v from U*, V*, W* */
    alwan_scalar u, v;
    if (ALWAN_FABS(W) < ALWAN_EPSILON) {
        u = un;
        v = vn;
    } else {
        u = U_star / (ALWAN_LITERAL(13.0) * W) + un;
        v = V_star / (ALWAN_LITERAL(13.0) * W) + vn;
    }

    /* Calculate X and Z from u, v, Y */
    if (ALWAN_FABS(v) < ALWAN_EPSILON) {
        xyz->v[0] = ALWAN_LITERAL(0.0);
        xyz->v[1] = Y;
        xyz->v[2] = ALWAN_LITERAL(0.0);
    } else {
        xyz->v[0] = (ALWAN_LITERAL(9.0) * u * Y) / (ALWAN_LITERAL(4.0) * v);  /* X */
        xyz->v[1] = Y;                                                          /* Y */
        xyz->v[2] = ((ALWAN_LITERAL(12.0) - ALWAN_LITERAL(3.0) * u - ALWAN_LITERAL(20.0) * v) * Y) / (ALWAN_LITERAL(4.0) * v);  /* Z */
    }
}

/* ================================================================
 * Lab <-> LCh(ab) Conversions
 * ================================================================ */

void alwan_lab_to_lch(alwan_lab const *lab, alwan_lch *lch) {
    alwan_scalar const L = lab->L;
    alwan_scalar const a = lab->a;
    alwan_scalar const b = lab->b;

    lch->L = L;  /* L* */
    lch->C = ALWAN_SQRT(a * a + b * b);  /* C*ab */
    alwan_scalar h_rad = ALWAN_ATAN2(b, a);
    alwan_scalar h_deg = h_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;
    /* Normalize hue to [0, 360) range */
    if (h_deg < ALWAN_LITERAL(0.0)) {
        h_deg += ALWAN_LITERAL(360.0);
    }
    lch->h = h_deg;  /* hab in degrees */
}

void alwan_lch_to_lab(alwan_lch const *lch, alwan_lab *lab) {
    alwan_scalar const L = lch->L;
    alwan_scalar const C = lch->C;
    alwan_scalar const h_deg = lch->h;
    alwan_scalar const h_rad = h_deg * ALWAN_PI / ALWAN_LITERAL(180.0);

    lab->L = L;  /* L* */
    lab->a = C * ALWAN_COS(h_rad);  /* a* */
    lab->b = C * ALWAN_SIN(h_rad);  /* b* */
}

/* ================================================================
 * Luv <-> LCh(uv) Conversions
 * ================================================================ */

void alwan_luv_to_lchuv(alwan_luv const *luv, alwan_lchuv *lchuv) {
    alwan_scalar const L = luv->L;
    alwan_scalar const u = luv->u;
    alwan_scalar const v = luv->v;

    lchuv->L = L;  /* L* */
    lchuv->C = ALWAN_SQRT(u * u + v * v);  /* C*uv */
    alwan_scalar h_rad = ALWAN_ATAN2(v, u);
    alwan_scalar h_deg = h_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;
    /* Normalize hue to [0, 360) range */
    if (h_deg < ALWAN_LITERAL(0.0)) {
        h_deg += ALWAN_LITERAL(360.0);
    }
    lchuv->h = h_deg;  /* huv in degrees */
}

void alwan_lchuv_to_luv(alwan_lchuv const *lchuv, alwan_luv *luv) {
    alwan_scalar const L = lchuv->L;
    alwan_scalar const C = lchuv->C;
    alwan_scalar const h_deg = lchuv->h;
    alwan_scalar const h_rad = h_deg * ALWAN_PI / ALWAN_LITERAL(180.0);

    luv->L = L;  /* L* */
    luv->u = C * ALWAN_COS(h_rad);  /* u* */
    luv->v = C * ALWAN_SIN(h_rad);  /* v* */
}

/* ================================================================
 * XYZ <-> CIE 1960 UCS Conversions
 * ================================================================ */

void alwan_xyz_to_ucs(alwan_vec3 const *xyz, alwan_vec3 *ucs) {
    alwan_scalar const X = xyz->v[0];
    alwan_scalar const Y = xyz->v[1];
    alwan_scalar const Z = xyz->v[2];

    /* CIE 1960 UCS color space (UVW tristimulus values)
     * U = (2/3) * X
     * V = Y
     * W = (1/2) * (-X + 3Y + Z)
     * Reference: colour.XYZ_to_UCS
     */
    ucs->v[0] = (ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0)) * X;  /* U */
    ucs->v[1] = Y;                                               /* V */
    ucs->v[2] = (ALWAN_LITERAL(0.5)) * (-X + ALWAN_LITERAL(3.0) * Y + Z);  /* W */
}

void alwan_ucs_to_xyz(alwan_vec3 const *ucs, alwan_vec3 *xyz) {
    alwan_scalar const U = ucs->v[0];
    alwan_scalar const V = ucs->v[1];
    alwan_scalar const W = ucs->v[2];

    /* Inverse CIE 1960 UCS transform
     * From: U = (2/3) * X, V = Y, W = (1/2) * (-X + 3Y + Z)
     * Solving:
     *   X = (3/2) * U
     *   Y = V
     *   Z = 2W + X - 3Y = 2W + (3/2)U - 3V
     * Reference: colour.UCS_to_XYZ
     */
    xyz->v[0] = (ALWAN_LITERAL(3.0) / ALWAN_LITERAL(2.0)) * U;  /* X */
    xyz->v[1] = V;                                               /* Y */
    xyz->v[2] = ALWAN_LITERAL(2.0) * W + xyz->v[0] - ALWAN_LITERAL(3.0) * V;  /* Z */
}

/* ================================================================
 * Color Difference (ΔE) Metrics
 * ================================================================ */

alwan_scalar alwan_delta_e_76(alwan_lab const *lab1, alwan_lab const *lab2) {
    alwan_scalar const dL = lab1->L - lab2->L;
    alwan_scalar const da = lab1->a - lab2->a;
    alwan_scalar const db = lab1->b - lab2->b;

    return ALWAN_SQRT(dL * dL + da * da + db * db);
}

alwan_scalar alwan_delta_e_94(alwan_lab const *lab1, alwan_lab const *lab2) {
    /* Graphic arts defaults: kL = 1, K1 = 0.045, K2 = 0.015 */
    alwan_scalar const kL = ALWAN_LITERAL(1.0);
    alwan_scalar const K1 = ALWAN_LITERAL(0.045);
    alwan_scalar const K2 = ALWAN_LITERAL(0.015);
    alwan_scalar const kC = ALWAN_LITERAL(1.0);
    alwan_scalar const kH = ALWAN_LITERAL(1.0);

    alwan_scalar const L1 = lab1->L;
    alwan_scalar const a1 = lab1->a;
    alwan_scalar const b1 = lab1->b;
    alwan_scalar const L2 = lab2->L;
    alwan_scalar const a2 = lab2->a;
    alwan_scalar const b2 = lab2->b;

    alwan_scalar const dL = L1 - L2;
    alwan_scalar const C1 = ALWAN_SQRT(a1 * a1 + b1 * b1);
    alwan_scalar const C2 = ALWAN_SQRT(a2 * a2 + b2 * b2);
    alwan_scalar const dCab = C1 - C2;

    alwan_scalar const da = a1 - a2;
    alwan_scalar const db = b1 - b2;
    alwan_scalar const dHab_sq = da * da + db * db - dCab * dCab;

    alwan_scalar const SL = ALWAN_LITERAL(1.0);
    alwan_scalar const SC = ALWAN_LITERAL(1.0) + K1 * C1;
    alwan_scalar const SH = ALWAN_LITERAL(1.0) + K2 * C1;

    alwan_scalar const term1 = (dL / (kL * SL));
    alwan_scalar const term2 = (dCab / (kC * SC));
    alwan_scalar const term3 = (dHab_sq > ALWAN_LITERAL(0.0)) ? (ALWAN_SQRT(dHab_sq) / (kH * SH)) : ALWAN_LITERAL(0.0);

    return ALWAN_SQRT(term1 * term1 + term2 * term2 + term3 * term3);
}

alwan_scalar alwan_delta_e_cmc(alwan_lab const *lab1, alwan_lab const *lab2, alwan_scalar l, alwan_scalar c) {
    alwan_scalar const L1 = lab1->L;
    alwan_scalar const a1 = lab1->a;
    alwan_scalar const b1 = lab1->b;
    alwan_scalar const L2 = lab2->L;
    alwan_scalar const a2 = lab2->a;
    alwan_scalar const b2 = lab2->b;

    alwan_scalar const dL = L1 - L2;
    alwan_scalar const C1 = ALWAN_SQRT(a1 * a1 + b1 * b1);
    alwan_scalar const C2 = ALWAN_SQRT(a2 * a2 + b2 * b2);
    alwan_scalar const dCab = C1 - C2;

    alwan_scalar const da = a1 - a2;
    alwan_scalar const db = b1 - b2;
    alwan_scalar const dHab_sq = da * da + db * db - dCab * dCab;

    /* Calculate hue angle for reference color */
    alwan_scalar h1 = ALWAN_ATAN2(b1, a1);
    if (h1 < ALWAN_LITERAL(0.0)) {
        h1 += ALWAN_LITERAL(2.0) * ALWAN_PI;
    }
    h1 = h1 * ALWAN_LITERAL(180.0) / ALWAN_PI;  /* Convert to degrees */

    /* SL weighting factor */
    alwan_scalar SL;
    if (L1 < ALWAN_LITERAL(16.0)) {
        SL = ALWAN_LITERAL(0.511);
    } else {
        SL = (ALWAN_LITERAL(0.040975) * L1) / (ALWAN_LITERAL(1.0) + ALWAN_LITERAL(0.01765) * L1);
    }

    /* SC weighting factor */
    alwan_scalar const SC = (ALWAN_LITERAL(0.0638) * C1) / (ALWAN_LITERAL(1.0) + ALWAN_LITERAL(0.0131) * C1) + ALWAN_LITERAL(0.638);

    /* F factor for SH */
    alwan_scalar const C1_4 = C1 * C1 * C1 * C1;
    alwan_scalar const F = ALWAN_SQRT(C1_4 / (C1_4 + ALWAN_LITERAL(1900.0)));

    /* T factor for SH */
    alwan_scalar T;
    if (h1 >= ALWAN_LITERAL(164.0) && h1 <= ALWAN_LITERAL(345.0)) {
        T = ALWAN_LITERAL(0.56) + ALWAN_FABS(ALWAN_LITERAL(0.2) * ALWAN_COS((h1 + ALWAN_LITERAL(168.0)) * ALWAN_PI / ALWAN_LITERAL(180.0)));
    } else {
        T = ALWAN_LITERAL(0.36) + ALWAN_FABS(ALWAN_LITERAL(0.4) * ALWAN_COS((h1 + ALWAN_LITERAL(35.0)) * ALWAN_PI / ALWAN_LITERAL(180.0)));
    }

    /* SH weighting factor */
    alwan_scalar const SH = SC * (F * T + ALWAN_LITERAL(1.0) - F);

    alwan_scalar const term1 = dL / (l * SL);
    alwan_scalar const term2 = dCab / (c * SC);
    alwan_scalar const term3 = (dHab_sq > ALWAN_LITERAL(0.0)) ? (ALWAN_SQRT(dHab_sq) / SH) : ALWAN_LITERAL(0.0);

    return ALWAN_SQRT(term1 * term1 + term2 * term2 + term3 * term3);
}

alwan_scalar alwan_delta_e_2000(alwan_lab const *lab1, alwan_lab const *lab2) {
    alwan_scalar const L1 = lab1->L;
    alwan_scalar const a1 = lab1->a;
    alwan_scalar const b1 = lab1->b;
    alwan_scalar const L2 = lab2->L;
    alwan_scalar const a2 = lab2->a;
    alwan_scalar const b2 = lab2->b;

    /* Calculate Cab */
    alwan_scalar const C1 = ALWAN_SQRT(a1 * a1 + b1 * b1);
    alwan_scalar const C2 = ALWAN_SQRT(a2 * a2 + b2 * b2);
    alwan_scalar const Cab_mean = (C1 + C2) / ALWAN_LITERAL(2.0);

    /* Calculate G (a' adjustment factor) */
    alwan_scalar const Cab_mean_7 = Cab_mean * Cab_mean * Cab_mean * Cab_mean * Cab_mean * Cab_mean * Cab_mean;
    alwan_scalar const G = ALWAN_LITERAL(0.5) * (ALWAN_LITERAL(1.0) - ALWAN_SQRT(Cab_mean_7 / (Cab_mean_7 + ALWAN_LITERAL(6103515625.0))));  /* 25^7 = 6103515625 */

    /* Calculate a' */
    alwan_scalar const a1_prime = (ALWAN_LITERAL(1.0) + G) * a1;
    alwan_scalar const a2_prime = (ALWAN_LITERAL(1.0) + G) * a2;

    /* Calculate C' and h' */
    alwan_scalar const C1_prime = ALWAN_SQRT(a1_prime * a1_prime + b1 * b1);
    alwan_scalar const C2_prime = ALWAN_SQRT(a2_prime * a2_prime + b2 * b2);

    alwan_scalar h1_prime = ALWAN_ATAN2(b1, a1_prime);
    if (h1_prime < ALWAN_LITERAL(0.0)) h1_prime += ALWAN_LITERAL(2.0) * ALWAN_PI;
    h1_prime = h1_prime * ALWAN_LITERAL(180.0) / ALWAN_PI;  /* Convert to degrees */

    alwan_scalar h2_prime = ALWAN_ATAN2(b2, a2_prime);
    if (h2_prime < ALWAN_LITERAL(0.0)) h2_prime += ALWAN_LITERAL(2.0) * ALWAN_PI;
    h2_prime = h2_prime * ALWAN_LITERAL(180.0) / ALWAN_PI;  /* Convert to degrees */

    /* Calculate ΔL', ΔC', ΔH' */
    alwan_scalar const dL_prime = L2 - L1;
    alwan_scalar const dC_prime = C2_prime - C1_prime;

    alwan_scalar dh_prime;
    if (C1_prime * C2_prime < ALWAN_EPSILON) {
        dh_prime = ALWAN_LITERAL(0.0);
    } else if (ALWAN_FABS(h2_prime - h1_prime) <= ALWAN_LITERAL(180.0)) {
        dh_prime = h2_prime - h1_prime;
    } else if (h2_prime - h1_prime > ALWAN_LITERAL(180.0)) {
        dh_prime = h2_prime - h1_prime - ALWAN_LITERAL(360.0);
    } else {
        dh_prime = h2_prime - h1_prime + ALWAN_LITERAL(360.0);
    }

    alwan_scalar const dH_prime = ALWAN_LITERAL(2.0) * ALWAN_SQRT(C1_prime * C2_prime) * ALWAN_SIN(dh_prime * ALWAN_PI / ALWAN_LITERAL(360.0));

    /* Calculate mean values */
    alwan_scalar const L_prime_mean = (L1 + L2) / ALWAN_LITERAL(2.0);
    alwan_scalar const C_prime_mean = (C1_prime + C2_prime) / ALWAN_LITERAL(2.0);

    alwan_scalar H_prime_mean;
    if (C1_prime * C2_prime < ALWAN_EPSILON) {
        H_prime_mean = h1_prime + h2_prime;
    } else if (ALWAN_FABS(h1_prime - h2_prime) <= ALWAN_LITERAL(180.0)) {
        H_prime_mean = (h1_prime + h2_prime) / ALWAN_LITERAL(2.0);
    } else if (h1_prime + h2_prime < ALWAN_LITERAL(360.0)) {
        H_prime_mean = (h1_prime + h2_prime + ALWAN_LITERAL(360.0)) / ALWAN_LITERAL(2.0);
    } else {
        H_prime_mean = (h1_prime + h2_prime - ALWAN_LITERAL(360.0)) / ALWAN_LITERAL(2.0);
    }

    /* Calculate weighting functions */
    alwan_scalar const T = ALWAN_LITERAL(1.0)
        - ALWAN_LITERAL(0.17) * ALWAN_COS((H_prime_mean - ALWAN_LITERAL(30.0)) * ALWAN_PI / ALWAN_LITERAL(180.0))
        + ALWAN_LITERAL(0.24) * ALWAN_COS((ALWAN_LITERAL(2.0) * H_prime_mean) * ALWAN_PI / ALWAN_LITERAL(180.0))
        + ALWAN_LITERAL(0.32) * ALWAN_COS((ALWAN_LITERAL(3.0) * H_prime_mean + ALWAN_LITERAL(6.0)) * ALWAN_PI / ALWAN_LITERAL(180.0))
        - ALWAN_LITERAL(0.20) * ALWAN_COS((ALWAN_LITERAL(4.0) * H_prime_mean - ALWAN_LITERAL(63.0)) * ALWAN_PI / ALWAN_LITERAL(180.0));

    alwan_scalar const dTheta = ALWAN_LITERAL(30.0) * ALWAN_EXP(-((H_prime_mean - ALWAN_LITERAL(275.0)) / ALWAN_LITERAL(25.0)) * ((H_prime_mean - ALWAN_LITERAL(275.0)) / ALWAN_LITERAL(25.0)));

    alwan_scalar const C_prime_mean_7 = C_prime_mean * C_prime_mean * C_prime_mean * C_prime_mean * C_prime_mean * C_prime_mean * C_prime_mean;
    alwan_scalar const RC = ALWAN_LITERAL(2.0) * ALWAN_SQRT(C_prime_mean_7 / (C_prime_mean_7 + ALWAN_LITERAL(6103515625.0)));

    alwan_scalar const L_prime_mean_minus_50_sq = (L_prime_mean - ALWAN_LITERAL(50.0)) * (L_prime_mean - ALWAN_LITERAL(50.0));
    alwan_scalar const SL = ALWAN_LITERAL(1.0) + (ALWAN_LITERAL(0.015) * L_prime_mean_minus_50_sq) / ALWAN_SQRT(ALWAN_LITERAL(20.0) + L_prime_mean_minus_50_sq);
    alwan_scalar const SC = ALWAN_LITERAL(1.0) + ALWAN_LITERAL(0.045) * C_prime_mean;
    alwan_scalar const SH = ALWAN_LITERAL(1.0) + ALWAN_LITERAL(0.015) * C_prime_mean * T;
    alwan_scalar const RT = -ALWAN_SIN((ALWAN_LITERAL(2.0) * dTheta) * ALWAN_PI / ALWAN_LITERAL(180.0)) * RC;

    /* Calculate ΔE00 with kL = kC = kH = 1 */
    alwan_scalar const term1 = dL_prime / SL;
    alwan_scalar const term2 = dC_prime / SC;
    alwan_scalar const term3 = dH_prime / SH;

    return ALWAN_SQRT(term1 * term1 + term2 * term2 + term3 * term3 + RT * term2 * term3);
}

/* ================================================================
 * Additional Color Difference Metrics
 * ================================================================ */

/* ΔE ITP - ITU-R BT.2124 HDR Color Difference in ICtCp space
 * Reference: ITU-R Report BT.2124
 * Formula: ΔE_ITP = K_ITP * sqrt(dI² + 0.25*dCT² + dCP²)
 * where K_ITP (scalar_factor) is typically 720 */
alwan_scalar alwan_delta_e_itp(alwan_ictcp const *ictcp1, alwan_ictcp const *ictcp2, alwan_scalar scalar_factor) {
    alwan_scalar const dI  = ictcp1->I - ictcp2->I;   /* Intensity difference */
    alwan_scalar const dCT = ictcp1->Ct - ictcp2->Ct;   /* Tritan (blue-yellow) difference */
    alwan_scalar const dCP = ictcp1->Cp - ictcp2->Cp;   /* Protan (red-green) difference */

    /* ITU-R BT.2124 formula with tritan weight of 0.25 */
    return scalar_factor * ALWAN_SQRT(dI * dI + ALWAN_LITERAL(0.25) * dCT * dCT + dCP * dCP);
}

/* ΔE HyAB - Hybrid Delta E
 * Reference: Sarifuddin, M., & Missaoui, R. (2005)
 * "A new perceptually uniform color space with associated color similarity measure"
 * Simplified Euclidean in Lab with adjusted chroma weighting */
alwan_scalar alwan_delta_e_hyab(alwan_lab const *lab1, alwan_lab const *lab2) {
    alwan_scalar const L1 = lab1->L;
    alwan_scalar const a1 = lab1->a;
    alwan_scalar const b1 = lab1->b;
    alwan_scalar const L2 = lab2->L;
    alwan_scalar const a2 = lab2->a;
    alwan_scalar const b2 = lab2->b;

    alwan_scalar const dL = L1 - L2;
    alwan_scalar const db = b1 - b2;

    /* Compute chroma values */
    alwan_scalar const C1 = ALWAN_SQRT(a1 * a1 + b1 * b1);
    alwan_scalar const C2 = ALWAN_SQRT(a2 * a2 + b2 * b2);

    /* Hybrid metric combines Euclidean with chroma adjustment */
    alwan_scalar const Cab = (C1 + C2) / ALWAN_LITERAL(2.0);
    alwan_scalar const G = ALWAN_LITERAL(0.5) * (ALWAN_LITERAL(1.0) - ALWAN_SQRT(
        (Cab * Cab * Cab * Cab * Cab * Cab * Cab) /
        ((Cab * Cab * Cab * Cab * Cab * Cab * Cab) + ALWAN_LITERAL(6103515625.0))  /* 25^7 */
    ));

    alwan_scalar const a1_prime = (ALWAN_LITERAL(1.0) + G) * a1;
    alwan_scalar const a2_prime = (ALWAN_LITERAL(1.0) + G) * a2;

    alwan_scalar const C1_prime = ALWAN_SQRT(a1_prime * a1_prime + b1 * b1);
    alwan_scalar const C2_prime = ALWAN_SQRT(a2_prime * a2_prime + b2 * b2);
    alwan_scalar const dC_prime = C1_prime - C2_prime;

    alwan_scalar const da_prime = a1_prime - a2_prime;
    alwan_scalar const dH_prime_sq = da_prime * da_prime + db * db - dC_prime * dC_prime;

    return ALWAN_SQRT(dL * dL + dC_prime * dC_prime + (dH_prime_sq > ALWAN_LITERAL(0.0) ? dH_prime_sq : ALWAN_LITERAL(0.0)));
}

/* ΔE DIN99 - Euclidean distance in DIN99 space
 * DIN99 family spaces are designed for perceptual uniformity
 * Simple Euclidean distance in the transformed space */
alwan_scalar alwan_delta_e_din99(alwan_vec3 const *din99_1, alwan_vec3 const *din99_2) {
    alwan_scalar const dL = din99_1->v[0] - din99_2->v[0];
    alwan_scalar const da = din99_1->v[1] - din99_2->v[1];
    alwan_scalar const db = din99_1->v[2] - din99_2->v[2];

    return ALWAN_SQRT(dL * dL + da * da + db * db);
}

/* ΔE CAM02-LCD - CIECAM02 Large Color Difference
 * Reference: CIE TC8-01 "Uniform Colour Spaces"
 * Formula: sqrt((dJ/K_L)² + (dM)² + (dh)²) in UCS space
 * LCD uses K_L = 1.0, c1 = 0.007, c2 = 0.0053
 * Takes Lab input and converts internally via Lab → XYZ → CIECAM02 → UCS */
alwan_scalar alwan_delta_e_cam02_lcd(alwan_vec3 const *lab1, alwan_vec3 const *lab2) {
    /* IEC 61966-2-1:1999 (sRGB) viewing conditions for delta E calculations */
    alwan_ciecam02_viewing_conditions vc;
    vc.white_xyz.x = ALWAN_LITERAL(95.045592705167159);   /* D65 × 100 */
    vc.white_xyz.y = ALWAN_LITERAL(100.0);
    vc.white_xyz.z = ALWAN_LITERAL(108.90577507598784);
    vc.adapting_luminance = ALWAN_LITERAL(64.0) / ALWAN_PI * ALWAN_LITERAL(0.2);  /* ~4.074 cd/m² */
    vc.background_luminance = ALWAN_LITERAL(20.0);
    vc.surround = ALWAN_CIECAM02_SURROUND_AVERAGE;
    vc.discount_illuminant = 0;

    /* Convert Lab to XYZ (using normalized D65 white point for Lab) */
    alwan_xyz white_lab;
    white_lab.x = ALWAN_LITERAL(0.95045592705167159);
    white_lab.y = ALWAN_LITERAL(1.0);
    white_lab.z = ALWAN_LITERAL(1.0890577507598784);

    alwan_xyz xyz1, xyz2;
    alwan_lab_to_xyz((alwan_lab const *)lab1, &white_lab, &xyz1);
    alwan_lab_to_xyz((alwan_lab const *)lab2, &white_lab, &xyz2);

    /* Scale XYZ to Y=100 range */
    xyz1.x *= ALWAN_LITERAL(100.0);
    xyz1.y *= ALWAN_LITERAL(100.0);
    xyz1.z *= ALWAN_LITERAL(100.0);
    xyz2.x *= ALWAN_LITERAL(100.0);
    xyz2.y *= ALWAN_LITERAL(100.0);
    xyz2.z *= ALWAN_LITERAL(100.0);

    /* Convert XYZ to CIECAM02 */
    alwan_ciecam02_correlates cam1, cam2;
    alwan_ciecam02_forward(&xyz1, &vc, &cam1);
    alwan_ciecam02_forward(&xyz2, &vc, &cam2);

    /* Convert to CAM02-LCD UCS coordinates (Jab) */
    /* CAM02-LCD uses c1=0.007, c2=0.0053, K_L=0.77 */
    alwan_scalar const c1 = ALWAN_LITERAL(0.007);
    alwan_scalar const c2 = ALWAN_LITERAL(0.0053);
    alwan_scalar const KL = ALWAN_LITERAL(0.77);

    alwan_scalar const J1_prime = (ALWAN_LITERAL(1.0) + ALWAN_LITERAL(100.0) * c1) * cam1.J / (ALWAN_LITERAL(1.0) + c1 * cam1.J);
    alwan_scalar const M1_prime = (ALWAN_LITERAL(1.0) / c2) * ALWAN_LOG(ALWAN_LITERAL(1.0) + c2 * cam1.M);
    alwan_scalar const h1_rad = cam1.h * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar const a1_prime = M1_prime * ALWAN_COS(h1_rad);
    alwan_scalar const b1_prime = M1_prime * ALWAN_SIN(h1_rad);

    alwan_scalar const J2_prime = (ALWAN_LITERAL(1.0) + ALWAN_LITERAL(100.0) * c1) * cam2.J / (ALWAN_LITERAL(1.0) + c1 * cam2.J);
    alwan_scalar const M2_prime = (ALWAN_LITERAL(1.0) / c2) * ALWAN_LOG(ALWAN_LITERAL(1.0) + c2 * cam2.M);
    alwan_scalar const h2_rad = cam2.h * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar const a2_prime = M2_prime * ALWAN_COS(h2_rad);
    alwan_scalar const b2_prime = M2_prime * ALWAN_SIN(h2_rad);

    /* Calculate delta E in UCS space - simple Euclidean distance */
    alwan_scalar const dJ = (J1_prime - J2_prime) / KL;
    alwan_scalar const da = a1_prime - a2_prime;
    alwan_scalar const db = b1_prime - b2_prime;

    return ALWAN_SQRT(dJ * dJ + da * da + db * db);
}

/* ΔE CAM02-SCD - CIECAM02 Small Color Difference
 * SCD uses tighter parameters for better discrimination of small differences
 * SCD uses K_L = 2.0, c1 = 0.007, c2 = 0.0363
 * Takes Lab input and converts internally via Lab → XYZ → CIECAM02 → UCS */
alwan_scalar alwan_delta_e_cam02_scd(alwan_vec3 const *lab1, alwan_vec3 const *lab2) {
    /* IEC 61966-2-1:1999 (sRGB) viewing conditions for delta E calculations */
    alwan_ciecam02_viewing_conditions vc;
    vc.white_xyz.x = ALWAN_LITERAL(95.045592705167159);   /* D65 × 100 */
    vc.white_xyz.y = ALWAN_LITERAL(100.0);
    vc.white_xyz.z = ALWAN_LITERAL(108.90577507598784);
    vc.adapting_luminance = ALWAN_LITERAL(64.0) / ALWAN_PI * ALWAN_LITERAL(0.2);  /* ~4.074 cd/m² */
    vc.background_luminance = ALWAN_LITERAL(20.0);
    vc.surround = ALWAN_CIECAM02_SURROUND_AVERAGE;
    vc.discount_illuminant = 0;

    /* Convert Lab to XYZ (using normalized D65 white point for Lab) */
    alwan_xyz white_lab;
    white_lab.x = ALWAN_LITERAL(0.95045592705167159);
    white_lab.y = ALWAN_LITERAL(1.0);
    white_lab.z = ALWAN_LITERAL(1.0890577507598784);

    alwan_xyz xyz1, xyz2;
    alwan_lab_to_xyz((alwan_lab const *)lab1, &white_lab, &xyz1);
    alwan_lab_to_xyz((alwan_lab const *)lab2, &white_lab, &xyz2);

    /* Scale XYZ to Y=100 range */
    xyz1.x *= ALWAN_LITERAL(100.0);
    xyz1.y *= ALWAN_LITERAL(100.0);
    xyz1.z *= ALWAN_LITERAL(100.0);
    xyz2.x *= ALWAN_LITERAL(100.0);
    xyz2.y *= ALWAN_LITERAL(100.0);
    xyz2.z *= ALWAN_LITERAL(100.0);

    /* Convert XYZ to CIECAM02 */
    alwan_ciecam02_correlates cam1, cam2;
    alwan_ciecam02_forward(&xyz1, &vc, &cam1);
    alwan_ciecam02_forward(&xyz2, &vc, &cam2);

    /* Convert to CAM02-SCD UCS coordinates (Jab) */
    /* CAM02-SCD uses c1=0.007, c2=0.0363, K_L=1.24 */
    alwan_scalar const c1 = ALWAN_LITERAL(0.007);
    alwan_scalar const c2 = ALWAN_LITERAL(0.0363);
    alwan_scalar const KL = ALWAN_LITERAL(1.24);

    alwan_scalar const J1_prime = (ALWAN_LITERAL(1.0) + ALWAN_LITERAL(100.0) * c1) * cam1.J / (ALWAN_LITERAL(1.0) + c1 * cam1.J);
    alwan_scalar const M1_prime = (ALWAN_LITERAL(1.0) / c2) * ALWAN_LOG(ALWAN_LITERAL(1.0) + c2 * cam1.M);
    alwan_scalar const h1_rad = cam1.h * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar const a1_prime = M1_prime * ALWAN_COS(h1_rad);
    alwan_scalar const b1_prime = M1_prime * ALWAN_SIN(h1_rad);

    alwan_scalar const J2_prime = (ALWAN_LITERAL(1.0) + ALWAN_LITERAL(100.0) * c1) * cam2.J / (ALWAN_LITERAL(1.0) + c1 * cam2.J);
    alwan_scalar const M2_prime = (ALWAN_LITERAL(1.0) / c2) * ALWAN_LOG(ALWAN_LITERAL(1.0) + c2 * cam2.M);
    alwan_scalar const h2_rad = cam2.h * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar const a2_prime = M2_prime * ALWAN_COS(h2_rad);
    alwan_scalar const b2_prime = M2_prime * ALWAN_SIN(h2_rad);

    /* Calculate delta E in UCS space - simple Euclidean distance */
    alwan_scalar const dJ = (J1_prime - J2_prime) / KL;
    alwan_scalar const da = a1_prime - a2_prime;
    alwan_scalar const db = b1_prime - b2_prime;

    return ALWAN_SQRT(dJ * dJ + da * da + db * db);
}

/* ΔE CAM16-LCD - CAM16 Large Color Difference
 * Same formula as CAM02-LCD but uses CAM16 chromatic adaptation
 * Takes Lab input and converts internally via Lab → XYZ → CAM16 → UCS */
alwan_scalar alwan_delta_e_cam16_lcd(alwan_vec3 const *lab1, alwan_vec3 const *lab2) {
    /* IEC 61966-2-1:1999 (sRGB) viewing conditions for delta E calculations */
    alwan_cam16_viewing_conditions vc;
    vc.white_xyz.x = ALWAN_LITERAL(95.045592705167159);   /* D65 × 100 */
    vc.white_xyz.y = ALWAN_LITERAL(100.0);
    vc.white_xyz.z = ALWAN_LITERAL(108.90577507598784);
    vc.adapting_luminance = ALWAN_LITERAL(64.0) / ALWAN_PI * ALWAN_LITERAL(0.2);  /* ~4.074 cd/m² */
    vc.background_luminance = ALWAN_LITERAL(20.0);
    vc.surround = ALWAN_CAM16_SURROUND_AVERAGE;
    vc.discount_illuminant = 0;

    /* Convert Lab to XYZ (using normalized D65 white point for Lab) */
    alwan_xyz white_lab;
    white_lab.x = ALWAN_LITERAL(0.95045592705167159);
    white_lab.y = ALWAN_LITERAL(1.0);
    white_lab.z = ALWAN_LITERAL(1.0890577507598784);

    alwan_xyz xyz1, xyz2;
    alwan_lab_to_xyz((alwan_lab const *)lab1, &white_lab, &xyz1);
    alwan_lab_to_xyz((alwan_lab const *)lab2, &white_lab, &xyz2);

    /* Scale XYZ to Y=100 range */
    xyz1.x *= ALWAN_LITERAL(100.0);
    xyz1.y *= ALWAN_LITERAL(100.0);
    xyz1.z *= ALWAN_LITERAL(100.0);
    xyz2.x *= ALWAN_LITERAL(100.0);
    xyz2.y *= ALWAN_LITERAL(100.0);
    xyz2.z *= ALWAN_LITERAL(100.0);

    /* Convert XYZ to CAM16 */
    alwan_cam16_correlates cam1, cam2;
    alwan_cam16_forward(&xyz1, &vc, &cam1);
    alwan_cam16_forward(&xyz2, &vc, &cam2);

    /* Convert to CAM16-LCD UCS coordinates (Jab) */
    /* CAM16-LCD uses same constants as CAM02-LCD: c1=0.007, c2=0.0053, K_L=0.77 */
    alwan_scalar const c1 = ALWAN_LITERAL(0.007);
    alwan_scalar const c2 = ALWAN_LITERAL(0.0053);
    alwan_scalar const KL = ALWAN_LITERAL(0.77);

    alwan_scalar const J1_prime = (ALWAN_LITERAL(1.0) + ALWAN_LITERAL(100.0) * c1) * cam1.J / (ALWAN_LITERAL(1.0) + c1 * cam1.J);
    alwan_scalar const M1_prime = (ALWAN_LITERAL(1.0) / c2) * ALWAN_LOG(ALWAN_LITERAL(1.0) + c2 * cam1.M);
    alwan_scalar const h1_rad = cam1.h * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar const a1_prime = M1_prime * ALWAN_COS(h1_rad);
    alwan_scalar const b1_prime = M1_prime * ALWAN_SIN(h1_rad);

    alwan_scalar const J2_prime = (ALWAN_LITERAL(1.0) + ALWAN_LITERAL(100.0) * c1) * cam2.J / (ALWAN_LITERAL(1.0) + c1 * cam2.J);
    alwan_scalar const M2_prime = (ALWAN_LITERAL(1.0) / c2) * ALWAN_LOG(ALWAN_LITERAL(1.0) + c2 * cam2.M);
    alwan_scalar const h2_rad = cam2.h * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar const a2_prime = M2_prime * ALWAN_COS(h2_rad);
    alwan_scalar const b2_prime = M2_prime * ALWAN_SIN(h2_rad);

    /* Calculate delta E in UCS space - simple Euclidean distance */
    alwan_scalar const dJ = (J1_prime - J2_prime) / KL;
    alwan_scalar const da = a1_prime - a2_prime;
    alwan_scalar const db = b1_prime - b2_prime;

    return ALWAN_SQRT(dJ * dJ + da * da + db * db);
}

/* ΔE CAM16-SCD - CAM16 Small Color Difference
 * Same formula as CAM02-SCD but uses CAM16 chromatic adaptation
 * Takes Lab input and converts internally via Lab → XYZ → CAM16 → UCS */
alwan_scalar alwan_delta_e_cam16_scd(alwan_vec3 const *lab1, alwan_vec3 const *lab2) {
    /* IEC 61966-2-1:1999 (sRGB) viewing conditions for delta E calculations */
    alwan_cam16_viewing_conditions vc;
    vc.white_xyz.x = ALWAN_LITERAL(95.045592705167159);   /* D65 × 100 */
    vc.white_xyz.y = ALWAN_LITERAL(100.0);
    vc.white_xyz.z = ALWAN_LITERAL(108.90577507598784);
    vc.adapting_luminance = ALWAN_LITERAL(64.0) / ALWAN_PI * ALWAN_LITERAL(0.2);  /* ~4.074 cd/m² */
    vc.background_luminance = ALWAN_LITERAL(20.0);
    vc.surround = ALWAN_CAM16_SURROUND_AVERAGE;
    vc.discount_illuminant = 0;

    /* Convert Lab to XYZ (using normalized D65 white point for Lab) */
    alwan_xyz white_lab;
    white_lab.x = ALWAN_LITERAL(0.95045592705167159);
    white_lab.y = ALWAN_LITERAL(1.0);
    white_lab.z = ALWAN_LITERAL(1.0890577507598784);

    alwan_xyz xyz1, xyz2;
    alwan_lab_to_xyz((alwan_lab const *)lab1, &white_lab, &xyz1);
    alwan_lab_to_xyz((alwan_lab const *)lab2, &white_lab, &xyz2);

    /* Scale XYZ to Y=100 range */
    xyz1.x *= ALWAN_LITERAL(100.0);
    xyz1.y *= ALWAN_LITERAL(100.0);
    xyz1.z *= ALWAN_LITERAL(100.0);
    xyz2.x *= ALWAN_LITERAL(100.0);
    xyz2.y *= ALWAN_LITERAL(100.0);
    xyz2.z *= ALWAN_LITERAL(100.0);

    /* Convert XYZ to CAM16 */
    alwan_cam16_correlates cam1, cam2;
    alwan_cam16_forward(&xyz1, &vc, &cam1);
    alwan_cam16_forward(&xyz2, &vc, &cam2);

    /* Convert to CAM16-SCD UCS coordinates (Jab) */
    /* CAM16-SCD uses same constants as CAM02-SCD: c1=0.007, c2=0.0363, K_L=1.24 */
    alwan_scalar const c1 = ALWAN_LITERAL(0.007);
    alwan_scalar const c2 = ALWAN_LITERAL(0.0363);
    alwan_scalar const KL = ALWAN_LITERAL(1.24);

    alwan_scalar const J1_prime = (ALWAN_LITERAL(1.0) + ALWAN_LITERAL(100.0) * c1) * cam1.J / (ALWAN_LITERAL(1.0) + c1 * cam1.J);
    alwan_scalar const M1_prime = (ALWAN_LITERAL(1.0) / c2) * ALWAN_LOG(ALWAN_LITERAL(1.0) + c2 * cam1.M);
    alwan_scalar const h1_rad = cam1.h * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar const a1_prime = M1_prime * ALWAN_COS(h1_rad);
    alwan_scalar const b1_prime = M1_prime * ALWAN_SIN(h1_rad);

    alwan_scalar const J2_prime = (ALWAN_LITERAL(1.0) + ALWAN_LITERAL(100.0) * c1) * cam2.J / (ALWAN_LITERAL(1.0) + c1 * cam2.J);
    alwan_scalar const M2_prime = (ALWAN_LITERAL(1.0) / c2) * ALWAN_LOG(ALWAN_LITERAL(1.0) + c2 * cam2.M);
    alwan_scalar const h2_rad = cam2.h * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar const a2_prime = M2_prime * ALWAN_COS(h2_rad);
    alwan_scalar const b2_prime = M2_prime * ALWAN_SIN(h2_rad);

    /* Calculate delta E in UCS space - simple Euclidean distance */
    alwan_scalar const dJ = (J1_prime - J2_prime) / KL;
    alwan_scalar const da = a1_prime - a2_prime;
    alwan_scalar const db = b1_prime - b2_prime;

    return ALWAN_SQRT(dJ * dJ + da * da + db * db);
}

/* ΔE ZCAM - Euclidean distance in ZCAM UCS (Jzazbz) space
 * ZCAM UCS is designed for perceptual uniformity in HDR
 * Simple Euclidean distance is appropriate */
alwan_scalar alwan_delta_e_zcam(alwan_jzazbz const *jab1, alwan_jzazbz const *jab2) {
    alwan_scalar const dJ = jab1->Jz - jab2->Jz;
    alwan_scalar const da = jab1->az - jab2->az;
    alwan_scalar const db = jab1->bz - jab2->bz;

    return ALWAN_SQRT(dJ * dJ + da * da + db * db);
}
