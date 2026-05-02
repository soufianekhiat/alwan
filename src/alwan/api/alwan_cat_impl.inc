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

    /* Get the CAT matrix M (constant data is f64; narrow to working precision) */
    alwan_mat3x3_f64 M64;
    switch (method) {
        case ALWAN_CAT_BRADFORD:       get_bradford_matrix(&M64);       break;
        case ALWAN_CAT_CAT02:          get_cat02_matrix(&M64);          break;
        case ALWAN_CAT_CAT16:          get_cat16_matrix(&M64);          break;
        case ALWAN_CAT_SHARP:          get_sharp_matrix(&M64);          break;
        case ALWAN_CAT_FAIRCHILD:      get_fairchild_matrix(&M64);      break;
        case ALWAN_CAT_CMCCAT97:       get_cmccat97_matrix(&M64);       break;
        case ALWAN_CAT_CMCCAT2000:     get_cmccat2000_matrix(&M64);     break;
        case ALWAN_CAT_CAT02_BRILL_2008: get_cat02_brill_2008_matrix(&M64); break;
        case ALWAN_CAT_BIANCO_2010:    get_bianco_2010_matrix(&M64);    break;
        case ALWAN_CAT_BIANCO_PC_2010: get_bianco_pc_2010_matrix(&M64); break;
        default: return ALWAN_E_INVALID;
    }

    ALWAN_CORE_MAT3X3 M;
    for (int i = 0; i < 9; i++) M.m[i] = (ALWAN_CORE_T)M64.m[i];

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
