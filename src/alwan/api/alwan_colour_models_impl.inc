/*
 * Yrg, IPT Ragoo 2021, sUCS, Izazbz and Hunter Rdab, instantiated per precision by
 * alwan_colour_models.c. Each follows colour-science's function of the same name in its
 * order of operations, with its signed power, sign(a) |a|^p, wherever it uses spow.
 */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(CM_KIRK_XYZ_TO_LMS)[9] = {
#include "../data/matrices/kirk2019_xyz_to_lms.csv"
};
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(CM_KIRK_LMS_TO_XYZ)[9] = {
#include "../data/matrices/kirk2019_lms_to_xyz.csv"
};
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(CM_IPT_XYZ_TO_LMS)[9] = {
#include "../data/matrices/xyz_to_lms_ipt.csv"
};
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(CM_IPT_LMS_TO_XYZ)[9] = {
#include "../data/matrices/lms_to_xyz_ipt.csv"
};
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(CM_RAGOO_XYZ_TO_LMS)[9] = {
#include "../data/matrices/ragoo2021_xyz_to_lms.csv"
};
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(CM_RAGOO_LMS_TO_XYZ)[9] = {
#include "../data/matrices/ragoo2021_lms_to_xyz.csv"
};
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(CM_RAGOO_LMS_P_TO_IPT)[9] = {
#include "../data/matrices/ragoo2021_lms_p_to_ipt.csv"
};
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(CM_RAGOO_IPT_TO_LMS_P)[9] = {
#include "../data/matrices/ragoo2021_ipt_to_lms_p.csv"
};
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(CM_SUCS_LMS_P_TO_IAB)[9] = {
#include "../data/matrices/sucs_lms_p_to_iab.csv"
};
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(CM_SUCS_IAB_TO_LMS_P)[9] = {
#include "../data/matrices/sucs_iab_to_lms_p.csv"
};
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(CM_JZ_XYZ_TO_LMS)[9] = {
#include "../data/matrices/jzazbz_xyz_to_lms.csv"
};
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(CM_JZ_LMS_TO_XYZ)[9] = {
#include "../data/matrices/jzazbz_lms_to_xyz.csv"
};
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(CM_IZ17_FWD)[9] = {
#include "../data/matrices/jzazbz_lms_p_to_izazbz.csv"
};
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(CM_IZ17_INV)[9] = {
#include "../data/matrices/jzazbz_izazbz_to_lms_p.csv"
};
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(CM_IZ21_FWD)[9] = {
#include "../data/matrices/jzazbz_lms_p_to_izazbz_2021.csv"
};
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(CM_IZ21_INV)[9] = {
#include "../data/matrices/jzazbz_izazbz_to_lms_p_2021.csv"
};
ALWAN_DIAG_POP

static void ALWAN_CORE_FNLIT(cm_mul)(ALWAN_CORE_T *o, ALWAN_CORE_T const *m, ALWAN_CORE_T a, ALWAN_CORE_T b,
                                     ALWAN_CORE_T c) {
    o[0] = m[0] * a + m[1] * b + m[2] * c;
    o[1] = m[3] * a + m[4] * b + m[5] * c;
    o[2] = m[6] * a + m[7] * b + m[8] * c;
}

