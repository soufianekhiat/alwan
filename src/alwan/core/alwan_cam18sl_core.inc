/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only CAM18sl colour appearance model for self-luminous stimuli (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: Hermans, Smet and Hanselaer (2018), "Color appearance model for self-luminous
 * stimuli", JOSA A 35(12), 2000-2009, with the corrections luxpy (whose co-author wrote the
 * model) documents: the CIE 2006 10 degree cone fundamentals throughout, cone scaling
 * k = (676.7, 794.0, 1461.5) against K = 683.144. Input is absolute CIE 2006 10 degree XYZ
 * (cd/m2); the background is an equal-energy white of luminance Y_b, whose cone signals
 * equal Y_b, so the von Kries step divides by the equal-energy white's own signals. The
 * CIE 2006 fundamentals give that white X = Y = Z by construction, which alwan takes
 * exactly; luxpy sums the tabulated functions and lands 5e-9 away. Field of view 10
 * degrees (size factor 1). Held to luxpy's cam18sl in suite 260.
 */

/* ================================================================
 * CAM18sl Correlates
 * ================================================================ */

typedef struct {
    ALWAN_CORE_T Q;   /* Brightness */
    ALWAN_CORE_T C;   /* Saturation s = M / Q */
    ALWAN_CORE_T h;   /* Hue angle (degrees) */
    ALWAN_CORE_T M;   /* Colourfulness */
    ALWAN_CORE_T a;   /* M cos h */
    ALWAN_CORE_T b;   /* M sin h */
} ALWAN_CORE_FNLIT(alwan_cam18sl_v_correlates);

/* CIE 170-2 10 degree cone fundamentals from XYZ: its published LMS to XYZ matrix,
 * X = 1.93986443 L - 1.34664359 M + 0.43044935 S, Y = 0.69283932 L + 0.34967567 M,
 * Z = 2.14687945 S, solved for LMS. */
ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_cam18sl_xyz_to_lms)(ALWAN_CORE_XYZ xyz) {
    ALWAN_CORE_T a11 = ALWAN_CORE_LITERAL(1.93986443);
    ALWAN_CORE_T a12 = ALWAN_CORE_LITERAL(-1.34664359);
    ALWAN_CORE_T a13 = ALWAN_CORE_LITERAL(0.43044935);
    ALWAN_CORE_T a21 = ALWAN_CORE_LITERAL(0.69283932);
    ALWAN_CORE_T a22 = ALWAN_CORE_LITERAL(0.34967567);
    ALWAN_CORE_T a33 = ALWAN_CORE_LITERAL(2.14687945);
    ALWAN_CORE_VEC3 lms;
    ALWAN_CORE_T s = xyz.z / a33;
    ALWAN_CORE_T xr = xyz.x - a13 * s;
    ALWAN_CORE_T det = a11 * a22 - a12 * a21;
    lms.v[0] = (a22 * xr - a12 * xyz.y) / det;
    lms.v[1] = (a11 * xyz.y - a21 * xr) / det;
    lms.v[2] = s;
    return lms;
}

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_cam18sl_lms_to_xyz)(ALWAN_CORE_VEC3 lms) {
    ALWAN_CORE_XYZ xyz;
    xyz.x = ALWAN_CORE_LITERAL(1.93986443) * lms.v[0] + ALWAN_CORE_LITERAL(-1.34664359) * lms.v[1]
          + ALWAN_CORE_LITERAL(0.43044935) * lms.v[2];
    xyz.y = ALWAN_CORE_LITERAL(0.69283932) * lms.v[0] + ALWAN_CORE_LITERAL(0.34967567) * lms.v[1];
    xyz.z = ALWAN_CORE_LITERAL(2.14687945) * lms.v[2];
    return xyz;
}

