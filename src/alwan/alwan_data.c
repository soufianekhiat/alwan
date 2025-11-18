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

/* sRGB primaries (rx, ry, gx, gy, bx, by) */
static alwan_scalar const g_srgb_primaries_3x2[] = {
#include "data/srgb_primaries_3x2.csv"
};

ALWAN_DIAG_POP

int alwan_data_get_a(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    (void)ctx;  /* Unused in embedded mode */
    *data = (alwan_scalar *)g_a_xy;
    *count = sizeof(g_a_xy) / sizeof(g_a_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_d50(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    (void)ctx;
    *data = (alwan_scalar *)g_d50_xy;
    *count = sizeof(g_d50_xy) / sizeof(g_d50_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_d55(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    (void)ctx;
    *data = (alwan_scalar *)g_d55_xy;
    *count = sizeof(g_d55_xy) / sizeof(g_d55_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_d60(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    (void)ctx;
    *data = (alwan_scalar *)g_d60_xy;
    *count = sizeof(g_d60_xy) / sizeof(g_d60_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_d65(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    (void)ctx;
    *data = (alwan_scalar *)g_d65_xy;
    *count = sizeof(g_d65_xy) / sizeof(g_d65_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_e(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    (void)ctx;
    *data = (alwan_scalar *)g_e_xy;
    *count = sizeof(g_e_xy) / sizeof(g_e_xy[0]);
    return ALWAN_OK;
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

int alwan_data_get_a(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    char path[512];
    build_path(path, sizeof(path), ctx ? ctx->runtime_data_root : NULL, "illuminants_xy/a_xy.csv");
    return load_csv(path, data, count);
}

int alwan_data_get_d50(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    char path[512];
    build_path(path, sizeof(path), ctx ? ctx->runtime_data_root : NULL, "illuminants_xy/d50_xy.csv");
    return load_csv(path, data, count);
}

int alwan_data_get_d55(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    char path[512];
    build_path(path, sizeof(path), ctx ? ctx->runtime_data_root : NULL, "illuminants_xy/d55_xy.csv");
    return load_csv(path, data, count);
}

int alwan_data_get_d60(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    char path[512];
    build_path(path, sizeof(path), ctx ? ctx->runtime_data_root : NULL, "illuminants_xy/d60_xy.csv");
    return load_csv(path, data, count);
}

int alwan_data_get_d65(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    char path[512];
    build_path(path, sizeof(path), ctx ? ctx->runtime_data_root : NULL, "illuminants_xy/d65_xy.csv");
    return load_csv(path, data, count);
}

int alwan_data_get_e(alwan_ctx *ctx, alwan_scalar **data, size_t *count) {
    char path[512];
    build_path(path, sizeof(path), ctx ? ctx->runtime_data_root : NULL, "illuminants_xy/e_xy.csv");
    return load_csv(path, data, count);
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
