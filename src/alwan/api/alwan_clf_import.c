/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * CLF import: read a ProcessList and evaluate it.
 *
 * CLF (Common LUT Format, SMPTE ST 2136-1) records the operations of a colour
 * transform rather than only their samples, which is why an LMT or an OCIO
 * transform survives a trip through it where a .cube does not. alwan wrote CLF
 * from the start; this reads it.
 *
 * Every ProcessNode type CLF defines is understood: Matrix, Range, Exponent,
 * LUT1D, LUT3D, ASC_CDL and Log. A file carrying a style one of them does not
 * implement is REFUSED rather than partly applied, because a pipeline missing
 * one of its stages is not the transform and is worse than no answer at all.
 *
 * The two added later, also measured rather than transcribed:
 *
 *   ASC_CDL      (in * slope + offset)^power per channel, then the saturation
 *                about the Rec. 709 luma. Fwd and Rev hold the SOP result in
 *                [0, 1] before the power and the final result after the
 *                saturation; the NoClamp pair do neither and pass a negative
 *                through the power unchanged rather than raising it.
 *   Log          log2, log10 and their inverses; linToLog and logToLin with
 *                the five parameters; and the camera pair, which adds a linear
 *                segment below linSideBreak whose slope is the curve's own
 *                slope there unless the file states one. A logarithm's input
 *                is floored at the smallest normal float32, as OCIO does, so
 *                log10(0) is -37.9298 rather than an infinity.
 *
 * Every number in here was measured against OpenColorIO reading the same file
 * rather than transcribed from the specification, and suite 162 pins them:
 *
 *   Range        out = (in - minIn) (maxOut - minOut) / (maxIn - minIn) + minOut,
 *                clamped to [minOut, maxOut] unless style="noClamp"
 *   basicFwd     max(x, 0)^g                    basicRev  max(x, 0)^(1/g)
 *   monCurveFwd  a linear segment below the breakpoint xb = a / (g - 1), then
 *                ((x + a) / (1 + a))^g above it, the segment's slope being
 *                yb / xb with yb the curve's value at xb, so value and slope
 *                both carry across. Negative input stays on that segment.
 *   monCurveRev  the same with the two axes exchanged.
 *
 * The XML is scanned rather than parsed by a general reader: CLF's shape is
 * known and shallow, and a dependency-free scanner that understands exactly
 * this shape is less surface than an XML library would be. The whole file is
 * read into memory first, so the sizes a header declares and the data that
 * follows cannot disagree between two passes.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_xml_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include <math.h>

enum {
    CLF_MAX_NODES = 256,          /* a ProcessList longer than this is refused */
    CLF_MAX_LUT1D = 65536,
    CLF_MAX_LUT3D = 256,
    CLF_NAME_CAP = 64,
    CLF_VALUE_CAP = 128,
    CLF_ID_CAP = 128
};

/* Exponent styles, in the order the spec names them. */
enum { CLF_BASIC_FWD = 0, CLF_BASIC_REV, CLF_MON_FWD, CLF_MON_REV };

/* ASC_CDL styles. The two clamping ones hold the SOP result and the final result
 * in [0, 1]; the others let both run. */
enum { CLF_CDL_FWD = 0, CLF_CDL_REV, CLF_CDL_FWD_NC, CLF_CDL_REV_NC };

/* Log styles. The camera pair carries a linear segment below linSideBreak. */
enum { CLF_LOG_LOG2 = 0, CLF_LOG_LOG10, CLF_LOG_ANTILOG2, CLF_LOG_ANTILOG10,
       CLF_LOG_LIN_TO_LOG, CLF_LOG_LOG_TO_LIN, CLF_LOG_CAM_LIN_TO_LOG, CLF_LOG_CAM_LOG_TO_LIN };

/* OCIO floors a logarithm's input at the smallest normal float32, so log10(0) comes
 * back as -37.9298 rather than an infinity. Measured, and matched here. */
#define CLF_LOG_FLOOR 1.17549435e-38

typedef struct {
    int type;                     /* alwan_clf_node_type */
    /* Matrix: row-major 3x4, the fourth column an offset that is zero for a 3x3 */
    double m[12];
    /* Range */
    double min_in, max_in, min_out, max_out;
    int clamp;
    /* Exponent, per channel because CLF allows a channel attribute */
    double g[3], a[3];
    int style;
    /* ASC_CDL */
    double slope[3], cdl_offset[3], power[3], sat;
    /* Log */
    double log_base, ls_slope, ls_offset, lin_slope, lin_offset, lin_break, linear_slope;
    int has_linear_slope;
    /* LUT1D: size * channels values. LUT3D: size^3 * 3, R-fastest in memory. */
    double *data;
    int size;
    int channels;
} clf_op;

struct alwan_clf_s {
    clf_op *ops;
    size_t count;
    char id[CLF_ID_CAP];
};

/* ---------------------------------------------------------------- locale */

