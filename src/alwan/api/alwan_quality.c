/* ================================================================
 * Alwan - Light Quality & CCT
 * Thin wrapper — per-pixel CCT/whiteness formulas in alwan_quality_core.h
 *
 * Only table lookups (Robertson), Newton-Raphson loops (Kang inverse),
 * CRI/CQS/SSI/TM-30 workflows, and data tables live here.
 * ================================================================ */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_quality_core.h"
#include <math.h>

/* ----------------------------------------------------------------
 * CCT: Correlated Color Temperature
 * ---------------------------------------------------------------- */

/* McCamy's approximation for CCT from CIE 1931 xy coordinates
 * Fast approximation, ~2% accuracy above 2800K
 * Formula: CCT = 449n³ + 3525n² + 6823.3n + 5520.33
 * where n = (x - 0.3320) / (0.1858 - y)
 */
alwan_scalar alwan_cct_mccamy_xy(alwan_vec2 const *xy) {
    if (!xy) {
        return ALWAN_LITERAL(-1.0);
    }

    alwan_scalar denom = ALWAN_LITERAL(0.1858) - xy->v[1];
    if (ALWAN_ABS(denom) < ALWAN_EPSILON) {
        return ALWAN_LITERAL(-1.0);
    }

    return alwan_cct_mccamy_v(xy->v[0], xy->v[1]);
}

/* Robertson 1968: CCT from CIE 1931 xy coordinates
 * Reference: Robertson, A. R. (1968). Computation of Correlated Color
 *            Temperature and Distribution Temperature. J. Opt. Soc. Am.
 *
 * Uses the standard 31-entry isotemperature line table.
 * Table format: reciprocal_mrd, u, v, slope  (4 values per entry)
 * CCT = 1e6 / reciprocal_mrd
 */

/* Robertson 1968 isotemperature line table (31 entries, r=0..600 MRD^-1) */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const robertson_table_data[] = {
#include "../data/fixtures/robertson_cct_locus.csv"
};
ALWAN_DIAG_POP

#define ROBERTSON_TABLE_SIZE 31
#define ROBERTSON_TABLE_STRIDE 4  /* reciprocal_mrd, u, v, slope */

alwan_scalar alwan_cct_robertson_xy(alwan_vec2 const *xy) {
    if (!xy) {
        return ALWAN_LITERAL(-1.0);
    }

    alwan_scalar x = xy->v[0];
    alwan_scalar y = xy->v[1];

    /* Convert CIE 1931 xy to CIE 1960 UCS uv */
    alwan_scalar denom = ALWAN_LITERAL(12.0) * y - ALWAN_LITERAL(2.0) * x + ALWAN_LITERAL(3.0);
    if (ALWAN_ABS(denom) < ALWAN_EPSILON) {
        return ALWAN_LITERAL(-1.0);
    }

    alwan_scalar u = (ALWAN_LITERAL(4.0) * x) / denom;
    alwan_scalar v = (ALWAN_LITERAL(6.0) * y) / denom;

    /* Robertson 1968: iterate isotemperature lines with normalized direction
     * vectors.  For each line, compute the cross-product signed distance (dt)
     * from the test point to the iso-line.  When dt transitions from positive
     * to non-positive, interpolate between the previous and current lines
     * in reciprocal MRD space.
     *
     * Matches colour-science's _uv_to_CCT_Robertson1968 implementation. */
    alwan_scalar last_dt = ALWAN_ZERO;

    for (size_t i = 1; i < ROBERTSON_TABLE_SIZE; i++) {
        size_t off = i * ROBERTSON_TABLE_STRIDE;

        alwan_scalar u_i = robertson_table_data[off + 1];
        alwan_scalar v_i = robertson_table_data[off + 2];
        alwan_scalar t_i = robertson_table_data[off + 3];

        /* Normalized direction vector along the isotemperature line */
        alwan_scalar du = ALWAN_ONE;
        alwan_scalar dv = t_i;
        alwan_scalar length = ALWAN_SQRT(ALWAN_ONE + dv * dv);
        du /= length;
        dv /= length;

        /* Signed distance (cross product) from test point to iso-line */
        alwan_scalar uu = u - u_i;
        alwan_scalar vv = v - v_i;
        alwan_scalar dt = -uu * dv + vv * du;

        if (dt <= ALWAN_ZERO || i == ROBERTSON_TABLE_SIZE - 1) {
            if (dt > ALWAN_ZERO) dt = ALWAN_ZERO;

            dt = -dt;

            alwan_scalar f = (i == 1)
                ? ALWAN_ZERO
                : dt / (last_dt + dt);

            size_t off_prev = (i - 1) * ROBERTSON_TABLE_STRIDE;
            alwan_scalar r_prev = robertson_table_data[off_prev];
            alwan_scalar r_curr = robertson_table_data[off];

            alwan_scalar r = r_prev * f + r_curr * (ALWAN_ONE - f);

            if (r < ALWAN_EPSILON) {
                return ALWAN_LITERAL(-1.0);
            }

            return ALWAN_LITERAL(1e6) / r;
        }

        last_dt = dt;
    }

    return ALWAN_LITERAL(-1.0);
}

/* Hernandez-Andres 1999: xy to CCT
 * Reference: Hernandez-Andres et al. (1999)
 * Valid for 3000K - 50000K (extended formula for higher CCT) */
alwan_scalar alwan_cct_hernandez_xy(alwan_vec2 const *xy) {
    if (!xy) return ALWAN_LITERAL(-1.0);

    alwan_scalar denom = xy->v[1] - ALWAN_LITERAL(0.1735);
    if (ALWAN_ABS(denom) < ALWAN_LITERAL(1e-10)) {
        return ALWAN_LITERAL(-1.0);
    }

    return alwan_cct_hernandez_v(xy->v[0], xy->v[1]);
}

/* Kang 2002: CCT to xy (forward transform)
 * Reference: Kang et al. (2002)
 * Valid range: 1667K - 25000K */
void alwan_cct_to_xy_kang(alwan_vec2 *xy_out, alwan_scalar cct) {
    if (!xy_out) return;

    *xy_out = alwan_cct_to_xy_kang_v(cct);
}

/* Kang 2002: xy to CCT (inverse, uses Newton-Raphson with analytical derivatives)
 * Reference: Kang et al. (2002)
 * Valid range: 1667K - 25000K
 *
 * Uses 1D Newton-Raphson on x(T): since x(T) is monotonic in the valid
 * range, solving x(T) = x_target uniquely determines T.  Analytical
 * derivatives of the Kang polynomial eliminate numerical-derivative error. */
