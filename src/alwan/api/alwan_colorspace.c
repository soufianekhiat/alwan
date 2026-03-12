/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_colorspace_core.h"

/* ================================================================
 * XYZ <-> xyY
 * ================================================================ */

void alwan_xyz_to_xyy(alwan_xyy *xyy, alwan_xyz const *xyz) {
    *xyy = alwan_xyz_to_xyy_v(*xyz);
}

void alwan_xyy_to_xyz(alwan_xyz *xyz, alwan_xyy const *xyy) {
    *xyz = alwan_xyy_to_xyz_v(*xyy);
}

/* ================================================================
 * XYZ <-> Lab
 * ================================================================ */

void alwan_xyz_to_lab(alwan_lab *lab, alwan_xyz const *xyz, alwan_xyz const *white_xyz) {
    *lab = alwan_xyz_to_lab_v(*xyz, *white_xyz);
    ALWAN_NORM_LAB(lab);
}

void alwan_lab_to_xyz(alwan_xyz *xyz, alwan_lab const *lab, alwan_xyz const *white_xyz) {
    alwan_lab tmp = *lab;
    ALWAN_DENORM_LAB(&tmp);
    *xyz = alwan_lab_to_xyz_v(tmp, *white_xyz);
}

/* ================================================================
 * XYZ <-> Luv
 * ================================================================ */

void alwan_xyz_to_luv(alwan_luv *luv, alwan_xyz const *xyz, alwan_xyz const *white_xyz) {
    *luv = alwan_xyz_to_luv_v(*xyz, *white_xyz);
    ALWAN_NORM_LUV(luv);
}

void alwan_luv_to_xyz(alwan_xyz *xyz, alwan_luv const *luv, alwan_xyz const *white_xyz) {
    alwan_luv tmp = *luv;
    ALWAN_DENORM_LUV(&tmp);
    *xyz = alwan_luv_to_xyz_v(tmp, *white_xyz);
}

/* ================================================================
 * XYZ <-> U*V*W*
 * ================================================================ */

void alwan_xyz_to_uvw(alwan_uvw *uvw, alwan_xyz const *xyz, alwan_xyz const *white_xyz) {
    *uvw = alwan_xyz_to_uvw_v(*xyz, *white_xyz);
}

void alwan_uvw_to_xyz(alwan_xyz *xyz, alwan_uvw const *uvw, alwan_xyz const *white_xyz) {
    *xyz = alwan_uvw_to_xyz_v(*uvw, *white_xyz);
}

/* ================================================================
 * Lab <-> LCh(ab)
 * ================================================================ */

void alwan_lab_to_lch(alwan_lch *lch, alwan_lab const *lab) {
    alwan_lab tmp = *lab;
    ALWAN_DENORM_LAB(&tmp);
    *lch = alwan_lab_to_lch_v(tmp);
    ALWAN_NORM_LCH(lch);
}

void alwan_lch_to_lab(alwan_lab *lab, alwan_lch const *lch) {
    alwan_lch tmp = *lch;
    ALWAN_DENORM_LCH(&tmp);
    *lab = alwan_lch_to_lab_v(tmp);
    ALWAN_NORM_LAB(lab);
}

/* ================================================================
 * Luv <-> LCh(uv)
 * ================================================================ */

void alwan_luv_to_lchuv(alwan_lchuv *lchuv, alwan_luv const *luv) {
    alwan_luv tmp = *luv;
    ALWAN_DENORM_LUV(&tmp);
    *lchuv = alwan_luv_to_lchuv_v(tmp);
    ALWAN_NORM_LCHUV(lchuv);
}

void alwan_lchuv_to_luv(alwan_luv *luv, alwan_lchuv const *lchuv) {
    alwan_lchuv tmp = *lchuv;
    ALWAN_DENORM_LCHUV(&tmp);
    *luv = alwan_lchuv_to_luv_v(tmp);
    ALWAN_NORM_LUV(luv);
}

/* ================================================================
 * Direct XYZ <-> Cylindrical
 * ================================================================ */

void alwan_xyz_to_lch(alwan_lch *lch, alwan_xyz const *xyz, alwan_xyz const *white_xyz) {
    *lch = alwan_xyz_to_lch_v(*xyz, *white_xyz);
    ALWAN_NORM_LCH(lch);
}

void alwan_lch_to_xyz(alwan_xyz *xyz, alwan_lch const *lch, alwan_xyz const *white_xyz) {
    alwan_lch tmp = *lch;
    ALWAN_DENORM_LCH(&tmp);
    *xyz = alwan_lch_to_xyz_v(tmp, *white_xyz);
}

void alwan_xyz_to_lchuv(alwan_lchuv *lchuv, alwan_xyz const *xyz, alwan_xyz const *white_xyz) {
    *lchuv = alwan_xyz_to_lchuv_v(*xyz, *white_xyz);
    ALWAN_NORM_LCHUV(lchuv);
}

void alwan_lchuv_to_xyz(alwan_xyz *xyz, alwan_lchuv const *lchuv, alwan_xyz const *white_xyz) {
    alwan_lchuv tmp = *lchuv;
    ALWAN_DENORM_LCHUV(&tmp);
    *xyz = alwan_lchuv_to_xyz_v(tmp, *white_xyz);
}

/* ================================================================
 * XYZ <-> CIE 1960 UCS
 * ================================================================ */

void alwan_xyz_to_ucs(alwan_ucs *ucs, alwan_xyz const *xyz) {
    *ucs = alwan_xyz_to_ucs_v(*xyz);
}

