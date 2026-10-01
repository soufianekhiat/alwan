/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Physical skies: the state shared by the two precisions (internal to alwan_sky.c).
 */

#ifndef ALWAN_SKY_INTERNAL_H
#define ALWAN_SKY_INTERNAL_H

enum {
    ALWAN__SKY_CMF_N = 471,      /* 360-830 nm at 1 nm */
    ALWAN__SKY_BATCH = 4,        /* Bruneton wavelengths precomputed together */
    ALWAN__SKY_MAX_WL = 128
};

typedef struct {
    alwan_sky_model model;
    double elevation, azimuth, turbidity, albedo, sun_radius, cos_sun_radius;
    double sun[3];
    alwan_observer_type observer;
    double cmf[3 * ALWAN__SKY_CMF_N];       /* the observer */
    double cmf_1931[3 * ALWAN__SKY_CMF_N];  /* CIE 1931 2 degree, for luminance */
    /* Preetham */
    double theta_s;
    double pr_b[3][3];      /* integral of basis S_j times the observer's CMF c */
    double pr_by[3];        /* integral of basis S_j times V(lambda) */
    double pr_sun_xyz[3];
    /* Hosek-Wilkie */
    double hw_config[11][9];
    double hw_rad[11];
    double hw_band_xyz[11][3];
    /* Bruneton */
    double br_bottom, br_top, br_rayleigh, br_rayleigh_h, br_mie_h, br_mie_alpha, br_mie_beta, br_mie_ssa, br_mie_g;
    double br_ozone_du, br_max_sun_zenith, br_altitude;
    int br_orders, br_tw, br_th, br_nr, br_nmu, br_nmus, br_nnu, br_iw, br_ih, br_spectral, br_nw;
    double br_wl[ALWAN__SKY_MAX_WL];
    double br_solar[ALWAN__SKY_MAX_WL];
    double br_xyz_w[ALWAN__SKY_MAX_WL][3];  /* each wavelength's CMF weight times its span */
} alwan__sky_state;

static alwan_status alwan__sky_load_cmf(double *cmf, alwan_observer_type observer, alwan_ctx *ctx);
static double alwan__sky_cmf_at(double const *cmf, int c, double nm);
static double alwan__sky_preetham_sun(alwan__sky_state const *st, double nm);
static double alwan__sky_hw_band(alwan__sky_state const *st, int b, double theta, double gamma);
static double alwan__sky_hw_spectral(alwan__sky_state const *st, double theta, double gamma, double wavelength);
static double alwan__sky_hw_sun(alwan__sky_state const *st, double wavelength, double elevation, double gamma);
static double alwan__sky_g173_at(int col, double nm);
static alwan_status alwan__sky_bruneton_build(alwan__sky_state *st, double **table_out, size_t *count_out);
static alwan_status alwan__sky_setup(alwan__sky_state *st, alwan_sky_model model, alwan_sky_params const *params,
                              alwan_ctx *ctx);
static void alwan__sky_angles(alwan__sky_state const *st, double const *d, double *cos_theta, double *gamma);
static double alwan__sky_analytic_spectral(alwan__sky_state const *st, double const *d, double nm, int include_sun);
static void alwan__sky_analytic_xyz(alwan__sky_state const *st, double const *d, int include_sun, double *xyz);
static alwan_status alwan__sky_g173_fill(double *values, size_t count, int col, double wmin, double wmax);

#endif /* ALWAN_SKY_INTERNAL_H */
