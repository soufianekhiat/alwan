/* No include guard — included twice (f32 and f64 passes) */

/* ================================================================
 * XYZ <-> xyY
 * ================================================================ */

void ALWAN_CORE_FNLIT(alwan_xyz_to_xyy)(ALWAN_CORE_XYY *xyy, ALWAN_CORE_XYZ const *xyz) {
    *xyy = ALWAN_CORE_FNVLIT(alwan_xyz_to_xyy)(*xyz);
}

void ALWAN_CORE_FNLIT(alwan_xyy_to_xyz)(ALWAN_CORE_XYZ *xyz, ALWAN_CORE_XYY const *xyy) {
    *xyz = ALWAN_CORE_FNVLIT(alwan_xyy_to_xyz)(*xyy);
}

/* ================================================================
 * XYZ <-> Lab
 * ================================================================ */

void ALWAN_CORE_FNLIT(alwan_xyz_to_lab)(ALWAN_CORE_LAB *lab, ALWAN_CORE_XYZ const *xyz, ALWAN_CORE_XYZ const *white_xyz) {
    *lab = ALWAN_CORE_FNVLIT(alwan_xyz_to_lab)(*xyz, *white_xyz);
    ALWAN_NORM_LAB(lab);
}

void ALWAN_CORE_FNLIT(alwan_lab_to_xyz)(ALWAN_CORE_XYZ *xyz, ALWAN_CORE_LAB const *lab, ALWAN_CORE_XYZ const *white_xyz) {
    ALWAN_CORE_LAB tmp = *lab;
    ALWAN_DENORM_LAB(&tmp);
    *xyz = ALWAN_CORE_FNVLIT(alwan_lab_to_xyz)(tmp, *white_xyz);
}

/* ================================================================
 * XYZ <-> Luv
 * ================================================================ */

void ALWAN_CORE_FNLIT(alwan_xyz_to_luv)(ALWAN_CORE_LUV *luv, ALWAN_CORE_XYZ const *xyz, ALWAN_CORE_XYZ const *white_xyz) {
    *luv = ALWAN_CORE_FNVLIT(alwan_xyz_to_luv)(*xyz, *white_xyz);
    ALWAN_NORM_LUV(luv);
}

void ALWAN_CORE_FNLIT(alwan_luv_to_xyz)(ALWAN_CORE_XYZ *xyz, ALWAN_CORE_LUV const *luv, ALWAN_CORE_XYZ const *white_xyz) {
    ALWAN_CORE_LUV tmp = *luv;
    ALWAN_DENORM_LUV(&tmp);
    *xyz = ALWAN_CORE_FNVLIT(alwan_luv_to_xyz)(tmp, *white_xyz);
}

/* ================================================================
 * XYZ <-> U*V*W*
 * ================================================================ */

void ALWAN_CORE_FNLIT(alwan_xyz_to_uvw)(ALWAN_CORE_UVW *uvw, ALWAN_CORE_XYZ const *xyz, ALWAN_CORE_XYZ const *white_xyz) {
    *uvw = ALWAN_CORE_FNVLIT(alwan_xyz_to_uvw)(*xyz, *white_xyz);
}

void ALWAN_CORE_FNLIT(alwan_uvw_to_xyz)(ALWAN_CORE_XYZ *xyz, ALWAN_CORE_UVW const *uvw, ALWAN_CORE_XYZ const *white_xyz) {
    *xyz = ALWAN_CORE_FNVLIT(alwan_uvw_to_xyz)(*uvw, *white_xyz);
}

/* ================================================================
 * Lab <-> LCh(ab)
 * ================================================================ */

void ALWAN_CORE_FNLIT(alwan_lab_to_lch)(ALWAN_CORE_LCH *lch, ALWAN_CORE_LAB const *lab) {
    ALWAN_CORE_LAB tmp = *lab;
    ALWAN_DENORM_LAB(&tmp);
    *lch = ALWAN_CORE_FNVLIT(alwan_lab_to_lch)(tmp);
    ALWAN_NORM_LCH(lch);
}

void ALWAN_CORE_FNLIT(alwan_lch_to_lab)(ALWAN_CORE_LAB *lab, ALWAN_CORE_LCH const *lch) {
    ALWAN_CORE_LCH tmp = *lch;
    ALWAN_DENORM_LCH(&tmp);
    *lab = ALWAN_CORE_FNVLIT(alwan_lch_to_lab)(tmp);
    ALWAN_NORM_LAB(lab);
}

/* ================================================================
 * Luv <-> LCh(uv)
 * ================================================================ */

void ALWAN_CORE_FNLIT(alwan_luv_to_lchuv)(ALWAN_CORE_LCHUV *lchuv, ALWAN_CORE_LUV const *luv) {
    ALWAN_CORE_LUV tmp = *luv;
    ALWAN_DENORM_LUV(&tmp);
    *lchuv = ALWAN_CORE_FNVLIT(alwan_luv_to_lchuv)(tmp);
    ALWAN_NORM_LCHUV(lchuv);
}

