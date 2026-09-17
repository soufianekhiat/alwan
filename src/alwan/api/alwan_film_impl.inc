/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Film pipeline, templated per precision. Included once per precision from
 * alwan_film.c between alwan_api_{f32,f64}_setup.h and alwan_api_teardown.h with
 * ALWAN_CORE_FILM_PROFILE set to the profile struct of that precision.
 *
 * The arithmetic follows spectral_film_lut's FilmSpectral (MIT), which is the
 * reference suite 158 holds this to: input_transform for the exposure and the
 * masking, log_exposure_to_density for the curve, printing_process and
 * compute_printer_light for the print, project for the viewing light. Sums over
 * the 41 bands are plain dot products with no bandwidth, as the model's are, and
 * run in band order so the deterministic build takes one path.
 *
 * The packed table layout is gendata/data/film_stocks.py's:
 *   [0] layers  [1] is_print  [2] is_positive  [3] curve_count  [4] log_h_min
 *   [5] log_h_max  [6..8] log_h_ref  [9..11] d_ref  [12..14] d_min  [15..17] d_max
 *   [18] gamma  [19] masking strength  [20] ISO  [21] rms  [22] exposure kelvin
 *   [23] year  [24..31] reserved  [32..40] masking matrix  [41..163] sensitivity
 *   [164..286] dye density  [287..327] D-min spectrum  [328..3399] curve
 */

#define ALWAN__FILM_OFF_MASK   32
#define ALWAN__FILM_OFF_SENS   41
#define ALWAN__FILM_OFF_DYE    164
#define ALWAN__FILM_OFF_DMIN   287
#define ALWAN__FILM_OFF_CURVE  328

/* 10^x through the routed exp, so the deterministic build stays on its polynomial. */
static ALWAN_CORE_T ALWAN_CORE_FN(film_pow10)(ALWAN_CORE_T x) {
    return ALWAN_CORE_EXP(x * ALWAN_CORE_LITERAL(2.302585092994046));
}

/* The stock's packed table and its static identity. Returns 0 for a value outside
 * the enum; a known stock compiled out leaves *table NULL. */
static int ALWAN_CORE_FN(film_table)(alwan_film_stock stock, ALWAN_CORE_T const **table, char const **name,
                                     char const **manufacturer, int *year) {
    *table = NULL;
    *name = NULL;
    *manufacturer = NULL;
    *year = 0;
    switch (stock) {
        case ALWAN_FILM_KODAK_5203:
            *name = "Kodak Vision3 50D 5203"; *manufacturer = "Kodak"; *year = 2011;
#if ALWAN_TABLE_FILM_KODAK_5203
            *table = ALWAN_CORE_FNLIT(alwan_table_film_kodak_5203);
#endif
            return 1;
        case ALWAN_FILM_KODAK_5207:
            *name = "Kodak Vision3 250D 5207"; *manufacturer = "Kodak"; *year = 2009;
#if ALWAN_TABLE_FILM_KODAK_5207
            *table = ALWAN_CORE_FNLIT(alwan_table_film_kodak_5207);
#endif
            return 1;
        case ALWAN_FILM_KODAK_5213:
            *name = "Kodak Vision3 200T 5213"; *manufacturer = "Kodak"; *year = 2010;
#if ALWAN_TABLE_FILM_KODAK_5213
            *table = ALWAN_CORE_FNLIT(alwan_table_film_kodak_5213);
#endif
            return 1;
        case ALWAN_FILM_KODAK_5219:
            *name = "Kodak Vision3 500T 5219"; *manufacturer = "Kodak"; *year = 2007;
#if ALWAN_TABLE_FILM_KODAK_5219
            *table = ALWAN_CORE_FNLIT(alwan_table_film_kodak_5219);
#endif
            return 1;
        case ALWAN_FILM_KODAK_PORTRA_400:
            *name = "Kodak Portra 400"; *manufacturer = "Kodak"; *year = 2010;
#if ALWAN_TABLE_FILM_KODAK_PORTRA_400
            *table = ALWAN_CORE_FNLIT(alwan_table_film_kodak_portra_400);
#endif
            return 1;
        case ALWAN_FILM_KODAK_EKTAR_100:
            *name = "Kodak Ektar 100"; *manufacturer = "Kodak"; *year = 2008;
#if ALWAN_TABLE_FILM_KODAK_EKTAR_100
            *table = ALWAN_CORE_FNLIT(alwan_table_film_kodak_ektar_100);
#endif
            return 1;
        case ALWAN_FILM_FUJI_ETERNA_500:
            *name = "Fuji Eterna 500"; *manufacturer = "Fujifilm"; *year = 2007;
#if ALWAN_TABLE_FILM_FUJI_ETERNA_500
            *table = ALWAN_CORE_FNLIT(alwan_table_film_fuji_eterna_500);
#endif
            return 1;
        case ALWAN_FILM_KODAK_5222:
            *name = "Kodak 5222"; *manufacturer = "Kodak"; *year = 1959;
#if ALWAN_TABLE_FILM_KODAK_5222
            *table = ALWAN_CORE_FNLIT(alwan_table_film_kodak_5222);
#endif
            return 1;
        case ALWAN_FILM_KODAK_2383:
            *name = "Kodak Vision 2383"; *manufacturer = "Kodak"; *year = 1998;
#if ALWAN_TABLE_FILM_KODAK_2383
            *table = ALWAN_CORE_FNLIT(alwan_table_film_kodak_2383);
#endif
            return 1;
        case ALWAN_FILM_KODAK_2393:
            *name = "Kodak Vision Premier 2393"; *manufacturer = "Kodak"; *year = 1998;
#if ALWAN_TABLE_FILM_KODAK_2393
            *table = ALWAN_CORE_FNLIT(alwan_table_film_kodak_2393);
#endif
            return 1;
        case ALWAN_FILM_FUJI_3513DI:
            *name = "Fuji Eterna-CP Type 3513DI"; *manufacturer = "Fujifilm"; *year = 2002;
#if ALWAN_TABLE_FILM_FUJI_3513DI
            *table = ALWAN_CORE_FNLIT(alwan_table_film_fuji_3513di);
#endif
            return 1;
        case ALWAN_FILM_KODAK_2302:
            *name = "Kodak 2302"; *manufacturer = "Kodak"; *year = 1999;
#if ALWAN_TABLE_FILM_KODAK_2302
            *table = ALWAN_CORE_FNLIT(alwan_table_film_kodak_2302);
#endif
            return 1;
        case ALWAN_FILM_KODAK_EKTACHROME_100D:
            *name = "Kodak Ektachrome 100D"; *manufacturer = "Kodak"; *year = 2018;
#if ALWAN_TABLE_FILM_KODAK_EKTACHROME_100D
            *table = ALWAN_CORE_FNLIT(alwan_table_film_kodak_ektachrome_100d);
#endif
            return 1;
        case ALWAN_FILM_FUJI_VELVIA_50:
            *name = "Fuji Velvia 50"; *manufacturer = "Fujifilm"; *year = 2007;
#if ALWAN_TABLE_FILM_FUJI_VELVIA_50
            *table = ALWAN_CORE_FNLIT(alwan_table_film_fuji_velvia_50);
#endif
            return 1;
        default:
            return 0;
    }
}

