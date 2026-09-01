/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only CIE Lab, Luv, xyY, LCh, UCS, UVW, Delta E Color Spaces (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Uses ALWAN_CORE_* macros (set by alwan_core_f32_setup.h or _f64_setup.h).
 */

/* ================================================================
 * XYZ <-> xyY
 * ================================================================ */

/* XYZ -> xyY (CIE 1931 chromaticity + luminance) */
ALWAN_INLINE ALWAN_CORE_XYY ALWAN_CORE_FNVLIT(alwan_xyz_to_xyy)(ALWAN_CORE_XYZ xyz) {
    ALWAN_CORE_XYY result;
    ALWAN_CORE_T sum = xyz.x + xyz.y + xyz.z;
    result.x = ALWAN_CORE_SELECT(sum < ALWAN_CORE_EPSILON, ALWAN_CORE_LITERAL(0.0), xyz.x / sum);
    result.y = ALWAN_CORE_SELECT(sum < ALWAN_CORE_EPSILON, ALWAN_CORE_LITERAL(0.0), xyz.y / sum);
    result.Y = ALWAN_CORE_SELECT(sum < ALWAN_CORE_EPSILON, ALWAN_CORE_LITERAL(0.0), xyz.y);
    return result;
}

/* xyY -> XYZ */
ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_xyy_to_xyz)(ALWAN_CORE_XYY xyy) {
    ALWAN_CORE_XYZ result;
    result.x = ALWAN_CORE_SELECT(xyy.y < ALWAN_CORE_EPSILON, ALWAN_CORE_LITERAL(0.0), (xyy.x * xyy.Y) / xyy.y);
    result.y = ALWAN_CORE_SELECT(xyy.y < ALWAN_CORE_EPSILON, ALWAN_CORE_LITERAL(0.0), xyy.Y);
    result.z = ALWAN_CORE_SELECT(xyy.y < ALWAN_CORE_EPSILON, ALWAN_CORE_LITERAL(0.0), ((ALWAN_CORE_LITERAL(1.0) - xyy.x - xyy.y) * xyy.Y) / xyy.y);
    return result;
}

/* ================================================================
 * XYZ <-> CIE Lab
 * ================================================================ */

/* XYZ -> CIE Lab (using reference white point) */
ALWAN_INLINE ALWAN_CORE_LAB ALWAN_CORE_FNVLIT(alwan_xyz_to_lab)(ALWAN_CORE_XYZ xyz, ALWAN_CORE_XYZ white) {
    ALWAN_CORE_LAB result;
    ALWAN_CORE_T xr = xyz.x / white.x;
    ALWAN_CORE_T yr = xyz.y / white.y;
    ALWAN_CORE_T zr = xyz.z / white.z;
    ALWAN_CORE_T fx = ALWAN_CORE_FNLIT(alwan_lab_f)(xr);
    ALWAN_CORE_T fy = ALWAN_CORE_FNLIT(alwan_lab_f)(yr);
    ALWAN_CORE_T fz = ALWAN_CORE_FNLIT(alwan_lab_f)(zr);
    result.L = ALWAN_CORE_LITERAL(116.0) * fy - ALWAN_CORE_LITERAL(16.0);
    result.a = ALWAN_CORE_LITERAL(500.0) * (fx - fy);
    result.b = ALWAN_CORE_LITERAL(200.0) * (fy - fz);
    return result;
}

/* CIE Lab -> XYZ (using reference white point) */
ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_lab_to_xyz)(ALWAN_CORE_LAB lab, ALWAN_CORE_XYZ white) {
    ALWAN_CORE_XYZ result;
    ALWAN_CORE_T fy = (lab.L + ALWAN_CORE_LITERAL(16.0)) / ALWAN_CORE_LITERAL(116.0);
    ALWAN_CORE_T fx = lab.a / ALWAN_CORE_LITERAL(500.0) + fy;
    ALWAN_CORE_T fz = fy - lab.b / ALWAN_CORE_LITERAL(200.0);
    result.x = white.x * ALWAN_CORE_FNLIT(alwan_lab_f_inv)(fx);
    result.y = white.y * ALWAN_CORE_FNLIT(alwan_lab_f_inv)(fy);
    result.z = white.z * ALWAN_CORE_FNLIT(alwan_lab_f_inv)(fz);
    return result;
}

/* ================================================================
 * XYZ <-> CIE Luv
 * ================================================================ */

