/* No include guard -- included twice (f32 and f64 passes).
 *
 * Native dual-precision RGB->spectrum upsampling. The basis spectra and
 * Jakob2019 coefficient LUTs are dual-declared (f32 + f64 twins from the
 * same CSV) in data/alwan_data_tables.h and read here via
 * ALWAN_CORE_FNLIT(NAME), so the f32 pass reads native float data and
 * every operation runs at the selected precision -- a genuine f32 path
 * rather than a widen-to-f64 facade.
 *
 * Every element read goes through a reader in core/alwan_table_core, so this
 * file is governed by ALWAN_READ_DATA_NO_BOUND_CHECK rather than by whatever
 * bound each loop happened to spell. The extents handed to the readers are the
 * ALWAN_TABLE_*_SIZE enum constants from the registry, so they are immediate
 * and the gate folds to nothing; the readers are header-only, which matters
 * here because this build sets no /GL and no /LTCG and upsampling runs per
 * pixel.
 */

static inline ALWAN_CORE_T ALWAN_CORE_FN(spu_clamp)(ALWAN_CORE_T x, ALWAN_CORE_T lo, ALWAN_CORE_T hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/* ----------------------------------------------------------------
 * Smits1999: RGB to Spectrum
 * ---------------------------------------------------------------- */
alwan_status ALWAN_CORE_FNLIT(alwan_rgb_to_spectrum_smits1999)(
        ALWAN_CORE_SPD *out_spd, ALWAN_CORE_RGB const *rgb, alwan_ctx *ctx) {
    if (!rgb || !out_spd) return ALWAN_E_INVALID;
#if !ALWAN_TABLE_SMITS1999
    /* The basis spectra were compiled out: data/alwan_data_tables_config.h. */
    (void)ctx;
    return ALWAN_E_NODATA;
#else

    int status = ALWAN_CORE_FNLIT(alwan_spd_create)(out_spd,
        (ALWAN_CORE_T)SMITS1999_WAVELENGTH_MIN, (ALWAN_CORE_T)SMITS1999_WAVELENGTH_MAX,
        SMITS1999_WAVELENGTH_COUNT, ctx);
    if (status != ALWAN_OK) return status;

    /* One name per basis spectrum, resolved once, exactly as the Jakob2019
     * path below names its three cubes before sampling. The per-sample read is
     * alwan_table1d_row: for a loop counter already proved in range the gate is
     * the identity on the address, so folding the old raw subscript onto it
     * cannot move a value. */
    ALWAN_CORE_T const *const basis_white   = ALWAN_CORE_FNLIT(alwan_table_smits1999_white);
    ALWAN_CORE_T const *const basis_cyan    = ALWAN_CORE_FNLIT(alwan_table_smits1999_cyan);
    ALWAN_CORE_T const *const basis_magenta = ALWAN_CORE_FNLIT(alwan_table_smits1999_magenta);
    ALWAN_CORE_T const *const basis_yellow  = ALWAN_CORE_FNLIT(alwan_table_smits1999_yellow);
    ALWAN_CORE_T const *const basis_red     = ALWAN_CORE_FNLIT(alwan_table_smits1999_red);
    ALWAN_CORE_T const *const basis_green   = ALWAN_CORE_FNLIT(alwan_table_smits1999_green);
    ALWAN_CORE_T const *const basis_blue    = ALWAN_CORE_FNLIT(alwan_table_smits1999_blue);

    ALWAN_CORE_T r = ALWAN_CORE_FN(spu_clamp)(rgb->r, ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T g = ALWAN_CORE_FN(spu_clamp)(rgb->g, ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T b = ALWAN_CORE_FN(spu_clamp)(rgb->b, ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));

    if (r <= g && r <= b) {
        for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
            out_spd->values[i] = ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                    basis_white, ALWAN_TABLE_SMITS1999_SIZE, (int)i) * r;
        if (g <= b) {
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                        basis_cyan, ALWAN_TABLE_SMITS1999_SIZE, (int)i) * (g - r);
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                        basis_blue, ALWAN_TABLE_SMITS1999_SIZE, (int)i) * (b - g);
        } else {
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                        basis_cyan, ALWAN_TABLE_SMITS1999_SIZE, (int)i) * (b - r);
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                        basis_green, ALWAN_TABLE_SMITS1999_SIZE, (int)i) * (g - b);
        }
    } else if (g <= r && g <= b) {
        for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
            out_spd->values[i] = ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                    basis_white, ALWAN_TABLE_SMITS1999_SIZE, (int)i) * g;
        if (r <= b) {
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                        basis_magenta, ALWAN_TABLE_SMITS1999_SIZE, (int)i) * (r - g);
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                        basis_blue, ALWAN_TABLE_SMITS1999_SIZE, (int)i) * (b - r);
        } else {
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                        basis_magenta, ALWAN_TABLE_SMITS1999_SIZE, (int)i) * (b - g);
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                        basis_red, ALWAN_TABLE_SMITS1999_SIZE, (int)i) * (r - b);
        }
    } else {
        for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
            out_spd->values[i] = ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                    basis_white, ALWAN_TABLE_SMITS1999_SIZE, (int)i) * b;
        if (r <= g) {
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                        basis_yellow, ALWAN_TABLE_SMITS1999_SIZE, (int)i) * (r - b);
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                        basis_green, ALWAN_TABLE_SMITS1999_SIZE, (int)i) * (g - r);
        } else {
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                        basis_yellow, ALWAN_TABLE_SMITS1999_SIZE, (int)i) * (g - b);
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                        basis_red, ALWAN_TABLE_SMITS1999_SIZE, (int)i) * (r - g);
        }
    }

    return ALWAN_OK;
