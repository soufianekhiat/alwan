/* No include guard -- included twice (f32 and f64 passes) */

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
    ALWAN_NORM_UVW(uvw);
}

void ALWAN_CORE_FNLIT(alwan_uvw_to_xyz)(ALWAN_CORE_XYZ *xyz, ALWAN_CORE_UVW const *uvw, ALWAN_CORE_XYZ const *white_xyz) {
    ALWAN_CORE_UVW tmp = *uvw;
    ALWAN_DENORM_UVW(&tmp);
    *xyz = ALWAN_CORE_FNVLIT(alwan_uvw_to_xyz)(tmp, *white_xyz);
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
 * Direct XYZ <-> Oklch (cylindrical Oklab, D65 assumed)
 * Composes the D65-referred xyz<->oklab transform with the oklab<->oklch
 * cylindrical step (whose API wrappers already apply NORM/DENORM). Mirrors the
 * xyz_to_lch sibling pattern; no white point because Oklab fixes D65.
 * ================================================================ */

void ALWAN_CORE_FNLIT(alwan_xyz_to_oklch)(ALWAN_CORE_OKLCH *oklch, ALWAN_CORE_XYZ const *xyz) {
    ALWAN_CORE_OKLAB oklab;
    ALWAN_CORE_FNLIT(alwan_xyz_to_oklab)(&oklab, xyz);
    ALWAN_CORE_FNLIT(alwan_oklab_to_oklch)(oklch, &oklab);
}

void ALWAN_CORE_FNLIT(alwan_oklch_to_xyz)(ALWAN_CORE_XYZ *xyz, ALWAN_CORE_OKLCH const *oklch) {
    ALWAN_CORE_OKLAB oklab;
    ALWAN_CORE_FNLIT(alwan_oklch_to_oklab)(&oklab, oklch);
    ALWAN_CORE_FNLIT(alwan_oklab_to_xyz)(xyz, &oklab);
}

/* ================================================================
 * Direct RGB <-> Perceptual Space Conversions
 * Convenience wrappers that fold the RGB-space-descriptor RGB<->XYZ step
 * (alwan_rgb_to_xyz / alwan_xyz_to_rgb) together with the XYZ<->perceptual step.
 * Lab/Luv carry an explicit white point; Oklab/Oklch are D65-referred (the
 * descriptor's primaries select the gamut, Oklab fixes the reference white).
 * NORM/DENORM is inherited from the called XYZ<->perceptual API wrappers.
 * ================================================================ */

int ALWAN_CORE_FNLIT(alwan_rgb_to_lab)(ALWAN_CORE_LAB *lab,
                                       ALWAN_CORE_RGB_SPACE_DESC const *space,
                                       ALWAN_CORE_RGB const *rgb,
                                       ALWAN_CORE_XYZ const *white_xyz) {
    if (!lab || !space || !rgb || !white_xyz) return ALWAN_E_INVALID;
    ALWAN_CORE_XYZ xyz;
    int status = ALWAN_CORE_FNLIT(alwan_rgb_to_xyz)(&xyz, space, rgb);
    if (status != ALWAN_OK) return status;
    ALWAN_CORE_FNLIT(alwan_xyz_to_lab)(lab, &xyz, white_xyz);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_lab_to_rgb)(ALWAN_CORE_RGB *rgb,
                                       ALWAN_CORE_RGB_SPACE_DESC const *space,
                                       ALWAN_CORE_LAB const *lab,
                                       ALWAN_CORE_XYZ const *white_xyz) {
    if (!rgb || !space || !lab || !white_xyz) return ALWAN_E_INVALID;
    ALWAN_CORE_XYZ xyz;
    ALWAN_CORE_FNLIT(alwan_lab_to_xyz)(&xyz, lab, white_xyz);
    return ALWAN_CORE_FNLIT(alwan_xyz_to_rgb)(rgb, space, &xyz);
}

int ALWAN_CORE_FNLIT(alwan_rgb_to_luv)(ALWAN_CORE_LUV *luv,
                                       ALWAN_CORE_RGB_SPACE_DESC const *space,
                                       ALWAN_CORE_RGB const *rgb,
                                       ALWAN_CORE_XYZ const *white_xyz) {
    if (!luv || !space || !rgb || !white_xyz) return ALWAN_E_INVALID;
    ALWAN_CORE_XYZ xyz;
    int status = ALWAN_CORE_FNLIT(alwan_rgb_to_xyz)(&xyz, space, rgb);
    if (status != ALWAN_OK) return status;
    ALWAN_CORE_FNLIT(alwan_xyz_to_luv)(luv, &xyz, white_xyz);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_luv_to_rgb)(ALWAN_CORE_RGB *rgb,
                                       ALWAN_CORE_RGB_SPACE_DESC const *space,
                                       ALWAN_CORE_LUV const *luv,
                                       ALWAN_CORE_XYZ const *white_xyz) {
    if (!rgb || !space || !luv || !white_xyz) return ALWAN_E_INVALID;
    ALWAN_CORE_XYZ xyz;
    ALWAN_CORE_FNLIT(alwan_luv_to_xyz)(&xyz, luv, white_xyz);
    return ALWAN_CORE_FNLIT(alwan_xyz_to_rgb)(rgb, space, &xyz);
}

int ALWAN_CORE_FNLIT(alwan_rgb_to_oklab)(ALWAN_CORE_OKLAB *oklab,
                                         ALWAN_CORE_RGB_SPACE_DESC const *space,
                                         ALWAN_CORE_RGB const *rgb) {
    if (!oklab || !space || !rgb) return ALWAN_E_INVALID;
    ALWAN_CORE_XYZ xyz;
    int status = ALWAN_CORE_FNLIT(alwan_rgb_to_xyz)(&xyz, space, rgb);
    if (status != ALWAN_OK) return status;
    ALWAN_CORE_FNLIT(alwan_xyz_to_oklab)(oklab, &xyz);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_oklab_to_rgb)(ALWAN_CORE_RGB *rgb,
                                         ALWAN_CORE_RGB_SPACE_DESC const *space,
                                         ALWAN_CORE_OKLAB const *oklab) {
    if (!rgb || !space || !oklab) return ALWAN_E_INVALID;
    ALWAN_CORE_XYZ xyz;
    ALWAN_CORE_FNLIT(alwan_oklab_to_xyz)(&xyz, oklab);
    return ALWAN_CORE_FNLIT(alwan_xyz_to_rgb)(rgb, space, &xyz);
}

int ALWAN_CORE_FNLIT(alwan_rgb_to_oklch)(ALWAN_CORE_OKLCH *oklch,
                                         ALWAN_CORE_RGB_SPACE_DESC const *space,
                                         ALWAN_CORE_RGB const *rgb) {
    if (!oklch || !space || !rgb) return ALWAN_E_INVALID;
    ALWAN_CORE_XYZ xyz;
    int status = ALWAN_CORE_FNLIT(alwan_rgb_to_xyz)(&xyz, space, rgb);
    if (status != ALWAN_OK) return status;
    ALWAN_CORE_FNLIT(alwan_xyz_to_oklch)(oklch, &xyz);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_oklch_to_rgb)(ALWAN_CORE_RGB *rgb,
                                         ALWAN_CORE_RGB_SPACE_DESC const *space,
                                         ALWAN_CORE_OKLCH const *oklch) {
    if (!rgb || !space || !oklch) return ALWAN_E_INVALID;
    ALWAN_CORE_XYZ xyz;
    ALWAN_CORE_FNLIT(alwan_oklch_to_xyz)(&xyz, oklch);
    return ALWAN_CORE_FNLIT(alwan_xyz_to_rgb)(rgb, space, &xyz);
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
 * Color Difference (dE) Metrics
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
