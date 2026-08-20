/* No include guard -- included twice (f32 and f64 passes) */

alwan_status ALWAN_CORE_FNLIT(alwan_cat_matrix)(ALWAN_CORE_MAT3X3 *out,
                     ALWAN_CORE_XYZ const *src_white_xyz,
                     ALWAN_CORE_XYZ const *dst_white_xyz,
                     alwan_cat_method method) {
    if (!src_white_xyz || !dst_white_xyz || !out) {
        return ALWAN_E_INVALID;
    }

    if (src_white_xyz->y < ALWAN_CORE_LITERAL(1e-10) || dst_white_xyz->y < ALWAN_CORE_LITERAL(1e-10)) {
        return ALWAN_E_INVALID;
    }

    /* Handle XYZ scaling separately */
    if (method == ALWAN_CAT_XYZ_SCALING) {
        *out = ALWAN_CORE_FNVLIT(alwan_cat_xyz_scaling)(*src_white_xyz, *dst_white_xyz);
        return ALWAN_OK;
    }

    /* Select the CAT matrix M directly from the native-precision data twin
     * (ALWAN_CORE_FNLIT(g_cat_NAME) -> g_cat_NAME_f32 / g_cat_NAME_f64). */
    ALWAN_CORE_T const *M_data;
    switch (method) {
        case ALWAN_CAT_BRADFORD:         M_data = ALWAN_CORE_FNLIT(g_cat_bradford);         break;
        case ALWAN_CAT_CAT02:            M_data = ALWAN_CORE_FNLIT(g_cat_cat02);            break;
        case ALWAN_CAT_CAT16:            M_data = ALWAN_CORE_FNLIT(g_cat_cat16);            break;
        case ALWAN_CAT_SHARP:            M_data = ALWAN_CORE_FNLIT(g_cat_sharp);            break;
        case ALWAN_CAT_FAIRCHILD:        M_data = ALWAN_CORE_FNLIT(g_cat_fairchild);        break;
        case ALWAN_CAT_CMCCAT97:         M_data = ALWAN_CORE_FNLIT(g_cat_cmccat97);         break;
        case ALWAN_CAT_CMCCAT2000:       M_data = ALWAN_CORE_FNLIT(g_cat_cmccat2000);       break;
        case ALWAN_CAT_CAT02_BRILL_2008: M_data = ALWAN_CORE_FNLIT(g_cat_cat02_brill_2008); break;
        case ALWAN_CAT_BIANCO_2010:      M_data = ALWAN_CORE_FNLIT(g_cat_bianco_2010);      break;
        case ALWAN_CAT_BIANCO_PC_2010:   M_data = ALWAN_CORE_FNLIT(g_cat_bianco_pc_2010);   break;
        default: return ALWAN_E_INVALID;
    }

    ALWAN_CORE_MAT3X3 M;
    memcpy(M.m, M_data, sizeof(M.m));

    /* Compute M^-1 */
    ALWAN_CORE_MAT3X3 M_inv;
    int status = ALWAN_CORE_FNLIT(alwan_mat3_inv)(&M_inv, &M);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Delegate to core: M_adapt = M^-1 * diag(rgb_dst / rgb_src) * M */
    *out = ALWAN_CORE_FNVLIT(alwan_cat_matrix)(M, M_inv, *src_white_xyz, *dst_white_xyz);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Bulk Chromatic Adaptation
 * Computes the adaptation matrix once, then applies it to every
 * element in a stride-based buffer.
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_xyz_adapt)(ALWAN_CORE_T *xyz_out, size_t out_stride,
                     ALWAN_CORE_T const *xyz_in, size_t in_stride, size_t count,
                     ALWAN_CORE_XYZ const *src_white_xyz,
                     ALWAN_CORE_XYZ const *dst_white_xyz,
                     alwan_cat_method method) {
    if (!xyz_in || !xyz_out || !src_white_xyz || !dst_white_xyz) {
        return ALWAN_E_INVALID;
    }

    /* Compute adaptation matrix once */
    ALWAN_CORE_MAT3X3 cat_mat;
    int status = ALWAN_CORE_FNLIT(alwan_cat_matrix)(&cat_mat, src_white_xyz, dst_white_xyz, method);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Apply to all colors (strides are in bytes) */
    for (size_t i = 0; i < count; i++) {
        ALWAN_CORE_T const *in_ptr = (ALWAN_CORE_T const *)((char const *)xyz_in + i * in_stride);
        ALWAN_CORE_T *out_ptr = (ALWAN_CORE_T *)((char *)xyz_out + i * out_stride);

        /* Build an XYZ from the strided input */
        ALWAN_CORE_XYZ xyz_input;
        xyz_input.x = in_ptr[0];
        xyz_input.y = in_ptr[1];
        xyz_input.z = in_ptr[2];

        /* Delegate to core single-color adaptation */
        ALWAN_CORE_XYZ xyz_adapted = ALWAN_CORE_FNVLIT(alwan_cat_adapt)(cat_mat, xyz_input);

        /* Store output color */
        out_ptr[0] = xyz_adapted.x;
        out_ptr[1] = xyz_adapted.y;
        out_ptr[2] = xyz_adapted.z;
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Zhai & Luo 2018 Two-Step Chromatic Adaptation
 * Reference: Zhai, Luo (2018) "Study of chromatic adaptation via
 *            neutral white matches on different viewing media"
 *            Optics Express, 26(6), 7724.
 *
 * Selects the cone-response matrix M directly from the
 * native-precision data twin (g_cat_NAME_f32 / g_cat_NAME_f64).
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_cat_zhai2018)(ALWAN_CORE_XYZ *xyz_out,
                       ALWAN_CORE_XYZ const *xyz_in,
                       ALWAN_CORE_XYZ const *xyz_src,
                       ALWAN_CORE_XYZ const *xyz_dst,
                       ALWAN_CORE_T D_src,
                       ALWAN_CORE_T D_dst,
                       ALWAN_CORE_XYZ const *xyz_baseline,
                       alwan_cat_method transform) {
    if (!xyz_in || !xyz_src || !xyz_dst || !xyz_out) {
        return ALWAN_E_INVALID;
    }

    /* Validate degree of adaptation [0, 1] */
    if (D_src < ALWAN_CORE_LITERAL(0.0) || D_src > ALWAN_CORE_LITERAL(1.0) ||
        D_dst < ALWAN_CORE_LITERAL(0.0) || D_dst > ALWAN_CORE_LITERAL(1.0)) {
        return ALWAN_E_RANGE;
    }

    /* Only CAT02 and CAT16 are supported for Zhai 2018 */
    if (transform != ALWAN_CAT_CAT02 && transform != ALWAN_CAT_CAT16) {
        return ALWAN_E_INVALID;
    }

    /* Default baseline illuminant is equal-energy white (E) */
    ALWAN_CORE_XYZ baseline_default = { ALWAN_CORE_LITERAL(100.0), ALWAN_CORE_LITERAL(100.0), ALWAN_CORE_LITERAL(100.0) };
    ALWAN_CORE_XYZ const *xyz_o = xyz_baseline ? xyz_baseline : &baseline_default;

    /* Select the CAT matrix M for the chosen method from the native-precision twin */
    ALWAN_CORE_T const *M_data = (transform == ALWAN_CAT_CAT02)
        ? ALWAN_CORE_FNLIT(g_cat_cat02)
        : ALWAN_CORE_FNLIT(g_cat_cat16);
    ALWAN_CORE_MAT3X3 M;
    memcpy(M.m, M_data, sizeof(M.m));

    /* Compute M inverse */
    ALWAN_CORE_MAT3X3 M_inv;
    int status = ALWAN_CORE_FNLIT(alwan_mat3_inv)(&M_inv, &M);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Delegate to core two-step adaptation */
    *xyz_out = ALWAN_CORE_FNVLIT(alwan_cat_zhai2018)(M, M_inv, *xyz_in, *xyz_src, *xyz_dst,
                                    D_src, D_dst, *xyz_o);

    return ALWAN_OK;
}