alwan_status ALWAN_CORE_FNLIT(alwan_film_get_profile)(ALWAN_CORE_FILM_PROFILE *out, alwan_film_stock stock) {
    ALWAN_CORE_T const *t;
    char const *name, *manufacturer;
    int year, i;
    if (!out) return ALWAN_E_INVALID;
    if (!ALWAN_CORE_FN(film_table)(stock, &t, &name, &manufacturer, &year)) return ALWAN_E_INVALID;
    if (!t) return ALWAN_E_NODATA;
    /* The layout is fixed by the generator; a table that does not say so is treated
     * as absent rather than read through the wrong offsets. */
    if (t[3] != (ALWAN_CORE_T)ALWAN_FILM_CURVE_COUNT) return ALWAN_E_NODATA;
    out->stock = stock;
    out->name = name;
    out->manufacturer = manufacturer;
    out->year = year;
    out->layers = t[0] > ALWAN_CORE_LITERAL(2.0) ? 3 : 1;
    out->is_print = t[1] > ALWAN_CORE_LITERAL(0.5);
    out->is_positive = t[2] > ALWAN_CORE_LITERAL(0.5);
    out->curve_count = ALWAN_FILM_CURVE_COUNT;
    out->log_h_min = t[4];
    out->log_h_max = t[5];
    for (i = 0; i < 3; i++) {
        out->log_h_ref[i] = t[6 + i];
        out->d_ref[i] = t[9 + i];
        out->d_min[i] = t[12 + i];
        out->d_max[i] = t[15 + i];
    }
    out->gamma = t[18];
    out->color_masking = t[19];
    out->iso = t[20];
    out->rms_granularity = t[21];
    out->exposure_kelvin = t[22];
    for (i = 0; i < 9; i++) out->masking[i] = t[ALWAN__FILM_OFF_MASK + i];
    out->sensitivity = t + ALWAN__FILM_OFF_SENS;
    out->dye_density = t + ALWAN__FILM_OFF_DYE;
    out->d_min_spectral = t + ALWAN__FILM_OFF_DMIN;
    out->density_curve = t + ALWAN__FILM_OFF_CURVE;
    return ALWAN_OK;
}

/* A profile a caller filled in, or one they built by hand, has every pointer and
 * the curve on the one grid size the reader is written for. */
static int ALWAN_CORE_FN(film_profile_ok)(ALWAN_CORE_FILM_PROFILE const *p) {
    return p && p->sensitivity && p->dye_density && p->d_min_spectral && p->density_curve &&
           (p->layers == 1 || p->layers == 3) && p->curve_count == ALWAN_FILM_CURVE_COUNT &&
           p->log_h_max > p->log_h_min;
}

/* Unscaled exposure of layer c: sum_k spd_k balance_k S_kc. */
static ALWAN_CORE_T ALWAN_CORE_FN(film_layer_exposure)(ALWAN_CORE_FILM_PROFILE const *p, ALWAN_CORE_T const *spd,
                                                       ALWAN_CORE_T const *balance, int c) {
    ALWAN_CORE_T sum = ALWAN_CORE_LITERAL(0.0);
    int k;
    for (k = 0; k < ALWAN_FILM_BANDS; k++) {
        ALWAN_CORE_T s = spd[k] * p->sensitivity[k * 3 + c];
        if (balance) s *= balance[k];
        sum += s;
    }
    return sum;
}