/* Naka-Rushton, n = 0.58, signed: sign(x) |x|^n / (|x|^n + sig^n). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_cam18sl_nr)(ALWAN_CORE_T x, ALWAN_CORE_T sig_n) {
    ALWAN_CORE_T ax = ALWAN_CORE_ABS(x);
    ALWAN_CORE_T p = ALWAN_CORE_POW(ax, ALWAN_CORE_LITERAL(0.58));
    ALWAN_CORE_T r = p / (p + sig_n);
    return ALWAN_CORE_SELECT(x < ALWAN_CORE_ZERO, -r, r);
}

/* Its inverse: sign(z) | |z| sig^n / (sign(z) - z) |^(1 / n), 0 at 0. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_cam18sl_nr_inv)(ALWAN_CORE_T z, ALWAN_CORE_T sig_n) {
    ALWAN_CORE_T sgn = ALWAN_CORE_SELECT(z < ALWAN_CORE_ZERO, -ALWAN_CORE_ONE, ALWAN_CORE_ONE);
    ALWAN_CORE_T az = ALWAN_CORE_ABS(z);
    ALWAN_CORE_T q = az * sig_n / (sgn - z);
    ALWAN_CORE_T aq = ALWAN_CORE_ABS(q);
    ALWAN_CORE_T r = ALWAN_CORE_POW(aq, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(0.58));
    r = ALWAN_CORE_SELECT(z < ALWAN_CORE_ZERO, -r, r);
    return ALWAN_CORE_SELECT(az > ALWAN_CORE_ZERO, r, ALWAN_CORE_ZERO);
}

/* The semi-saturation constant raised to n: sig = 291.20 + 71.8 Y_b^0.78. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_cam18sl_sig_n)(ALWAN_CORE_T Y_b) {
    ALWAN_CORE_T yb = ALWAN_CORE_SELECT(Y_b > ALWAN_CORE_ZERO, Y_b, ALWAN_CORE_ZERO);
    ALWAN_CORE_T sig = ALWAN_CORE_LITERAL(291.20) + ALWAN_CORE_LITERAL(71.8) * ALWAN_CORE_POW(yb, ALWAN_CORE_LITERAL(0.78));
    return ALWAN_CORE_POW(sig, ALWAN_CORE_LITERAL(0.58));
}

/* Cone signals scaled to rho, gamma, beta: lms / K * k. */
ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_cam18sl_rgb)(ALWAN_CORE_VEC3 lms) {
    ALWAN_CORE_VEC3 r;
    ALWAN_CORE_T K = ALWAN_CORE_LITERAL(683.144);
    r.v[0] = lms.v[0] / K * ALWAN_CORE_LITERAL(676.7);
    r.v[1] = lms.v[1] / K * ALWAN_CORE_LITERAL(794.0);
    r.v[2] = lms.v[2] / K * ALWAN_CORE_LITERAL(1461.5);
    return r;
}

/* ================================================================
 * CAM18sl Forward Transform: XYZ (cd/m2) -> correlates
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_cam18sl_v_correlates) ALWAN_CORE_FNVLIT(alwan_cam18sl_forward)(
    ALWAN_CORE_XYZ xyz,
    ALWAN_CORE_T Y_b) {
    ALWAN_CORE_FNLIT(alwan_cam18sl_v_correlates) result;
    ALWAN_CORE_XYZ eew;
    eew.x = ALWAN_CORE_ONE; eew.y = ALWAN_CORE_ONE; eew.z = ALWAN_CORE_ONE;
    ALWAN_CORE_VEC3 rgb = ALWAN_CORE_FNVLIT(alwan_cam18sl_rgb)(ALWAN_CORE_FNVLIT(alwan_cam18sl_xyz_to_lms)(xyz));
    ALWAN_CORE_VEC3 w = ALWAN_CORE_FNVLIT(alwan_cam18sl_rgb)(ALWAN_CORE_FNVLIT(alwan_cam18sl_xyz_to_lms)(eew));

    /* von Kries to the equal-energy white of the background's luminance (D = 1); a zero
     * background leaves the signals as they are, as luxpy does. */
    ALWAN_CORE_T ra = ALWAN_CORE_SELECT(Y_b > ALWAN_CORE_ZERO, rgb.v[0] / w.v[0], rgb.v[0]);
    ALWAN_CORE_T ga = ALWAN_CORE_SELECT(Y_b > ALWAN_CORE_ZERO, rgb.v[1] / w.v[1], rgb.v[1]);
    ALWAN_CORE_T ba = ALWAN_CORE_SELECT(Y_b > ALWAN_CORE_ZERO, rgb.v[2] / w.v[2], rgb.v[2]);

    ALWAN_CORE_T sig_n = ALWAN_CORE_FNVLIT(alwan_cam18sl_sig_n)(Y_b);
    ALWAN_CORE_T rc = ALWAN_CORE_FNVLIT(alwan_cam18sl_nr)(ra, sig_n);
    ALWAN_CORE_T gc = ALWAN_CORE_FNVLIT(alwan_cam18sl_nr)(ga, sig_n);
    ALWAN_CORE_T bc = ALWAN_CORE_FNVLIT(alwan_cam18sl_nr)(ba, sig_n);

    ALWAN_CORE_T A = ALWAN_CORE_LITERAL(2.0) * rc + gc + bc / ALWAN_CORE_LITERAL(20.0);
    ALWAN_CORE_T a = ALWAN_CORE_LITERAL(0.63) * (rc - ALWAN_CORE_LITERAL(12.0) / ALWAN_CORE_LITERAL(11.0) * gc
                                                 + bc / ALWAN_CORE_LITERAL(11.0));
    ALWAN_CORE_T b = ALWAN_CORE_LITERAL(0.12) * (rc + gc - ALWAN_CORE_LITERAL(2.0) * bc);

    ALWAN_CORE_T M = ALWAN_CORE_LITERAL(3260.0) * ALWAN_CORE_SQRT(a * a + b * b);
    ALWAN_CORE_T Q = ALWAN_CORE_LITERAL(0.937) * (A + ALWAN_CORE_LITERAL(0.0024) * ALWAN_CORE_POW(M, ALWAN_CORE_LITERAL(1.09)));
    ALWAN_CORE_T h = ALWAN_CORE_ATAN2(b, a) * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;
    h = ALWAN_CORE_SELECT(h < ALWAN_CORE_ZERO, h + ALWAN_CORE_LITERAL(360.0), h);

    result.Q = Q;
    result.M = M;
    result.C = ALWAN_CORE_SELECT(Q != ALWAN_CORE_ZERO, M / Q, ALWAN_CORE_ZERO);
    result.h = h;
    result.a = ALWAN_CORE_LITERAL(3260.0) * a;
    result.b = ALWAN_CORE_LITERAL(3260.0) * b;
    return result;
}