/* XYZ -> CIE Luv (using reference white point) */
ALWAN_INLINE ALWAN_CORE_LUV ALWAN_CORE_FNVLIT(alwan_xyz_to_luv)(ALWAN_CORE_XYZ xyz, ALWAN_CORE_XYZ white) {
    ALWAN_CORE_LUV result;

    /* u', v' for sample */
    ALWAN_CORE_T denom = xyz.x + ALWAN_CORE_LITERAL(15.0) * xyz.y + ALWAN_CORE_LITERAL(3.0) * xyz.z;
    ALWAN_CORE_T u_prime = ALWAN_CORE_SELECT(denom < ALWAN_CORE_EPSILON,
        ALWAN_CORE_LITERAL(0.0),
        (ALWAN_CORE_LITERAL(4.0) * xyz.x) / denom);
    ALWAN_CORE_T v_prime = ALWAN_CORE_SELECT(denom < ALWAN_CORE_EPSILON,
        ALWAN_CORE_LITERAL(0.0),
        (ALWAN_CORE_LITERAL(9.0) * xyz.y) / denom);

    /* u'n, v'n for white point */
    ALWAN_CORE_T denom_n = white.x + ALWAN_CORE_LITERAL(15.0) * white.y + ALWAN_CORE_LITERAL(3.0) * white.z;
    ALWAN_CORE_T u_prime_n = ALWAN_CORE_SELECT(denom_n < ALWAN_CORE_EPSILON,
        ALWAN_CORE_LITERAL(0.0),
        (ALWAN_CORE_LITERAL(4.0) * white.x) / denom_n);
    ALWAN_CORE_T v_prime_n = ALWAN_CORE_SELECT(denom_n < ALWAN_CORE_EPSILON,
        ALWAN_CORE_LITERAL(0.0),
        (ALWAN_CORE_LITERAL(9.0) * white.y) / denom_n);

    /* L* = 116 * f(Y/Yn) - 16 */
    ALWAN_CORE_T yr = xyz.y / white.y;
    ALWAN_CORE_T L = ALWAN_CORE_LITERAL(116.0) * ALWAN_CORE_FNLIT(alwan_lab_f)(yr) - ALWAN_CORE_LITERAL(16.0);

    result.L = L;
    result.u = ALWAN_CORE_LITERAL(13.0) * L * (u_prime - u_prime_n);
    result.v = ALWAN_CORE_LITERAL(13.0) * L * (v_prime - v_prime_n);
    return result;
}

/* CIE Luv -> XYZ (using reference white point) */
ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_luv_to_xyz)(ALWAN_CORE_LUV luv, ALWAN_CORE_XYZ white) {
    ALWAN_CORE_XYZ result;

    /* u'n, v'n for white point */
    ALWAN_CORE_T denom_n = white.x + ALWAN_CORE_LITERAL(15.0) * white.y + ALWAN_CORE_LITERAL(3.0) * white.z;
    ALWAN_CORE_T u_prime_n = ALWAN_CORE_SELECT(denom_n < ALWAN_CORE_EPSILON,
        ALWAN_CORE_LITERAL(0.0),
        (ALWAN_CORE_LITERAL(4.0) * white.x) / denom_n);
    ALWAN_CORE_T v_prime_n = ALWAN_CORE_SELECT(denom_n < ALWAN_CORE_EPSILON,
        ALWAN_CORE_LITERAL(0.0),
        (ALWAN_CORE_LITERAL(9.0) * white.y) / denom_n);

    /* Recover Y from L* */
    ALWAN_CORE_T fy = (luv.L + ALWAN_CORE_LITERAL(16.0)) / ALWAN_CORE_LITERAL(116.0);
    ALWAN_CORE_T Y = white.y * ALWAN_CORE_FNLIT(alwan_lab_f_inv)(fy);

    /* Recover u', v' from u*, v* */
    ALWAN_CORE_T L13 = ALWAN_CORE_LITERAL(13.0) * luv.L;
    ALWAN_CORE_T u_prime = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(luv.L) < ALWAN_CORE_EPSILON,
        ALWAN_CORE_LITERAL(0.0),
        luv.u / L13 + u_prime_n);
    ALWAN_CORE_T v_prime = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(luv.L) < ALWAN_CORE_EPSILON,
        ALWAN_CORE_LITERAL(0.0),
        luv.v / L13 + v_prime_n);

    /* Recover X, Z from u', v', Y */
    result.x = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(v_prime) < ALWAN_CORE_EPSILON,
        ALWAN_CORE_LITERAL(0.0),
        Y * (ALWAN_CORE_LITERAL(9.0) * u_prime) / (ALWAN_CORE_LITERAL(4.0) * v_prime));
    result.y = Y;
    result.z = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(v_prime) < ALWAN_CORE_EPSILON,
        ALWAN_CORE_LITERAL(0.0),
        Y * (ALWAN_CORE_LITERAL(12.0) - ALWAN_CORE_LITERAL(3.0) * u_prime - ALWAN_CORE_LITERAL(20.0) * v_prime) / (ALWAN_CORE_LITERAL(4.0) * v_prime));

    return result;
}

/* ================================================================
 * CIE Lab <-> LCh(ab)
 * ================================================================ */

/* CIE Lab -> LCh(ab) (cylindrical form) */
ALWAN_INLINE ALWAN_CORE_LCH ALWAN_CORE_FNVLIT(alwan_lab_to_lch)(ALWAN_CORE_LAB lab) {
    ALWAN_CORE_LCH result;
    result.L = lab.L;
    result.C = ALWAN_CORE_SQRT(lab.a * lab.a + lab.b * lab.b);
    ALWAN_CORE_T h_rad = ALWAN_CORE_ATAN2(lab.b, lab.a);
    ALWAN_CORE_T h_deg = h_rad * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;
    result.h = ALWAN_CORE_SELECT(h_deg < ALWAN_CORE_LITERAL(0.0), h_deg + ALWAN_CORE_LITERAL(360.0), h_deg);
    return result;
}

