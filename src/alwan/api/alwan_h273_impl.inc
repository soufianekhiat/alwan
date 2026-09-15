/*
 * ITU-T H.273 getters that return numbers, instantiated per precision by
 * alwan_h273.c.
 */

alwan_status ALWAN_CORE_FNLIT(alwan_h273_color_primaries)(ALWAN_CORE_T primaries_xy[6], ALWAN_CORE_T white_xy[2],
                                                         int color_primaries) {
    alwan__h273_primaries const *p;
    int k;
    if (!primaries_xy || !white_xy) return ALWAN_E_INVALID;
    p = alwan__h273_find_primaries(color_primaries);
    if (!p) return alwan__h273_unlisted(color_primaries);
    for (k = 0; k < 6; k++) primaries_xy[k] = (ALWAN_CORE_T)p->xy[k];
    white_xy[0] = (ALWAN_CORE_T)p->xy[6];
    white_xy[1] = (ALWAN_CORE_T)p->xy[7];
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_h273_matrix_coefficients)(ALWAN_CORE_T *kr_out, ALWAN_CORE_T *kb_out,
                                                             int matrix_coefficients, int color_primaries) {
    size_t i;
    if (!kr_out || !kb_out) return ALWAN_E_INVALID;
    if (matrix_coefficients == 12 || matrix_coefficients == 13) {
        /* Chromaticity-derived, H.273 equations 32 to 37: the Y row of the primaries'
         * RGB to XYZ matrix. 13 is the constant-luminance form of the same Kr and Kb. */
        alwan__h273_primaries const *p = alwan__h273_find_primaries(color_primaries);
        ALWAN_CORE_RGB_SPACE_DESC desc;
        ALWAN_CORE_MAT3X3 to_xyz, from_xyz;
        alwan_status st;
        int k;
        if (!p) return alwan__h273_unlisted(color_primaries);
        memset(&desc, 0, sizeof desc);
        for (k = 0; k < 6; k++) desc.primaries_xy[k] = (ALWAN_CORE_T)p->xy[k];
        desc.white_xy[0] = (ALWAN_CORE_T)p->xy[6];
        desc.white_xy[1] = (ALWAN_CORE_T)p->xy[7];
        desc.oetf = ALWAN_TF_LINEAR;
        desc.eotf = ALWAN_TF_LINEAR;
        st = ALWAN_CORE_FNLIT(alwan_rgb_derive_matrices)(&to_xyz, &from_xyz, &desc);
        if (st != ALWAN_OK) return st;
        *kr_out = to_xyz.m[3];
        *kb_out = to_xyz.m[5];
        return ALWAN_OK;
    }
    for (i = 0; i < sizeof(g_h273_matrix) / sizeof(g_h273_matrix[0]); i++) {
        if (g_h273_matrix[i].code == matrix_coefficients) {
            *kr_out = (ALWAN_CORE_T)g_h273_matrix[i].kr;
            *kb_out = (ALWAN_CORE_T)g_h273_matrix[i].kb;
            return ALWAN_OK;
        }
    }
    /* Identity, YCgCo, Y'D'zD'x and ICtCp are specified but have no Kr and Kb. */
    if (matrix_coefficients == 0 || matrix_coefficients == 8 || matrix_coefficients == 11 ||
        matrix_coefficients == 14) {
        return ALWAN_E_NODATA;
    }
    return alwan__h273_unlisted(matrix_coefficients);
}
