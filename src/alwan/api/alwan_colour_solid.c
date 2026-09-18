/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * The optimal colour solid, and whether a tristimulus is inside it.
 *
 * Every reflectance a surface can have lies in [0, 1] at each wavelength, so
 * every colour a surface can be, under a given light and observer, lies in
 *
 *     { sum_i R_i w_i : 0 <= R_i <= 1 },   w_i = k S_i xbar_i
 *
 * with k chosen so a perfect reflector reads Y = 1. That set is a ZONOTOPE:
 * the image of a cube under a linear map. Naming it is not decoration, it is
 * what makes the code short and exact.
 *
 * WHY THERE IS NO CONVEX HULL HERE. A zonotope's support in a direction n is
 * sum_i max(0, n . w_i), because the best reflectance for that direction is
 * simply R_i = 1 wherever n . w_i is positive. Its facets are spanned by pairs
 * of generators, so every facet normal is w_i x w_j. Membership is therefore
 * exact with two dot products per pair, and needs no hull library, no Delaunay
 * triangulation and none of the degeneracy handling either would bring.
 *
 * The vertices are the classical optimal colour stimuli, and they fall out of
 * the same fact. Maximising n . x over the cube picks R_i = 1 exactly where
 * n . w_i > 0, and for a three-dimensional colour signal that quantity changes
 * sign at most twice as n turns, which is Schroedinger's result: the optimal
 * reflectances are the ones with at most two transitions, the "pulse waves".
 * There are bins * (bins - 1) + 2 of them, and they are generated here in the
 * order colour-science's generate_pulse_waves uses so the two can be compared
 * term by term.
 *
 * THE QUADRATURE IS RECTANGLES, deliberately. A pulse wave is a band that is
 * wholly on or wholly off, so weighting the two end bands at half, as the
 * trapezoid rule does, would contradict what the construction means. The
 * weights come from alwan_spectral_weights_observer, whose end columns are
 * then doubled to undo the trapezoid's half weights and the rows renormalised
 * so a perfect reflector reads Y = 1 again. Measured: that reproduces
 * colour-science's own generator matrix to 1.7e-16.
 *
 * Reference: colour-science's colour.volume.XYZ_outer_surface and
 * is_within_visible_spectrum, which suite 164 pins against. Those build the
 * same solid and test membership with a Delaunay mesh; this agrees with them
 * because the zonotope IS the convex hull of those vertices.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

enum {
    /* create is O(bins^3) in the facet build and holds bins^2 / 2 facets, so
     * the cap is about what a caller can afford rather than a limit of the
     * maths. 256 bins is 65,282 vertices and 32,640 facets. */
    ALWAN_COLOUR_SOLID_MAX_BINS = 256,
    ALWAN_COLOUR_SOLID_MIN_BINS = 3
};

struct alwan_colour_solid_s {
    double *gen;            /* bins * 3, the per-band generators w_i */
    size_t  bins;
    /* facet_count * 5: the unit normal, then the solid's support in +n and -n */
    double *facet;
    size_t  facet_count;
    double  white[3];
};

/* ---------------------------------------------------------------- build */