/* LCh(ab) -> CIE Lab */
ALWAN_INLINE ALWAN_CORE_LAB ALWAN_CORE_FNVLIT(alwan_lch_to_lab)(ALWAN_CORE_LCH lch) {
    ALWAN_CORE_LAB result;
    ALWAN_CORE_T h_rad = lch.h * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);
    result.L = lch.L;
    result.a = lch.C * ALWAN_CORE_COS(h_rad);
    result.b = lch.C * ALWAN_CORE_SIN(h_rad);
    return result;
}

/* ================================================================
 * CIE Luv <-> LCh(uv)
 * ================================================================ */

/* CIE Luv -> LCh(uv) (cylindrical form) */
ALWAN_INLINE ALWAN_CORE_LCHUV ALWAN_CORE_FNVLIT(alwan_luv_to_lchuv)(ALWAN_CORE_LUV luv) {
    ALWAN_CORE_LCHUV result;
    result.L = luv.L;
    result.C = ALWAN_CORE_SQRT(luv.u * luv.u + luv.v * luv.v);
    ALWAN_CORE_T h_rad = ALWAN_CORE_ATAN2(luv.v, luv.u);
    ALWAN_CORE_T h_deg = h_rad * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;
    result.h = ALWAN_CORE_SELECT(h_deg < ALWAN_CORE_LITERAL(0.0), h_deg + ALWAN_CORE_LITERAL(360.0), h_deg);
    return result;
}

/* LCh(uv) -> CIE Luv */
ALWAN_INLINE ALWAN_CORE_LUV ALWAN_CORE_FNVLIT(alwan_lchuv_to_luv)(ALWAN_CORE_LCHUV lchuv) {
    ALWAN_CORE_LUV result;
    ALWAN_CORE_T h_rad = lchuv.h * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);
    result.L = lchuv.L;
    result.u = lchuv.C * ALWAN_CORE_COS(h_rad);
    result.v = lchuv.C * ALWAN_CORE_SIN(h_rad);
    return result;
}

/* ================================================================
 * XYZ <-> CIE 1960 UCS
 * ================================================================ */

/* XYZ -> UCS (CIE 1960 uniform color space) */
ALWAN_INLINE ALWAN_CORE_UCS ALWAN_CORE_FNVLIT(alwan_xyz_to_ucs)(ALWAN_CORE_XYZ xyz) {
    ALWAN_CORE_UCS result;
    result.U = (ALWAN_CORE_LITERAL(2.0) / ALWAN_CORE_LITERAL(3.0)) * xyz.x;
    result.V = xyz.y;
    result.W = (ALWAN_CORE_LITERAL(0.5)) * (-xyz.x + ALWAN_CORE_LITERAL(3.0) * xyz.y + xyz.z);
    return result;
}

/* UCS -> XYZ */
ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_ucs_to_xyz)(ALWAN_CORE_UCS ucs) {
    ALWAN_CORE_XYZ result;
    result.x = (ALWAN_CORE_LITERAL(3.0) / ALWAN_CORE_LITERAL(2.0)) * ucs.U;
    result.y = ucs.V;
    result.z = ALWAN_CORE_LITERAL(2.0) * ucs.W + result.x - ALWAN_CORE_LITERAL(3.0) * ucs.V;
    return result;
}

/* ================================================================
 * XYZ <-> U*V*W* (CIE 1964)
 * ================================================================ */

/* XYZ -> U*V*W* */
ALWAN_INLINE ALWAN_CORE_UVW ALWAN_CORE_FNVLIT(alwan_xyz_to_uvw)(ALWAN_CORE_XYZ xyz, ALWAN_CORE_XYZ white) {
    ALWAN_CORE_UVW result;

    /* CIE 1960 UCS chromaticity (u, v) for sample */
    ALWAN_CORE_T sum = xyz.x + ALWAN_CORE_LITERAL(15.0) * xyz.y + ALWAN_CORE_LITERAL(3.0) * xyz.z;
    ALWAN_CORE_T u = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(sum) < ALWAN_CORE_EPSILON, ALWAN_CORE_LITERAL(0.0),
                                  (ALWAN_CORE_LITERAL(4.0) * xyz.x) / sum);
    ALWAN_CORE_T v = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(sum) < ALWAN_CORE_EPSILON, ALWAN_CORE_LITERAL(0.0),
                                  (ALWAN_CORE_LITERAL(6.0) * xyz.y) / sum);

    /* White point chromaticity */
    ALWAN_CORE_T sum_n = white.x + ALWAN_CORE_LITERAL(15.0) * white.y + ALWAN_CORE_LITERAL(3.0) * white.z;
    ALWAN_CORE_T un = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(sum_n) < ALWAN_CORE_EPSILON, ALWAN_CORE_LITERAL(0.0),
                                   (ALWAN_CORE_LITERAL(4.0) * white.x) / sum_n);
    ALWAN_CORE_T vn = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(sum_n) < ALWAN_CORE_EPSILON, ALWAN_CORE_LITERAL(0.0),
                                   (ALWAN_CORE_LITERAL(6.0) * white.y) / sum_n);

    /* W* = 25 * Yp^(1/3) - 17, where Yp is the luminance factor in percent [0,100].
     * Computed scale-invariantly as (Y/Yn)*100 so callers may pass XYZ on either
     * the [0,1] or the [0,100] scale and still match CIE 1964 / colour-science. */
    ALWAN_CORE_T Yp = ALWAN_CORE_SELECT(white.y < ALWAN_CORE_EPSILON, ALWAN_CORE_LITERAL(0.0),
                                  (xyz.y / white.y) * ALWAN_CORE_LITERAL(100.0));
    ALWAN_CORE_T W = ALWAN_CORE_SELECT(Yp < ALWAN_CORE_EPSILON,
                                  ALWAN_CORE_LITERAL(-17.0),
                                  ALWAN_CORE_LITERAL(25.0) * ALWAN_CORE_CBRT(Yp) - ALWAN_CORE_LITERAL(17.0));

    result.U = ALWAN_CORE_LITERAL(13.0) * W * (u - un);
    result.V = ALWAN_CORE_LITERAL(13.0) * W * (v - vn);
    result.W = W;
    return result;
}

