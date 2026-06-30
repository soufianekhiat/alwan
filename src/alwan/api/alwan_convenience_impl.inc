/* No include guard -- included twice (f32 and f64 passes) */

/* ----------------------------------------------------------------
 * RGB <-> HSV
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_rgb_to_hsv)(ALWAN_CORE_HSV *hsv_out, ALWAN_CORE_RGB const *rgb) {
    if (!rgb || !hsv_out) return ALWAN_E_INVALID;
    *hsv_out = ALWAN_CORE_FNVLIT(alwan_rgb_to_hsv)(*rgb);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_hsv_to_rgb)(ALWAN_CORE_RGB *rgb_out, ALWAN_CORE_HSV const *hsv) {
    if (!hsv || !rgb_out) return ALWAN_E_INVALID;
    *rgb_out = ALWAN_CORE_FNVLIT(alwan_hsv_to_rgb)(*hsv);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> HSL
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_rgb_to_hsl)(ALWAN_CORE_HSL *hsl_out, ALWAN_CORE_RGB const *rgb) {
    if (!rgb || !hsl_out) return ALWAN_E_INVALID;
    *hsl_out = ALWAN_CORE_FNVLIT(alwan_rgb_to_hsl)(*rgb);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_hsl_to_rgb)(ALWAN_CORE_RGB *rgb_out, ALWAN_CORE_HSL const *hsl) {
    if (!hsl || !rgb_out) return ALWAN_E_INVALID;
    *rgb_out = ALWAN_CORE_FNVLIT(alwan_hsl_to_rgb)(*hsl);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Linear sRGB <-> HSV
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_linear_srgb_to_hsv)(ALWAN_CORE_HSV *hsv_out, ALWAN_CORE_RGB const *rgb) {
    if (!rgb || !hsv_out) return ALWAN_E_INVALID;
    *hsv_out = ALWAN_CORE_FNVLIT(alwan_linear_srgb_to_hsv)(*rgb);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_hsv_to_linear_srgb)(ALWAN_CORE_RGB *rgb_out, ALWAN_CORE_HSV const *hsv) {
    if (!hsv || !rgb_out) return ALWAN_E_INVALID;
    *rgb_out = ALWAN_CORE_FNVLIT(alwan_hsv_to_linear_srgb)(*hsv);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Linear sRGB <-> HSL
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_linear_srgb_to_hsl)(ALWAN_CORE_HSL *hsl_out, ALWAN_CORE_RGB const *rgb) {
    if (!rgb || !hsl_out) return ALWAN_E_INVALID;
    *hsl_out = ALWAN_CORE_FNVLIT(alwan_linear_srgb_to_hsl)(*rgb);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_hsl_to_linear_srgb)(ALWAN_CORE_RGB *rgb_out, ALWAN_CORE_HSL const *hsl) {
    if (!hsl || !rgb_out) return ALWAN_E_INVALID;
    *rgb_out = ALWAN_CORE_FNVLIT(alwan_hsl_to_linear_srgb)(*hsl);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> HSP
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_rgb_to_hsp)(ALWAN_CORE_HSP *hsp_out, ALWAN_CORE_RGB const *rgb) {
    if (!rgb || !hsp_out) return ALWAN_E_INVALID;
    *hsp_out = ALWAN_CORE_FNVLIT(alwan_rgb_to_hsp)(*rgb);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_hsp_to_rgb)(ALWAN_CORE_RGB *rgb_out, ALWAN_CORE_HSP const *hsp) {
    if (!hsp || !rgb_out) return ALWAN_E_INVALID;
    *rgb_out = ALWAN_CORE_FNVLIT(alwan_hsp_to_rgb)(*hsp);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> HSPLog
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_rgb_to_hsplog)(ALWAN_CORE_HSPLOG *hsplog_out, ALWAN_CORE_RGB const *rgb) {
    if (!rgb || !hsplog_out) return ALWAN_E_INVALID;
    *hsplog_out = ALWAN_CORE_FNVLIT(alwan_rgb_to_hsplog)(*rgb);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_hsplog_to_rgb)(ALWAN_CORE_RGB *rgb_out, ALWAN_CORE_HSPLOG const *hsplog) {
    if (!hsplog || !rgb_out) return ALWAN_E_INVALID;
    *rgb_out = ALWAN_CORE_FNVLIT(alwan_hsplog_to_rgb)(*hsplog);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> HSY
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_rgb_to_hsy)(ALWAN_CORE_HSY *hsy_out, ALWAN_CORE_RGB const *rgb) {
    if (!rgb || !hsy_out) return ALWAN_E_INVALID;
    *hsy_out = ALWAN_CORE_FNVLIT(alwan_rgb_to_hsy)(*rgb);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_hsy_to_rgb)(ALWAN_CORE_RGB *rgb_out, ALWAN_CORE_HSY const *hsy) {
    if (!hsy || !rgb_out) return ALWAN_E_INVALID;
    *rgb_out = ALWAN_CORE_FNVLIT(alwan_hsy_to_rgb)(*hsy);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> CMY
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_rgb_to_cmy)(ALWAN_CORE_CMY *cmy_out, ALWAN_CORE_RGB const *rgb) {
    if (!rgb || !cmy_out) return ALWAN_E_INVALID;
    *cmy_out = ALWAN_CORE_FNVLIT(alwan_rgb_to_cmy)(*rgb);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_cmy_to_rgb)(ALWAN_CORE_RGB *rgb_out, ALWAN_CORE_CMY const *cmy) {
    if (!cmy || !rgb_out) return ALWAN_E_INVALID;
    *rgb_out = ALWAN_CORE_FNVLIT(alwan_cmy_to_rgb)(*cmy);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> YcCbcCrc
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_rgb_to_yccbccrc)(ALWAN_CORE_YCCBCCRC *yccbccrc_out, ALWAN_CORE_RGB const *rgb, int bit_depth) {
    if (!rgb || !yccbccrc_out) return ALWAN_E_INVALID;
    *yccbccrc_out = ALWAN_CORE_FNVLIT(alwan_rgb_to_yccbccrc)(*rgb, bit_depth);
    ALWAN_NORM_YCCBCCRC(yccbccrc_out);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_yccbccrc_to_rgb)(ALWAN_CORE_RGB *rgb_out, ALWAN_CORE_YCCBCCRC const *yccbccrc, int bit_depth) {
    if (!yccbccrc || !rgb_out) return ALWAN_E_INVALID;
    ALWAN_CORE_YCCBCCRC tmp = *yccbccrc;
    ALWAN_DENORM_YCCBCCRC(&tmp);
    *rgb_out = ALWAN_CORE_FNVLIT(alwan_yccbccrc_to_rgb)(tmp, bit_depth);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> YCoCg
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_rgb_to_ycocg)(ALWAN_CORE_YCOCG *ycocg_out, ALWAN_CORE_RGB const *rgb) {
    if (!rgb || !ycocg_out) return ALWAN_E_INVALID;
    *ycocg_out = ALWAN_CORE_FNVLIT(alwan_rgb_to_ycocg)(*rgb);
    ALWAN_NORM_YCOCG(ycocg_out);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_ycocg_to_rgb)(ALWAN_CORE_RGB *rgb_out, ALWAN_CORE_YCOCG const *ycocg) {
    if (!ycocg || !rgb_out) return ALWAN_E_INVALID;
    ALWAN_CORE_YCOCG tmp = *ycocg;
    ALWAN_DENORM_YCOCG(&tmp);
    *rgb_out = ALWAN_CORE_FNVLIT(alwan_ycocg_to_rgb)(tmp);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * YCbCr range conversion
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_ycbcr_full_to_legal)(ALWAN_CORE_YCBCR *out, ALWAN_CORE_YCBCR const *in, int bit_depth) {
    if (!in || !out) return ALWAN_E_INVALID;
    ALWAN_CORE_YCBCR tmp = *in;
    ALWAN_DENORM_YCBCR(&tmp);
    *out = ALWAN_CORE_FNVLIT(alwan_ycbcr_full_to_legal)(tmp, bit_depth);
    ALWAN_NORM_YCBCR(out);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_ycbcr_legal_to_full)(ALWAN_CORE_YCBCR *out, ALWAN_CORE_YCBCR const *in, int bit_depth) {
    if (!in || !out) return ALWAN_E_INVALID;
    ALWAN_CORE_YCBCR tmp = *in;
    ALWAN_DENORM_YCBCR(&tmp);
    *out = ALWAN_CORE_FNVLIT(alwan_ycbcr_legal_to_full)(tmp, bit_depth);
    ALWAN_NORM_YCBCR(out);
    return ALWAN_OK;
}
