/* No include guard -- included twice (f32 and f64 passes) */

/* ================================================================
 * Prismatic
 * ================================================================ */

void ALWAN_CORE_FNLIT(alwan_rgb_to_prismatic)(ALWAN_CORE_PRISMATIC *prismatic, ALWAN_CORE_RGB const *rgb) {
    if (!prismatic || !rgb) return;
    *prismatic = ALWAN_CORE_FNVLIT(alwan_rgb_to_prismatic)(*rgb);
}

void ALWAN_CORE_FNLIT(alwan_prismatic_to_rgb)(ALWAN_CORE_RGB *rgb, ALWAN_CORE_PRISMATIC const *prismatic) {
    if (!rgb || !prismatic) return;
    *rgb = ALWAN_CORE_FNVLIT(alwan_prismatic_to_rgb)(*prismatic);
}

/* ================================================================
 * HCL
 * ================================================================ */

void ALWAN_CORE_FNLIT(alwan_rgb_to_hcl)(ALWAN_CORE_HCL *hcl, ALWAN_CORE_RGB const *rgb) {
    if (!hcl || !rgb) return;
    *hcl = ALWAN_CORE_FNVLIT(alwan_rgb_to_hcl)(*rgb);
    ALWAN_NORM_HCL(hcl);
}

void ALWAN_CORE_FNLIT(alwan_hcl_to_rgb)(ALWAN_CORE_RGB *rgb, ALWAN_CORE_HCL const *hcl) {
    if (!rgb || !hcl) return;
    ALWAN_CORE_HCL tmp = *hcl;
    ALWAN_DENORM_HCL(&tmp);
    *rgb = ALWAN_CORE_FNVLIT(alwan_hcl_to_rgb)(tmp);
}

/* ================================================================
 * IHLS
 * ================================================================ */

void ALWAN_CORE_FNLIT(alwan_rgb_to_ihls)(ALWAN_CORE_IHLS *ihls, ALWAN_CORE_RGB const *rgb) {
    if (!ihls || !rgb) return;
    *ihls = ALWAN_CORE_FNVLIT(alwan_rgb_to_ihls)(*rgb);
    ALWAN_NORM_IHLS(ihls);
}

void ALWAN_CORE_FNLIT(alwan_ihls_to_rgb)(ALWAN_CORE_RGB *rgb, ALWAN_CORE_IHLS const *ihls) {
    if (!rgb || !ihls) return;
    ALWAN_CORE_IHLS tmp = *ihls;
    ALWAN_DENORM_IHLS(&tmp);
    *rgb = ALWAN_CORE_FNVLIT(alwan_ihls_to_rgb)(tmp);
}

/* ================================================================
 * HLC (cylindrical CIELAB with H,L,C ordering)
 * ================================================================ */

void ALWAN_CORE_FNLIT(alwan_lch_to_hlc)(ALWAN_CORE_HLC *hlc, ALWAN_CORE_LCH const *lch) {
    if (!hlc || !lch) return;
    ALWAN_CORE_LCH tmp = *lch;
    ALWAN_DENORM_LCH(&tmp);
    *hlc = ALWAN_CORE_FNVLIT(alwan_lch_to_hlc)(tmp);
    ALWAN_NORM_HLC(hlc);
}

void ALWAN_CORE_FNLIT(alwan_hlc_to_lch)(ALWAN_CORE_LCH *lch, ALWAN_CORE_HLC const *hlc) {
    if (!lch || !hlc) return;
    ALWAN_CORE_HLC tmp = *hlc;
    ALWAN_DENORM_HLC(&tmp);
    *lch = ALWAN_CORE_FNVLIT(alwan_hlc_to_lch)(tmp);
    ALWAN_NORM_LCH(lch);
}

/* ================================================================
 * Cubehelix
 * ================================================================ */

void ALWAN_CORE_FNLIT(alwan_cubehelix_to_rgb)(ALWAN_CORE_RGB *rgb, ALWAN_CORE_CUBEHELIX const *ch) {
    if (!rgb || !ch) return;
    ALWAN_CORE_CUBEHELIX tmp = *ch;
    ALWAN_DENORM_CUBEHELIX(&tmp);
    *rgb = ALWAN_CORE_FNVLIT(alwan_cubehelix_to_rgb)(tmp);
}

void ALWAN_CORE_FNLIT(alwan_rgb_to_cubehelix)(ALWAN_CORE_CUBEHELIX *ch, ALWAN_CORE_RGB const *rgb) {
    if (!ch || !rgb) return;
    *ch = ALWAN_CORE_FNVLIT(alwan_rgb_to_cubehelix)(*rgb);
    ALWAN_NORM_CUBEHELIX(ch);
}

/* ================================================================
 * HSLuv / HPLuv
 * ================================================================ */

void ALWAN_CORE_FNLIT(alwan_hsluv_to_srgb)(ALWAN_CORE_RGB *rgb, ALWAN_CORE_HSLUV const *hsluv) {
    if (!rgb || !hsluv) return;
    ALWAN_CORE_HSLUV tmp = *hsluv;
    ALWAN_DENORM_HSLUV(&tmp);
    *rgb = ALWAN_CORE_FNVLIT(alwan_hsluv_to_srgb)(tmp);
}

void ALWAN_CORE_FNLIT(alwan_srgb_to_hsluv)(ALWAN_CORE_HSLUV *hsluv, ALWAN_CORE_RGB const *srgb) {
    if (!hsluv || !srgb) return;
    *hsluv = ALWAN_CORE_FNVLIT(alwan_srgb_to_hsluv)(*srgb);
    ALWAN_NORM_HSLUV(hsluv);
}