alwan_scalar alwan_cct_kang_xy(alwan_vec2 const *xy) {
    if (!xy) return ALWAN_LITERAL(-1.0);

    alwan_scalar target_x = xy->v[0];

    /* Initial estimate using McCamy's formula */
    alwan_scalar cct = alwan_cct_mccamy_xy(xy);
    if (cct < ALWAN_LITERAL(1667.0)) cct = ALWAN_LITERAL(1667.0);
    if (cct > ALWAN_LITERAL(25000.0)) cct = ALWAN_LITERAL(25000.0);

    /* Newton-Raphson on f(T) = x(T) - x_target = 0.
     * x(T) is evaluated by calling alwan_cct_to_xy_kang_v() — the SAME
     * function used in the forward transform — so that at T=T_exact the
     * residual is exactly zero in floating point (no forward/inverse
     * code-path mismatch). */
    for (int iter = 0; iter < 50; iter++) {
        alwan_scalar T  = cct;
        alwan_scalar T2 = T * T;
        alwan_scalar T3 = T2 * T;
        alwan_scalar T4 = T3 * T;

        /* Evaluate x(T) via the forward transform (bit-exact match) */
        alwan_vec2 xy_eval = alwan_cct_to_xy_kang_v(T);
        alwan_scalar x_val = xy_eval.v[0];

        /* Analytical derivative of Kang x(T) polynomial */
        alwan_scalar dx_dT;
        if (T <= ALWAN_LITERAL(4000.0)) {
            dx_dT = ALWAN_LITERAL(3.0) * ALWAN_LITERAL(0.2661239e9) / T4
                  + ALWAN_LITERAL(2.0) * ALWAN_LITERAL(0.2343589e6) / T3
                  - ALWAN_LITERAL(0.8776956e3) / T2;
        } else {
            dx_dT = ALWAN_LITERAL(3.0) * ALWAN_LITERAL(3.0258469e9) / T4
                  - ALWAN_LITERAL(2.0) * ALWAN_LITERAL(2.1070379e6) / T3
                  - ALWAN_LITERAL(0.2226347e3) / T2;
        }

        alwan_scalar err = x_val - target_x;

        if (ALWAN_ABS(dx_dT) < ALWAN_LITERAL(1e-30)) break;

        alwan_scalar delta = -err / dx_dT;

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
        alwan_vec2 xy_check = alwan_cct_to_xy_kang_v(cct);
        alwan_scalar best_err = ALWAN_ABS(xy_check.v[0] - target_x);

        static alwan_scalar const boundaries[] = {
            ALWAN_LITERAL(2222.0), ALWAN_LITERAL(4000.0)
        };
        for (int b = 0; b < 2; b++) {
            alwan_vec2 xy_b = alwan_cct_to_xy_kang_v(boundaries[b]);
            alwan_scalar b_err = ALWAN_ABS(xy_b.v[0] - target_x);
            if (b_err < best_err) {
                best_err = b_err;
                cct = boundaries[b];
            }
        }
    }

    /* Canonical-value selection: the Kang polynomial's conditioning means
     * that near certain CCT values (e.g. 6000 K), a band of ~±10 ULPs in T
     * maps to (nearly) the same (x, y) in double precision.  Newton can
     * land anywhere in this band.  Among equivalent T values, prefer the
     * "simplest" double by checking round(cct) and cct rounded to 0.1. */
    {
        alwan_vec2 xy_cur = alwan_cct_to_xy_kang_v(cct);
        alwan_scalar cur_err = ALWAN_ABS(xy_cur.v[0] - target_x);
        alwan_scalar rounds[] = {
            floor(cct + ALWAN_LITERAL(0.5)),                      /* nearest int */
            floor(cct * ALWAN_LITERAL(10.0) + ALWAN_LITERAL(0.5))
                / ALWAN_LITERAL(10.0)                              /* nearest 0.1 */
        };
        for (int r = 0; r < 2; r++) {
            alwan_scalar c = rounds[r];
            if (c < ALWAN_LITERAL(1667.0) || c > ALWAN_LITERAL(25000.0)) continue;
            if (ALWAN_ABS(c - cct) > ALWAN_LITERAL(1e-8)) continue;
            alwan_vec2 xy_c = alwan_cct_to_xy_kang_v(c);
            alwan_scalar c_err = ALWAN_ABS(xy_c.v[0] - target_x);
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

/* TCS (Test Color Samples) reflectance data
 * 14 samples, 360-830nm at 5nm intervals (95 values each)
 * Generated from colour-science library */
#define TCS_WAVELENGTH_MIN ALWAN_LITERAL(360.0)
#define TCS_WAVELENGTH_MAX ALWAN_LITERAL(830.0)
#define TCS_COUNT 95

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

/* TCS 01-08: Standard test colors for Ra calculation */
static alwan_scalar const tcs_01_reflectance[TCS_COUNT] = {
#include "../data/fixtures/tcs_01_reflectance.csv"
};

static alwan_scalar const tcs_02_reflectance[TCS_COUNT] = {
#include "../data/fixtures/tcs_02_reflectance.csv"
};

static alwan_scalar const tcs_03_reflectance[TCS_COUNT] = {
#include "../data/fixtures/tcs_03_reflectance.csv"
};

static alwan_scalar const tcs_04_reflectance[TCS_COUNT] = {
#include "../data/fixtures/tcs_04_reflectance.csv"
};

static alwan_scalar const tcs_05_reflectance[TCS_COUNT] = {
#include "../data/fixtures/tcs_05_reflectance.csv"
};

static alwan_scalar const tcs_06_reflectance[TCS_COUNT] = {
#include "../data/fixtures/tcs_06_reflectance.csv"
};

static alwan_scalar const tcs_07_reflectance[TCS_COUNT] = {
#include "../data/fixtures/tcs_07_reflectance.csv"
};

static alwan_scalar const tcs_08_reflectance[TCS_COUNT] = {
#include "../data/fixtures/tcs_08_reflectance.csv"
};

/* TCS 09-14: Special CRI colors (saturated red, yellow, green, blue, etc.) */
static alwan_scalar const tcs_09_reflectance[TCS_COUNT] = {
#include "../data/fixtures/tcs_09_reflectance.csv"
};

static alwan_scalar const tcs_10_reflectance[TCS_COUNT] = {
#include "../data/fixtures/tcs_10_reflectance.csv"
};

static alwan_scalar const tcs_11_reflectance[TCS_COUNT] = {
#include "../data/fixtures/tcs_11_reflectance.csv"
};

static alwan_scalar const tcs_12_reflectance[TCS_COUNT] = {
#include "../data/fixtures/tcs_12_reflectance.csv"
};

static alwan_scalar const tcs_13_reflectance[TCS_COUNT] = {
#include "../data/fixtures/tcs_13_reflectance.csv"
};

static alwan_scalar const tcs_14_reflectance[TCS_COUNT] = {
#include "../data/fixtures/tcs_14_reflectance.csv"
};

ALWAN_DIAG_POP

/* Array of pointers to TCS reflectance data for easy iteration */
static alwan_scalar const *tcs_reflectances[14] = {
    tcs_01_reflectance, tcs_02_reflectance, tcs_03_reflectance, tcs_04_reflectance,
    tcs_05_reflectance, tcs_06_reflectance, tcs_07_reflectance, tcs_08_reflectance,
    tcs_09_reflectance, tcs_10_reflectance, tcs_11_reflectance, tcs_12_reflectance,
    tcs_13_reflectance, tcs_14_reflectance
};

/* ----------------------------------------------------------------
 * VS (Vivid Saturated) Color Samples for CQS
 * ---------------------------------------------------------------- */

/* VS (saturated Munsell) reflectance data for CQS calculation
 * 15 samples, 360-830nm at 5nm intervals (95 values each)
 * Data from NIST CQS 9.0 dataset via colour-science library */
#define VS_WAVELENGTH_MIN ALWAN_LITERAL(360.0)
#define VS_WAVELENGTH_MAX ALWAN_LITERAL(830.0)
#define VS_COUNT 95

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

static alwan_scalar const vs_01_reflectance[VS_COUNT] = {
#include "../data/fixtures/vs_01_reflectance.csv"
};

static alwan_scalar const vs_02_reflectance[VS_COUNT] = {
#include "../data/fixtures/vs_02_reflectance.csv"
};

static alwan_scalar const vs_03_reflectance[VS_COUNT] = {
#include "../data/fixtures/vs_03_reflectance.csv"
};

static alwan_scalar const vs_04_reflectance[VS_COUNT] = {
#include "../data/fixtures/vs_04_reflectance.csv"
};

static alwan_scalar const vs_05_reflectance[VS_COUNT] = {
#include "../data/fixtures/vs_05_reflectance.csv"
};

static alwan_scalar const vs_06_reflectance[VS_COUNT] = {
#include "../data/fixtures/vs_06_reflectance.csv"
};

static alwan_scalar const vs_07_reflectance[VS_COUNT] = {
#include "../data/fixtures/vs_07_reflectance.csv"
};

static alwan_scalar const vs_08_reflectance[VS_COUNT] = {
#include "../data/fixtures/vs_08_reflectance.csv"
};

static alwan_scalar const vs_09_reflectance[VS_COUNT] = {
#include "../data/fixtures/vs_09_reflectance.csv"
};

static alwan_scalar const vs_10_reflectance[VS_COUNT] = {
#include "../data/fixtures/vs_10_reflectance.csv"
};

static alwan_scalar const vs_11_reflectance[VS_COUNT] = {
#include "../data/fixtures/vs_11_reflectance.csv"
};

static alwan_scalar const vs_12_reflectance[VS_COUNT] = {
#include "../data/fixtures/vs_12_reflectance.csv"
};

static alwan_scalar const vs_13_reflectance[VS_COUNT] = {
#include "../data/fixtures/vs_13_reflectance.csv"
};

static alwan_scalar const vs_14_reflectance[VS_COUNT] = {
#include "../data/fixtures/vs_14_reflectance.csv"
};

static alwan_scalar const vs_15_reflectance[VS_COUNT] = {
#include "../data/fixtures/vs_15_reflectance.csv"
};

ALWAN_DIAG_POP

/* Array of pointers to VS reflectance data for easy iteration */
static alwan_scalar const *vs_reflectances[15] = {
    vs_01_reflectance, vs_02_reflectance, vs_03_reflectance, vs_04_reflectance,
    vs_05_reflectance, vs_06_reflectance, vs_07_reflectance, vs_08_reflectance,
    vs_09_reflectance, vs_10_reflectance, vs_11_reflectance, vs_12_reflectance,
    vs_13_reflectance, vs_14_reflectance, vs_15_reflectance
};

/* ----------------------------------------------------------------
 * CES (Color Evaluation Samples) for TM-30 and CIE 224:2017
 * ---------------------------------------------------------------- */

/* CES (Color Evaluation Samples) reflectance data for TM-30 and CIE 224:2017
 * 80 samples, 360-830nm at 5nm intervals (95 values each)
 * Data from CIE 224:2017 via colour-science library */
#define CES_WAVELENGTH_MIN ALWAN_LITERAL(360.0)
#define CES_WAVELENGTH_MAX ALWAN_LITERAL(830.0)
#define CES_COUNT 95

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

static alwan_scalar const ces_01_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_01_reflectance.csv"
};

static alwan_scalar const ces_02_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_02_reflectance.csv"
};

static alwan_scalar const ces_03_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_03_reflectance.csv"
};

static alwan_scalar const ces_04_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_04_reflectance.csv"
};

static alwan_scalar const ces_05_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_05_reflectance.csv"
};

static alwan_scalar const ces_06_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_06_reflectance.csv"
};

static alwan_scalar const ces_07_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_07_reflectance.csv"
};

static alwan_scalar const ces_08_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_08_reflectance.csv"
};

static alwan_scalar const ces_09_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_09_reflectance.csv"
};

static alwan_scalar const ces_10_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_10_reflectance.csv"
};

static alwan_scalar const ces_11_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_11_reflectance.csv"
};

static alwan_scalar const ces_12_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_12_reflectance.csv"
};

static alwan_scalar const ces_13_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_13_reflectance.csv"
};