/* ================================================================
 * CAM18sl Inverse Transform: Q, M, h -> XYZ (cd/m2)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_cam18sl_inverse)(
    ALWAN_CORE_FNLIT(alwan_cam18sl_v_correlates) correlates,
    ALWAN_CORE_T Y_b) {
    ALWAN_CORE_T M = correlates.M;
    ALWAN_CORE_T A = correlates.Q / ALWAN_CORE_LITERAL(0.937) - ALWAN_CORE_LITERAL(0.0024) * ALWAN_CORE_POW(M, ALWAN_CORE_LITERAL(1.09));
    ALWAN_CORE_T hr = correlates.h * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);
    ALWAN_CORE_T a = M / ALWAN_CORE_LITERAL(3260.0) * ALWAN_CORE_COS(hr) / ALWAN_CORE_LITERAL(0.63);
    ALWAN_CORE_T b = M / ALWAN_CORE_LITERAL(3260.0) * ALWAN_CORE_SIN(hr) / ALWAN_CORE_LITERAL(0.12);

    /* The inverse of [2 1 1/20; 1 -12/11 1/11; 1 1 -2], exactly. */
    ALWAN_CORE_T rc = ALWAN_CORE_LITERAL(20.0) / ALWAN_CORE_LITERAL(61.0) * A
                    + ALWAN_CORE_LITERAL(451.0) / ALWAN_CORE_LITERAL(1403.0) * a
                    + ALWAN_CORE_LITERAL(32.0) / ALWAN_CORE_LITERAL(1403.0) * b;
    ALWAN_CORE_T gc = ALWAN_CORE_LITERAL(20.0) / ALWAN_CORE_LITERAL(61.0) * A
                    - ALWAN_CORE_LITERAL(891.0) / ALWAN_CORE_LITERAL(1403.0) * a
                    - ALWAN_CORE_LITERAL(29.0) / ALWAN_CORE_LITERAL(1403.0) * b;
    ALWAN_CORE_T bc = ALWAN_CORE_LITERAL(20.0) / ALWAN_CORE_LITERAL(61.0) * A
                    - ALWAN_CORE_LITERAL(220.0) / ALWAN_CORE_LITERAL(1403.0) * a
                    - ALWAN_CORE_LITERAL(700.0) / ALWAN_CORE_LITERAL(1403.0) * b;

    ALWAN_CORE_T sig_n = ALWAN_CORE_FNVLIT(alwan_cam18sl_sig_n)(Y_b);
    ALWAN_CORE_T ra = ALWAN_CORE_FNVLIT(alwan_cam18sl_nr_inv)(rc, sig_n);
    ALWAN_CORE_T ga = ALWAN_CORE_FNVLIT(alwan_cam18sl_nr_inv)(gc, sig_n);
    ALWAN_CORE_T ba = ALWAN_CORE_FNVLIT(alwan_cam18sl_nr_inv)(bc, sig_n);

    ALWAN_CORE_XYZ eew;
    eew.x = ALWAN_CORE_ONE; eew.y = ALWAN_CORE_ONE; eew.z = ALWAN_CORE_ONE;
    ALWAN_CORE_VEC3 w = ALWAN_CORE_FNVLIT(alwan_cam18sl_rgb)(ALWAN_CORE_FNVLIT(alwan_cam18sl_xyz_to_lms)(eew));
    ALWAN_CORE_T r = ALWAN_CORE_SELECT(Y_b > ALWAN_CORE_ZERO, ra * w.v[0], ra);
    ALWAN_CORE_T g = ALWAN_CORE_SELECT(Y_b > ALWAN_CORE_ZERO, ga * w.v[1], ga);
    ALWAN_CORE_T bb = ALWAN_CORE_SELECT(Y_b > ALWAN_CORE_ZERO, ba * w.v[2], ba);

    ALWAN_CORE_T K = ALWAN_CORE_LITERAL(683.144);
    ALWAN_CORE_VEC3 lms;
    lms.v[0] = r / ALWAN_CORE_LITERAL(676.7) * K;
    lms.v[1] = g / ALWAN_CORE_LITERAL(794.0) * K;
    lms.v[2] = bb / ALWAN_CORE_LITERAL(1461.5) * K;
    return ALWAN_CORE_FNVLIT(alwan_cam18sl_lms_to_xyz)(lms);
}
