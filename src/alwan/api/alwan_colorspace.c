/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_colorspace_core.h"

/* ================================================================
 * XYZ <-> xyY — delegated to alwan_colorspace_core.h
 * ================================================================ */

void alwan_xyz_to_xyy(alwan_xyy *xyy, alwan_xyz const *xyz) {
    *xyy = alwan_xyz_to_xyy_v(*xyz);
}

void alwan_xyy_to_xyz(alwan_xyz *xyz, alwan_xyy const *xyy) {
    *xyz = alwan_xyy_to_xyz_v(*xyy);
}

/* ================================================================
 * XYZ <-> Lab — delegated to alwan_colorspace_core.h
 * ================================================================ */

void alwan_xyz_to_lab(alwan_lab *lab, alwan_xyz const *xyz, alwan_xyz const *white_xyz) {
    *lab = alwan_xyz_to_lab_v(*xyz, *white_xyz);
}

void alwan_lab_to_xyz(alwan_xyz *xyz, alwan_lab const *lab, alwan_xyz const *white_xyz) {
    *xyz = alwan_lab_to_xyz_v(*lab, *white_xyz);
}

/* ================================================================
 * XYZ <-> Luv — delegated to alwan_colorspace_core.h
 * ================================================================ */

void alwan_xyz_to_luv(alwan_luv *luv, alwan_xyz const *xyz, alwan_xyz const *white_xyz) {
    *luv = alwan_xyz_to_luv_v(*xyz, *white_xyz);
}

void alwan_luv_to_xyz(alwan_xyz *xyz, alwan_luv const *luv, alwan_xyz const *white_xyz) {
    *xyz = alwan_luv_to_xyz_v(*luv, *white_xyz);
}

/* ================================================================
 * XYZ <-> U*V*W* — delegated to alwan_colorspace_core.h
 * ================================================================ */

void alwan_xyz_to_uvw(alwan_uvw *uvw, alwan_xyz const *xyz, alwan_xyz const *white_xyz) {
    *uvw = alwan_xyz_to_uvw_v(*xyz, *white_xyz);
}

void alwan_uvw_to_xyz(alwan_xyz *xyz, alwan_uvw const *uvw, alwan_xyz const *white_xyz) {
    *xyz = alwan_uvw_to_xyz_v(*uvw, *white_xyz);
}

/* ================================================================
 * Lab <-> LCh(ab) — delegated to alwan_colorspace_core.h
 * ================================================================ */

void alwan_lab_to_lch(alwan_lch *lch, alwan_lab const *lab) {
    *lch = alwan_lab_to_lch_v(*lab);
}

void alwan_lch_to_lab(alwan_lab *lab, alwan_lch const *lch) {
    *lab = alwan_lch_to_lab_v(*lch);
}

/* ================================================================
 * Luv <-> LCh(uv) — delegated to alwan_colorspace_core.h
 * ================================================================ */

void alwan_luv_to_lchuv(alwan_lchuv *lchuv, alwan_luv const *luv) {
    *lchuv = alwan_luv_to_lchuv_v(*luv);
}

void alwan_lchuv_to_luv(alwan_luv *luv, alwan_lchuv const *lchuv) {
    *luv = alwan_lchuv_to_luv_v(*lchuv);
}

/* ================================================================
 * Direct XYZ <-> Cylindrical — delegated to alwan_colorspace_core.h
 * ================================================================ */

void alwan_xyz_to_lch(alwan_lch *lch, alwan_xyz const *xyz, alwan_xyz const *white_xyz) {
    *lch = alwan_xyz_to_lch_v(*xyz, *white_xyz);
}

void alwan_lch_to_xyz(alwan_xyz *xyz, alwan_lch const *lch, alwan_xyz const *white_xyz) {
    *xyz = alwan_lch_to_xyz_v(*lch, *white_xyz);
}

void alwan_xyz_to_lchuv(alwan_lchuv *lchuv, alwan_xyz const *xyz, alwan_xyz const *white_xyz) {
    *lchuv = alwan_xyz_to_lchuv_v(*xyz, *white_xyz);
}

void alwan_lchuv_to_xyz(alwan_xyz *xyz, alwan_lchuv const *lchuv, alwan_xyz const *white_xyz) {
    *xyz = alwan_lchuv_to_xyz_v(*lchuv, *white_xyz);
}

/* ================================================================
 * XYZ <-> CIE 1960 UCS — delegated to alwan_colorspace_core.h
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
    return alwan_delta_e_76_v(*lab1, *lab2);
}

alwan_scalar alwan_delta_e_ok(alwan_oklab const *a, alwan_oklab const *b) {
    return alwan_delta_e_ok_v(*a, *b);
}

alwan_scalar alwan_delta_e_94(alwan_lab const *lab1, alwan_lab const *lab2) {
    return alwan_delta_e_94_v(*lab1, *lab2);
}

alwan_scalar alwan_delta_e_cmc(alwan_lab const *lab1, alwan_lab const *lab2, alwan_scalar l, alwan_scalar c) {
    return alwan_delta_e_cmc_v(*lab1, *lab2, l, c);
}

alwan_scalar alwan_delta_e_2000(alwan_lab const *lab1, alwan_lab const *lab2) {
    return alwan_delta_e_2000_v(*lab1, *lab2);
}

/* ================================================================
 * Additional Color Difference Metrics
 * ================================================================ */

alwan_scalar alwan_delta_e_itp(alwan_ictcp const *ictcp1, alwan_ictcp const *ictcp2, alwan_scalar scalar_factor) {
    return alwan_delta_e_itp_v(*ictcp1, *ictcp2, scalar_factor);
}

alwan_scalar alwan_delta_e_hyab(alwan_lab const *lab1, alwan_lab const *lab2) {
    return alwan_delta_e_hyab_v(*lab1, *lab2);
}

alwan_scalar alwan_delta_e_din99(alwan_din99 const *din99_1, alwan_din99 const *din99_2) {
    return alwan_delta_e_din99_v(*din99_1, *din99_2);
}

alwan_scalar alwan_delta_e_cam02_lcd(alwan_cam_jab const *jab1, alwan_cam_jab const *jab2) {
    return alwan_delta_e_cam02_lcd_v(*jab1, *jab2);
}

alwan_scalar alwan_delta_e_cam02_scd(alwan_cam_jab const *jab1, alwan_cam_jab const *jab2) {
    return alwan_delta_e_cam02_scd_v(*jab1, *jab2);
}

alwan_scalar alwan_delta_e_cam16_lcd(alwan_cam_jab const *jab1, alwan_cam_jab const *jab2) {
    return alwan_delta_e_cam16_lcd_v(*jab1, *jab2);
}

alwan_scalar alwan_delta_e_cam16_scd(alwan_cam_jab const *jab1, alwan_cam_jab const *jab2) {
    return alwan_delta_e_cam16_scd_v(*jab1, *jab2);
}

alwan_scalar alwan_delta_e_cam02_ucs(alwan_cam_jab const *jab1, alwan_cam_jab const *jab2) {
    return alwan_delta_e_cam02_ucs_v(*jab1, *jab2);
}

alwan_scalar alwan_delta_e_cam16_ucs(alwan_cam_jab const *jab1, alwan_cam_jab const *jab2) {
    return alwan_delta_e_cam16_ucs_v(*jab1, *jab2);
}

alwan_scalar alwan_delta_e_zcam(alwan_jzazbz const *jab1, alwan_jzazbz const *jab2) {
    return alwan_delta_e_zcam_v(*jab1, *jab2);
}
