/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * LUT import — .cube file reader
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ----------------------------------------------------------------
 * .cube file parser (Adobe/Iridas format)
 *
 * Supports:
 *   LUT_3D_SIZE N
 *   LUT_1D_SIZE N
 *   DOMAIN_MIN r g b
 *   DOMAIN_MAX r g b
 *   TITLE "name"
 *   # comments
 *   R G B data lines
 * ---------------------------------------------------------------- */

/* Skip leading whitespace */
static char const *skip_ws(char const *s) {
    while (*s && (*s == ' ' || *s == '\t')) s++;
    return s;
}

/* Check if line is comment or empty */
static int is_comment_or_empty(char const *line) {
    char const *s = skip_ws(line);
    return (*s == '#' || *s == '\0' || *s == '\n' || *s == '\r');
}

int alwan_cube_import_3d_f64(alwan_f64 *lut, int *out_size,
                          char const *path) {
    if (!lut || !out_size || !path) return ALWAN_E_INVALID;

    FILE *f = fopen(path, "r");
    if (!f) return ALWAN_E_INVALID;

    int size = 0;
    size_t data_count = 0;
    char line[512];

    while (fgets(line, sizeof(line), f)) {
        char const *s = skip_ws(line);

        /* Skip empty lines and comments */
        if (is_comment_or_empty(s)) continue;

        /* Parse header keywords */
        if (strncmp(s, "LUT_3D_SIZE", 11) == 0) {
            size = atoi(s + 11);
            if (size < 2 || size > 256) {
                fclose(f);
                return ALWAN_E_RANGE;
            }
            continue;
        }
        if (strncmp(s, "LUT_1D_SIZE", 11) == 0) continue; /* skip 1D in 3D loader */
        if (strncmp(s, "DOMAIN_MIN", 10) == 0) continue;
        if (strncmp(s, "DOMAIN_MAX", 10) == 0) continue;
        if (strncmp(s, "TITLE", 5) == 0) continue;

        /* Must be data — parse RGB triplet */
        if (size == 0) {
            fclose(f);
            return ALWAN_E_INVALID; /* data before size declaration */
        }

        double r, g, b;
        if (sscanf(s, "%lf %lf %lf", &r, &g, &b) != 3) {
            fclose(f);
            return ALWAN_E_INVALID;
        }

        size_t total = (size_t)size * (size_t)size * (size_t)size;
        if (data_count >= total) {
            fclose(f);
            return ALWAN_E_RANGE; /* too many data points */
        }

        lut[data_count * 3 + 0] = (alwan_f64)r;
        lut[data_count * 3 + 1] = (alwan_f64)g;
        lut[data_count * 3 + 2] = (alwan_f64)b;
        data_count++;
    }

    fclose(f);

    if (size == 0) return ALWAN_E_NODATA;

    size_t expected = (size_t)size * (size_t)size * (size_t)size;
    if (data_count != expected) return ALWAN_E_RANGE;

    *out_size = size;
    return ALWAN_OK;
}