#endif
}

/* ----------------------------------------------------------------
 * Mallett2019: RGB to Spectrum (linear combination of basis functions)
 * ---------------------------------------------------------------- */
alwan_status ALWAN_CORE_FNLIT(alwan_rgb_to_spectrum_mallett2019)(
        ALWAN_CORE_SPD *out_spd, ALWAN_CORE_RGB const *rgb, alwan_ctx *ctx) {
    if (!rgb || !out_spd) return ALWAN_E_INVALID;
#if !ALWAN_TABLE_MALLETT2019
    (void)ctx;
    return ALWAN_E_NODATA;
#else

    int status = ALWAN_CORE_FNLIT(alwan_spd_create)(out_spd,
        (ALWAN_CORE_T)MALLETT2019_WAVELENGTH_MIN, (ALWAN_CORE_T)MALLETT2019_WAVELENGTH_MAX,
        MALLETT2019_WAVELENGTH_COUNT, ctx);
    if (status != ALWAN_OK) return status;

    ALWAN_CORE_T r = rgb->r, g = rgb->g, b = rgb->b;

    /* Same name-then-read shape as smits1999 above. */
    ALWAN_CORE_T const *const basis_red   = ALWAN_CORE_FNLIT(alwan_table_mallett2019_red);
    ALWAN_CORE_T const *const basis_green = ALWAN_CORE_FNLIT(alwan_table_mallett2019_green);
    ALWAN_CORE_T const *const basis_blue  = ALWAN_CORE_FNLIT(alwan_table_mallett2019_blue);

    for (size_t i = 0; i < MALLETT2019_WAVELENGTH_COUNT; i++) {
        out_spd->values[i] = r * ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                                     basis_red, ALWAN_TABLE_MALLETT2019_SIZE, (int)i) +
                             g * ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                                     basis_green, ALWAN_TABLE_MALLETT2019_SIZE, (int)i) +
                             b * ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                                     basis_blue, ALWAN_TABLE_MALLETT2019_SIZE, (int)i);
    }
    for (size_t i = 0; i < MALLETT2019_WAVELENGTH_COUNT; i++) {
        if (out_spd->values[i] < ALWAN_CORE_LITERAL(0.0))
            out_spd->values[i] = ALWAN_CORE_LITERAL(0.0);
    }

    return ALWAN_OK;
#endif
}

/* ----------------------------------------------------------------
 * Otsu2018: XYZ to reflectance by clustered basis functions
 * ---------------------------------------------------------------- */
/* The selector is a seven node binary tree over CIE xy, walked exactly as
 * colour's Dataset_Otsu2018.select walks it: start at node 0, compare
 * xy[direction] against origin, take the lesser branch when less than or equal
 * and the greater one otherwise. A NEGATIVE branch means "continue at internal
 * node -branch"; a non-negative one is the cluster and the walk ends.
 *
 * Two things keep a malformed table from being dangerous rather than merely
 * wrong. The loop is bounded by the node count instead of trusting the data to
 * terminate, and every read goes through alwan_table1d_row, so the address is
 * clamped whatever the branch value says. The decoded indices are small integers
 * stored in a float table, not measurements, so rounding them is exact. */