alwan_status ALWAN_CORE_FNLIT(alwan_film_expose)(ALWAN_CORE_T log_h_out[3], ALWAN_CORE_FILM_PROFILE const *profile,
                                                 ALWAN_CORE_T const *spd, ALWAN_CORE_T const *balance,
                                                 ALWAN_CORE_T const *factors) {
    int c;
    if (!log_h_out || !spd || !ALWAN_CORE_FN(film_profile_ok)(profile)) return ALWAN_E_INVALID;
    for (c = 0; c < profile->layers; c++) {
        ALWAN_CORE_T h = ALWAN_CORE_FN(film_layer_exposure)(profile, spd, balance, c);
        if (factors) h *= factors[c];
        if (h < ALWAN_CORE_LITERAL(1e-16)) h = ALWAN_CORE_LITERAL(1e-16);
        log_h_out[c] = ALWAN_CORE_LOG10(h);
    }
    for (; c < 3; c++) log_h_out[c] = log_h_out[0];
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_film_calibrate)(ALWAN_CORE_T factors_out[3], ALWAN_CORE_FILM_PROFILE const *profile,
                                                    ALWAN_CORE_T const *grey_spd, ALWAN_CORE_T const *balance) {
    int c;
    if (!factors_out || !grey_spd || !ALWAN_CORE_FN(film_profile_ok)(profile)) return ALWAN_E_INVALID;
    for (c = 0; c < profile->layers; c++) {
        ALWAN_CORE_T const h = ALWAN_CORE_FN(film_layer_exposure)(profile, grey_spd, balance, c);
        if (!(h > ALWAN_CORE_LITERAL(0.0))) return ALWAN_E_RANGE;
        factors_out[c] = ALWAN_CORE_FN(film_pow10)(profile->log_h_ref[c]) / h;
    }
    for (; c < 3; c++) factors_out[c] = factors_out[0];
    return ALWAN_OK;
}

/* The characteristic curve of layer c at a log exposure: linear on the uniform
 * grid, the end value outside it. */
static ALWAN_CORE_T ALWAN_CORE_FN(film_curve)(ALWAN_CORE_FILM_PROFILE const *p, int c, ALWAN_CORE_T log_h) {
    int const n = ALWAN_FILM_CURVE_COUNT;   /* film_profile_ok holds curve_count to this */
    ALWAN_CORE_T const *curve = p->density_curve;
    ALWAN_CORE_TABLE_CELL cell;
    ALWAN_CORE_T y0, y1;
    if (log_h <= p->log_h_min) return curve[c];
    if (log_h >= p->log_h_max) return curve[(size_t)(n - 1) * 3 + (size_t)c];
    cell = ALWAN_CORE_FNVLIT(alwan_table_cell_unit)(
        (log_h - p->log_h_min) / ((p->log_h_max - p->log_h_min) / (ALWAN_CORE_T)(n - 1)), n);
    y0 = curve[(size_t)cell.i0 * 3 + (size_t)c];
    y1 = curve[(size_t)cell.i1 * 3 + (size_t)c];
    return y0 + cell.frac * (y1 - y0);
}

alwan_status ALWAN_CORE_FNLIT(alwan_film_develop)(ALWAN_CORE_T density_out[3], ALWAN_CORE_FILM_PROFILE const *profile,
                                                  ALWAN_CORE_T const log_h[3], int masking) {
    ALWAN_CORE_T lh[3];
    int c;
    if (!density_out || !log_h || !ALWAN_CORE_FN(film_profile_ok)(profile)) return ALWAN_E_INVALID;
    if (masking && profile->layers == 3 && !profile->is_print) {
        /* image @ M.T in the reference: row i of M dotted with the log exposures. */
        for (c = 0; c < 3; c++) {
            lh[c] = profile->masking[c * 3 + 0] * log_h[0] + profile->masking[c * 3 + 1] * log_h[1] +
                    profile->masking[c * 3 + 2] * log_h[2];
        }
    } else {
        for (c = 0; c < 3; c++) lh[c] = log_h[c];
    }
    for (c = 0; c < profile->layers; c++) density_out[c] = ALWAN_CORE_FN(film_curve)(profile, c, lh[c]);
    for (; c < 3; c++) density_out[c] = density_out[0];
    return ALWAN_OK;
}

/* Dye density at band k for the activations, summed over the layers the stock has. */
static ALWAN_CORE_T ALWAN_CORE_FN(film_dye_at)(ALWAN_CORE_FILM_PROFILE const *p, ALWAN_CORE_T const density[3], int k) {
    ALWAN_CORE_T d = ALWAN_CORE_LITERAL(0.0);
    int c;
    for (c = 0; c < p->layers; c++) d += density[c] * p->dye_density[k * 3 + c];
    return d;
}

alwan_status ALWAN_CORE_FNLIT(alwan_film_transmittance)(ALWAN_CORE_T *transmittance_out,
                                                        ALWAN_CORE_FILM_PROFILE const *profile,
                                                        ALWAN_CORE_T const density[3]) {
    int k;
    if (!transmittance_out || !density || !ALWAN_CORE_FN(film_profile_ok)(profile)) return ALWAN_E_INVALID;
    for (k = 0; k < ALWAN_FILM_BANDS; k++) {
        transmittance_out[k] = ALWAN_CORE_FN(film_pow10)(
            -(ALWAN_CORE_FN(film_dye_at)(profile, density, k) + profile->d_min_spectral[k]));
    }
    return ALWAN_OK;
}

/* Solve a 3 x 3 system by Cramer's rule. Returns 0 on a singular matrix. */
static int ALWAN_CORE_FN(film_solve3)(ALWAN_CORE_T const a[9], ALWAN_CORE_T const b[3], ALWAN_CORE_T x[3]) {
    ALWAN_CORE_T const det = a[0] * (a[4] * a[8] - a[5] * a[7]) - a[1] * (a[3] * a[8] - a[5] * a[6]) +
                             a[2] * (a[3] * a[7] - a[4] * a[6]);
    ALWAN_CORE_T inv[9];
    int i;
    if (det == ALWAN_CORE_LITERAL(0.0) || det != det) return 0;
    inv[0] = (a[4] * a[8] - a[5] * a[7]) / det;
    inv[1] = (a[2] * a[7] - a[1] * a[8]) / det;
    inv[2] = (a[1] * a[5] - a[2] * a[4]) / det;
    inv[3] = (a[5] * a[6] - a[3] * a[8]) / det;
    inv[4] = (a[0] * a[8] - a[2] * a[6]) / det;
    inv[5] = (a[2] * a[3] - a[0] * a[5]) / det;
    inv[6] = (a[3] * a[7] - a[4] * a[6]) / det;
    inv[7] = (a[1] * a[6] - a[0] * a[7]) / det;
    inv[8] = (a[0] * a[4] - a[1] * a[3]) / det;
    for (i = 0; i < 3; i++) x[i] = inv[i * 3] * b[0] + inv[i * 3 + 1] * b[1] + inv[i * 3 + 2] * b[2];
    return 1;
}

