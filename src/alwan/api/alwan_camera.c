/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Spectral camera characterisation.
 *
 *   camera pack         52 measured cameras from rawtoaces-data (Apache-2.0),
 *                       looked up by make and model, aliases included
 *   spectral IDT        ACES P-2013-001 Method A as rawtoaces v1 solves it: white
 *                       balance multipliers and a white-preserving 3x3 to AP0,
 *                       fitted over training reflectances under an illuminant
 *   spectral to ACES    relative exposure values through the Academy RICD
 *
 * Everything here computes in f64. The f32 entry points widen their SPDs, call the
 * f64 path and narrow, as the CCM fits do: the IDT is an iterative solve whose
 * stopping tolerances sit below f32 epsilon, and the tables are f64 in every build.
 * They are ALWAN_WITH_F64_FACADE entries.
 *
 * The reference is colour-science: colour.matrix_idt and
 * colour.sd_to_aces_relative_exposure_values, fed the same bytes alwan ships, in
 * alwan_dev suite 115.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_table_core.h"
#include "../core/alwan_colorspace_core.h"
#include "../core/alwan_jzazbz_core.h"
#include "../data/alwan_data_tables.h"
#include <string.h>

/* ================================================================
 * Camera registry
 * ================================================================ */

#if ALWAN_TABLE_CAMERA_RAWTOACES
typedef struct {
    char const *make;
    char const *model;
    char const *creator;
    char const *equipment;
} alwan__camera_row;

static alwan__camera_row const k_cameras[] = {
#include "../data/camera_sensitivities/rawtoaces/cameras.inc"
};

typedef struct {
    char const *make;
    char const *model;
    size_t index;
} alwan__camera_alias;

static alwan__camera_alias const k_camera_aliases[] = {
#include "../data/camera_sensitivities/rawtoaces/aliases.inc"
};

typedef struct {
    char const *alias;
    char const *make;
} alwan__make_alias;

static alwan__make_alias const k_make_aliases[] = {
#include "../data/camera_sensitivities/rawtoaces/make_aliases.inc"
};

_Static_assert(sizeof(k_cameras) / sizeof(k_cameras[0]) == ALWAN_TABLE_RAWTOACES_CAMERAS,
               "cameras.inc and the sensitivity table must describe the same cameras");
#endif

/* ASCII case-insensitive equality: maker strings arrive as "NIKON", "Nikon" and
 * "nikon" depending on who wrote the metadata. */
static int alwan__eq_nocase(char const *a, char const *b) {
    for (;; a++, b++) {
        unsigned char ca = (unsigned char)*a;
        unsigned char cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
        if (ca == 0) return 1;
    }
}

size_t alwan_camera_count(void) {
#if ALWAN_TABLE_CAMERA_RAWTOACES
    return ALWAN_TABLE_RAWTOACES_CAMERAS;
#else
    return 0;
#endif
}

alwan_status alwan_camera_find(size_t *index_out, char const *make, char const *model) {
    if (!index_out || !make || !model) {
        return ALWAN_E_INVALID;
    }
#if !ALWAN_TABLE_CAMERA_RAWTOACES
    return ALWAN_E_NODATA;
#else
    {
        char const *canonical = make;
        size_t i;
        for (i = 0; i < sizeof(k_make_aliases) / sizeof(k_make_aliases[0]); i++) {
            if (alwan__eq_nocase(make, k_make_aliases[i].alias)) {
                canonical = k_make_aliases[i].make;
                break;
            }
        }
        for (i = 0; i < ALWAN_TABLE_RAWTOACES_CAMERAS; i++) {
            if (alwan__eq_nocase(canonical, k_cameras[i].make) && alwan__eq_nocase(model, k_cameras[i].model)) {
                *index_out = i;
                return ALWAN_OK;
            }
        }
        for (i = 0; i < sizeof(k_camera_aliases) / sizeof(k_camera_aliases[0]); i++) {
            if (alwan__eq_nocase(canonical, k_camera_aliases[i].make)
                && alwan__eq_nocase(model, k_camera_aliases[i].model)) {
                *index_out = k_camera_aliases[i].index;
                return ALWAN_OK;
            }
        }
        return ALWAN_E_NODATA;
    }
#endif
}

alwan_status alwan_camera_info(char const **make_out, char const **model_out, size_t index) {
    if (!make_out || !model_out) {
        return ALWAN_E_INVALID;
    }
#if !ALWAN_TABLE_CAMERA_RAWTOACES
    (void)index;
    return ALWAN_E_NODATA;
#else
    if (index >= ALWAN_TABLE_RAWTOACES_CAMERAS) {
        return ALWAN_E_RANGE;
    }
    *make_out = k_cameras[index].make;
    *model_out = k_cameras[index].model;
    return ALWAN_OK;
#endif
}

/* ================================================================
 * SPD helpers (f64)
 * ================================================================ */

/* Linear interpolation on an SPD's uniform grid, addressed through the table gate.
 * Outside the grid: the edge value when hold_edges is set, zero otherwise. */
static alwan_f64 alwan__spd_at(alwan_spd_f64 const *s, alwan_f64 wl, int hold_edges) {
    alwan_f64 v = ALWAN_LITERAL(0.0);
    alwan_f64 coord;
    if (s->count == 1) {
        return s->values[0];
    }
    if (!hold_edges && (wl < s->wavelength_min || wl > s->wavelength_max)) {
        return ALWAN_LITERAL(0.0);
    }
    coord = (wl - s->wavelength_min) / (s->wavelength_max - s->wavelength_min);
    if (alwan_table1d_sample_f64(&v, s->values, (int)s->count, coord, ALWAN_SAMPLE_LINEAR) != ALWAN_OK) {
        return ALWAN_LITERAL(0.0);
    }
    return v;
}

static alwan_f64 alwan__spd_wavelength(alwan_spd_f64 const *s, size_t i) {
    if (s->count <= 1) return s->wavelength_min;
    return s->wavelength_min + ((alwan_f64)i / (alwan_f64)(s->count - 1)) * (s->wavelength_max - s->wavelength_min);
}

static int alwan__spd_ok(alwan_spd_f64 const *s) {
    return s && s->values && s->count >= 2 && s->wavelength_max > s->wavelength_min;
}

static int alwan__same_grid(alwan_spd_f64 const *a, alwan_spd_f64 const *b) {
    return a->count == b->count && a->wavelength_min == b->wavelength_min && a->wavelength_max == b->wavelength_max;
}

static alwan_status alwan__spd_widen(alwan_spd_f64 *out, alwan_spd_f32 const *in, alwan_ctx *ctx) {
    alwan_status st;
    size_t i;
    if (!in || !in->values || in->count == 0) {
        return ALWAN_E_INVALID;
    }
    st = alwan_spd_create_f64(out, (alwan_f64)in->wavelength_min, (alwan_f64)in->wavelength_max, in->count, ctx);
    if (st != ALWAN_OK) {
        return st;
    }
    for (i = 0; i < in->count; i++) {
        out->values[i] = (alwan_f64)in->values[i];
    }
    return ALWAN_OK;
}

static alwan_status alwan__spd_narrow(alwan_spd_f32 *out, alwan_spd_f64 const *in, alwan_ctx *ctx) {
    alwan_status st = alwan_spd_create_f32(out, (alwan_f32)in->wavelength_min, (alwan_f32)in->wavelength_max,
                                           in->count, ctx);
    size_t i;
    if (st != ALWAN_OK) {
        return st;
    }
    for (i = 0; i < in->count; i++) {
        out->values[i] = (alwan_f32)in->values[i];
    }
    return ALWAN_OK;
}

/* ================================================================
 * The camera pack's tables as SPDs
 * ================================================================ */

