/* ================================================================
 * Alwan - Light Quality & CCT
 * CCT/whiteness formulas in alwan_quality_core.h
 *
 * Only table lookups (Robertson), Newton-Raphson loops (Kang inverse),
 * CRI/CQS/SSI/TM-30 workflows, and data tables live here.
 * ================================================================ */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_quality_core.h"
#include "../core/alwan_table_core.h"
#include "../data/alwan_data_tables.h"
#include <math.h>

/* ----------------------------------------------------------------
 * CCT: Correlated Color Temperature
 * ---------------------------------------------------------------- */

/* McCamy's approximation for CCT from CIE 1931 xy coordinates
 * Fast approximation, ~2% accuracy above 2800K
 * Formula: CCT = 449n^3 + 3525n^2 + 6823.3n + 5520.33
 * where n = (x - 0.3320) / (0.1858 - y)
 */
alwan_f64 alwan_cct_mccamy_xy_f64(alwan_vec2_f64 const *xy) {
    if (!xy) {
        return ALWAN_LITERAL(-1.0);
    }

    alwan_f64 denom = ALWAN_LITERAL(0.1858) - xy->v[1];
    if (ALWAN_ABS(denom) < ALWAN_EPSILON) {
        return ALWAN_LITERAL(-1.0);
    }

    return alwan_cct_mccamy_f64_v(xy->v[0], xy->v[1]);
}

/* Robertson 1968: CCT from CIE 1931 xy coordinates
 * Reference: Robertson, A. R. (1968). Computation of Correlated Color
 *            Temperature and Distribution Temperature. J. Opt. Soc. Am.
 *
 * Uses the standard 31-entry isotemperature line table.
 * Table format: reciprocal_mrd, u, v, slope  (4 values per entry)
 * CCT = 1e6 / reciprocal_mrd
 */

/* The Robertson 1968 isotemperature locus is homed in the registry as
 * alwan_table_robertson_locus_{f32,f64}: ALWAN_TABLE_ROBERTSON_ROWS rows of
 * ALWAN_TABLE_ROBERTSON_STRIDE values (reciprocal_mrd, u, v, slope), declared
 * with its extents in data/alwan_data_tables.h. ROBERTSON_AT reads it through
 * alwan_table2d_row_at_f64_v, which gates the row AND the column, so
 * ALWAN_READ_DATA_NO_BOUND_CHECK governs this table like every other one.
 *
 * Both precisions read the f64 table. alwan_cct_robertson_xy_f32 has always
 * narrowed each value from f64, and repointing it at the f32 twin would change
 * its output: decimal->f32 is not always equal to decimal->f64->f32. */
#define ROBERTSON_AT(row, col)                                  \
    alwan_table2d_row_at_f64_v(alwan_table_robertson_locus_f64, \
                               ALWAN_TABLE_ROBERTSON_ROWS,      \
                               ALWAN_TABLE_ROBERTSON_STRIDE,    \
                               (row), (col))

alwan_f64 alwan_cct_robertson_xy_f64(alwan_vec2_f64 const *xy) {
    if (!xy) {
        return ALWAN_LITERAL(-1.0);
    }
#if !ALWAN_TABLE_ROBERTSON_LOCUS
    /* The isotemperature table was compiled out (data/alwan_data_tables_config.h).
     * -1 is this function's documented error value. */
    return ALWAN_LITERAL(-1.0);
#else

    alwan_f64 x = xy->v[0];
    alwan_f64 y = xy->v[1];

    /* Convert CIE 1931 xy to CIE 1960 UCS uv */
    alwan_f64 denom = ALWAN_LITERAL(12.0) * y - ALWAN_LITERAL(2.0) * x + ALWAN_LITERAL(3.0);
    if (ALWAN_ABS(denom) < ALWAN_EPSILON) {
        return ALWAN_LITERAL(-1.0);
    }

    alwan_f64 u = (ALWAN_LITERAL(4.0) * x) / denom;
    alwan_f64 v = (ALWAN_LITERAL(6.0) * y) / denom;

    /* Robertson 1968: iterate isotemperature lines with normalized direction
     * vectors.  For each line, compute the cross-product signed distance (dt)
     * from the test point to the iso-line.  When dt transitions from positive
     * to non-positive, interpolate between the previous and current lines
     * in reciprocal MRD space.
     *
     * Matches colour-science's _uv_to_CCT_Robertson1968 implementation. */
    alwan_f64 last_dt = ALWAN_ZERO;

    for (int i = 1; i < ALWAN_TABLE_ROBERTSON_ROWS; i++) {
        alwan_f64 u_i = ROBERTSON_AT(i, 1);
        alwan_f64 v_i = ROBERTSON_AT(i, 2);
        alwan_f64 t_i = ROBERTSON_AT(i, 3);

        /* Normalized direction vector along the isotemperature line */
        alwan_f64 du = ALWAN_ONE;
        alwan_f64 dv = t_i;
        alwan_f64 length = ALWAN_SQRT(ALWAN_ONE + dv * dv);
        du /= length;
        dv /= length;

        /* Signed distance (cross product) from test point to iso-line */
        alwan_f64 uu = u - u_i;
        alwan_f64 vv = v - v_i;
        alwan_f64 dt = -uu * dv + vv * du;

        if (dt <= ALWAN_ZERO || i == ALWAN_TABLE_ROBERTSON_ROWS - 1) {
            if (dt > ALWAN_ZERO) dt = ALWAN_ZERO;

            dt = -dt;

            alwan_f64 f = (i == 1)
                ? ALWAN_ZERO
                : dt / (last_dt + dt);

            alwan_f64 r_prev = ROBERTSON_AT(i - 1, 0);
            alwan_f64 r_curr = ROBERTSON_AT(i, 0);

            alwan_f64 r = r_prev * f + r_curr * (ALWAN_ONE - f);

            if (r < ALWAN_EPSILON) {
                return ALWAN_LITERAL(-1.0);
            }

            return ALWAN_LITERAL(1e6) / r;
        }

        last_dt = dt;
    }

    return ALWAN_LITERAL(-1.0);
#endif
}

/* Hernandez-Andres 1999: xy to CCT
 * Reference: Hernandez-Andres et al. (1999)
 * Valid for 3000K - 50000K (extended formula for higher CCT) */
alwan_f64 alwan_cct_hernandez_xy_f64(alwan_vec2_f64 const *xy) {
    if (!xy) return ALWAN_LITERAL(-1.0);

    alwan_f64 denom = xy->v[1] - ALWAN_LITERAL(0.1735);
    if (ALWAN_ABS(denom) < ALWAN_LITERAL(1e-10)) {
        return ALWAN_LITERAL(-1.0);
    }

    return alwan_cct_hernandez_f64_v(xy->v[0], xy->v[1]);
}

/* Kang 2002: CCT to xy (forward transform)
 * Reference: Kang et al. (2002)
 * Valid range: 1667K - 25000K */
void alwan_cct_to_xy_kang_f64(alwan_vec2_f64 *xy_out, alwan_f64 cct) {
    if (!xy_out) return;

    *xy_out = alwan_cct_to_xy_kang_f64_v(cct);
}

/* Kang 2002: xy to CCT (inverse, uses Newton-Raphson with analytical derivatives)
 * Reference: Kang et al. (2002)
 * Valid range: 1667K - 25000K
 *
 * Uses 1D Newton-Raphson on x(T): since x(T) is monotonic in the valid
 * range, solving x(T) = x_target uniquely determines T.  Analytical
 * derivatives of the Kang polynomial eliminate numerical-derivative error. */
