/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Physical skies: Preetham 1999, Hosek-Wilkie 2012/2013 and Bruneton 2017.
 *
 * Hosek-Wilkie: the cooking of the model state and the solar radiance function are a
 * port of ArHosekSkyModel.c 1.4a, whose notice is:
 *
 *   This source is published under the following 3-clause BSD license.
 *   Copyright (c) 2012 - 2013, Lukas Hosek and Alexander Wilkie
 *   All rights reserved.
 *   Redistribution and use in source and binary forms, with or without modification,
 *   are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright notice, this list
 *       of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright notice, this
 *       list of conditions and the following disclaimer in the documentation and/or
 *       other materials provided with the distribution.
 *     * None of the names of the contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 *   EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 *   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 *   SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *   STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 *   OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Bruneton: the precomputation is a port of atmosphere/functions.glsl and of the
 * reference model's Init (atmosphere/reference/model.cc), Copyright (c) 2017 Eric
 * Bruneton, 3-clause BSD; the full notice is in core/alwan_sky_atmosphere_reader.inc and
 * licenses/bruneton-BSD-3-Clause.txt. The quantities are computed per wavelength exactly as there,
 * a batch of wavelengths at a time so the geometry is shared.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_sky_core.h"
#include "../core/alwan_table_core.h"
#include "../data/alwan_data_tables.h"
#include "../data/alwan_data_sky.h"
#include <string.h>

#include "alwan_sky_internal.h"

/* ================================================================
 * Observer CMFs at 1 nm, 360-830 nm
 * ================================================================ */

/* Each CMF value through alwan_xyz_from_spd_f64 itself: the trapezoid rule on a unit
 * impulse returns the CMF at an interior sample and half of it at an end, so every
 * observer alwan_xyz_from_spd knows is available here with nothing duplicated. */
static alwan_status alwan__sky_load_cmf(double *cmf, alwan_observer_type observer, alwan_ctx *ctx) {
    alwan_spd_f64 spd;
    alwan_status st = alwan_spd_create_f64(&spd, 360.0, 830.0, ALWAN__SKY_CMF_N, ctx);
    int i;
    if (st != ALWAN_OK) return st;
    for (i = 0; i < ALWAN__SKY_CMF_N && st == ALWAN_OK; i++) {
        alwan_xyz_f64 xyz;
        double const end = (i == 0 || i == ALWAN__SKY_CMF_N - 1) ? 2.0 : 1.0;
        memset(spd.values, 0, sizeof(double) * ALWAN__SKY_CMF_N);
        spd.values[i] = 1.0;
        st = alwan_xyz_from_spd_f64(&xyz, &spd, NULL, observer, ALWAN_INTEGRATE_TRAPEZOID, 0.0, ctx);
        cmf[i] = xyz.x * end;
        cmf[ALWAN__SKY_CMF_N + i] = xyz.y * end;
        cmf[2 * ALWAN__SKY_CMF_N + i] = xyz.z * end;
    }
    alwan_spd_destroy_f64(&spd, ctx);
    return st;
}

/* A CMF channel at a wavelength, linearly between the 1 nm samples, 0 outside. */
static double alwan__sky_cmf_at(double const *cmf, int c, double nm) {
    double p;
    int i;
    if (!(nm >= 360.0) || !(nm <= 830.0)) return 0.0;
    p = nm - 360.0;
    i = (int)ALWAN_FLOOR_F64(p);
    if (i >= ALWAN__SKY_CMF_N - 1) return cmf[c * ALWAN__SKY_CMF_N + ALWAN__SKY_CMF_N - 1];
    p -= (double)i;
    return cmf[c * ALWAN__SKY_CMF_N + i] * (1.0 - p) + cmf[c * ALWAN__SKY_CMF_N + i + 1] * p;
}

/* ================================================================
 * Preetham, Shirley and Smits 1999
 * ================================================================ */

static double alwan__sky_basis(int row, int k5) {
#if ALWAN_TABLE_DAYLIGHT_BASIS
    return alwan_table2d_row_at_f64_v(alwan_table_daylight_basis_f64, ALWAN_TABLE_DAYLIGHT_BASIS_ROWS,
                                      ALWAN_TABLE_QUALITY_SPECTRUM, row, k5);
#else
    (void)row; (void)k5;
    return 0.0;
#endif
}

/* The daylight basis S_row at a wavelength, linearly between its 5 nm samples. */
static double alwan__sky_basis_at(int row, double nm) {
    double p;
    int k;
    if (!(nm >= 360.0) || !(nm <= 830.0)) return 0.0;
    p = (nm - 360.0) / 5.0;
    k = (int)ALWAN_FLOOR_F64(p);
    if (k >= 94) return alwan__sky_basis(row, 94);
    p -= (double)k;
    return alwan__sky_basis(row, k) * (1.0 - p) + alwan__sky_basis(row, k + 1) * p;
}

/* A Table 2 column at a wavelength, linearly between its 10 nm rows; an absent value
 * (-1) reads as 0. 380-780 nm. */
static double alwan__sky_table2_at(int col, double nm) {
    double p, a, b;
    int k;
    if (!(nm >= 380.0) || !(nm <= 780.0)) return 0.0;
    p = (nm - 380.0) / 10.0;
    k = (int)ALWAN_FLOOR_F64(p);
    if (k >= ALWAN__SKY_PREETHAM_ROWS - 1) k = ALWAN__SKY_PREETHAM_ROWS - 2;
    p -= (double)k;
    a = alwan__sky_preetham[k * ALWAN__SKY_PREETHAM_COLS + col];
    b = alwan__sky_preetham[(k + 1) * ALWAN__SKY_PREETHAM_COLS + col];
    if (a < 0.0) a = 0.0;
    if (b < 0.0) b = 0.0;
    return a * (1.0 - p) + b * p;
}

static alwan_status alwan__sky_preetham_init(alwan__sky_state *st) {
#if ALWAN_TABLE_DAYLIGHT_BASIS
    int c, j, k;
    st->theta_s = ALWAN_PI_F64 * 0.5 - st->elevation;
    /* integrals of the basis against the observer and against V(lambda) on the 5 nm grid */
    for (j = 0; j < 3; j++) {
        double by = 0.0;
        for (c = 0; c < 3; c++) st->pr_b[c][j] = 0.0;
        for (k = 0; k < 95; k++) {
            double const w = (k == 0 || k == 94) ? 2.5 : 5.0;
            double const s = alwan__sky_basis(j, k);
            for (c = 0; c < 3; c++) st->pr_b[c][j] += w * s * st->cmf[c * ALWAN__SKY_CMF_N + 5 * k];
            by += w * s * st->cmf_1931[ALWAN__SKY_CMF_N + 5 * k];
        }
        st->pr_by[j] = by;
    }
    /* the sun's colour is the same over the whole disc: integrate it once */
    {
        double nm;
        for (c = 0; c < 3; c++) st->pr_sun_xyz[c] = 0.0;
        for (nm = 380.0; nm <= 750.0; nm += 1.0) {
            double const w = (nm == 380.0 || nm == 750.0) ? 0.5 : 1.0;
            double const s = alwan__sky_preetham_sun(st, nm);
            for (c = 0; c < 3; c++) st->pr_sun_xyz[c] += w * s * alwan__sky_cmf_at(st->cmf, c, nm);
        }
    }
    return ALWAN_OK;
#else
    (void)st;
    return ALWAN_E_NODATA;
#endif
}

/* Appendix A.1: the sun through the atmosphere, W m^-2 sr^-1 nm^-1, 380-750 nm, with the
 * paper's l = 0.0035 m of ozone and w = 0.02 m of water vapour; Table 2's k_o and k_wa are
 * per metre (0.125 per cm for ozone at 600 nm, Iqbal's value), so the units agree. */
static double alwan__sky_preetham_sun(alwan__sky_state const *st, double nm) {
    double const lam = nm / 1000.0;
    double const theta_deg = st->theta_s * 180.0 / ALWAN_PI_F64;
    double m, tr, ta, to, tg, twa, ko, kg, kwa, beta;
    if (!(nm >= 380.0) || !(nm <= 750.0)) return 0.0;
    m = 1.0 / (ALWAN_COS_F64(st->theta_s) + 0.15 * ALWAN_POW_F64(93.885 - theta_deg, -1.253));
    tr = ALWAN_EXP_F64(-0.008735 * ALWAN_POW_F64(lam, -4.08) * m);
    beta = 0.04608 * st->turbidity - 0.04586;
    ta = ALWAN_EXP_F64(-beta * ALWAN_POW_F64(lam, -1.3) * m);
    ko = alwan__sky_table2_at(6, nm);
    kwa = alwan__sky_table2_at(7, nm);
    kg = alwan__sky_table2_at(8, nm);
    to = ALWAN_EXP_F64(-ko * 0.0035 * m);
    tg = ALWAN_EXP_F64(-1.41 * kg * m / ALWAN_POW_F64(1.0 + 118.93 * kg * m, 0.45));
    twa = ALWAN_EXP_F64(-0.2385 * kwa * 0.02 * m / ALWAN_POW_F64(1.0 + 20.07 * kwa * 0.02 * m, 0.45));
    return alwan__sky_table2_at(5, nm) * tr * ta * to * tg * twa;
}

/* The sky's luminance and chromaticity along a direction above the horizon, as the
 * daylight-basis weights: out[0] the radiance scale (W per basis unit), out[1] M1,
 * out[2] M2. */
static void alwan__sky_preetham_weights(alwan__sky_state const *st, double cos_t, double gamma, double *out) {
    double const yv = alwan_sky_preetham_value_f64_v(0, st->turbidity, st->theta_s, cos_t, gamma);
    double const xv = alwan_sky_preetham_value_f64_v(1, st->turbidity, st->theta_s, cos_t, gamma);
    double const yy = alwan_sky_preetham_value_f64_v(2, st->turbidity, st->theta_s, cos_t, gamma);
    double const md = 0.0241 + 0.2562 * xv - 0.7341 * yy;
    double const m1 = (-1.3515 - 1.7703 * xv + 5.9114 * yy) / md;
    double const m2 = (0.0300 - 31.4424 * xv + 30.0717 * yy) / md;
    double const n = st->pr_by[0] + m1 * st->pr_by[1] + m2 * st->pr_by[2];
    out[0] = 1000.0 * yv / (683.0 * n);
    out[1] = m1;
    out[2] = m2;
}