void ALWAN_CORE_FNLIT(alwan_lchuv_to_luv)(ALWAN_CORE_LUV *luv, ALWAN_CORE_LCHUV const *lchuv) {
    ALWAN_CORE_LCHUV tmp = *lchuv;
    ALWAN_DENORM_LCHUV(&tmp);
    *luv = ALWAN_CORE_FNVLIT(alwan_lchuv_to_luv)(tmp);
    ALWAN_NORM_LUV(luv);
}

/* ================================================================
 * Direct XYZ <-> Cylindrical
 * ================================================================ */

void ALWAN_CORE_FNLIT(alwan_xyz_to_lch)(ALWAN_CORE_LCH *lch, ALWAN_CORE_XYZ const *xyz, ALWAN_CORE_XYZ const *white_xyz) {
    *lch = ALWAN_CORE_FNVLIT(alwan_xyz_to_lch)(*xyz, *white_xyz);
    ALWAN_NORM_LCH(lch);
}

void ALWAN_CORE_FNLIT(alwan_lch_to_xyz)(ALWAN_CORE_XYZ *xyz, ALWAN_CORE_LCH const *lch, ALWAN_CORE_XYZ const *white_xyz) {
    ALWAN_CORE_LCH tmp = *lch;
    ALWAN_DENORM_LCH(&tmp);
    *xyz = ALWAN_CORE_FNVLIT(alwan_lch_to_xyz)(tmp, *white_xyz);
}

void ALWAN_CORE_FNLIT(alwan_xyz_to_lchuv)(ALWAN_CORE_LCHUV *lchuv, ALWAN_CORE_XYZ const *xyz, ALWAN_CORE_XYZ const *white_xyz) {
    *lchuv = ALWAN_CORE_FNVLIT(alwan_xyz_to_lchuv)(*xyz, *white_xyz);
    ALWAN_NORM_LCHUV(lchuv);
}

void ALWAN_CORE_FNLIT(alwan_lchuv_to_xyz)(ALWAN_CORE_XYZ *xyz, ALWAN_CORE_LCHUV const *lchuv, ALWAN_CORE_XYZ const *white_xyz) {
    ALWAN_CORE_LCHUV tmp = *lchuv;
    ALWAN_DENORM_LCHUV(&tmp);
    *xyz = ALWAN_CORE_FNVLIT(alwan_lchuv_to_xyz)(tmp, *white_xyz);
}

/* ================================================================
 * XYZ <-> CIE 1960 UCS
 * ================================================================ */

void ALWAN_CORE_FNLIT(alwan_xyz_to_ucs)(ALWAN_CORE_UCS *ucs, ALWAN_CORE_XYZ const *xyz) {
    *ucs = ALWAN_CORE_FNVLIT(alwan_xyz_to_ucs)(*xyz);
}

void ALWAN_CORE_FNLIT(alwan_ucs_to_xyz)(ALWAN_CORE_XYZ *xyz, ALWAN_CORE_UCS const *ucs) {
    *xyz = ALWAN_CORE_FNVLIT(alwan_ucs_to_xyz)(*ucs);
}

/* ================================================================
 * Color Difference (ΔE) Metrics
 * ================================================================ */

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_delta_e_76)(ALWAN_CORE_LAB const *lab1, ALWAN_CORE_LAB const *lab2) {
    ALWAN_CORE_LAB t1 = *lab1, t2 = *lab2;
    ALWAN_DENORM_LAB(&t1); ALWAN_DENORM_LAB(&t2);
    return ALWAN_CORE_FNVLIT(alwan_delta_e_76)(t1, t2);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_delta_e_ok)(ALWAN_CORE_OKLAB const *a, ALWAN_CORE_OKLAB const *b) {
    return ALWAN_CORE_FNVLIT(alwan_delta_e_ok)(*a, *b);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_delta_e_94)(ALWAN_CORE_LAB const *lab1, ALWAN_CORE_LAB const *lab2) {
    ALWAN_CORE_LAB t1 = *lab1, t2 = *lab2;
    ALWAN_DENORM_LAB(&t1); ALWAN_DENORM_LAB(&t2);
    return ALWAN_CORE_FNVLIT(alwan_delta_e_94)(t1, t2);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_delta_e_cmc)(ALWAN_CORE_LAB const *lab1, ALWAN_CORE_LAB const *lab2, ALWAN_CORE_DELTA_E_CMC_PARAMS const *params) {
    ALWAN_CORE_LAB t1 = *lab1, t2 = *lab2;
    ALWAN_DENORM_LAB(&t1); ALWAN_DENORM_LAB(&t2);
    return ALWAN_CORE_FNVLIT(alwan_delta_e_cmc)(t1, t2, (ALWAN_CORE_T)params->l, (ALWAN_CORE_T)params->c);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_delta_e_2000)(ALWAN_CORE_LAB const *lab1, ALWAN_CORE_LAB const *lab2) {
    ALWAN_CORE_LAB t1 = *lab1, t2 = *lab2;
    ALWAN_DENORM_LAB(&t1); ALWAN_DENORM_LAB(&t2);
    return ALWAN_CORE_FNVLIT(alwan_delta_e_2000)(t1, t2);
}