alwan_f64 alwan_cct_kang_xy_f64(alwan_vec2_f64 const *xy) {
    if (!xy) return ALWAN_LITERAL(-1.0);

    alwan_f64 target_x = xy->v[0];

    /* Initial estimate using McCamy's formula */
    alwan_f64 cct = alwan_cct_mccamy_xy_f64(xy);
    if (cct < ALWAN_LITERAL(1667.0)) cct = ALWAN_LITERAL(1667.0);
    if (cct > ALWAN_LITERAL(25000.0)) cct = ALWAN_LITERAL(25000.0);

    /* Newton-Raphson on f(T) = x(T) - x_target = 0.
     * x(T) is evaluated by calling alwan_cct_to_xy_kang_f64_v() -- the SAME
     * function used in the forward transform -- so that at T=T_exact the
     * residual is exactly zero in floating point (no forward/inverse
     * code-path mismatch). */
    for (int iter = 0; iter < 50; iter++) {
        alwan_f64 T  = cct;
        alwan_f64 T2 = T * T;
        alwan_f64 T3 = T2 * T;
        alwan_f64 T4 = T3 * T;

        /* Evaluate x(T) via the forward transform (bit-exact match) */
        alwan_vec2_f64 xy_eval = alwan_cct_to_xy_kang_f64_v(T);
        alwan_f64 x_val = xy_eval.v[0];

        /* Analytical derivative of Kang x(T) polynomial */
        alwan_f64 dx_dT;
        if (T <= ALWAN_LITERAL(4000.0)) {
            dx_dT = ALWAN_LITERAL(3.0) * ALWAN_LITERAL(0.2661239e9) / T4
                  + ALWAN_LITERAL(2.0) * ALWAN_LITERAL(0.2343589e6) / T3
                  - ALWAN_LITERAL(0.8776956e3) / T2;
        } else {
            dx_dT = ALWAN_LITERAL(3.0) * ALWAN_LITERAL(3.0258469e9) / T4
                  - ALWAN_LITERAL(2.0) * ALWAN_LITERAL(2.1070379e6) / T3
                  - ALWAN_LITERAL(0.2226347e3) / T2;
        }

        alwan_f64 err = x_val - target_x;

        if (ALWAN_ABS(dx_dT) < ALWAN_LITERAL(1e-30)) break;

        alwan_f64 delta = -err / dx_dT;

        /* Damping for stability */
        if (ALWAN_ABS(delta) > ALWAN_LITERAL(500.0)) {
            delta = (delta > ALWAN_LITERAL(0.0))
                  ? ALWAN_LITERAL(500.0) : ALWAN_LITERAL(-500.0);
        }

        cct += delta;

        /* Clamp to valid range */
        if (cct < ALWAN_LITERAL(1667.0)) cct = ALWAN_LITERAL(1667.0);
        if (cct > ALWAN_LITERAL(25000.0)) cct = ALWAN_LITERAL(25000.0);

        /* Converged when step is negligible */
        if (ALWAN_ABS(delta) < ALWAN_LITERAL(1e-13)) break;
    }

    /* The Kang polynomial is piecewise (boundaries at T=2222 and T=4000).
     * At these boundaries x(T) has a small discontinuity, so Newton-Raphson
     * may fail to converge exactly.  If the forward-transform residual is
     * still large, check whether a boundary value is a better match. */
    {
        alwan_vec2_f64 xy_check = alwan_cct_to_xy_kang_f64_v(cct);
        alwan_f64 best_err = ALWAN_ABS(xy_check.v[0] - target_x);

        static alwan_f64 const boundaries[] = {
            ALWAN_LITERAL(2222.0), ALWAN_LITERAL(4000.0)
        };
        for (int b = 0; b < 2; b++) {
            alwan_vec2_f64 xy_b = alwan_cct_to_xy_kang_f64_v(boundaries[b]);
            alwan_f64 b_err = ALWAN_ABS(xy_b.v[0] - target_x);
            if (b_err < best_err) {
                best_err = b_err;
                cct = boundaries[b];
            }
        }
    }

    /* Canonical-value selection: the Kang polynomial's conditioning means
     * that near certain CCT values (e.g. 6000 K), a band of ~+-10 ULPs in T
     * maps to (nearly) the same (x, y) in double precision.  Newton can
     * land anywhere in this band.  Among equivalent T values, prefer the
     * "simplest" double by checking round(cct) and cct rounded to 0.1. */
    {
        alwan_vec2_f64 xy_cur = alwan_cct_to_xy_kang_f64_v(cct);
        alwan_f64 cur_err = ALWAN_ABS(xy_cur.v[0] - target_x);
        alwan_f64 rounds[] = {
            ALWAN_FLOOR_F64(cct + ALWAN_LITERAL(0.5)),                      /* nearest int */
            ALWAN_FLOOR_F64(cct * ALWAN_LITERAL(10.0) + ALWAN_LITERAL(0.5))
                / ALWAN_LITERAL(10.0)                              /* nearest 0.1 */
        };
        for (int r = 0; r < 2; r++) {
            alwan_f64 c = rounds[r];
            if (c < ALWAN_LITERAL(1667.0) || c > ALWAN_LITERAL(25000.0)) continue;
            if (ALWAN_ABS(c - cct) > ALWAN_LITERAL(1e-8)) continue;
            alwan_vec2_f64 xy_c = alwan_cct_to_xy_kang_f64_v(c);
            alwan_f64 c_err = ALWAN_ABS(xy_c.v[0] - target_x);
            if (c_err <= cur_err) {
                cct = c;
                cur_err = c_err;
                break;
            }
        }
    }

    return cct;
}


/* ----------------------------------------------------------------
 * CRI: Color Rendering Index (CIE 13.3-1995)
 * ---------------------------------------------------------------- */

/* TCS (Test Color Samples), CIE 13.3-1995 TCS01..TCS14.
 *
 * Homed in the registry as alwan_table_tcs_reflectance_{f32,f64}: one
 * row-major table of ALWAN_TABLE_TCS_SAMPLES rows x
 * ALWAN_TABLE_QUALITY_SPECTRUM wavelengths (360-830nm at 5nm), declared with
 * its extents in data/alwan_data_tables.h and read through
 * alwan_table2d_row_at_f64_v. That reader gates the row AND the column, which
 * the 14 arrays behind an array of pointers could not: there, a sample index
 * past the end of the set was invisible to the compiler. */
#define TCS_WAVELENGTH_MIN ALWAN_LITERAL(360.0)
#define TCS_WAVELENGTH_MAX ALWAN_LITERAL(830.0)

/* ----------------------------------------------------------------
 * VS (Vivid Saturated) Color Samples for CQS
 * ---------------------------------------------------------------- */

/* VS (saturated Munsell) samples for CQS, NIST CQS 9.0.
 *
 * Homed in the registry as alwan_table_vs_reflectance_{f32,f64}:
 * ALWAN_TABLE_VS_SAMPLES rows x ALWAN_TABLE_QUALITY_SPECTRUM wavelengths,
 * read through alwan_table2d_row_at_f64_v. */
#define VS_WAVELENGTH_MIN ALWAN_LITERAL(360.0)
#define VS_WAVELENGTH_MAX ALWAN_LITERAL(830.0)

/* ----------------------------------------------------------------
 * CES (Color Evaluation Samples) for TM-30 and CIE 224:2017
 * ---------------------------------------------------------------- */

/* CES (Colour Evaluation Samples) for TM-30 and CIE 224:2017.
 *
 * Homed in the registry as alwan_table_ces_reflectance_{f32,f64}:
 * ALWAN_TABLE_CES_SAMPLES rows x ALWAN_TABLE_QUALITY_SPECTRUM wavelengths,
 * read through alwan_table2d_row_at_f64_v.
 *
 * ALWAN_TABLE_CES_SAMPLES is the one number that changes if the set is
 * regenerated at 99 samples: the loop bound below reads it rather than a
 * literal. The 99.0 divisor in alwan_tm30_rf_f64 is a SEPARATE, separately
 * baselined decision and must not be changed with it. */
#define CES_WAVELENGTH_MIN ALWAN_LITERAL(360.0)
#define CES_WAVELENGTH_MIN_INT 360   /* same value, integer form for index arithmetic */
#define CES_WAVELENGTH_MAX ALWAN_LITERAL(830.0)

/* ----------------------------------------------------------------
 * CRI (Color Rendering Index)
 * ---------------------------------------------------------------- */


/* CIE 1960 UCS chromaticity from XYZ. */
static void cri_xyz_to_uv(alwan_f64 *u, alwan_f64 *v, alwan_xyz_f64 const *xyz) {
    alwan_f64 const denom = xyz->x + ALWAN_LITERAL(15.0) * xyz->y + ALWAN_LITERAL(3.0) * xyz->z;
    if (ALWAN_ABS(denom) < ALWAN_EPSILON) {
        *u = ALWAN_LITERAL(0.0);
        *v = ALWAN_LITERAL(0.0);
        return;
    }
    *u = ALWAN_LITERAL(4.0) * xyz->x / denom;
    *v = ALWAN_LITERAL(6.0) * xyz->y / denom;
}

/* The c and d terms of the CIE 13.3-1995 von Kries adaptation. */
static alwan_f64 cri_term_c(alwan_f64 u, alwan_f64 v) {
    if (ALWAN_ABS(v) < ALWAN_EPSILON) return ALWAN_LITERAL(0.0);
    return (ALWAN_LITERAL(4.0) - u - ALWAN_LITERAL(10.0) * v) / v;
}

static alwan_f64 cri_term_d(alwan_f64 u, alwan_f64 v) {
    if (ALWAN_ABS(v) < ALWAN_EPSILON) return ALWAN_LITERAL(0.0);
    return (ALWAN_LITERAL(1.708) * v + ALWAN_LITERAL(0.404) - ALWAN_LITERAL(1.481) * u) / v;
}

/* U*V*W* (CIE 1964) for a sample at chromaticity (u, v) and luminance factor Y,
 * relative to a white at (u_w, v_w). */
static void cri_uvw(alwan_f64 *U, alwan_f64 *V, alwan_f64 *W,
                    alwan_f64 u, alwan_f64 v, alwan_f64 Y,
                    alwan_f64 u_w, alwan_f64 v_w) {
    alwan_f64 const y = (Y > ALWAN_LITERAL(0.0)) ? Y : ALWAN_LITERAL(0.0);
    *W = ALWAN_LITERAL(25.0) * ALWAN_POW(y, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0)) - ALWAN_LITERAL(17.0);
    *U = ALWAN_LITERAL(13.0) * (*W) * (u - u_w);
    *V = ALWAN_LITERAL(13.0) * (*W) * (v - v_w);
}

/* Defined further down, next to the TM-30 implementation. Used by all three
 * light-quality metrics to build their reference illuminant. */
static alwan_status quality_daylight_spd(alwan_spd_f64 *out, alwan_f64 cct, alwan_ctx *ctx);

/* CRI Ra (General Color Rendering Index) calculation
 * Based on CIE 13.3-1995 specification
 *
 * Algorithm:
 * 1. Compute CCT of test illuminant
 * 2. Generate reference illuminant at same CCT (blackbody <5000K, D-illuminant >=5000K)
 * 3. For each of first 8 TCS samples:
 *    a. Compute XYZ under test illuminant
 *    b. Compute XYZ under reference illuminant
 *    c. Convert to U*V*W* color space (CIE 1964)
 *    d. Calculate color difference dE in U*V*W*
 *    e. Calculate special CRI: R_i = 100 - 4.6 * dE_i
 * 4. Ra = average of R1...R8
 *
 * Returns: Ra value, or negative on error (not clamped; see alwan.h)
 *
 * Three independent defects were fixed here on 2026-08-27, taking the mean
 * absolute deviation against colour-science from ~30 Ra to 0.054:
 *   - an unpublished /15.5 divisor on the colour difference. HP1 scored 94.1
 *     where the CIE value is 8.5.
 *   - a Planckian reference at every CCT, where CIE 13.3 specifies daylight
 *     at or above 5000 K. D65 scored 97.2 against its own reference.
 *   - no chromatic adaptation. Each sample was converted to U*V*W* against
 *     its own white and the two differenced, instead of adapting the test
 *     sample onto the reference illuminant first.
 */