/* ================================================================
 * Hosek and Wilkie (ArHosekSkyModel.c 1.4a)
 * ================================================================ */

static void alwan__sky_hw_cook(double const *dataset, double *config, double turbidity, double albedo,
                               double solar_elevation) {
    double const *elev_matrix;
    int const int_turbidity = (int)turbidity;
    double const turbidity_rem = turbidity - (double)int_turbidity;
    unsigned int i;
    solar_elevation = ALWAN_POW_F64(solar_elevation / (ALWAN_PI_F64 / 2.0), (1.0 / 3.0));
#define ALWAN__HW_BEZ(em, i) \
    ( ALWAN_POW_F64(1.0-solar_elevation, 5.0) * em[i]  + \
      5.0  * ALWAN_POW_F64(1.0-solar_elevation, 4.0) * solar_elevation * em[i+9] + \
      10.0*ALWAN_POW_F64(1.0-solar_elevation, 3.0)*ALWAN_POW_F64(solar_elevation, 2.0) * em[i+18] + \
      10.0*ALWAN_POW_F64(1.0-solar_elevation, 2.0)*ALWAN_POW_F64(solar_elevation, 3.0) * em[i+27] + \
      5.0*(1.0-solar_elevation)*ALWAN_POW_F64(solar_elevation, 4.0) * em[i+36] + \
      ALWAN_POW_F64(solar_elevation, 5.0)  * em[i+45])
    elev_matrix = dataset + (9 * 6 * (int_turbidity - 1));
    for (i = 0; i < 9; ++i) config[i] = (1.0 - albedo) * (1.0 - turbidity_rem) * ALWAN__HW_BEZ(elev_matrix, i);
    elev_matrix = dataset + (9 * 6 * 10 + 9 * 6 * (int_turbidity - 1));
    for (i = 0; i < 9; ++i) config[i] += (albedo) * (1.0 - turbidity_rem) * ALWAN__HW_BEZ(elev_matrix, i);
    if (int_turbidity == 10) return;
    elev_matrix = dataset + (9 * 6 * (int_turbidity));
    for (i = 0; i < 9; ++i) config[i] += (1.0 - albedo) * (turbidity_rem) * ALWAN__HW_BEZ(elev_matrix, i);
    elev_matrix = dataset + (9 * 6 * 10 + 9 * 6 * (int_turbidity));
    for (i = 0; i < 9; ++i) config[i] += (albedo) * (turbidity_rem) * ALWAN__HW_BEZ(elev_matrix, i);
#undef ALWAN__HW_BEZ
}

static double alwan__sky_hw_cook_radiance(double const *dataset, double turbidity, double albedo,
                                          double solar_elevation) {
    double const *elev_matrix;
    int const int_turbidity = (int)turbidity;
    double const turbidity_rem = turbidity - (double)int_turbidity;
    double res;
    solar_elevation = ALWAN_POW_F64(solar_elevation / (ALWAN_PI_F64 / 2.0), (1.0 / 3.0));
#define ALWAN__HW_BEZR(em) \
    ( ALWAN_POW_F64(1.0-solar_elevation, 5.0) * em[0] + \
      5.0*ALWAN_POW_F64(1.0-solar_elevation, 4.0)*solar_elevation * em[1] + \
      10.0*ALWAN_POW_F64(1.0-solar_elevation, 3.0)*ALWAN_POW_F64(solar_elevation, 2.0) * em[2] + \
      10.0*ALWAN_POW_F64(1.0-solar_elevation, 2.0)*ALWAN_POW_F64(solar_elevation, 3.0) * em[3] + \
      5.0*(1.0-solar_elevation)*ALWAN_POW_F64(solar_elevation, 4.0) * em[4] + \
      ALWAN_POW_F64(solar_elevation, 5.0) * em[5])
    elev_matrix = dataset + (6 * (int_turbidity - 1));
    res = (1.0 - albedo) * (1.0 - turbidity_rem) * ALWAN__HW_BEZR(elev_matrix);
    elev_matrix = dataset + (6 * 10 + 6 * (int_turbidity - 1));
    res += (albedo) * (1.0 - turbidity_rem) * ALWAN__HW_BEZR(elev_matrix);
    if (int_turbidity == 10) return res;
    elev_matrix = dataset + (6 * (int_turbidity));
    res += (1.0 - albedo) * (turbidity_rem) * ALWAN__HW_BEZR(elev_matrix);
    elev_matrix = dataset + (6 * 10 + 6 * (int_turbidity));
    res += (albedo) * (turbidity_rem) * ALWAN__HW_BEZR(elev_matrix);
#undef ALWAN__HW_BEZR
    return res;
}

/* One band's sky radiance along (theta, gamma), as ArHosekSkyModel_GetRadianceInternal
 * times the cooked radiance. */
static double alwan__sky_hw_band(alwan__sky_state const *st, int b, double theta, double gamma) {
    double const *c = st->hw_config[b];
    return alwan_sky_hosek_f64_v(c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8], ALWAN_COS_F64(theta), gamma)
         * st->hw_rad[b];
}

/* arhosekskymodel_radiance: the band below, and the one above weighted by where the
 * wavelength falls between them. */
static double alwan__sky_hw_spectral(alwan__sky_state const *st, double theta, double gamma, double wavelength) {
    int const low_wl = (int)((wavelength - 320.0) / 40.0);
    double interp, val_low, result;
    if (!(wavelength >= 320.0) || low_wl < 0 || low_wl >= 11) return 0.0;
    interp = ALWAN_FMOD_F64((wavelength - 320.0) / 40.0, 1.0);
    val_low = alwan__sky_hw_band(st, low_wl, theta, gamma);
    if (interp < 1e-6) return val_low;
    result = (1.0 - interp) * val_low;
    if (low_wl + 1 < 11) result += interp * alwan__sky_hw_band(st, low_wl + 1, theta, gamma);
    return result;
}

static double alwan__sky_hw_sr_internal(int turbidity, int wl, double elevation) {
    int const pieces = 45;
    int const order = 4;
    int pos = (int)(ALWAN_POW_F64(2.0 * elevation / ALWAN_PI_F64, 1.0 / 3.0) * pieces);
    double break_x, res = 0.0, x, x_exp = 1.0;
    double const *coefs;
    int i;
    if (pos > 44) pos = 44;
    break_x = ALWAN_POW_F64(((double)pos / (double)pieces), 3.0) * (ALWAN_PI_F64 * 0.5);
    coefs = alwan__sky_hw_solar + wl * ALWAN__SKY_HW_SOLAR + (order * pieces * turbidity + order * (pos + 1) - 1);
    x = elevation - break_x;
    for (i = 0; i < order; ++i) {
        res += x_exp * *coefs--;
        x_exp *= x;
    }
    return res;
}

/* arhosekskymodel_solar_radiance_internal2: the direct sun, limb darkened, at a view
 * elevation and angle gamma from the sun's centre, 320-720 nm. */
static double alwan__sky_hw_sun(alwan__sky_state const *st, double wavelength, double elevation, double gamma) {
    int turb_low = (int)st->turbidity - 1;
    double turb_frac = st->turbidity - (double)(turb_low + 1);
    int wl_low;
    double wl_frac, direct, ld[6], sol_rad_sin, ar2, singamma, sc2, sample_cosine, dark;
    int i;
    if (!(wavelength >= 320.0) || !(wavelength <= 720.0)) return 0.0;
    if (turb_low == 9) {
        turb_low = 8;
        turb_frac = 1.0;
    }
    wl_low = (int)((wavelength - 320.0) / 40.0);
    wl_frac = ALWAN_FMOD_F64(wavelength, 40.0) / 40.0;
    if (wl_low == 10) {
        wl_low = 9;
        wl_frac = 1.0;
    }
    direct = (1.0 - turb_frac)
               * ((1.0 - wl_frac) * alwan__sky_hw_sr_internal(turb_low, wl_low, elevation)
                  + wl_frac * alwan__sky_hw_sr_internal(turb_low, wl_low + 1, elevation))
           + turb_frac
               * ((1.0 - wl_frac) * alwan__sky_hw_sr_internal(turb_low + 1, wl_low, elevation)
                  + wl_frac * alwan__sky_hw_sr_internal(turb_low + 1, wl_low + 1, elevation));
    for (i = 0; i < 6; i++) {
        ld[i] = (1.0 - wl_frac) * alwan__sky_hw_limb[wl_low * 6 + i] + wl_frac * alwan__sky_hw_limb[(wl_low + 1) * 6 + i];
    }
    sol_rad_sin = ALWAN_SIN_F64(st->sun_radius);
    ar2 = 1 / (sol_rad_sin * sol_rad_sin);
    singamma = ALWAN_SIN_F64(gamma);
    sc2 = 1.0 - ar2 * singamma * singamma;
    if (sc2 < 0.0) sc2 = 0.0;
    sample_cosine = ALWAN_SQRT_F64(sc2);
    dark = ld[0]
         + ld[1] * sample_cosine
         + ld[2] * ALWAN_POW_F64(sample_cosine, 2.0)
         + ld[3] * ALWAN_POW_F64(sample_cosine, 3.0)
         + ld[4] * ALWAN_POW_F64(sample_cosine, 4.0)
         + ld[5] * ALWAN_POW_F64(sample_cosine, 5.0);
    return direct * dark;
}

