/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * LUT interchange beyond .cube: Sony Pictures Imageworks .spi1d and .spi3d,
 * and Autodesk .3dl (the Flame and Lustre flavours).
 *
 * The .cube reader and writer live in alwan_import.c and alwan_export.c and
 * set the conventions this unit follows: files are opened in binary mode so
 * the bytes on disk are the bytes in memory, LC_NUMERIC is saved and set to
 * "C" around every parse and format block so a host locale with "," as the
 * decimal separator cannot silently change the numbers, and every value that
 * parses is checked finite before it is stored, because sscanf("%lf") turns
 * "1e999" into an infinity and "nan" into a NaN without failing.
 *
 * Parsing runs in double on both precisions and narrows on store. These files
 * carry decimal text rather than a float32 bit pattern, so there is nothing
 * for an f32 parse to preserve, and one body per format cannot drift from its
 * twin the way two hand-written ones can.
 *
 * Cube ordering. alwan holds a 3D LUT R-fastest, the order .cube uses, so the
 * sample at grid position (r, g, b) is at ((b * size + g) * size + r) * 3.
 * .spi3d and .3dl are both B-fastest on disk. Both directions transpose here,
 * and suite 161 holds each one to OCIO's own evaluation of the same file,
 * which is the check that catches an ordering written backwards: a symmetric
 * LUT would pass a round trip through alwan alone and still be wrong.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>

/* ---------------------------------------------------------------- locale */

static char *lutfmt_save_lc_numeric(void) {
    char const *cur = setlocale(LC_NUMERIC, NULL);
    size_t n;
    char *saved;
    if (!cur) return NULL;
    n = strlen(cur);
    saved = (char *)ALWAN_ALLOC(n + 1, 1);
    if (!saved) return NULL;
    memcpy(saved, cur, n + 1);
    setlocale(LC_NUMERIC, "C");
    return saved;
}

static void lutfmt_restore_lc_numeric(char *saved) {
    if (saved) {
        setlocale(LC_NUMERIC, saved);
        ALWAN_FREE(saved);
    }
}

/* ---------------------------------------------------------------- scalars */

/* A parsed value has to be finite before it is stored. The bound is 1e308
 * rather than a comparison against HUGE_VAL so it still holds under a compiler
 * told to assume finite math. */
static int lutfmt_finite(double v) {
    return (v == v) && (v > -1e308) && (v < 1e308);
}

/* One body per format writes through this, so the f32 and f64 entry points are
 * the same code rather than two copies that can drift. */
typedef struct {
    void *p;
    int is_f32;
} lutfmt_sink;

static void lutfmt_put(lutfmt_sink const *s, size_t i, double v) {
    if (s->is_f32) ((alwan_f32 *)s->p)[i] = (alwan_f32)v;
    else ((alwan_f64 *)s->p)[i] = v;
}

typedef struct {
    void const *p;
    int is_f32;
} lutfmt_source;

static double lutfmt_get(lutfmt_source const *s, size_t i) {
    return s->is_f32 ? (double)((alwan_f32 const *)s->p)[i] : ((alwan_f64 const *)s->p)[i];
}

