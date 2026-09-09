/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * CGATS.17 / OpenQualia lexing, shared by both precision passes.
 *
 * Nothing here depends on ALWAN_CORE_T. It is a separate header with a
 * guard rather than part of alwan_chart_impl.inc because that .inc is
 * included once per precision into one translation unit, and a file-static
 * helper defined twice there is a redefinition. Numbers are parsed and
 * formatted in double whatever the pass: an f32 chart then narrows once, at
 * the store, instead of accumulating in float.
 */
#ifndef ALWAN_CHART_COMMON_H
#define ALWAN_CHART_COMMON_H

#include "../alwan.h"
#include <string.h>

#define ALWAN_CHART_MAX_FIELDS 4096

/* Bounds the decimal exponent a cell may carry. Past this a double is zero or
 * infinity anyway, so clamping loses nothing and keeps both the accumulator
 * and the scaling loop bounded on hostile input. */
#define ALWAN_CHART_EXP_CAP 400

typedef struct {
    char const *p;
    size_t      len;
} alwan__chart_tok;

static char *alwan__chart_strdup_n(char const *s, size_t n) {
    char *out = (char *)ALWAN_ALLOC(n + 1, 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static int alwan__chart_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

static char alwan__chart_upper(char c) {
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

/* Case-insensitive compare of a counted span against a NUL-terminated key. */
static int alwan__chart_eq_n(char const *span, size_t n, char const *key) {
    size_t i = 0;
    for (; i < n; i++) {
        if (!key[i]) return 0;
        if (alwan__chart_upper(span[i]) != alwan__chart_upper(key[i])) return 0;
    }
    return key[i] == '\0';
}

/* strtod without the locale: CGATS numbers are always '.'-separated, and a
 * caller running in a comma-decimal locale must still read the same file the
 * same way. Rejects trailing junk rather than accepting a prefix, so a
 * malformed cell fails the load instead of silently reading as its first
 * few digits. */
static double alwan__chart_strtod(char const *s, size_t n, int *ok) {
    size_t i = 0;
    int neg = 0, any = 0;
    double mant = 0.0, scale = 0.1;
    int exp_val = 0, exp_neg = 0;
    *ok = 0;
    while (i < n && alwan__chart_is_space(s[i])) i++;
    if (i < n && (s[i] == '+' || s[i] == '-')) { neg = (s[i] == '-'); i++; }
    for (; i < n && s[i] >= '0' && s[i] <= '9'; i++) { mant = mant * 10.0 + (s[i] - '0'); any = 1; }
    if (i < n && s[i] == '.') {
        i++;
        for (; i < n && s[i] >= '0' && s[i] <= '9'; i++, scale *= 0.1) { mant += (s[i] - '0') * scale; any = 1; }
    }
    if (!any) return 0.0;
    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
        size_t save = i;
        int edigits = 0;
        i++;
        if (i < n && (s[i] == '+' || s[i] == '-')) { exp_neg = (s[i] == '-'); i++; }
        for (; i < n && s[i] >= '0' && s[i] <= '9'; i++) {
            /* Stop accumulating rather than overflow. A double runs out of
             * range well before 1e400, so any exponent past the cap already
             * means zero or infinity, and the cap also bounds the scaling
             * loop below: without it "1e2000000000" is a two-billion
             * iteration spin, and "1e99999999999" is signed overflow. */
            if (exp_val <= ALWAN_CHART_EXP_CAP) exp_val = exp_val * 10 + (s[i] - '0');
            edigits = 1;
        }
        if (!edigits) { i = save; exp_val = 0; exp_neg = 0; }
    }
    while (i < n && alwan__chart_is_space(s[i])) i++;
    if (i != n) return 0.0;                          /* trailing junk: not a number */
    if (exp_val > ALWAN_CHART_EXP_CAP) exp_val = ALWAN_CHART_EXP_CAP;
    while (exp_val-- > 0) mant = exp_neg ? mant * 0.1 : mant * 10.0;
    /* A long enough digit string reaches infinity on its own, with no
     * exponent involved. A measurement file has no business carrying either,
     * and letting one through would put it in the chart. */
    if (!(mant <= 1.0e308 && mant >= -1.0e308)) return 0.0;
    *ok = 1;
    return neg ? -mant : mant;
}

/* Whole non-negative integer, for NUMBER_OF_SETS and friends. */
static int alwan__chart_atoz(char const *s, size_t *out) {
    size_t v = 0, i = 0;
    int any = 0;
    while (s[i] && alwan__chart_is_space(s[i])) i++;
    for (; s[i] >= '0' && s[i] <= '9'; i++) {
        /* No row count near this is real, and wrapping would make a damaged
         * NUMBER_OF_SETS agree with the row count by accident. */
        if (v > (size_t)1 << 40) return 0;
        v = v * 10 + (size_t)(s[i] - '0');
        any = 1;
    }
    while (s[i] && alwan__chart_is_space(s[i])) i++;
    if (!any || s[i]) return 0;
    *out = v;
    return 1;
}

/* Splits one line. A CGATS field holding a space is double-quoted, and both
 * the DATA_FORMAT block and the DATA rows follow that rule, so one tokeniser
 * serves both. Returns the field count, or (size_t)-1 if the line needs more
 * slots than `cap`. */
static size_t alwan__chart_split(char const *line, size_t len, alwan__chart_tok *out, size_t cap) {
    size_t i = 0, n = 0;
    while (i < len) {
        while (i < len && alwan__chart_is_space(line[i])) i++;
        if (i >= len) break;
        if (n >= cap) return (size_t)-1;
        if (line[i] == '"') {
            size_t start = ++i;
            while (i < len && line[i] != '"') i++;
            out[n].p = line + start;
            out[n].len = i - start;
            if (i < len) i++;                        /* closing quote */
        } else {
            size_t start = i;
            while (i < len && !alwan__chart_is_space(line[i])) i++;
            out[n].p = line + start;
            out[n].len = i - start;
        }
        n++;
    }
    return n;
}

/* The three spellings the standard allows for a reflectance column, each
 * naming its wavelength in nm: SPEC_560, SPECTRAL_NM560, nm560. The
 * wavelength may carry a decimal point. Order matters: SPECTRAL_NM has to be
 * tested before NM or it matches the shorter prefix and parses "_NM560". */
static int alwan__chart_spectral_nm(char const *f, size_t n, double *nm) {
    static char const *const prefixes[] = { "SPECTRAL_NM", "SPEC_", "NM" };
    size_t k;
    for (k = 0; k < sizeof(prefixes) / sizeof(prefixes[0]); k++) {
        size_t plen = strlen(prefixes[k]);
        int ok;
        double v;
        if (n <= plen || !alwan__chart_eq_n(f, plen, prefixes[k])) continue;
        v = alwan__chart_strtod(f + plen, n - plen, &ok);
        if (!ok || v <= 0.0) continue;
        *nm = v;
        return 1;
    }
    return 0;
}

/* The illuminant spellings a measurement file actually uses. Anything else
 * keeps D50, which is what CGATS assumes when ILLUMINANT is absent; the raw
 * string stays reachable through alwan_chart_header either way. */
static alwan_illuminant alwan__chart_illuminant_from_name(char const *s) {
    static const struct { char const *name; alwan_illuminant ill; } table[] = {
        { "A",   ALWAN_ILLUMINANT_A   }, { "B",   ALWAN_ILLUMINANT_B   },
        { "C",   ALWAN_ILLUMINANT_C   }, { "D40", ALWAN_ILLUMINANT_D40 },
        { "D45", ALWAN_ILLUMINANT_D45 }, { "D50", ALWAN_ILLUMINANT_D50 },
        { "D55", ALWAN_ILLUMINANT_D55 }, { "D60", ALWAN_ILLUMINANT_D60 },
        { "D65", ALWAN_ILLUMINANT_D65 }, { "D75", ALWAN_ILLUMINANT_D75 },
        { "D93", ALWAN_ILLUMINANT_D93 }, { "E",   ALWAN_ILLUMINANT_E   },
        { "F1",  ALWAN_ILLUMINANT_F1  }, { "F2",  ALWAN_ILLUMINANT_F2  },
        { "F7",  ALWAN_ILLUMINANT_F7  }, { "F11", ALWAN_ILLUMINANT_F11 }
    };
    size_t i;
    if (!s) return ALWAN_ILLUMINANT_D50;
    for (i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (alwan__chart_eq_n(s, strlen(s), table[i].name)) return table[i].ill;
    }
    return ALWAN_ILLUMINANT_D50;
}

static alwan_observer_type alwan__chart_observer_from_name(char const *s) {
    if (!s) return ALWAN_OBSERVER_CIE_1931_2DEG;
    if (alwan__chart_eq_n(s, strlen(s), "10") ||
        alwan__chart_eq_n(s, strlen(s), "10DEG") ||
        alwan__chart_eq_n(s, strlen(s), "10 DEGREE")) {
        return ALWAN_OBSERVER_CIE_1964_10DEG;
    }
    return ALWAN_OBSERVER_CIE_1931_2DEG;
}

/* ----------------------------------------------------------------
 * Bounded output sink
 *
 * Returns the length it would have written, so a NULL/0 probe sizes the
 * buffer the way snprintf does.
 * ---------------------------------------------------------------- */

typedef struct {
    char  *buf;
    size_t cap;
    size_t len;
} alwan__chart_sink;

static void alwan__chart_put(alwan__chart_sink *s, char const *text, size_t n) {
    size_t i;
    for (i = 0; i < n; i++, s->len++) {
        if (s->buf && s->len < s->cap) s->buf[s->len] = text[i];
    }
}

static void alwan__chart_puts(alwan__chart_sink *s, char const *text) {
    alwan__chart_put(s, text, strlen(text));
}

static void alwan__chart_put_size(alwan__chart_sink *s, size_t v) {
    char tmp[24];
    size_t d = 0, k;
    do { tmp[d++] = (char)('0' + (v % 10)); v /= 10; } while (v && d < sizeof(tmp));
    for (k = 0; k < d / 2; k++) { char t = tmp[k]; tmp[k] = tmp[d - 1 - k]; tmp[d - 1 - k] = t; }
    alwan__chart_put(s, tmp, d);
}

/* Fixed point with `dp` decimals. A chart carries bounded reflectances and
 * tristimulus, so no exponent form is needed, and staying off snprintf keeps
 * the output locale-independent the way the reader is. */
static void alwan__chart_put_num(alwan__chart_sink *s, double x, int dp) {
    double scale = 1.0;
    int i;
    size_t whole;
    unsigned long frac;
    /* NaN has no representation here, and a magnitude this large would make
     * the (size_t) cast below undefined. The reader rejects both, so neither
     * reaches a chart from a file; this is the guard for a caller who builds
     * one another way. */
    if (!(x >= -1.0e18 && x <= 1.0e18)) { alwan__chart_puts(s, "0"); return; }
    if (x < 0) { alwan__chart_puts(s, "-"); x = -x; }
    for (i = 0; i < dp; i++) scale *= 10.0;
    x += 0.5 / scale;                                    /* round half up */
    whole = (size_t)x;
    frac = (unsigned long)((x - (double)whole) * scale);
    alwan__chart_put_size(s, whole);
    if (dp > 0) {
        char tmp[16];
        int d;
        alwan__chart_puts(s, ".");
        for (d = dp - 1; d >= 0; d--) { tmp[d] = (char)('0' + (frac % 10)); frac /= 10; }
        alwan__chart_put(s, tmp, (size_t)dp);
    }
}

#endif /* ALWAN_CHART_COMMON_H */