/* ================================================================
 * Additional Color Difference Metrics
 * ================================================================ */

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_delta_e_itp)(ALWAN_CORE_ICTCP const *ictcp1, ALWAN_CORE_ICTCP const *ictcp2, ALWAN_CORE_DELTA_E_ITP_PARAMS const *params) {
    return ALWAN_CORE_FNVLIT(alwan_delta_e_itp)(*ictcp1, *ictcp2, (ALWAN_CORE_T)params->scalar_factor);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_delta_e_hyab)(ALWAN_CORE_LAB const *lab1, ALWAN_CORE_LAB const *lab2) {
    ALWAN_CORE_LAB t1 = *lab1, t2 = *lab2;
    ALWAN_DENORM_LAB(&t1); ALWAN_DENORM_LAB(&t2);
    return ALWAN_CORE_FNVLIT(alwan_delta_e_hyab)(t1, t2);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_delta_e_din99)(ALWAN_CORE_DIN99 const *din99_1, ALWAN_CORE_DIN99 const *din99_2) {
    ALWAN_CORE_DIN99 t1 = *din99_1, t2 = *din99_2;
    ALWAN_DENORM_DIN99(&t1); ALWAN_DENORM_DIN99(&t2);
    return ALWAN_CORE_FNVLIT(alwan_delta_e_din99)(t1, t2);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_delta_e_cam02_lcd)(ALWAN_CORE_CAM_JAB const *jab1, ALWAN_CORE_CAM_JAB const *jab2) {
    ALWAN_CORE_CAM_JAB t1 = *jab1, t2 = *jab2;
    ALWAN_DENORM_CAM_JAB(&t1); ALWAN_DENORM_CAM_JAB(&t2);
    return ALWAN_CORE_FNVLIT(alwan_delta_e_cam02_lcd)(t1, t2);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_delta_e_cam02_scd)(ALWAN_CORE_CAM_JAB const *jab1, ALWAN_CORE_CAM_JAB const *jab2) {
    ALWAN_CORE_CAM_JAB t1 = *jab1, t2 = *jab2;
    ALWAN_DENORM_CAM_JAB(&t1); ALWAN_DENORM_CAM_JAB(&t2);
    return ALWAN_CORE_FNVLIT(alwan_delta_e_cam02_scd)(t1, t2);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_delta_e_cam16_lcd)(ALWAN_CORE_CAM_JAB const *jab1, ALWAN_CORE_CAM_JAB const *jab2) {
    ALWAN_CORE_CAM_JAB t1 = *jab1, t2 = *jab2;
    ALWAN_DENORM_CAM_JAB(&t1); ALWAN_DENORM_CAM_JAB(&t2);
    return ALWAN_CORE_FNVLIT(alwan_delta_e_cam16_lcd)(t1, t2);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_delta_e_cam16_scd)(ALWAN_CORE_CAM_JAB const *jab1, ALWAN_CORE_CAM_JAB const *jab2) {
    ALWAN_CORE_CAM_JAB t1 = *jab1, t2 = *jab2;
    ALWAN_DENORM_CAM_JAB(&t1); ALWAN_DENORM_CAM_JAB(&t2);
    return ALWAN_CORE_FNVLIT(alwan_delta_e_cam16_scd)(t1, t2);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_delta_e_cam02_ucs)(ALWAN_CORE_CAM_JAB const *jab1, ALWAN_CORE_CAM_JAB const *jab2) {
    ALWAN_CORE_CAM_JAB t1 = *jab1, t2 = *jab2;
    ALWAN_DENORM_CAM_JAB(&t1); ALWAN_DENORM_CAM_JAB(&t2);
    return ALWAN_CORE_FNVLIT(alwan_delta_e_cam02_ucs)(t1, t2);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_delta_e_cam16_ucs)(ALWAN_CORE_CAM_JAB const *jab1, ALWAN_CORE_CAM_JAB const *jab2) {
    ALWAN_CORE_CAM_JAB t1 = *jab1, t2 = *jab2;
    ALWAN_DENORM_CAM_JAB(&t1); ALWAN_DENORM_CAM_JAB(&t2);
    return ALWAN_CORE_FNVLIT(alwan_delta_e_cam16_ucs)(t1, t2);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_delta_e_zcam)(ALWAN_CORE_JZAZBZ const *jab1, ALWAN_CORE_JZAZBZ const *jab2) {
    return ALWAN_CORE_FNVLIT(alwan_delta_e_zcam)(*jab1, *jab2);
}