static char const *lutfmt_skip_ws(char const *s) {
    while (*s && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')) s++;
    return s;
}

/* Blank, or a comment. .spi3d and .3dl have no comment syntax of their own;
 * '#' is accepted because tools add it and a reader that dies on a comment is
 * a reader people work around by editing files. */
static int lutfmt_blank(char const *line) {
    char const *s = lutfmt_skip_ws(line);
    return *s == '\0' || *s == '#';
}

/* Cube index: alwan is R-fastest, both of these formats are B-fastest. */
static size_t lutfmt_rgb_index(int r, int g, int b, int size) {
    return (((size_t)b * (size_t)size + (size_t)g) * (size_t)size + (size_t)r) * 3;
}

/* ================================================================
 * .spi1d
 *
 *   Version 1
 *   From <lo> <hi>
 *   Length <N>
 *   Components <1 or 3>
 *   {
 *       v [v v]          N lines
 *   }
 *
 * "From" is the input domain the table is sampled over. alwan's 1D samplers
 * address [0, 1], so the domain is reported rather than applied: a file whose
 * domain is not [0, 1] needs the caller to map before sampling, and the
 * header says so.
 * ================================================================ */

static alwan_status spi1d_import(lutfmt_sink const *sink, int *out_size, int *out_channels,
                                 double *out_domain, char const *path) {
    FILE *f;
    char *saved;
    char line[512];
    int status = ALWAN_OK;
    int size = 0, channels = 0, in_body = 0;
    long row = 0;
    double lo = 0.0, hi = 1.0;

    if (!out_size || !path) return ALWAN_E_INVALID;
    f = fopen(path, "rb");
    if (!f) return ALWAN_E_INVALID;
    saved = lutfmt_save_lc_numeric();

    while (fgets(line, sizeof line, f)) {
        char const *s = lutfmt_skip_ws(line);
        if (lutfmt_blank(s)) continue;
        if (!in_body) {
            if (strncmp(s, "Version", 7) == 0) continue;
            if (strncmp(s, "From", 4) == 0) {
                if (sscanf(s + 4, "%lf %lf", &lo, &hi) != 2 || !lutfmt_finite(lo) || !lutfmt_finite(hi)) {
                    status = ALWAN_E_INVALID; goto done;
                }
                continue;
            }
            if (strncmp(s, "Length", 6) == 0) {
                size = atoi(s + 6);
                if (size < 2 || size > 65536) { status = ALWAN_E_RANGE; goto done; }
                continue;
            }
            if (strncmp(s, "Components", 10) == 0) {
                channels = atoi(s + 10);
                if (channels != 1 && channels != 3) { status = ALWAN_E_RANGE; goto done; }
                continue;
            }
            if (*s == '{') {
                if (size == 0) { status = ALWAN_E_NODATA; goto done; }
                if (channels == 0) channels = 1;   /* the field is optional in the wild */
                *out_size = size;
                if (out_channels) *out_channels = channels;
                if (out_domain) { out_domain[0] = lo; out_domain[1] = hi; }
                if (!sink->p) goto done;           /* size query: header only */
                in_body = 1;
                continue;
            }
            continue;                              /* an unknown header line is ignored */
        }
        if (*s == '}') break;
        if (row >= (long)size) { status = ALWAN_E_RANGE; goto done; }
        {
            double v[3];
            int n = sscanf(s, "%lf %lf %lf", &v[0], &v[1], &v[2]);
            int c;
            if (n < channels) { status = ALWAN_E_INVALID; goto done; }
            for (c = 0; c < channels; c++) {
                if (!lutfmt_finite(v[c])) { status = ALWAN_E_INVALID; goto done; }
                lutfmt_put(sink, (size_t)row * (size_t)channels + (size_t)c, v[c]);
            }
        }
        row++;
    }
    if (sink->p && in_body && row != (long)size) status = ALWAN_E_RANGE;
    if (!in_body && size == 0) status = ALWAN_E_NODATA;

done:
    lutfmt_restore_lc_numeric(saved);
    fclose(f);
    return status;
}

static alwan_status spi1d_export(char const *path, lutfmt_source const *src, int size, int channels,
                                 double lo, double hi) {
    FILE *f;
    char *saved;
    int i, c;
    char const *fmt;

    if (!path || !src->p) return ALWAN_E_INVALID;
    if (size < 2 || size > 65536) return ALWAN_E_RANGE;
    if (channels != 1 && channels != 3) return ALWAN_E_RANGE;
    if (!lutfmt_finite(lo) || !lutfmt_finite(hi) || !(hi > lo)) return ALWAN_E_INVALID;

    f = fopen(path, "wb");
    if (!f) return ALWAN_E_INVALID;
    saved = lutfmt_save_lc_numeric();
    fmt = src->is_f32 ? " %.9g" : " %.17g";

    fprintf(f, "Version 1\n");
    fprintf(f, src->is_f32 ? "From %.9g %.9g\n" : "From %.17g %.17g\n", lo, hi);
    fprintf(f, "Length %d\n", size);
    fprintf(f, "Components %d\n", channels);
    fprintf(f, "{\n");
    for (i = 0; i < size; i++) {
        fprintf(f, "       ");
        for (c = 0; c < channels; c++) {
            fprintf(f, fmt, lutfmt_get(src, (size_t)i * (size_t)channels + (size_t)c));
        }
        fprintf(f, "\n");
    }
    fprintf(f, "}\n");

    lutfmt_restore_lc_numeric(saved);
    if (fclose(f) != 0) return ALWAN_E_INVALID;
    return ALWAN_OK;
}

/* ================================================================
 * .spi3d
 *
 *   SPILUT 1.0
 *   3 3                  input and output component counts
 *   N N N                cube dimensions, cubic here
 *   i j k  r g b         one line per sample, i = red, k = blue
 *
 * The indices are explicit, so a reader does not depend on the line order.
 * alwan writes them k-fastest, which is what the format's own writers do.
 * ================================================================ */

static alwan_status spi3d_import(lutfmt_sink const *sink, int *out_size, char const *path) {
    FILE *f;
    char *saved;
    char line[512];
    int status = ALWAN_OK;
    int size = 0, seen_magic = 0, seen_dims = 0;
    long rows = 0;

    if (!out_size || !path) return ALWAN_E_INVALID;
    f = fopen(path, "rb");
    if (!f) return ALWAN_E_INVALID;
    saved = lutfmt_save_lc_numeric();

    while (fgets(line, sizeof line, f)) {
        char const *s = lutfmt_skip_ws(line);
        if (lutfmt_blank(s)) continue;
        if (!seen_magic) {
            if (strncmp(s, "SPILUT", 6) != 0) { status = ALWAN_E_NODATA; goto done; }
            seen_magic = 1;
            continue;
        }
        if (!seen_dims) {
            int a = 0, b = 0, c = 0;
            int n = sscanf(s, "%d %d %d", &a, &b, &c);
            if (n == 2) continue;                  /* the component-count line */
            if (n != 3) { status = ALWAN_E_INVALID; goto done; }
            if (a != b || b != c) { status = ALWAN_E_RANGE; goto done; }   /* cubic only */
            if (a < 2 || a > 256) { status = ALWAN_E_RANGE; goto done; }
            size = a;
            seen_dims = 1;
            *out_size = size;
            if (!sink->p) goto done;               /* size query: header only */
            continue;
        }
        {
            int ir, ig, ib;
            double r, g, b;
            if (sscanf(s, "%d %d %d %lf %lf %lf", &ir, &ig, &ib, &r, &g, &b) != 6) {
                status = ALWAN_E_INVALID; goto done;
            }
            if (ir < 0 || ig < 0 || ib < 0 || ir >= size || ig >= size || ib >= size) {
                status = ALWAN_E_RANGE; goto done;
            }
            if (!lutfmt_finite(r) || !lutfmt_finite(g) || !lutfmt_finite(b)) {
                status = ALWAN_E_INVALID; goto done;
            }
            {
                size_t at = lutfmt_rgb_index(ir, ig, ib, size);
                lutfmt_put(sink, at + 0, r);
                lutfmt_put(sink, at + 1, g);
                lutfmt_put(sink, at + 2, b);
            }
            rows++;
        }
    }
    if (!seen_dims) status = ALWAN_E_NODATA;
    else if (sink->p && rows != (long)size * size * size) status = ALWAN_E_RANGE;

done:
    lutfmt_restore_lc_numeric(saved);
    fclose(f);
    return status;
}

static alwan_status spi3d_export(char const *path, lutfmt_source const *src, int size) {
    FILE *f;
    char *saved;
    int r, g, b;
    char const *fmt;

    if (!path || !src->p) return ALWAN_E_INVALID;
    if (size < 2 || size > 256) return ALWAN_E_RANGE;

    f = fopen(path, "wb");
    if (!f) return ALWAN_E_INVALID;
    saved = lutfmt_save_lc_numeric();
    fmt = src->is_f32 ? "%d %d %d %.9g %.9g %.9g\n" : "%d %d %d %.17g %.17g %.17g\n";

    fprintf(f, "SPILUT 1.0\n");
    fprintf(f, "3 3\n");
    fprintf(f, "%d %d %d\n", size, size, size);
    for (r = 0; r < size; r++) {
        for (g = 0; g < size; g++) {
            for (b = 0; b < size; b++) {
                size_t at = lutfmt_rgb_index(r, g, b, size);
                fprintf(f, fmt, r, g, b,
                        lutfmt_get(src, at + 0), lutfmt_get(src, at + 1), lutfmt_get(src, at + 2));
            }
        }
    }

    lutfmt_restore_lc_numeric(saved);
    if (fclose(f) != 0) return ALWAN_E_INVALID;
    return ALWAN_OK;
}

/* ================================================================
 * .3dl
 *
 * Two flavours of one format. Flame writes the mesh line and the data and
 * nothing else; Lustre puts "3DMESH" and "Mesh <e> <d>" above it, where the
 * cube edge is 2^e + 1 and d is the output bit depth, and "LUT8" and a gamma
 * line below it. Both hold integers, B-fastest, on a mesh whose own values
 * are the input grid (10-bit by convention and ignored here, since the grid
 * is regular and its length is the cube edge).
 *
 * The output bit depth is not written in the Flame flavour, so it is inferred
 * from the largest value present: the smallest of 8, 10, 12, 14 and 16 bits
 * that can hold it. A table whose largest value is 4000 therefore reads as
 * 12-bit, which is what every other reader does and what the writers intend;
 * a table that never reaches its own maximum reads one stop bright, and there
 * is nothing in the file to prevent that.
 *
 * Telling the mesh line from a data line is decidable, and OCIO 2.5 gets it
 * wrong at one size: with L non-blank lines, L a perfect cube means there is
 * no mesh line and L - 1 a perfect cube means there is one, and both cannot
 * hold at once because consecutive cubes differ by more than one. At size 3
 * the mesh line has three entries and OCIO's Flame reader counts it as data,
 * so it cannot read back the size-3 file it just wrote. alwan reads it.
 * ================================================================ */

static int lutfmt_cube_edge(long lines) {
    /* The integer cube root of lines, or 0 when lines is not a cube. */
    long n;
    if (lines < 8) return 0;
    for (n = 2; n <= 256; n++) {
        long cube = n * n * n;
        if (cube == lines) return (int)n;
        if (cube > lines) break;
    }
    return 0;
}

/* Numbers on one whitespace-separated line, and the largest of them. A mesh
 * line carries one entry per grid point, so its length is the cube edge and
 * any length other than three identifies it outright. */
static int tdl_line_numbers(char const *s, long *out_max) {
    int count = 0;
    long maxv = 0;
    while (*s) {
        char *end = NULL;
        long v;
        s = lutfmt_skip_ws(s);
        if (!*s) break;
        v = strtol(s, &end, 10);
        if (end == s) break;                           /* not a number: stop here */
        if (v > maxv) maxv = v;
        count++;
        s = end;
    }
    if (out_max) *out_max = maxv;
    return count;
}

/* A numeric line, a Lustre keyword, or neither. */
static int tdl_is_numeric(char const *s) {
    return isdigit((unsigned char)*s) || *s == '-' || *s == '+';
}

/* One pass over the file: the mesh line's length where it has one, the Lustre
 * header's declared edge exponent and output depth, the count of three-number
 * data lines, and the largest value any of them holds. */
static void tdl_scan(FILE *f, long *out_lines, long *out_max, int *out_mesh_len,
                     int *out_declared_e, int *out_declared_depth) {
    char line[4096];
    long lines = 0, maxv = 0;
    int mesh_len = 0, decl_e = 0, decl_d = 0;
    while (fgets(line, sizeof line, f)) {
        char const *s = lutfmt_skip_ws(line);
        if (lutfmt_blank(s)) continue;
        if (strncmp(s, "3DMESH", 6) == 0) continue;
        if (strncmp(s, "Mesh", 4) == 0) {
            int e = 0, d = 0;
            if (sscanf(s + 4, "%d %d", &e, &d) == 2) {
                if (e >= 1 && e <= 8) decl_e = e;
                if (d >= 8 && d <= 16) decl_d = d;
            }
            continue;
        }
        if (strncmp(s, "LUT", 3) == 0) break;          /* Lustre trailer */
        if (!tdl_is_numeric(s)) continue;
        {
            long m = 0;
            int n = tdl_line_numbers(s, &m);
            if (n == 3) {
                if (m > maxv) maxv = m;
                lines++;
            } else if (n >= 2 && !mesh_len) {
                mesh_len = n;                          /* the input grid */
            }
        }
    }
    *out_lines = lines;
    *out_max = maxv;
    *out_mesh_len = mesh_len;
    *out_declared_e = decl_e;
    *out_declared_depth = decl_d;
}

static int tdl_depth_for(long maxv) {
    if (maxv <= 255) return 8;
    if (maxv <= 1023) return 10;
    if (maxv <= 4095) return 12;
    if (maxv <= 16383) return 14;
    return 16;
}

static alwan_status tdl_import(lutfmt_sink const *sink, int *out_size, char const *path) {
    FILE *f;
    char *saved;
    char line[4096];
    int status = ALWAN_OK;
    long lines = 0, maxv = 0;
    int mesh_len = 0, decl_e = 0, decl_d = 0, size = 0, skip_first = 0, depth;
    double scale;
    long row = 0;

    if (!out_size || !path) return ALWAN_E_INVALID;
    f = fopen(path, "rb");
    if (!f) return ALWAN_E_INVALID;
    saved = lutfmt_save_lc_numeric();

    tdl_scan(f, &lines, &maxv, &mesh_len, &decl_e, &decl_d);

    /* The mesh line names the edge outright wherever it is not three numbers
     * long, and the Lustre header names it as 2^e + 1. Only a bare size-3 file
     * needs the line arithmetic: with L three-number lines, L a cube means
     * every line is data and L - 1 a cube means the first one is the mesh, and
     * both cannot hold at once. */
    if (mesh_len >= 2) {
        size = mesh_len;
    } else if (decl_e) {
        size = (1 << decl_e) + 1;
        skip_first = 1;
    } else {
        size = lutfmt_cube_edge(lines);
        if (!size) { size = lutfmt_cube_edge(lines - 1); skip_first = 1; }
    }
    if (size < 2 || size > 256) { status = ALWAN_E_NODATA; goto done; }
    if (mesh_len >= 2 && lines != (long)size * size * size) { status = ALWAN_E_RANGE; goto done; }
    *out_size = size;
    if (!sink->p) goto done;                            /* size query */

    depth = decl_d ? decl_d : tdl_depth_for(maxv);
    scale = 1.0 / (double)((1L << depth) - 1);

    rewind(f);
    while (fgets(line, sizeof line, f)) {
        char const *s = lutfmt_skip_ws(line);
        long v[3];
        int n;
        if (lutfmt_blank(s)) continue;
        if (strncmp(s, "3DMESH", 6) == 0) continue;
        if (strncmp(s, "Mesh", 4) == 0) continue;
        if (strncmp(s, "LUT", 3) == 0) break;
        if (!tdl_is_numeric(s)) continue;
        n = tdl_line_numbers(s, NULL);
        if (n != 3) continue;                           /* a mesh line of any other length */
        if (sscanf(s, "%ld %ld %ld", &v[0], &v[1], &v[2]) < 3) continue;
        if (skip_first) { skip_first = 0; continue; }   /* the three-number mesh line */
        if (row >= (long)size * size * size) { status = ALWAN_E_RANGE; goto done; }
        {
            /* B-fastest on disk, R-fastest in memory. */
            int b = (int)(row % size);
            int g = (int)((row / size) % size);
            int r = (int)(row / ((long)size * size));
            size_t at = lutfmt_rgb_index(r, g, b, size);
            lutfmt_put(sink, at + 0, (double)v[0] * scale);
            lutfmt_put(sink, at + 1, (double)v[1] * scale);
            lutfmt_put(sink, at + 2, (double)v[2] * scale);
        }
        row++;
    }
    if (row != (long)size * size * size) status = ALWAN_E_RANGE;

done:
    lutfmt_restore_lc_numeric(saved);
    fclose(f);
    return status;
}

static alwan_status tdl_export(char const *path, lutfmt_source const *src, int size, int bit_depth) {
    FILE *f;
    char *saved;
    int r, g, b, e;
    long maxcode;

    if (!path || !src->p) return ALWAN_E_INVALID;
    if (size < 2 || size > 256) return ALWAN_E_RANGE;
    if (bit_depth <= 0) bit_depth = 12;
    if (bit_depth != 8 && bit_depth != 10 && bit_depth != 12 && bit_depth != 14 && bit_depth != 16) {
        return ALWAN_E_RANGE;
    }
    /* Lustre states the edge as 2^e + 1, so the header goes on only where the
     * size is one more than a power of two. 3, 5, 17, 33 and 65 are; anything
     * else is written in the Flame flavour, where the mesh line's own length
     * is the edge and no header is needed to say so. */
    for (e = 1; e <= 8; e++) if ((1 << e) + 1 == size) break;

    maxcode = (1L << bit_depth) - 1;
    f = fopen(path, "wb");
    if (!f) return ALWAN_E_INVALID;
    saved = lutfmt_save_lc_numeric();

    if (e <= 8) {
        fprintf(f, "3DMESH\n");
        fprintf(f, "Mesh %d %d\n", e, bit_depth);
    }
    for (r = 0; r < size; r++) {
        fprintf(f, r ? " %ld" : "%ld", (long)((double)r * 1023.0 / (double)(size - 1) + 0.5));
    }
    fprintf(f, "\n");
    for (r = 0; r < size; r++) {
        for (g = 0; g < size; g++) {
            for (b = 0; b < size; b++) {
                size_t at = lutfmt_rgb_index(r, g, b, size);
                long o[3];
                int c;
                for (c = 0; c < 3; c++) {
                    double v = lutfmt_get(src, at + (size_t)c) * (double)maxcode + 0.5;
                    if (!(v > 0.0)) v = 0.0;               /* NaN lands here too */
                    if (v > (double)maxcode) v = (double)maxcode;
                    o[c] = (long)v;
                }
                fprintf(f, "%ld %ld %ld\n", o[0], o[1], o[2]);
            }
        }
    }
    if (e <= 8) fprintf(f, "\nLUT8\ngamma 1.0\n");

    lutfmt_restore_lc_numeric(saved);
    if (fclose(f) != 0) return ALWAN_E_INVALID;
    return ALWAN_OK;
}

/* ================================================================
 * Public entry points: one pair per format per precision.
 * ================================================================ */

alwan_status alwan_spi1d_import_f64(alwan_f64 *lut, int *out_size, int *out_channels,
                                    alwan_f64 *out_domain, char const *path) {
    lutfmt_sink sink;
    double dom[2];
    alwan_status st;
    sink.p = lut; sink.is_f32 = 0;
    st = spi1d_import(&sink, out_size, out_channels, dom, path);
    if (st == ALWAN_OK && out_domain) { out_domain[0] = dom[0]; out_domain[1] = dom[1]; }
    return st;
}

alwan_status alwan_spi1d_import_f32(alwan_f32 *lut, int *out_size, int *out_channels,
                                    alwan_f32 *out_domain, char const *path) {
    lutfmt_sink sink;
    double dom[2];
    alwan_status st;
    sink.p = lut; sink.is_f32 = 1;
    st = spi1d_import(&sink, out_size, out_channels, dom, path);
    if (st == ALWAN_OK && out_domain) {
        out_domain[0] = (alwan_f32)dom[0];
        out_domain[1] = (alwan_f32)dom[1];
    }
    return st;
}

alwan_status alwan_spi1d_export_f64(char const *path, alwan_f64 const *lut, int size, int channels,
                                    alwan_f64 domain_min, alwan_f64 domain_max) {
    lutfmt_source src;
    src.p = lut; src.is_f32 = 0;
    return spi1d_export(path, &src, size, channels, domain_min, domain_max);
}

alwan_status alwan_spi1d_export_f32(char const *path, alwan_f32 const *lut, int size, int channels,
                                    alwan_f32 domain_min, alwan_f32 domain_max) {
    lutfmt_source src;
    src.p = lut; src.is_f32 = 1;
    return spi1d_export(path, &src, size, channels, (double)domain_min, (double)domain_max);
}

alwan_status alwan_spi3d_import_f64(alwan_f64 *lut, int *out_size, char const *path) {
    lutfmt_sink sink;
    sink.p = lut; sink.is_f32 = 0;
    return spi3d_import(&sink, out_size, path);
}

alwan_status alwan_spi3d_import_f32(alwan_f32 *lut, int *out_size, char const *path) {
    lutfmt_sink sink;
    sink.p = lut; sink.is_f32 = 1;
    return spi3d_import(&sink, out_size, path);
}

alwan_status alwan_spi3d_export_f64(char const *path, alwan_f64 const *lut, int size) {
    lutfmt_source src;
    src.p = lut; src.is_f32 = 0;
    return spi3d_export(path, &src, size);
}

alwan_status alwan_spi3d_export_f32(char const *path, alwan_f32 const *lut, int size) {
    lutfmt_source src;
    src.p = lut; src.is_f32 = 1;
    return spi3d_export(path, &src, size);
}

alwan_status alwan_3dl_import_f64(alwan_f64 *lut, int *out_size, char const *path) {
    lutfmt_sink sink;
    sink.p = lut; sink.is_f32 = 0;
    return tdl_import(&sink, out_size, path);
}

alwan_status alwan_3dl_import_f32(alwan_f32 *lut, int *out_size, char const *path) {
    lutfmt_sink sink;
    sink.p = lut; sink.is_f32 = 1;
    return tdl_import(&sink, out_size, path);
}

alwan_status alwan_3dl_export_f64(char const *path, alwan_f64 const *lut, int size, int bit_depth) {
    lutfmt_source src;
    src.p = lut; src.is_f32 = 0;
    return tdl_export(path, &src, size, bit_depth);
}

alwan_status alwan_3dl_export_f32(char const *path, alwan_f32 const *lut, int size, int bit_depth) {
    lutfmt_source src;
    src.p = lut; src.is_f32 = 1;
    return tdl_export(path, &src, size, bit_depth);
}
