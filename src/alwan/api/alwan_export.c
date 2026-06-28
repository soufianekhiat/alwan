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
#include <locale.h>

/* ----------------------------------------------------------------
 * Determinism / portability hardening (Option 2):
 *
 * - All file opens use "wb" / "rb" (binary mode). This stops MSVC's
 *   text-mode CRLF translation so the on-disk content matches the
 *   in-memory buffer byte-for-byte. Line endings are always written
 *   explicitly as "\n".
 *
 * - Numeric formatting (printf/snprintf %f, %g, %e) honors LC_NUMERIC,
 *   so a host locale that uses "," as the decimal separator would
 *   produce unparseable .cube / .clf output. The local_c_numeric
 *   helpers below save the caller's LC_NUMERIC, switch to "C" for
 *   the export block, and restore on exit. NOTE: setlocale is a
 *   process-global; concurrent threads using LC_NUMERIC during the
 *   export will see "C". This is acceptable for LUT export (one-shot,
 *   not per-pixel) and cannot be avoided portably in C99/C11.
 *
 * - All numeric data is formatted with %.17g (f64) or %.9g (f32) so
 *   the round-trip through .cube/.clf is exact (lossless to nearest
 *   IEEE-754 double / float).
 * ---------------------------------------------------------------- */

static char *alwan_save_lc_numeric(void) {
    char const *cur = setlocale(LC_NUMERIC, NULL);
    if (!cur) return NULL;
    size_t n = strlen(cur);
    char *saved = (char *)ALWAN_ALLOC(n + 1, 1);
    if (!saved) return NULL;
    memcpy(saved, cur, n + 1);
    setlocale(LC_NUMERIC, "C");
    return saved;
}

static void alwan_restore_lc_numeric(char *saved) {
    if (saved) {
        setlocale(LC_NUMERIC, saved);
        ALWAN_FREE(saved);
    }
}

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

    FILE *f = fopen(path, "wb");
    if (!f) return ALWAN_E_INVALID;

    char *saved_locale = alwan_save_lc_numeric();

    /* Header */
    if (title && title[0]) {
        fprintf(f, "TITLE \"%s\"\n", title);
    }
    fprintf(f, "LUT_3D_SIZE %d\n", size);
    fprintf(f, "DOMAIN_MIN 0.0 0.0 0.0\n");
    fprintf(f, "DOMAIN_MAX 1.0 1.0 1.0\n");
    fprintf(f, "\n");

    /* Data: R varies fastest, then G, then B. %.17g preserves f64
     * round-trip exactly. */
    size_t const total = (size_t)size * (size_t)size * (size_t)size;
    for (size_t i = 0; i < total; i++) {
        fprintf(f, "%.17g %.17g %.17g\n",
                (double)lut[i * 3 + 0],
                (double)lut[i * 3 + 1],
                (double)lut[i * 3 + 2]);
    }

    alwan_restore_lc_numeric(saved_locale);
    fclose(f);
    return ALWAN_OK;
}

int alwan_cube_export_1d_f64(char const *path,
                          alwan_f64 const *lut,
                          int size,
                          char const *title) {
    if (!path || !lut || size < 2 || size > 65536) return ALWAN_E_INVALID;

    FILE *f = fopen(path, "wb");
    if (!f) return ALWAN_E_INVALID;

    char *saved_locale = alwan_save_lc_numeric();

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
        fprintf(f, "%.17g %.17g %.17g\n",
                (double)lut[i], (double)lut[i], (double)lut[i]);
    }

    alwan_restore_lc_numeric(saved_locale);
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

    char *saved_locale = alwan_save_lc_numeric();

    size_t pos = 0;
    int n;
    int err = ALWAN_OK;

    /* Header */
    if (title && title[0]) {
        n = snprintf(buf + pos, buf_size - pos, "TITLE \"%s\"\n", title);
        if (n < 0 || pos + (size_t)n >= buf_size) { err = ALWAN_E_RANGE; goto done; }
        pos += (size_t)n;
    }

    n = snprintf(buf + pos, buf_size - pos,
                 "LUT_3D_SIZE %d\nDOMAIN_MIN 0.0 0.0 0.0\nDOMAIN_MAX 1.0 1.0 1.0\n\n",
                 size);
    if (n < 0 || pos + (size_t)n >= buf_size) { err = ALWAN_E_RANGE; goto done; }
    pos += (size_t)n;

    /* Data — %.17g for lossless f64 round-trip. */
    size_t const total = (size_t)size * (size_t)size * (size_t)size;
    for (size_t i = 0; i < total; i++) {
        n = snprintf(buf + pos, buf_size - pos, "%.17g %.17g %.17g\n",
                     (double)lut[i * 3 + 0],
                     (double)lut[i * 3 + 1],
                     (double)lut[i * 3 + 2]);
        if (n < 0 || pos + (size_t)n >= buf_size) { err = ALWAN_E_RANGE; goto done; }
        pos += (size_t)n;
    }

    *bytes_written = pos;

