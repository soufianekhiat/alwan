/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <math.h>

/* ================================================================
 * XYZ ↔ xyY Conversions
 * ================================================================ */

void alwan_xyz_to_xyy(alwan_vec3 const *xyz, alwan_vec3 *xyy) {
    Scalar const X = xyz->v[0];
    Scalar const Y = xyz->v[1];
    Scalar const Z = xyz->v[2];

    Scalar const sum = X + Y + Z;

    if (sum < ALWAN_EPSILON) {
        /* Black point: return [0,0,0] for reversibility */
        xyy->v[0] = ALWAN_LITERAL(0.0);
        xyy->v[1] = ALWAN_LITERAL(0.0);
        xyy->v[2] = ALWAN_LITERAL(0.0);
    } else {
        xyy->v[0] = X / sum;  /* x */
        xyy->v[1] = Y / sum;  /* y */
        xyy->v[2] = Y;        /* Y */
    }
}

void alwan_xyy_to_xyz(alwan_vec3 const *xyy, alwan_vec3 *xyz) {
    Scalar const x = xyy->v[0];
    Scalar const y = xyy->v[1];
    Scalar const Y = xyy->v[2];

    if (y < ALWAN_EPSILON) {
        /* Degenerate case */
        xyz->v[0] = ALWAN_LITERAL(0.0);
        xyz->v[1] = ALWAN_LITERAL(0.0);
        xyz->v[2] = ALWAN_LITERAL(0.0);
    } else {
        xyz->v[0] = (x * Y) / y;           /* X */
        xyz->v[1] = Y;                      /* Y */
        xyz->v[2] = ((ALWAN_LITERAL(1.0) - x - y) * Y) / y;  /* Z */
    }
}

/* ================================================================
 * XYZ ↔ Lab Conversions
 * ================================================================ */

/* CIE Lab f(t) function with δ = 6/29 */
static inline Scalar lab_f(Scalar t) {
    Scalar const delta = ALWAN_LITERAL(6.0) / ALWAN_LITERAL(29.0);
    Scalar const delta3 = delta * delta * delta;
    Scalar const kappa = ALWAN_LITERAL(1.0) / (ALWAN_LITERAL(3.0) * delta * delta);
    Scalar const offset = ALWAN_LITERAL(16.0) / ALWAN_LITERAL(116.0);

    if (t > delta3) {
        return ALWAN_POW(t, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));
    } else {
        return kappa * t + offset;
    }
}

/* Inverse of CIE Lab f(t) function */
static inline Scalar lab_f_inv(Scalar t) {
    Scalar const delta = ALWAN_LITERAL(6.0) / ALWAN_LITERAL(29.0);
    Scalar const threshold = delta;
    Scalar const kappa = ALWAN_LITERAL(3.0) * delta * delta;
    Scalar const offset = ALWAN_LITERAL(16.0) / ALWAN_LITERAL(116.0);

    if (t > threshold) {
        return t * t * t;
    } else {
        return kappa * (t - offset);
    }
}

void alwan_xyz_to_lab(alwan_vec3 const *xyz, alwan_vec3 const *white_xyz, alwan_vec3 *lab) {
    Scalar const xr = xyz->v[0] / white_xyz->v[0];
    Scalar const yr = xyz->v[1] / white_xyz->v[1];
    Scalar const zr = xyz->v[2] / white_xyz->v[2];

    Scalar const fx = lab_f(xr);
    Scalar const fy = lab_f(yr);
    Scalar const fz = lab_f(zr);

    lab->v[0] = ALWAN_LITERAL(116.0) * fy - ALWAN_LITERAL(16.0);  /* L* */
    lab->v[1] = ALWAN_LITERAL(500.0) * (fx - fy);                  /* a* */
    lab->v[2] = ALWAN_LITERAL(200.0) * (fy - fz);                  /* b* */
}