int alwan_cube_import_1d_f64(alwan_f64 *lut, int *out_size,
                          char const *path) {
    if (!lut || !out_size || !path) return ALWAN_E_INVALID;

    FILE *f = fopen(path, "r");
    if (!f) return ALWAN_E_INVALID;

    int size = 0;
    int data_count = 0;
    char line[512];

    while (fgets(line, sizeof(line), f)) {
        char const *s = skip_ws(line);

        if (is_comment_or_empty(s)) continue;

        if (strncmp(s, "LUT_1D_SIZE", 11) == 0) {
            size = atoi(s + 11);
            if (size < 2 || size > 65536) {
                fclose(f);
                return ALWAN_E_RANGE;
            }
            continue;
        }
        if (strncmp(s, "LUT_3D_SIZE", 11) == 0) continue;
        if (strncmp(s, "DOMAIN_MIN", 10) == 0) continue;
        if (strncmp(s, "DOMAIN_MAX", 10) == 0) continue;
        if (strncmp(s, "TITLE", 5) == 0) continue;

        if (size == 0) {
            fclose(f);
            return ALWAN_E_INVALID;
        }

        double r, g, b;
        if (sscanf(s, "%lf %lf %lf", &r, &g, &b) != 3) {
            fclose(f);
            return ALWAN_E_INVALID;
        }

        if (data_count >= size) {
            fclose(f);
            return ALWAN_E_RANGE;
        }

        /* Average of R, G, B for 1D (they're usually identical) */
        lut[data_count] = (alwan_f64)((r + g + b) / 3.0);
        data_count++;
    }

    fclose(f);

    if (size == 0) return ALWAN_E_NODATA;
    if (data_count != size) return ALWAN_E_RANGE;

    *out_size = size;
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Import from memory buffer
 * ---------------------------------------------------------------- */

int alwan_cube_import_3d_buffer_f64(alwan_f64 *lut, int *out_size,
                                 char const *buf, size_t buf_len) {
    if (!lut || !out_size || !buf || buf_len == 0) return ALWAN_E_INVALID;

    int size = 0;
    size_t data_count = 0;
    char const *pos = buf;
    char const *end = buf + buf_len;

    while (pos < end) {
        /* Find end of line */
        char const *eol = pos;
        while (eol < end && *eol != '\n' && *eol != '\r') eol++;

        /* Copy line to temp buffer */
        size_t line_len = (size_t)(eol - pos);
        if (line_len >= 511) line_len = 510;
        char line[512];
        memcpy(line, pos, line_len);
        line[line_len] = '\0';

        /* Advance past line ending */
        pos = eol;
        if (pos < end && *pos == '\r') pos++;
        if (pos < end && *pos == '\n') pos++;

        char const *s = skip_ws(line);
        if (is_comment_or_empty(s)) continue;

        if (strncmp(s, "LUT_3D_SIZE", 11) == 0) {
            size = atoi(s + 11);
            if (size < 2 || size > 256) return ALWAN_E_RANGE;
            continue;
        }
        if (strncmp(s, "LUT_1D_SIZE", 11) == 0) continue;
        if (strncmp(s, "DOMAIN_MIN", 10) == 0) continue;
        if (strncmp(s, "DOMAIN_MAX", 10) == 0) continue;
        if (strncmp(s, "TITLE", 5) == 0) continue;

        if (size == 0) return ALWAN_E_INVALID;

        double r, g, b;
        if (sscanf(s, "%lf %lf %lf", &r, &g, &b) != 3) return ALWAN_E_INVALID;

        size_t total = (size_t)size * (size_t)size * (size_t)size;
        if (data_count >= total) return ALWAN_E_RANGE;

        lut[data_count * 3 + 0] = (alwan_f64)r;
        lut[data_count * 3 + 1] = (alwan_f64)g;
        lut[data_count * 3 + 2] = (alwan_f64)b;
        data_count++;
    }

    if (size == 0) return ALWAN_E_NODATA;

    size_t expected = (size_t)size * (size_t)size * (size_t)size;
    if (data_count != expected) return ALWAN_E_RANGE;

    *out_size = size;
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * f32 wrappers — delegate to f64 via a temporary buffer.
 * ---------------------------------------------------------------- */

int alwan_cube_import_3d_f32(alwan_f32 *lut, int *out_size, char const *path) {
    if (!lut || !out_size || !path) return ALWAN_E_INVALID;
    size_t max_total = 256ULL * 256ULL * 256ULL * 3ULL;
    alwan_f64 *tmp = (alwan_f64 *)malloc(max_total * sizeof(alwan_f64));
    if (!tmp) return ALWAN_E_NOMEM;
    int rc = alwan_cube_import_3d_f64(tmp, out_size, path);
    if (rc == ALWAN_OK) {
        size_t total = (size_t)(*out_size) * (size_t)(*out_size) * (size_t)(*out_size) * 3;
        for (size_t i = 0; i < total; i++) lut[i] = (alwan_f32)tmp[i];
    }
    free(tmp);
    return rc;
}

int alwan_cube_import_1d_f32(alwan_f32 *lut, int *out_size, char const *path) {
    if (!lut || !out_size || !path) return ALWAN_E_INVALID;
    alwan_f64 *tmp = (alwan_f64 *)malloc(65536 * sizeof(alwan_f64));
    if (!tmp) return ALWAN_E_NOMEM;
    int rc = alwan_cube_import_1d_f64(tmp, out_size, path);
    if (rc == ALWAN_OK) {
        for (int i = 0; i < *out_size; i++) lut[i] = (alwan_f32)tmp[i];
    }
    free(tmp);
    return rc;
}

int alwan_cube_import_3d_buffer_f32(alwan_f32 *lut, int *out_size,
                                 char const *buf, size_t buf_len) {
    if (!lut || !out_size || !buf || buf_len == 0) return ALWAN_E_INVALID;
    size_t max_total = 256ULL * 256ULL * 256ULL * 3ULL;
    alwan_f64 *tmp = (alwan_f64 *)malloc(max_total * sizeof(alwan_f64));
    if (!tmp) return ALWAN_E_NOMEM;
    int rc = alwan_cube_import_3d_buffer_f64(tmp, out_size, buf, buf_len);
    if (rc == ALWAN_OK) {
        size_t total = (size_t)(*out_size) * (size_t)(*out_size) * (size_t)(*out_size) * 3;
        for (size_t i = 0; i < total; i++) lut[i] = (alwan_f32)tmp[i];
    }
    free(tmp);
    return rc;
}
