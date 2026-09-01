/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Light Quality & CCT formulas
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 */

#ifndef ALWAN_QUALITY_CORE_H
#define ALWAN_QUALITY_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_quality_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_quality_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */

/* Krystek 1985 rational-polynomial locus coefficients: the GPU-side mirror of
 * the two CSVs the .inc embeds per precision, the same way cam, aces_ff and 14
 * other cores mirror their small coefficient blocks. Six values read at literal
 * indices, so they are not table-registry data; that registry covers arrays
 * reached through a float coordinate or a row index, through the bounds gate.
 * The scan below reads these on every backend. */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR alwan_scalar KRYSTEK_U_COEFFS[6] = {
#include "../data/planckian_locus_krystek_u.csv"
};
ALWAN_CONSTEXPR alwan_scalar KRYSTEK_V_COEFFS[6] = {
#include "../data/planckian_locus_krystek_v.csv"
};
ALWAN_DIAG_POP

/* ================================================================
 * Ohno 2013 CCT from CIE 1960 UCS (u, v)
 *
 * Reference: Ohno (2013) "Practical Use and Calculation of CCT and Duv",
 * LEUKOS 10:1, the triangular solution with the parabolic correction.
 *
 * Locus samples come from the Krystek 1985 rational polynomial already used
 * here. The scan is uniform in MIRED, not kelvin, because the locus is close
 * to uniform there; a kelvin-uniform scan wastes almost every sample above
 * 10000 K and resolves the low end far too coarsely.
 *
 * What this used to be: an initial Hernandez-Andres 1999 estimate, followed by
 * a Planckian locus evaluation whose du and dv were computed and then thrown
 * away with (void) casts, followed by a return of the unrefined estimate. The
 * comment claimed "Robertson's method as fallback, with Ohno's refinement" and
 * neither was present. It was wrong by up to 473 K (27.8%) on the locus: a
 * 1700 K input came back as 1227 K.
 *
 * Accuracy, measured against colour-science Ohno 2013 on 60 locus points
 * (1700-25000 K) plus the same points offset to Duv = +/-0.01:
 *
 *     on the locus    max 47.6 K, mean 8.0 K   (was max 473 K, mean 59.5 K)
 *     1700-5000 K     max  1.2 K, mean 0.5 K
 *     |Duv| = 0.01    mean ~34 K
 *
 * The residual above 5000 K is the Krystek locus approximation, not the solver:
 * Krystek's own deviation from the true Planckian locus is 1.3e-04 in uv at
 * 10000 K, about 35 K measured along the locus. Above 15000 K, where Krystek is
 * out of its published range and its error reaches ~1550 K equivalent, this
 * returns the Hernandez-Andres estimate instead.
 * ================================================================ */
