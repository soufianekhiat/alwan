/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * M1: Extended Color Spaces & Models
 * See alwan_extended_core.h
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_extended_core.h"

/* ================================================================
 * Prismatic
 * ================================================================ */

void alwan_rgb_to_prismatic(alwan_prismatic *prismatic, alwan_rgb const *rgb) {
    if (!prismatic || !rgb) return;
    *prismatic = alwan_rgb_to_prismatic_v(*rgb);
}

void alwan_prismatic_to_rgb(alwan_rgb *rgb, alwan_prismatic const *prismatic) {
    if (!rgb || !prismatic) return;
    *rgb = alwan_prismatic_to_rgb_v(*prismatic);
}

/* ================================================================
 * HCL
 * ================================================================ */

void alwan_rgb_to_hcl(alwan_hcl *hcl, alwan_rgb const *rgb) {
    if (!hcl || !rgb) return;
    *hcl = alwan_rgb_to_hcl_v(*rgb);
    ALWAN_NORM_HCL(hcl);
}

void alwan_hcl_to_rgb(alwan_rgb *rgb, alwan_hcl const *hcl) {
    if (!rgb || !hcl) return;
    alwan_hcl tmp = *hcl;
    ALWAN_DENORM_HCL(&tmp);
    *rgb = alwan_hcl_to_rgb_v(tmp);
}

/* ================================================================
 * IHLS
 * ================================================================ */

void alwan_rgb_to_ihls(alwan_ihls *ihls, alwan_rgb const *rgb) {
    if (!ihls || !rgb) return;
    *ihls = alwan_rgb_to_ihls_v(*rgb);
    ALWAN_NORM_IHLS(ihls);
}

void alwan_ihls_to_rgb(alwan_rgb *rgb, alwan_ihls const *ihls) {
    if (!rgb || !ihls) return;
    alwan_ihls tmp = *ihls;
    ALWAN_DENORM_IHLS(&tmp);
    *rgb = alwan_ihls_to_rgb_v(tmp);
}

/* ================================================================
 * HLC (cylindrical CIELAB with H,L,C ordering)
 * ================================================================ */

void alwan_lch_to_hlc(alwan_hlc *hlc, alwan_lch const *lch) {
    if (!hlc || !lch) return;
    *hlc = alwan_lch_to_hlc_v(*lch);
}

void alwan_hlc_to_lch(alwan_lch *lch, alwan_hlc const *hlc) {
    if (!lch || !hlc) return;
    *lch = alwan_hlc_to_lch_v(*hlc);
}

/* ================================================================
 * Cubehelix
 * ================================================================ */

void alwan_cubehelix_to_rgb(alwan_rgb *rgb, alwan_cubehelix const *ch) {
    if (!rgb || !ch) return;
    *rgb = alwan_cubehelix_to_rgb_v(*ch);
}

void alwan_rgb_to_cubehelix(alwan_cubehelix *ch, alwan_rgb const *rgb) {
    if (!ch || !rgb) return;
    *ch = alwan_rgb_to_cubehelix_v(*rgb);
}

/* ================================================================
 * HSLuv / HPLuv
 * ================================================================ */

void alwan_hsluv_to_srgb(alwan_rgb *rgb, alwan_hsluv const *hsluv) {
    if (!rgb || !hsluv) return;
    *rgb = alwan_hsluv_to_srgb_v(*hsluv);
}

void alwan_srgb_to_hsluv(alwan_hsluv *hsluv, alwan_rgb const *srgb) {
    if (!hsluv || !srgb) return;
    *hsluv = alwan_srgb_to_hsluv_v(*srgb);
}

void alwan_hpluv_to_srgb(alwan_rgb *rgb, alwan_hpluv const *hpluv) {
    if (!rgb || !hpluv) return;
    *rgb = alwan_hpluv_to_srgb_v(*hpluv);
}

void alwan_srgb_to_hpluv(alwan_hpluv *hpluv, alwan_rgb const *srgb) {
    if (!hpluv || !srgb) return;
    *hpluv = alwan_srgb_to_hpluv_v(*srgb);
}

/* ================================================================
 * Okhsl / Okhsv
 * ================================================================ */

void alwan_okhsl_to_srgb(alwan_rgb *rgb, alwan_okhsl const *okhsl) {
    if (!rgb || !okhsl) return;
    *rgb = alwan_okhsl_to_srgb_v(*okhsl);
}

void alwan_srgb_to_okhsl(alwan_okhsl *okhsl, alwan_rgb const *srgb) {
    if (!okhsl || !srgb) return;
    *okhsl = alwan_srgb_to_okhsl_v(*srgb);
}

void alwan_okhsv_to_srgb(alwan_rgb *rgb, alwan_okhsv const *okhsv) {
    if (!rgb || !okhsv) return;
    *rgb = alwan_okhsv_to_srgb_v(*okhsv);
}

void alwan_srgb_to_okhsv(alwan_okhsv *okhsv, alwan_rgb const *srgb) {
    if (!okhsv || !srgb) return;
    *okhsv = alwan_srgb_to_okhsv_v(*srgb);
}

/* ================================================================
 * hdr-CIELAB
 * ================================================================ */

void alwan_xyz_to_hdr_cielab(alwan_lab *hdr_lab, alwan_xyz const *xyz) {
    if (!xyz || !hdr_lab) return;
    *hdr_lab = alwan_xyz_to_hdr_cielab_v(*xyz);
}

void alwan_hdr_cielab_to_xyz(alwan_xyz *xyz, alwan_lab const *hdr_lab) {
    if (!hdr_lab || !xyz) return;
    *xyz = alwan_hdr_cielab_to_xyz_v(*hdr_lab);
}

/* ================================================================
 * hdr-IPT
 * ================================================================ */

void alwan_xyz_to_hdr_ipt(alwan_ipt *hdr_ipt, alwan_xyz const *xyz) {
    if (!xyz || !hdr_ipt) return;
    *hdr_ipt = alwan_xyz_to_hdr_ipt_v(*xyz);
}

void alwan_hdr_ipt_to_xyz(alwan_xyz *xyz, alwan_ipt const *hdr_ipt) {
    if (!hdr_ipt || !xyz) return;
    *xyz = alwan_hdr_ipt_to_xyz_v(*hdr_ipt);
}

/* ================================================================
 * IgPgTg
 * ================================================================ */

void alwan_xyz_to_igpgtg(alwan_igpgtg *igpgtg, alwan_xyz const *xyz) {
    if (!xyz || !igpgtg) return;
    *igpgtg = alwan_xyz_to_igpgtg_v(*xyz);
}

void alwan_igpgtg_to_xyz(alwan_xyz *xyz, alwan_igpgtg const *igpgtg) {
    if (!igpgtg || !xyz) return;
    *xyz = alwan_igpgtg_to_xyz_v(*igpgtg);
}

/* ================================================================
 * ICaCb
 * ================================================================ */

void alwan_xyz_to_icacb(alwan_icacb *icacb, alwan_xyz const *xyz) {
    if (!xyz || !icacb) return;
    *icacb = alwan_xyz_to_icacb_v(*xyz);
}

void alwan_icacb_to_xyz(alwan_xyz *xyz, alwan_icacb const *icacb) {
    if (!icacb || !xyz) return;
    *xyz = alwan_icacb_to_xyz_v(*icacb);
}
