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

/* ----------------------------------------------------------------
 * IPT Transformation Data
 * ---------------------------------------------------------------- */

/* IPT nonlinearity exponent */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const IPT_V_EXPONENT = {
#include "../data/ipt_exponent.csv"
};

static alwan_scalar const IPT_V_XYZ_TO_LMS[9] = {
#include "../data/ipt_xyz_to_lms.csv"
};

static alwan_scalar const IPT_V_LMS_TO_XYZ[9] = {
#include "../data/ipt_lms_to_xyz.csv"
};

static alwan_scalar const IPT_V_LMS_P_TO_IPT[9] = {
#include "../data/ipt_lms_p_to_ipt.csv"
};

static alwan_scalar const IPT_V_IPT_TO_LMS_P[9] = {
#include "../data/ipt_ipt_to_lms_p.csv"
};
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
    alwan_scalar l = IPT_V_XYZ_TO_LMS[0] * xyz.x + IPT_V_XYZ_TO_LMS[1] * xyz.y + IPT_V_XYZ_TO_LMS[2] * xyz.z;
    alwan_scalar m = IPT_V_XYZ_TO_LMS[3] * xyz.x + IPT_V_XYZ_TO_LMS[4] * xyz.y + IPT_V_XYZ_TO_LMS[5] * xyz.z;
    alwan_scalar s = IPT_V_XYZ_TO_LMS[6] * xyz.x + IPT_V_XYZ_TO_LMS[7] * xyz.y + IPT_V_XYZ_TO_LMS[8] * xyz.z;

    /* Step 2: LMS -> LMS' via nonlinearity */
    alwan_scalar l_ = ipt_nonlinearity_v(l);
    alwan_scalar m_ = ipt_nonlinearity_v(m);
    alwan_scalar s_ = ipt_nonlinearity_v(s);

    /* Step 3: LMS' -> IPT via matrix */
    result.I = IPT_V_LMS_P_TO_IPT[0] * l_ + IPT_V_LMS_P_TO_IPT[1] * m_ + IPT_V_LMS_P_TO_IPT[2] * s_;
    result.P = IPT_V_LMS_P_TO_IPT[3] * l_ + IPT_V_LMS_P_TO_IPT[4] * m_ + IPT_V_LMS_P_TO_IPT[5] * s_;
    result.T = IPT_V_LMS_P_TO_IPT[6] * l_ + IPT_V_LMS_P_TO_IPT[7] * m_ + IPT_V_LMS_P_TO_IPT[8] * s_;

    return result;
}

/* ----------------------------------------------------------------
 * IPT -> XYZ (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_xyz alwan_ipt_to_xyz_v(alwan_ipt ipt) {
    alwan_xyz result;

    /* Step 1: IPT -> LMS' via inverse matrix */
    alwan_scalar l_ = IPT_V_IPT_TO_LMS_P[0] * ipt.I + IPT_V_IPT_TO_LMS_P[1] * ipt.P + IPT_V_IPT_TO_LMS_P[2] * ipt.T;
    alwan_scalar m_ = IPT_V_IPT_TO_LMS_P[3] * ipt.I + IPT_V_IPT_TO_LMS_P[4] * ipt.P + IPT_V_IPT_TO_LMS_P[5] * ipt.T;
    alwan_scalar s_ = IPT_V_IPT_TO_LMS_P[6] * ipt.I + IPT_V_IPT_TO_LMS_P[7] * ipt.P + IPT_V_IPT_TO_LMS_P[8] * ipt.T;

    /* Step 2: LMS' -> LMS via inverse nonlinearity */
    alwan_scalar l = ipt_nonlinearity_inverse_v(l_);
    alwan_scalar m = ipt_nonlinearity_inverse_v(m_);
    alwan_scalar s = ipt_nonlinearity_inverse_v(s_);

    /* Step 3: LMS -> XYZ via inverse matrix */
    result.x = IPT_V_LMS_TO_XYZ[0] * l + IPT_V_LMS_TO_XYZ[1] * m + IPT_V_LMS_TO_XYZ[2] * s;
    result.y = IPT_V_LMS_TO_XYZ[3] * l + IPT_V_LMS_TO_XYZ[4] * m + IPT_V_LMS_TO_XYZ[5] * s;
    result.z = IPT_V_LMS_TO_XYZ[6] * l + IPT_V_LMS_TO_XYZ[7] * m + IPT_V_LMS_TO_XYZ[8] * s;

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