done:
    alwan_restore_lc_numeric(saved_locale);
    return err;
}

/* ----------------------------------------------------------------
 * f32 variants — native single-precision writers.
 *
 * These format directly from the f32 LUT using %.9g (lossless f32
 * round-trip) rather than widening to f64 and emitting %.17g. This
 * keeps the f32 path genuinely single-precision and avoids a large
 * temporary f64 buffer (previously up to 256^3*3*8 bytes ~= 400 MB).
 * ---------------------------------------------------------------- */

int alwan_cube_export_3d_f32(char const *path, alwan_f32 const *lut, int size, char const *title) {
    if (!path || !lut || size < 2 || size > 256) return ALWAN_E_INVALID;

    FILE *f = fopen(path, "wb");
    if (!f) return ALWAN_E_INVALID;

    char *saved_locale = alwan_save_lc_numeric();

    if (title && title[0]) {
        fprintf(f, "TITLE \"%s\"\n", title);
    }
    fprintf(f, "LUT_3D_SIZE %d\n", size);
    fprintf(f, "DOMAIN_MIN 0.0 0.0 0.0\n");
    fprintf(f, "DOMAIN_MAX 1.0 1.0 1.0\n");
    fprintf(f, "\n");

    size_t const total = (size_t)size * (size_t)size * (size_t)size;
    for (size_t i = 0; i < total; i++) {
        fprintf(f, "%.9g %.9g %.9g\n",
                (double)lut[i * 3 + 0],
                (double)lut[i * 3 + 1],
                (double)lut[i * 3 + 2]);
    }

    alwan_restore_lc_numeric(saved_locale);
    fclose(f);
    return ALWAN_OK;
}

int alwan_cube_export_1d_f32(char const *path, alwan_f32 const *lut, int size, char const *title) {
    if (!path || !lut || size < 2 || size > 65536) return ALWAN_E_INVALID;

    FILE *f = fopen(path, "wb");
    if (!f) return ALWAN_E_INVALID;

    char *saved_locale = alwan_save_lc_numeric();

    if (title && title[0]) {
        fprintf(f, "TITLE \"%s\"\n", title);
    }
    fprintf(f, "LUT_1D_SIZE %d\n", size);
    fprintf(f, "DOMAIN_MIN 0.0\n");
    fprintf(f, "DOMAIN_MAX 1.0\n");
    fprintf(f, "\n");

    for (int i = 0; i < size; i++) {
        fprintf(f, "%.9g %.9g %.9g\n",
                (double)lut[i], (double)lut[i], (double)lut[i]);
    }

    alwan_restore_lc_numeric(saved_locale);
    fclose(f);
    return ALWAN_OK;
}

int alwan_cube_export_3d_buffer_f32(char *buf, size_t buf_size, size_t *bytes_written,
                                 alwan_f32 const *lut, int size, char const *title) {
    if (!buf || !lut || !bytes_written || size < 2 || size > 256 || buf_size == 0) return ALWAN_E_INVALID;

    char *saved_locale = alwan_save_lc_numeric();

    size_t pos = 0;
    int n;
    int err = ALWAN_OK;

    if (title && title[0]) {
        n = snprintf(buf + pos, buf_size - pos, "TITLE \"%s\"\n", title);
        if (n < 0 || pos + (size_t)n >= buf_size) { err = ALWAN_E_RANGE; goto done_f32; }
        pos += (size_t)n;
    }

    n = snprintf(buf + pos, buf_size - pos,
                 "LUT_3D_SIZE %d\nDOMAIN_MIN 0.0 0.0 0.0\nDOMAIN_MAX 1.0 1.0 1.0\n\n",
                 size);
    if (n < 0 || pos + (size_t)n >= buf_size) { err = ALWAN_E_RANGE; goto done_f32; }
    pos += (size_t)n;

    size_t const total = (size_t)size * (size_t)size * (size_t)size;
    for (size_t i = 0; i < total; i++) {
        n = snprintf(buf + pos, buf_size - pos, "%.9g %.9g %.9g\n",
                     (double)lut[i * 3 + 0],
                     (double)lut[i * 3 + 1],
                     (double)lut[i * 3 + 2]);
        if (n < 0 || pos + (size_t)n >= buf_size) { err = ALWAN_E_RANGE; goto done_f32; }
        pos += (size_t)n;
    }

    *bytes_written = pos;

done_f32:
    alwan_restore_lc_numeric(saved_locale);
    return err;
}