void alwan_lab_to_xyz(alwan_vec3 const *lab, alwan_vec3 const *white_xyz, alwan_vec3 *xyz) {
    Scalar const L = lab->v[0];
    Scalar const a = lab->v[1];
    Scalar const b = lab->v[2];

    Scalar const fy = (L + ALWAN_LITERAL(16.0)) / ALWAN_LITERAL(116.0);
    Scalar const fx = a / ALWAN_LITERAL(500.0) + fy;
    Scalar const fz = fy - b / ALWAN_LITERAL(200.0);

    xyz->v[0] = white_xyz->v[0] * lab_f_inv(fx);  /* X */
    xyz->v[1] = white_xyz->v[1] * lab_f_inv(fy);  /* Y */
    xyz->v[2] = white_xyz->v[2] * lab_f_inv(fz);  /* Z */
}

/* ================================================================
 * XYZ ↔ Luv Conversions
 * ================================================================ */

/* Calculate u', v' chromaticity coordinates */
static inline void xyz_to_uv_prime(Scalar X, Scalar Y, Scalar Z, Scalar *u_prime, Scalar *v_prime) {
    Scalar const denom = X + ALWAN_LITERAL(15.0) * Y + ALWAN_LITERAL(3.0) * Z;

    if (denom < ALWAN_EPSILON) {
        *u_prime = ALWAN_LITERAL(0.0);
        *v_prime = ALWAN_LITERAL(0.0);
    } else {
        *u_prime = (ALWAN_LITERAL(4.0) * X) / denom;
        *v_prime = (ALWAN_LITERAL(9.0) * Y) / denom;
    }
}

void alwan_xyz_to_luv(alwan_vec3 const *xyz, alwan_vec3 const *white_xyz, alwan_vec3 *luv) {
    Scalar const yr = xyz->v[1] / white_xyz->v[1];

    /* Calculate L* (same formula as Lab) */
    Scalar const fy = lab_f(yr);
    Scalar const L = ALWAN_LITERAL(116.0) * fy - ALWAN_LITERAL(16.0);

    /* Calculate u', v' for sample and white point */
    Scalar u_prime, v_prime;
    Scalar un_prime, vn_prime;
    xyz_to_uv_prime(xyz->v[0], xyz->v[1], xyz->v[2], &u_prime, &v_prime);
    xyz_to_uv_prime(white_xyz->v[0], white_xyz->v[1], white_xyz->v[2], &un_prime, &vn_prime);

    luv->v[0] = L;  /* L* */
    luv->v[1] = ALWAN_LITERAL(13.0) * L * (u_prime - un_prime);  /* u* */
    luv->v[2] = ALWAN_LITERAL(13.0) * L * (v_prime - vn_prime);  /* v* */
}

void alwan_luv_to_xyz(alwan_vec3 const *luv, alwan_vec3 const *white_xyz, alwan_vec3 *xyz) {
    Scalar const L = luv->v[0];
    Scalar const u = luv->v[1];
    Scalar const v = luv->v[2];

    /* Calculate white point u', v' */
    Scalar un_prime, vn_prime;
    xyz_to_uv_prime(white_xyz->v[0], white_xyz->v[1], white_xyz->v[2], &un_prime, &vn_prime);

    /* Recover Y from L* */
    Scalar const fy = (L + ALWAN_LITERAL(16.0)) / ALWAN_LITERAL(116.0);
    Scalar const Y = white_xyz->v[1] * lab_f_inv(fy);

    /* Recover u', v' */
    Scalar u_prime, v_prime;
    if (ALWAN_FABS(L) < ALWAN_EPSILON) {
        u_prime = un_prime;
        v_prime = vn_prime;
    } else {
        u_prime = u / (ALWAN_LITERAL(13.0) * L) + un_prime;
        v_prime = v / (ALWAN_LITERAL(13.0) * L) + vn_prime;
    }

    /* Recover X, Z from u', v', Y */
    if (ALWAN_FABS(v_prime) < ALWAN_EPSILON) {
        xyz->v[0] = ALWAN_LITERAL(0.0);
        xyz->v[1] = ALWAN_LITERAL(0.0);
        xyz->v[2] = ALWAN_LITERAL(0.0);
    } else {
        xyz->v[0] = Y * (ALWAN_LITERAL(9.0) * u_prime) / (ALWAN_LITERAL(4.0) * v_prime);  /* X */
        xyz->v[1] = Y;  /* Y */
        xyz->v[2] = Y * (ALWAN_LITERAL(12.0) - ALWAN_LITERAL(3.0) * u_prime - ALWAN_LITERAL(20.0) * v_prime) / (ALWAN_LITERAL(4.0) * v_prime);  /* Z */
    }
}