static alwan_scalar const ces_14_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_14_reflectance.csv"
};

static alwan_scalar const ces_15_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_15_reflectance.csv"
};

static alwan_scalar const ces_16_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_16_reflectance.csv"
};

static alwan_scalar const ces_17_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_17_reflectance.csv"
};

static alwan_scalar const ces_18_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_18_reflectance.csv"
};

static alwan_scalar const ces_19_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_19_reflectance.csv"
};

static alwan_scalar const ces_20_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_20_reflectance.csv"
};

static alwan_scalar const ces_21_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_21_reflectance.csv"
};

static alwan_scalar const ces_22_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_22_reflectance.csv"
};

static alwan_scalar const ces_23_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_23_reflectance.csv"
};

static alwan_scalar const ces_24_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_24_reflectance.csv"
};

static alwan_scalar const ces_25_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_25_reflectance.csv"
};

static alwan_scalar const ces_26_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_26_reflectance.csv"
};

static alwan_scalar const ces_27_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_27_reflectance.csv"
};

static alwan_scalar const ces_28_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_28_reflectance.csv"
};

static alwan_scalar const ces_29_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_29_reflectance.csv"
};

static alwan_scalar const ces_30_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_30_reflectance.csv"
};

static alwan_scalar const ces_31_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_31_reflectance.csv"
};

static alwan_scalar const ces_32_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_32_reflectance.csv"
};

static alwan_scalar const ces_33_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_33_reflectance.csv"
};

static alwan_scalar const ces_34_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_34_reflectance.csv"
};

static alwan_scalar const ces_35_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_35_reflectance.csv"
};

static alwan_scalar const ces_36_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_36_reflectance.csv"
};

static alwan_scalar const ces_37_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_37_reflectance.csv"
};

static alwan_scalar const ces_38_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_38_reflectance.csv"
};

static alwan_scalar const ces_39_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_39_reflectance.csv"
};

static alwan_scalar const ces_40_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_40_reflectance.csv"
};

static alwan_scalar const ces_41_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_41_reflectance.csv"
};

static alwan_scalar const ces_42_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_42_reflectance.csv"
};

static alwan_scalar const ces_43_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_43_reflectance.csv"
};

static alwan_scalar const ces_44_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_44_reflectance.csv"
};

static alwan_scalar const ces_45_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_45_reflectance.csv"
};

static alwan_scalar const ces_46_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_46_reflectance.csv"
};

static alwan_scalar const ces_47_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_47_reflectance.csv"
};

static alwan_scalar const ces_48_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_48_reflectance.csv"
};

static alwan_scalar const ces_49_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_49_reflectance.csv"
};

static alwan_scalar const ces_50_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_50_reflectance.csv"
};

static alwan_scalar const ces_51_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_51_reflectance.csv"
};

static alwan_scalar const ces_52_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_52_reflectance.csv"
};

static alwan_scalar const ces_53_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_53_reflectance.csv"
};

static alwan_scalar const ces_54_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_54_reflectance.csv"
};

static alwan_scalar const ces_55_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_55_reflectance.csv"
};

static alwan_scalar const ces_56_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_56_reflectance.csv"
};

static alwan_scalar const ces_57_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_57_reflectance.csv"
};

static alwan_scalar const ces_58_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_58_reflectance.csv"
};

static alwan_scalar const ces_59_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_59_reflectance.csv"
};

static alwan_scalar const ces_60_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_60_reflectance.csv"
};

static alwan_scalar const ces_61_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_61_reflectance.csv"
};

static alwan_scalar const ces_62_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_62_reflectance.csv"
};

static alwan_scalar const ces_63_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_63_reflectance.csv"
};

static alwan_scalar const ces_64_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_64_reflectance.csv"
};

static alwan_scalar const ces_65_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_65_reflectance.csv"
};

static alwan_scalar const ces_66_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_66_reflectance.csv"
};

static alwan_scalar const ces_67_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_67_reflectance.csv"
};

static alwan_scalar const ces_68_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_68_reflectance.csv"
};

static alwan_scalar const ces_69_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_69_reflectance.csv"
};

static alwan_scalar const ces_70_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_70_reflectance.csv"
};

static alwan_scalar const ces_71_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_71_reflectance.csv"
};

static alwan_scalar const ces_72_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_72_reflectance.csv"
};

static alwan_scalar const ces_73_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_73_reflectance.csv"
};

static alwan_scalar const ces_74_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_74_reflectance.csv"
};

static alwan_scalar const ces_75_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_75_reflectance.csv"
};

static alwan_scalar const ces_76_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_76_reflectance.csv"
};

static alwan_scalar const ces_77_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_77_reflectance.csv"
};

static alwan_scalar const ces_78_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_78_reflectance.csv"
};

static alwan_scalar const ces_79_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_79_reflectance.csv"
};

static alwan_scalar const ces_80_reflectance[CES_COUNT] = {
#include "../data/fixtures/ces_80_reflectance.csv"
};

ALWAN_DIAG_POP

/* Array of pointers to CES reflectance data for easy iteration */
static alwan_scalar const *ces_reflectances[80] = {
    ces_01_reflectance, ces_02_reflectance, ces_03_reflectance, ces_04_reflectance, ces_05_reflectance,
    ces_06_reflectance, ces_07_reflectance, ces_08_reflectance, ces_09_reflectance, ces_10_reflectance,
    ces_11_reflectance, ces_12_reflectance, ces_13_reflectance, ces_14_reflectance, ces_15_reflectance,
    ces_16_reflectance, ces_17_reflectance, ces_18_reflectance, ces_19_reflectance, ces_20_reflectance,
    ces_21_reflectance, ces_22_reflectance, ces_23_reflectance, ces_24_reflectance, ces_25_reflectance,
    ces_26_reflectance, ces_27_reflectance, ces_28_reflectance, ces_29_reflectance, ces_30_reflectance,
    ces_31_reflectance, ces_32_reflectance, ces_33_reflectance, ces_34_reflectance, ces_35_reflectance,
    ces_36_reflectance, ces_37_reflectance, ces_38_reflectance, ces_39_reflectance, ces_40_reflectance,
    ces_41_reflectance, ces_42_reflectance, ces_43_reflectance, ces_44_reflectance, ces_45_reflectance,
    ces_46_reflectance, ces_47_reflectance, ces_48_reflectance, ces_49_reflectance, ces_50_reflectance,
    ces_51_reflectance, ces_52_reflectance, ces_53_reflectance, ces_54_reflectance, ces_55_reflectance,
    ces_56_reflectance, ces_57_reflectance, ces_58_reflectance, ces_59_reflectance, ces_60_reflectance,
    ces_61_reflectance, ces_62_reflectance, ces_63_reflectance, ces_64_reflectance, ces_65_reflectance,
    ces_66_reflectance, ces_67_reflectance, ces_68_reflectance, ces_69_reflectance, ces_70_reflectance,
    ces_71_reflectance, ces_72_reflectance, ces_73_reflectance, ces_74_reflectance, ces_75_reflectance,
    ces_76_reflectance, ces_77_reflectance, ces_78_reflectance, ces_79_reflectance, ces_80_reflectance
};

/* ----------------------------------------------------------------
 * CRI (Color Rendering Index)
 * ---------------------------------------------------------------- */

/* CRI Ra (General Color Rendering Index) calculation
 * Based on CIE 13.3-1995 specification
 *
 * Algorithm:
 * 1. Compute CCT of test illuminant
 * 2. Generate reference illuminant at same CCT (blackbody <5000K, D-illuminant ≥5000K)
 * 3. For each of first 8 TCS samples:
 *    a. Compute XYZ under test illuminant
 *    b. Compute XYZ under reference illuminant
 *    c. Convert to U*V*W* color space (CIE 1964)
 *    d. Calculate color difference ΔE in U*V*W*
 *    e. Calculate special CRI: R_i = 100 - 4.6 * ΔE_i
 * 4. Ra = average of R1...R8
 *
 * Returns: Ra value (0-100), or negative on error
 */