/* U*V*W* -> XYZ */
ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_uvw_to_xyz)(ALWAN_CORE_UVW uvw, ALWAN_CORE_XYZ white) {
    ALWAN_CORE_XYZ result;

    /* Y from W* */
    ALWAN_CORE_T W_plus_17 = uvw.W + ALWAN_CORE_LITERAL(17.0);
    ALWAN_CORE_T Y_cbrt = ALWAN_CORE_SELECT(W_plus_17 < ALWAN_CORE_EPSILON, ALWAN_CORE_LITERAL(0.0),
                                       W_plus_17 / ALWAN_CORE_LITERAL(25.0));
    /* Y_cbrt^3 is the luminance factor in percent [0,100]; rescale to the caller's
     * XYZ scale via Yn so the round-trip with xyz_to_uvw is exact (inverse of *100). */
    ALWAN_CORE_T Yp = Y_cbrt * Y_cbrt * Y_cbrt;
    ALWAN_CORE_T Y = Yp * white.y * ALWAN_CORE_LITERAL(0.01);

    /* White point chromaticity */
    ALWAN_CORE_T sum_n = white.x + ALWAN_CORE_LITERAL(15.0) * white.y + ALWAN_CORE_LITERAL(3.0) * white.z;
    ALWAN_CORE_T un = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(sum_n) < ALWAN_CORE_EPSILON, ALWAN_CORE_LITERAL(0.0),
                                   (ALWAN_CORE_LITERAL(4.0) * white.x) / sum_n);
    ALWAN_CORE_T vn = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(sum_n) < ALWAN_CORE_EPSILON, ALWAN_CORE_LITERAL(0.0),
                                   (ALWAN_CORE_LITERAL(6.0) * white.y) / sum_n);

    /* Recover u, v */
    ALWAN_CORE_T W13 = ALWAN_CORE_LITERAL(13.0) * uvw.W;
    ALWAN_CORE_T u = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(uvw.W) < ALWAN_CORE_EPSILON, un,
                                  uvw.U / W13 + un);
    ALWAN_CORE_T v = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(uvw.W) < ALWAN_CORE_EPSILON, vn,
                                  uvw.V / W13 + vn);

    /* X and Z from u, v, Y. Invert CIE 1960 UCS u=4X/D, v=6Y/D (D=X+15Y+3Z):
     * D = 6Y/v, so X = u*D/4 = 6uY/(4v) and Z = (D - X - 15Y)/3 = (8 - 2u - 20v)*Y/(4v). */
    result.x = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(v) < ALWAN_CORE_EPSILON, ALWAN_CORE_LITERAL(0.0),
                            (ALWAN_CORE_LITERAL(6.0) * u * Y) / (ALWAN_CORE_LITERAL(4.0) * v));
    result.y = Y;
    result.z = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(v) < ALWAN_CORE_EPSILON, ALWAN_CORE_LITERAL(0.0),
                            ((ALWAN_CORE_LITERAL(8.0) - ALWAN_CORE_LITERAL(2.0) * u - ALWAN_CORE_LITERAL(20.0) * v) * Y) / (ALWAN_CORE_LITERAL(4.0) * v));

    return result;
}

/* ================================================================
 * Direct XYZ <-> Cylindrical Conversions
 * ================================================================ */

/* XYZ -> LCh(ab) (compose lab_v + lch_v) */
ALWAN_INLINE ALWAN_CORE_LCH ALWAN_CORE_FNVLIT(alwan_xyz_to_lch)(ALWAN_CORE_XYZ xyz, ALWAN_CORE_XYZ white) {
    return ALWAN_CORE_FNVLIT(alwan_lab_to_lch)(ALWAN_CORE_FNVLIT(alwan_xyz_to_lab)(xyz, white));
}

/* LCh(ab) -> XYZ */
ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_lch_to_xyz)(ALWAN_CORE_LCH lch, ALWAN_CORE_XYZ white) {
    return ALWAN_CORE_FNVLIT(alwan_lab_to_xyz)(ALWAN_CORE_FNVLIT(alwan_lch_to_lab)(lch), white);
}

/* XYZ -> LCh(uv) (compose luv_v + lchuv_v) */
ALWAN_INLINE ALWAN_CORE_LCHUV ALWAN_CORE_FNVLIT(alwan_xyz_to_lchuv)(ALWAN_CORE_XYZ xyz, ALWAN_CORE_XYZ white) {
    return ALWAN_CORE_FNVLIT(alwan_luv_to_lchuv)(ALWAN_CORE_FNVLIT(alwan_xyz_to_luv)(xyz, white));
}

