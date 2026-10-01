/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Physical skies, one precision. No include guard: included once per precision pass
 * from alwan_sky.c, after the shared double-precision engine.
 */

#define ALWAN_SKY_CAT2_(a, b) a##b
#define ALWAN_SKY_CAT_(a, b)  ALWAN_SKY_CAT2_(a, b)
#define ALWAN_SKY_S       struct ALWAN_SKY_CAT_(ALWAN_CORE_FNLIT(alwan_sky), _s)
#define ALWAN_SKY_T       ALWAN_CORE_FNLIT(alwan_sky)
#define ALWAN_SKY_LAYOUT  ALWAN_CORE_FNLIT(alwan_sky_layout)

ALWAN_SKY_S {
    alwan__sky_state st;
    ALWAN_CORE_T *table;   /* Bruneton's tables, as the reader reads them */
    size_t count;
};

alwan_status ALWAN_CORE_FNLIT(alwan_sky_create)(ALWAN_SKY_T **out, alwan_sky_model model, alwan_sky_params const *params,
                                                alwan_ctx *ctx) {
    ALWAN_SKY_S *s;
    alwan_status rc;
    if (!out) return ALWAN_E_INVALID;
    *out = NULL;
    s = (ALWAN_SKY_S *)ALWAN_ALLOC(sizeof(ALWAN_SKY_S), sizeof(double));
    if (!s) return ALWAN_E_NOMEM;
    memset(s, 0, sizeof *s);
    rc = alwan__sky_setup(&s->st, model, params, ctx);
    if (rc == ALWAN_OK && model == ALWAN_SKY_BRUNETON) {
        double *tab = NULL;
        size_t n = 0;
        rc = alwan__sky_bruneton_build(&s->st, &tab, &n);
        if (rc == ALWAN_OK) {
            /* the engine works in double; the table is kept in this precision */
            size_t i;
            s->table = (ALWAN_CORE_T *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(ALWAN_CORE_T)),
                                                   sizeof(ALWAN_CORE_T));
            if (!s->table) {
                rc = ALWAN_E_NOMEM;
            } else {
                for (i = 0; i < n; i++) s->table[i] = (ALWAN_CORE_T)tab[i];
            }
            ALWAN_FREE(tab);
            s->count = n;
        }
    }
    if (rc != ALWAN_OK) {
        ALWAN_FREE(s->table);
        ALWAN_FREE(s);
        return rc;
    }
    *out = (ALWAN_SKY_T *)s;
    return ALWAN_OK;
}

void ALWAN_CORE_FNLIT(alwan_sky_destroy)(ALWAN_SKY_T *sky, alwan_ctx *ctx) {
    ALWAN_SKY_S *s = (ALWAN_SKY_S *)sky;
    (void)ctx;
    if (!s) return;
    ALWAN_FREE(s->table);
    ALWAN_FREE(s);
}

alwan_status ALWAN_CORE_FNLIT(alwan_sky_sun_direction)(ALWAN_CORE_VEC3 *out, ALWAN_SKY_T const *sky) {
    ALWAN_SKY_S const *s = (ALWAN_SKY_S const *)sky;
    if (!out || !s) return ALWAN_E_INVALID;
    out->v[0] = (ALWAN_CORE_T)s->st.sun[0];
    out->v[1] = (ALWAN_CORE_T)s->st.sun[1];
    out->v[2] = (ALWAN_CORE_T)s->st.sun[2];
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_sky_set_sun)(ALWAN_SKY_T *sky, alwan_f64 elevation, alwan_f64 azimuth) {
    ALWAN_SKY_S *s = (ALWAN_SKY_S *)sky;
    if (!s) return ALWAN_E_INVALID;
    return alwan__sky_set_sun(&s->st, elevation, azimuth);
}

alwan_status ALWAN_CORE_FNLIT(alwan_sky_set_altitude)(ALWAN_SKY_T *sky, alwan_f64 altitude) {
    ALWAN_SKY_S *s = (ALWAN_SKY_S *)sky;
    if (!s || s->st.model != ALWAN_SKY_BRUNETON) return ALWAN_E_INVALID;
    if (!(altitude >= 0.0) || !(altitude < s->st.br_top - s->st.br_bottom)) return ALWAN_E_RANGE;
    s->st.br_altitude = altitude;
    return ALWAN_OK;
}

/* A unit direction from dir; 0 for a zero, infinite or NaN one. */
static int ALWAN_CORE_FNLIT(alwan__sky_unit)(double *d, ALWAN_CORE_VEC3 const *dir) {
    double const x = (double)dir->v[0], y = (double)dir->v[1], z = (double)dir->v[2];
    double const l = ALWAN_SQRT_F64(x * x + y * y + z * z);
    if (!(l > 0.0) || !(l < 1e300)) return 0;
    d[0] = x / l;
    d[1] = y / l;
    d[2] = z / l;
    return 1;
}