alwan_status ALWAN_CORE_FNLIT(alwan_film_printer_light)(ALWAN_CORE_T *light_out, ALWAN_CORE_FILM_PROFILE const *negative,
                                                        ALWAN_CORE_FILM_PROFILE const *print, ALWAN_CORE_T red,
                                                        ALWAN_CORE_T green, ALWAN_CORE_T blue) {
    ALWAN_CORE_T grey_t[ALWAN_FILM_BANDS];
    int k;
    if (!light_out || !ALWAN_CORE_FN(film_profile_ok)(negative) || !ALWAN_CORE_FN(film_profile_ok)(print)) {
        return ALWAN_E_INVALID;
    }
    /* The negative's reference grey as a transmittance, base included. */
    for (k = 0; k < ALWAN_FILM_BANDS; k++) {
        grey_t[k] = ALWAN_CORE_FN(film_pow10)(
            -(ALWAN_CORE_FN(film_dye_at)(negative, negative->d_ref, k) + negative->d_min_spectral[k]));
    }
    if (print->layers == 1) {
        /* One layer: a flat light that lands the grey at the print's reference exposure. */
        ALWAN_CORE_T const target = ALWAN_CORE_FN(film_pow10)(green * ALWAN_CORE_LITERAL(0.3010299956639812) +
                                                              print->log_h_ref[0]);
        ALWAN_CORE_T seen = ALWAN_CORE_LITERAL(0.0);
        for (k = 0; k < ALWAN_FILM_BANDS; k++) seen += print->sensitivity[k * 3] * grey_t[k];
        if (!(seen > ALWAN_CORE_LITERAL(0.0))) return ALWAN_E_RANGE;
        for (k = 0; k < ALWAN_FILM_BANDS; k++) light_out[k] = target / seen;
        return ALWAN_OK;
    }
#if ALWAN_TABLE_FILM_PRINTER_LIGHTS
    {
        ALWAN_CORE_T const *lights = ALWAN_CORE_FNLIT(alwan_table_film_printer_lights);
        ALWAN_CORE_T a[9], target[3], x[3];
        int i, j;
        /* a[i][j]: what the print's layer i sees of printer light j through the grey. */
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                ALWAN_CORE_T sum = ALWAN_CORE_LITERAL(0.0);
                for (k = 0; k < ALWAN_FILM_BANDS; k++) {
                    sum += print->sensitivity[k * 3 + i] * (lights[k * 3 + j] * grey_t[k]);
                }
                a[i * 3 + j] = sum;
            }
        }
        target[0] = ALWAN_CORE_FN(film_pow10)(print->log_h_ref[0] + red * ALWAN_CORE_LITERAL(0.3010299956639812));
        target[1] = ALWAN_CORE_FN(film_pow10)(print->log_h_ref[1] + green * ALWAN_CORE_LITERAL(0.3010299956639812));
        target[2] = ALWAN_CORE_FN(film_pow10)(print->log_h_ref[2] + blue * ALWAN_CORE_LITERAL(0.3010299956639812));
        if (!ALWAN_CORE_FN(film_solve3)(a, target, x)) return ALWAN_E_RANGE;
        for (k = 0; k < ALWAN_FILM_BANDS; k++) {
            light_out[k] = lights[k * 3] * x[0] + lights[k * 3 + 1] * x[1] + lights[k * 3 + 2] * x[2];
        }
        return ALWAN_OK;
    }
#else
    (void)red; (void)blue;
    return ALWAN_E_NODATA;
#endif
}

alwan_status ALWAN_CORE_FNLIT(alwan_film_print)(ALWAN_CORE_T density_out[3], ALWAN_CORE_FILM_PROFILE const *negative,
                                                ALWAN_CORE_T const density[3], ALWAN_CORE_FILM_PROFILE const *print,
                                                ALWAN_CORE_T const *light) {
    ALWAN_CORE_T t[ALWAN_FILM_BANDS], log_h[3];
    int c, k;
    if (!density_out || !density || !light || !ALWAN_CORE_FN(film_profile_ok)(negative) ||
        !ALWAN_CORE_FN(film_profile_ok)(print)) {
        return ALWAN_E_INVALID;
    }
    for (k = 0; k < ALWAN_FILM_BANDS; k++) {
        t[k] = ALWAN_CORE_FN(film_pow10)(
            -(ALWAN_CORE_FN(film_dye_at)(negative, density, k) + negative->d_min_spectral[k]));
    }
    for (c = 0; c < print->layers; c++) {
        ALWAN_CORE_T h = ALWAN_CORE_LITERAL(0.0);
        for (k = 0; k < ALWAN_FILM_BANDS; k++) h += t[k] * (print->sensitivity[k * 3 + c] * light[k]);
        if (h < ALWAN_CORE_LITERAL(1e-5)) h = ALWAN_CORE_LITERAL(1e-5);
        log_h[c] = ALWAN_CORE_LOG10(h);
    }
    for (; c < 3; c++) log_h[c] = log_h[0];
    return ALWAN_CORE_FNLIT(alwan_film_develop)(density_out, print, log_h, 0);
}