/* ================================================================
 * Lab ↔ LCh(ab) Conversions
 * ================================================================ */

void alwan_lab_to_lch(alwan_vec3 const *lab, alwan_vec3 *lch) {
    Scalar const L = lab->v[0];
    Scalar const a = lab->v[1];
    Scalar const b = lab->v[2];

    lch->v[0] = L;  /* L* */
    lch->v[1] = ALWAN_SQRT(a * a + b * b);  /* C*ab */
    Scalar h_rad = ALWAN_ATAN2(b, a);
    lch->v[2] = h_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;  /* hab in degrees */
}

void alwan_lch_to_lab(alwan_vec3 const *lch, alwan_vec3 *lab) {
    Scalar const L = lch->v[0];
    Scalar const C = lch->v[1];
    Scalar const h_deg = lch->v[2];
    Scalar const h_rad = h_deg * ALWAN_PI / ALWAN_LITERAL(180.0);

    lab->v[0] = L;  /* L* */
    lab->v[1] = C * ALWAN_COS(h_rad);  /* a* */
    lab->v[2] = C * ALWAN_SIN(h_rad);  /* b* */
}

/* ================================================================
 * Luv ↔ LCh(uv) Conversions
 * ================================================================ */

void alwan_luv_to_lchuv(alwan_vec3 const *luv, alwan_vec3 *lchuv) {
    Scalar const L = luv->v[0];
    Scalar const u = luv->v[1];
    Scalar const v = luv->v[2];

    lchuv->v[0] = L;  /* L* */
    lchuv->v[1] = ALWAN_SQRT(u * u + v * v);  /* C*uv */
    Scalar h_rad = ALWAN_ATAN2(v, u);
    lchuv->v[2] = h_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;  /* huv in degrees */
}

void alwan_lchuv_to_luv(alwan_vec3 const *lchuv, alwan_vec3 *luv) {
    Scalar const L = lchuv->v[0];
    Scalar const C = lchuv->v[1];
    Scalar const h_deg = lchuv->v[2];
    Scalar const h_rad = h_deg * ALWAN_PI / ALWAN_LITERAL(180.0);

    luv->v[0] = L;  /* L* */
    luv->v[1] = C * ALWAN_COS(h_rad);  /* u* */
    luv->v[2] = C * ALWAN_SIN(h_rad);  /* v* */
}

/* ================================================================
 * Color Difference (ΔE) Metrics
 * ================================================================ */

Scalar alwan_delta_e_76(alwan_vec3 const *lab1, alwan_vec3 const *lab2) {
    Scalar const dL = lab1->v[0] - lab2->v[0];
    Scalar const da = lab1->v[1] - lab2->v[1];
    Scalar const db = lab1->v[2] - lab2->v[2];

    return ALWAN_SQRT(dL * dL + da * da + db * db);
}