static void alwan__sky_hw_init(alwan__sky_state *st) {
    int b, c, nm;
    for (b = 0; b < ALWAN__SKY_HW_BANDS; b++) {
        alwan__sky_hw_cook(alwan__sky_hw_config + b * ALWAN__SKY_HW_CONFIG, st->hw_config[b], st->turbidity,
                           st->albedo, st->elevation);
        st->hw_rad[b] = alwan__sky_hw_cook_radiance(alwan__sky_hw_radiance + b * ALWAN__SKY_HW_RADIANCE,
                                                    st->turbidity, st->albedo, st->elevation);
        for (c = 0; c < 3; c++) st->hw_band_xyz[b][c] = 0.0;
    }
    /* XYZ of each band's share of the interpolated spectrum, 360-830 nm at 1 nm */
    for (nm = 360; nm <= 830; nm++) {
        double const w = (nm == 360 || nm == 830) ? 0.5 : 1.0;
        int const low = (nm - 320) / 40;             /* whole nanometres: integer division */
        double const interp = (double)((nm - 320) % 40) / 40.0;
        if (low >= 11) continue;
        for (c = 0; c < 3; c++) {
            double const cm = st->cmf[c * ALWAN__SKY_CMF_N + (nm - 360)] * w;
            if (interp < 1e-6) {
                st->hw_band_xyz[low][c] += cm;
            } else {
                st->hw_band_xyz[low][c] += (1.0 - interp) * cm;
                if (low + 1 < 11) st->hw_band_xyz[low + 1][c] += interp * cm;
            }
        }
    }
}

/* ================================================================
 * ASTM G173
 * ================================================================ */

static double alwan__sky_g173_at(int col, double nm) {
    int lo = 0, hi = ALWAN__SKY_G173_ROWS - 1;
    double a, b, f;
    if (!(nm >= alwan__sky_g173[0]) || !(nm <= alwan__sky_g173[(ALWAN__SKY_G173_ROWS - 1) * 4])) return 0.0;
    while (hi - lo > 1) {
        int const mid = (lo + hi) / 2;
        if (alwan__sky_g173[mid * 4] <= nm) lo = mid; else hi = mid;
    }
    a = alwan__sky_g173[lo * 4];
    b = alwan__sky_g173[hi * 4];
    f = b > a ? (nm - a) / (b - a) : 0.0;
    return alwan__sky_g173[lo * 4 + 1 + col] * (1.0 - f) + alwan__sky_g173[hi * 4 + 1 + col] * f;
}

/* Bruneton's solar spectrum: the extraterrestrial column averaged over [l, l + 10) for
 * l = 360, 370, ..., 830 (his demo's table, reproduced), read linearly between the bin
 * values and held beyond them, as his Interpolate does. */
static double alwan__sky_bruneton_solar(double const *bins, double nm) {
    double p;
    int i;
    if (nm <= 360.0) return bins[0];
    if (nm >= 830.0) return bins[47];
    p = (nm - 360.0) / 10.0;
    i = (int)ALWAN_FLOOR_F64(p);
    if (i > 46) i = 46;
    p -= (double)i;
    return bins[i] * (1.0 - p) + bins[i + 1] * p;
}

static void alwan__sky_bruneton_solar_bins(double *bins) {
    int b, r;
    for (b = 0; b < 48; b++) {
        double const lo = 360.0 + 10.0 * b, hi = lo + 10.0;
        double sum = 0.0;
        int n = 0;
        for (r = 0; r < ALWAN__SKY_G173_ROWS; r++) {
            double const wl = alwan__sky_g173[r * 4];
            if (wl >= lo && wl < hi) {
                sum += alwan__sky_g173[r * 4 + 1];
                n++;
            }
        }
        bins[b] = n > 0 ? sum / (double)n : 0.0;
    }
}

static double alwan__sky_bruneton_ozone(double nm) {
    double p;
    int i;
    if (nm <= 360.0) return alwan__sky_ozone[0];
    if (nm >= 830.0) return alwan__sky_ozone[47];
    p = (nm - 360.0) / 10.0;
    i = (int)ALWAN_FLOOR_F64(p);
    if (i > 46) i = 46;
    p -= (double)i;
    return alwan__sky_ozone[i] * (1.0 - p) + alwan__sky_ozone[i + 1] * p;
}

/* ================================================================
 * Bruneton 2017: the precomputation, a batch of up to ALWAN__SKY_BATCH wavelengths
 * ================================================================ */

typedef struct {
    double width, exp_term, exp_scale, linear_term, constant_term;
} alwan__bru_layer;

typedef struct {
    double bottom, top, mu_s_min, mie_g, sun_ar, albedo;
    alwan__bru_layer lay[3][2];    /* 0 Rayleigh, 1 Mie, 2 absorption */
    int tw, th, nr, nmu, nmus, nnu, iw, ih;
    int g;                         /* wavelengths in the batch */
    double ray[ALWAN__SKY_BATCH], mie_s[ALWAN__SKY_BATCH], mie_e[ALWAN__SKY_BATCH];
    double absorb[ALWAN__SKY_BATCH], solar[ALWAN__SKY_BATCH];
    double big_a;                  /* the mu_s parameterisation's A, fixed by the atmosphere */
} alwan__bru;

static double alwan__bru_clamp_cos(double mu) { return mu < -1.0 ? -1.0 : (mu > 1.0 ? 1.0 : mu); }
static double alwan__bru_clamp_dist(double d) { return d > 0.0 ? d : 0.0; }
static double alwan__bru_clamp_r(alwan__bru const *a, double r) {
    return r < a->bottom ? a->bottom : (r > a->top ? a->top : r);
}
static double alwan__bru_safe_sqrt(double x) { return ALWAN_SQRT_F64(x > 0.0 ? x : 0.0); }

static double alwan__bru_dist_top(alwan__bru const *a, double r, double mu) {
    double const disc = r * r * (mu * mu - 1.0) + a->top * a->top;
    return alwan__bru_clamp_dist(-r * mu + alwan__bru_safe_sqrt(disc));
}
static double alwan__bru_dist_bottom(alwan__bru const *a, double r, double mu) {
    double const disc = r * r * (mu * mu - 1.0) + a->bottom * a->bottom;
    return alwan__bru_clamp_dist(-r * mu - alwan__bru_safe_sqrt(disc));
}
static int alwan__bru_hits_ground(alwan__bru const *a, double r, double mu) {
    return mu < 0.0 && r * r * (mu * mu - 1.0) + a->bottom * a->bottom >= 0.0;
}
static double alwan__bru_layer_density(alwan__bru_layer const *l, double alt) {
    double const d = l->exp_term * ALWAN_EXP_F64(l->exp_scale * alt) + l->linear_term * alt + l->constant_term;
    return d < 0.0 ? 0.0 : (d > 1.0 ? 1.0 : d);
}
static double alwan__bru_profile(alwan__bru_layer const *p, double alt) {
    return alt < p[0].width ? alwan__bru_layer_density(&p[0], alt) : alwan__bru_layer_density(&p[1], alt);
}
static double alwan__bru_optical_length(alwan__bru const *a, alwan__bru_layer const *p, double r, double mu) {
    int const n = 500;
    double const dx = alwan__bru_dist_top(a, r, mu) / (double)n;
    double result = 0.0;
    int i;
    for (i = 0; i <= n; ++i) {
        double const d_i = (double)i * dx;
        double const r_i = ALWAN_SQRT_F64(d_i * d_i + 2.0 * r * mu * d_i + r * r);
        double const y_i = alwan__bru_profile(p, r_i - a->bottom);
        double const w_i = (i == 0 || i == n) ? 0.5 : 1.0;
        result += y_i * w_i * dx;
    }
    return result;
}
static double alwan__bru_coord(double x, int n) { return 0.5 / (double)n + x * (1.0 - 1.0 / (double)n); }
static double alwan__bru_unit(double u, int n) { return (u - 0.5 / (double)n) / (1.0 - 1.0 / (double)n); }

static int alwan__bru_clampi(int i, int n) { return i < 0 ? 0 : (i > n - 1 ? n - 1 : i); }

/* bilinear / trilinear clamp-to-edge reads of a batch table (g values a texel) */
static void alwan__bru_tex2d(double *out, double const *tab, int w, int h, int g, double x, double y) {
    double u = x * (double)w - 0.5, v = y * (double)h - 0.5;
    double const fu = ALWAN_FLOOR_F64(u), fv = ALWAN_FLOOR_F64(v);
    int const i = (int)fu, j = (int)fv;
    int const i0 = alwan__bru_clampi(i, w), i1 = alwan__bru_clampi(i + 1, w);
    int const j0 = alwan__bru_clampi(j, h), j1 = alwan__bru_clampi(j + 1, h);
    double const *p00, *p10, *p01, *p11;
    double w00, w10, w01, w11;
    int k;
    u -= fu;
    v -= fv;
    w00 = (1.0 - u) * (1.0 - v); w10 = u * (1.0 - v); w01 = (1.0 - u) * v; w11 = u * v;
    p00 = tab + (size_t)(i0 + j0 * w) * g; p10 = tab + (size_t)(i1 + j0 * w) * g;
    p01 = tab + (size_t)(i0 + j1 * w) * g; p11 = tab + (size_t)(i1 + j1 * w) * g;
    for (k = 0; k < g; k++) out[k] = p00[k] * w00 + p10[k] * w10 + p01[k] * w01 + p11[k] * w11;
}

static void alwan__bru_tex3d(double *out, double const *tab, int w, int h, int d, int g, double x, double y, double z,
                             double scale, int accumulate) {
    double u = x * (double)w - 0.5, v = y * (double)h - 0.5, s = z * (double)d - 0.5;
    double const fu = ALWAN_FLOOR_F64(u), fv = ALWAN_FLOOR_F64(v), fs = ALWAN_FLOOR_F64(s);
    int const i = (int)fu, j = (int)fv, l = (int)fs;
    int const i0 = alwan__bru_clampi(i, w), i1 = alwan__bru_clampi(i + 1, w);
    int const j0 = alwan__bru_clampi(j, h), j1 = alwan__bru_clampi(j + 1, h);
    int const l0 = alwan__bru_clampi(l, d), l1 = alwan__bru_clampi(l + 1, d);
    size_t const wh = (size_t)w * (size_t)h;
    double c[8];
    double const *p[8];
    int k, q;
    u -= fu;
    v -= fv;
    s -= fs;
    c[0] = (1.0 - u) * (1.0 - v) * (1.0 - s); p[0] = tab + ((size_t)i0 + (size_t)j0 * w + l0 * wh) * g;
    c[1] = u * (1.0 - v) * (1.0 - s);         p[1] = tab + ((size_t)i1 + (size_t)j0 * w + l0 * wh) * g;
    c[2] = (1.0 - u) * v * (1.0 - s);         p[2] = tab + ((size_t)i0 + (size_t)j1 * w + l0 * wh) * g;
    c[3] = u * v * (1.0 - s);                 p[3] = tab + ((size_t)i1 + (size_t)j1 * w + l0 * wh) * g;
    c[4] = (1.0 - u) * (1.0 - v) * s;         p[4] = tab + ((size_t)i0 + (size_t)j0 * w + l1 * wh) * g;
    c[5] = u * (1.0 - v) * s;                 p[5] = tab + ((size_t)i1 + (size_t)j0 * w + l1 * wh) * g;
    c[6] = (1.0 - u) * v * s;                 p[6] = tab + ((size_t)i0 + (size_t)j1 * w + l1 * wh) * g;
    c[7] = u * v * s;                         p[7] = tab + ((size_t)i1 + (size_t)j1 * w + l1 * wh) * g;
    for (k = 0; k < g; k++) {
        double sum = p[0][k] * c[0];
        for (q = 1; q < 8; q++) sum += p[q][k] * c[q];
        if (accumulate) out[k] += sum * scale; else out[k] = sum * scale;
    }
}

