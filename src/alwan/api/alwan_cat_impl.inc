/* No include guard -- included twice (f32 and f64 passes) */

/* The cone-response matrix behind a method, or NULL where the method has no fixed matrix:
 * XYZ scaling is a diagonal built from the white points, and Zhai 2018 is a model rather
 * than a matrix. Returns the native-precision data twin, so the f32 path reads float data
 * (ALWAN_CORE_FNLIT(g_cat_NAME) -> g_cat_NAME_f32 / g_cat_NAME_f64). */
static ALWAN_CORE_T const *ALWAN_CORE_FNLIT(cat_matrix_data)(alwan_cat_method method) {
    switch (method) {
        case ALWAN_CAT_BRADFORD:         return ALWAN_CORE_FNLIT(g_cat_bradford);
        case ALWAN_CAT_CAT02:            return ALWAN_CORE_FNLIT(g_cat_cat02);
        case ALWAN_CAT_CAT16:            return ALWAN_CORE_FNLIT(g_cat_cat16);
        case ALWAN_CAT_SHARP:            return ALWAN_CORE_FNLIT(g_cat_sharp);
        case ALWAN_CAT_FAIRCHILD:        return ALWAN_CORE_FNLIT(g_cat_fairchild);
        case ALWAN_CAT_CMCCAT97:         return ALWAN_CORE_FNLIT(g_cat_cmccat97);
        case ALWAN_CAT_CMCCAT2000:       return ALWAN_CORE_FNLIT(g_cat_cmccat2000);
        case ALWAN_CAT_CAT02_BRILL_2008: return ALWAN_CORE_FNLIT(g_cat_cat02_brill_2008);
        case ALWAN_CAT_BIANCO_2010:      return ALWAN_CORE_FNLIT(g_cat_bianco_2010);
        case ALWAN_CAT_BIANCO_PC_2010:   return ALWAN_CORE_FNLIT(g_cat_bianco_pc_2010);
        case ALWAN_CAT_VON_KRIES:        return ALWAN_CORE_FNLIT(g_cat_von_kries);
        default: return NULL;
    }
}

/* Load a method's matrix and its inverse. */
static alwan_status ALWAN_CORE_FNLIT(cat_matrix_pair)(ALWAN_CORE_MAT3X3 *M_out,
                                                      ALWAN_CORE_MAT3X3 *M_inv_out,
                                                      alwan_cat_method method) {
    ALWAN_CORE_T const *M_data = ALWAN_CORE_FNLIT(cat_matrix_data)(method);
    if (!M_data) {
        return ALWAN_E_INVALID;
    }
    memcpy(M_out->m, M_data, sizeof(M_out->m));
    return (alwan_status)ALWAN_CORE_FNLIT(alwan_mat3_inv)(M_inv_out, M_out);
}

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

    ALWAN_CORE_T const *M_data = ALWAN_CORE_FNLIT(cat_matrix_data)(method);
    if (!M_data) {
        return ALWAN_E_INVALID;
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

/* ----------------------------------------------------------------
 * CIE 1994 Chromatic Adaptation Model (CIE 109-1994)
 *
 * The cone space is not the caller's to choose: the model is defined in the
 * von Kries space and its coefficients are fitted to it.
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_cat_cie1994)(ALWAN_CORE_XYZ *xyz_out,
                       ALWAN_CORE_XYZ const *xyz_in,
                       ALWAN_CORE_VEC2 const *xy_o1,
                       ALWAN_CORE_VEC2 const *xy_o2,
                       ALWAN_CORE_T Y_o,
                       ALWAN_CORE_T E_o1,
                       ALWAN_CORE_T E_o2,
                       ALWAN_CORE_T n) {
    if (!xyz_out || !xyz_in || !xy_o1 || !xy_o2) {
        return ALWAN_E_INVALID;
    }

    /* xi, eta and zeta all divide by the adapting field's y */
    if (xy_o1->v[1] <= ALWAN_CORE_LITERAL(0.0) || xy_o2->v[1] <= ALWAN_CORE_LITERAL(0.0)) {
        return ALWAN_E_INVALID;
    }

    ALWAN_CORE_MAT3X3 M, M_inv;
    alwan_status status = ALWAN_CORE_FNLIT(cat_matrix_pair)(&M, &M_inv, ALWAN_CAT_VON_KRIES);
    if (status != ALWAN_OK) {
        return status;
    }

    *xyz_out = ALWAN_CORE_FNVLIT(alwan_cat_cie1994)(M, M_inv, *xyz_in,
                                  xy_o1->v[0], xy_o1->v[1], xy_o2->v[0], xy_o2->v[1],
                                  Y_o, E_o1, E_o2, n);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * vK20 Chromatic Adaptation Model (Fairchild 2020)
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_cat_vk20)(ALWAN_CORE_XYZ *xyz_out,
                       ALWAN_CORE_XYZ const *xyz_in,
                       ALWAN_CORE_XYZ const *xyz_p,
                       ALWAN_CORE_XYZ const *xyz_n,
                       ALWAN_CORE_XYZ const *xyz_r,
                       ALWAN_CORE_T D_n,
                       ALWAN_CORE_T D_r,
                       ALWAN_CORE_T D_p,
                       alwan_cat_method transform) {
    if (!xyz_out || !xyz_in || !xyz_p || !xyz_n) {
        return ALWAN_E_INVALID;
    }

    /* The model's own reference illuminant, on the Y = 1 scale it is defined on */
    ALWAN_CORE_XYZ reference_default = { ALWAN_CORE_LITERAL(0.97941176),
                                         ALWAN_CORE_LITERAL(1.0),
                                         ALWAN_CORE_LITERAL(1.73235294) };
    ALWAN_CORE_XYZ const *xyz_ref = xyz_r ? xyz_r : &reference_default;

    ALWAN_CORE_MAT3X3 M, M_inv;
    alwan_status status = ALWAN_CORE_FNLIT(cat_matrix_pair)(&M, &M_inv, transform);
    if (status != ALWAN_OK) {
        return status;
    }

    *xyz_out = ALWAN_CORE_FNVLIT(alwan_cat_vk20)(M, M_inv, *xyz_in, *xyz_p, *xyz_n, *xyz_ref,
                                D_n, D_r, D_p);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Li 2025 Chromatic Adaptation Model
 *
 * Defined in CAT16 cone space, so the matrix is not a parameter.
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_cat_li2025)(ALWAN_CORE_XYZ *xyz_out,
                       ALWAN_CORE_XYZ const *xyz_in,
                       ALWAN_CORE_XYZ const *xyz_ws,
                       ALWAN_CORE_XYZ const *xyz_wd,
                       ALWAN_CORE_T L_A,
                       ALWAN_CORE_T F_surround,
                       int discount_illuminant) {
    if (!xyz_out || !xyz_in || !xyz_ws || !xyz_wd) {
        return ALWAN_E_INVALID;
    }

    ALWAN_CORE_MAT3X3 M, M_inv;
    alwan_status status = ALWAN_CORE_FNLIT(cat_matrix_pair)(&M, &M_inv, ALWAN_CAT_CAT16);
    if (status != ALWAN_OK) {
        return status;
    }

    *xyz_out = ALWAN_CORE_FNVLIT(alwan_cat_li2025)(M, M_inv, *xyz_in, *xyz_ws, *xyz_wd,
                                 L_A, F_surround, discount_illuminant);

    return ALWAN_OK;
}