void ALWAN_CORE_FNLIT(alwan_hpluv_to_srgb)(ALWAN_CORE_RGB *rgb, ALWAN_CORE_HPLUV const *hpluv) {
    if (!rgb || !hpluv) return;
    ALWAN_CORE_HPLUV tmp = *hpluv;
    ALWAN_DENORM_HPLUV(&tmp);
    *rgb = ALWAN_CORE_FNVLIT(alwan_hpluv_to_srgb)(tmp);
}

void ALWAN_CORE_FNLIT(alwan_srgb_to_hpluv)(ALWAN_CORE_HPLUV *hpluv, ALWAN_CORE_RGB const *srgb) {
    if (!hpluv || !srgb) return;
    *hpluv = ALWAN_CORE_FNVLIT(alwan_srgb_to_hpluv)(*srgb);
    ALWAN_NORM_HPLUV(hpluv);
}

/* ================================================================
 * Okhsl / Okhsv
 * ================================================================ */

void ALWAN_CORE_FNLIT(alwan_okhsl_to_srgb)(ALWAN_CORE_RGB *rgb, ALWAN_CORE_OKHSL const *okhsl) {
    if (!rgb || !okhsl) return;
    *rgb = ALWAN_CORE_FNVLIT(alwan_okhsl_to_srgb)(*okhsl);
}

void ALWAN_CORE_FNLIT(alwan_srgb_to_okhsl)(ALWAN_CORE_OKHSL *okhsl, ALWAN_CORE_RGB const *srgb) {
    if (!okhsl || !srgb) return;
    *okhsl = ALWAN_CORE_FNVLIT(alwan_srgb_to_okhsl)(*srgb);
}

void ALWAN_CORE_FNLIT(alwan_okhsv_to_srgb)(ALWAN_CORE_RGB *rgb, ALWAN_CORE_OKHSV const *okhsv) {
    if (!rgb || !okhsv) return;
    *rgb = ALWAN_CORE_FNVLIT(alwan_okhsv_to_srgb)(*okhsv);
}

void ALWAN_CORE_FNLIT(alwan_srgb_to_okhsv)(ALWAN_CORE_OKHSV *okhsv, ALWAN_CORE_RGB const *srgb) {
    if (!okhsv || !srgb) return;
    *okhsv = ALWAN_CORE_FNVLIT(alwan_srgb_to_okhsv)(*srgb);
}

/* ================================================================
 * hdr-CIELAB
 * ================================================================ */

void ALWAN_CORE_FNLIT(alwan_xyz_to_hdr_cielab)(ALWAN_CORE_LAB *hdr_lab, ALWAN_CORE_XYZ const *xyz) {
    if (!xyz || !hdr_lab) return;
    *hdr_lab = ALWAN_CORE_FNVLIT(alwan_xyz_to_hdr_cielab)(*xyz);
}

void ALWAN_CORE_FNLIT(alwan_hdr_cielab_to_xyz)(ALWAN_CORE_XYZ *xyz, ALWAN_CORE_LAB const *hdr_lab) {
    if (!hdr_lab || !xyz) return;
    *xyz = ALWAN_CORE_FNVLIT(alwan_hdr_cielab_to_xyz)(*hdr_lab);
}

/* ================================================================
 * hdr-IPT
 * ================================================================ */

void ALWAN_CORE_FNLIT(alwan_xyz_to_hdr_ipt)(ALWAN_CORE_IPT *hdr_ipt, ALWAN_CORE_XYZ const *xyz) {
    if (!xyz || !hdr_ipt) return;
    *hdr_ipt = ALWAN_CORE_FNVLIT(alwan_xyz_to_hdr_ipt)(*xyz);
}

void ALWAN_CORE_FNLIT(alwan_hdr_ipt_to_xyz)(ALWAN_CORE_XYZ *xyz, ALWAN_CORE_IPT const *hdr_ipt) {
    if (!hdr_ipt || !xyz) return;
    *xyz = ALWAN_CORE_FNVLIT(alwan_hdr_ipt_to_xyz)(*hdr_ipt);
}

/* ================================================================
 * IgPgTg
 * ================================================================ */

void ALWAN_CORE_FNLIT(alwan_xyz_to_igpgtg)(ALWAN_CORE_IGPGTG *igpgtg, ALWAN_CORE_XYZ const *xyz) {
    if (!xyz || !igpgtg) return;
    *igpgtg = ALWAN_CORE_FNVLIT(alwan_xyz_to_igpgtg)(*xyz);
}

void ALWAN_CORE_FNLIT(alwan_igpgtg_to_xyz)(ALWAN_CORE_XYZ *xyz, ALWAN_CORE_IGPGTG const *igpgtg) {
    if (!igpgtg || !xyz) return;
    *xyz = ALWAN_CORE_FNVLIT(alwan_igpgtg_to_xyz)(*igpgtg);
}

/* ================================================================
 * ICaCb
 * ================================================================ */

void ALWAN_CORE_FNLIT(alwan_xyz_to_icacb)(ALWAN_CORE_ICACB *icacb, ALWAN_CORE_XYZ const *xyz) {
    if (!xyz || !icacb) return;
    *icacb = ALWAN_CORE_FNVLIT(alwan_xyz_to_icacb)(*xyz);
}

void ALWAN_CORE_FNLIT(alwan_icacb_to_xyz)(ALWAN_CORE_XYZ *xyz, ALWAN_CORE_ICACB const *icacb) {
    if (!icacb || !xyz) return;
    *xyz = ALWAN_CORE_FNVLIT(alwan_icacb_to_xyz)(*icacb);
}