static void alwan__bru_trans_uv(alwan__bru const *a, double r, double mu, double *x, double *y) {
    double const hh = ALWAN_SQRT_F64(a->top * a->top - a->bottom * a->bottom);
    double const rho = alwan__bru_safe_sqrt(r * r - a->bottom * a->bottom);
    double const d = alwan__bru_dist_top(a, r, mu);
    double const d_min = a->top - r;
    double const d_max = rho + hh;
    *x = alwan__bru_coord((d - d_min) / (d_max - d_min), a->tw);
    *y = alwan__bru_coord(rho / hh, a->th);
}

static void alwan__bru_trans_to_top(double *out, alwan__bru const *a, double const *trans, double r, double mu) {
    double x, y;
    alwan__bru_trans_uv(a, r, mu, &x, &y);
    alwan__bru_tex2d(out, trans, a->tw, a->th, a->g, x, y);
}

static void alwan__bru_transmittance(double *out, alwan__bru const *a, double const *trans, double r, double mu,
                                     double d, int ground) {
    double const r_d = alwan__bru_clamp_r(a, ALWAN_SQRT_F64(d * d + 2.0 * r * mu * d + r * r));
    double const mu_d = alwan__bru_clamp_cos((r * mu + d) / r_d);
    double t1[ALWAN__SKY_BATCH], t2[ALWAN__SKY_BATCH];
    int k;
    if (ground) {
        alwan__bru_trans_to_top(t1, a, trans, r_d, -mu_d);
        alwan__bru_trans_to_top(t2, a, trans, r, -mu);
    } else {
        alwan__bru_trans_to_top(t1, a, trans, r, mu);
        alwan__bru_trans_to_top(t2, a, trans, r_d, mu_d);
    }
    for (k = 0; k < a->g; k++) {
        double const q = t1[k] / t2[k];
        out[k] = q < 1.0 ? q : 1.0;
    }
}

static double alwan__bru_smoothstep(double e0, double e1, double x) {
    double t = (x - e0) / (e1 - e0);
    t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
    return t * t * (3.0 - 2.0 * t);
}

static void alwan__bru_trans_to_sun(double *out, alwan__bru const *a, double const *trans, double r, double mu_s) {
    double const sin_h = a->bottom / r;
    double const one_m = 1.0 - sin_h * sin_h;
    double const cos_h = -ALWAN_SQRT_F64(one_m > 0.0 ? one_m : 0.0);
    double const f = alwan__bru_smoothstep(-sin_h * a->sun_ar, sin_h * a->sun_ar, mu_s - cos_h);
    int k;
    alwan__bru_trans_to_top(out, a, trans, r, mu_s);
    for (k = 0; k < a->g; k++) out[k] *= f;
}

static double alwan__bru_rayleigh_phase(double nu) { return 3.0 / (16.0 * ALWAN_PI_F64) * (1.0 + nu * nu); }
static double alwan__bru_mie_phase(double g, double nu) {
    double const k = 3.0 / (8.0 * ALWAN_PI_F64) * (1.0 - g * g) / (2.0 + g * g);
    double const x = 1.0 + g * g - 2.0 * g * nu;
    return k * (1.0 + nu * nu) / (x * ALWAN_SQRT_F64(x));   /* x^1.5 */
}

/* GetScatteringTextureUvwzFromRMuMuSNu, in its four separable parts. */
static double alwan__bru_u_r(alwan__bru const *a, double r) {
    double const hh = ALWAN_SQRT_F64(a->top * a->top - a->bottom * a->bottom);
    double const rho = alwan__bru_safe_sqrt(r * r - a->bottom * a->bottom);
    return alwan__bru_coord(rho / hh, a->nr);
}
static double alwan__bru_u_mu(alwan__bru const *a, double r, double mu, int ground) {
    double const hh = ALWAN_SQRT_F64(a->top * a->top - a->bottom * a->bottom);
    double const rho = alwan__bru_safe_sqrt(r * r - a->bottom * a->bottom);
    double const r_mu = r * mu;
    double const disc = r_mu * r_mu - r * r + a->bottom * a->bottom;
    double d, d_min, d_max;
    if (ground) {
        d = -r_mu - alwan__bru_safe_sqrt(disc);
        d_min = r - a->bottom;
        d_max = rho;
        return 0.5 - 0.5 * alwan__bru_coord(d_max == d_min ? 0.0 : (d - d_min) / (d_max - d_min), a->nmu / 2);
    }
    d = -r_mu + alwan__bru_safe_sqrt(disc + hh * hh);
    d_min = a->top - r;
    d_max = rho + hh;
    return 0.5 + 0.5 * alwan__bru_coord((d - d_min) / (d_max - d_min), a->nmu / 2);
}
static double alwan__bru_big_a(alwan__bru const *a) {
    double const hh = ALWAN_SQRT_F64(a->top * a->top - a->bottom * a->bottom);
    double const d_min = a->top - a->bottom, d_max = hh;
    double const big_d = alwan__bru_dist_top(a, a->bottom, a->mu_s_min);
    return (big_d - d_min) / (d_max - d_min);
}
static double alwan__bru_u_mu_s(alwan__bru const *a, double mu_s) {
    double const hh = ALWAN_SQRT_F64(a->top * a->top - a->bottom * a->bottom);
    double const d = alwan__bru_dist_top(a, a->bottom, mu_s);
    double const d_min = a->top - a->bottom, d_max = hh;
    double const aa = (d - d_min) / (d_max - d_min);
    double m = 1.0 - aa / a->big_a;
    if (m < 0.0) m = 0.0;
    return alwan__bru_coord(m / (1.0 + aa), a->nmus);
}

/* The same read of one or two tables (tab_b may be NULL) sharing every coordinate and
 * weight: tab_a times scale_a plus tab_b times scale_b, set into out. The density integral's
 * inner loop, where the Rayleigh and Mie tables are read at the same place. */
static void alwan__bru_scat2(double *out, alwan__bru const *a, double const *tab_a, double scale_a,
                             double const *tab_b, double scale_b, double u_mu_s, double u_mu, double u_r, double nu) {
    int const w = a->nnu * a->nmus, h = a->nmu, d = a->nr, g = a->g;
    size_t const wh = (size_t)w * (size_t)h;
    double const tex_coord_x = (nu + 1.0) / 2.0 * (double)(a->nnu - 1);
    double const tex_x = ALWAN_FLOOR_F64(tex_coord_x);
    double const lerp = tex_coord_x - tex_x;
    double v = u_mu * (double)h - 0.5, s = u_r * (double)d - 0.5;
    double const fv = ALWAN_FLOOR_F64(v), fs = ALWAN_FLOOR_F64(s);
    int const j0 = alwan__bru_clampi((int)fv, h), j1 = alwan__bru_clampi((int)fv + 1, h);
    int const l0 = alwan__bru_clampi((int)fs, d), l1 = alwan__bru_clampi((int)fs + 1, d);
    size_t base[4];
    double wjl[4], wi[4];
    size_t ii[4];
    int sl, c, k, q;
    v -= fv;
    s -= fs;
    base[0] = (size_t)j0 * w + l0 * wh; wjl[0] = (1.0 - v) * (1.0 - s);
    base[1] = (size_t)j1 * w + l0 * wh; wjl[1] = v * (1.0 - s);
    base[2] = (size_t)j0 * w + l1 * wh; wjl[2] = (1.0 - v) * s;
    base[3] = (size_t)j1 * w + l1 * wh; wjl[3] = v * s;
    for (sl = 0; sl < 2; sl++) {
        double const x = (tex_x + (double)sl + u_mu_s) / (double)a->nnu;
        double u = x * (double)w - 0.5;
        double const fu = ALWAN_FLOOR_F64(u);
        double const ws = sl == 0 ? 1.0 - lerp : lerp;
        u -= fu;
        ii[sl * 2] = (size_t)alwan__bru_clampi((int)fu, w);
        ii[sl * 2 + 1] = (size_t)alwan__bru_clampi((int)fu + 1, w);
        wi[sl * 2] = (1.0 - u) * ws;
        wi[sl * 2 + 1] = u * ws;
    }
    for (q = 0; q < g; q++) out[q] = 0.0;
    for (c = 0; c < 4; c++) {
        for (k = 0; k < 4; k++) {
            size_t const at = (base[c] + ii[k]) * g;
            double const wt = wjl[c] * wi[k];
            double const wa = wt * scale_a;
            if (tab_b) {
                double const wb = wt * scale_b;
                for (q = 0; q < g; q++) out[q] += tab_a[at + q] * wa + tab_b[at + q] * wb;
            } else {
                for (q = 0; q < g; q++) out[q] += tab_a[at + q] * wa;
            }
        }
    }
}

/* The two nu slices of a scattering-size batch table at (u_mu_s, u_mu, u_r), blended at nu,
 * times scale, into out (or added). */