static alwan_status alwan__camera_sensitivities(alwan_spd_f64 *spd_r, alwan_spd_f64 *spd_g, alwan_spd_f64 *spd_b,
                                                size_t index, alwan_ctx *ctx) {
    if (!spd_r || !spd_g || !spd_b) {
        return ALWAN_E_INVALID;
    }
#if !ALWAN_TABLE_CAMERA_RAWTOACES
    (void)index; (void)ctx;
    return ALWAN_E_NODATA;
#else
    {
        alwan_spd_f64 *out[3];
        size_t c, w;
        out[0] = spd_r; out[1] = spd_g; out[2] = spd_b;
        if (index >= ALWAN_TABLE_RAWTOACES_CAMERAS) {
            return ALWAN_E_RANGE;
        }
        for (c = 0; c < 3; c++) {
            alwan_status const st = alwan_spd_create_f64(out[c], ALWAN_LITERAL(380.0), ALWAN_LITERAL(780.0),
                                                         ALWAN_TABLE_RAWTOACES_BANDS, ctx);
            if (st != ALWAN_OK) {
                while (c > 0) { c--; alwan_spd_destroy_f64(out[c], ctx); }
                return st;
            }
        }
        for (w = 0; w < ALWAN_TABLE_RAWTOACES_BANDS; w++) {
            for (c = 0; c < 3; c++) {
                out[c]->values[w] = alwan_table2d_row_at_f64_v(alwan_table_camera_rawtoaces_f64,
                    ALWAN_TABLE_RAWTOACES_CAMERAS, ALWAN_TABLE_CAMERA_RAWTOACES_STRIDE,
                    (int)index, (int)(w * 3 + c));
            }
        }
        return ALWAN_OK;
    }
#endif
}

static alwan_status alwan__idt_training(alwan_spd_f64 *out, size_t patch_index, alwan_ctx *ctx) {
    if (!out) {
        return ALWAN_E_INVALID;
    }
#if !ALWAN_TABLE_IDT_TRAINING_190
    (void)patch_index; (void)ctx;
    return ALWAN_E_NODATA;
#else
    {
        alwan_status st;
        size_t w;
        if (patch_index >= ALWAN_TABLE_IDT_TRAINING_PATCHES) {
            return ALWAN_E_RANGE;
        }
        st = alwan_spd_create_f64(out, ALWAN_LITERAL(380.0), ALWAN_LITERAL(780.0), ALWAN_TABLE_RAWTOACES_BANDS, ctx);
        if (st != ALWAN_OK) {
            return st;
        }
        for (w = 0; w < ALWAN_TABLE_RAWTOACES_BANDS; w++) {
            out->values[w] = alwan_table2d_row_at_f64_v(alwan_table_idt_training_190_f64,
                ALWAN_TABLE_IDT_TRAINING_PATCHES, ALWAN_TABLE_RAWTOACES_BANDS, (int)patch_index, (int)w);
        }
        return ALWAN_OK;
    }
#endif
}

static alwan_status alwan__iso7589_tungsten(alwan_spd_f64 *out, alwan_ctx *ctx) {
    if (!out) {
        return ALWAN_E_INVALID;
    }
#if !ALWAN_TABLE_ISO7589_TUNGSTEN
    (void)ctx;
    return ALWAN_E_NODATA;
#else
    {
        size_t w;
        alwan_status const st = alwan_spd_create_f64(out, ALWAN_LITERAL(380.0), ALWAN_LITERAL(780.0),
                                                     ALWAN_TABLE_RAWTOACES_BANDS, ctx);
        if (st != ALWAN_OK) {
            return st;
        }
        for (w = 0; w < ALWAN_TABLE_RAWTOACES_BANDS; w++) {
            out->values[w] = alwan_table1d_row_f64_v(alwan_table_iso7589_tungsten_f64,
                                                     ALWAN_TABLE_RAWTOACES_BANDS, (int)w);
        }
        return ALWAN_OK;
    }
#endif
}

size_t alwan_idt_training_count(void) {
#if ALWAN_TABLE_IDT_TRAINING_190
    return ALWAN_TABLE_IDT_TRAINING_PATCHES;
#else
    return 0;
#endif
}

/* ================================================================
 * White balance
 * ================================================================ */

/* 1 / sum(sensitivity * illuminant) per channel, scaled so the smallest is 1. The
 * illuminant is read on the sensitivities' grid. */
static alwan_status alwan__white_balance(alwan_f64 wb[3], alwan_spd_f64 const *const sens[3],
                                         alwan_f64 const *illum_on_grid) {
    alwan_f64 w_min;
    size_t c, i;
    for (c = 0; c < 3; c++) {
        alwan_f64 sum = ALWAN_LITERAL(0.0);
        for (i = 0; i < sens[0]->count; i++) {
            sum += sens[c]->values[i] * illum_on_grid[i];
        }
        if (!(sum > ALWAN_LITERAL(0.0))) {
            return ALWAN_E_RANGE;
        }
        wb[c] = ALWAN_LITERAL(1.0) / sum;
    }
    w_min = wb[0];
    if (wb[1] < w_min) w_min = wb[1];
    if (wb[2] < w_min) w_min = wb[2];
    for (c = 0; c < 3; c++) {
        wb[c] *= ALWAN_LITERAL(1.0) / w_min;
    }
    return ALWAN_OK;
}

static alwan_status alwan__white_balance_spd(alwan_f64 wb[3], alwan_spd_f64 const *sr, alwan_spd_f64 const *sg,
                                             alwan_spd_f64 const *sb, alwan_spd_f64 const *illuminant) {
    alwan_spd_f64 const *sens[3];
    alwan_f64 *ill;
    alwan_status st;
    size_t i;
    if (!alwan__spd_ok(sr) || !alwan__spd_ok(sg) || !alwan__spd_ok(sb) || !illuminant || !illuminant->values
        || illuminant->count == 0 || !alwan__same_grid(sr, sg) || !alwan__same_grid(sr, sb)) {
        return ALWAN_E_INVALID;
    }
    ill = (alwan_f64 *)ALWAN_ALLOC(sr->count * sizeof(alwan_f64), sizeof(alwan_f64));
    if (!ill) {
        return ALWAN_E_NOMEM;
    }
    for (i = 0; i < sr->count; i++) {
        ill[i] = alwan__spd_at(illuminant, alwan__spd_wavelength(sr, i), 1);
    }
    sens[0] = sr; sens[1] = sg; sens[2] = sb;
    st = alwan__white_balance(wb, sens, ill);
    ALWAN_FREE(ill);
    return st;
}

/* ================================================================
 * The IDT solve
 * ================================================================ */

typedef struct {
    size_t n;                  /* training patches */
    alwan_f64 const *rgb;      /* n x 3, white-balanced camera RGB */
    alwan_f64 const *target;   /* n x 3, Lab or Jzazbz of the adapted training XYZ */
    alwan_f64 npm[9];          /* AP0 RGB -> XYZ */
    alwan_xyz_f64 white;       /* the ACES white, Y = 1: the Lab reference white */
    int jzazbz;
} alwan__idt_problem;

/* Six free parameters; each row's third entry makes the row sum to 1, so camera
 * white maps to ACES white. That is rawtoaces v1's constraint and colour's
 * whitepoint_preserving_matrix, and it is written as 1 - (a + b) to match it. */
static void alwan__idt_matrix(alwan_f64 const x[6], alwan_f64 m[9]) {
    m[0] = x[0]; m[1] = x[1]; m[2] = ALWAN_LITERAL(1.0) - (x[0] + x[1]);
    m[3] = x[2]; m[4] = x[3]; m[5] = ALWAN_LITERAL(1.0) - (x[2] + x[3]);
    m[6] = x[4]; m[7] = x[5]; m[8] = ALWAN_LITERAL(1.0) - (x[4] + x[5]);
}