/* LCh(uv) -> XYZ */
ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_lchuv_to_xyz)(ALWAN_CORE_LCHUV lchuv, ALWAN_CORE_XYZ white) {
    return ALWAN_CORE_FNVLIT(alwan_luv_to_xyz)(ALWAN_CORE_FNVLIT(alwan_lchuv_to_luv)(lchuv), white);
}

/* ================================================================
 * Delta E (Color Difference) Metrics
 * ================================================================ */

/* dE76: Euclidean distance in Lab */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_delta_e_76)(ALWAN_CORE_LAB lab1, ALWAN_CORE_LAB lab2) {
    ALWAN_CORE_T dL = lab1.L - lab2.L;
    ALWAN_CORE_T da = lab1.a - lab2.a;
    ALWAN_CORE_T db = lab1.b - lab2.b;
    return ALWAN_CORE_SQRT(dL * dL + da * da + db * db);
}

/* dE OK: Euclidean distance in Oklab space (CSS Color Level 4 JND criterion) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_delta_e_ok)(ALWAN_CORE_OKLAB a, ALWAN_CORE_OKLAB b) {
    ALWAN_CORE_T dL = a.L - b.L;
    ALWAN_CORE_T da = a.a - b.a;
    ALWAN_CORE_T db = a.b - b.b;
    return ALWAN_CORE_SQRT(dL * dL + da * da + db * db);
}

/* dE ITP: ITU-R BT.2124 HDR Color Difference in ICtCp space */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_delta_e_itp)(ALWAN_CORE_ICTCP ictcp1, ALWAN_CORE_ICTCP ictcp2, ALWAN_CORE_T scalar_factor) {
    ALWAN_CORE_T dI  = ictcp1.I  - ictcp2.I;
    ALWAN_CORE_T dCT = ictcp1.Ct - ictcp2.Ct;
    ALWAN_CORE_T dCP = ictcp1.Cp - ictcp2.Cp;
    return scalar_factor * ALWAN_CORE_SQRT(dI * dI + ALWAN_CORE_LITERAL(0.25) * dCT * dCT + dCP * dCP);
}

/* dE DIN99: Euclidean distance in DIN99 space */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_delta_e_din99)(ALWAN_CORE_DIN99 din99_1, ALWAN_CORE_DIN99 din99_2) {
    ALWAN_CORE_T dL = din99_1.L99 - din99_2.L99;
    ALWAN_CORE_T da = din99_1.a99 - din99_2.a99;
    ALWAN_CORE_T db = din99_1.b99 - din99_2.b99;
    return ALWAN_CORE_SQRT(dL * dL + da * da + db * db);
}

/* dE ZCAM: Euclidean distance in Jzazbz space */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_delta_e_zcam)(ALWAN_CORE_JZAZBZ jab1, ALWAN_CORE_JZAZBZ jab2) {
    ALWAN_CORE_T dJ = jab1.Jz - jab2.Jz;
    ALWAN_CORE_T da = jab1.az - jab2.az;
    ALWAN_CORE_T db = jab1.bz - jab2.bz;
    return ALWAN_CORE_SQRT(dJ * dJ + da * da + db * db);
}

