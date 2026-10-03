/* No include guard -- included twice (f32 and f64 passes) */

/*
 * Exposure-bracket merging, after colour-hdri (colour_hdri.generation), which the tests
 * hold this to: the weighted radiance merge of a bracket, with the four weighting
 * functions colour-hdri ships. Split from alwan_exposure_impl.inc (image processing,
 * bound for suwar); the exposure settings type stays alwan's.
 */

#define ALWAN__EXPOSURE ALWAN_CORE_FNLIT(alwan_exposure_settings)

/* The settings a frame was shot with are usable: as alwan_exposure_impl.inc checks. */
static int ALWAN_CORE_FN(merge_exposure_ok)(ALWAN_CORE_T f_number, ALWAN_CORE_T exposure_time, ALWAN_CORE_T iso) {
    return f_number > ALWAN_CORE_ZERO && exposure_time > ALWAN_CORE_ZERO && iso > ALWAN_CORE_ZERO;
}

/* ----------------------------------------------------------------
 * Bracket merging
 * ---------------------------------------------------------------- */

/* colour-hdri's anchored sigmoid with its k = 2. */
static ALWAN_CORE_T ALWAN_CORE_FN(merge_sigmoid)(ALWAN_CORE_T x, ALWAN_CORE_T c, ALWAN_CORE_T d) {
    ALWAN_CORE_T const r = (x - c) / (d - c);
    ALWAN_CORE_T u;
    if (!(r > ALWAN_CORE_ZERO)) {
        return ALWAN_CORE_ZERO;
    }
    u = ALWAN_CORE_ONE / r - ALWAN_CORE_ONE;
    return ALWAN_CORE_ONE / (ALWAN_CORE_ONE + u * u);
}

static ALWAN_CORE_T ALWAN_CORE_FN(merge_weight)(ALWAN_CORE_T a, alwan_merge_weight fn) {
    switch (fn) {
    case ALWAN_MERGE_WEIGHT_HAT: {
        ALWAN_CORE_T const x = ALWAN_CORE_LITERAL(2.0) * a - ALWAN_CORE_ONE;
        ALWAN_CORE_T const x2 = x * x;
        ALWAN_CORE_T const x4 = x2 * x2;
        return ALWAN_CORE_ONE - x4 * x4 * x4;
    }
    case ALWAN_MERGE_WEIGHT_NORMAL: {
        ALWAN_CORE_T const d = a - ALWAN_CORE_LITERAL(0.5);
        return ALWAN_CORE_EXP(-(d * d) / (ALWAN_CORE_LITERAL(2.0) * (ALWAN_CORE_LITERAL(0.15) * ALWAN_CORE_LITERAL(0.15))));
    }
    case ALWAN_MERGE_WEIGHT_DOUBLE_SIGMOID:
        if (a <= ALWAN_CORE_LITERAL(0.025)) return ALWAN_CORE_ZERO;
        if (a <= ALWAN_CORE_LITERAL(0.2)) {
            return ALWAN_CORE_FN(merge_sigmoid)(a, ALWAN_CORE_LITERAL(0.025), ALWAN_CORE_LITERAL(0.2));
        }
        if (a < ALWAN_CORE_LITERAL(0.8)) return ALWAN_CORE_ONE;
        if (a < ALWAN_CORE_LITERAL(0.975)) {
            return ALWAN_CORE_ONE - ALWAN_CORE_FN(merge_sigmoid)(a, ALWAN_CORE_LITERAL(0.8), ALWAN_CORE_LITERAL(0.975));
        }
        return ALWAN_CORE_ZERO;
    default: {
        /* Debevec 1997: a triangle on [0.01, 0.99], normalised by its own peak.
         * colour-hdri divides by the largest weight in the image instead, which is
         * the same number whenever the image holds a value at the midpoint. */
        ALWAN_CORE_T const lo = ALWAN_CORE_LITERAL(0.01);
        ALWAN_CORE_T const hi = ALWAN_CORE_LITERAL(0.99);
        ALWAN_CORE_T const mid = (lo + hi) / ALWAN_CORE_LITERAL(2.0);
        ALWAN_CORE_T const w = (a <= mid ? a - lo : hi - a) / (mid - lo);
        return w < ALWAN_CORE_ZERO ? ALWAN_CORE_ZERO : w;
    }
    }
}

