/* No include guard -- included twice (f32 and f64 passes).
 *
 * API tier for the table readers. Everything here is loop-invariant argument
 * validation plus one call into the core reader: null checks, rank/mode
 * acceptance, and the ALWAN_SAMPLE_STRICT path. The addressing gate and the
 * blends live in core/alwan_table_core.inc and are shared with the kernels.
 *
 * A mode the rank cannot honour is REJECTED here with ALWAN_E_INVALID, never
 * quietly downgraded. Silent substitution is how a colour pipeline ships the
 * wrong curve and nobody notices for two releases. The core reader has no
 * status channel, so rejection has to happen at this tier -- once, before any
 * loop, which makes it free.
 */

/* STRICT is scalar-only in this phase. A bulk reader would have to test per
 * element; the branchless accumulate is designed but not shipped, so a bulk
 * caller gets ALWAN_E_INVALID rather than a silently ignored flag. */
#define ALWAN_TABLE_STRICT_CHECK_1(coord)                                        \
    if (ALWAN_SAMPLE_IS_STRICT(mode) &&                                          \
        !ALWAN_CORE_FNVLIT(alwan_table_coord_in_range)(coord)) return ALWAN_E_RANGE

#define ALWAN_TABLE_STRICT_CHECK_3(c)                                            \
    if (ALWAN_SAMPLE_IS_STRICT(mode) &&                                          \
        !(ALWAN_CORE_FNVLIT(alwan_table_coord_in_range)((c)->r) &&                \
          ALWAN_CORE_FNVLIT(alwan_table_coord_in_range)((c)->g) &&                \
          ALWAN_CORE_FNVLIT(alwan_table_coord_in_range)((c)->b))) return ALWAN_E_RANGE

/* Rank 1 accepts NEAREST, LINEAR and CATMULL_ROM. BILINEAR, TRILINEAR and
 * TETRAHEDRAL are nonsense here and stay rejected. */
static int ALWAN_CORE_FN(table_mode_ok_rank1)(alwan_sample_mode mode) {
    int const base = ALWAN_SAMPLE_BASE(mode);
    return base == ALWAN_SAMPLE_NEAREST || base == ALWAN_SAMPLE_LINEAR ||
           base == ALWAN_SAMPLE_CATMULL_ROM;
}

/* The 2-d grid accepts NEAREST and BILINEAR, plus LINEAR meaning "the linear
 * one for this rank", the same allowance rank 3 makes for the zero value. */
static int ALWAN_CORE_FN(table_mode_ok_grid2d)(alwan_sample_mode mode) {
    int const base = ALWAN_SAMPLE_BASE(mode);
    return base == ALWAN_SAMPLE_NEAREST || base == ALWAN_SAMPLE_BILINEAR ||
           base == ALWAN_SAMPLE_LINEAR;
}

/* The 2-d strip is a flattened cube: pass TRILINEAR, or LINEAR to mean "the
 * linear one for this rank". BILINEAR is still rejected rather than accepted
 * as a friendly spelling for TRILINEAR. */
static int ALWAN_CORE_FN(table_mode_ok_strip)(alwan_sample_mode mode) {
    int const base = ALWAN_SAMPLE_BASE(mode);
    return base == ALWAN_SAMPLE_NEAREST || base == ALWAN_SAMPLE_TRILINEAR ||
           base == ALWAN_SAMPLE_LINEAR;
}

/* LINEAR is accepted at rank 3 and resolves to trilinear. It is the zero
 * value, so rejecting it would turn a zero-initialized mode into
 * ALWAN_E_INVALID instead of the interpolated read the caller expects. */
static int ALWAN_CORE_FN(table_mode_ok_rank3)(alwan_sample_mode mode) {
    int const base = ALWAN_SAMPLE_BASE(mode);
    return base == ALWAN_SAMPLE_NEAREST || base == ALWAN_SAMPLE_TRILINEAR ||
           base == ALWAN_SAMPLE_TETRAHEDRAL || base == ALWAN_SAMPLE_LINEAR;
}