/* rawtoaces v1: the Frobenius norm of the Lab differences over every patch.
 * Jzazbz (colour's optimisation_factory_Jzazbz): the sum of per-patch distances. */
static alwan_f64 alwan__idt_objective(alwan__idt_problem const *P, alwan_f64 const x[6]) {
    alwan_f64 m[9];
    alwan_f64 acc = ALWAN_LITERAL(0.0);
    size_t p;
    alwan__idt_matrix(x, m);
    for (p = 0; p < P->n; p++) {
        alwan_f64 const *r = P->rgb + 3 * p;
        alwan_f64 const *t = P->target + 3 * p;
        alwan_f64 const c0 = m[0] * r[0] + m[1] * r[1] + m[2] * r[2];
        alwan_f64 const c1 = m[3] * r[0] + m[4] * r[1] + m[5] * r[2];
        alwan_f64 const c2 = m[6] * r[0] + m[7] * r[1] + m[8] * r[2];
        alwan_xyz_f64 X;
        alwan_f64 d0, d1, d2;
        X.x = P->npm[0] * c0 + P->npm[1] * c1 + P->npm[2] * c2;
        X.y = P->npm[3] * c0 + P->npm[4] * c1 + P->npm[5] * c2;
        X.z = P->npm[6] * c0 + P->npm[7] * c1 + P->npm[8] * c2;
        if (P->jzazbz) {
            alwan_jzazbz_f64 const j = alwan_xyz_to_jzazbz_f64_v(X);
            d0 = j.Jz - t[0]; d1 = j.az - t[1]; d2 = j.bz - t[2];
            acc += ALWAN_SQRT(d0 * d0 + d1 * d1 + d2 * d2);
        } else {
            alwan_lab_f64 const l = alwan_xyz_to_lab_f64_v(X, P->white);
            d0 = l.L - t[0]; d1 = l.a - t[1]; d2 = l.b - t[2];
            acc += d0 * d0 + d1 * d1 + d2 * d2;
        }
    }
    return P->jzazbz ? acc : ALWAN_SQRT(acc);
}

/* Central differences with the step scipy's '3-point' scheme uses, the cube root of
 * double epsilon scaled by the parameter. */
static void alwan__idt_gradient(alwan__idt_problem const *P, alwan_f64 const x[6], alwan_f64 g[6]) {
    int j;
    for (j = 0; j < 6; j++) {
        alwan_f64 xp[6], xm[6];
        alwan_f64 const ax = ALWAN_ABS(x[j]);
        alwan_f64 const h = ALWAN_LITERAL(6.0554544523933395e-06) * (ax > ALWAN_LITERAL(1.0) ? ax : ALWAN_LITERAL(1.0));
        int k;
        for (k = 0; k < 6; k++) { xp[k] = x[k]; xm[k] = x[k]; }
        xp[j] = x[j] + h;
        xm[j] = x[j] - h;
        g[j] = (alwan__idt_objective(P, xp) - alwan__idt_objective(P, xm)) / (xp[j] - xm[j]);
    }
}

/* BFGS with an Armijo backtracking line search, from the identity. Six parameters and
 * a smooth objective: it stops when the gradient is at the finite-difference noise
 * floor, when no step decreases the objective, or at the iteration budget. Every
 * operation is a fixed sequence, so the answer is the same on every platform. */
static void alwan__idt_bfgs(alwan__idt_problem const *P, alwan_f64 x[6], int max_iterations) {
    alwan_f64 H[36];
    alwan_f64 g[6];
    alwan_f64 f;
    int it, i, j;
    for (i = 0; i < 36; i++) H[i] = ALWAN_LITERAL(0.0);
    for (i = 0; i < 6; i++) H[i * 6 + i] = ALWAN_LITERAL(1.0);
    f = alwan__idt_objective(P, x);
    alwan__idt_gradient(P, x, g);
    for (it = 0; it < max_iterations; it++) {
        alwan_f64 d[6], xn[6], gn[6], s[6], y[6], Hy[6];
        alwan_f64 gmax = ALWAN_LITERAL(0.0), gd = ALWAN_LITERAL(0.0), alpha = ALWAN_LITERAL(1.0), fn = f;
        alwan_f64 sy, yHy;
        int accepted = 0, ls;
        for (i = 0; i < 6; i++) {
            if (ALWAN_ABS(g[i]) > gmax) gmax = ALWAN_ABS(g[i]);
        }
        if (gmax < ALWAN_LITERAL(1e-12)) {
            break;
        }
        for (i = 0; i < 6; i++) {
            d[i] = ALWAN_LITERAL(0.0);
            for (j = 0; j < 6; j++) d[i] -= H[i * 6 + j] * g[j];
            gd += g[i] * d[i];
        }
        if (!(gd < ALWAN_LITERAL(0.0))) {
            /* Not a descent direction: restart from steepest descent. */
            for (i = 0; i < 36; i++) H[i] = ALWAN_LITERAL(0.0);
            for (i = 0; i < 6; i++) { H[i * 6 + i] = ALWAN_LITERAL(1.0); d[i] = -g[i]; }
            gd = ALWAN_LITERAL(0.0);
            for (i = 0; i < 6; i++) gd += g[i] * d[i];
        }
        for (ls = 0; ls < 60; ls++) {
            for (i = 0; i < 6; i++) xn[i] = x[i] + alpha * d[i];
            fn = alwan__idt_objective(P, xn);
            if (fn <= f + ALWAN_LITERAL(1e-4) * alpha * gd) {
                accepted = 1;
                break;
            }
            alpha *= ALWAN_LITERAL(0.5);
        }
        if (!accepted) {
            break;
        }
        alwan__idt_gradient(P, xn, gn);
        sy = ALWAN_LITERAL(0.0);
        for (i = 0; i < 6; i++) {
            s[i] = xn[i] - x[i];
            y[i] = gn[i] - g[i];
            sy += s[i] * y[i];
        }
        if (sy > ALWAN_LITERAL(1e-300)) {
            yHy = ALWAN_LITERAL(0.0);
            for (i = 0; i < 6; i++) {
                Hy[i] = ALWAN_LITERAL(0.0);
                for (j = 0; j < 6; j++) Hy[i] += H[i * 6 + j] * y[j];
                yHy += y[i] * Hy[i];
            }
            for (i = 0; i < 6; i++) {
                for (j = 0; j < 6; j++) {
                    H[i * 6 + j] += (sy + yHy) * s[i] * s[j] / (sy * sy)
                                  - (Hy[i] * s[j] + s[i] * Hy[j]) / sy;
                }
            }
        }
        for (i = 0; i < 6; i++) { x[i] = xn[i]; g[i] = gn[i]; }
        f = fn;
    }
}

static alwan_xyz_f64 alwan__xy_to_xyz(alwan_f64 x, alwan_f64 y) {
    alwan_xyz_f64 r;
    r.x = x / y;
    r.y = ALWAN_LITERAL(1.0);
    r.z = (ALWAN_LITERAL(1.0) - x - y) / y;
    return r;
}