void alwan_ucs_to_xyz(alwan_xyz *xyz, alwan_ucs const *ucs) {
    *xyz = alwan_ucs_to_xyz_v(*ucs);
}

/* ================================================================
 * Color Difference (ΔE) Metrics
 * ================================================================ */

alwan_scalar alwan_delta_e_76(alwan_lab const *lab1, alwan_lab const *lab2) {
    alwan_lab t1 = *lab1, t2 = *lab2;
    ALWAN_DENORM_LAB(&t1); ALWAN_DENORM_LAB(&t2);
    return alwan_delta_e_76_v(t1, t2);
}

alwan_scalar alwan_delta_e_ok(alwan_oklab const *a, alwan_oklab const *b) {
    return alwan_delta_e_ok_v(*a, *b);
}

alwan_scalar alwan_delta_e_94(alwan_lab const *lab1, alwan_lab const *lab2) {
    alwan_lab t1 = *lab1, t2 = *lab2;
    ALWAN_DENORM_LAB(&t1); ALWAN_DENORM_LAB(&t2);
    return alwan_delta_e_94_v(t1, t2);
}

alwan_scalar alwan_delta_e_cmc(alwan_lab const *lab1, alwan_lab const *lab2, alwan_scalar l, alwan_scalar c) {
    alwan_lab t1 = *lab1, t2 = *lab2;
    ALWAN_DENORM_LAB(&t1); ALWAN_DENORM_LAB(&t2);
    return alwan_delta_e_cmc_v(t1, t2, l, c);
}

alwan_scalar alwan_delta_e_2000(alwan_lab const *lab1, alwan_lab const *lab2) {
    alwan_lab t1 = *lab1, t2 = *lab2;
    ALWAN_DENORM_LAB(&t1); ALWAN_DENORM_LAB(&t2);
    return alwan_delta_e_2000_v(t1, t2);
}

/* ================================================================
 * Additional Color Difference Metrics
 * ================================================================ */

alwan_scalar alwan_delta_e_itp(alwan_ictcp const *ictcp1, alwan_ictcp const *ictcp2, alwan_scalar scalar_factor) {
    return alwan_delta_e_itp_v(*ictcp1, *ictcp2, scalar_factor);
}

alwan_scalar alwan_delta_e_hyab(alwan_lab const *lab1, alwan_lab const *lab2) {
    alwan_lab t1 = *lab1, t2 = *lab2;
    ALWAN_DENORM_LAB(&t1); ALWAN_DENORM_LAB(&t2);
    return alwan_delta_e_hyab_v(t1, t2);
}

alwan_scalar alwan_delta_e_din99(alwan_din99 const *din99_1, alwan_din99 const *din99_2) {
    alwan_din99 t1 = *din99_1, t2 = *din99_2;
    ALWAN_DENORM_DIN99(&t1); ALWAN_DENORM_DIN99(&t2);
    return alwan_delta_e_din99_v(t1, t2);
}

alwan_scalar alwan_delta_e_cam02_lcd(alwan_cam_jab const *jab1, alwan_cam_jab const *jab2) {
    alwan_cam_jab t1 = *jab1, t2 = *jab2;
    ALWAN_DENORM_CAM_JAB(&t1); ALWAN_DENORM_CAM_JAB(&t2);
    return alwan_delta_e_cam02_lcd_v(t1, t2);
}

alwan_scalar alwan_delta_e_cam02_scd(alwan_cam_jab const *jab1, alwan_cam_jab const *jab2) {
    alwan_cam_jab t1 = *jab1, t2 = *jab2;
    ALWAN_DENORM_CAM_JAB(&t1); ALWAN_DENORM_CAM_JAB(&t2);
    return alwan_delta_e_cam02_scd_v(t1, t2);
}

alwan_scalar alwan_delta_e_cam16_lcd(alwan_cam_jab const *jab1, alwan_cam_jab const *jab2) {
    alwan_cam_jab t1 = *jab1, t2 = *jab2;
    ALWAN_DENORM_CAM_JAB(&t1); ALWAN_DENORM_CAM_JAB(&t2);
    return alwan_delta_e_cam16_lcd_v(t1, t2);
}

alwan_scalar alwan_delta_e_cam16_scd(alwan_cam_jab const *jab1, alwan_cam_jab const *jab2) {
    alwan_cam_jab t1 = *jab1, t2 = *jab2;
    ALWAN_DENORM_CAM_JAB(&t1); ALWAN_DENORM_CAM_JAB(&t2);
    return alwan_delta_e_cam16_scd_v(t1, t2);
}

alwan_scalar alwan_delta_e_cam02_ucs(alwan_cam_jab const *jab1, alwan_cam_jab const *jab2) {
    alwan_cam_jab t1 = *jab1, t2 = *jab2;
    ALWAN_DENORM_CAM_JAB(&t1); ALWAN_DENORM_CAM_JAB(&t2);
    return alwan_delta_e_cam02_ucs_v(t1, t2);
}

alwan_scalar alwan_delta_e_cam16_ucs(alwan_cam_jab const *jab1, alwan_cam_jab const *jab2) {
    alwan_cam_jab t1 = *jab1, t2 = *jab2;
    ALWAN_DENORM_CAM_JAB(&t1); ALWAN_DENORM_CAM_JAB(&t2);
    return alwan_delta_e_cam16_ucs_v(t1, t2);
}

alwan_scalar alwan_delta_e_zcam(alwan_jzazbz const *jab1, alwan_jzazbz const *jab2) {
    return alwan_delta_e_zcam_v(*jab1, *jab2);
}