static int ALWAN_CORE_FN(otsu2018_select)(ALWAN_CORE_T const *selector,
                                          ALWAN_CORE_T x, ALWAN_CORE_T y) {
    int node = 0;
    for (int step = 0; step < ALWAN_TABLE_OTSU2018_NODES; step++) {
        int const base = node * 4;
        ALWAN_CORE_T const origin = ALWAN_CORE_FNVLIT(alwan_table1d_row)(
            selector, ALWAN_TABLE_OTSU2018_SELECTOR_SIZE, base + 0);
        ALWAN_CORE_T const direction = ALWAN_CORE_FNVLIT(alwan_table1d_row)(
            selector, ALWAN_TABLE_OTSU2018_SELECTOR_SIZE, base + 1);
        ALWAN_CORE_T const coord = (direction < ALWAN_CORE_LITERAL(0.5)) ? x : y;
        int const slot = (coord <= origin) ? 2 : 3;
        ALWAN_CORE_T const branch = ALWAN_CORE_FNVLIT(alwan_table1d_row)(
            selector, ALWAN_TABLE_OTSU2018_SELECTOR_SIZE, base + slot);

        if (branch < ALWAN_CORE_LITERAL(0.0)) {
            int const next = (int)(-branch + ALWAN_CORE_LITERAL(0.5));
            node = (next >= 0 && next < ALWAN_TABLE_OTSU2018_NODES) ? next : 0;
        } else {
            int const leaf = (int)(branch + ALWAN_CORE_LITERAL(0.5));
            return (leaf >= 0 && leaf < ALWAN_TABLE_OTSU2018_CLUSTERS) ? leaf : 0;
        }
    }
    return 0;
}

alwan_status ALWAN_CORE_FNLIT(alwan_xyz_to_spectrum_otsu2018)(
        ALWAN_CORE_SPD *out_spd, ALWAN_CORE_XYZ const *xyz, alwan_ctx *ctx) {
    if (!xyz || !out_spd) return ALWAN_E_INVALID;
#if !ALWAN_TABLE_OTSU2018
    (void)ctx;
    return ALWAN_E_NODATA;
#else

    int status = ALWAN_CORE_FNLIT(alwan_spd_create)(out_spd,
        (ALWAN_CORE_T)OTSU2018_WAVELENGTH_MIN, (ALWAN_CORE_T)OTSU2018_WAVELENGTH_MAX,
        OTSU2018_WAVELENGTH_COUNT, ctx);
    if (status != ALWAN_OK) return status;

    /* Same name-then-read shape as the two methods above. */
    ALWAN_CORE_T const *const selector  = ALWAN_CORE_FNLIT(alwan_table_otsu2018_selector);
    ALWAN_CORE_T const *const basis     = ALWAN_CORE_FNLIT(alwan_table_otsu2018_basis);
    ALWAN_CORE_T const *const means     = ALWAN_CORE_FNLIT(alwan_table_otsu2018_means);
    ALWAN_CORE_T const *const m_inverse = ALWAN_CORE_FNLIT(alwan_table_otsu2018_m_inverse);
    ALWAN_CORE_T const *const xyz_mu    = ALWAN_CORE_FNLIT(alwan_table_otsu2018_xyz_mu);

    /* xy for the tree. A zero sum has no chromaticity; it walks from the origin
     * rather than dividing, and lands in whichever cluster owns that corner. */
    ALWAN_CORE_T const sum = xyz->x + xyz->y + xyz->z;
    ALWAN_CORE_T const inv = (sum != ALWAN_CORE_LITERAL(0.0))
                             ? ALWAN_CORE_ONE / sum : ALWAN_CORE_LITERAL(0.0);
    int const k = ALWAN_CORE_FN(otsu2018_select)(selector, xyz->x * inv, xyz->y * inv);

    /* weights = M_inverse * (XYZ - XYZ_mu), both already built for this cluster
     * by the generator, so nothing is integrated here. */
    ALWAN_CORE_T d[3], w[3];
    d[0] = xyz->x - ALWAN_CORE_FNVLIT(alwan_table1d_row)(
        xyz_mu, ALWAN_TABLE_OTSU2018_XYZ_MU_SIZE, k * 3 + 0);
    d[1] = xyz->y - ALWAN_CORE_FNVLIT(alwan_table1d_row)(
        xyz_mu, ALWAN_TABLE_OTSU2018_XYZ_MU_SIZE, k * 3 + 1);
    d[2] = xyz->z - ALWAN_CORE_FNVLIT(alwan_table1d_row)(
        xyz_mu, ALWAN_TABLE_OTSU2018_XYZ_MU_SIZE, k * 3 + 2);

    for (int i = 0; i < 3; i++) {
        int const row = k * 9 + i * 3;
        w[i] = ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                   m_inverse, ALWAN_TABLE_OTSU2018_M_INVERSE_SIZE, row + 0) * d[0]
             + ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                   m_inverse, ALWAN_TABLE_OTSU2018_M_INVERSE_SIZE, row + 1) * d[1]
             + ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                   m_inverse, ALWAN_TABLE_OTSU2018_M_INVERSE_SIZE, row + 2) * d[2];
    }

    for (int j = 0; j < OTSU2018_WAVELENGTH_COUNT; j++) {
        ALWAN_CORE_T v = ALWAN_CORE_FNVLIT(alwan_table1d_row)(
            means, ALWAN_TABLE_OTSU2018_MEANS_SIZE, k * ALWAN_TABLE_OTSU2018_SAMPLES + j);
        for (int i = 0; i < 3; i++) {
            v += w[i] * ALWAN_CORE_FNVLIT(alwan_table1d_row)(
                basis, ALWAN_TABLE_OTSU2018_BASIS_SIZE,
                (k * ALWAN_TABLE_OTSU2018_BASIS_PER_CLUSTER + i)
                    * ALWAN_TABLE_OTSU2018_SAMPLES + j);
        }
        /* The reference clamps to [0, 1]; a reflectance outside it is not one. */
        if (v < ALWAN_CORE_LITERAL(0.0)) v = ALWAN_CORE_LITERAL(0.0);
        if (v > ALWAN_CORE_ONE) v = ALWAN_CORE_ONE;
        out_spd->values[j] = v;
    }

    return ALWAN_OK;