/* ----------------------------------------------------------------
 * Caller-supplied tables
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_table1d_sample)(ALWAN_CORE_T *result,
                        ALWAN_CORE_T const *table, int size,
                        ALWAN_CORE_T coord, alwan_sample_mode mode) {
    if (!result || !table || size < 2) return ALWAN_E_INVALID;
    if (!ALWAN_CORE_FN(table_mode_ok_rank1)(mode)) return ALWAN_E_INVALID;
    ALWAN_TABLE_STRICT_CHECK_1(coord);
    *result = ALWAN_CORE_FNVLIT(alwan_table1d_sample)(table, size, coord, mode);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_table2d_grid_sample)(ALWAN_CORE_T *result,
                        ALWAN_CORE_T const *table, int rows, int stride,
                        ALWAN_CORE_T row_coord, ALWAN_CORE_T col_coord,
                        alwan_sample_mode mode) {
    if (!result || !table || rows < 2 || stride < 2) return ALWAN_E_INVALID;
    if (!ALWAN_CORE_FN(table_mode_ok_grid2d)(mode)) return ALWAN_E_INVALID;
    ALWAN_TABLE_STRICT_CHECK_1(row_coord);
    ALWAN_TABLE_STRICT_CHECK_1(col_coord);
    *result = ALWAN_CORE_FNVLIT(alwan_table2d_grid_sample)(
        table, rows, stride, row_coord, col_coord, mode);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_table2d_sample)(ALWAN_CORE_RGB *result,
                        ALWAN_CORE_T const *strip, int size,
                        ALWAN_CORE_RGB const *coord, alwan_sample_mode mode) {
    if (!result || !strip || !coord || size < 2) return ALWAN_E_INVALID;
    if (!ALWAN_CORE_FN(table_mode_ok_strip)(mode)) return ALWAN_E_INVALID;
    ALWAN_TABLE_STRICT_CHECK_3(coord);
    ALWAN_CORE_VEC3 in;
    in.v[0] = coord->r; in.v[1] = coord->g; in.v[2] = coord->b;
    ALWAN_CORE_VEC3 const out = ALWAN_CORE_FNVLIT(alwan_table2d_sample)(strip, size, in, mode);
    result->r = out.v[0];
    result->g = out.v[1];
    result->b = out.v[2];
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_table3d_sample)(ALWAN_CORE_RGB *result,
                        ALWAN_CORE_T const *cube, int size,
                        ALWAN_CORE_RGB const *coord, alwan_sample_mode mode) {
    if (!result || !cube || !coord || size < 2) return ALWAN_E_INVALID;
    if (!ALWAN_CORE_FN(table_mode_ok_rank3)(mode)) return ALWAN_E_INVALID;
    ALWAN_TABLE_STRICT_CHECK_3(coord);
    ALWAN_CORE_VEC3 in;
    in.v[0] = coord->r; in.v[1] = coord->g; in.v[2] = coord->b;
    ALWAN_CORE_VEC3 const out = ALWAN_CORE_FNVLIT(alwan_table3d_sample)(cube, size, in, mode);
    result->r = out.v[0];
    result->g = out.v[1];
    result->b = out.v[2];
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_table1d_mat3_sample)(ALWAN_CORE_MAT3X3 *result,
                        ALWAN_CORE_MAT3X3 const *table, int size,
                        ALWAN_CORE_T coord, alwan_sample_mode mode) {
    if (!result || !table || size < 2) return ALWAN_E_INVALID;
    if (!ALWAN_CORE_FN(table_mode_ok_rank1)(mode)) return ALWAN_E_INVALID;
    ALWAN_TABLE_STRICT_CHECK_1(coord);
    *result = ALWAN_CORE_FNVLIT(alwan_table1d_mat3_sample)(table, size, coord, mode);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Named readers for embedded tables
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_machado_matrix_sample)(ALWAN_CORE_MAT3X3 *result,
                        alwan_cvd_type cvd_type,
                        ALWAN_CORE_T severity, alwan_sample_mode mode) {
    if (!result) return ALWAN_E_INVALID;
    if (!ALWAN_CORE_FN(table_mode_ok_rank1)(mode)) return ALWAN_E_INVALID;

    /* The Machado ramps stay in core/alwan_vision_core.h -- see the STAYS PUT
     * section of data/alwan_data_tables.h for why they cannot be externed. */
    /* The anomaly variants share the dichromat ramp; severity is what
     * separates them, exactly as alwan_simulate_cvd_machado dispatches. */
    ALWAN_CORE_MAT3X3 const *table;
    switch (cvd_type) {
        case ALWAN_CVD_PROTANOPIA:
        case ALWAN_CVD_PROTANOMALY:   table = ALWAN_CORE_FNLIT(MACHADO_PROTAN); break;
        case ALWAN_CVD_DEUTERANOPIA:
        case ALWAN_CVD_DEUTERANOMALY: table = ALWAN_CORE_FNLIT(MACHADO_DEUTAN); break;
        case ALWAN_CVD_TRITANOPIA:
        case ALWAN_CVD_TRITANOMALY:   table = ALWAN_CORE_FNLIT(MACHADO_TRITAN); break;
        default: return ALWAN_E_INVALID;
    }
    ALWAN_TABLE_STRICT_CHECK_1(severity);
    *result = ALWAN_CORE_FNVLIT(alwan_table1d_mat3_sample)(
        table, ALWAN_MACHADO_SEVERITY_STEPS, severity, mode);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_jakob2019_coeff_sample)(ALWAN_CORE_T *result_c012,
                        alwan_jakob2019_gamut gamut,
                        ALWAN_CORE_RGB const *coord, alwan_sample_mode mode) {
    if (!result_c012 || !coord) return ALWAN_E_INVALID;
    if (!ALWAN_CORE_FN(table_mode_ok_strip)(mode)) return ALWAN_E_INVALID;

    ALWAN_CORE_T const *c0;
    ALWAN_CORE_T const *c1;
    ALWAN_CORE_T const *c2;
    switch (gamut) {
        case ALWAN_JAKOB2019_SRGB:
            c0 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_srgb_c0);
            c1 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_srgb_c1);
            c2 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_srgb_c2); break;
        case ALWAN_JAKOB2019_PROPHOTO_RGB:
            c0 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_prophoto_c0);
            c1 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_prophoto_c1);
            c2 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_prophoto_c2); break;
        case ALWAN_JAKOB2019_ACES2065_1:
            c0 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_aces_c0);
            c1 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_aces_c1);
            c2 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_aces_c2); break;
        case ALWAN_JAKOB2019_REC2020:
            c0 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_rec2020_c0);
            c1 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_rec2020_c1);
            c2 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_rec2020_c2); break;
        case ALWAN_JAKOB2019_ERGB:
            c0 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_ergb_c0);
            c1 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_ergb_c1);
            c2 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_ergb_c2); break;
        case ALWAN_JAKOB2019_XYZ:
            c0 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_xyz_c0);
            c1 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_xyz_c1);
            c2 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_xyz_c2); break;
        default: return ALWAN_E_INVALID;
    }
    ALWAN_TABLE_STRICT_CHECK_3(coord);
    ALWAN_CORE_VEC3 in;
    in.v[0] = coord->r; in.v[1] = coord->g; in.v[2] = coord->b;
    ALWAN_CORE_VEC3 const out = ALWAN_CORE_FNVLIT(alwan_table3d_planar3_sample)(
        c0, c1, c2, ALWAN_TABLE_JAKOB2019_RES, in, mode);
    result_c012[0] = out.v[0];
    result_c012[1] = out.v[1];
    result_c012[2] = out.v[2];
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_agx_contrast_sample)(ALWAN_CORE_T *result, int sb2383,
                        ALWAN_CORE_T coord, alwan_sample_mode mode) {
    if (!result) return ALWAN_E_INVALID;
    if (!ALWAN_CORE_FN(table_mode_ok_rank1)(mode)) return ALWAN_E_INVALID;
    ALWAN_TABLE_STRICT_CHECK_1(coord);

    ALWAN_CORE_T const *table = sb2383
        ? ALWAN_CORE_FNLIT(alwan_table_agx_sb2383_contrast)
        : ALWAN_CORE_FNLIT(alwan_table_agx_default_contrast);

    /* Delta blend, matching agx_lut_eval exactly, so this reader and the view
     * transform cannot disagree in the last bit. */
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_NEAREST) {
        *result = ALWAN_CORE_FNVLIT(alwan_table1d_sample_nearest)(
            table, ALWAN_TABLE_AGX_CONTRAST_SIZE, coord);
    } else {
        *result = ALWAN_CORE_FNVLIT(alwan_table1d_sample_linear_delta)(
            table, ALWAN_TABLE_AGX_CONTRAST_SIZE, coord);
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_agx_blender_cube_sample)(ALWAN_CORE_RGB *result,
                        ALWAN_CORE_RGB const *coord, alwan_sample_mode mode) {
    if (!result || !coord) return ALWAN_E_INVALID;
    if (!ALWAN_CORE_FN(table_mode_ok_rank3)(mode)) return ALWAN_E_INVALID;
    ALWAN_TABLE_STRICT_CHECK_3(coord);
    ALWAN_CORE_VEC3 in;
    in.v[0] = coord->r; in.v[1] = coord->g; in.v[2] = coord->b;
    ALWAN_CORE_VEC3 const out = ALWAN_CORE_FNVLIT(alwan_table3d_sample)(
        ALWAN_CORE_FNLIT(alwan_table_agx_blender_cube),
        ALWAN_TABLE_AGX_BLENDER_CUBE_RES, in, mode);
    result->r = out.v[0];
    result->g = out.v[1];
    result->b = out.v[2];
    return ALWAN_OK;
}

#undef ALWAN_TABLE_STRICT_CHECK_1
#undef ALWAN_TABLE_STRICT_CHECK_3