/* dE94: CIE94 color difference (graphic arts defaults) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_delta_e_94)(ALWAN_CORE_LAB lab1, ALWAN_CORE_LAB lab2) {
    const ALWAN_CORE_T K1 = ALWAN_CORE_LITERAL(0.045);
    const ALWAN_CORE_T K2 = ALWAN_CORE_LITERAL(0.015);

    ALWAN_CORE_T dL = lab1.L - lab2.L;
    ALWAN_CORE_T C1 = ALWAN_CORE_SQRT(lab1.a * lab1.a + lab1.b * lab1.b);
    ALWAN_CORE_T C2 = ALWAN_CORE_SQRT(lab2.a * lab2.a + lab2.b * lab2.b);
    ALWAN_CORE_T dCab = C1 - C2;

    ALWAN_CORE_T da = lab1.a - lab2.a;
    ALWAN_CORE_T db = lab1.b - lab2.b;
    ALWAN_CORE_T dHab_sq = da * da + db * db - dCab * dCab;
    dHab_sq = ALWAN_CORE_SELECT(dHab_sq > ALWAN_CORE_LITERAL(0.0), dHab_sq, ALWAN_CORE_LITERAL(0.0));

    ALWAN_CORE_T SC = ALWAN_CORE_LITERAL(1.0) + K1 * C1;
    ALWAN_CORE_T SH = ALWAN_CORE_LITERAL(1.0) + K2 * C1;

    ALWAN_CORE_T term1 = dL;
    ALWAN_CORE_T term2 = dCab / SC;
    ALWAN_CORE_T term3 = ALWAN_CORE_SQRT(dHab_sq) / SH;

    return ALWAN_CORE_SQRT(term1 * term1 + term2 * term2 + term3 * term3);
}

/* dE CMC: CMC l:c color difference */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_delta_e_cmc)(ALWAN_CORE_LAB lab1, ALWAN_CORE_LAB lab2, ALWAN_CORE_T l, ALWAN_CORE_T c) {
    ALWAN_CORE_T dL = lab1.L - lab2.L;
    ALWAN_CORE_T C1 = ALWAN_CORE_SQRT(lab1.a * lab1.a + lab1.b * lab1.b);
    ALWAN_CORE_T C2 = ALWAN_CORE_SQRT(lab2.a * lab2.a + lab2.b * lab2.b);
    ALWAN_CORE_T dCab = C1 - C2;

    ALWAN_CORE_T da = lab1.a - lab2.a;
    ALWAN_CORE_T db = lab1.b - lab2.b;
    ALWAN_CORE_T dHab_sq = da * da + db * db - dCab * dCab;
    dHab_sq = ALWAN_CORE_SELECT(dHab_sq > ALWAN_CORE_LITERAL(0.0), dHab_sq, ALWAN_CORE_LITERAL(0.0));

    /* h1 in degrees [0, 360) */
    ALWAN_CORE_T h1_rad = ALWAN_CORE_ATAN2(lab1.b, lab1.a);
    h1_rad = ALWAN_CORE_SELECT(h1_rad < ALWAN_CORE_LITERAL(0.0), h1_rad + ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI, h1_rad);
    ALWAN_CORE_T h1 = h1_rad * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;

    /* SL */
    ALWAN_CORE_T SL = ALWAN_CORE_SELECT(lab1.L < ALWAN_CORE_LITERAL(16.0),
                                   ALWAN_CORE_LITERAL(0.511),
                                   (ALWAN_CORE_LITERAL(0.040975) * lab1.L) / (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_LITERAL(0.01765) * lab1.L));

    /* SC */
    ALWAN_CORE_T SC = (ALWAN_CORE_LITERAL(0.0638) * C1) / (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_LITERAL(0.0131) * C1) + ALWAN_CORE_LITERAL(0.638);

    /* F */
    ALWAN_CORE_T C1_4 = C1 * C1 * C1 * C1;
    ALWAN_CORE_T F = ALWAN_CORE_SQRT(C1_4 / (C1_4 + ALWAN_CORE_LITERAL(1900.0)));

    /* T */
    ALWAN_CORE_T T = ALWAN_CORE_SELECT(h1 >= ALWAN_CORE_LITERAL(164.0),
                      ALWAN_CORE_SELECT(h1 <= ALWAN_CORE_LITERAL(345.0),
                        ALWAN_CORE_LITERAL(0.56) + ALWAN_CORE_ABS(ALWAN_CORE_LITERAL(0.2) * ALWAN_CORE_COS((h1 + ALWAN_CORE_LITERAL(168.0)) * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0))),
                        ALWAN_CORE_LITERAL(0.36) + ALWAN_CORE_ABS(ALWAN_CORE_LITERAL(0.4) * ALWAN_CORE_COS((h1 + ALWAN_CORE_LITERAL(35.0)) * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0)))),
                      ALWAN_CORE_LITERAL(0.36) + ALWAN_CORE_ABS(ALWAN_CORE_LITERAL(0.4) * ALWAN_CORE_COS((h1 + ALWAN_CORE_LITERAL(35.0)) * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0))));

    ALWAN_CORE_T SH = SC * (F * T + ALWAN_CORE_LITERAL(1.0) - F);

    ALWAN_CORE_T term1 = dL / (l * SL);
    ALWAN_CORE_T term2 = dCab / (c * SC);
    ALWAN_CORE_T term3 = ALWAN_CORE_SQRT(dHab_sq) / SH;

    return ALWAN_CORE_SQRT(term1 * term1 + term2 * term2 + term3 * term3);
}

