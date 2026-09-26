/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Jakob and Hanika (2019): the coefficient lookup, as rgb2spec_fetch does it.
 *
 * No include guard: included once per precision pass by alwan_table_impl.inc and
 * alwan_spectrum_upsample_impl.inc, which live in different translation units.
 *
 * The tables (data/alwan_data_tables.h) are rgb2spec_opt's own, float32 in every
 * build. The channel l holding the largest component z picks one of three
 * blocks; the other two components as fractions of z are regular axes over
 * [0, 1], and z runs on the non-uniform lightness axis in _scale, dense near
 * black. The x and y cells go through the gate in core/alwan_table_core; the
 * lightness cell is found by comparisons on the axis, as rgb2spec does, never by
 * a cast. Values are widened to the pass's precision as they are read, and the
 * blend runs in that precision.
 *
 * Two places where rgb2spec's C has no answer, and what alwan does:
 *   - black (z = 0): rgb2spec divides by z. alwan takes both fractions as 0,
 *     which reads the k = 0 entry, the one rgb2spec_opt fitted for black. It is
 *     what colour's LUT3D_Jakob2019 returns there.
 *   - ties for the largest component go to the later channel, as in rgb2spec's
 *     `if (rgb[j] >= rgb[i]) i = j` (colour's argmax picks the first).
 * A NaN component reads as 0, the low edge every table reader in alwan sends it
 * to, so NaN never reaches an index or the output.
 */

/* rgb2spec_find_interval: the last k in [0, RES - 2] with scale[k] <= z. */
static int ALWAN_CORE_FN(alwan__jakob2019_interval)(alwan_f32 const *scale, ALWAN_CORE_T z) {
    int left = 0, size = ALWAN_TABLE_JAKOB2019_RES - 2;
    while (size > 0) {
        int const half = size >> 1, middle = left + half + 1;
        if ((ALWAN_CORE_T)scale[middle] <= z) {
            left = middle;
            size -= half + 1;
        } else {
            size = half;
        }
    }
    return left < ALWAN_TABLE_JAKOB2019_RES - 2 ? left : ALWAN_TABLE_JAKOB2019_RES - 2;
}

/* c0, c1, c2 for an RGB in the table's gamut, clamped to [0, 1]. nearest picks
 * the closest node on each axis instead of blending the eight around it. */
static void ALWAN_CORE_FN(alwan__jakob2019_fetch)(ALWAN_CORE_T *out,
        alwan_f32 const *coeff, alwan_f32 const *scale,
        ALWAN_CORE_T r, ALWAN_CORE_T g, ALWAN_CORE_T b, int nearest) {
    enum { RES = ALWAN_TABLE_JAKOB2019_RES };
    ALWAN_CORE_T rgb[3];
    int l = 0, j;
    size_t c;
    /* !(v > 0) catches NaN as well as v <= 0 */
    rgb[0] = !(r > ALWAN_CORE_ZERO) ? ALWAN_CORE_ZERO : (r > ALWAN_CORE_ONE ? ALWAN_CORE_ONE : r);
    rgb[1] = !(g > ALWAN_CORE_ZERO) ? ALWAN_CORE_ZERO : (g > ALWAN_CORE_ONE ? ALWAN_CORE_ONE : g);
    rgb[2] = !(b > ALWAN_CORE_ZERO) ? ALWAN_CORE_ZERO : (b > ALWAN_CORE_ONE ? ALWAN_CORE_ONE : b);
    for (j = 1; j < 3; j++) {
        if (rgb[j] >= rgb[l]) l = j;
    }

    ALWAN_CORE_T const z = rgb[l];
    ALWAN_CORE_T const inv = z > ALWAN_CORE_ZERO ? ALWAN_CORE_ONE / z : ALWAN_CORE_ZERO;
    ALWAN_CORE_TABLE_CELL const cx = ALWAN_CORE_FNVLIT(alwan_table_cell)(rgb[(l + 1) % 3] * inv, RES);
    ALWAN_CORE_TABLE_CELL const cy = ALWAN_CORE_FNVLIT(alwan_table_cell)(rgb[(l + 2) % 3] * inv, RES);
    int const zi = ALWAN_CORE_FN(alwan__jakob2019_interval)(scale, z);
    ALWAN_CORE_T const s0 = (ALWAN_CORE_T)scale[zi];
    ALWAN_CORE_T const z1 = (z - s0) / ((ALWAN_CORE_T)scale[zi + 1] - s0);

    if (nearest) {
        size_t const k = (size_t)(z1 >= ALWAN_CORE_LITERAL(0.5) ? zi + 1 : zi);
        size_t const yi = (size_t)ALWAN_CORE_FNVLIT(alwan_table_cell_nearest)(cy);
        size_t const xi = (size_t)ALWAN_CORE_FNVLIT(alwan_table_cell_nearest)(cx);
        size_t const o = ((((size_t)l * RES + k) * RES + yi) * RES + xi) * 3;
        for (c = 0; c < 3; c++) out[c] = (ALWAN_CORE_T)coeff[o + c];
        return;
    }

    {
        /* The gate's cell stops at the last node with i1 == i0 there; rgb2spec keeps
         * i0 at RES - 2 and a weight of 1. Both read the last node alone. */
        ALWAN_CORE_T const x1 = cx.frac, x0 = ALWAN_CORE_ONE - x1;
        ALWAN_CORE_T const y1 = cy.frac, y0 = ALWAN_CORE_ONE - y1;
        ALWAN_CORE_T const z0 = ALWAN_CORE_ONE - z1;
        size_t const blk = (size_t)l * RES;
        size_t const o000 = (((blk + (size_t)zi) * RES + (size_t)cy.i0) * RES + (size_t)cx.i0) * 3;
        size_t const o001 = (((blk + (size_t)zi) * RES + (size_t)cy.i0) * RES + (size_t)cx.i1) * 3;
        size_t const o010 = (((blk + (size_t)zi) * RES + (size_t)cy.i1) * RES + (size_t)cx.i0) * 3;
        size_t const o011 = (((blk + (size_t)zi) * RES + (size_t)cy.i1) * RES + (size_t)cx.i1) * 3;
        size_t const dz = RES * RES * 3;
        for (c = 0; c < 3; c++) {
            ALWAN_CORE_T const lo =
                ((ALWAN_CORE_T)coeff[o000 + c] * x0 + (ALWAN_CORE_T)coeff[o001 + c] * x1) * y0 +
                ((ALWAN_CORE_T)coeff[o010 + c] * x0 + (ALWAN_CORE_T)coeff[o011 + c] * x1) * y1;
            ALWAN_CORE_T const hi =
                ((ALWAN_CORE_T)coeff[o000 + dz + c] * x0 + (ALWAN_CORE_T)coeff[o001 + dz + c] * x1) * y0 +
                ((ALWAN_CORE_T)coeff[o010 + dz + c] * x0 + (ALWAN_CORE_T)coeff[o011 + dz + c] * x1) * y1;
            out[c] = lo * z0 + hi * z1;
        }
    }
}

/* One gamut's table and lightness axis. A gamut compiled out
 * (data/alwan_data_tables_config.h) is ALWAN_E_NODATA. */
static alwan_status ALWAN_CORE_FN(alwan__jakob2019_tables)(alwan_jakob2019_gamut gamut,
        alwan_f32 const **coeff, alwan_f32 const **scale) {
    *coeff = NULL;
    *scale = NULL;
    switch (gamut) {
        case ALWAN_JAKOB2019_SRGB:
#if ALWAN_TABLE_JAKOB2019_SRGB
            *coeff = alwan_table_jakob2019_srgb_coeff_f32; *scale = alwan_table_jakob2019_srgb_scale_f32;
#endif
            break;
        case ALWAN_JAKOB2019_PROPHOTO_RGB:
#if ALWAN_TABLE_JAKOB2019_PROPHOTO
            *coeff = alwan_table_jakob2019_prophoto_coeff_f32; *scale = alwan_table_jakob2019_prophoto_scale_f32;
#endif
            break;
        case ALWAN_JAKOB2019_ACES2065_1:
#if ALWAN_TABLE_JAKOB2019_ACES
            *coeff = alwan_table_jakob2019_aces_coeff_f32; *scale = alwan_table_jakob2019_aces_scale_f32;
#endif
            break;
        case ALWAN_JAKOB2019_REC2020:
#if ALWAN_TABLE_JAKOB2019_REC2020
            *coeff = alwan_table_jakob2019_rec2020_coeff_f32; *scale = alwan_table_jakob2019_rec2020_scale_f32;
#endif
            break;
        case ALWAN_JAKOB2019_ERGB:
#if ALWAN_TABLE_JAKOB2019_ERGB
            *coeff = alwan_table_jakob2019_ergb_coeff_f32; *scale = alwan_table_jakob2019_ergb_scale_f32;
#endif
            break;
        case ALWAN_JAKOB2019_XYZ:
#if ALWAN_TABLE_JAKOB2019_XYZ
            *coeff = alwan_table_jakob2019_xyz_coeff_f32; *scale = alwan_table_jakob2019_xyz_scale_f32;
#endif
            break;
        default:
            return ALWAN_E_INVALID;
    }
    return (*coeff && *scale) ? ALWAN_OK : ALWAN_E_NODATA;
}