static alwan_status quality_cri(alwan_f64 *ra_out, alwan_spd_f64 const *test_spd,
                                alwan_ctx *ctx, alwan_cri_f64 *spec_out) {
    if (!ctx || !test_spd) {
        return ALWAN_E_INVALID;
    }
#if !ALWAN_TABLE_TCS_REFLECTANCE
    /* The TCS reflectances were compiled out (data/alwan_data_tables_config.h).
     * alwan_cri_ra turns this into its documented -1. */
    return ALWAN_E_NODATA;
#else

    /* Step 1: Resample test SPD to TCS wavelength range (360-830nm @ 5nm) */
    alwan_spd_f64 test_spd_resampled;
    int status = alwan_spd_resample_f64(&test_spd_resampled, test_spd, TCS_WAVELENGTH_MIN, TCS_WAVELENGTH_MAX, ALWAN_TABLE_QUALITY_SPECTRUM, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO, ctx);
    if (status != ALWAN_OK) {
        return ALWAN_E_NODATA;
    }

    /* Step 2: Calculate XYZ and CCT of test illuminant */
    /* Use perfect white reflector to get illuminant's white point */
    alwan_spd_f64 perfect_white;
    status = alwan_spd_create_f64(&perfect_white, test_spd_resampled.wavelength_min, test_spd_resampled.wavelength_max, test_spd_resampled.count, ctx);
    if (status != ALWAN_OK) {
        alwan_spd_destroy_f64(&test_spd_resampled, ctx);
        return ALWAN_E_NODATA;
    }

    for (size_t i = 0; i < perfect_white.count; i++) {
        perfect_white.values[i] = ALWAN_LITERAL(1.0);
    }

    alwan_xyz_f64 xyz_test_white;
    status = alwan_xyz_from_spd_f64(&xyz_test_white, &perfect_white, &test_spd_resampled, ALWAN_OBSERVER_CIE_1931_2DEG, ALWAN_INTEGRATE_TRAPEZOID, ALWAN_LITERAL(0.0), ctx);
    alwan_spd_destroy_f64(&perfect_white, ctx);

    if (status != ALWAN_OK) {
        alwan_spd_destroy_f64(&test_spd_resampled, ctx);
        return ALWAN_E_NODATA;
    }

    /* Calculate normalization factor to scale XYZ to Y=100 for white point */
    alwan_f64 test_norm_factor = ALWAN_LITERAL(1.0);
    if (xyz_test_white.y > ALWAN_EPSILON) {
        test_norm_factor = ALWAN_LITERAL(100.0) / xyz_test_white.y;
        xyz_test_white.x *= test_norm_factor;
        xyz_test_white.y *= test_norm_factor;
        xyz_test_white.z *= test_norm_factor;
    }

    /* Convert XYZ to xy chromaticity */
    alwan_f64 sum = xyz_test_white.x + xyz_test_white.y + xyz_test_white.z;
    if (sum < ALWAN_EPSILON) {
        alwan_spd_destroy_f64(&test_spd_resampled, ctx);
        return ALWAN_E_DIVZERO;
    }

    alwan_vec2_f64 xy_test;
    xy_test.v[0] = xyz_test_white.x / sum;
    xy_test.v[1] = xyz_test_white.y / sum;

    /* Calculate CCT using Robertson's method */
    alwan_f64 cct = alwan_cct_robertson_xy_f64(&xy_test);
    if (cct < ALWAN_LITERAL(0.0)) {
        alwan_spd_destroy_f64(&test_spd_resampled, ctx);
        return ALWAN_E_RANGE;
    }

    /* Step 3: Reference illuminant at the test CCT, per CIE 13.3-1995:
     * a Planckian radiator below 5000 K, CIE daylight at or above it.
     *
     * A blackbody was used at every CCT until 2026-08-27, under a comment
     * saying "CIE 13.3-1995 specifies D-illuminant for CCT >= 5000K but
     * blackbody is a common substitute". It is not a substitute, it is a
     * different reference: D65 scored 97.2 against a 6504 K blackbody where a
     * daylight source measured against its own daylight reference must be 100.
     * The same substitution was present in CQS and TM-30. */
    alwan_spd_f64 reference_spd = {0};
    if (cct < ALWAN_LITERAL(5000.0)) {
        status = alwan_spd_blackbody_f64(&reference_spd, cct, TCS_WAVELENGTH_MIN, TCS_WAVELENGTH_MAX, ALWAN_TABLE_QUALITY_SPECTRUM, ctx);
    } else {
        status = quality_daylight_spd(&reference_spd, cct, ctx);
    }
    if (status != ALWAN_OK) {
        alwan_spd_destroy_f64(&test_spd_resampled, ctx);
        return ALWAN_E_NODATA;
    }

    /* Calculate reference white point */
    status = alwan_spd_create_f64(&perfect_white, reference_spd.wavelength_min, reference_spd.wavelength_max, reference_spd.count, ctx);
    if (status != ALWAN_OK) {
        alwan_spd_destroy_f64(&reference_spd, ctx);
        return ALWAN_E_NODATA;
    }

    for (size_t i = 0; i < perfect_white.count; i++) {
        perfect_white.values[i] = ALWAN_LITERAL(1.0);
    }

    alwan_xyz_f64 xyz_ref_white;
    status = alwan_xyz_from_spd_f64(&xyz_ref_white, &perfect_white, &reference_spd, ALWAN_OBSERVER_CIE_1931_2DEG, ALWAN_INTEGRATE_TRAPEZOID, ALWAN_LITERAL(0.0), ctx);
    alwan_spd_destroy_f64(&perfect_white, ctx);

    if (status != ALWAN_OK) {
        alwan_spd_destroy_f64(&reference_spd, ctx);
        alwan_spd_destroy_f64(&test_spd_resampled, ctx);
        return ALWAN_E_NODATA;
    }

    /* Calculate normalization factor for reference illuminant */
    alwan_f64 ref_norm_factor = ALWAN_LITERAL(1.0);
    if (xyz_ref_white.y > ALWAN_EPSILON) {
        ref_norm_factor = ALWAN_LITERAL(100.0) / xyz_ref_white.y;
        xyz_ref_white.x *= ref_norm_factor;
        xyz_ref_white.y *= ref_norm_factor;
        xyz_ref_white.z *= ref_norm_factor;
    }

    /* White-point chromaticities and adaptation terms, computed once. */
    alwan_f64 u_test_w, v_test_w, u_ref_w, v_ref_w;
    cri_xyz_to_uv(&u_test_w, &v_test_w, &xyz_test_white);
    cri_xyz_to_uv(&u_ref_w,  &v_ref_w,  &xyz_ref_white);
    alwan_f64 const c_t = cri_term_c(u_test_w, v_test_w);
    alwan_f64 const d_t = cri_term_d(u_test_w, v_test_w);
    alwan_f64 const c_r = cri_term_c(u_ref_w,  v_ref_w);
    alwan_f64 const d_r = cri_term_d(u_ref_w,  v_ref_w);

    /* Step 3: Calculate special CRI per TCS sample. Ra needs the first eight; a
     * caller asking for the specification gets all fourteen, and pays for the six
     * extra integrations only then. */
    alwan_f64 r_values[ALWAN_CRI_SAMPLES];
    int const count = spec_out ? ALWAN_CRI_SAMPLES : 8;

    for (int i = 0; i < count; i++) {
        /* Create TCS reflectance SPD */
        alwan_spd_f64 tcs_spd;
        status = alwan_spd_create_f64(&tcs_spd, TCS_WAVELENGTH_MIN, TCS_WAVELENGTH_MAX, ALWAN_TABLE_QUALITY_SPECTRUM, ctx);
        if (status != ALWAN_OK) {
            alwan_spd_destroy_f64(&reference_spd, ctx);
            return ALWAN_E_NODATA;
        }

        /* Copy TCS reflectance data */
        for (int j = 0; j < ALWAN_TABLE_QUALITY_SPECTRUM; j++) {
            tcs_spd.values[j] = alwan_table2d_row_at_f64_v(
                alwan_table_tcs_reflectance_f64, ALWAN_TABLE_TCS_SAMPLES,
                ALWAN_TABLE_QUALITY_SPECTRUM, i, j);
        }

        /* Calculate XYZ under test illuminant */
        alwan_xyz_f64 xyz_test;
        status = alwan_xyz_from_spd_f64(&xyz_test, &tcs_spd, &test_spd_resampled, ALWAN_OBSERVER_CIE_1931_2DEG, ALWAN_INTEGRATE_TRAPEZOID, ALWAN_LITERAL(0.0), ctx);
        if (status != ALWAN_OK) {
            alwan_spd_destroy_f64(&tcs_spd, ctx);
            alwan_spd_destroy_f64(&reference_spd, ctx);
            alwan_spd_destroy_f64(&test_spd_resampled, ctx);
            return ALWAN_E_NODATA;
        }

        /* Calculate XYZ under reference illuminant */
        alwan_xyz_f64 xyz_ref;
        status = alwan_xyz_from_spd_f64(&xyz_ref, &tcs_spd, &reference_spd, ALWAN_OBSERVER_CIE_1931_2DEG, ALWAN_INTEGRATE_TRAPEZOID, ALWAN_LITERAL(0.0), ctx);

        alwan_spd_destroy_f64(&tcs_spd, ctx);

        if (status != ALWAN_OK) {
            alwan_spd_destroy_f64(&reference_spd, ctx);
            alwan_spd_destroy_f64(&test_spd_resampled, ctx);
            return ALWAN_E_NODATA;
        }

        /* Normalize sample XYZ values using the same factors as the white points */
        xyz_test.x *= test_norm_factor;
        xyz_test.y *= test_norm_factor;
        xyz_test.z *= test_norm_factor;

        xyz_ref.x *= ref_norm_factor;
        xyz_ref.y *= ref_norm_factor;
        xyz_ref.z *= ref_norm_factor;

        /* CIE 13.3-1995 chromatic adaptation, then U*V*W* against the REFERENCE
         * white for both samples.
         *
         * Until 2026-08-27 each sample was converted to U*V*W* against its own
         * illuminant's white and the two differenced directly. That skips the
         * adaptation the standard specifies: the test sample's chromaticity is
         * supposed to be mapped onto the reference illuminant by a von Kries
         * step in CIE 1960 UCS, using the c and d terms, before any difference
         * is taken. */
        alwan_f64 u_k, v_k, u_kr, v_kr;
        cri_xyz_to_uv(&u_k, &v_k, &xyz_test);
        cri_xyz_to_uv(&u_kr, &v_kr, &xyz_ref);

        {
            alwan_f64 const ck = cri_term_c(u_k, v_k);
            alwan_f64 const dk = cri_term_d(u_k, v_k);
            alwan_f64 const cc = (ALWAN_ABS(c_t) > ALWAN_EPSILON) ? c_r / c_t : ALWAN_LITERAL(0.0);
            alwan_f64 const dd = (ALWAN_ABS(d_t) > ALWAN_EPSILON) ? d_r / d_t : ALWAN_LITERAL(0.0);
            alwan_f64 const den = ALWAN_LITERAL(16.518) + ALWAN_LITERAL(1.481) * cc * ck - dd * dk;
            if (ALWAN_ABS(den) > ALWAN_EPSILON) {
                u_k = (ALWAN_LITERAL(10.872) + ALWAN_LITERAL(0.404) * cc * ck
                       - ALWAN_LITERAL(4.0) * dd * dk) / den;
                v_k = ALWAN_LITERAL(5.52) / den;
            }
        }

        alwan_f64 U_t, V_t, W_t, U_r, V_r, W_r;
        cri_uvw(&U_t, &V_t, &W_t, u_k,  v_k,  xyz_test.y, u_ref_w, v_ref_w);
        cri_uvw(&U_r, &V_r, &W_r, u_kr, v_kr, xyz_ref.y,  u_ref_w, v_ref_w);

        alwan_f64 du = U_t - U_r;
        alwan_f64 dv = V_t - V_r;
        alwan_f64 dw = W_t - W_r;
        alwan_f64 delta_e = ALWAN_SQRT(du * du + dv * dv + dw * dw);

        /* CIE 13.3-1995: R_i = 100 - 4.6 * dE_i, with dE_i the raw U*V*W*
         * Euclidean distance and no divisor. An unpublished /15.5 "empirical
         * scaling factor" sat here until 2026-08-27; it compressed every score
         * toward 100 so the metric stopped discriminating between sources.
         * High-pressure sodium scored 94.1 where the CIE value is 8.5, and an
         * RGB LED scored 97.0 where it should be 57.4. Ra is allowed to go
         * negative for sources with very poor rendering; do not clamp it. */
        r_values[i] = ALWAN_LITERAL(100.0) - ALWAN_LITERAL(4.6) * delta_e;
    }

    /* Cleanup */
    alwan_spd_destroy_f64(&reference_spd, ctx);
    alwan_spd_destroy_f64(&test_spd_resampled, ctx);

    /* Step 4: Ra is the average of R1 to R8. R9 to R14 are reported and never
     * averaged in: CIE 13.3 defines Ra on the eight muted samples and leaves the
     * saturated four, the skin tone and the leaf green as special indices. */
    alwan_f64 ra = ALWAN_LITERAL(0.0);
    for (int i = 0; i < 8; i++) {
        ra += r_values[i];
    }
    ra /= ALWAN_LITERAL(8.0);

    if (spec_out) {
        spec_out->ra = ra;
        for (int i = 0; i < ALWAN_CRI_SAMPLES; i++) {
            spec_out->rs[i] = r_values[i];
        }
    }
    *ra_out = ra;
    return ALWAN_OK;
#endif
}