Scalar alwan_delta_e_94(alwan_vec3 const *lab1, alwan_vec3 const *lab2) {
    /* Graphic arts defaults: kL = 1, K1 = 0.045, K2 = 0.015 */
    Scalar const kL = ALWAN_LITERAL(1.0);
    Scalar const K1 = ALWAN_LITERAL(0.045);
    Scalar const K2 = ALWAN_LITERAL(0.015);
    Scalar const kC = ALWAN_LITERAL(1.0);
    Scalar const kH = ALWAN_LITERAL(1.0);

    Scalar const L1 = lab1->v[0];
    Scalar const a1 = lab1->v[1];
    Scalar const b1 = lab1->v[2];
    Scalar const L2 = lab2->v[0];
    Scalar const a2 = lab2->v[1];
    Scalar const b2 = lab2->v[2];

    Scalar const dL = L1 - L2;
    Scalar const C1 = ALWAN_SQRT(a1 * a1 + b1 * b1);
    Scalar const C2 = ALWAN_SQRT(a2 * a2 + b2 * b2);
    Scalar const dCab = C1 - C2;

    Scalar const da = a1 - a2;
    Scalar const db = b1 - b2;
    Scalar const dHab_sq = da * da + db * db - dCab * dCab;

    Scalar const SL = ALWAN_LITERAL(1.0);
    Scalar const SC = ALWAN_LITERAL(1.0) + K1 * C1;
    Scalar const SH = ALWAN_LITERAL(1.0) + K2 * C1;

    Scalar const term1 = (dL / (kL * SL));
    Scalar const term2 = (dCab / (kC * SC));
    Scalar const term3 = (dHab_sq > ALWAN_LITERAL(0.0)) ? (ALWAN_SQRT(dHab_sq) / (kH * SH)) : ALWAN_LITERAL(0.0);

    return ALWAN_SQRT(term1 * term1 + term2 * term2 + term3 * term3);
}

Scalar alwan_delta_e_cmc(alwan_vec3 const *lab1, alwan_vec3 const *lab2, Scalar l, Scalar c) {
    Scalar const L1 = lab1->v[0];
    Scalar const a1 = lab1->v[1];
    Scalar const b1 = lab1->v[2];
    Scalar const L2 = lab2->v[0];
    Scalar const a2 = lab2->v[1];
    Scalar const b2 = lab2->v[2];

    Scalar const dL = L1 - L2;
    Scalar const C1 = ALWAN_SQRT(a1 * a1 + b1 * b1);
    Scalar const C2 = ALWAN_SQRT(a2 * a2 + b2 * b2);
    Scalar const dCab = C1 - C2;

    Scalar const da = a1 - a2;
    Scalar const db = b1 - b2;
    Scalar const dHab_sq = da * da + db * db - dCab * dCab;

    /* Calculate hue angle for reference color */
    Scalar h1 = ALWAN_ATAN2(b1, a1);
    if (h1 < ALWAN_LITERAL(0.0)) {
        h1 += ALWAN_LITERAL(2.0) * ALWAN_PI;
    }
    h1 = h1 * ALWAN_LITERAL(180.0) / ALWAN_PI;  /* Convert to degrees */

    /* SL weighting factor */
    Scalar SL;
    if (L1 < ALWAN_LITERAL(16.0)) {
        SL = ALWAN_LITERAL(0.511);
    } else {
        SL = (ALWAN_LITERAL(0.040975) * L1) / (ALWAN_LITERAL(1.0) + ALWAN_LITERAL(0.01765) * L1);
    }

    /* SC weighting factor */
    Scalar const SC = (ALWAN_LITERAL(0.0638) * C1) / (ALWAN_LITERAL(1.0) + ALWAN_LITERAL(0.0131) * C1) + ALWAN_LITERAL(0.638);

    /* F factor for SH */
    Scalar const C1_4 = C1 * C1 * C1 * C1;
    Scalar const F = ALWAN_SQRT(C1_4 / (C1_4 + ALWAN_LITERAL(1900.0)));

    /* T factor for SH */
    Scalar T;
    if (h1 >= ALWAN_LITERAL(164.0) && h1 <= ALWAN_LITERAL(345.0)) {
        T = ALWAN_LITERAL(0.56) + ALWAN_FABS(ALWAN_LITERAL(0.2) * ALWAN_COS((h1 + ALWAN_LITERAL(168.0)) * ALWAN_PI / ALWAN_LITERAL(180.0)));
    } else {
        T = ALWAN_LITERAL(0.36) + ALWAN_FABS(ALWAN_LITERAL(0.4) * ALWAN_COS((h1 + ALWAN_LITERAL(35.0)) * ALWAN_PI / ALWAN_LITERAL(180.0)));
    }

    /* SH weighting factor */
    Scalar const SH = SC * (F * T + ALWAN_LITERAL(1.0) - F);

    Scalar const term1 = dL / (l * SL);
    Scalar const term2 = dCab / (c * SC);
    Scalar const term3 = (dHab_sq > ALWAN_LITERAL(0.0)) ? (ALWAN_SQRT(dHab_sq) / SH) : ALWAN_LITERAL(0.0);

    return ALWAN_SQRT(term1 * term1 + term2 * term2 + term3 * term3);
}