/* colour-science's spow: sign(a) |a|^p, 0 at 0. */
static ALWAN_CORE_T ALWAN_CORE_FNLIT(cm_spow)(ALWAN_CORE_T a, ALWAN_CORE_T p) {
    if (a == ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(0.0);
    return a < ALWAN_CORE_LITERAL(0.0) ? -ALWAN_CORE_POW(-a, p) : ALWAN_CORE_POW(a, p);
}

/* ---------------------------------------------------------------- Yrg (Kirk 2019) */

alwan_status ALWAN_CORE_FNLIT(alwan_xyz_to_yrg)(ALWAN_CORE_VEC3 *yrg_out, ALWAN_CORE_XYZ const *xyz) {
    ALWAN_CORE_T lms[3], a, l = ALWAN_CORE_LITERAL(0.0), m = ALWAN_CORE_LITERAL(0.0);
    if (!yrg_out || !xyz) return ALWAN_E_INVALID;
    ALWAN_CORE_FNLIT(cm_mul)(lms, ALWAN_CORE_FNLIT(CM_KIRK_XYZ_TO_LMS), xyz->x, xyz->y, xyz->z);
    a = lms[0] + lms[1] + lms[2];
    if (a != ALWAN_CORE_LITERAL(0.0)) {
        l = lms[0] / a;
        m = lms[1] / a;
    }
    yrg_out->v[0] = ALWAN_CORE_LITERAL(0.68990272) * lms[0] + ALWAN_CORE_LITERAL(0.34832189) * lms[1];
    yrg_out->v[1] = ALWAN_CORE_LITERAL(1.0671) * l - ALWAN_CORE_LITERAL(0.6873) * m + ALWAN_CORE_LITERAL(0.02062);
    yrg_out->v[2] = ALWAN_CORE_LITERAL(-0.0362) * l + ALWAN_CORE_LITERAL(1.7182) * m - ALWAN_CORE_LITERAL(0.05155);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_yrg_to_xyz)(ALWAN_CORE_XYZ *xyz_out, ALWAN_CORE_VEC3 const *yrg) {
    ALWAN_CORE_T l, m, d, a, lms[3], o[3];
    if (!xyz_out || !yrg) return ALWAN_E_INVALID;
    l = ALWAN_CORE_LITERAL(0.95) * yrg->v[1] + ALWAN_CORE_LITERAL(0.38) * yrg->v[2];
    m = ALWAN_CORE_LITERAL(0.02) * yrg->v[1] + ALWAN_CORE_LITERAL(0.59) * yrg->v[2] + ALWAN_CORE_LITERAL(0.03);
    d = ALWAN_CORE_LITERAL(0.68990272) * l + ALWAN_CORE_LITERAL(0.34832189) * m;
    if (d == ALWAN_CORE_LITERAL(0.0)) return ALWAN_E_INVALID;
    a = yrg->v[0] / d;
    lms[0] = l * a;
    lms[1] = m * a;
    lms[2] = (ALWAN_CORE_LITERAL(1.0) - l - m) * a;
    ALWAN_CORE_FNLIT(cm_mul)(o, ALWAN_CORE_FNLIT(CM_KIRK_LMS_TO_XYZ), lms[0], lms[1], lms[2]);
    xyz_out->x = o[0];
    xyz_out->y = o[1];
    xyz_out->z = o[2];
    return ALWAN_OK;
}

/* ---------------------------------------------------------------- IPT Ragoo 2021 and sUCS */

/* colour-science's XYZ_to_Iab and Iab_to_XYZ: a matrix to LMS, a signed power, a matrix. */
static void ALWAN_CORE_FNLIT(cm_iab)(ALWAN_CORE_T *o, ALWAN_CORE_XYZ const *xyz, ALWAN_CORE_T const *m1, ALWAN_CORE_T p,
                                     ALWAN_CORE_T const *m2) {
    ALWAN_CORE_T lms[3];
    ALWAN_CORE_FNLIT(cm_mul)(lms, m1, xyz->x, xyz->y, xyz->z);
    ALWAN_CORE_FNLIT(cm_mul)(o, m2, ALWAN_CORE_FNLIT(cm_spow)(lms[0], p), ALWAN_CORE_FNLIT(cm_spow)(lms[1], p),
                             ALWAN_CORE_FNLIT(cm_spow)(lms[2], p));
}

static void ALWAN_CORE_FNLIT(cm_iab_inv)(ALWAN_CORE_XYZ *xyz_out, ALWAN_CORE_T i, ALWAN_CORE_T a, ALWAN_CORE_T b,
                                         ALWAN_CORE_T const *m1, ALWAN_CORE_T p, ALWAN_CORE_T const *m2) {
    ALWAN_CORE_T lp[3], o[3];
    ALWAN_CORE_FNLIT(cm_mul)(lp, m1, i, a, b);
    ALWAN_CORE_FNLIT(cm_mul)(o, m2, ALWAN_CORE_FNLIT(cm_spow)(lp[0], p), ALWAN_CORE_FNLIT(cm_spow)(lp[1], p),
                             ALWAN_CORE_FNLIT(cm_spow)(lp[2], p));
    xyz_out->x = o[0];
    xyz_out->y = o[1];
    xyz_out->z = o[2];
}

alwan_status ALWAN_CORE_FNLIT(alwan_xyz_to_ipt_ragoo2021)(ALWAN_CORE_IPT *ipt_out, ALWAN_CORE_XYZ const *xyz) {
    ALWAN_CORE_T o[3];
    if (!ipt_out || !xyz) return ALWAN_E_INVALID;
    ALWAN_CORE_FNLIT(cm_iab)(o, xyz, ALWAN_CORE_FNLIT(CM_RAGOO_XYZ_TO_LMS), ALWAN_CORE_LITERAL(0.4071),
                             ALWAN_CORE_FNLIT(CM_RAGOO_LMS_P_TO_IPT));
    ipt_out->I = o[0];
    ipt_out->P = o[1];
    ipt_out->T = o[2];
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_ipt_ragoo2021_to_xyz)(ALWAN_CORE_XYZ *xyz_out, ALWAN_CORE_IPT const *ipt) {
    if (!xyz_out || !ipt) return ALWAN_E_INVALID;
    ALWAN_CORE_FNLIT(cm_iab_inv)(xyz_out, ipt->I, ipt->P, ipt->T, ALWAN_CORE_FNLIT(CM_RAGOO_IPT_TO_LMS_P),
                                 ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(0.4071), ALWAN_CORE_FNLIT(CM_RAGOO_LMS_TO_XYZ));
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_xyz_to_sucs)(ALWAN_CORE_VEC3 *iab_out, ALWAN_CORE_XYZ const *xyz) {
    if (!iab_out || !xyz) return ALWAN_E_INVALID;
    ALWAN_CORE_FNLIT(cm_iab)(iab_out->v, xyz, ALWAN_CORE_FNLIT(CM_IPT_XYZ_TO_LMS), ALWAN_CORE_LITERAL(0.43),
                             ALWAN_CORE_FNLIT(CM_SUCS_LMS_P_TO_IAB));
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_sucs_to_xyz)(ALWAN_CORE_XYZ *xyz_out, ALWAN_CORE_VEC3 const *iab) {
    if (!xyz_out || !iab) return ALWAN_E_INVALID;
    ALWAN_CORE_FNLIT(cm_iab_inv)(xyz_out, iab->v[0], iab->v[1], iab->v[2], ALWAN_CORE_FNLIT(CM_SUCS_IAB_TO_LMS_P),
                                 ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(0.43), ALWAN_CORE_FNLIT(CM_IPT_LMS_TO_XYZ));
    return ALWAN_OK;
}

/* ---------------------------------------------------------------- Izazbz */

/* Safdar 2017's constants: ST 2084's with m_2 = 1.7 x 2523 / 32, written as
 * colour-science computes them. */
#define CM_IZ_B  ALWAN_CORE_LITERAL(1.15)
#define CM_IZ_G  ALWAN_CORE_LITERAL(0.66)
#define CM_IZ_C1 ALWAN_CORE_LITERAL(0.8359375)
#define CM_IZ_C2 ALWAN_CORE_LITERAL(18.8515625)
#define CM_IZ_C3 ALWAN_CORE_LITERAL(18.6875)
#define CM_IZ_M1 ALWAN_CORE_LITERAL(0.1593017578125)
#define CM_IZ_M2 (ALWAN_CORE_LITERAL(1.7) * ALWAN_CORE_LITERAL(2523.0) / ALWAN_CORE_LITERAL(32.0))
#define CM_IZ_D0_2021 ALWAN_CORE_LITERAL(3.7035226210190005e-11)

alwan_status ALWAN_CORE_FNLIT(alwan_xyz_to_izazbz)(ALWAN_CORE_VEC3 *izazbz_out, ALWAN_CORE_XYZ const *xyz_d65,
                                                   alwan_izazbz_method method) {
    ALWAN_CORE_T lms[3], lp[3];
    int i;
    if (!izazbz_out || !xyz_d65) return ALWAN_E_INVALID;
    if ((int)method != (int)ALWAN_IZAZBZ_SAFDAR2017 && (int)method != (int)ALWAN_IZAZBZ_SAFDAR2021) {
        return ALWAN_E_INVALID;
    }
    ALWAN_CORE_FNLIT(cm_mul)(lms, ALWAN_CORE_FNLIT(CM_JZ_XYZ_TO_LMS),
                             CM_IZ_B * xyz_d65->x - (CM_IZ_B - ALWAN_CORE_LITERAL(1.0)) * xyz_d65->z,
                             CM_IZ_G * xyz_d65->y - (CM_IZ_G - ALWAN_CORE_LITERAL(1.0)) * xyz_d65->x, xyz_d65->z);
    for (i = 0; i < 3; i++) {
        ALWAN_CORE_T const y_p = ALWAN_CORE_FNLIT(cm_spow)(lms[i] / ALWAN_CORE_LITERAL(10000.0), CM_IZ_M1);
        lp[i] = ALWAN_CORE_FNLIT(cm_spow)((CM_IZ_C1 + CM_IZ_C2 * y_p) / (CM_IZ_C3 * y_p + ALWAN_CORE_LITERAL(1.0)),
                                          CM_IZ_M2);
    }
    if ((int)method == (int)ALWAN_IZAZBZ_SAFDAR2017) {
        ALWAN_CORE_FNLIT(cm_mul)(izazbz_out->v, ALWAN_CORE_FNLIT(CM_IZ17_FWD), lp[0], lp[1], lp[2]);
    } else {
        ALWAN_CORE_FNLIT(cm_mul)(izazbz_out->v, ALWAN_CORE_FNLIT(CM_IZ21_FWD), lp[0], lp[1], lp[2]);
        izazbz_out->v[0] -= CM_IZ_D0_2021;
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_izazbz_to_xyz)(ALWAN_CORE_XYZ *xyz_out, ALWAN_CORE_VEC3 const *izazbz,
                                                   alwan_izazbz_method method) {
    ALWAN_CORE_T lp[3], lms[3], xp[3], x;
    int i;
    if (!xyz_out || !izazbz) return ALWAN_E_INVALID;
    if ((int)method == (int)ALWAN_IZAZBZ_SAFDAR2017) {
        ALWAN_CORE_FNLIT(cm_mul)(lp, ALWAN_CORE_FNLIT(CM_IZ17_INV), izazbz->v[0], izazbz->v[1], izazbz->v[2]);
    } else if ((int)method == (int)ALWAN_IZAZBZ_SAFDAR2021) {
        ALWAN_CORE_FNLIT(cm_mul)(lp, ALWAN_CORE_FNLIT(CM_IZ21_INV), izazbz->v[0] + CM_IZ_D0_2021, izazbz->v[1],
                                 izazbz->v[2]);
    } else {
        return ALWAN_E_INVALID;
    }
    for (i = 0; i < 3; i++) {
        ALWAN_CORE_T const v_p = ALWAN_CORE_FNLIT(cm_spow)(lp[i], ALWAN_CORE_LITERAL(1.0) / CM_IZ_M2);
        ALWAN_CORE_T n = v_p - CM_IZ_C1;
        if (n < ALWAN_CORE_LITERAL(0.0)) n = ALWAN_CORE_LITERAL(0.0);
        lms[i] = ALWAN_CORE_LITERAL(10000.0) *
                 ALWAN_CORE_FNLIT(cm_spow)(n / (CM_IZ_C2 - CM_IZ_C3 * v_p), ALWAN_CORE_LITERAL(1.0) / CM_IZ_M1);
    }
    ALWAN_CORE_FNLIT(cm_mul)(xp, ALWAN_CORE_FNLIT(CM_JZ_LMS_TO_XYZ), lms[0], lms[1], lms[2]);
    x = (xp[0] + (CM_IZ_B - ALWAN_CORE_LITERAL(1.0)) * xp[2]) / CM_IZ_B;
    xyz_out->x = x;
    xyz_out->y = (xp[1] + (CM_IZ_G - ALWAN_CORE_LITERAL(1.0)) * x) / CM_IZ_G;
    xyz_out->z = xp[2];
    return ALWAN_OK;
}

#undef CM_IZ_B
#undef CM_IZ_G
#undef CM_IZ_C1
#undef CM_IZ_C2
#undef CM_IZ_C3
#undef CM_IZ_M1
#undef CM_IZ_M2
#undef CM_IZ_D0_2021

/* ---------------------------------------------------------------- Hunter Rdab */

/* The white and K_a, K_b colour-science's XYZ_to_Hunter_Rdab resolves: Hunter's D65 table
 * and its K_ab when xyz_n is NULL, K_ab by Hunter 1966 from an xyz_n given alone. */
static alwan_status ALWAN_CORE_FNLIT(cm_rdab_white)(ALWAN_CORE_T *w, ALWAN_CORE_XYZ const *xyz_n,
                                                   ALWAN_CORE_VEC2 const *k_ab) {
    if (xyz_n) {
        if (!(xyz_n->x > ALWAN_CORE_LITERAL(0.0) && xyz_n->y > ALWAN_CORE_LITERAL(0.0) &&
              xyz_n->z > ALWAN_CORE_LITERAL(0.0))) {
            return ALWAN_E_INVALID;
        }
        w[0] = xyz_n->x;
        w[1] = xyz_n->y;
        w[2] = xyz_n->z;
    } else {
        w[0] = ALWAN_CORE_LITERAL(95.02);
        w[1] = ALWAN_CORE_LITERAL(100.0);
        w[2] = ALWAN_CORE_LITERAL(108.82);
    }
    if (k_ab) {
        w[3] = k_ab->v[0];
        w[4] = k_ab->v[1];
    } else if (!xyz_n) {
        w[3] = ALWAN_CORE_LITERAL(172.3);
        w[4] = ALWAN_CORE_LITERAL(67.2);
    } else {
        ALWAN_CORE_VEC2 const k = ALWAN_CORE_FNVLIT(alwan_hunter_coefficients)(*xyz_n);
        w[3] = k.v[0];
        w[4] = k.v[1];
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_xyz_to_hunter_rdab)(ALWAN_CORE_VEC3 *rdab_out, ALWAN_CORE_XYZ const *xyz,
                                                        ALWAN_CORE_XYZ const *xyz_n, ALWAN_CORE_VEC2 const *k_ab) {
    ALWAN_CORE_T w[5], f, y_yn;
    if (!rdab_out || !xyz) return ALWAN_E_INVALID;
    if (ALWAN_CORE_FNLIT(cm_rdab_white)(w, xyz_n, k_ab) != ALWAN_OK) return ALWAN_E_INVALID;
    f = ALWAN_CORE_LITERAL(0.51) * ((ALWAN_CORE_LITERAL(21.0) + ALWAN_CORE_LITERAL(0.2) * xyz->y) /
                                    (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_LITERAL(0.2) * xyz->y));
    y_yn = xyz->y / w[1];
    rdab_out->v[0] = xyz->y;
    rdab_out->v[1] = w[3] * f * (xyz->x / w[0] - y_yn);
    rdab_out->v[2] = w[4] * f * (y_yn - xyz->z / w[2]);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_hunter_rdab_to_xyz)(ALWAN_CORE_XYZ *xyz_out, ALWAN_CORE_VEC3 const *rdab,
                                                        ALWAN_CORE_XYZ const *xyz_n, ALWAN_CORE_VEC2 const *k_ab) {
    ALWAN_CORE_T w[5], f, rd_yn;
    if (!xyz_out || !rdab) return ALWAN_E_INVALID;
    if (ALWAN_CORE_FNLIT(cm_rdab_white)(w, xyz_n, k_ab) != ALWAN_OK) return ALWAN_E_INVALID;
    if (w[3] == ALWAN_CORE_LITERAL(0.0) || w[4] == ALWAN_CORE_LITERAL(0.0)) return ALWAN_E_INVALID;
    f = ALWAN_CORE_LITERAL(0.51) * ((ALWAN_CORE_LITERAL(21.0) + ALWAN_CORE_LITERAL(0.2) * rdab->v[0]) /
                                    (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_LITERAL(0.2) * rdab->v[0]));
    rd_yn = rdab->v[0] / w[1];
    xyz_out->x = (rdab->v[1] / (w[3] * f) + rd_yn) * w[0];
    xyz_out->y = rdab->v[0];
    xyz_out->z = -(rdab->v[2] / (w[4] * f) - rd_yn) * w[2];
    return ALWAN_OK;
}
