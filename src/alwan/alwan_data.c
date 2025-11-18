/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Data loading: embedded vs runtime
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if ALWAN_EMBED_DATA

/* ----------------------------------------------------------------
 * Embedded data mode: compile-time data inclusion
 * ---------------------------------------------------------------- */

/* Disable float conversion warnings for embedded CSV data */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

/* Illuminant A (x, y) */
static alwan_scalar const g_a_xy[] = {
#include "data/illuminants_xy/a_xy.csv"
};

/* Illuminant D50 (x, y) */
static alwan_scalar const g_d50_xy[] = {
#include "data/illuminants_xy/d50_xy.csv"
};

/* Illuminant D55 (x, y) */
static alwan_scalar const g_d55_xy[] = {
#include "data/illuminants_xy/d55_xy.csv"
};

/* Illuminant D60 (x, y) */
static alwan_scalar const g_d60_xy[] = {
#include "data/illuminants_xy/d60_xy.csv"
};

/* Illuminant D65 (x, y) */
static alwan_scalar const g_d65_xy[] = {
#include "data/illuminants_xy/d65_xy.csv"
};

/* Illuminant E (x, y) */
static alwan_scalar const g_e_xy[] = {
#include "data/illuminants_xy/e_xy.csv"
};

/* P8: Illuminant B (x, y) */
static alwan_scalar const g_b_xy[] = {
#include "data/illuminants_xy/b_xy.csv"
};

/* P8: Illuminant C (x, y) */
static alwan_scalar const g_c_xy[] = {
#include "data/illuminants_xy/c_xy.csv"
};

/* P8: Illuminant D75 (x, y) */
static alwan_scalar const g_d75_xy[] = {
#include "data/illuminants_xy/d75_xy.csv"
};

/* sRGB primaries (rx, ry, gx, gy, bx, by) */
static alwan_scalar const g_srgb_primaries_3x2[] = {
#include "data/srgb_primaries_3x2.csv"
};

ALWAN_DIAG_POP