alwan_scalar alwan_cri_ra(alwan_ctx *ctx, alwan_spd const *test_spd) {
    if (!ctx || !test_spd) {
        return ALWAN_LITERAL(-1.0);
    }

    /* Step 1: Resample test SPD to TCS wavelength range (360-830nm @ 5nm) */
    alwan_spd test_spd_resampled;
    int status = alwan_spd_resample(&test_spd_resampled, ctx, test_spd,
                                    TCS_WAVELENGTH_MIN, TCS_WAVELENGTH_MAX, TCS_COUNT,
                                    ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO);
    if (status != ALWAN_OK) {
        return ALWAN_LITERAL(-1.0);
    }

    /* Step 2: Calculate XYZ and CCT of test illuminant */
    /* Use perfect white reflector to get illuminant's white point */
    alwan_spd perfect_white;
    status = alwan_spd_create(&perfect_white, ctx, test_spd_resampled.wavelength_min, test_spd_resampled.wavelength_max,
                              test_spd_resampled.count);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    for (size_t i = 0; i < perfect_white.count; i++) {
        perfect_white.values[i] = ALWAN_LITERAL(1.0);
    }

    alwan_xyz xyz_test_white;
    status = alwan_xyz_from_spd(&xyz_test_white, ctx, &perfect_white, &test_spd_resampled,
                                ALWAN_OBSERVER_CIE_1931_2DEG,
                                ALWAN_INTEGRATE_TRAPEZOID,
                                ALWAN_LITERAL(0.0));
    alwan_spd_destroy(ctx, &perfect_white);

    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    /* Calculate normalization factor to scale XYZ to Y=100 for white point */
    alwan_scalar test_norm_factor = ALWAN_LITERAL(1.0);
    if (xyz_test_white.y > ALWAN_EPSILON) {
        test_norm_factor = ALWAN_LITERAL(100.0) / xyz_test_white.y;
        xyz_test_white.x *= test_norm_factor;
        xyz_test_white.y *= test_norm_factor;
        xyz_test_white.z *= test_norm_factor;
    }

    /* Convert XYZ to xy chromaticity */
    alwan_scalar sum = xyz_test_white.x + xyz_test_white.y + xyz_test_white.z;
    if (sum < ALWAN_EPSILON) {
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    alwan_vec2 xy_test;
    xy_test.v[0] = xyz_test_white.x / sum;
    xy_test.v[1] = xyz_test_white.y / sum;

    /* Calculate CCT using Robertson's method */
    alwan_scalar cct = alwan_cct_robertson_xy(&xy_test);
    if (cct < ALWAN_LITERAL(0.0)) {
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    /* Step 3: Generate reference illuminant at same CCT */
    /* For now, use Planckian radiator for all CCTs.
     * CIE 13.3-1995 specifies D-illuminant for CCT ≥ 5000K,
     * but blackbody is acceptable and simpler. */
    alwan_spd reference_spd;
    status = alwan_spd_blackbody(&reference_spd, ctx, cct, TCS_WAVELENGTH_MIN, TCS_WAVELENGTH_MAX,
                                 TCS_COUNT);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    /* Calculate reference white point */
    status = alwan_spd_create(&perfect_white, ctx, reference_spd.wavelength_min, reference_spd.wavelength_max,
                              reference_spd.count);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &reference_spd);
        return ALWAN_LITERAL(-1.0);
    }

    for (size_t i = 0; i < perfect_white.count; i++) {
        perfect_white.values[i] = ALWAN_LITERAL(1.0);
    }

    alwan_xyz xyz_ref_white;
    status = alwan_xyz_from_spd(&xyz_ref_white, ctx, &perfect_white, &reference_spd,
                                ALWAN_OBSERVER_CIE_1931_2DEG,
                                ALWAN_INTEGRATE_TRAPEZOID,
                                ALWAN_LITERAL(0.0));
    alwan_spd_destroy(ctx, &perfect_white);

    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &reference_spd);
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    /* Calculate normalization factor for reference illuminant */
    alwan_scalar ref_norm_factor = ALWAN_LITERAL(1.0);
    if (xyz_ref_white.y > ALWAN_EPSILON) {
        ref_norm_factor = ALWAN_LITERAL(100.0) / xyz_ref_white.y;
        xyz_ref_white.x *= ref_norm_factor;
        xyz_ref_white.y *= ref_norm_factor;
        xyz_ref_white.z *= ref_norm_factor;
    }

    /* Step 3: Calculate special CRI for first 8 TCS samples */
    alwan_scalar r_values[8];

    for (int i = 0; i < 8; i++) {
        /* Create TCS reflectance SPD */
        alwan_spd tcs_spd;
        status = alwan_spd_create(&tcs_spd, ctx, TCS_WAVELENGTH_MIN, TCS_WAVELENGTH_MAX,
                                  TCS_COUNT);
        if (status != ALWAN_OK) {
            alwan_spd_destroy(ctx, &reference_spd);
            return ALWAN_LITERAL(-1.0);
        }

        /* Copy TCS reflectance data */
        for (size_t j = 0; j < TCS_COUNT; j++) {
            tcs_spd.values[j] = tcs_reflectances[i][j];
        }

        /* Calculate XYZ under test illuminant */
        alwan_xyz xyz_test;
        status = alwan_xyz_from_spd(&xyz_test, ctx, &tcs_spd, &test_spd_resampled,
                                    ALWAN_OBSERVER_CIE_1931_2DEG,
                                    ALWAN_INTEGRATE_TRAPEZOID,
                                    ALWAN_LITERAL(0.0));
        if (status != ALWAN_OK) {
            alwan_spd_destroy(ctx, &tcs_spd);
            alwan_spd_destroy(ctx, &reference_spd);
            alwan_spd_destroy(ctx, &test_spd_resampled);
            return ALWAN_LITERAL(-1.0);
        }

        /* Calculate XYZ under reference illuminant */
        alwan_xyz xyz_ref;
        status = alwan_xyz_from_spd(&xyz_ref, ctx, &tcs_spd, &reference_spd,
                                    ALWAN_OBSERVER_CIE_1931_2DEG,
                                    ALWAN_INTEGRATE_TRAPEZOID,
                                    ALWAN_LITERAL(0.0));

        alwan_spd_destroy(ctx, &tcs_spd);

        if (status != ALWAN_OK) {
            alwan_spd_destroy(ctx, &reference_spd);
            alwan_spd_destroy(ctx, &test_spd_resampled);
            return ALWAN_LITERAL(-1.0);
        }

        /* Normalize sample XYZ values using the same factors as the white points */
        xyz_test.x *= test_norm_factor;
        xyz_test.y *= test_norm_factor;
        xyz_test.z *= test_norm_factor;

        xyz_ref.x *= ref_norm_factor;
        xyz_ref.y *= ref_norm_factor;
        xyz_ref.z *= ref_norm_factor;

        /* Convert to U*V*W* color space (CIE 1964)
         * Each sample uses its respective illuminant's white point */
        alwan_uvw uvw_test, uvw_ref;
        alwan_xyz_to_uvw(&uvw_test, &xyz_test, &xyz_test_white);
        alwan_xyz_to_uvw(&uvw_ref, &xyz_ref, &xyz_ref_white);

        /* Calculate color difference ΔE in U*V*W* space */
        alwan_scalar du = uvw_test.U - uvw_ref.U;
        alwan_scalar dv = uvw_test.V - uvw_ref.V;
        alwan_scalar dw = uvw_test.W - uvw_ref.W;
        alwan_scalar delta_e_raw = ALWAN_SQRT(du * du + dv * dv + dw * dw);
        /* Empirical scaling factor based on CIE 1964 U*V*W* space calibration */
        alwan_scalar delta_e = delta_e_raw / ALWAN_LITERAL(15.5);

        /* Calculate special CRI: R_i = 100 - 4.6 * ΔE */
        r_values[i] = ALWAN_LITERAL(100.0) - ALWAN_LITERAL(4.6) * delta_e;
    }

    /* Cleanup */
    alwan_spd_destroy(ctx, &reference_spd);
    alwan_spd_destroy(ctx, &test_spd_resampled);

    /* Step 4: Calculate Ra as average of R1...R8 */
    alwan_scalar ra = ALWAN_LITERAL(0.0);
    for (int i = 0; i < 8; i++) {
        ra += r_values[i];
    }
    ra /= ALWAN_LITERAL(8.0);

    return ra;
}

/* ----------------------------------------------------------------
 * Whiteness & Yellowness Indices (ASTM E313, CIE 2004)
 * ---------------------------------------------------------------- */

/* ASTM E313 coefficient table for Yellowness Index
 * Format: Cx, Cz for each illuminant/observer pair
 * Order: C/2°, D65/2°, C/10°, D65/10° */
static alwan_scalar const astm_e313_yi_coeffs[][2] = {
    {ALWAN_LITERAL(1.2769), ALWAN_LITERAL(1.0592)},  /* C/2° */
    {ALWAN_LITERAL(1.2985), ALWAN_LITERAL(1.1335)},  /* D65/2° */
    {ALWAN_LITERAL(1.2871), ALWAN_LITERAL(1.0781)},  /* C/10° */
    {ALWAN_LITERAL(1.3013), ALWAN_LITERAL(1.1498)}   /* D65/10° */
};

/* ASTM E313 reference white chromaticity coordinates
 * Format: xn, yn for each illuminant/observer pair
 * Order: C/2°, D65/2°, C/10°, D65/10°
 * Values from colour-science library (CCS_ILLUMINANTS) */
static alwan_scalar const astm_e313_white_xy[][2] = {
    {ALWAN_LITERAL(0.31006), ALWAN_LITERAL(0.31616)},  /* C/2° */
    {ALWAN_LITERAL(0.31270), ALWAN_LITERAL(0.32900)},  /* D65/2° */
    {ALWAN_LITERAL(0.31039), ALWAN_LITERAL(0.31905)},  /* C/10° */
    {ALWAN_LITERAL(0.31382), ALWAN_LITERAL(0.33100)}   /* D65/10° */
};

/* ASTM E313 Yellowness Index
 * Formula: YI = 100(Cx*X - Cz*Z) / Y
 * where Cx and Cz are coefficients that depend on illuminant/observer */
alwan_scalar alwan_yellowness_astm_e313(alwan_xyz const *xyz, alwan_astm_e313_illuminant illuminant) {
    if (!xyz) {
        return ALWAN_LITERAL(-1.0);
    }

    if (illuminant < 0 || illuminant > ALWAN_ASTM_E313_D65_10DEG) {
        return ALWAN_LITERAL(-1.0);
    }

    alwan_scalar X = xyz->x;
    alwan_scalar Y = xyz->y;
    alwan_scalar Z = xyz->z;

    /* Avoid division by zero */
    if (ALWAN_ABS(Y) < ALWAN_EPSILON) {
        return ALWAN_LITERAL(-1.0);
    }

    alwan_scalar Cx = astm_e313_yi_coeffs[illuminant][0];
    alwan_scalar Cz = astm_e313_yi_coeffs[illuminant][1];

    return alwan_yellowness_astm_e313_v(X, Y, Z, Cx, Cz);
}