static void alwan__bru_scat_at(double *out, alwan__bru const *a, double const *tab, double u_mu_s, double u_mu,
                               double u_r, double nu, double scale, int accumulate) {
    int const w = a->nnu * a->nmus;
    double const tex_coord_x = (nu + 1.0) / 2.0 * (double)(a->nnu - 1);
    double const tex_x = ALWAN_FLOOR_F64(tex_coord_x);
    double const lerp = tex_coord_x - tex_x;
    alwan__bru_tex3d(out, tab, w, a->nmu, a->nr, a->g, (tex_x + u_mu_s) / (double)a->nnu, u_mu, u_r,
                     scale * (1.0 - lerp), accumulate);
    alwan__bru_tex3d(out, tab, w, a->nmu, a->nr, a->g, (tex_x + 1.0 + u_mu_s) / (double)a->nnu, u_mu, u_r,
                     scale * lerp, 1);
}

/* GetScattering of a scattering-size batch table, times scale, into out (or added). */
static void alwan__bru_get_scat(double *out, alwan__bru const *a, double const *tab, double r, double mu, double mu_s,
                                double nu, int ground, double scale, int accumulate) {
    alwan__bru_scat_at(out, a, tab, alwan__bru_u_mu_s(a, mu_s), alwan__bru_u_mu(a, r, mu, ground), alwan__bru_u_r(a, r),
                       nu, scale, accumulate);
}

static void alwan__bru_scat_from_frag(alwan__bru const *a, double fx, double fy, double fz, double *r, double *mu,
                                      double *mu_s, double *nu, int *ground) {
    double const hh = ALWAN_SQRT_F64(a->top * a->top - a->bottom * a->bottom);
    double const frag_nu = ALWAN_FLOOR_F64(fx / (double)a->nmus);
    double const frag_mu_s = fx - (double)a->nmus * ALWAN_FLOOR_F64(fx / (double)a->nmus);
    double const ux = frag_nu / (double)(a->nnu - 1);
    double const uy = frag_mu_s / (double)a->nmus;
    double const uz = fy / (double)a->nmu;
    double const uw = fz / (double)a->nr;
    double const rho = hh * alwan__bru_unit(uw, a->nr);
    double d_min, d_max, d, x_mu_s, big_d, big_a, aa, lim;
    *r = ALWAN_SQRT_F64(rho * rho + a->bottom * a->bottom);
    if (uz < 0.5) {
        d_min = *r - a->bottom;
        d_max = rho;
        d = d_min + (d_max - d_min) * alwan__bru_unit(1.0 - 2.0 * uz, a->nmu / 2);
        *mu = d == 0.0 ? -1.0 : alwan__bru_clamp_cos(-(rho * rho + d * d) / (2.0 * *r * d));
        *ground = 1;
    } else {
        d_min = a->top - *r;
        d_max = rho + hh;
        d = d_min + (d_max - d_min) * alwan__bru_unit(2.0 * uz - 1.0, a->nmu / 2);
        *mu = d == 0.0 ? 1.0 : alwan__bru_clamp_cos((hh * hh - rho * rho - d * d) / (2.0 * *r * d));
        *ground = 0;
    }
    x_mu_s = alwan__bru_unit(uy, a->nmus);
    d_min = a->top - a->bottom;
    d_max = hh;
    big_d = alwan__bru_dist_top(a, a->bottom, a->mu_s_min);
    big_a = (big_d - d_min) / (d_max - d_min);
    aa = (big_a - x_mu_s * big_a) / (1.0 + x_mu_s * big_a);
    d = d_min + (aa < big_a ? aa : big_a) * (d_max - d_min);
    *mu_s = d == 0.0 ? 1.0 : alwan__bru_clamp_cos((hh * hh - d * d) / (2.0 * a->bottom * d));
    *nu = alwan__bru_clamp_cos(ux * 2.0 - 1.0);
    lim = ALWAN_SQRT_F64((1.0 - *mu * *mu) * (1.0 - *mu_s * *mu_s));
    {
        double const lo = *mu * *mu_s - lim, hi = *mu * *mu_s + lim;
        *nu = *nu < lo ? lo : (*nu > hi ? hi : *nu);
    }
}

static void alwan__bru_irr_rmus_from_frag(alwan__bru const *a, double fx, double fy, double *r, double *mu_s) {
    double const x_mu_s = alwan__bru_unit(fx / (double)a->iw, a->iw);
    double const x_r = alwan__bru_unit(fy / (double)a->ih, a->ih);
    *r = a->bottom + x_r * (a->top - a->bottom);
    *mu_s = alwan__bru_clamp_cos(2.0 * x_mu_s - 1.0);
}

static void alwan__bru_get_irr(double *out, alwan__bru const *a, double const *irr, double r, double mu_s) {
    double const x_r = (r - a->bottom) / (a->top - a->bottom);
    double const x_mu_s = mu_s * 0.5 + 0.5;
    alwan__bru_tex2d(out, irr, a->iw, a->ih, a->g, alwan__bru_coord(x_mu_s, a->iw), alwan__bru_coord(x_r, a->ih));
}

/* GetScattering(order): order 1 the single Rayleigh and Mie with their phases, else the
 * multiple scattering table, into out (added when accumulate). */
static void alwan__bru_get_scat_order(double *out, alwan__bru const *a, double const *ray, double const *mie,
                                      double const *mult, double r, double mu, double mu_s, double nu, int ground,
                                      int order, double scale) {
    if (order == 1) {
        alwan__bru_get_scat(out, a, ray, r, mu, mu_s, nu, ground, scale * alwan__bru_rayleigh_phase(nu), 1);
        alwan__bru_get_scat(out, a, mie, r, mu, mu_s, nu, ground, scale * alwan__bru_mie_phase(a->mie_g, nu), 1);
    } else {
        alwan__bru_get_scat(out, a, mult, r, mu, mu_s, nu, ground, scale, 1);
    }
}

/* The precomputation of one batch: transmittance, irradiance (sky, all orders past the
 * first), scattering (Rayleigh single + all multiple orders, over the Rayleigh phase) and
 * single Mie, each g values a texel. ol holds the three optical lengths a transmittance
 * texel, shared by every batch. */
