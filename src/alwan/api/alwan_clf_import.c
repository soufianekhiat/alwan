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
 * Five ProcessNode types are understood: Matrix, Range, Exponent, LUT1D and
 * LUT3D. A file carrying any other node is REFUSED rather than partly applied,
 * because a pipeline missing one of its stages is not the transform and is
 * worse than no answer at all.
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

/* ---------------------------------------------------------------- scanning */

typedef struct {
    char const *p;
    char const *end;
} clf_scan;

static void clf_skip_space(clf_scan *s) {
    while (s->p < s->end && (*s->p == ' ' || *s->p == '\t' || *s->p == '\r' || *s->p == '\n')) {
        s->p++;
    }
}

static int clf_starts_with(clf_scan const *s, char const *lit) {
    size_t n = strlen(lit);
    return (size_t)(s->end - s->p) >= n && strncmp(s->p, lit, n) == 0;
}

/* Skip to just past the first occurrence of lit. Returns 0 at end of input. */
static int clf_skip_past(clf_scan *s, char const *lit) {
    size_t n = strlen(lit);
    while (s->p + n <= s->end) {
        if (strncmp(s->p, lit, n) == 0) { s->p += n; return 1; }
        s->p++;
    }
    s->p = s->end;
    return 0;
}

/* An element or attribute name: letters, digits, underscore, colon, dot, dash. */
static int clf_name_char(char c) {
    return isalnum((unsigned char)c) || c == '_' || c == ':' || c == '.' || c == '-';
}

static int clf_read_name(clf_scan *s, char *buf, size_t cap) {
    size_t n = 0;
    while (s->p < s->end && clf_name_char(*s->p)) {
        if (n + 1 < cap) buf[n] = *s->p;
        n++;
        s->p++;
    }
    buf[n < cap ? n : cap - 1] = '\0';
    return n > 0 && n < cap;
}

/* One attribute of the element being read. Returns 1 on an attribute, 0 when the
 * element's tag ends, -1 on malformed input. *self_closing is set at the end. */
static int clf_read_attr(clf_scan *s, char *name, char *value, int *self_closing) {
    char quote;
    size_t n = 0;
    clf_skip_space(s);
    if (s->p >= s->end) return -1;
    if (*s->p == '/') {
        s->p++;
        if (s->p >= s->end || *s->p != '>') return -1;
        s->p++;
        *self_closing = 1;
        return 0;
    }
    if (*s->p == '>') { s->p++; *self_closing = 0; return 0; }
    if (!clf_read_name(s, name, CLF_NAME_CAP)) return -1;
    clf_skip_space(s);
    if (s->p >= s->end || *s->p != '=') return -1;
    s->p++;
    clf_skip_space(s);
    if (s->p >= s->end || (*s->p != '"' && *s->p != '\'')) return -1;
    quote = *s->p++;
    while (s->p < s->end && *s->p != quote) {
        if (n + 1 < CLF_VALUE_CAP) value[n] = *s->p;
        n++;
        s->p++;
    }
    if (s->p >= s->end) return -1;
    s->p++;
    if (n >= CLF_VALUE_CAP) return -1;          /* an attribute longer than we hold */
    value[n] = '\0';
    return 1;
}

/* Exactly one number, wherever the scanner stands. */
static int clf_read_one(clf_scan *s, double *out) {
    char *endp = NULL;
    double v;
    clf_skip_space(s);
    if (s->p >= s->end || *s->p == '<') return 0;
    v = strtod(s->p, &endp);
    if (endp == s->p) return 0;
    s->p = endp;
    if (!clf_finite(v)) return 0;
    *out = v;
    return 1;
}