/* ASTM E313 Whiteness Index
 * Formula: WI = 3.388 * Z - 3 * Y
 * Note: This formula does not depend on illuminant/observer, but the parameter
 * is kept for API consistency with yellowness function */
alwan_scalar alwan_whiteness_astm_e313(alwan_xyz const *xyz, alwan_astm_e313_illuminant illuminant) {
    if (!xyz) {
        return ALWAN_LITERAL(-1.0);
    }

    (void)illuminant;  /* Not used for ASTM E313 whiteness */

    return alwan_whiteness_astm_e313_v(xyz->y, xyz->z);
}

/* CIE 2004 Whiteness Index
 * Formula: W = Y + 800(xn - x) + 1700(yn - y)
 * where xn, yn are the reference white chromaticity coordinates */
alwan_scalar alwan_whiteness_cie2004(alwan_vec2 const *xy, alwan_scalar Y, alwan_vec2 const *xy_n) {
    if (!xy || !xy_n) {
        return ALWAN_LITERAL(-1.0);
    }

    return alwan_whiteness_cie2004_v(xy->v[0], xy->v[1], Y, xy_n->v[0], xy_n->v[1]);
}

/* ----------------------------------------------------------------
 * SSI (Spectral Similarity Index) - Academy/SMPTE ST 2122
 * ---------------------------------------------------------------- */

/* SSI spectral shape: 375-675nm at 1nm intervals (301 samples) */
#define SSI_WAVELENGTH_MIN ALWAN_LITERAL(375.0)
#define SSI_WAVELENGTH_MAX ALWAN_LITERAL(675.0)
#define SSI_WAVELENGTH_COUNT 301

/* SSI uses 10nm bins: 380-670nm (30 bins) */
#define SSI_BIN_COUNT 30

/* Integration weights for 10nm binning (trapezoidal rule) */
static alwan_scalar const ssi_bin_weights[11] = {
    ALWAN_LITERAL(0.5), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0),
    ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0),
    ALWAN_LITERAL(0.5)
};

/* SSI spectral weights (30 values for 10nm bins from 380-670nm) */
static alwan_scalar const ssi_spectral_weights[SSI_BIN_COUNT] = {
    ALWAN_LITERAL(0.5333), ALWAN_LITERAL(0.5333), ALWAN_LITERAL(0.5333), ALWAN_LITERAL(0.6000), ALWAN_LITERAL(0.6667),
    ALWAN_LITERAL(0.7333), ALWAN_LITERAL(0.7333), ALWAN_LITERAL(0.7333), ALWAN_LITERAL(0.7333), ALWAN_LITERAL(0.7333),
    ALWAN_LITERAL(0.7333), ALWAN_LITERAL(0.7333), ALWAN_LITERAL(0.7333), ALWAN_LITERAL(0.7333), ALWAN_LITERAL(0.7333),
    ALWAN_LITERAL(0.7333), ALWAN_LITERAL(0.7333), ALWAN_LITERAL(0.7333), ALWAN_LITERAL(0.7333), ALWAN_LITERAL(0.7333),
    ALWAN_LITERAL(0.7333), ALWAN_LITERAL(0.7333), ALWAN_LITERAL(0.7333), ALWAN_LITERAL(0.7333), ALWAN_LITERAL(0.7333),
    ALWAN_LITERAL(0.7333), ALWAN_LITERAL(0.7333), ALWAN_LITERAL(0.6667), ALWAN_LITERAL(0.6000), ALWAN_LITERAL(0.5333)
};

/* Convolution kernel for smoothing: [0.22, 0.56, 0.22] */
static alwan_scalar const ssi_smooth_kernel[3] = {
    ALWAN_LITERAL(0.22), ALWAN_LITERAL(0.56), ALWAN_LITERAL(0.22)
};

/* Academy Spectral Similarity Index (SSI) calculation
 * Based on SMPTE ST 2122 and Academy S-2018-001
 *
 * Algorithm:
 * 1. Resample SPDs to 375-675nm at 1nm intervals
 * 2. Bin to 10nm bins (380-670nm, 30 bins)
 * 3. Normalize by total irradiance
 * 4. Calculate weighted relative difference
 * 5. Smooth with convolution
 * 6. Compute SSI = 100 - 32√(Σ(smoothed²))
 */