Scalar alwan_delta_e_2000(alwan_vec3 const *lab1, alwan_vec3 const *lab2) {
    Scalar const L1 = lab1->v[0];
    Scalar const a1 = lab1->v[1];
    Scalar const b1 = lab1->v[2];
    Scalar const L2 = lab2->v[0];
    Scalar const a2 = lab2->v[1];
    Scalar const b2 = lab2->v[2];

    /* Calculate Cab */
    Scalar const C1 = ALWAN_SQRT(a1 * a1 + b1 * b1);
    Scalar const C2 = ALWAN_SQRT(a2 * a2 + b2 * b2);
    Scalar const Cab_mean = (C1 + C2) / ALWAN_LITERAL(2.0);

    /* Calculate G (a' adjustment factor) */
    Scalar const Cab_mean_7 = Cab_mean * Cab_mean * Cab_mean * Cab_mean * Cab_mean * Cab_mean * Cab_mean;
    Scalar const G = ALWAN_LITERAL(0.5) * (ALWAN_LITERAL(1.0) - ALWAN_SQRT(Cab_mean_7 / (Cab_mean_7 + ALWAN_LITERAL(6103515625.0))));  /* 25^7 = 6103515625 */

    /* Calculate a' */
    Scalar const a1_prime = (ALWAN_LITERAL(1.0) + G) * a1;
    Scalar const a2_prime = (ALWAN_LITERAL(1.0) + G) * a2;

    /* Calculate C' and h' */
    Scalar const C1_prime = ALWAN_SQRT(a1_prime * a1_prime + b1 * b1);
    Scalar const C2_prime = ALWAN_SQRT(a2_prime * a2_prime + b2 * b2);

    Scalar h1_prime = ALWAN_ATAN2(b1, a1_prime);
    if (h1_prime < ALWAN_LITERAL(0.0)) h1_prime += ALWAN_LITERAL(2.0) * ALWAN_PI;
    h1_prime = h1_prime * ALWAN_LITERAL(180.0) / ALWAN_PI;  /* Convert to degrees */

    Scalar h2_prime = ALWAN_ATAN2(b2, a2_prime);
    if (h2_prime < ALWAN_LITERAL(0.0)) h2_prime += ALWAN_LITERAL(2.0) * ALWAN_PI;
    h2_prime = h2_prime * ALWAN_LITERAL(180.0) / ALWAN_PI;  /* Convert to degrees */

    /* Calculate ΔL', ΔC', ΔH' */
    Scalar const dL_prime = L2 - L1;
    Scalar const dC_prime = C2_prime - C1_prime;

    Scalar dh_prime;
    if (C1_prime * C2_prime < ALWAN_EPSILON) {
        dh_prime = ALWAN_LITERAL(0.0);
    } else if (ALWAN_FABS(h2_prime - h1_prime) <= ALWAN_LITERAL(180.0)) {
        dh_prime = h2_prime - h1_prime;
    } else if (h2_prime - h1_prime > ALWAN_LITERAL(180.0)) {
        dh_prime = h2_prime - h1_prime - ALWAN_LITERAL(360.0);
    } else {
        dh_prime = h2_prime - h1_prime + ALWAN_LITERAL(360.0);
    }

    Scalar const dH_prime = ALWAN_LITERAL(2.0) * ALWAN_SQRT(C1_prime * C2_prime) * ALWAN_SIN(dh_prime * ALWAN_PI / ALWAN_LITERAL(360.0));

    /* Calculate mean values */
    Scalar const L_prime_mean = (L1 + L2) / ALWAN_LITERAL(2.0);
    Scalar const C_prime_mean = (C1_prime + C2_prime) / ALWAN_LITERAL(2.0);

    Scalar H_prime_mean;
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
    Scalar const T = ALWAN_LITERAL(1.0)
        - ALWAN_LITERAL(0.17) * ALWAN_COS((H_prime_mean - ALWAN_LITERAL(30.0)) * ALWAN_PI / ALWAN_LITERAL(180.0))
        + ALWAN_LITERAL(0.24) * ALWAN_COS((ALWAN_LITERAL(2.0) * H_prime_mean) * ALWAN_PI / ALWAN_LITERAL(180.0))
        + ALWAN_LITERAL(0.32) * ALWAN_COS((ALWAN_LITERAL(3.0) * H_prime_mean + ALWAN_LITERAL(6.0)) * ALWAN_PI / ALWAN_LITERAL(180.0))
        - ALWAN_LITERAL(0.20) * ALWAN_COS((ALWAN_LITERAL(4.0) * H_prime_mean - ALWAN_LITERAL(63.0)) * ALWAN_PI / ALWAN_LITERAL(180.0));

    Scalar const dTheta = ALWAN_LITERAL(30.0) * ALWAN_EXP(-((H_prime_mean - ALWAN_LITERAL(275.0)) / ALWAN_LITERAL(25.0)) * ((H_prime_mean - ALWAN_LITERAL(275.0)) / ALWAN_LITERAL(25.0)));

    Scalar const C_prime_mean_7 = C_prime_mean * C_prime_mean * C_prime_mean * C_prime_mean * C_prime_mean * C_prime_mean * C_prime_mean;
    Scalar const RC = ALWAN_LITERAL(2.0) * ALWAN_SQRT(C_prime_mean_7 / (C_prime_mean_7 + ALWAN_LITERAL(6103515625.0)));

    Scalar const L_prime_mean_minus_50_sq = (L_prime_mean - ALWAN_LITERAL(50.0)) * (L_prime_mean - ALWAN_LITERAL(50.0));
    Scalar const SL = ALWAN_LITERAL(1.0) + (ALWAN_LITERAL(0.015) * L_prime_mean_minus_50_sq) / ALWAN_SQRT(ALWAN_LITERAL(20.0) + L_prime_mean_minus_50_sq);
    Scalar const SC = ALWAN_LITERAL(1.0) + ALWAN_LITERAL(0.045) * C_prime_mean;
    Scalar const SH = ALWAN_LITERAL(1.0) + ALWAN_LITERAL(0.015) * C_prime_mean * T;
    Scalar const RT = -ALWAN_SIN((ALWAN_LITERAL(2.0) * dTheta) * ALWAN_PI / ALWAN_LITERAL(180.0)) * RC;

    /* Calculate ΔE00 with kL = kC = kH = 1 */
    Scalar const term1 = dL_prime / SL;
    Scalar const term2 = dC_prime / SC;
    Scalar const term3 = dH_prime / SH;

    return ALWAN_SQRT(term1 * term1 + term2 * term2 + term3 * term3 + RT * term2 * term3);
}