/* Read numbers until the closing tag of the element holding them. */
static int clf_read_numbers(clf_scan *s, double *out, size_t want, size_t *got) {
    size_t n = 0;
    for (;;) {
        char *endp = NULL;
        double v;
        clf_skip_space(s);
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
}

static int clf_channel_index(char const *v) {
    if (strcmp(v, "R") == 0) return 0;
    if (strcmp(v, "G") == 0) return 1;
    if (strcmp(v, "B") == 0) return 2;
    return -1;
}

/* Skip an element's content, including any nested elements, to its close tag. */
static int clf_skip_element(clf_scan *s, char const *name) {
    char close[CLF_NAME_CAP + 4];
    size_t n = strlen(name);
    if (n + 4 >= sizeof close) return 0;
    close[0] = '<'; close[1] = '/';
    memcpy(close + 2, name, n);
    close[n + 2] = '>';
    close[n + 3] = '\0';
    return clf_skip_past(s, close);
}

/* Parse the body of one ProcessNode. The scanner sits just past its open tag. */
static alwan_status clf_parse_body(clf_scan *s, clf_op *op, char const *node_name) {
    char name[CLF_NAME_CAP], value[CLF_VALUE_CAP];
    (void)node_name;
    for (;;) {
        clf_skip_space(s);
        if (s->p >= s->end) return ALWAN_E_INVALID;
        if (clf_starts_with(s, "<!--")) {
            if (!clf_skip_past(s, "-->")) return ALWAN_E_INVALID;
            continue;
        }
        if (s->p + 1 < s->end && s->p[0] == '<' && s->p[1] == '/') {
            /* The node's own close tag ends it. */
            if (!clf_skip_past(s, ">")) return ALWAN_E_INVALID;
            return ALWAN_OK;
        }
        if (*s->p != '<') { s->p++; continue; }          /* stray text */
        s->p++;
        if (!clf_read_name(s, name, sizeof name)) return ALWAN_E_INVALID;

        if (strcmp(name, "Array") == 0) {
            int dims[4];
            int ndim = 0, self = 0, r;
            size_t want, got = 0;
            while ((r = clf_read_attr(s, name, value, &self)) == 1) {
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
            if (!self && !clf_skip_element(s, "Array")) return ALWAN_E_INVALID;
            continue;
        }

        if (strcmp(name, "ExponentParams") == 0) {
            int self = 0, r, ch = -1;
            double g = 1.0, a = 0.0;
            int have_a = 0;
            while ((r = clf_read_attr(s, name, value, &self)) == 1) {
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
            if (!self && !clf_skip_element(s, "ExponentParams")) return ALWAN_E_INVALID;
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
                while ((r = clf_read_attr(s, name, value, &self)) == 1) { /* no attributes used */ }
                if (r < 0) return ALWAN_E_INVALID;
                if (self) return ALWAN_E_INVALID;
                if (!clf_read_numbers(s, v, 1, &got) || got != 1) return ALWAN_E_INVALID;
                switch (bi) {
                    case 0: op->min_in = v[0]; break;
                    case 1: op->max_in = v[0]; break;
                    case 2: op->min_out = v[0]; break;
                    default: op->max_out = v[0]; break;
                }
                if (!clf_skip_element(s, bounds[bi])) return ALWAN_E_INVALID;
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
            while ((r = clf_read_attr(s, name, value, &self)) == 1) { /* ignored */ }
            if (r < 0) return ALWAN_E_INVALID;
            if (!self && !clf_skip_element(s, elem)) return ALWAN_E_INVALID;
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
    clf_scan s;
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
    for (;;) {
        clf_skip_space(&s);
        if (s.p >= s.end) { status = ALWAN_E_NODATA; goto done; }
        if (clf_starts_with(&s, "<?")) { if (!clf_skip_past(&s, "?>")) { status = ALWAN_E_INVALID; goto done; } continue; }
        if (clf_starts_with(&s, "<!--")) { if (!clf_skip_past(&s, "-->")) { status = ALWAN_E_INVALID; goto done; } continue; }
        if (clf_starts_with(&s, "<!")) { if (!clf_skip_past(&s, ">")) { status = ALWAN_E_INVALID; goto done; } continue; }
        break;
    }
    if (s.p >= s.end || *s.p != '<') { status = ALWAN_E_NODATA; goto done; }
    s.p++;
    if (!clf_read_name(&s, name, sizeof name) || strcmp(name, "ProcessList") != 0) {
        status = ALWAN_E_NODATA;                 /* not a CLF */
        goto done;
    }
    while ((r = clf_read_attr(&s, name, value, &self)) == 1) {
        if (strcmp(name, "id") == 0) {
            size_t n = strlen(value);
            if (n >= CLF_ID_CAP) n = CLF_ID_CAP - 1;
            memcpy(clf->id, value, n);
            clf->id[n] = '\0';
        }
    }
    if (r < 0) { status = ALWAN_E_INVALID; goto done; }

    for (;;) {
        clf_skip_space(&s);
        if (s.p >= s.end) { status = ALWAN_E_INVALID; goto done; }   /* no close tag */
        if (clf_starts_with(&s, "<!--")) {
            if (!clf_skip_past(&s, "-->")) { status = ALWAN_E_INVALID; goto done; }
            continue;
        }
        if (clf_starts_with(&s, "</ProcessList")) break;
        if (*s.p != '<') { s.p++; continue; }
        s.p++;
        if (!clf_read_name(&s, name, sizeof name)) { status = ALWAN_E_INVALID; goto done; }

        {
            clf_op *op;
            int type = -1, style = -1;
            if (strcmp(name, "Matrix") == 0) type = ALWAN_CLF_NODE_MATRIX;
            else if (strcmp(name, "Range") == 0) type = ALWAN_CLF_NODE_RANGE;
            else if (strcmp(name, "Exponent") == 0) type = ALWAN_CLF_NODE_EXPONENT;
            else if (strcmp(name, "LUT1D") == 0) type = ALWAN_CLF_NODE_LUT1D;
            else if (strcmp(name, "LUT3D") == 0) type = ALWAN_CLF_NODE_LUT3D;
            else if (strcmp(name, "Description") == 0 || strcmp(name, "InputDescriptor") == 0 ||
                     strcmp(name, "OutputDescriptor") == 0 || strcmp(name, "Info") == 0) {
                char elem[CLF_NAME_CAP];
                size_t n = strlen(name);
                memcpy(elem, name, n + 1);
                while ((r = clf_read_attr(&s, name, value, &self)) == 1) { /* ignored */ }
                if (r < 0) { status = ALWAN_E_INVALID; goto done; }
                if (!self && !clf_skip_element(&s, elem)) { status = ALWAN_E_INVALID; goto done; }
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

            while ((r = clf_read_attr(&s, name, value, &self)) == 1) {
                if (strcmp(name, "style") == 0) {
                    if (type == ALWAN_CLF_NODE_RANGE) {
                        if (strcmp(value, "noClamp") == 0) op->clamp = 0;
                        else if (strcmp(value, "Clamp") != 0 && strcmp(value, "clamp") != 0) {
                            status = ALWAN_E_NODATA; goto done;
                        }
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
            if (type == ALWAN_CLF_NODE_EXPONENT) {
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