static alwan_status alwan__idt_solve(alwan_f64 m_out[9], alwan_f64 wb_out[3],
                                     alwan_spd_f64 const *sr, alwan_spd_f64 const *sg, alwan_spd_f64 const *sb,
                                     alwan_spd_f64 const *illuminant,
                                     alwan_spd_f64 const *training, size_t training_count,
                                     alwan_idt_params const *params, alwan_ctx *ctx) {
    alwan_idt_params const defaults = { ALWAN_IDT_OBJECTIVE_LAB, 0, 0 };
    alwan_spd_f64 const *sens[3];
    alwan_spd_f64 cx, cy, cz;
    alwan_rgb_space_desc_f64 ap0;
    alwan__idt_problem P;
    alwan_mat3x3_f64 cat;
    alwan_xyz_f64 white_ill, white_aces;
    alwan_f64 *work, *S, *I, *C, *R, *rgb, *target;
    alwan_f64 x[6] = { 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 };
    alwan_f64 k, peak = ALWAN_LITERAL(-1.0), wx = 0.0, wy = 0.0, wz = 0.0, y_norm;
    size_t n, n_patch, i, p, c, ci = 0;
    int builtin;
    alwan_status st;

    if (!m_out || !wb_out || !alwan__spd_ok(sr) || !alwan__spd_ok(sg) || !alwan__spd_ok(sb) || !illuminant
        || !illuminant->values || illuminant->count == 0) {
        return ALWAN_E_INVALID;
    }
    if (!alwan__same_grid(sr, sg) || !alwan__same_grid(sr, sb)) {
        return ALWAN_E_INVALID;
    }
    if (!params) params = &defaults;
    if (params->objective != ALWAN_IDT_OBJECTIVE_LAB && params->objective != ALWAN_IDT_OBJECTIVE_JZAZBZ) {
        return ALWAN_E_INVALID;
    }
    builtin = (training == NULL);
    if (builtin) {
#if !ALWAN_TABLE_IDT_TRAINING_190
        return ALWAN_E_NODATA;
#else
        n_patch = ALWAN_TABLE_IDT_TRAINING_PATCHES;
#endif
    } else {
        if (training_count == 0) return ALWAN_E_INVALID;
        for (p = 0; p < training_count; p++) {
            if (!training[p].values || training[p].count == 0) return ALWAN_E_INVALID;
        }
        n_patch = training_count;
    }
    st = alwan_rgb_get_space_descriptor_f64(&ap0, ALWAN_RGB_SPACE_ACES2065_1, ctx);
    if (st != ALWAN_OK) {
        return st;
    }
    st = alwan_spd_observer_f64(&cx, &cy, &cz, ALWAN_OBSERVER_CIE_1931_2DEG, ctx);
    if (st != ALWAN_OK) {
        return st;
    }

    n = sr->count;
    work = (alwan_f64 *)ALWAN_ALLOC((n * 8 + n_patch * 6) * sizeof(alwan_f64), sizeof(alwan_f64));
    if (!work) {
        alwan_spd_destroy_f64(&cx, ctx); alwan_spd_destroy_f64(&cy, ctx); alwan_spd_destroy_f64(&cz, ctx);
        return ALWAN_E_NOMEM;
    }
    S = work;              /* n x 3 */
    C = S + n * 3;         /* n x 3 */
    I = C + n * 3;         /* n */
    R = I + n;             /* n: one training patch on the grid */
    rgb = R + n;           /* n_patch x 3 */
    target = rgb + n_patch * 3;

    sens[0] = sr; sens[1] = sg; sens[2] = sb;
    for (i = 0; i < n; i++) {
        alwan_f64 const wl = alwan__spd_wavelength(sr, i);
        for (c = 0; c < 3; c++) {
            S[i * 3 + c] = sens[c]->values[i];
        }
        C[i * 3 + 0] = alwan__spd_at(&cx, wl, 1);
        C[i * 3 + 1] = alwan__spd_at(&cy, wl, 1);
        C[i * 3 + 2] = alwan__spd_at(&cz, wl, 1);
        I[i] = alwan__spd_at(illuminant, wl, 1);
    }
    alwan_spd_destroy_f64(&cx, ctx); alwan_spd_destroy_f64(&cy, ctx); alwan_spd_destroy_f64(&cz, ctx);

    /* Normalise the illuminant by the channel with the highest peak, so the camera
     * responses are of order 1 (colour.characterisation.normalise_illuminant). */
    for (c = 0; c < 3; c++) {
        for (i = 0; i < n; i++) {
            if (S[i * 3 + c] > peak) { peak = S[i * 3 + c]; ci = c; }
        }
    }
    k = ALWAN_LITERAL(0.0);
    for (i = 0; i < n; i++) k += I[i] * S[i * 3 + ci];
    if (!(k > ALWAN_LITERAL(0.0))) { ALWAN_FREE(work); return ALWAN_E_RANGE; }
    k = ALWAN_LITERAL(1.0) / k;
    for (i = 0; i < n; i++) I[i] *= k;

    {
        alwan_spd_f64 const *const sens_c[3] = { sr, sg, sb };
        st = alwan__white_balance(wb_out, sens_c, I);
        if (st != ALWAN_OK) { ALWAN_FREE(work); return st; }
    }

    for (i = 0; i < n; i++) {
        wx += C[i * 3 + 0] * I[i];
        wy += C[i * 3 + 1] * I[i];
        wz += C[i * 3 + 2] * I[i];
    }
    if (!(wy > ALWAN_LITERAL(0.0))) { ALWAN_FREE(work); return ALWAN_E_RANGE; }
    y_norm = ALWAN_LITERAL(1.0) / wy;
    white_ill.x = wx * y_norm; white_ill.y = wy * y_norm; white_ill.z = wz * y_norm;
    white_aces = alwan__xy_to_xyz(ALWAN_ACES_WHITE_x, ALWAN_ACES_WHITE_y);
    if (!params->skip_chromatic_adaptation) {
        st = alwan_cat_matrix_f64(&cat, &white_ill, &white_aces, ALWAN_CAT_CAT02);
        if (st != ALWAN_OK) { ALWAN_FREE(work); return st; }
    }

    for (p = 0; p < n_patch; p++) {
        alwan_f64 cr = 0.0, cg = 0.0, cb = 0.0, X = 0.0, Y = 0.0, Z = 0.0;
        for (i = 0; i < n; i++) {
            alwan_f64 const wl = alwan__spd_wavelength(sr, i);
            if (builtin) {
#if ALWAN_TABLE_IDT_TRAINING_190
                R[i] = (n == ALWAN_TABLE_RAWTOACES_BANDS && sr->wavelength_min == ALWAN_LITERAL(380.0)
                        && sr->wavelength_max == ALWAN_LITERAL(780.0))
                     ? alwan_table2d_row_at_f64_v(alwan_table_idt_training_190_f64, ALWAN_TABLE_IDT_TRAINING_PATCHES,
                                                  ALWAN_TABLE_RAWTOACES_BANDS, (int)p, (int)i)
                     : 0.0;
                if (!(n == ALWAN_TABLE_RAWTOACES_BANDS && sr->wavelength_min == ALWAN_LITERAL(380.0)
                      && sr->wavelength_max == ALWAN_LITERAL(780.0))) {
                    alwan_f64 v = 0.0;
                    alwan_f64 const coord = (wl - ALWAN_LITERAL(380.0)) / ALWAN_LITERAL(400.0);
                    (void)alwan_table1d_sample_f64(&v, alwan_table_idt_training_190_f64 + p * ALWAN_TABLE_RAWTOACES_BANDS,
                                                   ALWAN_TABLE_RAWTOACES_BANDS, coord, ALWAN_SAMPLE_LINEAR);
                    R[i] = v;
                }
#endif
            } else {
                R[i] = alwan__spd_at(&training[p], wl, 1);
            }
        }
        for (i = 0; i < n; i++) {
            alwan_f64 const e = I[i] * R[i];
            cr += e * S[i * 3 + 0]; cg += e * S[i * 3 + 1]; cb += e * S[i * 3 + 2];
            X += e * C[i * 3 + 0];  Y += e * C[i * 3 + 1];  Z += e * C[i * 3 + 2];
        }
        rgb[p * 3 + 0] = cr * wb_out[0];
        rgb[p * 3 + 1] = cg * wb_out[1];
        rgb[p * 3 + 2] = cb * wb_out[2];
        {
            alwan_xyz_f64 xyz;
            xyz.x = X * y_norm; xyz.y = Y * y_norm; xyz.z = Z * y_norm;
            if (!params->skip_chromatic_adaptation) {
                alwan_xyz_f64 a;
                a.x = cat.m[0] * xyz.x + cat.m[1] * xyz.y + cat.m[2] * xyz.z;
                a.y = cat.m[3] * xyz.x + cat.m[4] * xyz.y + cat.m[5] * xyz.z;
                a.z = cat.m[6] * xyz.x + cat.m[7] * xyz.y + cat.m[8] * xyz.z;
                xyz = a;
            }
            if (params->objective == ALWAN_IDT_OBJECTIVE_JZAZBZ) {
                alwan_jzazbz_f64 const j = alwan_xyz_to_jzazbz_f64_v(xyz);
                target[p * 3 + 0] = j.Jz; target[p * 3 + 1] = j.az; target[p * 3 + 2] = j.bz;
            } else {
                alwan_lab_f64 const l = alwan_xyz_to_lab_f64_v(xyz, white_aces);
                target[p * 3 + 0] = l.L; target[p * 3 + 1] = l.a; target[p * 3 + 2] = l.b;
            }
        }
    }

    P.n = n_patch;
    P.rgb = rgb;
    P.target = target;
    for (i = 0; i < 9; i++) P.npm[i] = ap0.rgb_to_xyz.m[i];
    P.white = white_aces;
    P.jzazbz = (params->objective == ALWAN_IDT_OBJECTIVE_JZAZBZ);
    alwan__idt_bfgs(&P, x, params->max_iterations > 0 ? params->max_iterations : 500);
    alwan__idt_matrix(x, m_out);
    ALWAN_FREE(work);
    return ALWAN_OK;
}

