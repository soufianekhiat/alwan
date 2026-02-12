/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only IPT Color Space (Image Processing Transform)
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: Ebner & Fairchild (1998)
 */

#ifndef ALWAN_IPT_CORE_H
#define ALWAN_IPT_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"
#include "alwan_math_core.h"

/* ----------------------------------------------------------------
 * IPT Transformation Data
 * ---------------------------------------------------------------- */

/* IPT nonlinearity exponent */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const IPT_V_EXPONENT = {
#include "../data/ipt_exponent.csv"
};

ALWAN_CONSTEXPR alwan_mat3x3 IPT_V_XYZ_TO_LMS = {{
#include "../data/ipt_xyz_to_lms.csv"
}};

ALWAN_CONSTEXPR alwan_mat3x3 IPT_V_LMS_TO_XYZ = {{
#include "../data/ipt_lms_to_xyz.csv"
}};

ALWAN_CONSTEXPR alwan_mat3x3 IPT_V_LMS_P_TO_IPT = {{
#include "../data/ipt_lms_p_to_ipt.csv"
}};

ALWAN_CONSTEXPR alwan_mat3x3 IPT_V_IPT_TO_LMS_P = {{
#include "../data/ipt_ipt_to_lms_p.csv"
}};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * IPT Nonlinearity Helpers
 * ---------------------------------------------------------------- */

/* Apply IPT nonlinearity: f(x) = sign(x) * |x|^0.43 */
ALWAN_INLINE alwan_scalar ipt_nonlinearity_v(alwan_scalar x) {
    return ALWAN_SELECT(x >= ALWAN_LITERAL(0.0),
                        ALWAN_POW(x, IPT_V_EXPONENT),
                        -ALWAN_POW(-x, IPT_V_EXPONENT));
}

/* Apply inverse IPT nonlinearity: f_inv(x) = sign(x) * |x|^(1/0.43) */
ALWAN_INLINE alwan_scalar ipt_nonlinearity_inverse_v(alwan_scalar x) {
    alwan_scalar inv_exponent = ALWAN_LITERAL(1.0) / IPT_V_EXPONENT;
    return ALWAN_SELECT(x >= ALWAN_LITERAL(0.0),
                        ALWAN_POW(x, inv_exponent),
                        -ALWAN_POW(-x, inv_exponent));
}

/* ----------------------------------------------------------------
 * XYZ -> IPT (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_ipt alwan_xyz_to_ipt_v(alwan_xyz xyz) {
    alwan_ipt result;

    /* Step 1: XYZ -> LMS via matrix */
    alwan_vec3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    alwan_vec3 lms_v = alwan_mat3_mulv_v(IPT_V_XYZ_TO_LMS, xyz_v);
    alwan_scalar l = lms_v.v[0];
    alwan_scalar m = lms_v.v[1];
    alwan_scalar s = lms_v.v[2];

    /* Step 2: LMS -> LMS' via nonlinearity */
    alwan_scalar l_ = ipt_nonlinearity_v(l);
    alwan_scalar m_ = ipt_nonlinearity_v(m);
    alwan_scalar s_ = ipt_nonlinearity_v(s);

    /* Step 3: LMS' -> IPT via matrix */
    alwan_vec3 lms_p_v = {{l_, m_, s_}};
    alwan_vec3 ipt_v = alwan_mat3_mulv_v(IPT_V_LMS_P_TO_IPT, lms_p_v);
    result.I = ipt_v.v[0];
    result.P = ipt_v.v[1];
    result.T = ipt_v.v[2];

    return result;
}

/* ----------------------------------------------------------------
 * IPT -> XYZ (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_xyz alwan_ipt_to_xyz_v(alwan_ipt ipt) {
    alwan_xyz result;

    /* Step 1: IPT -> LMS' via inverse matrix */
    alwan_vec3 ipt_v = {{ipt.I, ipt.P, ipt.T}};
    alwan_vec3 lms_p_v = alwan_mat3_mulv_v(IPT_V_IPT_TO_LMS_P, ipt_v);
    alwan_scalar l_ = lms_p_v.v[0];
    alwan_scalar m_ = lms_p_v.v[1];
    alwan_scalar s_ = lms_p_v.v[2];

    /* Step 2: LMS' -> LMS via inverse nonlinearity */
    alwan_scalar l = ipt_nonlinearity_inverse_v(l_);
    alwan_scalar m = ipt_nonlinearity_inverse_v(m_);
    alwan_scalar s = ipt_nonlinearity_inverse_v(s_);

    /* Step 3: LMS -> XYZ via inverse matrix */
    alwan_vec3 lms_v = {{l, m, s}};
    alwan_vec3 xyz_v = alwan_mat3_mulv_v(IPT_V_LMS_TO_XYZ, lms_v);
    result.x = xyz_v.v[0];
    result.y = xyz_v.v[1];
    result.z = xyz_v.v[2];

    return result;
}

/* ----------------------------------------------------------------
 * IPT -> IPTch (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_iptch alwan_ipt_to_iptch_v(alwan_ipt ipt) {
    alwan_iptch result;

    /* I stays same */
    result.I = ipt.I;

    /* C = sqrt(P*P + T*T) */
    result.C = ALWAN_SQRT(ipt.P * ipt.P + ipt.T * ipt.T);

    /* h = atan2(T, P) */
    result.h = ALWAN_ATAN2(ipt.T, ipt.P);

    return result;
}

/* ----------------------------------------------------------------
 * IPTch -> IPT (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_ipt alwan_iptch_to_ipt_v(alwan_iptch iptch) {
    alwan_ipt result;

    /* I stays same */
    result.I = iptch.I;

    /* P = C * cos(h) */
    result.P = iptch.C * ALWAN_COS(iptch.h);

    /* T = C * sin(h) */
    result.T = iptch.C * ALWAN_SIN(iptch.h);

    return result;
}

#endif /* ALWAN_IPT_CORE_H */