alwan_status ALWAN_CORE_FNLIT(alwan_film_project)(ALWAN_CORE_XYZ *xyz_out, ALWAN_CORE_FILM_PROFILE const *profile,
                                                  ALWAN_CORE_T const density[3], ALWAN_CORE_T const *light) {
    if (!xyz_out || !density || !light || !ALWAN_CORE_FN(film_profile_ok)(profile)) return ALWAN_E_INVALID;
#if ALWAN_TABLE_CMF_CIE_1931_2DEG
    {
        ALWAN_CORE_T const *cx = ALWAN_CORE_FNLIT(alwan_table_cmf_cie_1931_2deg_x);
        ALWAN_CORE_T const *cy = ALWAN_CORE_FNLIT(alwan_table_cmf_cie_1931_2deg_y);
        ALWAN_CORE_T const *cz = ALWAN_CORE_FNLIT(alwan_table_cmf_cie_1931_2deg_z);
        ALWAN_CORE_T X = ALWAN_CORE_LITERAL(0.0), Y = ALWAN_CORE_LITERAL(0.0), Z = ALWAN_CORE_LITERAL(0.0);
        int k;
        for (k = 0; k < ALWAN_FILM_BANDS; k++) {
            /* 380 + 10 k nm on the observer's 360-830 nm at 1 nm table. */
            int const row = 20 + 10 * k;
            ALWAN_CORE_T dye = ALWAN_CORE_FN(film_dye_at)(profile, density, k);
            ALWAN_CORE_T t, w;
            if (dye < ALWAN_CORE_LITERAL(0.0)) dye = ALWAN_CORE_LITERAL(0.0);
            t = ALWAN_CORE_FN(film_pow10)(-(dye + profile->d_min_spectral[k]));
            w = t * light[k];
            X += w * ALWAN_CORE_FNVLIT(alwan_table1d_row)(cx, ALWAN_TABLE_SPD_360_830_1NM_SIZE, row);
            Y += w * ALWAN_CORE_FNVLIT(alwan_table1d_row)(cy, ALWAN_TABLE_SPD_360_830_1NM_SIZE, row);
            Z += w * ALWAN_CORE_FNVLIT(alwan_table1d_row)(cz, ALWAN_TABLE_SPD_360_830_1NM_SIZE, row);
        }
        xyz_out->x = X;
        xyz_out->y = Y;
        xyz_out->z = Z;
        return ALWAN_OK;
    }
#else
    return ALWAN_E_NODATA;
#endif
}

