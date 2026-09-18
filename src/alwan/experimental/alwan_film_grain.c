/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * EXPERIMENTAL: film grain, the Boolean disc model of Newson, Faraj, Galerne
 * and Delon rendered by Monte Carlo. The declarations, the model and the
 * references are in the film section of alwan.h. Everything under
 * experimental/ is research code.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <stdint.h>

alwan_status alwan_film_grain_params_default(alwan_film_grain_params *params, alwan_f64 radius) {
    if (!params || !(radius > 0.0)) return ALWAN_E_INVALID;
    params->radius = radius;
    params->radius_sigma = 0.0;
    params->filter_sigma = 0.8;
    params->samples = 64;
    params->seed = 1;
    return ALWAN_OK;
}

alwan_status alwan_film_grain_density_params_default(alwan_film_grain_density_params *params, alwan_f64 rms_granularity,
                                                     alwan_f64 pixel_pitch) {
    if (!params || !(rms_granularity > 0.0) || !(pixel_pitch > 0.0)) return ALWAN_E_INVALID;
    params->rms_granularity = rms_granularity;
    params->pixel_pitch = pixel_pitch;
    params->layer_scale[0] = 1.0;
    params->layer_scale[1] = 1.0;
    params->layer_scale[2] = 1.0;
    params->seed = 1;
    return ALWAN_OK;
}

/* The generator: a 64-bit linear congruential state with a 32-bit xorshift output,
 * PCG's shape, seeded through splitmix64 so nearby cells get unrelated streams. */
static uint64_t alwan__grain_mix(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

typedef struct { uint64_t s; } alwan__grain_rng;

/* One stream per (seed, column, row, channel). Columns and rows are signed so the
 * replicated cells past the edges get streams of their own. */
static void alwan__grain_seed(alwan__grain_rng *g, uint64_t seed, long i, long j, uint64_t channel) {
    g->s = alwan__grain_mix(alwan__grain_mix(alwan__grain_mix(alwan__grain_mix(seed) + (uint64_t)(int64_t)i) +
                                             (uint64_t)(int64_t)j) + channel);
}

static uint32_t alwan__grain_u32(alwan__grain_rng *g) {
    uint64_t const old = g->s;
    uint32_t const xs = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    uint32_t const rot = (uint32_t)(old >> 59u);
    g->s = old * 6364136223846793005ull + 1442695040888963407ull;
    return (xs >> rot) | (xs << ((32u - rot) & 31u));
}

/* Uniform in (0, 1]: never 0, so a log of it is finite. */
static double alwan__grain_uniform(alwan__grain_rng *g) {
    return ((double)(alwan__grain_u32(g) >> 8) + 1.0) * (1.0 / 16777216.0);
}

static double alwan__grain_normal(alwan__grain_rng *g) {
    double const u1 = alwan__grain_uniform(g), u2 = alwan__grain_uniform(g);
    return sqrt(-2.0 * log(u1)) * cos(6.283185307179586 * u2);
}

/* Poisson by Knuth's product for small means, a rounded normal past 40. */
static unsigned alwan__grain_poisson(alwan__grain_rng *g, double lambda) {
    if (lambda <= 0.0) return 0;
    if (lambda < 40.0) {
        double const L = exp(-lambda);
        double p = alwan__grain_uniform(g);
        unsigned k = 0;
        while (p > L) {
            k++;
            p *= alwan__grain_uniform(g);
        }
        return k;
    } else {
        double const v = lambda + sqrt(lambda) * alwan__grain_normal(g);
        return v < 0.0 ? 0u : (unsigned)(v + 0.5);
    }
}

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "../api/alwan_api_f32_setup.h"
#include "alwan_film_grain_impl.inc"
#include "../api/alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "../api/alwan_api_f64_setup.h"
#include "alwan_film_grain_impl.inc"
#include "../api/alwan_api_teardown.h"
#endif