static alwan_status alwan__bru_precompute(alwan__bru const *a, double const *ol, int orders, double *trans,
                                          double *irr, double *scat, double *mie_single) {
    int const g = a->g;
    int const sw = a->nnu * a->nmus;
    size_t const ns = (size_t)sw * (size_t)a->nmu * (size_t)a->nr;
    size_t const ni = (size_t)a->iw * (size_t)a->ih;
    double *delta_irr = (double *)ALWAN_ALLOC(alwan_safe_array_size(ni * g, sizeof(double)), sizeof(double));
    double *delta_ray = (double *)ALWAN_ALLOC(alwan_safe_array_size(ns * g, sizeof(double)), sizeof(double));
    double *density = (double *)ALWAN_ALLOC(alwan_safe_array_size(ns * g, sizeof(double)), sizeof(double));
    double *delta_mult = (double *)ALWAN_ALLOC(alwan_safe_array_size(ns * g, sizeof(double)), sizeof(double));
    double *delta_mie = mie_single;
    double dir_ct[16], dir_st[16], dir_dom[16], dir_cp[32], dir_sp[32];
    int i, j, k, order, q;
    if (!delta_irr || !delta_ray || !density || !delta_mult) {
        ALWAN_FREE(delta_irr); ALWAN_FREE(delta_ray); ALWAN_FREE(density); ALWAN_FREE(delta_mult);
        return ALWAN_E_NOMEM;
    }
    memset(delta_mult, 0, sizeof(double) * ns * g);
    /* the 16 x 32 directions of the scattering density integral, and their solid angles */
    {
        double const dtheta = ALWAN_PI_F64 / 16.0, dphi = ALWAN_PI_F64 / 16.0;
        int l, m;
        for (l = 0; l < 16; ++l) {
            double const theta = ((double)l + 0.5) * dtheta;
            dir_ct[l] = ALWAN_COS_F64(theta);
            dir_st[l] = ALWAN_SIN_F64(theta);
            dir_dom[l] = dtheta * dphi * ALWAN_SIN_F64(theta);
        }
        for (m = 0; m < 32; ++m) {
            double const phi = ((double)m + 0.5) * dphi;
            dir_cp[m] = ALWAN_COS_F64(phi);
            dir_sp[m] = ALWAN_SIN_F64(phi);
        }
    }

    /* transmittance */
    for (j = 0; j < a->th; j++) {
        for (i = 0; i < a->tw; i++) {
            double const *o = ol + ((size_t)i + (size_t)j * a->tw) * 3;
            for (q = 0; q < g; q++) {
                trans[((size_t)i + (size_t)j * a->tw) * g + q] =
                    ALWAN_EXP_F64(-(a->ray[q] * o[0] + a->mie_e[q] * o[1] + a->absorb[q] * o[2]));
            }
        }
    }
    /* direct irradiance into delta, the sky irradiance starts at 0 */
    for (j = 0; j < a->ih; j++) {
        for (i = 0; i < a->iw; i++) {
            double r, mu_s, t[ALWAN__SKY_BATCH], alpha_s, acf;
            size_t const at = ((size_t)i + (size_t)j * a->iw) * g;
            alwan__bru_irr_rmus_from_frag(a, i + 0.5, j + 0.5, &r, &mu_s);
            alpha_s = a->sun_ar;
            acf = mu_s < -alpha_s ? 0.0 : (mu_s > alpha_s ? mu_s : (mu_s + alpha_s) * (mu_s + alpha_s) / (4.0 * alpha_s));
            alwan__bru_trans_to_top(t, a, trans, r, mu_s);
            for (q = 0; q < g; q++) {
                delta_irr[at + q] = a->solar[q] * t[q] * acf;
                irr[at + q] = 0.0;
            }
        }
    }
    /* single scattering */
    for (k = 0; k < a->nr; k++) {
        for (j = 0; j < a->nmu; j++) {
            for (i = 0; i < sw; i++) {
                size_t const at = ((size_t)i + (size_t)j * sw + (size_t)k * sw * a->nmu) * g;
                double r, mu, mu_s, nu, dx, rs[ALWAN__SKY_BATCH], ms[ALWAN__SKY_BATCH];
                int ground, s;
                int const n = 50;
                alwan__bru_scat_from_frag(a, i + 0.5, j + 0.5, k + 0.5, &r, &mu, &mu_s, &nu, &ground);
                dx = (ground ? alwan__bru_dist_bottom(a, r, mu) : alwan__bru_dist_top(a, r, mu)) / (double)n;
                for (q = 0; q < g; q++) rs[q] = ms[q] = 0.0;
                for (s = 0; s <= n; ++s) {
                    double const d = (double)s * dx;
                    double const r_d = alwan__bru_clamp_r(a, ALWAN_SQRT_F64(d * d + 2.0 * r * mu * d + r * r));
                    double const mu_s_d = alwan__bru_clamp_cos((r * mu_s + d * nu) / r_d);
                    double const w = (s == 0 || s == n) ? 0.5 : 1.0;
                    double const dr = alwan__bru_profile(a->lay[0], r_d - a->bottom);
                    double const dm = alwan__bru_profile(a->lay[1], r_d - a->bottom);
                    double t1[ALWAN__SKY_BATCH], t2[ALWAN__SKY_BATCH];
                    alwan__bru_transmittance(t1, a, trans, r, mu, d, ground);
                    alwan__bru_trans_to_sun(t2, a, trans, r_d, mu_s_d);
                    for (q = 0; q < g; q++) {
                        double const t = t1[q] * t2[q];
                        rs[q] += t * dr * w;
                        ms[q] += t * dm * w;
                    }
                }
                for (q = 0; q < g; q++) {
                    double const ray = rs[q] * dx * a->solar[q] * a->ray[q];
                    delta_ray[at + q] = ray;
                    delta_mie[at + q] = ms[q] * dx * a->solar[q] * a->mie_s[q];
                    scat[at + q] = ray;
                }
            }
        }
    }
    /* orders 2 and up */
    for (order = 2; order <= orders; ++order) {
        /* scattering density */
        for (k = 0; k < a->nr; k++) {
            for (j = 0; j < a->nmu; j++) {
                for (i = 0; i < sw; i++) {
                    size_t const at = ((size_t)i + (size_t)j * sw + (size_t)k * sw * a->nmu) * g;
                    double r, mu, mu_s, nu, ox, os_x, os_y, rm[ALWAN__SKY_BATCH];
                    int ground, l, m;
                    alwan__bru_scat_from_frag(a, i + 0.5, j + 0.5, k + 0.5, &r, &mu, &mu_s, &nu, &ground);
                    ox = ALWAN_SQRT_F64(1.0 - mu * mu);
                    os_x = ox == 0.0 ? 0.0 : (nu - mu * mu_s) / ox;
                    {
                        double const t = 1.0 - os_x * os_x - mu_s * mu_s;
                        os_y = ALWAN_SQRT_F64(t > 0.0 ? t : 0.0);
                    }
                    for (q = 0; q < g; q++) rm[q] = 0.0;
                    {
                        /* the parts of the table coordinate that a texel or a ring fixes */
                        double const u_r = alwan__bru_u_r(a, r);
                        double const u_mu_s = alwan__bru_u_mu_s(a, mu_s);
                        double const dr = alwan__bru_profile(a->lay[0], r - a->bottom);
                        double const dm = alwan__bru_profile(a->lay[1], r - a->bottom);
                        for (l = 0; l < 16; ++l) {
                            double const ct = dir_ct[l], stt = dir_st[l];
                            int const rg = alwan__bru_hits_ground(a, r, ct);
                            double const u_mu = alwan__bru_u_mu(a, r, ct, rg);
                            double const domega = dir_dom[l];
                            double dist_ground = 0.0, tg[ALWAN__SKY_BATCH];
                            for (q = 0; q < g; q++) tg[q] = 0.0;
                            if (rg) {
                                dist_ground = alwan__bru_dist_bottom(a, r, ct);
                                alwan__bru_transmittance(tg, a, trans, r, ct, dist_ground, 1);
                            }
                            for (m = 0; m < 32; ++m) {
                                double const wx = dir_cp[m] * stt, wy = dir_sp[m] * stt, wz = ct;
                                double const nu1 = os_x * wx + os_y * wy + mu_s * wz;
                                double const nu2 = ox * wx + mu * wz;
                                double const pr = alwan__bru_rayleigh_phase(nu2), pm = alwan__bru_mie_phase(a->mie_g, nu2);
                                double inc[ALWAN__SKY_BATCH];
                                if (order - 1 == 1) {
                                    alwan__bru_scat2(inc, a, delta_ray, alwan__bru_rayleigh_phase(nu1), delta_mie,
                                                     alwan__bru_mie_phase(a->mie_g, nu1), u_mu_s, u_mu, u_r, nu1);
                                } else {
                                    alwan__bru_scat2(inc, a, delta_mult, 1.0, NULL, 0.0, u_mu_s, u_mu, u_r, nu1);
                                }
                                if (rg) {
                                    /* ground_normal = normalize(zenith * r + omega_i * distance_to_ground) */
                                    double const gx = wx * dist_ground, gy = wy * dist_ground, gz = r + wz * dist_ground;
                                    double const gl = ALWAN_SQRT_F64(gx * gx + gy * gy + gz * gz);
                                    double const cs = (gx * os_x + gy * os_y + gz * mu_s) / gl;
                                    double gi[ALWAN__SKY_BATCH];
                                    alwan__bru_get_irr(gi, a, delta_irr, a->bottom, cs);
                                    for (q = 0; q < g; q++) inc[q] += tg[q] * a->albedo * (1.0 / ALWAN_PI_F64) * gi[q];
                                }
                                for (q = 0; q < g; q++) {
                                    rm[q] += inc[q] * (a->ray[q] * dr * pr + a->mie_s[q] * dm * pm) * domega;
                                }
                            }
                        }
                    }
                    for (q = 0; q < g; q++) density[at + q] = rm[q];
                }
            }
        }
        /* indirect irradiance of order - 1 into delta, added to the sky irradiance */
        for (j = 0; j < a->ih; j++) {
            for (i = 0; i < a->iw; i++) {
                size_t const at = ((size_t)i + (size_t)j * a->iw) * g;
                double r, mu_s, res[ALWAN__SKY_BATCH];
                double const dtheta = ALWAN_PI_F64 / 32.0, dphi = ALWAN_PI_F64 / 32.0;
                double osx, osz;
                int jj, ii;
                alwan__bru_irr_rmus_from_frag(a, i + 0.5, j + 0.5, &r, &mu_s);
                osx = ALWAN_SQRT_F64(1.0 - mu_s * mu_s);
                osz = mu_s;
                for (q = 0; q < g; q++) res[q] = 0.0;
                for (jj = 0; jj < 16; ++jj) {
                    double const theta = ((double)jj + 0.5) * dtheta;
                    for (ii = 0; ii < 64; ++ii) {
                        double const phi = ((double)ii + 0.5) * dphi;
                        double const wx = ALWAN_COS_F64(phi) * ALWAN_SIN_F64(theta);
                        double const wz = ALWAN_COS_F64(theta);
                        double const domega = dtheta * dphi * ALWAN_SIN_F64(theta);
                        double const nu = wx * osx + wz * osz;
                        alwan__bru_get_scat_order(res, a, delta_ray, delta_mie, delta_mult, r, wz, mu_s, nu, 0,
                                                  order - 1, wz * domega);
                    }
                }
                for (q = 0; q < g; q++) {
                    delta_irr[at + q] = res[q];
                    irr[at + q] += res[q];
                }
            }
        }
        /* multiple scattering of this order into delta, added to the scattering over the
         * Rayleigh phase */
        for (k = 0; k < a->nr; k++) {
            for (j = 0; j < a->nmu; j++) {
                for (i = 0; i < sw; i++) {
                    size_t const at = ((size_t)i + (size_t)j * sw + (size_t)k * sw * a->nmu) * g;
                    double r, mu, mu_s, nu, dx, sum[ALWAN__SKY_BATCH];
                    int ground, s;
                    int const n = 50;
                    alwan__bru_scat_from_frag(a, i + 0.5, j + 0.5, k + 0.5, &r, &mu, &mu_s, &nu, &ground);
                    dx = (ground ? alwan__bru_dist_bottom(a, r, mu) : alwan__bru_dist_top(a, r, mu)) / (double)n;
                    for (q = 0; q < g; q++) sum[q] = 0.0;
                    for (s = 0; s <= n; ++s) {
                        double const d = (double)s * dx;
                        double const r_i = alwan__bru_clamp_r(a, ALWAN_SQRT_F64(d * d + 2.0 * r * mu * d + r * r));
                        double const mu_i = alwan__bru_clamp_cos((r * mu + d) / r_i);
                        double const mu_s_i = alwan__bru_clamp_cos((r * mu_s + d * nu) / r_i);
                        double const w = (s == 0 || s == n) ? 0.5 : 1.0;
                        double sd[ALWAN__SKY_BATCH], t[ALWAN__SKY_BATCH];
                        alwan__bru_get_scat(sd, a, density, r_i, mu_i, mu_s_i, nu, ground, 1.0, 0);
                        alwan__bru_transmittance(t, a, trans, r, mu, d, ground);
                        for (q = 0; q < g; q++) sum[q] += sd[q] * t[q] * dx * w;
                    }
                    for (q = 0; q < g; q++) {
                        delta_mult[at + q] = sum[q];
                        scat[at + q] += sum[q] * (1.0 / alwan__bru_rayleigh_phase(nu));
                    }
                }
            }
        }
    }
    ALWAN_FREE(delta_irr);
    ALWAN_FREE(delta_ray);
    ALWAN_FREE(density);
    ALWAN_FREE(delta_mult);
    return ALWAN_OK;
}