/* Bruneton, one channel along a unit direction: the sky (and the sun when asked and in
 * the disc), from the camera at the observer altitude. */
static double ALWAN_CORE_FNLIT(alwan__sky_bru_channel)(ALWAN_SKY_S const *s, int c, double const *d, int include_sun) {
    alwan__sky_state const *st = &s->st;
    ALWAN_CORE_T const r = (ALWAN_CORE_T)(st->br_bottom + st->br_altitude);
    ALWAN_CORE_T const mu = (ALWAN_CORE_T)d[1];
    ALWAN_CORE_T const mu_s = (ALWAN_CORE_T)st->sun[1];
    double const nu = d[0] * st->sun[0] + d[1] * st->sun[1] + d[2] * st->sun[2];
    double v = (double)ALWAN_CORE_FNVLIT(alwan_sky_atm_sky_radiance)(s->table, c, r, mu, mu_s, (ALWAN_CORE_T)nu);
    if (include_sun && nu > st->cos_sun_radius) {
        v += (double)ALWAN_CORE_FNVLIT(alwan_sky_atm_sun_radiance)(s->table, c, r, mu);
    }
    return v;
}

static double ALWAN_CORE_FNLIT(alwan__sky_bru_spectral)(ALWAN_SKY_S const *s, double nm, double const *d,
                                                        int include_sun) {
    alwan__sky_state const *st = &s->st;
    int k;
    /* Each end wavelength holds over its bin out to 360 and 830 nm, as the XYZ weights treat it. */
    if (!(nm >= 360.0) || !(nm <= 830.0)) return 0.0;
    if (nm < st->br_wl[0]) nm = st->br_wl[0];
    if (nm > st->br_wl[st->br_nw - 1]) nm = st->br_wl[st->br_nw - 1];
    if (st->br_nw == 1) return ALWAN_CORE_FNLIT(alwan__sky_bru_channel)(s, 0, d, include_sun);
    for (k = 0; k < st->br_nw - 2 && nm > st->br_wl[k + 1]; k++) { }
    {
        double const f = (nm - st->br_wl[k]) / (st->br_wl[k + 1] - st->br_wl[k]);
        double const a = ALWAN_CORE_FNLIT(alwan__sky_bru_channel)(s, k, d, include_sun);
        double const b = ALWAN_CORE_FNLIT(alwan__sky_bru_channel)(s, k + 1, d, include_sun);
        return a * (1.0 - f) + b * f;
    }
}

static void ALWAN_CORE_FNLIT(alwan__sky_xyz_d)(ALWAN_SKY_S const *s, double const *d, int include_sun, double *xyz) {
    alwan__sky_state const *st = &s->st;
    int c, k;
    if (st->model != ALWAN_SKY_BRUNETON) {
        alwan__sky_analytic_xyz(st, d, include_sun, xyz);
        return;
    }
    if (!st->br_spectral) {
        for (c = 0; c < 3; c++) xyz[c] = ALWAN_CORE_FNLIT(alwan__sky_bru_channel)(s, c, d, include_sun);
        return;
    }
    xyz[0] = xyz[1] = xyz[2] = 0.0;
    for (k = 0; k < st->br_nw; k++) {
        double const v = ALWAN_CORE_FNLIT(alwan__sky_bru_channel)(s, k, d, include_sun);
        for (c = 0; c < 3; c++) xyz[c] += v * st->br_xyz_w[k][c];
    }
}