static char *clf_save_lc_numeric(void) {
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

static void clf_restore_lc_numeric(char *saved) {
    if (saved) {
        setlocale(LC_NUMERIC, saved);
        ALWAN_FREE(saved);
    }
}

static int clf_finite(double v) {
    return (v == v) && (v > -1e308) && (v < 1e308);
}

/* --------------------------------------------------------------- scanning

 * The XML shape is scanned by alwan_xml_common.h, shared with the CxF3 reader.
 * What stays here is CLF's own: numbers, which this file parses with strtod
 * under a saved LC_NUMERIC where the chart reader hand-parses, and the node
 * semantics below. */

/* Exactly one number, wherever the scanner stands. */
static int clf_read_one(alwan__xml_scan *s, double *out) {
    char *endp = NULL;
    double v;
    alwan__xml_skip_space(s);
    if (s->p >= s->end || *s->p == '<') return 0;
    v = strtod(s->p, &endp);
    if (endp == s->p) return 0;
    s->p = endp;
    if (!clf_finite(v)) return 0;
    *out = v;
    return 1;
}

/* Read numbers until the closing tag of the element holding them. */
static int clf_read_numbers(alwan__xml_scan *s, double *out, size_t want, size_t *got) {
    size_t n = 0;
    for (;;) {
        char *endp = NULL;
        double v;
        alwan__xml_skip_space(s);
        if (s->p >= s->end) return 0;
        if (*s->p == '<') break;
        v = strtod(s->p, &endp);
        if (endp == s->p) return 0;
        s->p = endp;
        if (!clf_finite(v)) return 0;
        if (n < want) out[n] = v;
        n++;
    }
    *got = n;
    return n <= want;
}

/* ---------------------------------------------------------------- nodes */

static void clf_op_init(clf_op *op) {
    int i;
    memset(op, 0, sizeof *op);
    for (i = 0; i < 3; i++) { op->g[i] = 1.0; op->a[i] = 0.0; }
    op->clamp = 1;
    op->max_in = 1.0;
    op->max_out = 1.0;
    for (i = 0; i < 3; i++) { op->slope[i] = 1.0; op->cdl_offset[i] = 0.0; op->power[i] = 1.0; }
    op->sat = 1.0;
    op->log_base = 2.0;
    op->ls_slope = 1.0;
    op->lin_slope = 1.0;
    op->ls_offset = 0.0;
    op->lin_offset = 0.0;
    op->lin_break = 0.0;
    op->linear_slope = 0.0;
    op->has_linear_slope = 0;
}

static int clf_channel_index(char const *v) {
    if (strcmp(v, "R") == 0) return 0;
    if (strcmp(v, "G") == 0) return 1;
    if (strcmp(v, "B") == 0) return 2;
    return -1;
}

static alwan_status clf_parse_body(alwan__xml_scan *s, clf_op *op, char const *node_name);

/* Parse the body of one ProcessNode. The scanner sits just past its open tag. */
static alwan_status clf_parse_body(alwan__xml_scan *s, clf_op *op, char const *node_name) {
    char name[CLF_NAME_CAP], value[CLF_VALUE_CAP];
    (void)node_name;
    for (;;) {
        alwan__xml_skip_space(s);
        if (s->p >= s->end) return ALWAN_E_INVALID;
        if (alwan__xml_starts_with(s, "<!--")) {
            if (!alwan__xml_skip_past(s, "-->")) return ALWAN_E_INVALID;
            continue;
        }
        if (s->p + 1 < s->end && s->p[0] == '<' && s->p[1] == '/') {
            /* The node's own close tag ends it. */
            if (!alwan__xml_skip_past(s, ">")) return ALWAN_E_INVALID;
            return ALWAN_OK;
        }
        if (*s->p != '<') { s->p++; continue; }          /* stray text */
        s->p++;
        if (!alwan__xml_read_name(s, name, sizeof name)) return ALWAN_E_INVALID;

        if (strcmp(name, "Array") == 0) {
            int dims[4];
            int ndim = 0, self = 0, r;
            size_t want, got = 0;
            while ((r = alwan__xml_read_attr(s, name, CLF_NAME_CAP, value, CLF_VALUE_CAP, &self)) == 1) {
                if (strcmp(name, "dim") == 0) {
                    char const *q = value;
                    while (*q && ndim < 4) {
                        char *e = NULL;
                        long d = strtol(q, &e, 10);
                        if (e == q) break;
                        dims[ndim++] = (int)d;
                        q = e;
                        while (*q == ' ') q++;
                    }
                }
            }
            if (r < 0) return ALWAN_E_INVALID;
            if (op->type == ALWAN_CLF_NODE_MATRIX) {
                int rows, cols;
                if (ndim != 2) return ALWAN_E_INVALID;
                rows = dims[0]; cols = dims[1];
                if (rows != 3 || (cols != 3 && cols != 4)) return ALWAN_E_RANGE;
                want = (size_t)rows * (size_t)cols;
                {
                    double v[12];
                    int i, j;
                    if (!clf_read_numbers(s, v, want, &got) || got != want) return ALWAN_E_INVALID;
                    for (i = 0; i < 3; i++) {
                        for (j = 0; j < 3; j++) op->m[i * 4 + j] = v[i * cols + j];
                        op->m[i * 4 + 3] = (cols == 4) ? v[i * cols + 3] : 0.0;
                    }
                }
            } else if (op->type == ALWAN_CLF_NODE_LUT1D) {
                int channels;
                if (ndim != 2) return ALWAN_E_INVALID;
                op->size = dims[0];
                channels = dims[1];
                if (op->size < 2 || op->size > CLF_MAX_LUT1D) return ALWAN_E_RANGE;
                if (channels != 1 && channels != 3) return ALWAN_E_RANGE;
                op->channels = channels;
                want = (size_t)op->size * (size_t)channels;
                op->data = (double *)ALWAN_ALLOC(want * sizeof(double), sizeof(double));
                if (!op->data) return ALWAN_E_NOMEM;
                if (!clf_read_numbers(s, op->data, want, &got) || got != want) return ALWAN_E_INVALID;
            } else if (op->type == ALWAN_CLF_NODE_LUT3D) {
                if (ndim != 4) return ALWAN_E_INVALID;
                if (dims[0] != dims[1] || dims[1] != dims[2]) return ALWAN_E_RANGE;
                if (dims[3] != 3) return ALWAN_E_RANGE;
                op->size = dims[0];
                op->channels = 3;
                if (op->size < 2 || op->size > CLF_MAX_LUT3D) return ALWAN_E_RANGE;
                want = (size_t)op->size * op->size * op->size * 3;
                op->data = (double *)ALWAN_ALLOC(want * sizeof(double), sizeof(double));
                if (!op->data) return ALWAN_E_NOMEM;
                {
                    /* CLF writes a LUT3D with BLUE varying fastest and red slowest.
                     * alwan holds a cube the other way round, R-fastest, so the two
                     * are transposed here. Treating them as the same is exactly the
                     * mistake that shipped in alwan's own CLF writer until this was
                     * written; suite 162 pins the order against OCIO. */
                    int rr, gg, bb;
                    size_t k = 0;
                    for (rr = 0; rr < op->size; rr++) {
                        for (gg = 0; gg < op->size; gg++) {
                            for (bb = 0; bb < op->size; bb++) {
                                double v[3];
                                size_t at = (((size_t)bb * op->size + gg) * op->size + rr) * 3;
                                if (!clf_read_one(s, &v[0]) || !clf_read_one(s, &v[1]) ||
                                    !clf_read_one(s, &v[2])) {
                                    return ALWAN_E_INVALID;
                                }
                                op->data[at + 0] = v[0];
                                op->data[at + 1] = v[1];
                                op->data[at + 2] = v[2];
                                k++;
                            }
                        }
                    }
                    (void)k;
                }
            } else {
                return ALWAN_E_INVALID;                  /* an Array where none belongs */
            }
            if (!self && !alwan__xml_skip_element(s, "Array")) return ALWAN_E_INVALID;
            continue;
        }

        if (strcmp(name, "ExponentParams") == 0) {
            int self = 0, r, ch = -1;
            double g = 1.0, a = 0.0;
            int have_a = 0;
            while ((r = alwan__xml_read_attr(s, name, CLF_NAME_CAP, value, CLF_VALUE_CAP, &self)) == 1) {
                if (strcmp(name, "exponent") == 0) g = strtod(value, NULL);
                else if (strcmp(name, "offset") == 0) { a = strtod(value, NULL); have_a = 1; }
                else if (strcmp(name, "channel") == 0) ch = clf_channel_index(value);
            }
            if (r < 0) return ALWAN_E_INVALID;
            if (!clf_finite(g) || !clf_finite(a)) return ALWAN_E_INVALID;
            if ((op->style == CLF_MON_FWD || op->style == CLF_MON_REV)) {
                if (!have_a || !(a > 0.0)) return ALWAN_E_INVALID;   /* the segment needs it */
                if (!(g > 1.0)) return ALWAN_E_RANGE;                /* xb = a / (g - 1) */
            }
            if (ch < 0) {
                op->g[0] = op->g[1] = op->g[2] = g;
                op->a[0] = op->a[1] = op->a[2] = a;
            } else {
                op->g[ch] = g;
                op->a[ch] = a;
            }
            if (!self && !alwan__xml_skip_element(s, "ExponentParams")) return ALWAN_E_INVALID;
            continue;
        }

        if (strcmp(name, "SOPNode") == 0 || strcmp(name, "SatNode") == 0 ||
            strcmp(name, "SATNode") == 0) {
            /* These only group their children, so the same body parser reads them
             * and stops at their own close tag. */
            int self = 0, r;
            alwan_status st;
            while ((r = alwan__xml_read_attr(s, name, CLF_NAME_CAP, value, CLF_VALUE_CAP, &self)) == 1) { /* ignored */ }
            if (r < 0) return ALWAN_E_INVALID;
            if (self) continue;
            st = clf_parse_body(s, op, node_name);
            if (st != ALWAN_OK) return st;
            continue;
        }

        if (strcmp(name, "Slope") == 0 || strcmp(name, "Offset") == 0 ||
            strcmp(name, "Power") == 0 || strcmp(name, "Saturation") == 0) {
            int self = 0, r, which;
            double v[3];
            size_t got = 0, want;
            char elem[CLF_NAME_CAP];
            size_t ln = strlen(name);
            memcpy(elem, name, ln + 1);
            which = (elem[0] == 'S' && elem[1] == 'l') ? 0 :
                    (elem[0] == 'O') ? 1 : (elem[0] == 'P') ? 2 : 3;
            want = (which == 3) ? 1u : 3u;
            while ((r = alwan__xml_read_attr(s, name, CLF_NAME_CAP, value, CLF_VALUE_CAP, &self)) == 1) { /* ignored */ }
            if (r < 0) return ALWAN_E_INVALID;
            if (self) return ALWAN_E_INVALID;
            if (!clf_read_numbers(s, v, want, &got) || got != want) return ALWAN_E_INVALID;
            if (which == 0) { op->slope[0] = v[0]; op->slope[1] = v[1]; op->slope[2] = v[2]; }
            else if (which == 1) { op->cdl_offset[0] = v[0]; op->cdl_offset[1] = v[1]; op->cdl_offset[2] = v[2]; }
            else if (which == 2) { op->power[0] = v[0]; op->power[1] = v[1]; op->power[2] = v[2]; }
            else { op->sat = v[0]; }
            if (!alwan__xml_skip_element(s, elem)) return ALWAN_E_INVALID;
            continue;
        }

        if (strcmp(name, "LogParams") == 0) {
            int self = 0, r;
            while ((r = alwan__xml_read_attr(s, name, CLF_NAME_CAP, value, CLF_VALUE_CAP, &self)) == 1) {
                double d = strtod(value, NULL);
                if (!clf_finite(d)) return ALWAN_E_INVALID;
                if (strcmp(name, "base") == 0) op->log_base = d;
                else if (strcmp(name, "logSideSlope") == 0) op->ls_slope = d;
                else if (strcmp(name, "logSideOffset") == 0) op->ls_offset = d;
                else if (strcmp(name, "linSideSlope") == 0) op->lin_slope = d;
                else if (strcmp(name, "linSideOffset") == 0) op->lin_offset = d;
                else if (strcmp(name, "linSideBreak") == 0) op->lin_break = d;
                else if (strcmp(name, "linearSlope") == 0) { op->linear_slope = d; op->has_linear_slope = 1; }
            }
            if (r < 0) return ALWAN_E_INVALID;
            if (!(op->log_base > 1.0) || op->ls_slope == 0.0 || op->lin_slope == 0.0) {
                return ALWAN_E_INVALID;
            }
            if (!self && !alwan__xml_skip_element(s, "LogParams")) return ALWAN_E_INVALID;
            continue;
        }

        {
            /* The Range bounds are elements with text content. */
            static char const *const bounds[4] = { "minInValue", "maxInValue",
                                                   "minOutValue", "maxOutValue" };
            int bi;
            for (bi = 0; bi < 4; bi++) {
                if (strcmp(name, bounds[bi]) == 0) break;
            }
            if (bi < 4) {
                int self = 0, r;
                double v[1];
                size_t got = 0;
                while ((r = alwan__xml_read_attr(s, name, CLF_NAME_CAP, value, CLF_VALUE_CAP, &self)) == 1) { /* no attributes used */ }
                if (r < 0) return ALWAN_E_INVALID;
                if (self) return ALWAN_E_INVALID;
                if (!clf_read_numbers(s, v, 1, &got) || got != 1) return ALWAN_E_INVALID;
                switch (bi) {
                    case 0: op->min_in = v[0]; break;
                    case 1: op->max_in = v[0]; break;
                    case 2: op->min_out = v[0]; break;
                    default: op->max_out = v[0]; break;
                }
                if (!alwan__xml_skip_element(s, bounds[bi])) return ALWAN_E_INVALID;
                continue;
            }
        }

        {
            /* Description and anything else inside a node is skipped whole. */
            int self = 0, r;
            char elem[CLF_NAME_CAP];
            size_t n = strlen(name);
            if (n + 1 > sizeof elem) return ALWAN_E_INVALID;
            memcpy(elem, name, n + 1);
            while ((r = alwan__xml_read_attr(s, name, CLF_NAME_CAP, value, CLF_VALUE_CAP, &self)) == 1) { /* ignored */ }
            if (r < 0) return ALWAN_E_INVALID;
            if (!self && !alwan__xml_skip_element(s, elem)) return ALWAN_E_INVALID;
        }
    }
}

/* ---------------------------------------------------------------- evaluate */

static double clf_exponent_channel(double x, int style, double g, double a) {
    switch (style) {
        case CLF_BASIC_FWD:
            return x > 0.0 ? pow(x, g) : 0.0;
        case CLF_BASIC_REV:
            return x > 0.0 ? pow(x, 1.0 / g) : 0.0;
        case CLF_MON_FWD: {
            double const xb = a / (g - 1.0);
            double const yb = pow((xb + a) / (1.0 + a), g);
            if (x <= xb) return x * (yb / xb);
            return pow((x + a) / (1.0 + a), g);
        }
        default: {
            double const xb = a / (g - 1.0);
            double const yb = pow((xb + a) / (1.0 + a), g);
            if (x <= yb) return x * (xb / yb);
            return (1.0 + a) * pow(x, 1.0 / g) - a;
        }
    }
}

static double clf_lut1d_sample(clf_op const *op, double x, int channel) {
    int const n = op->size;
    int const stride = op->channels;
    int const c = (stride == 3) ? channel : 0;
    double t, f;
    int i0;
    if (!(x > 0.0)) x = 0.0;                    /* NaN lands here too */
    if (x > 1.0) x = 1.0;
    t = x * (double)(n - 1);
    i0 = (int)t;
    if (i0 > n - 2) i0 = n - 2;
    f = t - (double)i0;
    return op->data[(size_t)i0 * stride + c] * (1.0 - f) +
           op->data[(size_t)(i0 + 1) * stride + c] * f;
}

static void clf_lut3d_sample(clf_op const *op, double const *in, double *out) {
    int const n = op->size;
    double t[3], f[3];
    int i0[3], c, dr, dg, db;
    for (c = 0; c < 3; c++) {
        double v = in[c];
        if (!(v > 0.0)) v = 0.0;
        if (v > 1.0) v = 1.0;
        t[c] = v * (double)(n - 1);
        i0[c] = (int)t[c];
        if (i0[c] > n - 2) i0[c] = n - 2;
        f[c] = t[c] - (double)i0[c];
    }
    for (c = 0; c < 3; c++) out[c] = 0.0;
    for (dr = 0; dr < 2; dr++) {
        for (dg = 0; dg < 2; dg++) {
            for (db = 0; db < 2; db++) {
                double w = (dr ? f[0] : 1.0 - f[0]) * (dg ? f[1] : 1.0 - f[1]) *
                           (db ? f[2] : 1.0 - f[2]);
                size_t at = ((size_t)(i0[2] + db) * n + (size_t)(i0[1] + dg)) * n
                            + (size_t)(i0[0] + dr);
                for (c = 0; c < 3; c++) out[c] += w * op->data[at * 3 + (size_t)c];
            }
        }
    }
}

/* ASC CDL. The SOP is out = (in * slope + offset)^power per channel, then the
 * saturation is applied about the Rec. 709 luma. Measured against OCIO: the
 * clamping styles hold the SOP result in [0, 1] BEFORE the power and the final
 * result in [0, 1] after the saturation; the no-clamp styles do neither and
 * pass a negative through the power unchanged rather than raising it. */
static double clf_cdl_luma(double const *v) {
    return 0.2126 * v[0] + 0.7152 * v[1] + 0.0722 * v[2];
}

static double clf_unit(double x) {
    if (!(x > 0.0)) return 0.0;                 /* NaN lands here too */
    return x > 1.0 ? 1.0 : x;
}

static void clf_apply_cdl(clf_op const *op, double *v) {
    int const clamp = (op->style == CLF_CDL_FWD || op->style == CLF_CDL_REV);
    int const reverse = (op->style == CLF_CDL_REV || op->style == CLF_CDL_REV_NC);
    double luma;
    int c;

    if (!reverse) {
        for (c = 0; c < 3; c++) {
            double x = v[c] * op->slope[c] + op->cdl_offset[c];
            if (clamp) x = clf_unit(x);
            v[c] = (x > 0.0) ? pow(x, op->power[c]) : x;
            if (clamp) v[c] = clf_unit(v[c]);
        }
        luma = clf_cdl_luma(v);
        for (c = 0; c < 3; c++) v[c] = luma + op->sat * (v[c] - luma);
        if (clamp) for (c = 0; c < 3; c++) v[c] = clf_unit(v[c]);
        return;
    }

    /* The same stages undone, in the other order. */
    if (clamp) for (c = 0; c < 3; c++) v[c] = clf_unit(v[c]);
    luma = clf_cdl_luma(v);
    if (op->sat != 0.0) {
        for (c = 0; c < 3; c++) v[c] = luma + (v[c] - luma) / op->sat;
    }
    for (c = 0; c < 3; c++) {
        double x = v[c];
        if (clamp) x = clf_unit(x);
        if (x > 0.0 && op->power[c] != 0.0) x = pow(x, 1.0 / op->power[c]);
        x = (op->slope[c] != 0.0) ? (x - op->cdl_offset[c]) / op->slope[c] : 0.0;
        v[c] = clamp ? clf_unit(x) : x;
    }
}

/* Log. The plain styles are the logarithm and its inverse; linToLog and its
 * inverse carry the five parameters; the camera pair adds a linear segment
 * below linSideBreak whose slope is the curve's own slope there unless the file
 * states one. All measured against OCIO. */
static double clf_log_channel(clf_op const *op, double x) {
    double const lb = log(op->log_base);
    switch (op->style) {
        case CLF_LOG_LOG2:
            return log(x < CLF_LOG_FLOOR ? CLF_LOG_FLOOR : x) / log(2.0);
        case CLF_LOG_LOG10:
            return log10(x < CLF_LOG_FLOOR ? CLF_LOG_FLOOR : x);
        case CLF_LOG_ANTILOG2:
            return pow(2.0, x);
        case CLF_LOG_ANTILOG10:
            return pow(10.0, x);
        case CLF_LOG_LIN_TO_LOG: {
            double const arg = op->lin_slope * x + op->lin_offset;
            return op->ls_slope * log(arg < CLF_LOG_FLOOR ? CLF_LOG_FLOOR : arg) / lb + op->ls_offset;
        }
        case CLF_LOG_LOG_TO_LIN: {
            double const e = (x - op->ls_offset) / op->ls_slope;
            return (pow(op->log_base, e) - op->lin_offset) / op->lin_slope;
        }
        case CLF_LOG_CAM_LIN_TO_LOG: {
            double const xb = op->lin_break;
            double const arg = op->lin_slope * xb + op->lin_offset;
            double const yb = op->ls_slope * log(arg < CLF_LOG_FLOOR ? CLF_LOG_FLOOR : arg) / lb
                              + op->ls_offset;
            double const m = op->has_linear_slope ? op->linear_slope
                                                  : (op->ls_slope * op->lin_slope) / (arg * lb);
            if (x <= xb) return yb + (x - xb) * m;
            {
                double const a2 = op->lin_slope * x + op->lin_offset;
                return op->ls_slope * log(a2 < CLF_LOG_FLOOR ? CLF_LOG_FLOOR : a2) / lb + op->ls_offset;
            }
        }
        default: {
            double const xb = op->lin_break;
            double const arg = op->lin_slope * xb + op->lin_offset;
            double const yb = op->ls_slope * log(arg < CLF_LOG_FLOOR ? CLF_LOG_FLOOR : arg) / lb
                              + op->ls_offset;
            double const m = op->has_linear_slope ? op->linear_slope
                                                  : (op->ls_slope * op->lin_slope) / (arg * lb);
            if (x <= yb) return (m != 0.0) ? xb + (x - yb) / m : xb;
            {
                double const e = (x - op->ls_offset) / op->ls_slope;
                return (pow(op->log_base, e) - op->lin_offset) / op->lin_slope;
            }
        }
    }
}

static void clf_apply_op(clf_op const *op, double *v) {
    int c;
    switch (op->type) {
        case ALWAN_CLF_NODE_MATRIX: {
            double o[3];
            for (c = 0; c < 3; c++) {
                o[c] = op->m[c * 4 + 0] * v[0] + op->m[c * 4 + 1] * v[1] +
                       op->m[c * 4 + 2] * v[2] + op->m[c * 4 + 3];
            }
            v[0] = o[0]; v[1] = o[1]; v[2] = o[2];
            break;
        }
        case ALWAN_CLF_NODE_RANGE: {
            double const span = op->max_in - op->min_in;
            double const scale = (span != 0.0) ? (op->max_out - op->min_out) / span : 0.0;
            for (c = 0; c < 3; c++) {
                double o = (v[c] - op->min_in) * scale + op->min_out;
                if (op->clamp) {
                    if (o < op->min_out) o = op->min_out;
                    if (o > op->max_out) o = op->max_out;
                }
                v[c] = o;
            }
            break;
        }
        case ALWAN_CLF_NODE_EXPONENT:
            for (c = 0; c < 3; c++) v[c] = clf_exponent_channel(v[c], op->style, op->g[c], op->a[c]);
            break;
        case ALWAN_CLF_NODE_LUT1D:
            for (c = 0; c < 3; c++) v[c] = clf_lut1d_sample(op, v[c], c);
            break;
        case ALWAN_CLF_NODE_ASC_CDL:
            clf_apply_cdl(op, v);
            break;
        case ALWAN_CLF_NODE_LOG:
            for (c = 0; c < 3; c++) v[c] = clf_log_channel(op, v[c]);
            break;
        default: {
            double o[3];
            clf_lut3d_sample(op, v, o);
            v[0] = o[0]; v[1] = o[1]; v[2] = o[2];
            break;
        }
    }
}

/* ---------------------------------------------------------------- parse */

static void clf_free(alwan_clf *clf) {
    size_t i;
    if (!clf) return;
    if (clf->ops) {
        for (i = 0; i < clf->count; i++) {
            if (clf->ops[i].data) ALWAN_FREE(clf->ops[i].data);
        }
        ALWAN_FREE(clf->ops);
    }
    ALWAN_FREE(clf);
}

static alwan_status clf_parse(alwan_clf **out, char const *buf, size_t len) {
    alwan__xml_scan s;
    alwan_clf *clf;
    char name[CLF_NAME_CAP], value[CLF_VALUE_CAP];
    char *saved;
    alwan_status status = ALWAN_OK;
    int self = 0, r;

    clf = (alwan_clf *)ALWAN_ALLOC(sizeof *clf, sizeof(double));
    if (!clf) return ALWAN_E_NOMEM;
    memset(clf, 0, sizeof *clf);
    clf->ops = (clf_op *)ALWAN_ALLOC(sizeof(clf_op) * CLF_MAX_NODES, sizeof(double));
    if (!clf->ops) { ALWAN_FREE(clf); return ALWAN_E_NOMEM; }
    memset(clf->ops, 0, sizeof(clf_op) * CLF_MAX_NODES);

    s.p = buf;
    s.end = buf + len;
    saved = clf_save_lc_numeric();

    /* The prologue: declarations and comments, then <ProcessList ...>. */
    if (!alwan__xml_skip_prologue(&s)) { status = ALWAN_E_NODATA; goto done; }
    s.p++;
    if (!alwan__xml_read_name(&s, name, sizeof name) || strcmp(name, "ProcessList") != 0) {
        status = ALWAN_E_NODATA;                 /* not a CLF */
        goto done;
    }
    while ((r = alwan__xml_read_attr(&s, name, CLF_NAME_CAP, value, CLF_VALUE_CAP, &self)) == 1) {
        if (strcmp(name, "id") == 0) {
            size_t n = strlen(value);
            if (n >= CLF_ID_CAP) n = CLF_ID_CAP - 1;
            memcpy(clf->id, value, n);
            clf->id[n] = '\0';
        }
    }
    if (r < 0) { status = ALWAN_E_INVALID; goto done; }

    for (;;) {
        alwan__xml_skip_space(&s);
        if (s.p >= s.end) { status = ALWAN_E_INVALID; goto done; }   /* no close tag */
        if (alwan__xml_starts_with(&s, "<!--")) {
            if (!alwan__xml_skip_past(&s, "-->")) { status = ALWAN_E_INVALID; goto done; }
            continue;
        }
        if (alwan__xml_starts_with(&s, "</ProcessList")) break;
        if (*s.p != '<') { s.p++; continue; }
        s.p++;
        if (!alwan__xml_read_name(&s, name, sizeof name)) { status = ALWAN_E_INVALID; goto done; }

        {
            clf_op *op;
            int type = -1, style = -1;
            if (strcmp(name, "Matrix") == 0) type = ALWAN_CLF_NODE_MATRIX;
            else if (strcmp(name, "Range") == 0) type = ALWAN_CLF_NODE_RANGE;
            else if (strcmp(name, "Exponent") == 0) type = ALWAN_CLF_NODE_EXPONENT;
            else if (strcmp(name, "LUT1D") == 0) type = ALWAN_CLF_NODE_LUT1D;
            else if (strcmp(name, "LUT3D") == 0) type = ALWAN_CLF_NODE_LUT3D;
            else if (strcmp(name, "ASC_CDL") == 0) type = ALWAN_CLF_NODE_ASC_CDL;
            else if (strcmp(name, "Log") == 0) type = ALWAN_CLF_NODE_LOG;
            else if (strcmp(name, "Description") == 0 || strcmp(name, "InputDescriptor") == 0 ||
                     strcmp(name, "OutputDescriptor") == 0 || strcmp(name, "Info") == 0) {
                char elem[CLF_NAME_CAP];
                size_t n = strlen(name);
                memcpy(elem, name, n + 1);
                while ((r = alwan__xml_read_attr(&s, name, CLF_NAME_CAP, value, CLF_VALUE_CAP, &self)) == 1) { /* ignored */ }
                if (r < 0) { status = ALWAN_E_INVALID; goto done; }
                if (!self && !alwan__xml_skip_element(&s, elem)) { status = ALWAN_E_INVALID; goto done; }
                continue;
            } else {
                /* A ProcessNode this reader does not implement. Applying the rest
                 * would be a different transform, so the file is refused. */
                status = ALWAN_E_NODATA;
                goto done;
            }

            if (clf->count >= CLF_MAX_NODES) { status = ALWAN_E_RANGE; goto done; }
            op = &clf->ops[clf->count];
            clf_op_init(op);
            op->type = type;

            while ((r = alwan__xml_read_attr(&s, name, CLF_NAME_CAP, value, CLF_VALUE_CAP, &self)) == 1) {
                if (strcmp(name, "style") == 0) {
                    if (type == ALWAN_CLF_NODE_RANGE) {
                        if (strcmp(value, "noClamp") == 0) op->clamp = 0;
                        else if (strcmp(value, "Clamp") != 0 && strcmp(value, "clamp") != 0) {
                            status = ALWAN_E_NODATA; goto done;
                        }
                    } else if (type == ALWAN_CLF_NODE_ASC_CDL) {
                        if (strcmp(value, "Fwd") == 0) style = CLF_CDL_FWD;
                        else if (strcmp(value, "Rev") == 0) style = CLF_CDL_REV;
                        else if (strcmp(value, "FwdNoClamp") == 0) style = CLF_CDL_FWD_NC;
                        else if (strcmp(value, "RevNoClamp") == 0) style = CLF_CDL_REV_NC;
                        else { status = ALWAN_E_NODATA; goto done; }
                    } else if (type == ALWAN_CLF_NODE_LOG) {
                        if (strcmp(value, "log2") == 0) style = CLF_LOG_LOG2;
                        else if (strcmp(value, "log10") == 0) style = CLF_LOG_LOG10;
                        else if (strcmp(value, "antiLog2") == 0) style = CLF_LOG_ANTILOG2;
                        else if (strcmp(value, "antiLog10") == 0) style = CLF_LOG_ANTILOG10;
                        else if (strcmp(value, "linToLog") == 0) style = CLF_LOG_LIN_TO_LOG;
                        else if (strcmp(value, "logToLin") == 0) style = CLF_LOG_LOG_TO_LIN;
                        else if (strcmp(value, "cameraLinToLog") == 0) style = CLF_LOG_CAM_LIN_TO_LOG;
                        else if (strcmp(value, "cameraLogToLin") == 0) style = CLF_LOG_CAM_LOG_TO_LIN;
                        else { status = ALWAN_E_NODATA; goto done; }
                    } else if (type == ALWAN_CLF_NODE_EXPONENT) {
                        if (strcmp(value, "basicFwd") == 0) style = CLF_BASIC_FWD;
                        else if (strcmp(value, "basicRev") == 0) style = CLF_BASIC_REV;
                        else if (strcmp(value, "monCurveFwd") == 0) style = CLF_MON_FWD;
                        else if (strcmp(value, "monCurveRev") == 0) style = CLF_MON_REV;
                        else { status = ALWAN_E_NODATA; goto done; }
                    }
                }
            }
            if (r < 0) { status = ALWAN_E_INVALID; goto done; }
            if (type == ALWAN_CLF_NODE_EXPONENT || type == ALWAN_CLF_NODE_ASC_CDL ||
                type == ALWAN_CLF_NODE_LOG) {
                if (style < 0) { status = ALWAN_E_INVALID; goto done; }   /* style is required */
                op->style = style;
            }
            if (!self) {
                status = clf_parse_body(&s, op, name);
                if (status != ALWAN_OK) goto done;
            }
            if ((type == ALWAN_CLF_NODE_LUT1D || type == ALWAN_CLF_NODE_LUT3D) && !op->data) {
                status = ALWAN_E_INVALID;                 /* a LUT node with no Array */
                goto done;
            }
            if (type == ALWAN_CLF_NODE_RANGE && !(op->max_in > op->min_in)) {
                status = ALWAN_E_INVALID;
                goto done;
            }
            clf->count++;
        }
    }

    if (clf->count == 0) { status = ALWAN_E_NODATA; goto done; }

done:
    clf_restore_lc_numeric(saved);
    if (status != ALWAN_OK) {
        clf_free(clf);
        return status;
    }
    *out = clf;
    return ALWAN_OK;
}

/* ---------------------------------------------------------------- public */

alwan_status alwan_clf_import_buffer(alwan_clf **out, char const *buf, size_t len, alwan_ctx *ctx) {
    (void)ctx;
    if (!out || !buf || len == 0) return ALWAN_E_INVALID;
    *out = NULL;
    return clf_parse(out, buf, len);
}

alwan_status alwan_clf_import(alwan_clf **out, char const *path, alwan_ctx *ctx) {
    FILE *f;
    long size;
    char *buf;
    alwan_status st;
    size_t got;

    (void)ctx;
    if (!out || !path) return ALWAN_E_INVALID;
    *out = NULL;
    f = fopen(path, "rb");
    if (!f) return ALWAN_E_INVALID;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return ALWAN_E_INVALID; }
    size = ftell(f);
    if (size <= 0 || size > (long)(64 * 1024 * 1024)) { fclose(f); return ALWAN_E_RANGE; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return ALWAN_E_INVALID; }
    buf = (char *)ALWAN_ALLOC((size_t)size + 1, 1);
    if (!buf) { fclose(f); return ALWAN_E_NOMEM; }
    got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) { ALWAN_FREE(buf); return ALWAN_E_INVALID; }
    buf[size] = '\0';
    st = clf_parse(out, buf, (size_t)size);
    ALWAN_FREE(buf);
    return st;
}