void alwan_idt_params_init(alwan_idt_params *params) {
    if (!params) return;
    params->objective = ALWAN_IDT_OBJECTIVE_LAB;
    params->skip_chromatic_adaptation = 0;
    params->max_iterations = 0;
}

/* ================================================================
 * Spectral radiance to ACES2065-1 through the RICD
 * ================================================================ */

static alwan_status alwan__spd_to_aces(alwan_f64 out[3], alwan_spd_f64 const *spd, alwan_spd_f64 const *illuminant,
                                       int skip_chromatic_adaptation, alwan_ctx *ctx) {
    if (!out || !spd || !spd->values || spd->count == 0 || (illuminant && (!illuminant->values || illuminant->count == 0))) {
        return ALWAN_E_INVALID;
    }
#if !ALWAN_TABLE_ACES_RICD
    (void)skip_chromatic_adaptation; (void)ctx;
    return ALWAN_E_NODATA;
#else
    {
        alwan_spd_f64 d65, cx = { 0 }, cy = { 0 }, cz = { 0 };
        alwan_spd_f64 const *ill = illuminant;
        alwan_f64 kr = 0.0, kg = 0.0, kb = 0.0, er = 0.0, eg = 0.0, eb = 0.0, wx = 0.0, wy = 0.0, wz = 0.0;
        alwan_f64 E[3];
        alwan_status st;
        int i, have_cmf = 0;
        if (!ill) {
            st = alwan_spd_illuminant_f64(&d65, ALWAN_ILLUMINANT_D65, ctx);
            if (st != ALWAN_OK) return st;
            ill = &d65;
        }
        if (!skip_chromatic_adaptation) {
            st = alwan_spd_observer_f64(&cx, &cy, &cz, ALWAN_OBSERVER_CIE_1931_2DEG, ctx);
            if (st != ALWAN_OK) {
                if (ill == &d65) alwan_spd_destroy_f64(&d65, ctx);
                return st;
            }
            have_cmf = 1;
        }
        for (i = 0; i < ALWAN_TABLE_SPD_360_830_1NM_SIZE; i++) {
            alwan_f64 const wl = ALWAN_LITERAL(360.0) + (alwan_f64)i;
            alwan_f64 const s = alwan__spd_at(spd, wl, 1);
            alwan_f64 const e = alwan__spd_at(ill, wl, 1);
            alwan_f64 const rb = alwan_table1d_row_f64_v(alwan_table_aces_ricd_r_f64, ALWAN_TABLE_SPD_360_830_1NM_SIZE, i);
            alwan_f64 const gb = alwan_table1d_row_f64_v(alwan_table_aces_ricd_g_f64, ALWAN_TABLE_SPD_360_830_1NM_SIZE, i);
            alwan_f64 const bb = alwan_table1d_row_f64_v(alwan_table_aces_ricd_b_f64, ALWAN_TABLE_SPD_360_830_1NM_SIZE, i);
            kr += e * rb; kg += e * gb; kb += e * bb;
            er += e * s * rb; eg += e * s * gb; eb += e * s * bb;
            if (have_cmf) {
                wx += e * cx.values[i]; wy += e * cy.values[i]; wz += e * cz.values[i];
            }
        }
        if (ill == &d65) alwan_spd_destroy_f64(&d65, ctx);
        if (have_cmf) {
            alwan_spd_destroy_f64(&cx, ctx); alwan_spd_destroy_f64(&cy, ctx); alwan_spd_destroy_f64(&cz, ctx);
        }
        if (!(kr > 0.0) || !(kg > 0.0) || !(kb > 0.0)) {
            return ALWAN_E_RANGE;
        }
        E[0] = (ALWAN_LITERAL(1.0) / kr) * er;
        E[1] = (ALWAN_LITERAL(1.0) / kg) * eg;
        E[2] = (ALWAN_LITERAL(1.0) / kb) * eb;
        /* ACES flare: 0.5 % added, then scaled so 18 % grey stays 18 %. */
        for (i = 0; i < 3; i++) {
            E[i] += ALWAN_LITERAL(0.005);
            E[i] *= ALWAN_LITERAL(0.18) / (ALWAN_LITERAL(0.18) + ALWAN_LITERAL(0.005));
        }
        if (!skip_chromatic_adaptation) {
            /* The RICD responds under the illuminant's white. colour builds an AP0
             * space with that white, adapts its XYZ to the ACES white with CAT02 and
             * returns to the standard AP0, and so does this. */
            alwan_rgb_space_desc_f64 ap0, ap0_ill;
            alwan_mat3x3_f64 npm_ill, inv_ill, cat;
            alwan_xyz_f64 w_src, w_dst;
            alwan_f64 X[3], A[3];
            alwan_f64 const sum = wx + wy + wz;
            int r;
            if (!(sum > 0.0)) return ALWAN_E_RANGE;
            st = alwan_rgb_get_space_descriptor_f64(&ap0, ALWAN_RGB_SPACE_ACES2065_1, ctx);
            if (st != ALWAN_OK) return st;
            ap0_ill = ap0;
            ap0_ill.white_xy[0] = wx / sum;
            ap0_ill.white_xy[1] = wy / sum;
            st = alwan_rgb_derive_matrices_f64(&npm_ill, &inv_ill, &ap0_ill);
            if (st != ALWAN_OK) return st;
            w_src = alwan__xy_to_xyz(ap0_ill.white_xy[0], ap0_ill.white_xy[1]);
            w_dst = alwan__xy_to_xyz(ALWAN_ACES_WHITE_x, ALWAN_ACES_WHITE_y);
            st = alwan_cat_matrix_f64(&cat, &w_src, &w_dst, ALWAN_CAT_CAT02);
            if (st != ALWAN_OK) return st;
            for (r = 0; r < 3; r++) {
                X[r] = npm_ill.m[r * 3 + 0] * E[0] + npm_ill.m[r * 3 + 1] * E[1] + npm_ill.m[r * 3 + 2] * E[2];
            }
            for (r = 0; r < 3; r++) {
                A[r] = cat.m[r * 3 + 0] * X[0] + cat.m[r * 3 + 1] * X[1] + cat.m[r * 3 + 2] * X[2];
            }
            for (r = 0; r < 3; r++) {
                E[r] = ap0.xyz_to_rgb.m[r * 3 + 0] * A[0] + ap0.xyz_to_rgb.m[r * 3 + 1] * A[1]
                     + ap0.xyz_to_rgb.m[r * 3 + 2] * A[2];
            }
        }
        out[0] = E[0]; out[1] = E[1]; out[2] = E[2];
        return ALWAN_OK;
    }
#endif
}