alwan_status ALWAN_CORE_FNLIT(alwan_film_status_density)(ALWAN_CORE_T density_out[3], ALWAN_CORE_T const *transmittance,
                                                         alwan_film_status status) {
    ALWAN_CORE_T const *table = NULL;
    int c, k;
    if (!density_out || !transmittance) return ALWAN_E_INVALID;
    switch (status) {
        case ALWAN_FILM_STATUS_A:
#if ALWAN_TABLE_FILM_STATUS_A
            table = ALWAN_CORE_FNLIT(alwan_table_film_status_a);
#endif
            break;
        case ALWAN_FILM_STATUS_M:
#if ALWAN_TABLE_FILM_STATUS_M
            table = ALWAN_CORE_FNLIT(alwan_table_film_status_m);
#endif
            break;
        case ALWAN_FILM_STATUS_APD:
#if ALWAN_TABLE_FILM_APD
            table = ALWAN_CORE_FNLIT(alwan_table_film_apd);
#endif
            break;
        default:
            return ALWAN_E_INVALID;
    }
    if (!table) return ALWAN_E_NODATA;
    for (c = 0; c < 3; c++) {
        ALWAN_CORE_T sum = ALWAN_CORE_LITERAL(0.0);
        for (k = 0; k < ALWAN_FILM_BANDS; k++) sum += transmittance[k] * table[k * 3 + c];
        if (sum < ALWAN_CORE_LITERAL(1e-16)) sum = ALWAN_CORE_LITERAL(1e-16);
        density_out[c] = -ALWAN_CORE_LOG10(sum);
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_film_render)(ALWAN_CORE_XYZ *xyz_out, ALWAN_CORE_FILM_PROFILE const *negative,
                                                 ALWAN_CORE_FILM_PROFILE const *print, ALWAN_CORE_T const *spd,
                                                 ALWAN_CORE_T const *balance, ALWAN_CORE_T const *factors,
                                                 ALWAN_CORE_T const *printer_light, ALWAN_CORE_T const *projection_light) {
    ALWAN_CORE_T log_h[3], d[3], dp[3];
    alwan_status st;
    if (!xyz_out || !spd || !projection_light || (print && !printer_light)) return ALWAN_E_INVALID;
    st = ALWAN_CORE_FNLIT(alwan_film_expose)(log_h, negative, spd, balance, factors);
    if (st != ALWAN_OK) return st;
    st = ALWAN_CORE_FNLIT(alwan_film_develop)(d, negative, log_h, negative->color_masking != ALWAN_CORE_LITERAL(0.0));
    if (st != ALWAN_OK) return st;
    if (!print) return ALWAN_CORE_FNLIT(alwan_film_project)(xyz_out, negative, d, projection_light);
    st = ALWAN_CORE_FNLIT(alwan_film_print)(dp, negative, d, print, printer_light);
    if (st != ALWAN_OK) return st;
    return ALWAN_CORE_FNLIT(alwan_film_project)(xyz_out, print, dp, projection_light);
}

#ifndef ALWAN_FILM_MAPNAME
#  define ALWAN_FILM_MAPNAME_(base, sfx) base##sfx##_map_interleave
#  define ALWAN_FILM_MAPNAME(base, sfx) ALWAN_FILM_MAPNAME_(base, sfx)
#endif

alwan_status ALWAN_FILM_MAPNAME(alwan_film_render, ALWAN_CORE_SUFFIX)(
        ALWAN_CORE_T *out, size_t out_stride, ALWAN_CORE_T const *in, size_t in_stride, size_t count,
        size_t band_count, ALWAN_CORE_FILM_PROFILE const *negative, ALWAN_CORE_FILM_PROFILE const *print,
        ALWAN_CORE_T const *balance, ALWAN_CORE_T const *factors, ALWAN_CORE_T const *printer_light,
        ALWAN_CORE_T const *projection_light) {
    size_t i, step;
    if (!out || !in || count == 0) return ALWAN_E_INVALID;
    if (band_count == ALWAN_FILM_BANDS) step = 1;
    else if (band_count == 2 * ALWAN_FILM_BANDS - 1) step = 2;
    else return ALWAN_E_INVALID;
    for (i = 0; i < count; i++) {
        ALWAN_CORE_T const *px = (ALWAN_CORE_T const *)((char const *)in + i * in_stride);
        ALWAN_CORE_T *o = (ALWAN_CORE_T *)((char *)out + i * out_stride);
        ALWAN_CORE_T spd[ALWAN_FILM_BANDS];
        ALWAN_CORE_XYZ xyz;
        alwan_status st;
        int k;
        for (k = 0; k < ALWAN_FILM_BANDS; k++) spd[k] = px[(size_t)k * step];
        st = ALWAN_CORE_FNLIT(alwan_film_render)(&xyz, negative, print, spd, balance, factors, printer_light,
                                                 projection_light);
        if (st != ALWAN_OK) return st;
        o[0] = xyz.x;
        o[1] = xyz.y;
        o[2] = xyz.z;
    }
    return ALWAN_OK;
}

/* ---- A look applied to scene-linear RGB ---------------------------------- */

/* Pixels per pass through the Jakob upsampler: 85 bands each on the stack. */
#define ALWAN__FILM_RGB_TILE 32
#define ALWAN__FILM_JAKOB_BANDS 85

/* The look's light on the film grid, every tenth sample of the 1 nm illuminant
 * table from 380 nm, normalised to Y = 1 through the observer; the grey it lights;
 * and the Y of that light through the last stock's clear base, which is the white
 * the output is relative to. */
static alwan_status ALWAN_CORE_FN(film_look_light)(alwan_illuminant which, ALWAN_CORE_FILM_PROFILE const *final,
                                                   ALWAN_CORE_T light[ALWAN_FILM_BANDS],
                                                   ALWAN_CORE_T grey[ALWAN_FILM_BANDS], ALWAN_CORE_T *white_y,
                                                   alwan_ctx *ctx) {
#if ALWAN_TABLE_CMF_CIE_1931_2DEG
    ALWAN_CORE_T const *ybar = ALWAN_CORE_FNLIT(alwan_table_cmf_cie_1931_2deg_y);
    ALWAN_CORE_SPD light_spd;
    ALWAN_CORE_T light_y = ALWAN_CORE_LITERAL(0.0), w = ALWAN_CORE_LITERAL(0.0);
    alwan_status st;
    int k;
    st = ALWAN_CORE_FNLIT(alwan_spd_illuminant)(&light_spd, which, ctx);
    if (st != ALWAN_OK) return st;
    if (light_spd.count != ALWAN_TABLE_SPD_360_830_1NM_SIZE) {
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&light_spd, ctx);
        return ALWAN_E_NODATA;
    }
    for (k = 0; k < ALWAN_FILM_BANDS; k++) {
        light[k] = light_spd.values[20 + 10 * k];
        light_y += light[k] * ALWAN_CORE_FNVLIT(alwan_table1d_row)(ybar, ALWAN_TABLE_SPD_360_830_1NM_SIZE, 20 + 10 * k);
    }
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&light_spd, ctx);
    if (!(light_y > ALWAN_CORE_LITERAL(0.0))) return ALWAN_E_RANGE;
    for (k = 0; k < ALWAN_FILM_BANDS; k++) {
        light[k] /= light_y;
        grey[k] = ALWAN_CORE_LITERAL(0.18) * light[k];
        w += ALWAN_CORE_FN(film_pow10)(-final->d_min_spectral[k]) * light[k] *
             ALWAN_CORE_FNVLIT(alwan_table1d_row)(ybar, ALWAN_TABLE_SPD_360_830_1NM_SIZE, 20 + 10 * k);
    }
    *white_y = w;
    return ALWAN_OK;
#else
    (void)which; (void)final; (void)light; (void)grey; (void)white_y; (void)ctx;
    return ALWAN_E_NODATA;
#endif
}

/* Everything a look resolves once: the two profiles, the light, the calibration, the
 * printer light and the white the output is relative to. */
typedef struct {
    ALWAN_CORE_FILM_PROFILE neg, prt;
    int has_print;
    ALWAN_CORE_T light[ALWAN_FILM_BANDS], grey[ALWAN_FILM_BANDS], printer[ALWAN_FILM_BANDS];
    ALWAN_CORE_T factors[3], white_y;
} ALWAN_CORE_FN(film_look_state);