void alwan_clf_destroy(alwan_clf *clf, alwan_ctx *ctx) {
    (void)ctx;
    clf_free(clf);
}

size_t alwan_clf_node_count(alwan_clf const *clf) {
    return clf ? clf->count : 0;
}

alwan_status alwan_clf_node_type_at(alwan_clf_node_type *out, alwan_clf const *clf, size_t index) {
    if (!out || !clf) return ALWAN_E_INVALID;
    if (index >= clf->count) return ALWAN_E_RANGE;
    *out = (alwan_clf_node_type)clf->ops[index].type;
    return ALWAN_OK;
}

char const *alwan_clf_id(alwan_clf const *clf) {
    return clf ? clf->id : NULL;
}

alwan_status alwan_clf_apply_f64_map_interleave(alwan_f64 *out, size_t out_stride,
                                                alwan_f64 const *in, size_t in_stride,
                                                size_t count, alwan_clf const *clf) {
    size_t i, k;
    if (!out || !in || !clf || count == 0) return ALWAN_E_INVALID;
    if (out_stride == 0) out_stride = 3 * sizeof(alwan_f64);
    if (in_stride == 0) in_stride = 3 * sizeof(alwan_f64);
    for (i = 0; i < count; i++) {
        alwan_f64 const *src = (alwan_f64 const *)((char const *)in + i * in_stride);
        alwan_f64 *dst = (alwan_f64 *)((char *)out + i * out_stride);
        double v[3];
        v[0] = src[0]; v[1] = src[1]; v[2] = src[2];
        for (k = 0; k < clf->count; k++) clf_apply_op(&clf->ops[k], v);
        dst[0] = v[0]; dst[1] = v[1]; dst[2] = v[2];
    }
    return ALWAN_OK;
}

alwan_status alwan_clf_apply_f32_map_interleave(alwan_f32 *out, size_t out_stride,
                                                alwan_f32 const *in, size_t in_stride,
                                                size_t count, alwan_clf const *clf) {
    size_t i, k;
    if (!out || !in || !clf || count == 0) return ALWAN_E_INVALID;
    if (out_stride == 0) out_stride = 3 * sizeof(alwan_f32);
    if (in_stride == 0) in_stride = 3 * sizeof(alwan_f32);
    for (i = 0; i < count; i++) {
        alwan_f32 const *src = (alwan_f32 const *)((char const *)in + i * in_stride);
        alwan_f32 *dst = (alwan_f32 *)((char *)out + i * out_stride);
        double v[3];
        v[0] = (double)src[0]; v[1] = (double)src[1]; v[2] = (double)src[2];
        for (k = 0; k < clf->count; k++) clf_apply_op(&clf->ops[k], v);
        dst[0] = (alwan_f32)v[0]; dst[1] = (alwan_f32)v[1]; dst[2] = (alwan_f32)v[2];
    }
    return ALWAN_OK;
}