/* ================================================================
 * Public entry points
 * ================================================================ */

/* The f64 bodies are compiled in every build: the f32 entry points are facades over
 * them (ALWAN_WITH_F64_FACADE), as for the CCM fits. */
#if ALWAN_WITH_F64_FACADE
alwan_status alwan_camera_sensitivities_f64(alwan_spd_f64 *spd_r, alwan_spd_f64 *spd_g, alwan_spd_f64 *spd_b,
                                            size_t index, alwan_ctx *ctx) {
    return alwan__camera_sensitivities(spd_r, spd_g, spd_b, index, ctx);
}

alwan_status alwan_spd_idt_training_f64(alwan_spd_f64 *out, size_t patch_index, alwan_ctx *ctx) {
    return alwan__idt_training(out, patch_index, ctx);
}

alwan_status alwan_spd_iso7589_tungsten_f64(alwan_spd_f64 *out, alwan_ctx *ctx) {
    return alwan__iso7589_tungsten(out, ctx);
}

alwan_status alwan_idt_white_balance_f64(alwan_rgb_f64 *white_balance_out, alwan_spd_f64 const *sens_r,
                                         alwan_spd_f64 const *sens_g, alwan_spd_f64 const *sens_b,
                                         alwan_spd_f64 const *illuminant) {
    alwan_f64 wb[3];
    alwan_status st;
    if (!white_balance_out) return ALWAN_E_INVALID;
    st = alwan__white_balance_spd(wb, sens_r, sens_g, sens_b, illuminant);
    if (st == ALWAN_OK) {
        white_balance_out->r = wb[0]; white_balance_out->g = wb[1]; white_balance_out->b = wb[2];
    }
    return st;
}

alwan_status alwan_idt_matrix_f64(alwan_mat3x3_f64 *idt_out, alwan_rgb_f64 *white_balance_out,
                                  alwan_spd_f64 const *sens_r, alwan_spd_f64 const *sens_g, alwan_spd_f64 const *sens_b,
                                  alwan_spd_f64 const *illuminant, alwan_spd_f64 const *training, size_t training_count,
                                  alwan_idt_params const *params, alwan_ctx *ctx) {
    alwan_f64 m[9], wb[3];
    alwan_status st;
    int i;
    if (!idt_out || !white_balance_out) return ALWAN_E_INVALID;
    st = alwan__idt_solve(m, wb, sens_r, sens_g, sens_b, illuminant, training, training_count, params, ctx);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < 9; i++) idt_out->m[i] = m[i];
    white_balance_out->r = wb[0]; white_balance_out->g = wb[1]; white_balance_out->b = wb[2];
    return ALWAN_OK;
}

alwan_status alwan_spd_to_aces2065_1_f64(alwan_rgb_f64 *out, alwan_spd_f64 const *spd,
                                         alwan_spd_f64 const *illuminant, int skip_chromatic_adaptation,
                                         alwan_ctx *ctx) {
    alwan_f64 e[3];
    alwan_status st;
    if (!out) return ALWAN_E_INVALID;
    st = alwan__spd_to_aces(e, spd, illuminant, skip_chromatic_adaptation, ctx);
    if (st == ALWAN_OK) { out->r = e[0]; out->g = e[1]; out->b = e[2]; }
    return st;
}
#endif /* ALWAN_WITH_F64_FACADE */

#if ALWAN_WITH_F64
alwan_status alwan_camera_rgb_to_aces2065_1_f64_map_interleave(alwan_f64 *out, size_t out_stride,
        alwan_f64 const *in, size_t in_stride, size_t count, alwan_mat3x3_f64 const *idt,
        alwan_rgb_f64 const *white_balance, alwan_f64 exposure, int clip) {
    alwan_f64 b[3], b_min;
    size_t p;
    if (!out || !in || !idt || !white_balance) return ALWAN_E_INVALID;
    b[0] = white_balance->r; b[1] = white_balance->g; b[2] = white_balance->b;
    b_min = b[0];
    if (b[1] < b_min) b_min = b[1];
    if (b[2] < b_min) b_min = b[2];
    if (!(b_min > 0.0)) return ALWAN_E_RANGE;
    for (p = 0; p < count; p++) {
        alwan_f64 const *s = (alwan_f64 const *)((char const *)in + p * in_stride);
        alwan_f64 *o = (alwan_f64 *)((char *)out + p * out_stride);
        alwan_f64 r[3];
        int c;
        for (c = 0; c < 3; c++) {
            r[c] = b[c] * s[c] / b_min;
            if (clip && r[c] > 1.0) r[c] = 1.0;
        }
        for (c = 0; c < 3; c++) {
            o[c] = exposure * (idt->m[c * 3 + 0] * r[0] + idt->m[c * 3 + 1] * r[1] + idt->m[c * 3 + 2] * r[2]);
        }
    }
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64 */

#if ALWAN_WITH_F32
alwan_status alwan_camera_sensitivities_f32(alwan_spd_f32 *spd_r, alwan_spd_f32 *spd_g, alwan_spd_f32 *spd_b,
                                            size_t index, alwan_ctx *ctx) {
    alwan_spd_f64 w[3];
    alwan_spd_f32 *out[3];
    alwan_status st;
    int c;
    if (!spd_r || !spd_g || !spd_b) return ALWAN_E_INVALID;
    st = alwan__camera_sensitivities(&w[0], &w[1], &w[2], index, ctx);
    if (st != ALWAN_OK) return st;
    out[0] = spd_r; out[1] = spd_g; out[2] = spd_b;
    for (c = 0; c < 3 && st == ALWAN_OK; c++) st = alwan__spd_narrow(out[c], &w[c], ctx);
    for (c = 0; c < 3; c++) alwan_spd_destroy_f64(&w[c], ctx);
    return st;
}

alwan_status alwan_spd_idt_training_f32(alwan_spd_f32 *out, size_t patch_index, alwan_ctx *ctx) {
    alwan_spd_f64 w;
    alwan_status st;
    if (!out) return ALWAN_E_INVALID;
    st = alwan__idt_training(&w, patch_index, ctx);
    if (st != ALWAN_OK) return st;
    st = alwan__spd_narrow(out, &w, ctx);
    alwan_spd_destroy_f64(&w, ctx);
    return st;
}

alwan_status alwan_spd_iso7589_tungsten_f32(alwan_spd_f32 *out, alwan_ctx *ctx) {
    alwan_spd_f64 w;
    alwan_status st;
    if (!out) return ALWAN_E_INVALID;
    st = alwan__iso7589_tungsten(&w, ctx);
    if (st != ALWAN_OK) return st;
    st = alwan__spd_narrow(out, &w, ctx);
    alwan_spd_destroy_f64(&w, ctx);
    return st;
}

/* Widen three sensitivities and an illuminant, the shared prologue of the f32 facades. */
static alwan_status alwan__widen4(alwan_spd_f64 w[4], alwan_spd_f32 const *a, alwan_spd_f32 const *b,
                                  alwan_spd_f32 const *c, alwan_spd_f32 const *d) {
    alwan_spd_f32 const *in[4];
    alwan_status st = ALWAN_OK;
    int i;
    in[0] = a; in[1] = b; in[2] = c; in[3] = d;
    for (i = 0; i < 4; i++) {
        st = alwan__spd_widen(&w[i], in[i], NULL);
        if (st != ALWAN_OK) {
            while (i > 0) { i--; alwan_spd_destroy_f64(&w[i], NULL); }
            return st;
        }
    }
    return ALWAN_OK;
}

alwan_status alwan_idt_white_balance_f32(alwan_rgb_f32 *white_balance_out, alwan_spd_f32 const *sens_r,
                                         alwan_spd_f32 const *sens_g, alwan_spd_f32 const *sens_b,
                                         alwan_spd_f32 const *illuminant) {
    alwan_spd_f64 w[4];
    alwan_f64 wb[3];
    alwan_status st;
    int i;
    if (!white_balance_out) return ALWAN_E_INVALID;
    st = alwan__widen4(w, sens_r, sens_g, sens_b, illuminant);
    if (st != ALWAN_OK) return st;
    st = alwan__white_balance_spd(wb, &w[0], &w[1], &w[2], &w[3]);
    for (i = 0; i < 4; i++) alwan_spd_destroy_f64(&w[i], NULL);
    if (st == ALWAN_OK) {
        white_balance_out->r = (alwan_f32)wb[0];
        white_balance_out->g = (alwan_f32)wb[1];
        white_balance_out->b = (alwan_f32)wb[2];
    }
    return st;
}

alwan_status alwan_idt_matrix_f32(alwan_mat3x3_f32 *idt_out, alwan_rgb_f32 *white_balance_out,
                                  alwan_spd_f32 const *sens_r, alwan_spd_f32 const *sens_g, alwan_spd_f32 const *sens_b,
                                  alwan_spd_f32 const *illuminant, alwan_spd_f32 const *training, size_t training_count,
                                  alwan_idt_params const *params, alwan_ctx *ctx) {
    alwan_spd_f64 w[4];
    alwan_spd_f64 *tw = NULL;
    alwan_f64 m[9], wb[3];
    alwan_status st;
    size_t p, made = 0;
    int i;
    if (!idt_out || !white_balance_out) return ALWAN_E_INVALID;
    st = alwan__widen4(w, sens_r, sens_g, sens_b, illuminant);
    if (st != ALWAN_OK) return st;
    if (training) {
        if (training_count == 0) {
            st = ALWAN_E_INVALID;
        } else {
            tw = (alwan_spd_f64 *)ALWAN_ALLOC(training_count * sizeof(alwan_spd_f64), sizeof(alwan_f64));
            if (!tw) st = ALWAN_E_NOMEM;
            for (p = 0; tw && p < training_count && st == ALWAN_OK; p++) {
                st = alwan__spd_widen(&tw[p], &training[p], ctx);
                if (st == ALWAN_OK) made++;
            }
        }
    }
    if (st == ALWAN_OK) {
        st = alwan__idt_solve(m, wb, &w[0], &w[1], &w[2], &w[3], tw, training_count, params, ctx);
    }
    for (p = 0; p < made; p++) alwan_spd_destroy_f64(&tw[p], ctx);
    if (tw) ALWAN_FREE(tw);
    for (i = 0; i < 4; i++) alwan_spd_destroy_f64(&w[i], NULL);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < 9; i++) idt_out->m[i] = (alwan_f32)m[i];
    white_balance_out->r = (alwan_f32)wb[0];
    white_balance_out->g = (alwan_f32)wb[1];
    white_balance_out->b = (alwan_f32)wb[2];
    return ALWAN_OK;
}