static alwan_status alwan_colour_solid_generators(double *gen, size_t bins,
                                                  double wmin, double wmax,
                                                  alwan_observer_type observer,
                                                  alwan_illuminant illuminant,
                                                  alwan_ctx *ctx) {
    alwan_spd_f64 illum;
    double *w = NULL;
    alwan_status st;
    size_t i;
    double y_sum = 0.0;

    memset(&illum, 0, sizeof illum);
    st = alwan_spd_illuminant_f64(&illum, illuminant, ctx);
    if (st != ALWAN_OK) return st;

    w = (double *)ALWAN_ALLOC(alwan_safe_array_size(bins * 3, sizeof(double)), sizeof(double));
    if (!w) {
        alwan_spd_destroy_f64(&illum, ctx);
        return ALWAN_E_NOMEM;
    }

    /* normalize 0: the raw integral, because the rows are rescaled below after
     * the end columns are corrected, and normalising twice would be wrong. */
    st = alwan_spectral_weights_observer_f64(w, bins, wmin, wmax, &illum, observer,
                                             ALWAN_INTEGRATE_TRAPEZOID, 0, ctx);
    alwan_spd_destroy_f64(&illum, ctx);
    if (st != ALWAN_OK) { ALWAN_FREE(w); return st; }

    /* Trapezoid to rectangles: the first and last bands carry half weight under
     * the trapezoid rule, and a pulse wave's band is wholly on or wholly off. */
    for (i = 0; i < 3; i++) {
        w[i * bins + 0] *= 2.0;
        w[i * bins + (bins - 1)] *= 2.0;
    }
    for (i = 0; i < bins; i++) y_sum += w[1 * bins + i];
    if (!(y_sum > 0.0)) { ALWAN_FREE(w); return ALWAN_E_DIVZERO; }

    for (i = 0; i < bins; i++) {
        gen[i * 3 + 0] = w[0 * bins + i] / y_sum;
        gen[i * 3 + 1] = w[1 * bins + i] / y_sum;
        gen[i * 3 + 2] = w[2 * bins + i] / y_sum;
    }
    ALWAN_FREE(w);
    return ALWAN_OK;
}

/* The facets: one per pair of generators that spans a plane. Pairs whose cross
 * product is negligible next to the largest are dropped, since a normal that
 * short carries no constraint and would only amplify rounding. */
static alwan_status alwan_colour_solid_facets(alwan_colour_solid *s) {
    size_t const bins = s->bins;
    size_t const pairs = bins * (bins - 1) / 2;
    size_t i, j, k, n = 0;
    double longest = 0.0, cutoff;

    s->facet = (double *)ALWAN_ALLOC(alwan_safe_array_size(pairs * 5, sizeof(double)),
                                     sizeof(double));
    if (!s->facet) return ALWAN_E_NOMEM;

    for (i = 0; i < bins; i++) {
        for (j = i + 1; j < bins; j++) {
            double const *a = s->gen + i * 3, *b = s->gen + j * 3;
            double nx = a[1] * b[2] - a[2] * b[1];
            double ny = a[2] * b[0] - a[0] * b[2];
            double nz = a[0] * b[1] - a[1] * b[0];
            double len = sqrt(nx * nx + ny * ny + nz * nz);
            if (len > longest) longest = len;
        }
    }
    if (!(longest > 0.0)) return ALWAN_E_RANGE;      /* every generator parallel */
    cutoff = longest * 1e-12;

    for (i = 0; i < bins; i++) {
        for (j = i + 1; j < bins; j++) {
            double const *a = s->gen + i * 3, *b = s->gen + j * 3;
            double nx = a[1] * b[2] - a[2] * b[1];
            double ny = a[2] * b[0] - a[0] * b[2];
            double nz = a[0] * b[1] - a[1] * b[0];
            double len = sqrt(nx * nx + ny * ny + nz * nz);
            double hp = 0.0, hm = 0.0;
            if (!(len > cutoff)) continue;
            nx /= len; ny /= len; nz /= len;
            for (k = 0; k < bins; k++) {
                double const *g = s->gen + k * 3;
                double d = nx * g[0] + ny * g[1] + nz * g[2];
                if (d > 0.0) hp += d; else hm -= d;
            }
            s->facet[n * 5 + 0] = nx;
            s->facet[n * 5 + 1] = ny;
            s->facet[n * 5 + 2] = nz;
            s->facet[n * 5 + 3] = hp;
            s->facet[n * 5 + 4] = hm;
            n++;
        }
    }
    s->facet_count = n;
    return n > 0 ? ALWAN_OK : ALWAN_E_RANGE;
}

