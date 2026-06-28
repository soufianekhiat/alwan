/* No include guard — included twice (f32 and f64 passes) */

int ALWAN_CORE_FNLIT(alwan_cat_matrix)(ALWAN_CORE_MAT3X3 *out,
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
