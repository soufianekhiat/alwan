/* No include guard -- included twice (f32 and f64 passes) */

alwan_status ALWAN_CORE_FNLIT(alwan_linear_srgb_to_xyb)(ALWAN_CORE_XYB *xyb_out, ALWAN_CORE_RGB const *rgb) {
    if (!rgb || !xyb_out) return ALWAN_E_INVALID;
    *xyb_out = ALWAN_CORE_FNVLIT(alwan_linear_srgb_to_xyb)(*rgb);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_xyb_to_linear_srgb)(ALWAN_CORE_RGB *rgb_out, ALWAN_CORE_XYB const *xyb) {
    if (!xyb || !rgb_out) return ALWAN_E_INVALID;
    *rgb_out = ALWAN_CORE_FNVLIT(alwan_xyb_to_linear_srgb)(*xyb);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_xyz_to_xyb)(ALWAN_CORE_XYB *xyb_out, ALWAN_CORE_XYZ const *xyz) {
    if (!xyz || !xyb_out) return ALWAN_E_INVALID;
    *xyb_out = ALWAN_CORE_FNVLIT(alwan_xyz_to_xyb)(*xyz);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_xyb_to_xyz)(ALWAN_CORE_XYZ *xyz_out, ALWAN_CORE_XYB const *xyb) {
    if (!xyb || !xyz_out) return ALWAN_E_INVALID;
    *xyz_out = ALWAN_CORE_FNVLIT(alwan_xyb_to_xyz)(*xyb);
    return ALWAN_OK;
}