static alwan_status alwan_colour_solid_build(alwan_colour_solid **out,
                                             double wmin, double wmax, size_t bins,
                                             alwan_observer_type observer,
                                             alwan_illuminant illuminant,
                                             alwan_ctx *ctx) {
    alwan_colour_solid *s;
    alwan_status st;
    size_t i;

    if (out) *out = NULL;
    if (!out) return ALWAN_E_INVALID;
    if (bins < ALWAN_COLOUR_SOLID_MIN_BINS || bins > ALWAN_COLOUR_SOLID_MAX_BINS) {
        return ALWAN_E_RANGE;
    }
    if (!(wmax > wmin)) return ALWAN_E_INVALID;

    s = (alwan_colour_solid *)ALWAN_ALLOC(sizeof *s, sizeof(void *));
    if (!s) return ALWAN_E_NOMEM;
    memset(s, 0, sizeof *s);
    s->bins = bins;
    s->gen = (double *)ALWAN_ALLOC(alwan_safe_array_size(bins * 3, sizeof(double)),
                                   sizeof(double));
    if (!s->gen) { alwan_colour_solid_destroy(s, ctx); return ALWAN_E_NOMEM; }

    st = alwan_colour_solid_generators(s->gen, bins, wmin, wmax, observer, illuminant, ctx);
    if (st != ALWAN_OK) { alwan_colour_solid_destroy(s, ctx); return st; }

    for (i = 0; i < bins; i++) {
        s->white[0] += s->gen[i * 3 + 0];
        s->white[1] += s->gen[i * 3 + 1];
        s->white[2] += s->gen[i * 3 + 2];
    }

    st = alwan_colour_solid_facets(s);
    if (st != ALWAN_OK) { alwan_colour_solid_destroy(s, ctx); return st; }

    *out = s;
    return ALWAN_OK;
}

alwan_status alwan_colour_solid_create_f64(alwan_colour_solid **out,
                                           alwan_f64 wavelength_min, alwan_f64 wavelength_max,
                                           size_t bins, alwan_observer_type observer,
                                           alwan_illuminant illuminant, alwan_ctx *ctx) {
    return alwan_colour_solid_build(out, (double)wavelength_min, (double)wavelength_max,
                                    bins, observer, illuminant, ctx);
}

alwan_status alwan_colour_solid_create_f32(alwan_colour_solid **out,
                                           alwan_f32 wavelength_min, alwan_f32 wavelength_max,
                                           size_t bins, alwan_observer_type observer,
                                           alwan_illuminant illuminant, alwan_ctx *ctx) {
    return alwan_colour_solid_build(out, (double)wavelength_min, (double)wavelength_max,
                                    bins, observer, illuminant, ctx);
}

void alwan_colour_solid_destroy(alwan_colour_solid *solid, alwan_ctx *ctx) {
    (void)ctx;
    if (!solid) return;
    ALWAN_FREE(solid->gen);
    ALWAN_FREE(solid->facet);
    ALWAN_FREE(solid);
}

/* ---------------------------------------------------------------- query */

size_t alwan_colour_solid_num_vertices(alwan_colour_solid const *solid) {
    if (!solid) return 0;
    return solid->bins * (solid->bins - 1) + 2;
}

size_t alwan_colour_solid_num_facets(alwan_colour_solid const *solid) {
    return solid ? solid->facet_count : 0;
}

size_t alwan_colour_solid_bins(alwan_colour_solid const *solid) {
    return solid ? solid->bins : 0;
}

/* One vertex by index, in colour-science's "Bins" order: the black point, then
 * for each pulse width the `bins` rotations of that pulse, then white. Indexed
 * rather than filled in bulk so neither precision needs a scratch buffer and
 * neither has to assume anything about how alwan_xyz is laid out. */
static void alwan_colour_solid_vertex(double *xyz, alwan_colour_solid const *s, size_t index) {
    size_t const bins = s->bins;
    size_t const last = bins * (bins - 1) + 1;
    size_t r, i, t;
    xyz[0] = xyz[1] = xyz[2] = 0.0;
    if (index == 0) return;
    if (index >= last) {
        xyz[0] = s->white[0];
        xyz[1] = s->white[1];
        xyz[2] = s->white[2];
        return;
    }
    r = (index - 1) / bins;
    i = (index - 1) % bins;
    for (t = 0; t <= r; t++) {
        size_t const band = (i + t) % bins;
        xyz[0] += s->gen[band * 3 + 0];
        xyz[1] += s->gen[band * 3 + 1];
        xyz[2] += s->gen[band * 3 + 2];
    }
}