int alwan_data_get_illuminant_a(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    (void)ctx;  /* Unused in embedded mode */
    *data = (alwan_scalar *)g_a_xy;
    *count = sizeof(g_a_xy) / sizeof(g_a_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_d50(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    (void)ctx;
    *data = (alwan_scalar *)g_d50_xy;
    *count = sizeof(g_d50_xy) / sizeof(g_d50_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_d55(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    (void)ctx;
    *data = (alwan_scalar *)g_d55_xy;
    *count = sizeof(g_d55_xy) / sizeof(g_d55_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_d60(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    (void)ctx;
    *data = (alwan_scalar *)g_d60_xy;
    *count = sizeof(g_d60_xy) / sizeof(g_d60_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_d65(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    (void)ctx;
    *data = (alwan_scalar *)g_d65_xy;
    *count = sizeof(g_d65_xy) / sizeof(g_d65_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_e(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    (void)ctx;
    *data = (alwan_scalar *)g_e_xy;
    *count = sizeof(g_e_xy) / sizeof(g_e_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_b(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    (void)ctx;
    *data = (alwan_scalar *)g_b_xy;
    *count = sizeof(g_b_xy) / sizeof(g_b_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_c(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    (void)ctx;
    *data = (alwan_scalar *)g_c_xy;
    *count = sizeof(g_c_xy) / sizeof(g_c_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_d75(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    (void)ctx;
    *data = (alwan_scalar *)g_d75_xy;
    *count = sizeof(g_d75_xy) / sizeof(g_d75_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_xy(alwan_ctx *ctx, alwan_illuminant illuminant,
                                   alwan_scalar **data, size_t *count) {
    switch (illuminant) {
        case ALWAN_ILLUMINANT_A:   return alwan_data_get_illuminant_a(ctx, data, count);
        case ALWAN_ILLUMINANT_D50: return alwan_data_get_illuminant_d50(ctx, data, count);
        case ALWAN_ILLUMINANT_D55: return alwan_data_get_illuminant_d55(ctx, data, count);
        case ALWAN_ILLUMINANT_D60: return alwan_data_get_illuminant_d60(ctx, data, count);
        case ALWAN_ILLUMINANT_D65: return alwan_data_get_illuminant_d65(ctx, data, count);
        case ALWAN_ILLUMINANT_E:   return alwan_data_get_illuminant_e(ctx, data, count);
        case ALWAN_ILLUMINANT_B:   return alwan_data_get_illuminant_b(ctx, data, count);
        case ALWAN_ILLUMINANT_C:   return alwan_data_get_illuminant_c(ctx, data, count);
        case ALWAN_ILLUMINANT_D75: return alwan_data_get_illuminant_d75(ctx, data, count);
        default:
            return ALWAN_E_INVALID;  /* Unsupported illuminant or no xy data */
    }
}

int alwan_data_get_srgb_primaries(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    (void)ctx;
    *data = (alwan_scalar *)g_srgb_primaries_3x2;
    *count = sizeof(g_srgb_primaries_3x2) / sizeof(g_srgb_primaries_3x2[0]);
    return ALWAN_OK;
}

#else

/* ----------------------------------------------------------------
 * Runtime data mode: load from filesystem
 * ---------------------------------------------------------------- */

/* Internal context structure (needed for runtime_data_root access) */
struct alwan_ctx {
    alwan_alloc_fn alloc_fn;
    alwan_free_fn  free_fn;
    char *runtime_data_root;
};

/* Helper: construct file path from data root and relative path */
static void build_path(char *dest, size_t dest_size,
                       char const *data_root, char const *rel_path) {
    if (data_root && data_root[0]) {
        snprintf(dest, dest_size, "%s/%s", data_root, rel_path);
    } else {
        snprintf(dest, dest_size, "../src/alwan/data/%s", rel_path);
    }
}

/* Helper: load CSV file into dynamically allocated array */
static int load_csv(char const *filepath, alwan_scalar **out_data, size_t *out_count) {
    FILE *f = fopen(filepath, "r");
    if (!f) {
        return ALWAN_E_IO;
    }

    /* Count values by counting commas + 1 */
    size_t capacity = 16;
    size_t count = 0;
    alwan_scalar *data = (alwan_scalar *)ALWAN_ALLOC(capacity * sizeof(alwan_scalar));
    if (!data) {
        fclose(f);
        return ALWAN_E_NOMEM;
    }

    /* Parse CSV values */
    char line[4096];
    if (fgets(line, sizeof(line), f)) {
        char *ptr = line;
        char *end;

        while (*ptr) {
            /* Resize if needed */
            if (count >= capacity) {
                capacity *= 2;
                alwan_scalar *new_data = (alwan_scalar *)ALWAN_REALLOC(data, capacity * sizeof(alwan_scalar));
                if (!new_data) {
                    ALWAN_FREE(data);
                    fclose(f);
                    return ALWAN_E_NOMEM;
                }
                data = new_data;
            }

            /* Parse next value */
#if ALWAN_SCALAR_IS_FLOAT
            data[count] = strtof(ptr, &end);
#else
            data[count] = strtod(ptr, &end);
#endif

            if (end == ptr) {
                break;  /* No more values */
            }

            count++;
            ptr = end;

            /* Skip comma */
            if (*ptr == ',') {
                ptr++;
            }
        }
    }

    fclose(f);

    *out_data = data;
    *out_count = count;
    return ALWAN_OK;
}

int alwan_data_get_illuminant_a(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    char path[512];
    build_path(path, sizeof(path), ctx ? ctx->runtime_data_root : NULL, "illuminants_xy/a_xy.csv");
    return load_csv(path, data, count);
}

int alwan_data_get_illuminant_d50(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    char path[512];
    build_path(path, sizeof(path), ctx ? ctx->runtime_data_root : NULL, "illuminants_xy/d50_xy.csv");
    return load_csv(path, data, count);
}

int alwan_data_get_illuminant_d55(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    char path[512];
    build_path(path, sizeof(path), ctx ? ctx->runtime_data_root : NULL, "illuminants_xy/d55_xy.csv");
    return load_csv(path, data, count);
}

int alwan_data_get_illuminant_d60(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    char path[512];
    build_path(path, sizeof(path), ctx ? ctx->runtime_data_root : NULL, "illuminants_xy/d60_xy.csv");
    return load_csv(path, data, count);
}

int alwan_data_get_illuminant_d65(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    char path[512];
    build_path(path, sizeof(path), ctx ? ctx->runtime_data_root : NULL, "illuminants_xy/d65_xy.csv");
    return load_csv(path, data, count);
}

int alwan_data_get_illuminant_e(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    char path[512];
    build_path(path, sizeof(path), ctx ? ctx->runtime_data_root : NULL, "illuminants_xy/e_xy.csv");
    return load_csv(path, data, count);
}

int alwan_data_get_illuminant_b(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    char path[512];
    build_path(path, sizeof(path), ctx ? ctx->runtime_data_root : NULL, "illuminants_xy/b_xy.csv");
    return load_csv(path, data, count);
}

int alwan_data_get_illuminant_c(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    char path[512];
    build_path(path, sizeof(path), ctx ? ctx->runtime_data_root : NULL, "illuminants_xy/c_xy.csv");
    return load_csv(path, data, count);
}

int alwan_data_get_illuminant_d75(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    char path[512];
    build_path(path, sizeof(path), ctx ? ctx->runtime_data_root : NULL, "illuminants_xy/d75_xy.csv");
    return load_csv(path, data, count);
}

int alwan_data_get_illuminant_xy(alwan_ctx *ctx, alwan_illuminant illuminant,
                                   alwan_scalar **data, size_t *count) {
    switch (illuminant) {
        case ALWAN_ILLUMINANT_A:   return alwan_data_get_illuminant_a(ctx, data, count);
        case ALWAN_ILLUMINANT_D50: return alwan_data_get_illuminant_d50(ctx, data, count);
        case ALWAN_ILLUMINANT_D55: return alwan_data_get_illuminant_d55(ctx, data, count);
        case ALWAN_ILLUMINANT_D60: return alwan_data_get_illuminant_d60(ctx, data, count);
        case ALWAN_ILLUMINANT_D65: return alwan_data_get_illuminant_d65(ctx, data, count);
        case ALWAN_ILLUMINANT_E:   return alwan_data_get_illuminant_e(ctx, data, count);
        case ALWAN_ILLUMINANT_B:   return alwan_data_get_illuminant_b(ctx, data, count);
        case ALWAN_ILLUMINANT_C:   return alwan_data_get_illuminant_c(ctx, data, count);
        case ALWAN_ILLUMINANT_D75: return alwan_data_get_illuminant_d75(ctx, data, count);
        default:
            return ALWAN_E_INVALID;  /* Unsupported illuminant or no xy data */
    }
}

int alwan_data_get_srgb_primaries(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    char path[512];
    build_path(path, sizeof(path), ctx ? ctx->runtime_data_root : NULL, "srgb_primaries_3x2.csv");
    return load_csv(path, data, count);
}

void alwan_data_free(alwan_ctx *ctx, alwan_scalar *data) {
    (void)ctx;
    if (data) {
        ALWAN_FREE(data);
    }
}

#endif /* ALWAN_EMBED_DATA */

/* ----------------------------------------------------------------
 * Illuminant White Point Calculation (works in both modes)
 * ---------------------------------------------------------------- */

int alwan_illuminant_white_point(alwan_illuminant illuminant,
                                   alwan_observer_type observer,
                                   alwan_vec3 *out_xyz) {
    if (!out_xyz) {
        return ALWAN_E_INVALID;
    }

    /* For CIE 1931 2° observer, use pre-computed xy chromaticity values for efficiency */
    if (observer == ALWAN_OBSERVER_CIE_1931_2DEG) {
        /* Get xy chromaticity data for the illuminant */
        alwan_scalar *xy_data = NULL;
        size_t count = 0;
        int status = alwan_data_get_illuminant_xy(NULL, illuminant, &xy_data, &count);

        if (status != ALWAN_OK || count < 2) {
            return ALWAN_E_INVALID;
        }

        /* Extract x and y chromaticity coordinates */
        alwan_scalar x = xy_data[0];
        alwan_scalar y = xy_data[1];

        /* Convert xy to XYZ with Y = 1.0 (normalized)
         * Formula: X = x * Y / y
         *          Y = 1.0
         *          Z = (1 - x - y) * Y / y */
        alwan_scalar const Y = ALWAN_LITERAL(1.0);

        if (y <= ALWAN_LITERAL(0.0)) {
            return ALWAN_E_INVALID;  /* Invalid chromaticity */
        }

        out_xyz->v[0] = x * Y / y;                    /* X */
        out_xyz->v[1] = Y;                             /* Y */
        out_xyz->v[2] = (ALWAN_LITERAL(1.0) - x - y) * Y / y;  /* Z */

        return ALWAN_OK;
    }

    /* For other observers, compute from illuminant SPD + observer CMF integration */
    alwan_spd illum_spd;
    int status = alwan_spd_illuminant(NULL, illuminant, &illum_spd);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Integrate illuminant SPD with observer CMFs to get XYZ */
    alwan_vec3 xyz_unnormalized;
    status = alwan_xyz_from_spd(NULL, &illum_spd, NULL, observer,
                                ALWAN_INTEGRATE_SIMPSON, ALWAN_LITERAL(0.0),
                                &xyz_unnormalized);

    alwan_spd_destroy(NULL, &illum_spd);

    if (status != ALWAN_OK) {
        return status;
    }

    /* Normalize to Y = 1.0 */
    if (xyz_unnormalized.v[1] <= ALWAN_LITERAL(0.0)) {
        return ALWAN_E_INVALID;  /* Invalid Y value */
    }

    alwan_scalar norm_factor = ALWAN_LITERAL(1.0) / xyz_unnormalized.v[1];
    out_xyz->v[0] = xyz_unnormalized.v[0] * norm_factor;
    out_xyz->v[1] = ALWAN_LITERAL(1.0);
    out_xyz->v[2] = xyz_unnormalized.v[2] * norm_factor;

    return ALWAN_OK;
}