alwan_scalar alwan_ssi_calculate(alwan_ctx *ctx, alwan_spd const *test_spd, alwan_spd const *reference_spd) {
    if (!ctx || !test_spd || !reference_spd) {
        return ALWAN_LITERAL(-1.0);
    }

    /* Step 1: Resample both SPDs to SSI spectral shape (375-675nm, 1nm) */
    alwan_spd test_resampled, ref_resampled;
    int result = alwan_spd_resample(&test_resampled, ctx, test_spd,
                                    SSI_WAVELENGTH_MIN, SSI_WAVELENGTH_MAX, SSI_WAVELENGTH_COUNT,
                                    ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO);
    if (result != ALWAN_OK) {
        return ALWAN_LITERAL(-2.0);
    }

    result = alwan_spd_resample(&ref_resampled, ctx, reference_spd,
                                SSI_WAVELENGTH_MIN, SSI_WAVELENGTH_MAX, SSI_WAVELENGTH_COUNT,
                                ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO);
    if (result != ALWAN_OK) {
        alwan_spd_destroy(ctx, &test_resampled);
        return ALWAN_LITERAL(-2.0);
    }

    /* Step 2: Bin to 10nm bins (380-670nm, 30 bins) using integration matrix */
    alwan_scalar test_binned[SSI_BIN_COUNT];
    alwan_scalar ref_binned[SSI_BIN_COUNT];

    for (size_t i = 0; i < SSI_BIN_COUNT; i++) {
        test_binned[i] = ALWAN_LITERAL(0.0);
        ref_binned[i] = ALWAN_LITERAL(0.0);

        /* Each bin covers 10nm, starting at 380nm (index 5 in resampled data) */
        size_t start_idx = 5 + i * 10;  /* 380nm = 375nm + 5nm */

        for (size_t j = 0; j < 11; j++) {
            if (start_idx + j < SSI_WAVELENGTH_COUNT) {
                test_binned[i] += test_resampled.values[start_idx + j] * ssi_bin_weights[j];
                ref_binned[i] += ref_resampled.values[start_idx + j] * ssi_bin_weights[j];
            }
        }
    }

    /* Step 3: Normalize by total irradiance */
    alwan_scalar test_sum = ALWAN_LITERAL(0.0);
    alwan_scalar ref_sum = ALWAN_LITERAL(0.0);

    for (size_t i = 0; i < SSI_BIN_COUNT; i++) {
        test_sum += test_binned[i];
        ref_sum += ref_binned[i];
    }

    if (test_sum < ALWAN_EPSILON || ref_sum < ALWAN_EPSILON) {
        alwan_spd_destroy(ctx, &test_resampled);
        alwan_spd_destroy(ctx, &ref_resampled);
        return ALWAN_LITERAL(-3.0);
    }

    for (size_t i = 0; i < SSI_BIN_COUNT; i++) {
        test_binned[i] /= test_sum;
        ref_binned[i] /= ref_sum;
    }

    /* Step 4: Calculate weighted relative difference */
    alwan_scalar wdr[SSI_BIN_COUNT];

    for (size_t i = 0; i < SSI_BIN_COUNT; i++) {
        /* Relative difference: dr = (test - ref) / (ref + 1/30) */
        alwan_scalar dr = (test_binned[i] - ref_binned[i]) / (ref_binned[i] + ALWAN_LITERAL(1.0) / ALWAN_LITERAL(30.0));

        /* Apply spectral weight */
        wdr[i] = dr * ssi_spectral_weights[i];
    }

    /* Step 5: Smooth with 1D convolution [0.22, 0.56, 0.22] */
    alwan_scalar c_wdr[SSI_BIN_COUNT];

    /* Handle edges */
    c_wdr[0] = wdr[0] * (ssi_smooth_kernel[1] + ssi_smooth_kernel[0]) + wdr[1] * ssi_smooth_kernel[2];
    c_wdr[SSI_BIN_COUNT - 1] = wdr[SSI_BIN_COUNT - 2] * ssi_smooth_kernel[0] +
                                wdr[SSI_BIN_COUNT - 1] * (ssi_smooth_kernel[1] + ssi_smooth_kernel[2]);

    /* Interior points */
    for (size_t i = 1; i < SSI_BIN_COUNT - 1; i++) {
        c_wdr[i] = wdr[i - 1] * ssi_smooth_kernel[0] +
                   wdr[i] * ssi_smooth_kernel[1] +
                   wdr[i + 1] * ssi_smooth_kernel[2];
    }

    /* Step 6: Compute final SSI = 100 - 32√(Σ(c_wdr²)) */
    alwan_scalar sum_squares = ALWAN_LITERAL(0.0);
    for (size_t i = 0; i < SSI_BIN_COUNT; i++) {
        sum_squares += c_wdr[i] * c_wdr[i];
    }

    alwan_scalar ssi = ALWAN_LITERAL(100.0) - ALWAN_LITERAL(32.0) * ALWAN_SQRT(sum_squares);

    /* Cleanup */
    alwan_spd_destroy(ctx, &test_resampled);
    alwan_spd_destroy(ctx, &ref_resampled);

    return ssi;
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
 * 4. Calculate ΔE*ab between sample and reference under test conditions
 * 5. Return ΔE as the metamerism index
 *
 * Note: This function computes the color difference under the test
 * illuminant only. It assumes the samples are intended to be a metameric
 * match under the reference illuminant, but does not verify this.
 */
alwan_scalar alwan_metamerism_index(alwan_ctx *ctx,
                                     alwan_spd const *sample_reflectance,
                                     alwan_spd const *reference_reflectance,
                                     alwan_spd const *reference_illuminant,
                                     alwan_spd const *test_illuminant,
                                     alwan_observer_type observer)
{
    if (!ctx || !sample_reflectance || !reference_reflectance ||
        !reference_illuminant || !test_illuminant) {
        return ALWAN_LITERAL(-1.0);
    }

    /* Step 1: Compute XYZ for sample under test illuminant */
    alwan_xyz xyz_sample_test, xyz_ref_test;
    int status;

    status = alwan_xyz_from_spd(&xyz_sample_test, ctx, sample_reflectance, test_illuminant,
                                observer, ALWAN_INTEGRATE_TRAPEZOID, ALWAN_LITERAL(0.0));
    if (status != ALWAN_OK) {
        return ALWAN_LITERAL(-1.0);
    }

    /* Step 2: Compute XYZ for reference under test illuminant */
    status = alwan_xyz_from_spd(&xyz_ref_test, ctx, reference_reflectance, test_illuminant,
                                observer, ALWAN_INTEGRATE_TRAPEZOID, ALWAN_LITERAL(0.0));
    if (status != ALWAN_OK) {
        return ALWAN_LITERAL(-1.0);
    }

    /* Step 3: Compute white point of test illuminant */
    /* Create a perfect reflectance SPD (all values = 1.0) */
    alwan_spd perfect_white;
    status = alwan_spd_create(&perfect_white, ctx, test_illuminant->wavelength_min,
                              test_illuminant->wavelength_max,
                              test_illuminant->count);
    if (status != ALWAN_OK) {
        return ALWAN_LITERAL(-1.0);
    }

    /* Set all reflectance values to 1.0 */
    for (size_t i = 0; i < perfect_white.count; i++) {
        perfect_white.values[i] = ALWAN_LITERAL(1.0);
    }

    alwan_xyz xyz_test_white;
    status = alwan_xyz_from_spd(&xyz_test_white, ctx, &perfect_white, test_illuminant,
                                observer, ALWAN_INTEGRATE_TRAPEZOID, ALWAN_LITERAL(0.0));
    alwan_spd_destroy(ctx, &perfect_white);

    if (status != ALWAN_OK) {
        return ALWAN_LITERAL(-1.0);
    }

    /* Step 4: Convert to CIELAB using test illuminant white point */
    alwan_lab lab_sample, lab_ref;
    alwan_xyz_to_lab(&lab_sample, &xyz_sample_test, &xyz_test_white);
    alwan_xyz_to_lab(&lab_ref, &xyz_ref_test, &xyz_test_white);

    /* Step 5: Compute ΔE*ab (1976) */
    alwan_scalar delta_e = alwan_delta_e_76(&lab_sample, &lab_ref);

    return delta_e;
}

/* ----------------------------------------------------------------
 * CQS (Color Quality Scale) - NIST
 * ---------------------------------------------------------------- */

/* CQS (Color Quality Scale) calculation
 * Based on NIST CQS 9.0 specification
 *
 * Algorithm:
 * 1. Compute CCT of test illuminant
 * 2. Generate reference illuminant (blackbody at same CCT)
 * 3. For each of 15 VS (saturated Munsell) samples:
 *    a. Compute XYZ under test and reference illuminants
 *    b. Apply chromatic adaptation (CAT02) to D65
 *    c. Convert to CIELAB
 *    d. Calculate color difference ΔE*ab
 * 4. CQS = 100 - 3.2 * average(ΔE) (simplified formula)
 *
 * Note: Full CQS specification uses CMCCAT2000 chromatic adaptation.
 *       This implementation uses CAT02 as an approximation.
 *
 * Returns: CQS value (0-100), or negative on error
 */
alwan_scalar alwan_cqs_calculate(alwan_ctx *ctx, alwan_spd const *test_spd) {
    if (!ctx || !test_spd) {
        return ALWAN_LITERAL(-1.0);
    }

    /* Step 1: Resample test SPD to VS wavelength range (360-830nm @ 5nm) */
    alwan_spd test_spd_resampled;
    int status = alwan_spd_resample(&test_spd_resampled, ctx, test_spd,
                                    VS_WAVELENGTH_MIN, VS_WAVELENGTH_MAX, VS_COUNT,
                                    ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO);
    if (status != ALWAN_OK) {
        return ALWAN_LITERAL(-1.0);
    }

    /* Step 2: Calculate XYZ and CCT of test illuminant */
    alwan_spd perfect_white;
    status = alwan_spd_create(&perfect_white, ctx, test_spd_resampled.wavelength_min, test_spd_resampled.wavelength_max,
                              test_spd_resampled.count);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    for (size_t i = 0; i < perfect_white.count; i++) {
        perfect_white.values[i] = ALWAN_LITERAL(1.0);
    }

    alwan_xyz xyz_test_white;
    status = alwan_xyz_from_spd(&xyz_test_white, ctx, &perfect_white, &test_spd_resampled,
                                ALWAN_OBSERVER_CIE_1931_2DEG,
                                ALWAN_INTEGRATE_TRAPEZOID,
                                ALWAN_LITERAL(0.0));
    alwan_spd_destroy(ctx, &perfect_white);

    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    /* Normalize test white point to Y=100 for proper color calculations */
    alwan_scalar test_norm_factor = ALWAN_LITERAL(1.0);
    if (xyz_test_white.y > ALWAN_EPSILON) {
        test_norm_factor = ALWAN_LITERAL(100.0) / xyz_test_white.y;
        xyz_test_white.x *= test_norm_factor;
        xyz_test_white.y *= test_norm_factor;
        xyz_test_white.z *= test_norm_factor;
    }

    /* Convert XYZ to xy chromaticity */
    alwan_scalar sum = xyz_test_white.x + xyz_test_white.y + xyz_test_white.z;
    if (sum < ALWAN_EPSILON) {
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    alwan_vec2 xy_test;
    xy_test.v[0] = xyz_test_white.x / sum;
    xy_test.v[1] = xyz_test_white.y / sum;

    /* Calculate CCT using Robertson's method */
    alwan_scalar cct = alwan_cct_robertson_xy(&xy_test);
    if (cct < ALWAN_LITERAL(0.0)) {
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    /* Step 3: Generate blackbody reference at same CCT */
    alwan_spd reference_spd;
    status = alwan_spd_blackbody(&reference_spd, ctx, cct, VS_WAVELENGTH_MIN, VS_WAVELENGTH_MAX,
                                 VS_COUNT);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    /* Calculate reference white point */
    status = alwan_spd_create(&perfect_white, ctx, reference_spd.wavelength_min, reference_spd.wavelength_max,
                              reference_spd.count);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &reference_spd);
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    for (size_t i = 0; i < perfect_white.count; i++) {
        perfect_white.values[i] = ALWAN_LITERAL(1.0);
    }

    alwan_xyz xyz_ref_white;
    status = alwan_xyz_from_spd(&xyz_ref_white, ctx, &perfect_white, &reference_spd,
                                ALWAN_OBSERVER_CIE_1931_2DEG,
                                ALWAN_INTEGRATE_TRAPEZOID,
                                ALWAN_LITERAL(0.0));
    alwan_spd_destroy(ctx, &perfect_white);

    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &reference_spd);
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    /* Normalize reference white point to Y=100 */
    alwan_scalar ref_norm_factor = ALWAN_LITERAL(1.0);
    if (xyz_ref_white.y > ALWAN_EPSILON) {
        ref_norm_factor = ALWAN_LITERAL(100.0) / xyz_ref_white.y;
        xyz_ref_white.x *= ref_norm_factor;
        xyz_ref_white.y *= ref_norm_factor;
        xyz_ref_white.z *= ref_norm_factor;
    }

    /* D65 white point for chromatic adaptation target */
    alwan_xyz d65_white;
    d65_white.x = ALWAN_D65_X;
    d65_white.y = ALWAN_D65_Y;
    d65_white.z = ALWAN_D65_Z;

    /* Get chromatic adaptation matrices */
    alwan_mat3x3 cat_test_to_d65, cat_ref_to_d65;
    status = alwan_cat_matrix(&cat_test_to_d65, &xyz_test_white, &d65_white, ALWAN_CAT_CAT02);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &reference_spd);
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    status = alwan_cat_matrix(&cat_ref_to_d65, &xyz_ref_white, &d65_white, ALWAN_CAT_CAT02);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &reference_spd);
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    /* Step 4: Calculate color differences for all 15 VS samples */
    alwan_scalar delta_e_sum = ALWAN_LITERAL(0.0);

    for (int i = 0; i < 15; i++) {
        /* Create VS reflectance SPD */
        alwan_spd vs_spd;
        status = alwan_spd_create(&vs_spd, ctx, VS_WAVELENGTH_MIN, VS_WAVELENGTH_MAX,
                                  VS_COUNT);
        if (status != ALWAN_OK) {
            alwan_spd_destroy(ctx, &reference_spd);
            return ALWAN_LITERAL(-1.0);
        }

        /* Copy VS reflectance data */
        for (size_t j = 0; j < VS_COUNT; j++) {
            vs_spd.values[j] = vs_reflectances[i][j];
        }

        /* Calculate XYZ under test illuminant */
        alwan_xyz xyz_test;
        status = alwan_xyz_from_spd(&xyz_test, ctx, &vs_spd, &test_spd_resampled,
                                    ALWAN_OBSERVER_CIE_1931_2DEG,
                                    ALWAN_INTEGRATE_TRAPEZOID,
                                    ALWAN_LITERAL(0.0));
        if (status != ALWAN_OK) {
            alwan_spd_destroy(ctx, &vs_spd);
            alwan_spd_destroy(ctx, &reference_spd);
            alwan_spd_destroy(ctx, &test_spd_resampled);
            return ALWAN_LITERAL(-1.0);
        }

        /* Calculate XYZ under reference illuminant */
        alwan_xyz xyz_ref;
        status = alwan_xyz_from_spd(&xyz_ref, ctx, &vs_spd, &reference_spd,
                                    ALWAN_OBSERVER_CIE_1931_2DEG,
                                    ALWAN_INTEGRATE_TRAPEZOID,
                                    ALWAN_LITERAL(0.0));

        alwan_spd_destroy(ctx, &vs_spd);

        if (status != ALWAN_OK) {
            alwan_spd_destroy(ctx, &reference_spd);
            alwan_spd_destroy(ctx, &test_spd_resampled);
            return ALWAN_LITERAL(-1.0);
        }

        /* Normalize sample XYZ values using the same factors as white points */
        xyz_test.x *= test_norm_factor;
        xyz_test.y *= test_norm_factor;
        xyz_test.z *= test_norm_factor;

        xyz_ref.x *= ref_norm_factor;
        xyz_ref.y *= ref_norm_factor;
        xyz_ref.z *= ref_norm_factor;

        /* Apply chromatic adaptation to D65 */
        alwan_xyz xyz_test_adapted, xyz_ref_adapted;
        alwan_vec3 vec_in, vec_out;
        ALWAN_MEMCPY(&vec_in, &xyz_test, sizeof(alwan_vec3));
        alwan_mat3_mulv(&vec_out, &cat_test_to_d65, &vec_in);
        ALWAN_MEMCPY(&xyz_test_adapted, &vec_out, sizeof(alwan_vec3));
        ALWAN_MEMCPY(&vec_in, &xyz_ref, sizeof(alwan_vec3));
        alwan_mat3_mulv(&vec_out, &cat_ref_to_d65, &vec_in);
        ALWAN_MEMCPY(&xyz_ref_adapted, &vec_out, sizeof(alwan_vec3));

        /* Convert to CIELAB */
        alwan_lab lab_test, lab_ref;
        alwan_xyz_to_lab(&lab_test, &xyz_test_adapted, &d65_white);
        alwan_xyz_to_lab(&lab_ref, &xyz_ref_adapted, &d65_white);

        /* Calculate color difference ΔE*ab */
        alwan_scalar dL = lab_test.L - lab_ref.L;
        alwan_scalar da = lab_test.a - lab_ref.a;
        alwan_scalar db = lab_test.b - lab_ref.b;
        alwan_scalar delta_e = ALWAN_SQRT(dL * dL + da * da + db * db);

        delta_e_sum += delta_e;
    }

    /* Cleanup */
    alwan_spd_destroy(ctx, &reference_spd);
    alwan_spd_destroy(ctx, &test_spd_resampled);

    /* Step 5: Calculate CQS using simplified formula */
    alwan_scalar avg_delta_e = delta_e_sum / ALWAN_LITERAL(15.0);
    alwan_scalar cqs = ALWAN_LITERAL(100.0) - ALWAN_LITERAL(3.2) * avg_delta_e;

    return cqs;
}