alwan_status ALWAN_CORE_FNLIT(alwan_sky_spectral_radiance)(ALWAN_CORE_T *out, ALWAN_CORE_T const *wavelength_nm,
                                                           size_t count, ALWAN_CORE_VEC3 const *dir, int include_sun,
                                                           ALWAN_SKY_T const *sky) {
    ALWAN_SKY_S const *s = (ALWAN_SKY_S const *)sky;
    double d[3];
    size_t i;
    if (!out || !wavelength_nm || !dir || !s || count == 0) return ALWAN_E_INVALID;
    if (!ALWAN_CORE_FNLIT(alwan__sky_unit)(d, dir)) return ALWAN_E_INVALID;
    if (s->st.model == ALWAN_SKY_BRUNETON && !s->st.br_spectral) return ALWAN_E_NODATA;
    for (i = 0; i < count; i++) {
        double const nm = (double)wavelength_nm[i];
        double const v = s->st.model == ALWAN_SKY_BRUNETON
                       ? ALWAN_CORE_FNLIT(alwan__sky_bru_spectral)(s, nm, d, include_sun)
                       : alwan__sky_analytic_spectral(&s->st, d, nm, include_sun);
        out[i] = (ALWAN_CORE_T)v;
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_sky_xyz)(ALWAN_CORE_XYZ *out, ALWAN_CORE_VEC3 const *dir, int include_sun,
                                             ALWAN_SKY_T const *sky) {
    ALWAN_SKY_S const *s = (ALWAN_SKY_S const *)sky;
    double d[3], xyz[3];
    if (!out || !dir || !s) return ALWAN_E_INVALID;
    if (!ALWAN_CORE_FNLIT(alwan__sky_unit)(d, dir)) return ALWAN_E_INVALID;
    ALWAN_CORE_FNLIT(alwan__sky_xyz_d)(s, d, include_sun, xyz);
    out->x = (ALWAN_CORE_T)xyz[0];
    out->y = (ALWAN_CORE_T)xyz[1];
    out->z = (ALWAN_CORE_T)xyz[2];
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_sky_irradiance_xyz)(ALWAN_CORE_XYZ *sun_out, ALWAN_CORE_XYZ *sky_out,
                                                        ALWAN_CORE_VEC3 const *normal, ALWAN_SKY_T const *sky) {
    ALWAN_SKY_S const *s = (ALWAN_SKY_S const *)sky;
    alwan__sky_state const *st;
    double n[3], esun[3] = {0.0, 0.0, 0.0}, esky[3] = {0.0, 0.0, 0.0};
    double n_sun;
    int c;
    if (!sun_out || !sky_out || !normal || !s) return ALWAN_E_INVALID;
    if (!ALWAN_CORE_FNLIT(alwan__sky_unit)(n, normal)) return ALWAN_E_INVALID;
    st = &s->st;
    n_sun = n[0] * st->sun[0] + n[1] * st->sun[1] + n[2] * st->sun[2];
    if (st->model == ALWAN_SKY_BRUNETON) {
        ALWAN_CORE_T const r = (ALWAN_CORE_T)(st->br_bottom + st->br_altitude);
        ALWAN_CORE_T const mu_s = (ALWAN_CORE_T)st->sun[1];
        if (!st->br_spectral) {
            for (c = 0; c < 3; c++) {
                esky[c] = (double)ALWAN_CORE_FNVLIT(alwan_sky_atm_irradiance)(s->table, 0, c, r, mu_s, (ALWAN_CORE_T)n[1],
                                                                              (ALWAN_CORE_T)n_sun);
                esun[c] = (double)ALWAN_CORE_FNVLIT(alwan_sky_atm_irradiance)(s->table, 1, c, r, mu_s, (ALWAN_CORE_T)n[1],
                                                                              (ALWAN_CORE_T)n_sun);
            }
        } else {
            int k;
            for (k = 0; k < st->br_nw; k++) {
                double const vs = (double)ALWAN_CORE_FNVLIT(alwan_sky_atm_irradiance)(s->table, 0, k, r, mu_s,
                                                                                      (ALWAN_CORE_T)n[1], (ALWAN_CORE_T)n_sun);
                double const vd = (double)ALWAN_CORE_FNVLIT(alwan_sky_atm_irradiance)(s->table, 1, k, r, mu_s,
                                                                                      (ALWAN_CORE_T)n[1], (ALWAN_CORE_T)n_sun);
                for (c = 0; c < 3; c++) {
                    esky[c] += vs * st->br_xyz_w[k][c];
                    esun[c] += vd * st->br_xyz_w[k][c];
                }
            }
        }
    } else {
        /* the sky over the sphere, midpoint rule, 128 x 512 directions */
        int const nt = 128, np = 512;
        int i, j;
        double const dt = ALWAN_PI_F64 / nt, dp = 2.0 * ALWAN_PI_F64 / np;
        for (i = 0; i < nt; i++) {
            double const th = ((double)i + 0.5) * dt;
            double const sth = ALWAN_SIN_F64(th), cth = ALWAN_COS_F64(th);
            if (cth <= 0.0) continue;
            for (j = 0; j < np; j++) {
                double const ph = ((double)j + 0.5) * dp;
                double w[3], xyz[3], cosn;
                w[0] = sth * ALWAN_COS_F64(ph);
                w[1] = cth;
                w[2] = sth * ALWAN_SIN_F64(ph);
                cosn = w[0] * n[0] + w[1] * n[1] + w[2] * n[2];
                if (cosn <= 0.0) continue;
                alwan__sky_analytic_xyz(st, w, 0, xyz);
                for (c = 0; c < 3; c++) esky[c] += xyz[c] * cosn * sth * dt * dp;
            }
        }
        /* the sun over its disc: rings and spokes around its centre */
        if (st->sun[1] > -st->sun_radius) {
            double t1[3], t2[3], len;
            int const nr = 16, na = 32;
            double const da = 2.0 * ALWAN_PI_F64 / na, dg = st->sun_radius / nr;
            if (ALWAN_ABS_F64(st->sun[1]) < 0.9) {
                t1[0] = -st->sun[2]; t1[1] = 0.0; t1[2] = st->sun[0];
            } else {
                t1[0] = 0.0; t1[1] = st->sun[2]; t1[2] = -st->sun[1];
            }
            len = ALWAN_SQRT_F64(t1[0] * t1[0] + t1[1] * t1[1] + t1[2] * t1[2]);
            for (c = 0; c < 3; c++) t1[c] /= len;
            t2[0] = st->sun[1] * t1[2] - st->sun[2] * t1[1];
            t2[1] = st->sun[2] * t1[0] - st->sun[0] * t1[2];
            t2[2] = st->sun[0] * t1[1] - st->sun[1] * t1[0];
            for (i = 0; i < nr; i++) {
                double const g = ((double)i + 0.5) * dg;
                double const sg = ALWAN_SIN_F64(g), cg = ALWAN_COS_F64(g);
                for (j = 0; j < na; j++) {
                    double const a = ((double)j + 0.5) * da;
                    double w[3], xyz_all[3], xyz_sky[3], cosn;
                    for (c = 0; c < 3; c++) {
                        w[c] = cg * st->sun[c] + sg * (ALWAN_COS_F64(a) * t1[c] + ALWAN_SIN_F64(a) * t2[c]);
                    }
                    cosn = w[0] * n[0] + w[1] * n[1] + w[2] * n[2];
                    if (cosn <= 0.0) continue;
                    alwan__sky_analytic_xyz(st, w, 1, xyz_all);
                    alwan__sky_analytic_xyz(st, w, 0, xyz_sky);
                    for (c = 0; c < 3; c++) esun[c] += (xyz_all[c] - xyz_sky[c]) * cosn * sg * dg * da;
                }
            }
        }
    }
    sun_out->x = (ALWAN_CORE_T)esun[0]; sun_out->y = (ALWAN_CORE_T)esun[1]; sun_out->z = (ALWAN_CORE_T)esun[2];
    sky_out->x = (ALWAN_CORE_T)esky[0]; sky_out->y = (ALWAN_CORE_T)esky[1]; sky_out->z = (ALWAN_CORE_T)esky[2];
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_sky_bake_equirect)(ALWAN_CORE_T *out, size_t row_stride, size_t width, size_t height,
                                                       alwan_sky_bake_params const *params, ALWAN_SKY_T const *sky,
                                                       alwan_ctx *ctx) {
    ALWAN_SKY_S const *s = (ALWAN_SKY_S const *)sky;
    alwan_sky_bake_params z;
    ALWAN_CORE_FNLIT(alwan_rgb_space_desc) desc;
    size_t ch, x, y, k;
    int ns, sx, sy;
    double *acc;
    if (!out || !s || width == 0 || height == 0) return ALWAN_E_INVALID;
    if (!params) {
        memset(&z, 0, sizeof z);
        params = &z;
    }
    if (params->format == ALWAN_SKY_BAKE_XYZ || params->format == ALWAN_SKY_BAKE_RGB) {
        ch = 3;
    } else if (params->format == ALWAN_SKY_BAKE_SPECTRAL) {
        if (!params->wavelengths || params->wavelength_count == 0) return ALWAN_E_INVALID;
        if (s->st.model == ALWAN_SKY_BRUNETON && !s->st.br_spectral) return ALWAN_E_NODATA;
        ch = params->wavelength_count;
    } else {
        return ALWAN_E_INVALID;
    }
    if (row_stride < width * ch * sizeof(ALWAN_CORE_T)) return ALWAN_E_INVALID;
    if (params->format == ALWAN_SKY_BAKE_RGB) {
        alwan_status const rc = ALWAN_CORE_FNLIT(alwan_rgb_get_space_descriptor)(&desc, params->space, ctx);
        if (rc != ALWAN_OK) return rc;
    }
    ns = params->samples > 0 ? params->samples : 1;
    if (ns > 64) return ALWAN_E_RANGE;
    acc = (double *)ALWAN_ALLOC(alwan_safe_array_size(ch, sizeof(double)), sizeof(double));
    if (!acc) return ALWAN_E_NOMEM;
    for (y = 0; y < height; y++) {
        ALWAN_CORE_T *row = (ALWAN_CORE_T *)(void *)((char *)out + y * row_stride);
        for (x = 0; x < width; x++) {
            for (k = 0; k < ch; k++) acc[k] = 0.0;
            for (sy = 0; sy < ns; sy++) {
                for (sx = 0; sx < ns; sx++) {
                    double const u = ((double)x + ((double)sx + 0.5) / ns) / (double)width;
                    double const v = ((double)y + ((double)sy + 0.5) / ns) / (double)height;
                    double const th = ALWAN_PI_F64 * v, ph = 2.0 * ALWAN_PI_F64 * u;
                    double d[3];
                    d[0] = ALWAN_SIN_F64(th) * ALWAN_COS_F64(ph);
                    d[1] = ALWAN_COS_F64(th);
                    d[2] = ALWAN_SIN_F64(th) * ALWAN_SIN_F64(ph);
                    if (params->format == ALWAN_SKY_BAKE_SPECTRAL) {
                        for (k = 0; k < ch; k++) {
                            double const nm = params->wavelengths[k];
                            acc[k] += s->st.model == ALWAN_SKY_BRUNETON
                                    ? ALWAN_CORE_FNLIT(alwan__sky_bru_spectral)(s, nm, d, params->include_sun)
                                    : alwan__sky_analytic_spectral(&s->st, d, nm, params->include_sun);
                        }
                    } else {
                        double xyz[3];
                        ALWAN_CORE_FNLIT(alwan__sky_xyz_d)(s, d, params->include_sun, xyz);
                        for (k = 0; k < 3; k++) acc[k] += xyz[k];
                    }
                }
            }
            for (k = 0; k < ch; k++) acc[k] /= (double)(ns * ns);
            if (params->format == ALWAN_SKY_BAKE_RGB) {
                ALWAN_CORE_XYZ xyz;
                ALWAN_CORE_FNLIT(alwan_rgb) rgb;
                xyz.x = (ALWAN_CORE_T)acc[0];
                xyz.y = (ALWAN_CORE_T)acc[1];
                xyz.z = (ALWAN_CORE_T)acc[2];
                ALWAN_CORE_FNLIT(alwan_xyz_to_rgb)(&rgb, &desc, &xyz);
                row[x * 3 + 0] = rgb.r;
                row[x * 3 + 1] = rgb.g;
                row[x * 3 + 2] = rgb.b;
            } else {
                for (k = 0; k < ch; k++) row[x * ch + k] = (ALWAN_CORE_T)acc[k];
            }
        }
    }
    ALWAN_FREE(acc);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_sky_get_layout)(ALWAN_SKY_LAYOUT *out, ALWAN_SKY_T const *sky) {
    ALWAN_SKY_S const *s = (ALWAN_SKY_S const *)sky;
    if (!out || !s) return ALWAN_E_INVALID;
    if (s->st.model != ALWAN_SKY_BRUNETON || !s->table) return ALWAN_E_INVALID;
    out->table = s->table;
    out->count = s->count;
    out->channels = s->st.br_spectral ? (size_t)s->st.br_nw : 3;
    out->wavelengths = s->st.br_wl;
    out->wavelength_count = (size_t)s->st.br_nw;
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_spd_astm_g173)(ALWAN_CORE_SPD *out, alwan_astm_g173_column column,
                                                   ALWAN_CORE_T wavelength_min, ALWAN_CORE_T wavelength_max, size_t count,
                                                   alwan_ctx *ctx) {
    double *tmp;
    alwan_status rc;
    size_t i;
    if (!out || count == 0) return ALWAN_E_INVALID;
    if ((int)column < 0 || (int)column > 2) return ALWAN_E_INVALID;
    tmp = (double *)ALWAN_ALLOC(alwan_safe_array_size(count, sizeof(double)), sizeof(double));
    if (!tmp) return ALWAN_E_NOMEM;
    rc = alwan__sky_g173_fill(tmp, count, (int)column, (double)wavelength_min, (double)wavelength_max);
    if (rc == ALWAN_OK) rc = ALWAN_CORE_FNLIT(alwan_spd_create)(out, wavelength_min, wavelength_max, count, ctx);
    if (rc == ALWAN_OK) {
        for (i = 0; i < count; i++) out->values[i] = (ALWAN_CORE_T)tmp[i];
    }
    ALWAN_FREE(tmp);
    return rc;
}

#undef ALWAN_SKY_CAT2_
#undef ALWAN_SKY_CAT_
#undef ALWAN_SKY_S
#undef ALWAN_SKY_T
#undef ALWAN_SKY_LAYOUT