alwan_status alwan_spd_to_aces2065_1_f32(alwan_rgb_f32 *out, alwan_spd_f32 const *spd,
                                         alwan_spd_f32 const *illuminant, int skip_chromatic_adaptation,
                                         alwan_ctx *ctx) {
    alwan_spd_f64 s, e;
    alwan_f64 r[3];
    alwan_status st;
    if (!out) return ALWAN_E_INVALID;
    st = alwan__spd_widen(&s, spd, ctx);
    if (st != ALWAN_OK) return st;
    if (illuminant) {
        st = alwan__spd_widen(&e, illuminant, ctx);
        if (st != ALWAN_OK) { alwan_spd_destroy_f64(&s, ctx); return st; }
    }
    st = alwan__spd_to_aces(r, &s, illuminant ? &e : NULL, skip_chromatic_adaptation, ctx);
    alwan_spd_destroy_f64(&s, ctx);
    if (illuminant) alwan_spd_destroy_f64(&e, ctx);
    if (st == ALWAN_OK) { out->r = (alwan_f32)r[0]; out->g = (alwan_f32)r[1]; out->b = (alwan_f32)r[2]; }
    return st;
}

alwan_status alwan_camera_rgb_to_aces2065_1_f32_map_interleave(alwan_f32 *out, size_t out_stride,
        alwan_f32 const *in, size_t in_stride, size_t count, alwan_mat3x3_f32 const *idt,
        alwan_rgb_f32 const *white_balance, alwan_f32 exposure, int clip) {
    alwan_f32 b[3], b_min;
    size_t p;
    if (!out || !in || !idt || !white_balance) return ALWAN_E_INVALID;
    b[0] = white_balance->r; b[1] = white_balance->g; b[2] = white_balance->b;
    b_min = b[0];
    if (b[1] < b_min) b_min = b[1];
    if (b[2] < b_min) b_min = b[2];
    if (!(b_min > 0.0f)) return ALWAN_E_RANGE;
    for (p = 0; p < count; p++) {
        alwan_f32 const *s = (alwan_f32 const *)((char const *)in + p * in_stride);
        alwan_f32 *o = (alwan_f32 *)((char *)out + p * out_stride);
        alwan_f32 r[3];
        int c;
        for (c = 0; c < 3; c++) {
            r[c] = b[c] * s[c] / b_min;
            if (clip && r[c] > 1.0f) r[c] = 1.0f;
        }
        for (c = 0; c < 3; c++) {
            o[c] = exposure * (idt->m[c * 3 + 0] * r[0] + idt->m[c * 3 + 1] * r[1] + idt->m[c * 3 + 2] * r[2]);
        }
    }
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 */

/* ================================================================
 * Sensitivity recovery from a chart: Jiang, Liu, Gu and Suesstrunk 2013
 *
 * A camera photographs a chart of known reflectances under a known illuminant. Each
 * channel's sensitivity is taken as a combination of k basis vectors, and the
 * weights are the least-squares fit of the patch responses:
 *
 *     rgb_c = R diag(S) W_c x_c,    sensitivity_c = W_c x_c
 *
 * The basis is alwan's own, the principal components of the 52 rawtoaces-data
 * cameras, so recovery works for a camera that was never measured. The three
 * channels are scaled together so the largest value is 1, as colour-science does.
 * ================================================================ */

#if ALWAN_TABLE_CAMERA_BASIS
/* Least squares by Householder QR: A is m x n row-major, m >= n; A and b are destroyed. */
static int alwan__camera_lsq(double *A, size_t m, size_t n, double *b, double *x) {
    size_t k, i, j;
    for (k = 0; k < n; k++) {
        double norm2 = 0.0, norm, alpha, vk, vtv;
        for (i = k; i < m; i++) norm2 += A[i * n + k] * A[i * n + k];
        norm = ALWAN_SQRT(norm2);
        if (!(norm > 0.0)) return 0;
        alpha = A[k * n + k] > 0.0 ? -norm : norm;
        vk = A[k * n + k] - alpha;
        vtv = vk * vk + (norm2 - A[k * n + k] * A[k * n + k]);
        if (!(vtv > 0.0)) return 0;
        for (j = k + 1; j < n; j++) {
            double s = vk * A[k * n + j];
            double f;
            for (i = k + 1; i < m; i++) s += A[i * n + k] * A[i * n + j];
            f = 2.0 * s / vtv;
            A[k * n + j] -= f * vk;
            for (i = k + 1; i < m; i++) A[i * n + j] -= f * A[i * n + k];
        }
        {
            double s = vk * b[k];
            double f;
            for (i = k + 1; i < m; i++) s += A[i * n + k] * b[i];
            f = 2.0 * s / vtv;
            b[k] -= f * vk;
            for (i = k + 1; i < m; i++) b[i] -= f * A[i * n + k];
        }
        A[k * n + k] = alpha;
    }
    for (k = n; k-- > 0;) {
        double s = b[k];
        for (j = k + 1; j < n; j++) s -= A[k * n + j] * x[j];
        x[k] = s / A[k * n + k];
    }
    return 1;
}
#endif

static alwan_status alwan__sensitivities_from_chart(alwan_spd_f64 *spd_r, alwan_spd_f64 *spd_g, alwan_spd_f64 *spd_b,
                                                    alwan_f64 const *rgb, size_t rgb_stride,
                                                    alwan_spd_f64 const *refl, size_t patches,
                                                    alwan_spd_f64 const *ill, size_t k, alwan_ctx *ctx) {
    size_t p;
    if (!spd_r || !spd_g || !spd_b || !rgb || !refl || !ill || !ill->values || ill->count == 0) {
        return ALWAN_E_INVALID;
    }
    for (p = 0; p < patches; p++) {
        if (!refl[p].values || refl[p].count == 0) return ALWAN_E_INVALID;
    }
#if !ALWAN_TABLE_CAMERA_BASIS
    (void)rgb_stride; (void)k; (void)ctx;
    return ALWAN_E_NODATA;
#else
    {
        size_t const n = ALWAN_TABLE_RAWTOACES_BANDS;
        size_t const comps = ALWAN_TABLE_CAMERA_BASIS_COMPONENTS;
        alwan_spd_f64 *out[3];
        double *RS, *A, *b, *x, *X, peak = 0.0;
        alwan_status st = ALWAN_OK;
        size_t i, j, c;
        if (k == 0) k = comps;
        if (k > comps || patches < k) {
            return ALWAN_E_INVALID;
        }
        RS = (double *)ALWAN_ALLOC((patches * n + patches * k + patches + k + 3 * n) * sizeof(double), sizeof(double));
        if (!RS) return ALWAN_E_NOMEM;
        A = RS + patches * n;
        b = A + patches * k;
        x = b + patches;
        X = x + k;
        for (i = 0; i < n; i++) {
            double const wl = 380.0 + 5.0 * (double)i;
            double const s = alwan__spd_at(ill, wl, 1);
            for (p = 0; p < patches; p++) RS[p * n + i] = alwan__spd_at(&refl[p], wl, 1) * s;
        }
        for (c = 0; c < 3 && st == ALWAN_OK; c++) {
            for (p = 0; p < patches; p++) {
                alwan_f64 const *px = (alwan_f64 const *)((char const *)rgb + p * rgb_stride);
                for (j = 0; j < k; j++) {
                    double acc = 0.0;
                    for (i = 0; i < n; i++) {
                        acc += RS[p * n + i] * alwan_table1d_row_f64_v(alwan_table_camera_basis_rawtoaces_f64,
                            ALWAN_TABLE_CAMERA_BASIS_SIZE, (int)((c * comps + j) * n + i));
                    }
                    A[p * k + j] = acc;
                }
                b[p] = px[c];
            }
            if (!alwan__camera_lsq(A, patches, k, b, x)) {
                st = ALWAN_E_RANGE;
                break;
            }
            for (i = 0; i < n; i++) {
                double acc = 0.0;
                for (j = 0; j < k; j++) {
                    acc += alwan_table1d_row_f64_v(alwan_table_camera_basis_rawtoaces_f64,
                        ALWAN_TABLE_CAMERA_BASIS_SIZE, (int)((c * comps + j) * n + i)) * x[j];
                }
                X[c * n + i] = acc;
                if (acc > peak) peak = acc;
            }
        }
        if (st == ALWAN_OK && !(peak > 0.0)) st = ALWAN_E_RANGE;
        out[0] = spd_r; out[1] = spd_g; out[2] = spd_b;
        for (c = 0; c < 3 && st == ALWAN_OK; c++) {
            st = alwan_spd_create_f64(out[c], 380.0, 780.0, n, ctx);
            if (st != ALWAN_OK) {
                while (c > 0) { c--; alwan_spd_destroy_f64(out[c], ctx); }
                break;
            }
            for (i = 0; i < n; i++) out[c]->values[i] = X[c * n + i] / peak;
        }
        ALWAN_FREE(RS);
        return st;
    }
#endif
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_camera_sensitivities_from_chart_f64(alwan_spd_f64 *spd_r, alwan_spd_f64 *spd_g, alwan_spd_f64 *spd_b,
                                                       alwan_f64 const *camera_rgb, size_t rgb_stride,
                                                       alwan_spd_f64 const *reflectances, size_t patch_count,
                                                       alwan_spd_f64 const *illuminant, size_t basis_components,
                                                       alwan_ctx *ctx) {
    return alwan__sensitivities_from_chart(spd_r, spd_g, spd_b, camera_rgb, rgb_stride, reflectances, patch_count,
                                           illuminant, basis_components, ctx);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_camera_sensitivities_from_chart_f32(alwan_spd_f32 *spd_r, alwan_spd_f32 *spd_g, alwan_spd_f32 *spd_b,
                                                       alwan_f32 const *camera_rgb, size_t rgb_stride,
                                                       alwan_spd_f32 const *reflectances, size_t patch_count,
                                                       alwan_spd_f32 const *illuminant, size_t basis_components,
                                                       alwan_ctx *ctx) {
    alwan_spd_f64 *refl = NULL;
    alwan_spd_f64 ill = { 0 };
    alwan_spd_f64 out[3] = { { 0 } };
    alwan_spd_f32 *dst[3];
    alwan_f64 *rgb = NULL;
    alwan_status st = ALWAN_OK;
    size_t p, made = 0;
    int c;
    if (!spd_r || !spd_g || !spd_b || !camera_rgb || !reflectances || !illuminant || patch_count == 0) {
        return ALWAN_E_INVALID;
    }
    refl = (alwan_spd_f64 *)ALWAN_ALLOC(patch_count * sizeof(alwan_spd_f64), sizeof(double));
    rgb = (alwan_f64 *)ALWAN_ALLOC(patch_count * 3 * sizeof(alwan_f64), sizeof(double));
    if (!refl || !rgb) st = ALWAN_E_NOMEM;
    for (p = 0; p < patch_count && st == ALWAN_OK; p++) {
        alwan_f32 const *px = (alwan_f32 const *)((char const *)camera_rgb + p * rgb_stride);
        rgb[3 * p + 0] = (alwan_f64)px[0];
        rgb[3 * p + 1] = (alwan_f64)px[1];
        rgb[3 * p + 2] = (alwan_f64)px[2];
        st = alwan__spd_widen(&refl[p], &reflectances[p], ctx);
        if (st == ALWAN_OK) made++;
    }
    if (st == ALWAN_OK) st = alwan__spd_widen(&ill, illuminant, ctx);
    if (st == ALWAN_OK) {
        st = alwan__sensitivities_from_chart(&out[0], &out[1], &out[2], rgb, 3 * sizeof(alwan_f64), refl,
                                             patch_count, &ill, basis_components, ctx);
        alwan_spd_destroy_f64(&ill, ctx);
        if (st == ALWAN_OK) {
            dst[0] = spd_r; dst[1] = spd_g; dst[2] = spd_b;
            for (c = 0; c < 3; c++) {
                if (st == ALWAN_OK) st = alwan__spd_narrow(dst[c], &out[c], ctx);
                alwan_spd_destroy_f64(&out[c], ctx);
            }
        }
    }
    for (p = 0; p < made; p++) alwan_spd_destroy_f64(&refl[p], ctx);
    if (refl) ALWAN_FREE(refl);
    if (rgb) ALWAN_FREE(rgb);
    return st;
}
#endif /* ALWAN_WITH_F32 */