alwan_status ALWAN_CORE_FNLIT(alwan_hdr_merge_weight)(ALWAN_CORE_T *weight_out, ALWAN_CORE_T value,
                                                      alwan_merge_weight fn) {
    if (!weight_out || (int)fn < 0 || (int)fn > (int)ALWAN_MERGE_WEIGHT_DOUBLE_SIGMOID) {
        return ALWAN_E_INVALID;
    }
    *weight_out = ALWAN_CORE_FN(merge_weight)(value, fn);
    return ALWAN_OK;
}

#ifndef ALWAN_EXPOSURE_MAPNAME
#  define ALWAN_EXPOSURE_MAPNAME_(base, sfx) base##sfx##_map_interleave
#  define ALWAN_EXPOSURE_MAPNAME(base, sfx)  ALWAN_EXPOSURE_MAPNAME_(base, sfx)
#endif

alwan_status ALWAN_EXPOSURE_MAPNAME(alwan_hdr_merge, ALWAN_CORE_SUFFIX)(
        ALWAN_CORE_T *out, size_t out_stride,
        ALWAN_CORE_T const *const *images, size_t in_stride, size_t count,
        ALWAN__EXPOSURE const *settings, size_t image_count,
        alwan_merge_weight weight, ALWAN_CORE_T const *response, size_t response_size) {
    ALWAN_CORE_T const eps = ALWAN_CORE_LITERAL(2.220446049250313e-16);
    size_t i;
    if (!out || !images || !settings || image_count == 0
        || (int)weight < 0 || (int)weight > (int)ALWAN_MERGE_WEIGHT_DOUBLE_SIGMOID) {
        return ALWAN_E_INVALID;
    }
    if (response && (response_size < 2 || response_size > 1u << 24)) {
        return ALWAN_E_INVALID;
    }
    for (i = 0; i < image_count; i++) {
        if (!images[i] || !ALWAN_CORE_FN(merge_exposure_ok)(settings[i].f_number, settings[i].exposure_time,
                                                      settings[i].iso)) {
            return ALWAN_E_INVALID;
        }
    }
    for (size_t p = 0; p < count; p++) {
        ALWAN_CORE_T num[3] = { ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, ALWAN_CORE_ZERO };
        ALWAN_CORE_T den[3] = { ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, ALWAN_CORE_ZERO };
        ALWAN_CORE_T *o = (ALWAN_CORE_T *)((char *)out + p * out_stride);
        for (i = 0; i < image_count; i++) {
            ALWAN_CORE_T const *s = (ALWAN_CORE_T const *)((char const *)images[i] + p * in_stride);
            ALWAN_CORE_T const N = settings[i].f_number;
            /* colour-hdri's L: the reciprocal of the ISO 2720 average luminance. */
            ALWAN_CORE_T const L = ALWAN_CORE_ONE / (N * N / settings[i].exposure_time / settings[i].iso
                                                     * ALWAN_CORE_LITERAL(12.5));
            for (int c = 0; c < 3; c++) {
                ALWAN_CORE_T d = s[c];
                ALWAN_CORE_T w;
                /* The merge is defined on normalised sensor values: below the
                 * smallest positive value there is no signal, above 1 the
                 * photosite has saturated. */
                d = d < eps ? eps : (d > ALWAN_CORE_ONE ? ALWAN_CORE_ONE : d);
                w = ALWAN_CORE_FN(merge_weight)(d, weight);
                w = w < eps ? eps : (w > ALWAN_CORE_ONE ? ALWAN_CORE_ONE : w);
                /* The shortest exposure is trusted in the highlights, the longest
                 * in the shadows, whatever the weighting function says. */
                if (i == 0 && d >= ALWAN_CORE_LITERAL(0.5)) w = ALWAN_CORE_ONE;
                if (i == image_count - 1 && d <= ALWAN_CORE_LITERAL(0.5)) w = ALWAN_CORE_ONE;
                if (response) {
                    ALWAN_CORE_T v = d;
                    (void)ALWAN_CORE_FNLIT(alwan_table1d_sample)(&v, response + (size_t)c * response_size,
                                                                (int)response_size, d, ALWAN_SAMPLE_LINEAR);
                    d = v;
                }
                num[c] += w * d / L;
                den[c] += w;
            }
        }
        o[0] = num[0] / den[0];
        o[1] = num[1] / den[1];
        o[2] = num[2] / den[2];
    }
    return ALWAN_OK;
}

#undef ALWAN__EXPOSURE