static alwan_status ALWAN_CORE_FN(film_look_setup)(ALWAN_CORE_FN(film_look_state) *s, alwan_film_look const *look,
                                                   alwan_ctx *ctx) {
    ALWAN_CORE_T gain;
    alwan_status st;
    size_t bands = 0;
    int k;
    st = ALWAN_CORE_FNLIT(alwan_film_get_profile)(&s->neg, look->negative);
    if (st != ALWAN_OK) return st;
    s->has_print = look->print != ALWAN_FILM_NONE;
    if (s->has_print) {
        st = ALWAN_CORE_FNLIT(alwan_film_get_profile)(&s->prt, look->print);
        if (st != ALWAN_OK) return st;
    }
    /* The Jakob cubes for this gamut, and that they are 85 bands 360-780 at 5 nm. */
    st = ALWAN_FILM_MAPNAME(alwan_rgb_to_spectrum_jakob2019, ALWAN_CORE_SUFFIX)(NULL, 0, NULL, 0, 0, &bands, look->gamut);
    if (st != ALWAN_OK) return st;
    if (bands != ALWAN__FILM_JAKOB_BANDS) return ALWAN_E_NODATA;
    st = ALWAN_CORE_FN(film_look_light)(look->light, s->has_print ? &s->prt : &s->neg, s->light, s->grey, &s->white_y, ctx);
    if (st != ALWAN_OK) return st;
    st = ALWAN_CORE_FNLIT(alwan_film_calibrate)(s->factors, &s->neg, s->grey, NULL);
    if (st != ALWAN_OK) return st;
    if (!look->balance_on_grey) {
        /* One scale for every layer, the green layer's: the stock keeps its own balance. */
        s->factors[0] = s->factors[1];
        s->factors[2] = s->factors[1];
    }
    gain = ALWAN_CORE_EXP((ALWAN_CORE_T)look->stops * ALWAN_CORE_LITERAL(0.6931471805599453));
    for (k = 0; k < 3; k++) s->factors[k] *= gain;
    if (s->has_print) {
        st = ALWAN_CORE_FNLIT(alwan_film_printer_light)(s->printer, &s->neg, &s->prt, (ALWAN_CORE_T)look->red,
                                                        (ALWAN_CORE_T)look->green, (ALWAN_CORE_T)look->blue);
        if (st != ALWAN_OK) return st;
    }
    return ALWAN_OK;
}

/* One tile of RGB to linear layer exposures H, three per pixel, calibrated and pushed.
 * The same product alwan_film_expose forms, stopped before the floor and the log. */
static alwan_status ALWAN_CORE_FN(film_look_expose_tile)(ALWAN_CORE_FN(film_look_state) const *s,
                                                         alwan_film_look const *look, ALWAN_CORE_T const *rgb_in,
                                                         size_t in_stride, size_t off, size_t tile, ALWAN_CORE_T *h_out,
                                                         size_t h_stride) {
    ALWAN_CORE_T unit[ALWAN__FILM_RGB_TILE * 3], m[ALWAN__FILM_RGB_TILE];
    ALWAN_CORE_T cube[ALWAN__FILM_RGB_TILE * ALWAN__FILM_JAKOB_BANDS];
    size_t bands = ALWAN__FILM_JAKOB_BANDS, i;
    alwan_status st;
    int k, c;
    for (i = 0; i < tile; i++) {
        ALWAN_CORE_T const *px = (ALWAN_CORE_T const *)((char const *)rgb_in + (off + i) * in_stride);
        ALWAN_CORE_T r = px[0] > ALWAN_CORE_LITERAL(0.0) ? px[0] : ALWAN_CORE_LITERAL(0.0);
        ALWAN_CORE_T g = px[1] > ALWAN_CORE_LITERAL(0.0) ? px[1] : ALWAN_CORE_LITERAL(0.0);
        ALWAN_CORE_T b = px[2] > ALWAN_CORE_LITERAL(0.0) ? px[2] : ALWAN_CORE_LITERAL(0.0);
        ALWAN_CORE_T mx = r > g ? r : g;
        if (b > mx) mx = b;
        if (mx < ALWAN_CORE_LITERAL(1.0)) mx = ALWAN_CORE_LITERAL(1.0);
        m[i] = mx;
        unit[i * 3 + 0] = r / mx;
        unit[i * 3 + 1] = g / mx;
        unit[i * 3 + 2] = b / mx;
    }
    st = ALWAN_FILM_MAPNAME(alwan_rgb_to_spectrum_jakob2019, ALWAN_CORE_SUFFIX)(
        cube, ALWAN__FILM_JAKOB_BANDS * sizeof(ALWAN_CORE_T), unit, 3 * sizeof(ALWAN_CORE_T), tile, &bands, look->gamut);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < tile; i++) {
        ALWAN_CORE_T spd[ALWAN_FILM_BANDS];
        ALWAN_CORE_T *o = (ALWAN_CORE_T *)((char *)h_out + i * h_stride);
        for (k = 0; k < ALWAN_FILM_BANDS; k++) {
            /* 380 + 10 k nm is band 4 + 2 k of the 360-780 nm at 5 nm cube. */
            spd[k] = cube[i * ALWAN__FILM_JAKOB_BANDS + 4 + 2 * k] * m[i] * s->light[k];
        }
        for (c = 0; c < s->neg.layers; c++) {
            o[c] = ALWAN_CORE_FN(film_layer_exposure)(&s->neg, spd, NULL, c) * s->factors[c];
        }
        for (; c < 3; c++) o[c] = o[0];
    }
    return ALWAN_OK;
}

/* One pixel from linear layer exposures to XYZ relative to the clear base: the floor
 * and the log alwan_film_expose applies, then develop, print and project. */