#endif
}

/* ----------------------------------------------------------------
 * Jakob2019: trilinear LUT sample + polynomial evaluation
 * ---------------------------------------------------------------- */
/* The three coefficient cubes are PLANAR and b-fastest, not the interleaved
 * r-fastest layout alwan_table3d_sample_trilinear reads, so they go through
 * the planar reader. Addressing is the shared gate: spu_clamp is
 * NaN-transparent, so `(int)(clamp(NaN)*63)` used to land on INT_MIN and the
 * index arithmetic signed-overflowed before dereferencing. Finite coordinates
 * are bit-identical -- same clamp, same multiply, same corner blend. */
static void ALWAN_CORE_FN(jakob2019_lut_sample)(
        ALWAN_CORE_T const *lut_c0, ALWAN_CORE_T const *lut_c1, ALWAN_CORE_T const *lut_c2,
        ALWAN_CORE_T r, ALWAN_CORE_T g, ALWAN_CORE_T b,
        ALWAN_CORE_T *out_c0, ALWAN_CORE_T *out_c1, ALWAN_CORE_T *out_c2) {
    ALWAN_CORE_VEC3 coord;
    coord.v[0] = r; coord.v[1] = g; coord.v[2] = b;
    ALWAN_CORE_VEC3 const c = ALWAN_CORE_FNVLIT(alwan_table3d_planar3_sample_trilinear)(
        lut_c0, lut_c1, lut_c2, ALWAN_TABLE_JAKOB2019_RES, coord);
    *out_c0 = c.v[0];
    *out_c1 = c.v[1];
    *out_c2 = c.v[2];
}

static inline ALWAN_CORE_T ALWAN_CORE_FN(jakob2019_eval_poly)(
        ALWAN_CORE_T c0, ALWAN_CORE_T c1, ALWAN_CORE_T c2, ALWAN_CORE_T wavelength) {
    ALWAN_CORE_T const U = c0 * wavelength * wavelength + c1 * wavelength + c2;
    ALWAN_CORE_T const U_sq = U * U;
    ALWAN_CORE_T const denom = ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_SQRT(ALWAN_CORE_LITERAL(1.0) + U_sq);
    ALWAN_CORE_T const R = ALWAN_CORE_LITERAL(0.5) + U / denom;
    return ALWAN_CORE_FN(spu_clamp)(R, ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));
}