/* Fill st's Bruneton description from the parameters (NULL: the demo Earth). */
static alwan_status alwan__sky_bruneton_describe(alwan__sky_state *st, alwan_sky_atmosphere const *p) {
    alwan_sky_atmosphere z;
    size_t k;
    if (!p) {
        memset(&z, 0, sizeof z);
        p = &z;
    }
#define ALWAN__DEF(v, d) ((v) != 0 ? (v) : (d))
    st->br_bottom = ALWAN__DEF(p->bottom_radius, 6360000.0);
    st->br_top = ALWAN__DEF(p->top_radius, 6420000.0);
    st->br_rayleigh = ALWAN__DEF(p->rayleigh_scattering, 1.24062e-6);
    st->br_rayleigh_h = ALWAN__DEF(p->rayleigh_scale_height, 8000.0);
    st->br_mie_h = ALWAN__DEF(p->mie_scale_height, 1200.0);
    st->br_mie_alpha = p->mie_angstrom_alpha;
    st->br_mie_beta = ALWAN__DEF(p->mie_angstrom_beta, 5.328e-3);
    st->br_mie_ssa = ALWAN__DEF(p->mie_single_scattering_albedo, 0.9);
    st->br_mie_g = ALWAN__DEF(p->mie_phase_g, 0.8);
    st->br_ozone_du = p->ozone_dobson == 0 ? 300.0 : p->ozone_dobson;
    st->br_max_sun_zenith = ALWAN__DEF(p->max_sun_zenith_angle, 120.0 / 180.0 * ALWAN_PI_F64);
    st->br_altitude = p->observer_altitude;
    st->br_orders = ALWAN__DEF(p->scattering_orders, 4);
    st->br_tw = ALWAN__DEF(p->transmittance_width, 256);
    st->br_th = ALWAN__DEF(p->transmittance_height, 64);
    st->br_nr = ALWAN__DEF(p->scattering_r, 32);
    st->br_nmu = ALWAN__DEF(p->scattering_mu, 128);
    st->br_nmus = ALWAN__DEF(p->scattering_mu_s, 32);
    st->br_nnu = ALWAN__DEF(p->scattering_nu, 8);
    st->br_iw = ALWAN__DEF(p->irradiance_width, 64);
    st->br_ih = ALWAN__DEF(p->irradiance_height, 16);
    st->br_spectral = p->tables == ALWAN_SKY_TABLES_SPECTRAL;
    st->br_nw = (int)ALWAN__DEF(p->wavelength_count, 15);
#undef ALWAN__DEF
    if (!(st->br_top > st->br_bottom) || !(st->br_bottom > 0.0) || st->br_orders < 1 || st->br_tw < 2 ||
        st->br_th < 2 || st->br_nr < 2 || st->br_nmu < 4 || (st->br_nmu & 1) || st->br_nmus < 2 ||
        st->br_nnu < 2 || st->br_iw < 2 || st->br_ih < 2 || st->br_nw < 1 || st->br_nw > ALWAN__SKY_MAX_WL ||
        !(st->br_altitude >= 0.0) || !(st->br_altitude < st->br_top - st->br_bottom) ||
        !(st->br_max_sun_zenith > 0.0) || !(st->br_max_sun_zenith <= ALWAN_PI_F64) ||
        (p->tables != ALWAN_SKY_TABLES_XYZ && p->tables != ALWAN_SKY_TABLES_SPECTRAL) ||
        st->br_tw > 4096 || st->br_th > 4096 || st->br_nr > 256 || st->br_nmu > 1024 || st->br_nmus > 256 ||
        st->br_nnu > 64 || st->br_iw > 4096 || st->br_ih > 4096) {
        return ALWAN_E_INVALID;
    }
    for (k = 0; k < (size_t)st->br_nw; k++) {
        double const wl = p->wavelengths ? p->wavelengths[k]
                                         : 360.0 + ((double)k + 0.5) * 470.0 / (double)st->br_nw;
        if (!(wl >= 360.0) || !(wl <= 830.0) || (k > 0 && !(wl > st->br_wl[k - 1]))) return ALWAN_E_INVALID;
        st->br_wl[k] = wl;
    }
    /* each wavelength's share of the CMF integral: the gap it spans */
    for (k = 0; k < (size_t)st->br_nw; k++) {
        double dl;
        int c;
        if (st->br_nw == 1) {
            dl = 470.0;
        } else if (!p->wavelengths) {
            dl = 470.0 / (double)st->br_nw;
        } else if (k == 0) {
            dl = st->br_wl[1] - st->br_wl[0];
        } else if (k == (size_t)st->br_nw - 1) {
            dl = st->br_wl[k] - st->br_wl[k - 1];
        } else {
            dl = 0.5 * (st->br_wl[k + 1] - st->br_wl[k - 1]);
        }
        for (c = 0; c < 3; c++) st->br_xyz_w[k][c] = alwan__sky_cmf_at(st->cmf, c, st->br_wl[k]) * dl;
    }
    return ALWAN_OK;
}

/* The precomputed tables, double, laid out as the reader reads them. */
static alwan_status alwan__sky_bruneton_build(alwan__sky_state *st, double **table_out, size_t *count_out) {
    alwan__bru a;
    int const kch = st->br_spectral ? st->br_nw : 3;
    int const tw = st->br_tw, th = st->br_th;
    int const sw = st->br_nnu * st->br_nmus;
    size_t const nt = (size_t)tw * th, ns = (size_t)sw * st->br_nmu * st->br_nr, ni = (size_t)st->br_iw * st->br_ih;
    size_t const off_t = 16;
    size_t const off_sun = off_t + (st->br_spectral ? nt * kch : 0);
    size_t const off_scat = off_sun + nt * kch;
    size_t const off_mie = off_scat + ns * kch;
    size_t const off_irr = off_mie + ns * kch;
    size_t const total = off_irr + ni * kch;
    double *tab, *ol, *trans, *irr, *scat, *mie;
    double solar_bins[48];
    int b, i, j, q;
    alwan_status rc = ALWAN_OK;

    if (total > (size_t)0x7fffffff) return ALWAN_E_RANGE;
    memset(&a, 0, sizeof a);
    a.bottom = st->br_bottom;
    a.top = st->br_top;
    a.mu_s_min = ALWAN_COS_F64(st->br_max_sun_zenith);
    a.mie_g = st->br_mie_g;
    a.sun_ar = st->sun_radius;
    a.albedo = st->albedo;
    a.lay[0][1].exp_term = 1.0;
    a.lay[0][1].exp_scale = -1.0 / st->br_rayleigh_h;
    a.lay[1][1].exp_term = 1.0;
    a.lay[1][1].exp_scale = -1.0 / st->br_mie_h;
    a.lay[2][0].width = 25000.0;
    a.lay[2][0].linear_term = 1.0 / 15000.0;
    a.lay[2][0].constant_term = -2.0 / 3.0;
    a.lay[2][1].linear_term = -1.0 / 15000.0;
    a.lay[2][1].constant_term = 8.0 / 3.0;
    a.tw = tw; a.th = th; a.nr = st->br_nr; a.nmu = st->br_nmu; a.nmus = st->br_nmus; a.nnu = st->br_nnu;
    a.iw = st->br_iw; a.ih = st->br_ih;
    a.big_a = alwan__bru_big_a(&a);
    alwan__sky_bruneton_solar_bins(solar_bins);

    tab = (double *)ALWAN_ALLOC(alwan_safe_array_size(total, sizeof(double)), sizeof(double));
    ol = (double *)ALWAN_ALLOC(alwan_safe_array_size(nt * 3, sizeof(double)), sizeof(double));
    trans = (double *)ALWAN_ALLOC(alwan_safe_array_size(nt * ALWAN__SKY_BATCH, sizeof(double)), sizeof(double));
    irr = (double *)ALWAN_ALLOC(alwan_safe_array_size(ni * ALWAN__SKY_BATCH, sizeof(double)), sizeof(double));
    scat = (double *)ALWAN_ALLOC(alwan_safe_array_size(ns * ALWAN__SKY_BATCH, sizeof(double)), sizeof(double));
    mie = (double *)ALWAN_ALLOC(alwan_safe_array_size(ns * ALWAN__SKY_BATCH, sizeof(double)), sizeof(double));
    if (!tab || !ol || !trans || !irr || !scat || !mie) {
        rc = ALWAN_E_NOMEM;
        goto done;
    }
    memset(tab, 0, sizeof(double) * total);
    tab[0] = a.bottom; tab[1] = a.top; tab[2] = a.mu_s_min; tab[3] = a.mie_g; tab[4] = a.sun_ar;
    tab[5] = kch; tab[6] = tw; tab[7] = th; tab[8] = a.nr; tab[9] = a.nmu; tab[10] = a.nmus; tab[11] = a.nnu;
    tab[12] = a.iw; tab[13] = a.ih; tab[14] = st->br_spectral ? 1.0 : 0.0;

    /* the optical lengths, the same for every wavelength */
    for (j = 0; j < th; j++) {
        for (i = 0; i < tw; i++) {
            double const x_mu = alwan__bru_unit((i + 0.5) / tw, tw);
            double const x_r = alwan__bru_unit((j + 0.5) / th, th);
            double const hh = ALWAN_SQRT_F64(a.top * a.top - a.bottom * a.bottom);
            double const rho = hh * x_r;
            double const r = ALWAN_SQRT_F64(rho * rho + a.bottom * a.bottom);
            double const d_min = a.top - r, d_max = rho + hh;
            double const d = d_min + x_mu * (d_max - d_min);
            double mu = d == 0.0 ? 1.0 : (hh * hh - rho * rho - d * d) / (2.0 * r * d);
            double *o = ol + ((size_t)i + (size_t)j * tw) * 3;
            mu = alwan__bru_clamp_cos(mu);
            for (b = 0; b < 3; b++) o[b] = alwan__bru_optical_length(&a, a.lay[b], r, mu);
        }
    }

    for (b = 0; b < st->br_nw; b += ALWAN__SKY_BATCH) {
        int const g = (st->br_nw - b) < ALWAN__SKY_BATCH ? (st->br_nw - b) : ALWAN__SKY_BATCH;
        size_t t;
        a.g = g;
        for (q = 0; q < g; q++) {
            double const wl = st->br_wl[b + q];
            double const lam = wl * 1e-3;
            double const mie_c = st->br_mie_beta / st->br_mie_h * ALWAN_POW_F64(lam, -st->br_mie_alpha);
            a.ray[q] = st->br_rayleigh * ALWAN_POW_F64(lam, -4.0);
            a.mie_s[q] = mie_c * st->br_mie_ssa;
            a.mie_e[q] = mie_c;
            a.absorb[q] = st->br_ozone_du < 0.0 ? 0.0
                        : st->br_ozone_du * 2.687e20 / 15000.0 * alwan__sky_bruneton_ozone(wl);
            a.solar[q] = alwan__sky_bruneton_solar(solar_bins, wl);
            st->br_solar[b + q] = a.solar[q];
        }
        rc = alwan__bru_precompute(&a, ol, st->br_orders, trans, irr, scat, mie);
        if (rc != ALWAN_OK) goto done;
        /* into the output channels: one each (spectral) or the CMF-weighted sum (XYZ) */
        for (q = 0; q < g; q++) {
            int const w = b + q;
            int c;
            for (c = 0; c < kch; c++) {
                double const wt = st->br_spectral ? (c == w ? 1.0 : 0.0) : st->br_xyz_w[w][c];
                if (wt == 0.0) continue;
                for (t = 0; t < nt; t++) {
                    double const tv = trans[t * g + q];
                    if (st->br_spectral) tab[off_t + t * kch + c] = tv;
                    tab[off_sun + t * kch + c] += wt * a.solar[q] * tv;
                }
                for (t = 0; t < ns; t++) {
                    tab[off_scat + t * kch + c] += wt * scat[t * g + q];
                    tab[off_mie + t * kch + c] += wt * mie[t * g + q];
                }
                for (t = 0; t < ni; t++) tab[off_irr + t * kch + c] += wt * irr[t * g + q];
            }
        }
    }
done:
    ALWAN_FREE(ol); ALWAN_FREE(trans); ALWAN_FREE(irr); ALWAN_FREE(scat); ALWAN_FREE(mie);
    if (rc != ALWAN_OK) {
        ALWAN_FREE(tab);
        return rc;
    }
    *table_out = tab;
    *count_out = total;
    return ALWAN_OK;
}