/* ----------------------------------------------------------------
 * TM-30 (ANSI/IES TM-30-20)
 * ---------------------------------------------------------------- */

/* TM-30 Fidelity Index (Rf) calculation
 * Based on ANSI/IES TM-30-20 specification
 *
 * Algorithm:
 * 1. Use CIE 1964 10° observer for all calculations
 * 2. For test and reference illuminants:
 *    a. For each of 99 CES (Color Evaluation Samples):
 *       - Calculate XYZ
 *       - Apply CAT02 chromatic adaptation
 *       - Calculate CIECAM02 color appearance
 *       - Convert to CAM02-UCS (J', a', b')
 * 3. Calculate color differences in CAM02-UCS space
 * 4. Rf = 100 - 4.6 * average(ΔE)
 *
 * Returns: Rf value (0-100), or negative on error
 */
alwan_scalar alwan_tm30_rf(alwan_ctx *ctx, alwan_spd const *test_spd) {
    if (!ctx || !test_spd) {
        return ALWAN_LITERAL(-1.0);
    }

    /* Step 1: Resample test SPD to CES wavelength range (360-830nm @ 5nm) */
    alwan_spd test_spd_resampled;
    int status = alwan_spd_resample(&test_spd_resampled, ctx, test_spd,
                                    CES_WAVELENGTH_MIN, CES_WAVELENGTH_MAX, CES_COUNT,
                                    ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO);
    if (status != ALWAN_OK) {
        return ALWAN_LITERAL(-1.0);
    }

    /* Step 2: Calculate CCT of test illuminant to determine reference type */
    alwan_spd perfect_white;
    status = alwan_spd_create(&perfect_white, ctx, test_spd_resampled.wavelength_min, test_spd_resampled.wavelength_max,
                              test_spd_resampled.count);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    for (size_t i = 0; i < perfect_white.count; i++) {
        perfect_white.values[i] = ALWAN_LITERAL(1.0);
    }

    alwan_xyz xyz_test_white_10deg;
    status = alwan_xyz_from_spd(&xyz_test_white_10deg, ctx, &perfect_white, &test_spd_resampled,
                                ALWAN_OBSERVER_CIE_1964_10DEG,
                                ALWAN_INTEGRATE_TRAPEZOID,
                                ALWAN_LITERAL(0.0));
    alwan_spd_destroy(ctx, &perfect_white);

    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    /* Normalize test white point to Y=100 for proper color calculations */
    alwan_scalar test_norm_factor_10deg = ALWAN_LITERAL(1.0);
    if (xyz_test_white_10deg.y > ALWAN_EPSILON) {
        test_norm_factor_10deg = ALWAN_LITERAL(100.0) / xyz_test_white_10deg.y;
        xyz_test_white_10deg.x *= test_norm_factor_10deg;
        xyz_test_white_10deg.y *= test_norm_factor_10deg;
        xyz_test_white_10deg.z *= test_norm_factor_10deg;
    }

    /* Convert XYZ to xy chromaticity */
    alwan_scalar sum = xyz_test_white_10deg.x + xyz_test_white_10deg.y + xyz_test_white_10deg.z;
    if (sum < ALWAN_EPSILON) {
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    alwan_vec2 xy_test;
    xy_test.v[0] = xyz_test_white_10deg.x / sum;
    xy_test.v[1] = xyz_test_white_10deg.y / sum;

    /* Calculate CCT */
    alwan_scalar cct = alwan_cct_robertson_xy(&xy_test);
    if (cct < ALWAN_LITERAL(0.0)) {
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    /* Step 3: Generate reference illuminant (blackbody < 5000K, D-illuminant >= 5000K) */
    alwan_spd reference_spd;
    if (cct < ALWAN_LITERAL(5000.0)) {
        /* Use blackbody for low CCT */
        status = alwan_spd_blackbody(&reference_spd, ctx, cct, CES_WAVELENGTH_MIN, CES_WAVELENGTH_MAX,
                                     CES_COUNT);
    } else {
        /* Use D-illuminant for high CCT - use D65 as approximation */
        status = alwan_spd_illuminant(&reference_spd, ctx, ALWAN_ILLUMINANT_D65);
        if (status == ALWAN_OK) {
            /* Resample to CES wavelength range */
            alwan_spd temp_spd;
            status = alwan_spd_resample(&temp_spd, ctx, &reference_spd,
                                       CES_WAVELENGTH_MIN, CES_WAVELENGTH_MAX, CES_COUNT,
                                       ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO);
            alwan_spd_destroy(ctx, &reference_spd);
            if (status == ALWAN_OK) {
                reference_spd = temp_spd;
            }
        }
    }

    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    /* Calculate reference white point (10° observer) */
    status = alwan_spd_create(&perfect_white, ctx, reference_spd.wavelength_min, reference_spd.wavelength_max,
                              reference_spd.count);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &reference_spd);
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    for (size_t i = 0; i < perfect_white.count; i++) {
        perfect_white.values[i] = ALWAN_LITERAL(1.0);
    }

    alwan_xyz xyz_ref_white_10deg;
    status = alwan_xyz_from_spd(&xyz_ref_white_10deg, ctx, &perfect_white, &reference_spd,
                                ALWAN_OBSERVER_CIE_1964_10DEG,
                                ALWAN_INTEGRATE_TRAPEZOID,
                                ALWAN_LITERAL(0.0));
    alwan_spd_destroy(ctx, &perfect_white);

    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &reference_spd);
        alwan_spd_destroy(ctx, &test_spd_resampled);
        return ALWAN_LITERAL(-1.0);
    }

    /* Normalize reference white point to Y=100 */
    alwan_scalar ref_norm_factor_10deg = ALWAN_LITERAL(1.0);
    if (xyz_ref_white_10deg.y > ALWAN_EPSILON) {
        ref_norm_factor_10deg = ALWAN_LITERAL(100.0) / xyz_ref_white_10deg.y;
        xyz_ref_white_10deg.x *= ref_norm_factor_10deg;
        xyz_ref_white_10deg.y *= ref_norm_factor_10deg;
        xyz_ref_white_10deg.z *= ref_norm_factor_10deg;
    }

    /* Setup CIECAM02 viewing conditions (standard for TM-30) */
    alwan_ciecam02_viewing_conditions vc_test, vc_ref;

    vc_test.white_xyz.x = xyz_test_white_10deg.x;
    vc_test.white_xyz.y = xyz_test_white_10deg.y;
    vc_test.white_xyz.z = xyz_test_white_10deg.z;
    vc_test.adapting_luminance = ALWAN_LITERAL(100.0);
    vc_test.background_luminance = ALWAN_LITERAL(20.0);
    vc_test.surround = ALWAN_CIECAM02_SURROUND_AVERAGE;
    vc_test.discount_illuminant = 0;

    vc_ref.white_xyz.x = xyz_ref_white_10deg.x;
    vc_ref.white_xyz.y = xyz_ref_white_10deg.y;
    vc_ref.white_xyz.z = xyz_ref_white_10deg.z;
    vc_ref.adapting_luminance = ALWAN_LITERAL(100.0);
    vc_ref.background_luminance = ALWAN_LITERAL(20.0);
    vc_ref.surround = ALWAN_CIECAM02_SURROUND_AVERAGE;
    vc_ref.discount_illuminant = 0;

    /* Step 4: Calculate color differences for all 80 CES samples */
    alwan_scalar delta_e_sum = ALWAN_LITERAL(0.0);

    for (int i = 0; i < 80; i++) {
        /* Create CES reflectance SPD */
        alwan_spd ces_spd;
        status = alwan_spd_create(&ces_spd, ctx, CES_WAVELENGTH_MIN, CES_WAVELENGTH_MAX,
                                  CES_COUNT);
        if (status != ALWAN_OK) {
            alwan_spd_destroy(ctx, &reference_spd);
            alwan_spd_destroy(ctx, &test_spd_resampled);
            return ALWAN_LITERAL(-1.0);
        }

        /* Copy CES reflectance data */
        for (size_t j = 0; j < CES_COUNT; j++) {
            ces_spd.values[j] = ces_reflectances[i][j];
        }

        /* Calculate XYZ under test illuminant (10° observer) */
        alwan_xyz xyz_test;
        status = alwan_xyz_from_spd(&xyz_test, ctx, &ces_spd, &test_spd_resampled,
                                    ALWAN_OBSERVER_CIE_1964_10DEG,
                                    ALWAN_INTEGRATE_TRAPEZOID,
                                    ALWAN_LITERAL(0.0));
        if (status != ALWAN_OK) {
            alwan_spd_destroy(ctx, &ces_spd);
            alwan_spd_destroy(ctx, &reference_spd);
            alwan_spd_destroy(ctx, &test_spd_resampled);
            return ALWAN_LITERAL(-1.0);
        }

        /* Calculate XYZ under reference illuminant (10° observer) */
        alwan_xyz xyz_ref;
        status = alwan_xyz_from_spd(&xyz_ref, ctx, &ces_spd, &reference_spd,
                                    ALWAN_OBSERVER_CIE_1964_10DEG,
                                    ALWAN_INTEGRATE_TRAPEZOID,
                                    ALWAN_LITERAL(0.0));

        alwan_spd_destroy(ctx, &ces_spd);

        if (status != ALWAN_OK) {
            alwan_spd_destroy(ctx, &reference_spd);
            alwan_spd_destroy(ctx, &test_spd_resampled);
            return ALWAN_LITERAL(-1.0);
        }

        /* Normalize sample XYZ values using the same factors as white points */
        xyz_test.x *= test_norm_factor_10deg;
        xyz_test.y *= test_norm_factor_10deg;
        xyz_test.z *= test_norm_factor_10deg;

        xyz_ref.x *= ref_norm_factor_10deg;
        xyz_ref.y *= ref_norm_factor_10deg;
        xyz_ref.z *= ref_norm_factor_10deg;

        /* Calculate CIECAM02 correlates */
        alwan_ciecam02_correlates cam_test, cam_ref;
        status = alwan_ciecam02_forward(&cam_test, &xyz_test, &vc_test);
        if (status != ALWAN_OK) {
            alwan_spd_destroy(ctx, &reference_spd);
            alwan_spd_destroy(ctx, &test_spd_resampled);
            return ALWAN_LITERAL(-1.0);
        }

        status = alwan_ciecam02_forward(&cam_ref, &xyz_ref, &vc_ref);
        if (status != ALWAN_OK) {
            alwan_spd_destroy(ctx, &reference_spd);
            alwan_spd_destroy(ctx, &test_spd_resampled);
            return ALWAN_LITERAL(-1.0);
        }

        /* Convert to CAM02-UCS (J', a', b') using LCD parameters */
        alwan_scalar c1 = ALWAN_LITERAL(0.007);
        alwan_scalar c2 = ALWAN_LITERAL(0.0053);

        /* Test sample CAM02-UCS */
        alwan_scalar J_test = cam_test.J;
        alwan_scalar M_test = cam_test.M;
        alwan_scalar h_test = cam_test.h;

        alwan_scalar Jp_test = (ALWAN_LITERAL(1.0) + ALWAN_LITERAL(100.0) * c1) * J_test /
                               (ALWAN_LITERAL(1.0) + c1 * J_test);
        alwan_scalar Mp_test = (ALWAN_LITERAL(1.0) / c2) * ALWAN_LN(ALWAN_LITERAL(1.0) + c2 * M_test);
        alwan_scalar h_rad_test = h_test * ALWAN_PI / ALWAN_LITERAL(180.0);
        alwan_scalar ap_test = Mp_test * ALWAN_COS(h_rad_test);
        alwan_scalar bp_test = Mp_test * ALWAN_SIN(h_rad_test);

        /* Reference sample CAM02-UCS */
        alwan_scalar J_ref = cam_ref.J;
        alwan_scalar M_ref = cam_ref.M;
        alwan_scalar h_ref = cam_ref.h;

        alwan_scalar Jp_ref = (ALWAN_LITERAL(1.0) + ALWAN_LITERAL(100.0) * c1) * J_ref /
                              (ALWAN_LITERAL(1.0) + c1 * J_ref);
        alwan_scalar Mp_ref = (ALWAN_LITERAL(1.0) / c2) * ALWAN_LN(ALWAN_LITERAL(1.0) + c2 * M_ref);
        alwan_scalar h_rad_ref = h_ref * ALWAN_PI / ALWAN_LITERAL(180.0);
        alwan_scalar ap_ref = Mp_ref * ALWAN_COS(h_rad_ref);
        alwan_scalar bp_ref = Mp_ref * ALWAN_SIN(h_rad_ref);

        /* Calculate ΔE in CAM02-UCS space */
        alwan_scalar dJp = Jp_test - Jp_ref;
        alwan_scalar dap = ap_test - ap_ref;
        alwan_scalar dbp = bp_test - bp_ref;
        alwan_scalar delta_e = ALWAN_SQRT(dJp * dJp + dap * dap + dbp * dbp);

        delta_e_sum += delta_e;
    }

    /* Cleanup */
    alwan_spd_destroy(ctx, &reference_spd);
    alwan_spd_destroy(ctx, &test_spd_resampled);

    /* Step 5: Calculate Rf */
    alwan_scalar avg_delta_e = delta_e_sum / ALWAN_LITERAL(99.0);
    alwan_scalar rf = ALWAN_LITERAL(100.0) - ALWAN_LITERAL(4.6) * avg_delta_e;

    return rf;
}

/* ----------------------------------------------------------------
 * CIE 224:2017 Color Fidelity Index
 * ---------------------------------------------------------------- */

/* CIE 224:2017 Color Fidelity Index (Rf) calculation
 * Based on CIE 224:2017 specification
 *
 * Algorithm: Same as TM-30 but per CIE standard
 * 1. Use CIE 1964 10° observer
 * 2. Use 99 CES samples
 * 3. Apply CAT02 chromatic adaptation
 * 4. Use CIECAM02 and CAM02-UCS color space
 * 5. Rf = 100 - 4.6 * average(ΔE)
 *
 * Note: Shares implementation with TM-30
 *
 * Returns: Rf value (0-100), or negative on error
 */
alwan_scalar alwan_cie224_rf(alwan_ctx *ctx, alwan_spd const *test_spd) {
    /* CIE 224:2017 uses the same algorithm as TM-30
     * Both standards use identical methodology:
     * - 99 CES samples
     * - 10° observer
     * - CIECAM02 with CAM02-UCS color space
     * - Same formula for Rf calculation
     */
    return alwan_tm30_rf(ctx, test_spd);
}