/* dE2000: CIEDE2000 color difference */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_delta_e_2000)(ALWAN_CORE_LAB lab1, ALWAN_CORE_LAB lab2) {
    ALWAN_CORE_T C1 = ALWAN_CORE_SQRT(lab1.a * lab1.a + lab1.b * lab1.b);
    ALWAN_CORE_T C2 = ALWAN_CORE_SQRT(lab2.a * lab2.a + lab2.b * lab2.b);
    ALWAN_CORE_T Cab_mean = (C1 + C2) / ALWAN_CORE_LITERAL(2.0);

    /* G (a' adjustment) */
    ALWAN_CORE_T Cab7 = Cab_mean * Cab_mean * Cab_mean * Cab_mean * Cab_mean * Cab_mean * Cab_mean;
    ALWAN_CORE_T G = ALWAN_CORE_LITERAL(0.5) * (ALWAN_CORE_LITERAL(1.0) - ALWAN_CORE_SQRT(Cab7 / (Cab7 + ALWAN_CORE_LITERAL(6103515625.0))));

    ALWAN_CORE_T a1p = (ALWAN_CORE_LITERAL(1.0) + G) * lab1.a;
    ALWAN_CORE_T a2p = (ALWAN_CORE_LITERAL(1.0) + G) * lab2.a;

    ALWAN_CORE_T C1p = ALWAN_CORE_SQRT(a1p * a1p + lab1.b * lab1.b);
    ALWAN_CORE_T C2p = ALWAN_CORE_SQRT(a2p * a2p + lab2.b * lab2.b);

    /* h'1, h'2 in degrees [0, 360) */
    ALWAN_CORE_T h1p_rad = ALWAN_CORE_ATAN2(lab1.b, a1p);
    h1p_rad = ALWAN_CORE_SELECT(h1p_rad < ALWAN_CORE_LITERAL(0.0), h1p_rad + ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI, h1p_rad);
    ALWAN_CORE_T h1p = h1p_rad * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;

    ALWAN_CORE_T h2p_rad = ALWAN_CORE_ATAN2(lab2.b, a2p);
    h2p_rad = ALWAN_CORE_SELECT(h2p_rad < ALWAN_CORE_LITERAL(0.0), h2p_rad + ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI, h2p_rad);
    ALWAN_CORE_T h2p = h2p_rad * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;

    /* dL', dC', dh' */
    ALWAN_CORE_T dLp = lab2.L - lab1.L;
    ALWAN_CORE_T dCp = C2p - C1p;

    ALWAN_CORE_T abs_dh = ALWAN_CORE_ABS(h2p - h1p);
    ALWAN_CORE_T achromatic = ALWAN_CORE_SELECT(C1p * C2p < ALWAN_CORE_EPSILON, ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(0.0));

    ALWAN_CORE_T dhp = ALWAN_CORE_SELECT(achromatic > ALWAN_CORE_LITERAL(0.5), ALWAN_CORE_LITERAL(0.0),
                        ALWAN_CORE_SELECT(abs_dh <= ALWAN_CORE_LITERAL(180.0), h2p - h1p,
                          ALWAN_CORE_SELECT(h2p - h1p > ALWAN_CORE_LITERAL(180.0), h2p - h1p - ALWAN_CORE_LITERAL(360.0),
                                       h2p - h1p + ALWAN_CORE_LITERAL(360.0))));

    ALWAN_CORE_T dHp = ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_SQRT(C1p * C2p) * ALWAN_CORE_SIN(dhp * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(360.0));

    /* Mean values */
    ALWAN_CORE_T Lpm = (lab1.L + lab2.L) / ALWAN_CORE_LITERAL(2.0);
    ALWAN_CORE_T Cpm = (C1p + C2p) / ALWAN_CORE_LITERAL(2.0);

    ALWAN_CORE_T h_sum = h1p + h2p;
    ALWAN_CORE_T Hpm = ALWAN_CORE_SELECT(achromatic > ALWAN_CORE_LITERAL(0.5), h_sum,
                        ALWAN_CORE_SELECT(abs_dh <= ALWAN_CORE_LITERAL(180.0), h_sum / ALWAN_CORE_LITERAL(2.0),
                          ALWAN_CORE_SELECT(h_sum < ALWAN_CORE_LITERAL(360.0),
                                       (h_sum + ALWAN_CORE_LITERAL(360.0)) / ALWAN_CORE_LITERAL(2.0),
                                       (h_sum - ALWAN_CORE_LITERAL(360.0)) / ALWAN_CORE_LITERAL(2.0))));

    /* Weighting functions */
    ALWAN_CORE_T T2k = ALWAN_CORE_LITERAL(1.0)
        - ALWAN_CORE_LITERAL(0.17) * ALWAN_CORE_COS((Hpm - ALWAN_CORE_LITERAL(30.0)) * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0))
        + ALWAN_CORE_LITERAL(0.24) * ALWAN_CORE_COS((ALWAN_CORE_LITERAL(2.0) * Hpm) * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0))
        + ALWAN_CORE_LITERAL(0.32) * ALWAN_CORE_COS((ALWAN_CORE_LITERAL(3.0) * Hpm + ALWAN_CORE_LITERAL(6.0)) * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0))
        - ALWAN_CORE_LITERAL(0.20) * ALWAN_CORE_COS((ALWAN_CORE_LITERAL(4.0) * Hpm - ALWAN_CORE_LITERAL(63.0)) * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0));

    ALWAN_CORE_T dTheta = ALWAN_CORE_LITERAL(30.0) * ALWAN_CORE_EXP(-((Hpm - ALWAN_CORE_LITERAL(275.0)) / ALWAN_CORE_LITERAL(25.0)) * ((Hpm - ALWAN_CORE_LITERAL(275.0)) / ALWAN_CORE_LITERAL(25.0)));

    ALWAN_CORE_T Cpm7 = Cpm * Cpm * Cpm * Cpm * Cpm * Cpm * Cpm;
    ALWAN_CORE_T RC = ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_SQRT(Cpm7 / (Cpm7 + ALWAN_CORE_LITERAL(6103515625.0)));

    ALWAN_CORE_T Lpm50sq = (Lpm - ALWAN_CORE_LITERAL(50.0)) * (Lpm - ALWAN_CORE_LITERAL(50.0));
    ALWAN_CORE_T SL = ALWAN_CORE_LITERAL(1.0) + (ALWAN_CORE_LITERAL(0.015) * Lpm50sq) / ALWAN_CORE_SQRT(ALWAN_CORE_LITERAL(20.0) + Lpm50sq);
    ALWAN_CORE_T SC = ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_LITERAL(0.045) * Cpm;
    ALWAN_CORE_T SH = ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_LITERAL(0.015) * Cpm * T2k;
    ALWAN_CORE_T RT = -ALWAN_CORE_SIN((ALWAN_CORE_LITERAL(2.0) * dTheta) * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0)) * RC;

    ALWAN_CORE_T t1 = dLp / SL;
    ALWAN_CORE_T t2 = dCp / SC;
    ALWAN_CORE_T t3 = dHp / SH;

    return ALWAN_CORE_SQRT(t1 * t1 + t2 * t2 + t3 * t3 + RT * t2 * t3);
}

