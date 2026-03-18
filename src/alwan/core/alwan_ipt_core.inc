/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only IPT Color Space (Image Processing Transform) (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: Ebner & Fairchild (1998)
 */

/* ----------------------------------------------------------------
 * IPT Transformation Data
 * ---------------------------------------------------------------- */

/* IPT nonlinearity exponent */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(IPT_V_EXPONENT) = {
#include "../data/ipt_exponent.csv"
};

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(IPT_V_XYZ_TO_LMS) = {{
#include "../data/ipt_xyz_to_lms.csv"
}};

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(IPT_V_LMS_TO_XYZ) = {{
#include "../data/ipt_lms_to_xyz.csv"
}};

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(IPT_V_LMS_P_TO_IPT) = {{
#include "../data/ipt_lms_p_to_ipt.csv"
}};

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(IPT_V_IPT_TO_LMS_P) = {{
#include "../data/ipt_ipt_to_lms_p.csv"
}};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * IPT Nonlinearity Helpers
 * ---------------------------------------------------------------- */

/* Apply IPT nonlinearity: f(x) = sign(x) * |x|^0.43 */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(ipt_nonlinearity)(ALWAN_CORE_T x) {
    return ALWAN_CORE_SELECT(x >= ALWAN_CORE_LITERAL(0.0),
                        ALWAN_CORE_POW(x, ALWAN_CORE_FNLIT(IPT_V_EXPONENT)),
                        -ALWAN_CORE_POW(-x, ALWAN_CORE_FNLIT(IPT_V_EXPONENT)));
}

/* Apply inverse IPT nonlinearity: f_inv(x) = sign(x) * |x|^(1/0.43) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(ipt_nonlinearity_inverse)(ALWAN_CORE_T x) {
    ALWAN_CORE_T inv_exponent = ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_FNLIT(IPT_V_EXPONENT);
    return ALWAN_CORE_SELECT(x >= ALWAN_CORE_LITERAL(0.0),
                        ALWAN_CORE_POW(x, inv_exponent),
                        -ALWAN_CORE_POW(-x, inv_exponent));
}

/* ----------------------------------------------------------------
 * XYZ -> IPT (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_IPT ALWAN_CORE_FNVLIT(alwan_xyz_to_ipt)(ALWAN_CORE_XYZ xyz) {
    ALWAN_CORE_IPT result;

    /* Step 1: XYZ -> LMS via matrix */
    ALWAN_CORE_VEC3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    ALWAN_CORE_VEC3 lms_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(IPT_V_XYZ_TO_LMS), xyz_v);
    ALWAN_CORE_T l = lms_v.v[0];
    ALWAN_CORE_T m = lms_v.v[1];
    ALWAN_CORE_T s = lms_v.v[2];

    /* Step 2: LMS -> LMS' via nonlinearity */
    ALWAN_CORE_T l_ = ALWAN_CORE_FNVLIT(ipt_nonlinearity)(l);
    ALWAN_CORE_T m_ = ALWAN_CORE_FNVLIT(ipt_nonlinearity)(m);
    ALWAN_CORE_T s_ = ALWAN_CORE_FNVLIT(ipt_nonlinearity)(s);

    /* Step 3: LMS' -> IPT via matrix */
    ALWAN_CORE_VEC3 lms_p_v = {{l_, m_, s_}};
    ALWAN_CORE_VEC3 ipt_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(IPT_V_LMS_P_TO_IPT), lms_p_v);
    result.I = ipt_v.v[0];
    result.P = ipt_v.v[1];
    result.T = ipt_v.v[2];

    return result;
}

/* ----------------------------------------------------------------
 * IPT -> XYZ (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_ipt_to_xyz)(ALWAN_CORE_IPT ipt) {
    ALWAN_CORE_XYZ result;

    /* Step 1: IPT -> LMS' via inverse matrix */
    ALWAN_CORE_VEC3 ipt_v = {{ipt.I, ipt.P, ipt.T}};
    ALWAN_CORE_VEC3 lms_p_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(IPT_V_IPT_TO_LMS_P), ipt_v);
    ALWAN_CORE_T l_ = lms_p_v.v[0];
    ALWAN_CORE_T m_ = lms_p_v.v[1];
    ALWAN_CORE_T s_ = lms_p_v.v[2];

    /* Step 2: LMS' -> LMS via inverse nonlinearity */
    ALWAN_CORE_T l = ALWAN_CORE_FNVLIT(ipt_nonlinearity_inverse)(l_);
    ALWAN_CORE_T m = ALWAN_CORE_FNVLIT(ipt_nonlinearity_inverse)(m_);
    ALWAN_CORE_T s = ALWAN_CORE_FNVLIT(ipt_nonlinearity_inverse)(s_);

    /* Step 3: LMS -> XYZ via inverse matrix */
    ALWAN_CORE_VEC3 lms_v = {{l, m, s}};
    ALWAN_CORE_VEC3 xyz_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(IPT_V_LMS_TO_XYZ), lms_v);
    result.x = xyz_v.v[0];
    result.y = xyz_v.v[1];
    result.z = xyz_v.v[2];

    return result;
}

/* ----------------------------------------------------------------
 * IPT -> IPTch (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_IPTCH ALWAN_CORE_FNVLIT(alwan_ipt_to_iptch)(ALWAN_CORE_IPT ipt) {
    ALWAN_CORE_IPTCH result;

    /* I stays same */
    result.I = ipt.I;

    /* C = sqrt(P*P + T*T) */
    result.C = ALWAN_CORE_SQRT(ipt.P * ipt.P + ipt.T * ipt.T);

    /* h = atan2(T, P) */
    result.h = ALWAN_CORE_ATAN2(ipt.T, ipt.P);

    return result;
}

/* ----------------------------------------------------------------
 * IPTch -> IPT (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_IPT ALWAN_CORE_FNVLIT(alwan_iptch_to_ipt)(ALWAN_CORE_IPTCH iptch) {
    ALWAN_CORE_IPT result;

    /* I stays same */
    result.I = iptch.I;

    /* P = C * cos(h) */
    result.P = iptch.C * ALWAN_CORE_COS(iptch.h);

    /* T = C * sin(h) */
    result.T = iptch.C * ALWAN_CORE_SIN(iptch.h);

    return result;
}