ALWAN_INLINE alwan_scalar alwan_cct_ohno2013_v(alwan_scalar u, alwan_scalar v) {
    /* Hernandez-Andres 1999, via CIE 1931 xy. Kept as the high-temperature leg:
     * see the return at the end. */
    const alwan_scalar den0 = ALWAN_LITERAL(2.0) * u - ALWAN_LITERAL(8.0) * v + ALWAN_LITERAL(4.0);
    const alwan_scalar den  = ALWAN_SELECT(ALWAN_ABS(den0) < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1e-10), den0);
    const alwan_scalar xc = ALWAN_LITERAL(3.0) * u / den;
    const alwan_scalar yc = ALWAN_LITERAL(2.0) * v / den;
    const alwan_scalar nn = (xc - ALWAN_LITERAL(0.3366)) / (yc - ALWAN_LITERAL(0.1735));
    const alwan_scalar cct_ha = ALWAN_LITERAL(-949.86315)
        + ALWAN_LITERAL(6253.80338) * ALWAN_EXP(-nn / ALWAN_LITERAL(0.92159))
        + ALWAN_LITERAL(28.70599)   * ALWAN_EXP(-nn / ALWAN_LITERAL(0.20039))
        + ALWAN_LITERAL(0.00004)    * ALWAN_EXP(-nn / ALWAN_LITERAL(0.07125));

    /* 1000 K to 25000 K, matching the rest of alwan's CCT surface. Krystek is
     * published for 1000-15000 K; above that it is an extrapolation, which is
     * why the high end is sampled densely rather than trusted pointwise. */
    const alwan_scalar mired_min = ALWAN_LITERAL(66.667);   /* 15000 K, Krystek's published limit */
    const alwan_scalar mired_max = ALWAN_LITERAL(1000.0);
    const int steps = 256;

    int best = 1;
    alwan_scalar best_d = ALWAN_LITERAL(-1.0);
    int i;

    for (i = 0; i <= steps; i++) {
        const alwan_scalar m = mired_min + (mired_max - mired_min) * ((alwan_scalar)i / (alwan_scalar)steps);
        const alwan_scalar T_i = ALWAN_LITERAL(1.0e6) / m;
        const alwan_scalar T2 = T_i * T_i;
        const alwan_scalar up = (KRYSTEK_U_COEFFS[0] + KRYSTEK_U_COEFFS[1] * T_i + KRYSTEK_U_COEFFS[2] * T2) /
                      (KRYSTEK_U_COEFFS[3] + KRYSTEK_U_COEFFS[4] * T_i + KRYSTEK_U_COEFFS[5] * T2);
        const alwan_scalar vp = (KRYSTEK_V_COEFFS[0] + KRYSTEK_V_COEFFS[1] * T_i + KRYSTEK_V_COEFFS[2] * T2) /
                      (KRYSTEK_V_COEFFS[3] + KRYSTEK_V_COEFFS[4] * T_i + KRYSTEK_V_COEFFS[5] * T2);
        const alwan_scalar du = u - up;
        const alwan_scalar dv = v - vp;
        const alwan_scalar d  = ALWAN_SQRT(du * du + dv * dv);
        if (best_d < ALWAN_LITERAL(0.0) || d < best_d) {
            best_d = d;
            best = i;
        }
    }

    /* Keep one sample either side available for both solutions. */
    if (best < 1) best = 1;
    if (best > steps - 1) best = steps - 1;

    {
        alwan_scalar Tn[3], Mn[3], un[3], vn[3], dn[3];
        int k;
        for (k = 0; k < 3; k++) {
            const alwan_scalar m = mired_min + (mired_max - mired_min) *
                         ((alwan_scalar)(best - 1 + k) / (alwan_scalar)steps);
            const alwan_scalar T_k = ALWAN_LITERAL(1.0e6) / m;
            const alwan_scalar T2 = T_k * T_k;
            Tn[k] = T_k;
            Mn[k] = m;
            un[k] = (KRYSTEK_U_COEFFS[0] + KRYSTEK_U_COEFFS[1] * T_k + KRYSTEK_U_COEFFS[2] * T2) /
                    (KRYSTEK_U_COEFFS[3] + KRYSTEK_U_COEFFS[4] * T_k + KRYSTEK_U_COEFFS[5] * T2);
            vn[k] = (KRYSTEK_V_COEFFS[0] + KRYSTEK_V_COEFFS[1] * T_k + KRYSTEK_V_COEFFS[2] * T2) /
                    (KRYSTEK_V_COEFFS[3] + KRYSTEK_V_COEFFS[4] * T_k + KRYSTEK_V_COEFFS[5] * T2);
            dn[k] = ALWAN_SQRT((u - un[k]) * (u - un[k]) + (v - vn[k]) * (v - vn[k]));
        }

        {
            /* Triangular solution: project the point onto the chord joining the
             * two outer samples. */
            const alwan_scalar lu = un[2] - un[0];
            const alwan_scalar lv = vn[2] - vn[0];
            const alwan_scalar l  = ALWAN_SQRT(lu * lu + lv * lv);
            alwan_scalar t_tri = Tn[1];
            alwan_scalar duv = dn[1];
            if (l > ALWAN_LITERAL(1e-12)) {
                const alwan_scalar x = (dn[0] * dn[0] - dn[2] * dn[2] + l * l) / (ALWAN_LITERAL(2.0) * l);
                const alwan_scalar s = dn[0] * dn[0] - x * x;
                /* Interpolate in MIRED, not kelvin. The samples are uniform
                 * in mired and T = 1e6/m is strongly nonlinear, so a linear
                 * blend of Tn[0] and Tn[2] is wrong by hundreds of kelvin
                 * wherever T changes fast per step. */
                {
                    const alwan_scalar m_tri = Mn[0] + (Mn[2] - Mn[0]) * (x / l);
                    t_tri = (ALWAN_ABS(m_tri) > ALWAN_LITERAL(1e-9)) ? ALWAN_LITERAL(1.0e6) / m_tri : Tn[1];
                }
                duv = (s > ALWAN_LITERAL(0.0)) ? ALWAN_SQRT(s) : ALWAN_LITERAL(0.0);
            }

            /* Parabolic solution: fit d(T) through the three samples and take
             * the vertex. Ohno uses it once |Duv| reaches 0.002, where the
             * chord approximation starts to bite. */
            {
                const alwan_scalar X = (Mn[2] - Mn[1]) * (Mn[0] - Mn[2]) * (Mn[1] - Mn[0]);
                alwan_scalar t_par = t_tri;
                if (ALWAN_ABS(X) > ALWAN_LITERAL(1e-12)) {
                    const alwan_scalar a = (Mn[0] * (dn[2] - dn[1]) +
                                  Mn[1] * (dn[0] - dn[2]) +
                                  Mn[2] * (dn[1] - dn[0])) / X;
                    const alwan_scalar b = -(Mn[0] * Mn[0] * (dn[2] - dn[1]) +
                                   Mn[1] * Mn[1] * (dn[0] - dn[2]) +
                                   Mn[2] * Mn[2] * (dn[1] - dn[0])) / X;
                    if (ALWAN_ABS(a) > ALWAN_LITERAL(1e-20)) {
                        {
                            const alwan_scalar m_par = -b / (ALWAN_LITERAL(2.0) * a);
                            t_par = (ALWAN_ABS(m_par) > ALWAN_LITERAL(1e-9)) ? ALWAN_LITERAL(1.0e6) / m_par : t_tri;
                        }
                    }
                }
                {
                    const alwan_scalar t_ohno = (ALWAN_ABS(duv) >= ALWAN_LITERAL(0.002)) ? t_par : t_tri;

                    /* Krystek 1985 is published for 1000-15000 K. Above that it
                     * is an extrapolation and its own deviation from the true
                     * Planckian locus reaches 8.1e-04 in uv at 25000 K, which is
                     * about 1550 K measured along the locus -- more than any
                     * solver over that locus can recover. Measured against
                     * colour-science Ohno 2013 on 60 locus points: solving over
                     * Krystek is better below 10000 K (mean 38.6 K against
                     * 82.4 K) and far worse above 15000 K (max 1219 K against
                     * 33.5 K), because Hernandez-Andres is fitted directly to
                     * the true locus and stays good out there.
                     *
                     * So the solve is used where the locus model is valid and
                     * the closed form above it. The switch is at Krystek's
                     * published limit, not a tuned crossover. */
                    return (t_ohno > ALWAN_LITERAL(15000.0)) ? cct_ha : t_ohno;
                }
            }
        }
    }
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_QUALITY_CORE_H */