alwan_status alwan_colour_solid_vertices_f64(alwan_xyz_f64 *out, size_t capacity,
                                             alwan_colour_solid const *solid) {
    size_t want, i;
    if (!out || !solid) return ALWAN_E_INVALID;
    want = solid->bins * (solid->bins - 1) + 2;
    if (capacity < want) return ALWAN_E_RANGE;
    for (i = 0; i < want; i++) {
        double v[3];
        alwan_colour_solid_vertex(v, solid, i);
        out[i].x = v[0];
        out[i].y = v[1];
        out[i].z = v[2];
    }
    return ALWAN_OK;
}

alwan_status alwan_colour_solid_vertices_f32(alwan_xyz_f32 *out, size_t capacity,
                                             alwan_colour_solid const *solid) {
    size_t want, i;
    if (!out || !solid) return ALWAN_E_INVALID;
    want = solid->bins * (solid->bins - 1) + 2;
    if (capacity < want) return ALWAN_E_RANGE;
    for (i = 0; i < want; i++) {
        double v[3];
        alwan_colour_solid_vertex(v, solid, i);
        out[i].x = (alwan_f32)v[0];
        out[i].y = (alwan_f32)v[1];
        out[i].z = (alwan_f32)v[2];
    }
    return ALWAN_OK;
}

alwan_status alwan_colour_solid_white_f64(alwan_xyz_f64 *out, alwan_colour_solid const *solid) {
    if (!out || !solid) return ALWAN_E_INVALID;
    out->x = solid->white[0];
    out->y = solid->white[1];
    out->z = solid->white[2];
    return ALWAN_OK;
}

alwan_status alwan_colour_solid_white_f32(alwan_xyz_f32 *out, alwan_colour_solid const *solid) {
    if (!out || !solid) return ALWAN_E_INVALID;
    out->x = (alwan_f32)solid->white[0];
    out->y = (alwan_f32)solid->white[1];
    out->z = (alwan_f32)solid->white[2];
    return ALWAN_OK;
}

/* Membership. Exact for the solid on this band grid: a point is inside when it
 * violates no facet, and the facets are all of them. */
static alwan_status alwan_colour_solid_test(int *inside, alwan_colour_solid const *solid,
                                            double x, double y, double z, double tolerance) {
    size_t f;
    if (!inside || !solid) return ALWAN_E_INVALID;
    *inside = 0;
    if (!(x == x) || !(y == y) || !(z == z)) return ALWAN_E_INVALID;   /* NaN */
    if (tolerance < 0.0) tolerance = 0.0;
    for (f = 0; f < solid->facet_count; f++) {
        double const *p = solid->facet + f * 5;
        double const d = p[0] * x + p[1] * y + p[2] * z;
        if (d > p[3] + tolerance) return ALWAN_OK;
        if (-d > p[4] + tolerance) return ALWAN_OK;
    }
    *inside = 1;
    return ALWAN_OK;
}

alwan_status alwan_colour_solid_contains_f64(int *inside, alwan_colour_solid const *solid,
                                             alwan_xyz_f64 const *xyz, alwan_f64 tolerance) {
    if (!xyz) return ALWAN_E_INVALID;
    return alwan_colour_solid_test(inside, solid, xyz->x, xyz->y, xyz->z, (double)tolerance);
}

alwan_status alwan_colour_solid_contains_f32(int *inside, alwan_colour_solid const *solid,
                                             alwan_xyz_f32 const *xyz, alwan_f32 tolerance) {
    if (!xyz) return ALWAN_E_INVALID;
    return alwan_colour_solid_test(inside, solid, (double)xyz->x, (double)xyz->y,
                                   (double)xyz->z, (double)tolerance);
}