alwan_f64 alwan_cri_ra_f64(alwan_spd_f64 const *test_spd, alwan_ctx *ctx) {
    alwan_f64 ra;
    /* -1 stays this call's documented failure value. It is also a reachable Ra, which
     * is why the specification entry point below reports failure as a status. */
    if (quality_cri(&ra, test_spd, ctx, NULL) != ALWAN_OK) return ALWAN_LITERAL(-1.0);
    return ra;
}

alwan_status alwan_cri_specification_f64(alwan_cri_f64 *spec_out, alwan_spd_f64 const *test_spd, alwan_ctx *ctx) {
    alwan_f64 ra;
    if (!spec_out || !test_spd || !ctx) return ALWAN_E_INVALID;
    return quality_cri(&ra, test_spd, ctx, spec_out);
}

/* alwan_cri_specification_f32 lives with the other f32 wrappers, below the
 * spd_alloc_f64_from_f32 helper they all need. */

/* ----------------------------------------------------------------
 * Whiteness & Yellowness Indices (ASTM E313, CIE 2004)
 * ---------------------------------------------------------------- */

/* ASTM E313 coefficient table for Yellowness Index
 * Format: Cx, Cz for each illuminant/observer pair
 * Order: C/2 deg, D65/2 deg, C/10 deg, D65/10 deg
 *
 * STAYS HERE, and data/alwan_data_tables.h records why: these eight numbers
 * are published ASTM E313 coefficients written as ALWAN_LITERAL(), not a CSV
 * payload, so moving them means retyping them. The extent is spelled once,
 * below, and every subscript goes through alwan_table_row_f64_v -- the same
 * row gate the registry tables use, under the same
 * ALWAN_READ_DATA_NO_BOUND_CHECK. The callers validate the enum first; this is
 * the backstop for when that validation is itself wrong. */
enum { ASTM_E313_ROWS = 4 };

static alwan_f64 const astm_e313_yi_coeffs[ASTM_E313_ROWS][2] = {
    {ALWAN_LITERAL(1.2769), ALWAN_LITERAL(1.0592)},  /* C/2 deg */
    {ALWAN_LITERAL(1.2985), ALWAN_LITERAL(1.1335)},  /* D65/2 deg */
    {ALWAN_LITERAL(1.2871), ALWAN_LITERAL(1.0781)},  /* C/10 deg */
    {ALWAN_LITERAL(1.3013), ALWAN_LITERAL(1.1498)}   /* D65/10 deg */
};

/* ASTM E313 reference white chromaticity coordinates
 * Format: xn, yn for each illuminant/observer pair
 * Order: C/2 deg, D65/2 deg, C/10 deg, D65/10 deg
 * Values from colour-science library (CCS_ILLUMINANTS) */
static alwan_f64 const astm_e313_white_xy[][2] = {
    {ALWAN_LITERAL(0.31006), ALWAN_LITERAL(0.31616)},  /* C/2 deg */
    {ALWAN_D65_x, ALWAN_D65_y},                        /* D65/2 deg */
    {ALWAN_LITERAL(0.31039), ALWAN_LITERAL(0.31905)},  /* C/10 deg */
    {ALWAN_LITERAL(0.31382), ALWAN_LITERAL(0.33100)}   /* D65/10 deg */
};

/* ASTM E313 Yellowness Index
 * Formula: YI = 100(Cx*X - Cz*Z) / Y
 * where Cx and Cz are coefficients that depend on illuminant/observer */
alwan_status alwan_yellowness_astm_e313_f64(alwan_f64 *yi_out, alwan_xyz_f64 const *xyz, alwan_astm_e313_illuminant illuminant) {
    alwan_f64 X, Y, Z, Cx, Cz;
    int row;
    if (!yi_out || !xyz) return ALWAN_E_INVALID;
    if (illuminant < 0 || illuminant > ALWAN_ASTM_E313_D65_10DEG) return ALWAN_E_INVALID;
    X = xyz->x; Y = xyz->y; Z = xyz->z;
    if (ALWAN_ABS(Y) < ALWAN_EPSILON) return ALWAN_E_RANGE;   /* the index divides by Y */
    row = alwan_table_row_f64_v((int)illuminant, ASTM_E313_ROWS);
    Cx = astm_e313_yi_coeffs[row][0];
    Cz = astm_e313_yi_coeffs[row][1];
    *yi_out = alwan_yellowness_astm_e313_f64_v(X, Y, Z, Cx, Cz);
    return ALWAN_OK;
}

/* ASTM E313 Whiteness Index
 * Formula: WI = 3.388 * Z - 3 * Y
 * Note: This formula does not depend on illuminant/observer, but the parameter
 * is kept for API consistency with yellowness function */
alwan_status alwan_whiteness_astm_e313_f64(alwan_f64 *wi_out, alwan_xyz_f64 const *xyz, alwan_astm_e313_illuminant illuminant) {
    if (!wi_out || !xyz) return ALWAN_E_INVALID;
    if (illuminant < 0 || illuminant > ALWAN_ASTM_E313_D65_10DEG) return ALWAN_E_INVALID;
    *wi_out = alwan_whiteness_astm_e313_f64_v(xyz->y, xyz->z);   /* the formula has no illuminant term */
    return ALWAN_OK;
}

/* CIE 2004 Whiteness Index
 * Formula: W = Y + 800(xn - x) + 1700(yn - y)
 * where xn, yn are the reference white chromaticity coordinates */
alwan_status alwan_whiteness_cie2004_f64(alwan_f64 *w_out, alwan_vec2_f64 const *xy, alwan_f64 Y, alwan_vec2_f64 const *xy_n) {
    if (!w_out || !xy || !xy_n) return ALWAN_E_INVALID;
    *w_out = alwan_whiteness_cie2004_f64_v(xy->v[0], xy->v[1], Y, xy_n->v[0], xy_n->v[1]);
    return ALWAN_OK;
}

/* ================================================================
 * Native f32 API wrappers
 *
 * The CCT/whiteness/yellowness formulas are templated in
 * alwan_quality_core.inc and instantiated for both precisions by
 * alwan_quality_core.h, so each f32 entry point below computes
 * natively in float via the matching alwan_*_f32_v core function --
 * mirroring the f64 wrappers above (same guards, same parameter
 * convention), without re-deriving any formula.
 *
 * Exception: alwan_cct_kang_xy_f32 (the xy->CCT inverse) is a genuine
 * iterative Newton-Raphson solver and reuses the f64 solver by design.
 * ================================================================ */
#if ALWAN_WITH_F32

/* McCamy's approximation: CCT from CIE 1931 xy (native f32) */
alwan_f32 alwan_cct_mccamy_xy_f32(alwan_vec2_f32 const *xy) {
    if (!xy) {
        return ALWAN_LITERAL_F32(-1.0);
    }

    alwan_f32 denom = ALWAN_LITERAL_F32(0.1858) - xy->v[1];
    if (ALWAN_ABS_F32(denom) < ALWAN_EPSILON_F32) {
        return ALWAN_LITERAL_F32(-1.0);
    }

    return alwan_cct_mccamy_f32_v(xy->v[0], xy->v[1]);
}

