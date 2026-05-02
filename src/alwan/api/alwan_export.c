/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * LUT export — .cube file writer
 * Per-pixel LUT math in alwan_lut_core.h
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_lut_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * .cube file export (Adobe/Iridas format)
 *
 * Reference: Resolve/Nuke .cube specification
 * Header:
 *   TITLE "name"
 *   LUT_3D_SIZE N       (or LUT_1D_SIZE N)
 *   DOMAIN_MIN 0.0 0.0 0.0
 *   DOMAIN_MAX 1.0 1.0 1.0
 * Data:
 *   R G B (one triplet per line, R varies fastest)
 * ---------------------------------------------------------------- */

int alwan_cube_export_3d_f64(char const *path,
                          alwan_f64 const *lut,
                          int size,
                          char const *title) {
    if (!path || !lut || size < 2 || size > 256) return ALWAN_E_INVALID;

    FILE *f = fopen(path, "w");
    if (!f) return ALWAN_E_INVALID;

    /* Header */
    if (title && title[0]) {
        fprintf(f, "TITLE \"%s\"\n", title);
    }
    fprintf(f, "LUT_3D_SIZE %d\n", size);
    fprintf(f, "DOMAIN_MIN 0.0 0.0 0.0\n");
    fprintf(f, "DOMAIN_MAX 1.0 1.0 1.0\n");
    fprintf(f, "\n");

    /* Data: R varies fastest, then G, then B */
    size_t const total = (size_t)size * (size_t)size * (size_t)size;
    for (size_t i = 0; i < total; i++) {
        fprintf(f, "%.10f %.10f %.10f\n",
                (double)lut[i * 3 + 0],
                (double)lut[i * 3 + 1],
                (double)lut[i * 3 + 2]);
    }

    fclose(f);
    return ALWAN_OK;
}

int alwan_cube_export_1d_f64(char const *path,
                          alwan_f64 const *lut,
                          int size,
                          char const *title) {
    if (!path || !lut || size < 2 || size > 65536) return ALWAN_E_INVALID;

    FILE *f = fopen(path, "w");
    if (!f) return ALWAN_E_INVALID;

    /* Header */
    if (title && title[0]) {
        fprintf(f, "TITLE \"%s\"\n", title);
    }
    fprintf(f, "LUT_1D_SIZE %d\n", size);
    fprintf(f, "DOMAIN_MIN 0.0\n");
    fprintf(f, "DOMAIN_MAX 1.0\n");
    fprintf(f, "\n");

    /* Data: one value per line (applied to all channels equally) */
    for (int i = 0; i < size; i++) {
        fprintf(f, "%.10f %.10f %.10f\n",
                (double)lut[i], (double)lut[i], (double)lut[i]);
    }

    fclose(f);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * .cube file export to memory buffer
 * ---------------------------------------------------------------- */

int alwan_cube_export_3d_buffer_f64(char *buf, size_t buf_size, size_t *bytes_written,
                                 alwan_f64 const *lut,
                                 int size,
                                 char const *title) {
    if (!buf || !lut || !bytes_written || size < 2 || size > 256 || buf_size == 0) {
        return ALWAN_E_INVALID;
    }

    size_t pos = 0;
    int n;

    /* Header */
    if (title && title[0]) {
        n = snprintf(buf + pos, buf_size - pos, "TITLE \"%s\"\n", title);
        if (n < 0 || pos + (size_t)n >= buf_size) return ALWAN_E_RANGE;
        pos += (size_t)n;
    }

    n = snprintf(buf + pos, buf_size - pos,
                 "LUT_3D_SIZE %d\nDOMAIN_MIN 0.0 0.0 0.0\nDOMAIN_MAX 1.0 1.0 1.0\n\n",
                 size);
    if (n < 0 || pos + (size_t)n >= buf_size) return ALWAN_E_RANGE;
    pos += (size_t)n;

    /* Data */
    size_t const total = (size_t)size * (size_t)size * (size_t)size;
    for (size_t i = 0; i < total; i++) {
        n = snprintf(buf + pos, buf_size - pos, "%.10f %.10f %.10f\n",
                     (double)lut[i * 3 + 0],
                     (double)lut[i * 3 + 1],
                     (double)lut[i * 3 + 2]);
        if (n < 0 || pos + (size_t)n >= buf_size) return ALWAN_E_RANGE;
        pos += (size_t)n;
    }

    *bytes_written = pos;
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * f32 wrappers — widen to f64 and delegate.
 * ---------------------------------------------------------------- */

int alwan_cube_export_3d_f32(char const *path, alwan_f32 const *lut, int size, char const *title) {
    if (!path || !lut || size < 2 || size > 256) return ALWAN_E_INVALID;
    size_t total = (size_t)size * (size_t)size * (size_t)size;
    alwan_f64 *tmp = (alwan_f64 *)malloc(total * 3 * sizeof(alwan_f64));
    if (!tmp) return ALWAN_E_NOMEM;
    for (size_t i = 0; i < total * 3; i++) tmp[i] = (alwan_f64)lut[i];
    int rc = alwan_cube_export_3d_f64(path, tmp, size, title);
    free(tmp);
    return rc;
}

int alwan_cube_export_1d_f32(char const *path, alwan_f32 const *lut, int size, char const *title) {
    if (!path || !lut || size < 2 || size > 65536) return ALWAN_E_INVALID;
    alwan_f64 *tmp = (alwan_f64 *)malloc((size_t)size * sizeof(alwan_f64));
    if (!tmp) return ALWAN_E_NOMEM;
    for (int i = 0; i < size; i++) tmp[i] = (alwan_f64)lut[i];
    int rc = alwan_cube_export_1d_f64(path, tmp, size, title);
    free(tmp);
    return rc;
}

int alwan_cube_export_3d_buffer_f32(char *buf, size_t buf_size, size_t *bytes_written,
                                 alwan_f32 const *lut, int size, char const *title) {
    if (!buf || !lut || !bytes_written || size < 2 || size > 256 || buf_size == 0) return ALWAN_E_INVALID;
    size_t total = (size_t)size * (size_t)size * (size_t)size;
    alwan_f64 *tmp = (alwan_f64 *)malloc(total * 3 * sizeof(alwan_f64));
    if (!tmp) return ALWAN_E_NOMEM;
    for (size_t i = 0; i < total * 3; i++) tmp[i] = (alwan_f64)lut[i];
    int rc = alwan_cube_export_3d_buffer_f64(buf, buf_size, bytes_written, tmp, size, title);
    free(tmp);
    return rc;
}