/* ================================================================
 * Shared set-up
 * ================================================================ */

static alwan_status alwan__sky_setup(alwan__sky_state *st, alwan_sky_model model, alwan_sky_params const *params,
                              alwan_ctx *ctx) {
    alwan_sky_params z;
    alwan_status rc;
    double ce, se;
    if (!params) {
        memset(&z, 0, sizeof z);
        params = &z;
    }
    memset(st, 0, sizeof *st);
    st->model = model;
    if (model != ALWAN_SKY_PREETHAM && model != ALWAN_SKY_HOSEK_WILKIE && model != ALWAN_SKY_BRUNETON) {
        return ALWAN_E_INVALID;
    }
    st->elevation = params->sun_elevation;
    st->azimuth = params->sun_azimuth;
    st->turbidity = params->turbidity != 0 ? params->turbidity : 3.0;
    st->albedo = params->ground_albedo;
    st->observer = params->observer;
    st->sun_radius = params->sun_angular_radius != 0 ? params->sun_angular_radius
                   : (model == ALWAN_SKY_BRUNETON ? 0.00935 / 2.0 : (0.51 * (ALWAN_PI_F64 / 180.0)) / 2.0);
    if (!(st->elevation >= -ALWAN_PI_F64 * 0.5) || !(st->elevation <= ALWAN_PI_F64 * 0.5) ||
        !(st->albedo >= 0.0) || !(st->albedo <= 1.0) || !(st->sun_radius > 0.0) || !(st->sun_radius < 0.5) ||
        !(st->azimuth == st->azimuth)) {
        return ALWAN_E_RANGE;
    }
    if (model != ALWAN_SKY_BRUNETON &&
        (!(st->elevation >= 0.0) || !(st->turbidity >= 1.0) || !(st->turbidity <= 10.0))) {
        return ALWAN_E_RANGE;
    }
    st->cos_sun_radius = ALWAN_COS_F64(st->sun_radius);
    ce = ALWAN_COS_F64(st->elevation);
    se = ALWAN_SIN_F64(st->elevation);
    st->sun[0] = ce * ALWAN_COS_F64(st->azimuth);
    st->sun[1] = se;
    st->sun[2] = ce * ALWAN_SIN_F64(st->azimuth);
    rc = alwan__sky_load_cmf(st->cmf, st->observer, ctx);
    if (rc != ALWAN_OK) return rc;
    if (st->observer == ALWAN_OBSERVER_CIE_1931_2DEG) {
        memcpy(st->cmf_1931, st->cmf, sizeof st->cmf);
    } else {
        rc = alwan__sky_load_cmf(st->cmf_1931, ALWAN_OBSERVER_CIE_1931_2DEG, ctx);
        if (rc != ALWAN_OK) return rc;
    }
    if (model == ALWAN_SKY_PREETHAM) return alwan__sky_preetham_init(st);
    if (model == ALWAN_SKY_HOSEK_WILKIE) {
        alwan__sky_hw_init(st);
        return ALWAN_OK;
    }
    return alwan__sky_bruneton_describe(st, params->atmosphere);
}

/* Move the sun of a made sky: the analytic models re-cook what depends on it, Bruneton's
 * tables do not depend on it at all. */
static alwan_status alwan__sky_set_sun(alwan__sky_state *st, double elevation, double azimuth) {
    double ce, se;
    if (!(elevation >= -ALWAN_PI_F64 * 0.5) || !(elevation <= ALWAN_PI_F64 * 0.5) || !(azimuth == azimuth) ||
        !(azimuth > -1e300) || !(azimuth < 1e300)) {
        return ALWAN_E_RANGE;
    }
    if (st->model != ALWAN_SKY_BRUNETON && !(elevation >= 0.0)) return ALWAN_E_RANGE;
    st->elevation = elevation;
    st->azimuth = azimuth;
    ce = ALWAN_COS_F64(elevation);
    se = ALWAN_SIN_F64(elevation);
    st->sun[0] = ce * ALWAN_COS_F64(azimuth);
    st->sun[1] = se;
    st->sun[2] = ce * ALWAN_SIN_F64(azimuth);
    if (st->model == ALWAN_SKY_PREETHAM) return alwan__sky_preetham_init(st);
    if (st->model == ALWAN_SKY_HOSEK_WILKIE) alwan__sky_hw_init(st);
    return ALWAN_OK;
}

/* The view's angle from the zenith and from the sun, for a unit direction. */
static void alwan__sky_angles(alwan__sky_state const *st, double const *d, double *cos_theta, double *gamma) {
    double c = d[0] * st->sun[0] + d[1] * st->sun[1] + d[2] * st->sun[2];
    c = c < -1.0 ? -1.0 : (c > 1.0 ? 1.0 : c);
    *cos_theta = d[1];
    *gamma = ALWAN_ACOS_F64(c);
}

/* Analytic models: spectral radiance along a unit direction. */
static double alwan__sky_analytic_spectral(alwan__sky_state const *st, double const *d, double nm, int include_sun) {
    double cos_t, gamma, v = 0.0;
    alwan__sky_angles(st, d, &cos_t, &gamma);
    if (st->model == ALWAN_SKY_PREETHAM) {
        if (cos_t > 0.0) {
            double w[3];
            alwan__sky_preetham_weights(st, cos_t, gamma, w);
            v = w[0] * (alwan__sky_basis_at(0, nm) + w[1] * alwan__sky_basis_at(1, nm) + w[2] * alwan__sky_basis_at(2, nm));
        }
        if (include_sun && gamma < st->sun_radius) v += alwan__sky_preetham_sun(st, nm);
        return v;
    }
    if (cos_t >= 0.0) {
        double const theta = ALWAN_ACOS_F64(cos_t > 1.0 ? 1.0 : cos_t);
        v = alwan__sky_hw_spectral(st, theta, gamma, nm);
        if (include_sun && gamma < st->sun_radius) {
            v += alwan__sky_hw_sun(st, nm, ALWAN_PI_F64 * 0.5 - theta, gamma);
        }
    }
    return v;
}

/* Analytic models: XYZ along a unit direction. */
static void alwan__sky_analytic_xyz(alwan__sky_state const *st, double const *d, int include_sun, double *xyz) {
    double cos_t, gamma;
    int c;
    xyz[0] = xyz[1] = xyz[2] = 0.0;
    alwan__sky_angles(st, d, &cos_t, &gamma);
    if (st->model == ALWAN_SKY_PREETHAM) {
        if (cos_t > 0.0) {
            double w[3];
            alwan__sky_preetham_weights(st, cos_t, gamma, w);
            for (c = 0; c < 3; c++) xyz[c] = w[0] * (st->pr_b[c][0] + w[1] * st->pr_b[c][1] + w[2] * st->pr_b[c][2]);
        }
        if (include_sun && gamma < st->sun_radius) {
            for (c = 0; c < 3; c++) xyz[c] += st->pr_sun_xyz[c];
        }
        return;
    }
    if (cos_t >= 0.0) {
        double const theta = ALWAN_ACOS_F64(cos_t > 1.0 ? 1.0 : cos_t);
        int b, nm;
        for (b = 0; b < ALWAN__SKY_HW_BANDS; b++) {
            double const r = alwan__sky_hw_band(st, b, theta, gamma);
            for (c = 0; c < 3; c++) xyz[c] += r * st->hw_band_xyz[b][c];
        }
        if (include_sun && gamma < st->sun_radius) {
            for (nm = 360; nm <= 720; nm++) {
                double const w = (nm == 360 || nm == 720) ? 0.5 : 1.0;
                double const s = alwan__sky_hw_sun(st, (double)nm, ALWAN_PI_F64 * 0.5 - theta, gamma) * w;
                for (c = 0; c < 3; c++) xyz[c] += s * st->cmf[c * ALWAN__SKY_CMF_N + (nm - 360)];
            }
        }
    }
}

/* G173 into an SPD. */
static alwan_status alwan__sky_g173_fill(double *values, size_t count, int col, double wmin, double wmax) {
    size_t i;
    if (!(wmin >= 280.0) || !(wmax <= 4000.0) || !(wmax >= wmin) || count == 0 || (count > 1 && !(wmax > wmin)) ||
        col < 0 || col > 2) {
        return ALWAN_E_RANGE;
    }
    for (i = 0; i < count; i++) {
        double const wl = count == 1 ? wmin : wmin + (wmax - wmin) * (double)i / (double)(count - 1);
        values[i] = alwan__sky_g173_at(col, wl);
    }
    return ALWAN_OK;
}

/* ================================================================
 * The precision entry points
 * ================================================================ */

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_sky_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_sky_impl.inc"
#include "alwan_api_teardown.h"
#endif