/* Robertson 1968: CCT from CIE 1931 xy via isotemperature-line table
 * interpolation (native f32; the f64 locus table is read and the entire
 * search/interpolation is carried out in single precision). */
alwan_f32 alwan_cct_robertson_xy_f32(alwan_vec2_f32 const *xy) {
    if (!xy) {
        return ALWAN_LITERAL_F32(-1.0);
    }
#if !ALWAN_TABLE_ROBERTSON_LOCUS
    return ALWAN_LITERAL_F32(-1.0);   /* table compiled out, as in the f64 twin */
#else

    alwan_f32 x = xy->v[0];
    alwan_f32 y = xy->v[1];

    /* Convert CIE 1931 xy to CIE 1960 UCS uv */
    alwan_f32 denom = ALWAN_LITERAL_F32(12.0) * y - ALWAN_LITERAL_F32(2.0) * x + ALWAN_LITERAL_F32(3.0);
    if (ALWAN_ABS_F32(denom) < ALWAN_EPSILON_F32) {
        return ALWAN_LITERAL_F32(-1.0);
    }

    alwan_f32 u = (ALWAN_LITERAL_F32(4.0) * x) / denom;
    alwan_f32 v = (ALWAN_LITERAL_F32(6.0) * y) / denom;

    alwan_f32 last_dt = ALWAN_ZERO_F32;

    for (int i = 1; i < ALWAN_TABLE_ROBERTSON_ROWS; i++) {
        alwan_f32 u_i = (alwan_f32)ROBERTSON_AT(i, 1);
        alwan_f32 v_i = (alwan_f32)ROBERTSON_AT(i, 2);
        alwan_f32 t_i = (alwan_f32)ROBERTSON_AT(i, 3);

        alwan_f32 du = ALWAN_ONE_F32;
        alwan_f32 dv = t_i;
        alwan_f32 length = ALWAN_SQRT_F32(ALWAN_ONE_F32 + dv * dv);
        du /= length;
        dv /= length;

        alwan_f32 uu = u - u_i;
        alwan_f32 vv = v - v_i;
        alwan_f32 dt = -uu * dv + vv * du;

        if (dt <= ALWAN_ZERO_F32 || i == ALWAN_TABLE_ROBERTSON_ROWS - 1) {
            if (dt > ALWAN_ZERO_F32) dt = ALWAN_ZERO_F32;

            dt = -dt;

            alwan_f32 f = (i == 1)
                ? ALWAN_ZERO_F32
                : dt / (last_dt + dt);

            alwan_f32 r_prev = (alwan_f32)ROBERTSON_AT(i - 1, 0);
            alwan_f32 r_curr = (alwan_f32)ROBERTSON_AT(i, 0);

            alwan_f32 r = r_prev * f + r_curr * (ALWAN_ONE_F32 - f);

            if (r < ALWAN_EPSILON_F32) {
                return ALWAN_LITERAL_F32(-1.0);
            }

            return ALWAN_LITERAL_F32(1e6) / r;
        }

        last_dt = dt;
    }

    return ALWAN_LITERAL_F32(-1.0);
#endif
}

/* Hernandez-Andres 1999: xy to CCT (native f32) */
alwan_f32 alwan_cct_hernandez_xy_f32(alwan_vec2_f32 const *xy) {
    if (!xy) return ALWAN_LITERAL_F32(-1.0);

    alwan_f32 denom = xy->v[1] - ALWAN_LITERAL_F32(0.1735);
    if (ALWAN_ABS_F32(denom) < ALWAN_LITERAL_F32(1e-10)) {
        return ALWAN_LITERAL_F32(-1.0);
    }

    return alwan_cct_hernandez_f32_v(xy->v[0], xy->v[1]);
}

/* Kang 2002: CCT to xy forward transform (native f32) */
void alwan_cct_to_xy_kang_f32(alwan_vec2_f32 *xy_out, alwan_f32 cct) {
    if (!xy_out) return;

    *xy_out = alwan_cct_to_xy_kang_f32_v(cct);
}

/* Kang 2002: xy to CCT inverse.
 * f32 reuses the f64 solver by design -- this is an iterative
 * Newton-Raphson solve whose convergence/canonical-value-selection
 * tolerances are tuned below f32 epsilon, so single precision cannot
 * reproduce it natively. Convert in, solve in f64, convert out. */
alwan_f32 alwan_cct_kang_xy_f32(alwan_vec2_f32 const *xy) {
    if (!xy) return ALWAN_LITERAL_F32(-1.0);

    alwan_vec2_f64 xy64;
    xy64.v[0] = (alwan_f64)xy->v[0];
    xy64.v[1] = (alwan_f64)xy->v[1];

    return (alwan_f32)alwan_cct_kang_xy_f64(&xy64);
}

/* ASTM E313 Yellowness Index (native f32) */
alwan_status alwan_yellowness_astm_e313_f32(alwan_f32 *yi_out, alwan_xyz_f32 const *xyz, alwan_astm_e313_illuminant illuminant) {
    alwan_f32 X, Y, Z, Cx, Cz;
    int row;
    if (!yi_out || !xyz) return ALWAN_E_INVALID;
    if (illuminant < 0 || illuminant > ALWAN_ASTM_E313_D65_10DEG) return ALWAN_E_INVALID;
    X = xyz->x; Y = xyz->y; Z = xyz->z;
    if (ALWAN_ABS_F32(Y) < ALWAN_EPSILON_F32) return ALWAN_E_RANGE;
    row = alwan_table_row_f64_v((int)illuminant, ASTM_E313_ROWS);
    Cx = (alwan_f32)astm_e313_yi_coeffs[row][0];
    Cz = (alwan_f32)astm_e313_yi_coeffs[row][1];
    *yi_out = alwan_yellowness_astm_e313_f32_v(X, Y, Z, Cx, Cz);
    return ALWAN_OK;
}

/* ASTM E313 Whiteness Index (native f32) */
alwan_status alwan_whiteness_astm_e313_f32(alwan_f32 *wi_out, alwan_xyz_f32 const *xyz, alwan_astm_e313_illuminant illuminant) {
    if (!wi_out || !xyz) return ALWAN_E_INVALID;
    if (illuminant < 0 || illuminant > ALWAN_ASTM_E313_D65_10DEG) return ALWAN_E_INVALID;
    *wi_out = alwan_whiteness_astm_e313_f32_v(xyz->y, xyz->z);
    return ALWAN_OK;
}

/* CIE 2004 Whiteness Index (native f32) */
alwan_status alwan_whiteness_cie2004_f32(alwan_f32 *w_out, alwan_vec2_f32 const *xy, alwan_f32 Y, alwan_vec2_f32 const *xy_n) {
    if (!w_out || !xy || !xy_n) return ALWAN_E_INVALID;
    *w_out = alwan_whiteness_cie2004_f32_v(xy->v[0], xy->v[1], Y, xy_n->v[0], xy_n->v[1]);
    return ALWAN_OK;
}

#endif /* ALWAN_WITH_F32 */

/* ----------------------------------------------------------------
 * SSI (Spectral Similarity Index) - Academy/SMPTE ST 2122
 * ---------------------------------------------------------------- */

/* SSI spectral shape: 375-675nm at 1nm intervals (301 samples) */
#define SSI_WAVELENGTH_MIN ALWAN_LITERAL(375.0)
#define SSI_WAVELENGTH_MAX ALWAN_LITERAL(675.0)
#define SSI_WAVELENGTH_COUNT 301

/* SSI uses 10nm bins: 380-670nm (30 bins). One spelling of that count, the
 * registry enum, so the working buffers below cannot drift from the weight
 * table they are multiplied by. */
#define SSI_BIN_COUNT ALWAN_TABLE_SSI_BIN_COUNT

/* The 11-tap trapezoidal binning kernel and the 30 per-bin spectral weights
 * are homed in the registry as alwan_table_ssi_bin_weights_{f32,f64} (extent
 * ALWAN_TABLE_SSI_BIN_TAPS) and alwan_table_ssi_spectral_weights_{f32,f64}
 * (extent ALWAN_TABLE_SSI_BIN_COUNT), read through alwan_table1d_row_f64_v.
 *
 * The 3-tap smoothing kernel stays here: all nine of its subscripts are the
 * literals 0, 1 and 2, so no runtime value ever reaches them and there is
 * nothing for the row gate to gate. */
/* Convolution kernel for smoothing: [0.22, 0.56, 0.22] */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const ssi_smooth_kernel[3] = {
#include "../data/ssi_smooth_kernel.csv"
};
ALWAN_DIAG_POP

/* Academy Spectral Similarity Index (SSI) calculation
 * Based on SMPTE ST 2122 and Academy S-2018-001
 *
 * Algorithm:
 * 1. Resample SPDs to 375-675nm at 1nm intervals
 * 2. Bin to 10nm bins (380-670nm, 30 bins)
 * 3. Normalize by total irradiance
 * 4. Calculate weighted relative difference
 * 5. Smooth with convolution
 * 6. Compute SSI = 100 - 32*sqrt(sum(smoothed^2))
 */