alwan_status ALWAN_CORE_FNLIT(alwan_rgb_to_spectrum_jakob2019)(
        ALWAN_CORE_SPD *out_spd, alwan_jakob2019_gamut gamut,
        ALWAN_CORE_RGB const *rgb, alwan_ctx *ctx) {
    if (!rgb || !out_spd) return ALWAN_E_INVALID;

    /* A gamut whose cubes were compiled out (data/alwan_data_tables_config.h)
     * leaves the pointers NULL and reports ALWAN_E_NODATA below. */
    ALWAN_CORE_T const *lut_c0 = NULL, *lut_c1 = NULL, *lut_c2 = NULL;
    switch (gamut) {
        case ALWAN_JAKOB2019_SRGB:
#if ALWAN_TABLE_JAKOB2019_SRGB
            lut_c0 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_srgb_c0); lut_c1 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_srgb_c1); lut_c2 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_srgb_c2);
#endif
            break;
        case ALWAN_JAKOB2019_PROPHOTO_RGB:
#if ALWAN_TABLE_JAKOB2019_PROPHOTO
            lut_c0 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_prophoto_c0); lut_c1 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_prophoto_c1); lut_c2 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_prophoto_c2);
#endif
            break;
        case ALWAN_JAKOB2019_ACES2065_1:
#if ALWAN_TABLE_JAKOB2019_ACES
            lut_c0 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_aces_c0); lut_c1 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_aces_c1); lut_c2 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_aces_c2);
#endif
            break;
        case ALWAN_JAKOB2019_REC2020:
#if ALWAN_TABLE_JAKOB2019_REC2020
            lut_c0 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_rec2020_c0); lut_c1 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_rec2020_c1); lut_c2 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_rec2020_c2);
#endif
            break;
        case ALWAN_JAKOB2019_ERGB:
#if ALWAN_TABLE_JAKOB2019_ERGB
            lut_c0 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_ergb_c0); lut_c1 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_ergb_c1); lut_c2 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_ergb_c2);
#endif
            break;
        case ALWAN_JAKOB2019_XYZ:
#if ALWAN_TABLE_JAKOB2019_XYZ
            lut_c0 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_xyz_c0); lut_c1 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_xyz_c1); lut_c2 = ALWAN_CORE_FNLIT(alwan_table_jakob2019_xyz_c2);
#endif
            break;
        default:
            return ALWAN_E_INVALID;
    }
    if (!lut_c0 || !lut_c1 || !lut_c2) return ALWAN_E_NODATA;

    int status = ALWAN_CORE_FNLIT(alwan_spd_create)(out_spd,
        (ALWAN_CORE_T)JAKOB2019_WAVELENGTH_MIN, (ALWAN_CORE_T)JAKOB2019_WAVELENGTH_MAX,
        JAKOB2019_WAVELENGTH_COUNT, ctx);
    if (status != ALWAN_OK) return status;

    ALWAN_CORE_T c0, c1, c2;
    ALWAN_CORE_FN(jakob2019_lut_sample)(lut_c0, lut_c1, lut_c2, rgb->r, rgb->g, rgb->b, &c0, &c1, &c2);

    ALWAN_CORE_T const wl_min = (ALWAN_CORE_T)JAKOB2019_WAVELENGTH_MIN;
    ALWAN_CORE_T const wl_step = ((ALWAN_CORE_T)JAKOB2019_WAVELENGTH_MAX - (ALWAN_CORE_T)JAKOB2019_WAVELENGTH_MIN)
                               / (ALWAN_CORE_T)(JAKOB2019_WAVELENGTH_COUNT - 1);

    for (size_t i = 0; i < JAKOB2019_WAVELENGTH_COUNT; i++) {
        ALWAN_CORE_T const wavelength = wl_min + (ALWAN_CORE_T)i * wl_step;
        out_spd->values[i] = ALWAN_CORE_FN(jakob2019_eval_poly)(c0, c1, c2, wavelength);
    }

    return ALWAN_OK;
}