/* dE HyAB: Hybrid Delta E */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_delta_e_hyab)(ALWAN_CORE_LAB lab1, ALWAN_CORE_LAB lab2) {
    ALWAN_CORE_T dL = lab1.L - lab2.L;
    ALWAN_CORE_T db = lab1.b - lab2.b;

    ALWAN_CORE_T C1 = ALWAN_CORE_SQRT(lab1.a * lab1.a + lab1.b * lab1.b);
    ALWAN_CORE_T C2 = ALWAN_CORE_SQRT(lab2.a * lab2.a + lab2.b * lab2.b);
    ALWAN_CORE_T Cab = (C1 + C2) / ALWAN_CORE_LITERAL(2.0);
    ALWAN_CORE_T Cab7 = Cab * Cab * Cab * Cab * Cab * Cab * Cab;
    ALWAN_CORE_T G = ALWAN_CORE_LITERAL(0.5) * (ALWAN_CORE_LITERAL(1.0) - ALWAN_CORE_SQRT(Cab7 / (Cab7 + ALWAN_CORE_LITERAL(6103515625.0))));

    ALWAN_CORE_T a1p = (ALWAN_CORE_LITERAL(1.0) + G) * lab1.a;
    ALWAN_CORE_T a2p = (ALWAN_CORE_LITERAL(1.0) + G) * lab2.a;

    ALWAN_CORE_T C1p = ALWAN_CORE_SQRT(a1p * a1p + lab1.b * lab1.b);
    ALWAN_CORE_T C2p = ALWAN_CORE_SQRT(a2p * a2p + lab2.b * lab2.b);
    ALWAN_CORE_T dCp = C1p - C2p;

    ALWAN_CORE_T dap = a1p - a2p;
    ALWAN_CORE_T dHp_sq = dap * dap + db * db - dCp * dCp;
    dHp_sq = ALWAN_CORE_SELECT(dHp_sq > ALWAN_CORE_LITERAL(0.0), dHp_sq, ALWAN_CORE_LITERAL(0.0));

    return ALWAN_CORE_SQRT(dL * dL + dCp * dCp + dHp_sq);
}

/* ================================================================
 * CAM-based Delta E (generic Jab with KL scaling)
 * ================================================================ */

/* Generic CAM Jab delta E: sqrt((dJ/KL)^2 + da^2 + db^2) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_delta_e_cam_jab)(ALWAN_CORE_CAM_JAB jab1, ALWAN_CORE_CAM_JAB jab2, ALWAN_CORE_T KL) {
    ALWAN_CORE_T dJ = (jab1.J - jab2.J) / KL;
    ALWAN_CORE_T da = jab1.a - jab2.a;
    ALWAN_CORE_T db = jab1.b - jab2.b;
    return ALWAN_CORE_SQRT(dJ * dJ + da * da + db * db);
}

/* dE CAM02-LCD (KL=0.77) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_delta_e_cam02_lcd)(ALWAN_CORE_CAM_JAB jab1, ALWAN_CORE_CAM_JAB jab2) {
    return ALWAN_CORE_FNVLIT(alwan_delta_e_cam_jab)(jab1, jab2, ALWAN_CORE_LITERAL(0.77));
}

/* dE CAM02-SCD (KL=1.24) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_delta_e_cam02_scd)(ALWAN_CORE_CAM_JAB jab1, ALWAN_CORE_CAM_JAB jab2) {
    return ALWAN_CORE_FNVLIT(alwan_delta_e_cam_jab)(jab1, jab2, ALWAN_CORE_LITERAL(1.24));
}

/* dE CAM16-LCD (KL=0.77) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_delta_e_cam16_lcd)(ALWAN_CORE_CAM_JAB jab1, ALWAN_CORE_CAM_JAB jab2) {
    return ALWAN_CORE_FNVLIT(alwan_delta_e_cam_jab)(jab1, jab2, ALWAN_CORE_LITERAL(0.77));
}

/* dE CAM16-SCD (KL=1.24) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_delta_e_cam16_scd)(ALWAN_CORE_CAM_JAB jab1, ALWAN_CORE_CAM_JAB jab2) {
    return ALWAN_CORE_FNVLIT(alwan_delta_e_cam_jab)(jab1, jab2, ALWAN_CORE_LITERAL(1.24));
}

/* dE CAM02-UCS (KL=1.0) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_delta_e_cam02_ucs)(ALWAN_CORE_CAM_JAB jab1, ALWAN_CORE_CAM_JAB jab2) {
    return ALWAN_CORE_FNVLIT(alwan_delta_e_cam_jab)(jab1, jab2, ALWAN_CORE_LITERAL(1.0));
}

/* dE CAM16-UCS (KL=1.0) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_delta_e_cam16_ucs)(ALWAN_CORE_CAM_JAB jab1, ALWAN_CORE_CAM_JAB jab2) {
    return ALWAN_CORE_FNVLIT(alwan_delta_e_cam_jab)(jab1, jab2, ALWAN_CORE_LITERAL(1.0));
}