alwan_f64 alwan_ssi_calculate_f64(alwan_spd_f64 const *test_spd, alwan_spd_f64 const *reference_spd, alwan_ctx *ctx) {
    if (!ctx || !test_spd || !reference_spd) {
        return ALWAN_LITERAL(-1.0);
    }
#if !(ALWAN_TABLE_SSI_BIN_WEIGHTS && ALWAN_TABLE_SSI_SPECTRAL_WEIGHTS)
    return ALWAN_LITERAL(-1.0);   /* SSI weights compiled out: the documented error value */
#else

    /* Step 1: Resample both SPDs to SSI spectral shape (375-675nm, 1nm) */
    alwan_spd_f64 test_resampled, ref_resampled;
    int result = alwan_spd_resample_f64(&test_resampled, test_spd, SSI_WAVELENGTH_MIN, SSI_WAVELENGTH_MAX, SSI_WAVELENGTH_COUNT, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO, ctx);
    if (result != ALWAN_OK) {
        return ALWAN_LITERAL(-2.0);
    }

    result = alwan_spd_resample_f64(&ref_resampled, reference_spd, SSI_WAVELENGTH_MIN, SSI_WAVELENGTH_MAX, SSI_WAVELENGTH_COUNT, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO, ctx);
    if (result != ALWAN_OK) {
        alwan_spd_destroy_f64(&test_resampled, ctx);
        return ALWAN_LITERAL(-2.0);
    }

    /* Step 2: Bin to 10nm bins (380-670nm, 30 bins) using integration matrix */
    alwan_f64 test_binned[SSI_BIN_COUNT];
    alwan_f64 ref_binned[SSI_BIN_COUNT];

    for (size_t i = 0; i < SSI_BIN_COUNT; i++) {
        test_binned[i] = ALWAN_LITERAL(0.0);
        ref_binned[i] = ALWAN_LITERAL(0.0);

        /* Bin i is centred on 380 + 10 i nm and spans 375 + 10 i to 385 + 10 i: samples
         * 10 i to 10 i + 10 of the 375 nm grid, the ends at half weight, as colour's
         * _MATRIX_INTEGRATION. Until 2026-09-25 it started at 5 + 10 i, 5 nm late. */
        size_t start_idx = i * 10;

        for (size_t j = 0; j < ALWAN_TABLE_SSI_BIN_TAPS; j++) {
            if (start_idx + j < SSI_WAVELENGTH_COUNT) {
                alwan_f64 const w = alwan_table1d_row_f64_v(
                    alwan_table_ssi_bin_weights_f64, ALWAN_TABLE_SSI_BIN_TAPS, (int)j);
                test_binned[i] += test_resampled.values[start_idx + j] * w;
                ref_binned[i] += ref_resampled.values[start_idx + j] * w;
            }
        }
    }

    /* Step 3: Normalize by total irradiance */
    alwan_f64 test_sum = ALWAN_LITERAL(0.0);
    alwan_f64 ref_sum = ALWAN_LITERAL(0.0);

    for (size_t i = 0; i < SSI_BIN_COUNT; i++) {
        test_sum += test_binned[i];
        ref_sum += ref_binned[i];
    }

    if (test_sum < ALWAN_EPSILON || ref_sum < ALWAN_EPSILON) {
        alwan_spd_destroy_f64(&test_resampled, ctx);
        alwan_spd_destroy_f64(&ref_resampled, ctx);
        return ALWAN_LITERAL(-3.0);
    }

    for (size_t i = 0; i < SSI_BIN_COUNT; i++) {
        test_binned[i] /= test_sum;
        ref_binned[i] /= ref_sum;
    }

    /* Step 4: Calculate weighted relative difference */
    alwan_f64 wdr[SSI_BIN_COUNT];

    for (size_t i = 0; i < SSI_BIN_COUNT; i++) {
        /* Relative difference: dr = (test - ref) / (ref + 1/30) */
        alwan_f64 dr = (test_binned[i] - ref_binned[i]) / (ref_binned[i] + ALWAN_LITERAL(1.0) / ALWAN_LITERAL(30.0));

        /* Apply spectral weight */
        wdr[i] = dr * alwan_table1d_row_f64_v(alwan_table_ssi_spectral_weights_f64,
                                              ALWAN_TABLE_SSI_BIN_COUNT, (int)i);
    }

    /* Step 5: Smooth with 1D convolution [0.22, 0.56, 0.22] */
    alwan_f64 c_wdr[SSI_BIN_COUNT];

    /* The edges see zero beyond the array, as colour's convolve1d (mode constant, cval 0).
     * Until 2026-09-25 the missing tap was folded back onto the edge bin. */
    c_wdr[0] = wdr[0] * ssi_smooth_kernel[1] + wdr[1] * ssi_smooth_kernel[2];
    c_wdr[SSI_BIN_COUNT - 1] = wdr[SSI_BIN_COUNT - 2] * ssi_smooth_kernel[0] +
                                wdr[SSI_BIN_COUNT - 1] * ssi_smooth_kernel[1];

    /* Interior points */
    for (size_t i = 1; i < SSI_BIN_COUNT - 1; i++) {
        c_wdr[i] = wdr[i - 1] * ssi_smooth_kernel[0] +
                   wdr[i] * ssi_smooth_kernel[1] +
                   wdr[i + 1] * ssi_smooth_kernel[2];
    }

    /* Step 6: Compute final SSI = 100 - 32*sqrt(sum(c_wdr^2)) */
    alwan_f64 sum_squares = ALWAN_LITERAL(0.0);
    for (size_t i = 0; i < SSI_BIN_COUNT; i++) {
        sum_squares += c_wdr[i] * c_wdr[i];
    }

    alwan_f64 ssi = ALWAN_LITERAL(100.0) - ALWAN_LITERAL(32.0) * ALWAN_SQRT(sum_squares);

    /* Cleanup */
    alwan_spd_destroy_f64(&test_resampled, ctx);
    alwan_spd_destroy_f64(&ref_resampled, ctx);

    return ssi;
#endif
}

/* ================================================================
 * CIE Special Metamerism Index: Change in Illuminant
 * ================================================================
 * Based on CIE 15:2004 and CIE 80-1989
 *
 * Quantifies the degree of color mismatch when two samples that are
 * a metameric match under one illuminant are viewed under a different
 * illuminant.
 *
 * Algorithm:
 * 1. Compute tristimulus values for both samples under test illuminant
 * 2. Compute white point of test illuminant
 * 3. Convert to CIELAB using test illuminant white point
 * 4. Calculate dE*ab between sample and reference under test conditions
 * 5. Return dE as the metamerism index
 *
 * Computes color difference under the test illuminant only.
 * Assumes the samples are a metameric match under the reference illuminant.
 */
alwan_f64 alwan_metamerism_index_f64(alwan_spd_f64 const *sample_reflectance, alwan_spd_f64 const *reference_reflectance, alwan_spd_f64 const *reference_illuminant, alwan_spd_f64 const *test_illuminant, alwan_observer_type observer, alwan_ctx *ctx)
{
    if (!ctx || !sample_reflectance || !reference_reflectance ||
        !reference_illuminant || !test_illuminant) {
        return ALWAN_LITERAL(-1.0);
    }

    /* Step 1: Compute XYZ for sample under test illuminant */
    alwan_xyz_f64 xyz_sample_test, xyz_ref_test;
    int status;

    status = alwan_xyz_from_spd_f64(&xyz_sample_test, sample_reflectance, test_illuminant, observer, ALWAN_INTEGRATE_TRAPEZOID, ALWAN_LITERAL(0.0), ctx);
    if (status != ALWAN_OK) {
        return ALWAN_LITERAL(-1.0);
    }

    /* Step 2: Compute XYZ for reference under test illuminant */
    status = alwan_xyz_from_spd_f64(&xyz_ref_test, reference_reflectance, test_illuminant, observer, ALWAN_INTEGRATE_TRAPEZOID, ALWAN_LITERAL(0.0), ctx);
    if (status != ALWAN_OK) {
        return ALWAN_LITERAL(-1.0);
    }

    /* Step 2b: CIE 015's multiplicative correction for a pair that does not match exactly
     * under the reference illuminant: the sample's tristimulus values under the test
     * illuminant are scaled by reference / sample under the reference illuminant, channel by
     * channel, so the residual reference-illuminant mismatch is not counted as metamerism.
     * Until 2026-09-25 the reference illuminant was not used at all. */
    {
        alwan_xyz_f64 xyz_sample_ref, xyz_ref_ref;
        status = alwan_xyz_from_spd_f64(&xyz_sample_ref, sample_reflectance, reference_illuminant, observer, ALWAN_INTEGRATE_TRAPEZOID, ALWAN_LITERAL(0.0), ctx);
        if (status != ALWAN_OK) return ALWAN_LITERAL(-1.0);
        status = alwan_xyz_from_spd_f64(&xyz_ref_ref, reference_reflectance, reference_illuminant, observer, ALWAN_INTEGRATE_TRAPEZOID, ALWAN_LITERAL(0.0), ctx);
        if (status != ALWAN_OK) return ALWAN_LITERAL(-1.0);
        if (!(xyz_sample_ref.x > ALWAN_LITERAL(0.0) && xyz_sample_ref.y > ALWAN_LITERAL(0.0) && xyz_sample_ref.z > ALWAN_LITERAL(0.0))) {
            return ALWAN_LITERAL(-1.0);
        }
        xyz_sample_test.x *= xyz_ref_ref.x / xyz_sample_ref.x;
        xyz_sample_test.y *= xyz_ref_ref.y / xyz_sample_ref.y;
        xyz_sample_test.z *= xyz_ref_ref.z / xyz_sample_ref.z;
    }

    /* Step 3: Compute white point of test illuminant */
    /* Create a perfect reflectance SPD (all values = 1.0) */
    alwan_spd_f64 perfect_white;
    status = alwan_spd_create_f64(&perfect_white, test_illuminant->wavelength_min, test_illuminant->wavelength_max, test_illuminant->count, ctx);
    if (status != ALWAN_OK) {
        return ALWAN_LITERAL(-1.0);
    }

    /* Set all reflectance values to 1.0 */
    for (size_t i = 0; i < perfect_white.count; i++) {
        perfect_white.values[i] = ALWAN_LITERAL(1.0);
    }

    alwan_xyz_f64 xyz_test_white;
    status = alwan_xyz_from_spd_f64(&xyz_test_white, &perfect_white, test_illuminant, observer, ALWAN_INTEGRATE_TRAPEZOID, ALWAN_LITERAL(0.0), ctx);
    alwan_spd_destroy_f64(&perfect_white, ctx);

    if (status != ALWAN_OK) {
        return ALWAN_LITERAL(-1.0);
    }

    /* Step 4: Convert to CIELAB using test illuminant white point */
    alwan_lab_f64 lab_sample, lab_ref;
    alwan_xyz_to_lab_f64(&lab_sample, &xyz_sample_test, &xyz_test_white);
    alwan_xyz_to_lab_f64(&lab_ref, &xyz_ref_test, &xyz_test_white);

    /* Step 5: Compute dE*ab (1976) */
    alwan_f64 delta_e = alwan_delta_e_76_f64(&lab_sample, &lab_ref);

    return delta_e;
}