static alwan_status ALWAN_CORE_FN(film_look_finish_pixel)(ALWAN_CORE_FN(film_look_state) const *s,
                                                          ALWAN_CORE_T const h[3], ALWAN_CORE_T o[3]) {
    ALWAN_CORE_T log_h[3], d[3], dp[3];
    ALWAN_CORE_XYZ xyz;
    alwan_status st;
    int c;
    for (c = 0; c < 3; c++) {
        ALWAN_CORE_T v = h[c];
        if (v < ALWAN_CORE_LITERAL(1e-16)) v = ALWAN_CORE_LITERAL(1e-16);
        log_h[c] = ALWAN_CORE_LOG10(v);
    }
    st = ALWAN_CORE_FNLIT(alwan_film_develop)(d, &s->neg, log_h, s->neg.color_masking != ALWAN_CORE_LITERAL(0.0));
    if (st != ALWAN_OK) return st;
    if (s->has_print) {
        st = ALWAN_CORE_FNLIT(alwan_film_print)(dp, &s->neg, d, &s->prt, s->printer);
        if (st != ALWAN_OK) return st;
        st = ALWAN_CORE_FNLIT(alwan_film_project)(&xyz, &s->prt, dp, s->light);
    } else {
        st = ALWAN_CORE_FNLIT(alwan_film_project)(&xyz, &s->neg, d, s->light);
    }
    if (st != ALWAN_OK) return st;
    o[0] = xyz.x / s->white_y;
    o[1] = xyz.y / s->white_y;
    o[2] = xyz.z / s->white_y;
    return ALWAN_OK;
}

alwan_status ALWAN_FILM_MAPNAME(alwan_film_look_expose, ALWAN_CORE_SUFFIX)(
        ALWAN_CORE_T *exposure_out, size_t out_stride, ALWAN_CORE_T const *rgb_in, size_t in_stride, size_t count,
        alwan_film_look const *look, alwan_ctx *ctx) {
    ALWAN_CORE_FN(film_look_state) s;
    alwan_status st;
    size_t off;
    if (!exposure_out || !rgb_in || !look || count == 0) return ALWAN_E_INVALID;
    st = ALWAN_CORE_FN(film_look_setup)(&s, look, ctx);
    if (st != ALWAN_OK) return st;
    for (off = 0; off < count; off += ALWAN__FILM_RGB_TILE) {
        size_t const tile = count - off < ALWAN__FILM_RGB_TILE ? count - off : ALWAN__FILM_RGB_TILE;
        st = ALWAN_CORE_FN(film_look_expose_tile)(&s, look, rgb_in, in_stride, off, tile,
                                                  (ALWAN_CORE_T *)((char *)exposure_out + off * out_stride), out_stride);
        if (st != ALWAN_OK) return st;
    }
    return ALWAN_OK;
}

alwan_status ALWAN_FILM_MAPNAME(alwan_film_look_finish, ALWAN_CORE_SUFFIX)(
        ALWAN_CORE_T *xyz_out, size_t out_stride, ALWAN_CORE_T const *exposure_in, size_t in_stride, size_t count,
        alwan_film_look const *look, alwan_ctx *ctx) {
    ALWAN_CORE_FN(film_look_state) s;
    alwan_status st;
    size_t i;
    if (!xyz_out || !exposure_in || !look || count == 0) return ALWAN_E_INVALID;
    st = ALWAN_CORE_FN(film_look_setup)(&s, look, ctx);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < count; i++) {
        st = ALWAN_CORE_FN(film_look_finish_pixel)(&s, (ALWAN_CORE_T const *)((char const *)exposure_in + i * in_stride),
                                                   (ALWAN_CORE_T *)((char *)xyz_out + i * out_stride));
        if (st != ALWAN_OK) return st;
    }
    return ALWAN_OK;
}

alwan_status ALWAN_FILM_MAPNAME(alwan_film_render_rgb, ALWAN_CORE_SUFFIX)(
        ALWAN_CORE_T *xyz_out, size_t out_stride, ALWAN_CORE_T const *rgb_in, size_t in_stride, size_t count,
        alwan_film_look const *look, alwan_ctx *ctx) {
    ALWAN_CORE_FN(film_look_state) s;
    alwan_status st;
    size_t off;
    if (!xyz_out || !rgb_in || !look || count == 0) return ALWAN_E_INVALID;
    st = ALWAN_CORE_FN(film_look_setup)(&s, look, ctx);
    if (st != ALWAN_OK) return st;
    for (off = 0; off < count; off += ALWAN__FILM_RGB_TILE) {
        ALWAN_CORE_T h[ALWAN__FILM_RGB_TILE * 3];
        size_t const tile = count - off < ALWAN__FILM_RGB_TILE ? count - off : ALWAN__FILM_RGB_TILE;
        size_t i;
        st = ALWAN_CORE_FN(film_look_expose_tile)(&s, look, rgb_in, in_stride, off, tile, h, 3 * sizeof(ALWAN_CORE_T));
        if (st != ALWAN_OK) return st;
        for (i = 0; i < tile; i++) {
            st = ALWAN_CORE_FN(film_look_finish_pixel)(&s, h + i * 3, (ALWAN_CORE_T *)((char *)xyz_out + (off + i) * out_stride));
            if (st != ALWAN_OK) return st;
        }
    }
    return ALWAN_OK;
}

#undef ALWAN__FILM_RGB_TILE
#undef ALWAN__FILM_JAKOB_BANDS
#undef ALWAN__FILM_OFF_MASK
#undef ALWAN__FILM_OFF_SENS
#undef ALWAN__FILM_OFF_DYE
#undef ALWAN__FILM_OFF_DMIN
#undef ALWAN__FILM_OFF_CURVE