/* ----------------------------------------------------------------
 * CQS (Color Quality Scale) - NIST
 * ---------------------------------------------------------------- */


/* NIST CQS 9.0 Qa, through alwan_quality_colour.c, which follows colour-science step for
 * step. The -1 on failure is the 2.0.0 contract; alwan_cqs_specification reports a status.
 *
 * Until 2026-09-25 this ran its own pipeline: the test SPD resampled to 360-830 nm at 5 nm
 * and zero past its ends, trapezoid sums, Robertson's CCT, daylight without CIE 15's
 * rounded M1 and M2, and the VS samples at 5 nm. That left it up to 0.24 below
 * colour-science (HP1 33.466 against 33.702). */
alwan_f64 alwan_cqs_calculate_f64(alwan_spd_f64 const *test_spd, alwan_ctx *ctx) {
    alwan_cqs_f64 spec;
    if (!ctx || !test_spd) return ALWAN_LITERAL(-1.0);
    if (alwan__cqs_compute(&spec, test_spd, ALWAN_CQS_9_0, ctx) != ALWAN_OK) return ALWAN_LITERAL(-1.0);
    return spec.qa;
}

/* ----------------------------------------------------------------
 * TM-30 (ANSI/IES TM-30-20)
 * ---------------------------------------------------------------- */


/* Build the CIE daylight SPD for an arbitrary CCT on the quality grid.
 *
 * CIE 15:2004: xD is a cubic in 1/T with two branches meeting at 7000 K,
 * yD follows from xD, and the SPD is S0 + M1*S1 + M2*S2. Valid 4000-25000 K;
 * outside that the daylight model is not defined and the caller must not ask.
 *
 * This exists because alwan_tm30_rf_* previously substituted D65 for every CCT
 * at or above 5000 K, which scored D50 against D65 and cost ~3 Rf points on
 * every daylight source. */
static alwan_status quality_daylight_spd(alwan_spd_f64 *out, alwan_f64 cct, alwan_ctx *ctx) {
#if !ALWAN_TABLE_DAYLIGHT_BASIS
    /* The S0/S1/S2 basis was compiled out (data/alwan_data_tables_config.h). Every
     * caller already propagates a failed status as its own error value. */
    (void)out; (void)cct; (void)ctx;
    return ALWAN_E_NODATA;
#else
    alwan_f64 const t  = cct;
    alwan_f64 const t2 = t * t;
    alwan_f64 const t3 = t2 * t;
    alwan_f64 xd, yd;

    if (t < ALWAN_LITERAL(4000.0) || t > ALWAN_LITERAL(25000.0)) {
        return ALWAN_E_RANGE;
    }

    if (t <= ALWAN_LITERAL(7000.0)) {
        xd = ALWAN_LITERAL(0.244063)
           + ALWAN_LITERAL(0.09911e3)  / t
           + ALWAN_LITERAL(2.9678e6)   / t2
           - ALWAN_LITERAL(4.6070e9)   / t3;
    } else {
        xd = ALWAN_LITERAL(0.237040)
           + ALWAN_LITERAL(0.24748e3)  / t
           + ALWAN_LITERAL(1.9018e6)   / t2
           - ALWAN_LITERAL(2.0064e9)   / t3;
    }
    yd = ALWAN_LITERAL(-3.000) * xd * xd + ALWAN_LITERAL(2.870) * xd - ALWAN_LITERAL(0.275);

    {
        alwan_f64 const denom = ALWAN_LITERAL(0.0241)
                              + ALWAN_LITERAL(0.2562) * xd
                              - ALWAN_LITERAL(0.7341) * yd;
        alwan_f64 m1, m2;
        alwan_status st;
        int j;

        if (ALWAN_ABS(denom) < ALWAN_LITERAL(1e-12)) {
            return ALWAN_E_RANGE;
        }
        m1 = (ALWAN_LITERAL(-1.3515) - ALWAN_LITERAL(1.7703)  * xd
              + ALWAN_LITERAL(5.9114)  * yd) / denom;
        m2 = (ALWAN_LITERAL(0.0300)  - ALWAN_LITERAL(31.4424) * xd
              + ALWAN_LITERAL(30.0717) * yd) / denom;

        st = alwan_spd_create_f64(out, CES_WAVELENGTH_MIN, CES_WAVELENGTH_MAX,
                                  ALWAN_TABLE_QUALITY_SPECTRUM, ctx);
        if (st != ALWAN_OK) {
            return st;
        }
        for (j = 0; j < ALWAN_TABLE_QUALITY_SPECTRUM; j++) {
            alwan_f64 const s0 = alwan_table2d_row_at_f64_v(
                alwan_table_daylight_basis_f64, ALWAN_TABLE_DAYLIGHT_BASIS_ROWS,
                ALWAN_TABLE_QUALITY_SPECTRUM, 0, j);
            alwan_f64 const s1 = alwan_table2d_row_at_f64_v(
                alwan_table_daylight_basis_f64, ALWAN_TABLE_DAYLIGHT_BASIS_ROWS,
                ALWAN_TABLE_QUALITY_SPECTRUM, 1, j);
            alwan_f64 const s2 = alwan_table2d_row_at_f64_v(
                alwan_table_daylight_basis_f64, ALWAN_TABLE_DAYLIGHT_BASIS_ROWS,
                ALWAN_TABLE_QUALITY_SPECTRUM, 2, j);
            out->values[j] = s0 + m1 * s1 + m2 * s2;
        }
    }
    return ALWAN_OK;
#endif
}

/* TM-30 Fidelity Index (Rf) calculation
 * Based on ANSI/IES TM-30-20 specification
 *
 * Algorithm:
 * 1. Use CIE 1964 10 deg observer for all calculations
 * 2. For test and reference illuminants:
 *    a. For each of 99 CES (Color Evaluation Samples):
 *       - Calculate XYZ
 *       - Apply CAT02 chromatic adaptation
 *       - Calculate CIECAM02 color appearance
 *       - Convert to CAM02-UCS (J', a', b')
 * 3. Calculate color differences in CAM02-UCS space
 * 4. Rf = 10 * ln(exp((100 - 6.73 * dE_mean) / 10) + 1)
 *
 * Returns: Rf value (0-100), or negative on error
 *
 * Accuracy, measured against colour.quality.colour_fidelity_index_CIE2017
 * over 35 CIE illuminants: mean absolute deviation 0.51 Rf, worst 2.0 (HP1).
 * The residual is convention, not algorithm: alwan integrates 360-830 nm with
 * the trapezoid rule where cfi2017 integrates 380-780 nm by summation.
 * Before 2026-08-27 the same comparison was off by up to 65 Rf points, because
 * the 99 CES reflectances were 80 copies of a constant 0.5 and four of the
 * metric's coefficients were wrong.
 */
/* The TM-30 pipeline. Returns Rf, or -1 on failure, and fills spec_out when one is given.
 * The per-sample appearance coordinates that the fidelity index averages away are exactly
 * what the rest of TM-30-18 is built from, so they are kept here rather than the whole
 * spectral walk being run a second time. */
/* CIE 2017 / TM-30-18 through alwan_quality_colour.c, which follows colour-science step
 * for step. Until 2026-09-25 this resampled to 360-830 nm at 5 nm with trapezoid sums and a
 * Robertson CCT, and blended no references between 4000 and 5000 K: Rf up to 4e-3 and Rg
 * up to 6e-4 from colour-science. -1 on failure, the value alwan_tm30_rf documents. */
static alwan_f64 quality_tm30(alwan_spd_f64 const *test_spd, alwan_ctx *ctx, alwan_tm30_f64 *spec_out) {
    alwan_tm30_f64 spec;
    if (!ctx || !test_spd) return ALWAN_LITERAL(-1.0);
    if (alwan__cie2017_compute(&spec, test_spd, ctx) != ALWAN_OK) return ALWAN_LITERAL(-1.0);
    if (spec_out) *spec_out = spec;
    return spec.rf;
}

alwan_f64 alwan_tm30_rf_f64(alwan_spd_f64 const *test_spd, alwan_ctx *ctx) {
    return quality_tm30(test_spd, ctx, NULL);
}

alwan_status alwan_tm30_specification_f64(alwan_tm30_f64 *spec_out, alwan_spd_f64 const *test_spd, alwan_ctx *ctx) {
    alwan_f64 rf;
    if (!spec_out || !test_spd || !ctx) return ALWAN_E_INVALID;
    rf = quality_tm30(test_spd, ctx, spec_out);
    /* Rf asymptotes towards zero and never passes it; -1 is the pipeline's failure value,
     * which it returns when the CES reflectances were compiled out or a step failed. */
    if (rf < ALWAN_LITERAL(0.0)) return ALWAN_E_NODATA;
    return ALWAN_OK;
}

/* alwan_tm30_specification_f32 lives with the other f32 wrappers, below the
 * spd_alloc_f64_from_f32 helper they all need. */

/* ----------------------------------------------------------------
 * CIE 224:2017 Color Fidelity Index
 * ---------------------------------------------------------------- */

/* CIE 224:2017 Color Fidelity Index (Rf) calculation
 * Based on CIE 224:2017 specification
 *
 * Algorithm: Same as TM-30 but per CIE standard
 * 1. Use CIE 1964 10 deg observer
 * 2. Use 99 CES samples
 * 3. Apply CAT02 chromatic adaptation
 * 4. Use CIECAM02 and CAM02-UCS color space
 * 5. Rf = 10 * ln(exp((100 - 6.73 * dE_mean) / 10) + 1)
 *
 * Note: Shares implementation with TM-30
 *
 * Returns: Rf value (0-100), or negative on error
 */
alwan_f64 alwan_cie224_rf_f64(alwan_spd_f64 const *test_spd, alwan_ctx *ctx) {
    /* CIE 224:2017 uses the same algorithm as TM-30
     * Both standards use identical methodology:
     * - 99 CES samples
     * - 10 deg observer
     * - CIECAM02 with CAM02-UCS color space
     * - Same formula for Rf calculation
     */
    return alwan_tm30_rf_f64(test_spd, ctx);
}

/* ----------------------------------------------------------------
 * Weber & Michelson Contrast
 *
 * Templatized in alwan_vision_impl.inc.
 * ---------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * D-Series Illuminant from CCT
 * ---------------------------------------------------------------- */

alwan_status alwan_d_series_illuminant_xy_f64(alwan_vec2_f64 *xy_out, alwan_f64 cct) {
    if (!xy_out) return ALWAN_E_INVALID;
    if (cct < ALWAN_LITERAL(4000.0) || cct > ALWAN_LITERAL(25000.0))
        return ALWAN_E_RANGE;
    *xy_out = alwan_d_series_xy_f64_v(cct);
    return ALWAN_OK;
}

alwan_status alwan_d_series_illuminant_xy_f32(alwan_vec2_f32 *xy_out, alwan_f32 cct) {
    if (!xy_out) return ALWAN_E_INVALID;
    if (cct < 4000.0f || cct > 25000.0f)
        return ALWAN_E_RANGE;
    *xy_out = alwan_d_series_xy_f32_v(cct);
    return ALWAN_OK;
}

/* ================================================================
 * f32 wrappers for SPD-based quality metrics
 *
 * These allocate a temporary f64 SPD mirror and delegate to the
 * f64 implementations (which use f64 CMF tables and integrators).
 * ================================================================ */

static int spd_alloc_f64_from_f32(alwan_spd_f64 *out, alwan_ctx *ctx, alwan_spd_f32 const *in) {
    int rc = alwan_spd_create_f64(out, (alwan_f64)in->wavelength_min, (alwan_f64)in->wavelength_max, in->count, ctx);
    if (rc != ALWAN_OK) return rc;
    for (size_t i = 0; i < in->count; i++) {
        out->values[i] = (alwan_f64)in->values[i];
    }
    return ALWAN_OK;
}

alwan_f32 alwan_cri_ra_f32(alwan_spd_f32 const *test_spd, alwan_ctx *ctx) {
    if (!test_spd) return -1.0f;
    alwan_spd_f64 tmp;
    if (spd_alloc_f64_from_f32(&tmp, ctx, test_spd) != ALWAN_OK) return -1.0f;
    alwan_f64 result = alwan_cri_ra_f64(&tmp, ctx);
    alwan_spd_destroy_f64(&tmp, ctx);
    return (alwan_f32)result;
}

alwan_status alwan_cri_specification_f32(alwan_cri_f32 *spec_out, alwan_spd_f32 const *test_spd, alwan_ctx *ctx) {
    alwan_cri_f64 wide;
    alwan_spd_f64 tmp;
    alwan_status st;
    int i;
    if (!spec_out || !test_spd || !ctx) return ALWAN_E_INVALID;
    if (spd_alloc_f64_from_f32(&tmp, ctx, test_spd) != ALWAN_OK) return ALWAN_E_NOMEM;
    st = alwan_cri_specification_f64(&wide, &tmp, ctx);
    alwan_spd_destroy_f64(&tmp, ctx);
    if (st != ALWAN_OK) return st;
    spec_out->ra = (alwan_f32)wide.ra;
    for (i = 0; i < ALWAN_CRI_SAMPLES; i++) {
        spec_out->rs[i] = (alwan_f32)wide.rs[i];
    }
    return ALWAN_OK;
}

alwan_f32 alwan_cqs_calculate_f32(alwan_spd_f32 const *test_spd, alwan_ctx *ctx) {
    if (!test_spd) return -1.0f;
    alwan_spd_f64 tmp;
    if (spd_alloc_f64_from_f32(&tmp, ctx, test_spd) != ALWAN_OK) return -1.0f;
    alwan_f64 result = alwan_cqs_calculate_f64(&tmp, ctx);
    alwan_spd_destroy_f64(&tmp, ctx);
    return (alwan_f32)result;
}

alwan_f32 alwan_tm30_rf_f32(alwan_spd_f32 const *test_spd, alwan_ctx *ctx) {
    if (!test_spd) return -1.0f;
    alwan_spd_f64 tmp;
    if (spd_alloc_f64_from_f32(&tmp, ctx, test_spd) != ALWAN_OK) return -1.0f;
    alwan_f64 result = alwan_tm30_rf_f64(&tmp, ctx);
    alwan_spd_destroy_f64(&tmp, ctx);
    return (alwan_f32)result;
}

alwan_status alwan_tm30_specification_f32(alwan_tm30_f32 *spec_out, alwan_spd_f32 const *test_spd, alwan_ctx *ctx) {
    alwan_tm30_f64 wide;
    alwan_spd_f64 tmp;
    alwan_status st;
    int i;
    if (!spec_out || !test_spd || !ctx) return ALWAN_E_INVALID;
    if (spd_alloc_f64_from_f32(&tmp, ctx, test_spd) != ALWAN_OK) return ALWAN_E_NOMEM;
    st = alwan_tm30_specification_f64(&wide, &tmp, ctx);
    alwan_spd_destroy_f64(&tmp, ctx);
    if (st != ALWAN_OK) return st;
    spec_out->rf = (alwan_f32)wide.rf;
    spec_out->rg = (alwan_f32)wide.rg;
    for (i = 0; i < ALWAN_TM30_HUE_BINS; i++) {
        spec_out->rfs[i] = (alwan_f32)wide.rfs[i];
        spec_out->rcs[i] = (alwan_f32)wide.rcs[i];
        spec_out->rhs[i] = (alwan_f32)wide.rhs[i];
        spec_out->average_norms[i] = (alwan_f32)wide.average_norms[i];
        spec_out->averages_test[i][0] = (alwan_f32)wide.averages_test[i][0];
        spec_out->averages_test[i][1] = (alwan_f32)wide.averages_test[i][1];
        spec_out->averages_reference[i][0] = (alwan_f32)wide.averages_reference[i][0];
        spec_out->averages_reference[i][1] = (alwan_f32)wide.averages_reference[i][1];
    }
    for (i = 0; i < ALWAN_TM30_SAMPLES; i++) {
        int k;
        spec_out->bins[i] = wide.bins[i];
        spec_out->rs[i] = (alwan_f32)wide.rs[i];
        spec_out->delta_e[i] = (alwan_f32)wide.delta_e[i];
        for (k = 0; k < 3; k++) {
            spec_out->jab_test[i][k] = (alwan_f32)wide.jab_test[i][k];
            spec_out->jab_reference[i][k] = (alwan_f32)wide.jab_reference[i][k];
        }
    }
    spec_out->cct = (alwan_f32)wide.cct;
    spec_out->duv = (alwan_f32)wide.duv;
    return ALWAN_OK;
}

alwan_f32 alwan_cie224_rf_f32(alwan_spd_f32 const *test_spd, alwan_ctx *ctx) {
    if (!test_spd) return -1.0f;
    alwan_spd_f64 tmp;
    if (spd_alloc_f64_from_f32(&tmp, ctx, test_spd) != ALWAN_OK) return -1.0f;
    alwan_f64 result = alwan_cie224_rf_f64(&tmp, ctx);
    alwan_spd_destroy_f64(&tmp, ctx);
    return (alwan_f32)result;
}

alwan_f32 alwan_ssi_calculate_f32(alwan_spd_f32 const *test_spd, alwan_spd_f32 const *reference_spd, alwan_ctx *ctx) {
    if (!test_spd || !reference_spd) return -1.0f;
    alwan_spd_f64 tmp_test, tmp_ref;
    if (spd_alloc_f64_from_f32(&tmp_test, ctx, test_spd) != ALWAN_OK) return -1.0f;
    if (spd_alloc_f64_from_f32(&tmp_ref, ctx, reference_spd) != ALWAN_OK) {
        alwan_spd_destroy_f64(&tmp_test, ctx);
        return -1.0f;
    }
    alwan_f64 result = alwan_ssi_calculate_f64(&tmp_test, &tmp_ref, ctx);
    alwan_spd_destroy_f64(&tmp_test, ctx);
    alwan_spd_destroy_f64(&tmp_ref, ctx);
    return (alwan_f32)result;
}

alwan_f32 alwan_metamerism_index_f32(alwan_spd_f32 const *sample_reflectance, alwan_spd_f32 const *reference_reflectance, alwan_spd_f32 const *reference_illuminant, alwan_spd_f32 const *test_illuminant, alwan_observer_type observer, alwan_ctx *ctx) {
    if (!ctx || !sample_reflectance || !reference_reflectance ||
        !reference_illuminant || !test_illuminant) {
        return -1.0f;
    }

    alwan_spd_f64 a, b, ri, ti;
    if (spd_alloc_f64_from_f32(&a, ctx, sample_reflectance) != ALWAN_OK) return -1.0f;
    if (spd_alloc_f64_from_f32(&b, ctx, reference_reflectance) != ALWAN_OK) {
        alwan_spd_destroy_f64(&a, ctx);
        return -1.0f;
    }
    if (spd_alloc_f64_from_f32(&ri, ctx, reference_illuminant) != ALWAN_OK) {
        alwan_spd_destroy_f64(&a, ctx);
        alwan_spd_destroy_f64(&b, ctx);
        return -1.0f;
    }
    if (spd_alloc_f64_from_f32(&ti, ctx, test_illuminant) != ALWAN_OK) {
        alwan_spd_destroy_f64(&a, ctx);
        alwan_spd_destroy_f64(&b, ctx);
        alwan_spd_destroy_f64(&ri, ctx);
        return -1.0f;
    }

    alwan_f64 result = alwan_metamerism_index_f64(&a, &b, &ri, &ti, observer, ctx);
    alwan_spd_destroy_f64(&a, ctx);
    alwan_spd_destroy_f64(&b, ctx);
    alwan_spd_destroy_f64(&ri, ctx);
    alwan_spd_destroy_f64(&ti, ctx);
    return (alwan_f32)result;
}
