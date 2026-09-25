/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Luminaire photometric files: IES LM-63 (the 1986 form without a version line, 1991, 1995,
 * 2002 and 2019) and EULUMDAT (.ldt), read into one object; the intensity at any C-plane
 * and gamma angle by bilinear interpolation over the distribution completed by its
 * symmetry; and the luminous flux of that distribution.
 *
 * Both formats store a grid of intensities over C-planes (the horizontal angle, around the
 * luminaire's axis) and gamma angles (the vertical angle, from the nadir), and both store
 * only as much of the grid as the luminaire's symmetry needs: a single plane for a
 * rotationally symmetric luminaire, a quadrant, or a half. The stored planes are kept as
 * written, and a second, completed map runs from C0 to C360. Each of its planes is the
 * stored plane its symmetry maps it onto:
 *
 *   quadrant            C -> -C, 180 - C, C + 180     (IES 0 to 90; EULUMDAT Isym 4)
 *   about C0-C180       C -> -C                       (IES 0 to 180; Isym 2)
 *   about C90-C270      C -> 180 - C                  (IES 90 to 270; Isym 3)
 *   rotational          every plane is the one stored (IES one angle; Isym 1)
 *   none                C360 is C0                    (IES 0 to 360 or past 180; Isym 0)
 *
 * EULUMDAT Isym 3 stores the planes from C270 through C0 to C90 (the standard's
 * Mc1 = 3 Mc / 4 + 1 to Mc1 + Mc / 2), as written.
 *
 * Intensities are kept in the file's own units, scaled by its multiplier: an IES file's
 * candela multiplier (the ballast factor is reported, not applied), an EULUMDAT file's
 * conversion factor. So an IES map is in candela and an EULUMDAT map in candela per 1000
 * lumens of lamp flux, and a flux integrated from them is in lumens and in lumens per
 * 1000 lamp lumens respectively. Below the first gamma angle a file carries and above the
 * last the intensity is 0.
 *
 * This reads files the program did not write, so it is built on the guards the chart and
 * .cube readers settled on (docs/alwan_future.md, "Untrusted file input"). Numbers are
 * parsed here, without the C locale or sscanf, rejected unless the whole token is a number,
 * and rejected when their magnitude passes 1e308, compared against that bound rather than
 * HUGE_VAL. Every count is bounded before anything is allocated from it, products go
 * through alwan_safe_array_size, and the data a count promises must be present: a short
 * file, a count larger than the values that follow it, a non-increasing angle list or
 * anything after the last value fails the load and returns nothing.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LUM_MAX_ANGLES 100000u
#define LUM_MAX_VALUES 1000000u
#define LUM_MAX_KEYWORDS 4096u
#define LUM_MAX_TILT 10000u
#define LUM_MAX_LAMP_SETS 20u
#define LUM_MAX_FILE ((size_t)64 << 20)
#define LUM_EXP_CAP 400
#define LUM_ANGLE_TOL 1e-9
#define LUM_PI 3.14159265358979323846

struct alwan_luminaire {
    alwan_luminaire_info info;
    size_t nv, nh;
    double *vertical;      /* nv gamma angles */
    double *horizontal;    /* nh stored C-plane angles, as written */
    double *values;        /* nh x nv, plane by plane, as written */
    size_t nphi;           /* completed map: nphi planes from 0 to 360 */
    double *phi;
    double *map;           /* nphi x nv, scaled by the multiplier */
    size_t nkw;
    char **keys;
    char **vals;
};

/* ----------------------------------------------------------------
 * Text
 * ---------------------------------------------------------------- */

typedef struct {
    char const *p;
    size_t n;
} lum_span;

static int lum_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

static lum_span lum_trim(lum_span s) {
    while (s.n && lum_space(s.p[0])) { s.p++; s.n--; }
    while (s.n && lum_space(s.p[s.n - 1])) s.n--;
    return s;
}

static char lum_upper(char c) {
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

static int lum_starts_ci(lum_span s, char const *key) {
    size_t i;
    for (i = 0; key[i]; i++)
        if (i >= s.n || lum_upper(s.p[i]) != lum_upper(key[i])) return 0;
    return 1;
}

static int lum_equals(lum_span s, char const *key) {
    size_t k = strlen(key);
    return s.n == k && memcmp(s.p, key, k) == 0;
}

static char *lum_strdup(char const *p, size_t n) {
    char *out = (char *)ALWAN_ALLOC(n + 1, 1);
    if (!out) return NULL;
    if (n) memcpy(out, p, n);
    out[n] = '\0';
    return out;
}

/* The whole token must be a number. Up to 19 significant digits are kept exactly, and a
 * mantissa under 2^53 scaled by a power of ten up to 22 is one correctly rounded operation;
 * past that the scaling is repeated multiplication, within a few units in the last place.
 * The exponent is capped before it is used, so no input spins the loop or overflows the
 * accumulator, and a result beyond 1e308 is refused. */
static int lum_number(lum_span s, double *out) {
    static double const pow10[23] = {1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,
                                     1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
    size_t i = 0;
    int neg = 0, any = 0, digits = 0, e10 = 0, exp_val = 0, exp_neg = 0;
    unsigned long long mant = 0;
    double v;
    s = lum_trim(s);
    if (!s.n) return 0;
    if (s.p[i] == '+' || s.p[i] == '-') { neg = s.p[i] == '-'; i++; }
    for (; i < s.n && s.p[i] >= '0' && s.p[i] <= '9'; i++) {
        any = 1;
        if (mant == 0 && s.p[i] == '0') continue;
        if (digits < 19) { mant = mant * 10u + (unsigned)(s.p[i] - '0'); digits++; }
        else if (e10 < LUM_EXP_CAP) e10++;
    }
    if (i < s.n && s.p[i] == '.') {
        i++;
        for (; i < s.n && s.p[i] >= '0' && s.p[i] <= '9'; i++) {
            any = 1;
            if (mant == 0 && s.p[i] == '0') { if (e10 > -LUM_EXP_CAP) e10--; continue; }
            if (digits < 19) { mant = mant * 10u + (unsigned)(s.p[i] - '0'); digits++; if (e10 > -LUM_EXP_CAP) e10--; }
        }
    }
    if (!any) return 0;
    if (i < s.n && (s.p[i] == 'e' || s.p[i] == 'E')) {
        int ed = 0;
        i++;
        if (i < s.n && (s.p[i] == '+' || s.p[i] == '-')) { exp_neg = s.p[i] == '-'; i++; }
        for (; i < s.n && s.p[i] >= '0' && s.p[i] <= '9'; i++) {
            if (exp_val <= LUM_EXP_CAP) exp_val = exp_val * 10 + (s.p[i] - '0');
            ed = 1;
        }
        if (!ed) return 0;
    }
    if (i != s.n) return 0;
    if (exp_val > LUM_EXP_CAP) exp_val = LUM_EXP_CAP;
    e10 += exp_neg ? -exp_val : exp_val;
    if (mant == 0) {
        v = 0.0;
    } else if (mant < (1ull << 53) && e10 >= -22 && e10 <= 22) {
        v = e10 >= 0 ? (double)mant * pow10[e10] : (double)mant / pow10[-e10];
    } else {
        v = (double)mant;
        if (e10 > 2 * LUM_EXP_CAP) e10 = 2 * LUM_EXP_CAP;
        if (e10 < -2 * LUM_EXP_CAP) e10 = -2 * LUM_EXP_CAP;
        while (e10 > 0 && v <= 1.0e308) { v *= 10.0; e10--; }
        while (e10 < 0 && v != 0.0) { v /= 10.0; e10++; }
    }
    if (!(v <= 1.0e308 && v >= -1.0e308)) return 0;
    *out = neg ? -v : v;
    return 1;
}

/* A whole number within [lo, hi], written with or without a zero fraction. */
static int lum_integer(lum_span s, long lo, long hi, long *out) {
    double v;
    if (!lum_number(s, &v)) return 0;
    if (v != floor(v) || v < (double)lo || v > (double)hi) return 0;
    *out = (long)v;
    return 1;
}

/* Tokens of the IES photometric block: blanks, line breaks and commas separate them. */
typedef struct {
    char const *p;
    size_t n, at;
} lum_tokens;

static int lum_sep(char c) {
    return lum_space(c) || c == ',';
}

static int lum_next(lum_tokens *t, lum_span *out) {
    size_t b;
    while (t->at < t->n && lum_sep(t->p[t->at])) t->at++;
    if (t->at >= t->n) return 0;
    b = t->at;
    while (t->at < t->n && !lum_sep(t->p[t->at])) t->at++;
    out->p = t->p + b;
    out->n = t->at - b;
    return 1;
}

static int lum_next_number(lum_tokens *t, double *v) {
    lum_span s;
    return lum_next(t, &s) && lum_number(s, v);
}

static int lum_next_integer(lum_tokens *t, long lo, long hi, long *v) {
    lum_span s;
    return lum_next(t, &s) && lum_integer(s, lo, hi, v);
}

/* Lines, ended by LF, CR or CRLF. */
typedef struct {
    lum_span *line;
    size_t count;
} lum_lines;

static alwan_status lum_split(lum_lines *L, char const *buf, size_t len) {
    size_t i, n = 1, k = 0, start = 0, bytes;
    for (i = 0; i < len; i++)
        if (buf[i] == '\n' || buf[i] == '\r') n++;
    bytes = alwan_safe_array_size(n, sizeof(lum_span));
    if (!bytes) return ALWAN_E_RANGE;
    L->line = (lum_span *)ALWAN_ALLOC(bytes, sizeof(void *));
    if (!L->line) return ALWAN_E_NOMEM;
    for (i = 0; i < len; i++) {
        if (buf[i] == '\n' || buf[i] == '\r') {
            L->line[k].p = buf + start;
            L->line[k].n = i - start;
            k++;
            if (buf[i] == '\r' && i + 1 < len && buf[i + 1] == '\n') i++;
            start = i + 1;
        }
    }
    L->line[k].p = buf + start;
    L->line[k].n = len - start;
    k++;
    L->count = k;
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * The object
 * ---------------------------------------------------------------- */

void alwan_luminaire_destroy(alwan_luminaire *lum, alwan_ctx *ctx) {
    size_t i;
    (void)ctx;
    if (!lum) return;
    for (i = 0; i < lum->nkw; i++) {
        ALWAN_FREE(lum->keys[i]);
        ALWAN_FREE(lum->vals[i]);
    }
    ALWAN_FREE(lum->keys);
    ALWAN_FREE(lum->vals);
    ALWAN_FREE(lum->vertical);
    ALWAN_FREE(lum->horizontal);
    ALWAN_FREE(lum->values);
    ALWAN_FREE(lum->phi);
    ALWAN_FREE(lum->map);
    ALWAN_FREE(lum);
}

static alwan_status lum_keyword_add(struct alwan_luminaire *L, char const *k, size_t kn, char const *v, size_t vn) {
    if (L->nkw >= LUM_MAX_KEYWORDS) return ALWAN_E_RANGE;
    if (!L->keys) {
        L->keys = (char **)ALWAN_ALLOC(sizeof(char *) * LUM_MAX_KEYWORDS, sizeof(void *));
        L->vals = (char **)ALWAN_ALLOC(sizeof(char *) * LUM_MAX_KEYWORDS, sizeof(void *));
        if (!L->keys || !L->vals) return ALWAN_E_NOMEM;
    }
    L->keys[L->nkw] = lum_strdup(k, kn);
    L->vals[L->nkw] = lum_strdup(v, vn);
    if (!L->keys[L->nkw] || !L->vals[L->nkw]) {
        ALWAN_FREE(L->keys[L->nkw]);
        ALWAN_FREE(L->vals[L->nkw]);
        return ALWAN_E_NOMEM;
    }
    L->nkw++;
    return ALWAN_OK;
}

/* [MORE] continues the keyword before it, joined by a space. */
static alwan_status lum_keyword_more(struct alwan_luminaire *L, char const *v, size_t vn) {
    size_t old;
    char *joined;
    if (!L->nkw) return lum_keyword_add(L, "MORE", 4, v, vn);
    old = strlen(L->vals[L->nkw - 1]);
    if (old + vn + 2 > ((size_t)1 << 20)) return ALWAN_E_RANGE;
    joined = (char *)ALWAN_ALLOC(old + vn + 2, 1);
    if (!joined) return ALWAN_E_NOMEM;
    memcpy(joined, L->vals[L->nkw - 1], old);
    joined[old] = ' ';
    if (vn) memcpy(joined + old + 1, v, vn);
    joined[old + 1 + vn] = '\0';
    ALWAN_FREE(L->vals[L->nkw - 1]);
    L->vals[L->nkw - 1] = joined;
    return ALWAN_OK;
}

static double *lum_array(size_t n) {
    size_t bytes = alwan_safe_array_size(n ? n : 1, sizeof(double));
    return bytes ? (double *)ALWAN_ALLOC(bytes, sizeof(double)) : NULL;
}

static int lum_strictly_increasing(double const *a, size_t n) {
    size_t i;
    for (i = 1; i < n; i++)
        if (!(a[i] > a[i - 1])) return 0;
    return 1;
}

static int lum_near(double a, double b) {
    return fabs(a - b) <= LUM_ANGLE_TOL;
}

static double lum_wrap(double c) {
    double w = fmod(c, 360.0);
    if (w < 0.0) w += 360.0;
    if (w >= 360.0) w -= 360.0;
    return w;
}

/* ----------------------------------------------------------------
 * Completing the map
 * ---------------------------------------------------------------- */

static size_t lum_images(alwan_luminaire_symmetry sym, double c, double *out) {
    out[0] = lum_wrap(c);
    switch (sym) {
    case ALWAN_LUMINAIRE_SYMMETRY_C0_C180:
        out[1] = lum_wrap(-c);
        return 2;
    case ALWAN_LUMINAIRE_SYMMETRY_C90_C270:
        out[1] = lum_wrap(180.0 - c);
        return 2;
    case ALWAN_LUMINAIRE_SYMMETRY_QUADRANT:
        out[1] = lum_wrap(-c);
        out[2] = lum_wrap(180.0 - c);
        out[3] = lum_wrap(c + 180.0);
        return 4;
    default:
        return 1;
    }
}

static int lum_cmp_double(void const *a, void const *b) {
    double x = *(double const *)a, y = *(double const *)b;
    return (x > y) - (x < y);
}

/* The stored planes by angle, wrapped into [0, 360), so a symmetry image is found by
 * binary search rather than by a scan per completed plane. */
typedef struct {
    double c;
    size_t index;
} lum_plane;

static int lum_cmp_plane(void const *a, void const *b) {
    double x = ((lum_plane const *)a)->c, y = ((lum_plane const *)b)->c;
    if (x != y) return (x > y) - (x < y);
    return (((lum_plane const *)a)->index > ((lum_plane const *)b)->index) -
           (((lum_plane const *)a)->index < ((lum_plane const *)b)->index);
}

static long lum_lookup(lum_plane const *sorted, size_t n, double t) {
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (sorted[mid].c < t - LUM_ANGLE_TOL) lo = mid + 1; else hi = mid;
    }
    if (lo < n && sorted[lo].c <= t + LUM_ANGLE_TOL) return (long)sorted[lo].index;
    /* an image just under 360 matches a stored 0, and one at 0 a stored 359.999... */
    if (t > 360.0 - 2 * LUM_ANGLE_TOL && n && sorted[0].c <= LUM_ANGLE_TOL) return (long)sorted[0].index;
    if (t < 2 * LUM_ANGLE_TOL && n && sorted[n - 1].c >= 360.0 - LUM_ANGLE_TOL) return (long)sorted[n - 1].index;
    return -1;
}

/* The stored plane whose angle is a symmetry image of c, or -1. */
static long lum_find_plane(struct alwan_luminaire const *L, lum_plane const *sorted, double c) {
    double img[4];
    size_t k, n = lum_images(L->info.symmetry, c, img);
    for (k = 0; k < n; k++) {
        long p = lum_lookup(sorted, L->nh, img[k]);
        if (p >= 0) return p;
    }
    return -1;
}

static alwan_status lum_complete(struct alwan_luminaire *L) {
    double const mult = L->info.multiplier;
    size_t cap, n = 0, i, j, k;
    double *cand;
    if (L->info.symmetry == ALWAN_LUMINAIRE_SYMMETRY_ROTATIONAL) {
        L->nphi = 2;
        L->phi = lum_array(2);
        L->map = lum_array(2 * L->nv);
        if (!L->phi || !L->map) return ALWAN_E_NOMEM;
        L->phi[0] = 0.0;
        L->phi[1] = 360.0;
        for (j = 0; j < L->nv; j++) L->map[j] = L->map[L->nv + j] = L->values[j] * mult;
        return ALWAN_OK;
    }
    if (L->nh > LUM_MAX_ANGLES) return ALWAN_E_RANGE;
    cap = 4 * L->nh + 2;
    cand = lum_array(cap);
    if (!cand) return ALWAN_E_NOMEM;
    for (i = 0; i < L->nh; i++) {
        double img[4];
        size_t m = lum_images(L->info.symmetry, L->horizontal[i], img);
        for (k = 0; k < m; k++) cand[n++] = img[k];
    }
    qsort(cand, n, sizeof(double), lum_cmp_double);
    k = 0;
    for (i = 0; i < n; i++)
        if (k == 0 || !lum_near(cand[i], cand[k - 1])) cand[k++] = cand[i];
    n = k;
    if (n == 0 || !lum_near(cand[0], 0.0)) {
        ALWAN_FREE(cand);
        return ALWAN_E_RANGE;
    }
    cand[0] = 0.0;
    cand[n++] = 360.0;
    L->phi = cand;
    L->nphi = n;
    {
        size_t bytes = alwan_safe_array_size(n, L->nv);
        if (!bytes || !alwan_safe_array_size(bytes, sizeof(double))) return ALWAN_E_RANGE;
        L->map = lum_array(n * L->nv);
        if (!L->map) return ALWAN_E_NOMEM;
    }
    {
        lum_plane *sorted = (lum_plane *)ALWAN_ALLOC(alwan_safe_array_size(L->nh, sizeof(lum_plane)), sizeof(double));
        if (!sorted) return ALWAN_E_NOMEM;
        for (i = 0; i < L->nh; i++) {
            sorted[i].c = lum_wrap(L->horizontal[i]);
            sorted[i].index = i;
        }
        qsort(sorted, L->nh, sizeof(lum_plane), lum_cmp_plane);
        for (i = 0; i < n; i++) {
            long p = lum_find_plane(L, sorted, i + 1 == n ? 0.0 : L->phi[i]);
            if (p < 0) {
                ALWAN_FREE(sorted);
                return ALWAN_E_RANGE;
            }
            for (j = 0; j < L->nv; j++) L->map[i * L->nv + j] = L->values[(size_t)p * L->nv + j] * mult;
        }
        ALWAN_FREE(sorted);
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * IES LM-63
 * ---------------------------------------------------------------- */

static int lum_ies_version(lum_span s) {
    static struct { char const *name; int year; } const table[] = {
        {"IESNA:LM-63-1986", 1986}, {"IESNA:LM-63-1991", 1991}, {"IESNA91", 1991},
        {"IESNA:LM-63-1995", 1995}, {"IESNA:LM-63-2002", 2002}, {"IES:LM-63-2019", 2019}};
    size_t i;
    for (i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        if (lum_equals(s, table[i].name)) return table[i].year;
    return 0;
}

/* The TILT line: "TILT", blanks, "=". */
static int lum_is_tilt(lum_span s, lum_span *value) {
    size_t i = 4;
    s = lum_trim(s);
    if (!lum_starts_ci(s, "TILT")) return 0;
    while (i < s.n && (s.p[i] == ' ' || s.p[i] == '\t')) i++;
    if (i >= s.n || s.p[i] != '=') return 0;
    value->p = s.p + i + 1;
    value->n = s.n - i - 1;
    *value = lum_trim(*value);
    return 1;
}

static alwan_status lum_parse_ies(struct alwan_luminaire *L, lum_lines const *lines, char const *buf, size_t len) {
    alwan_luminaire_info *info = &L->info;
    size_t li = 0, i, count;
    lum_span first = lum_trim(lines->line[0]), tilt;
    lum_tokens tk;
    long lamps, nv, nh, ptype, units;
    double v;
    alwan_status st;

    info->format = ALWAN_LUMINAIRE_IES;
    info->version = 1986;
    if (lum_starts_ci(first, "IESNA") || lum_starts_ci(first, "IES:")) {
        info->version = lum_ies_version(first);
        li = 1;
    }
    for (;; li++) {
        lum_span s;
        if (li >= lines->count) return ALWAN_E_INVALID;
        if (lum_is_tilt(lines->line[li], &tilt)) break;
        s = lum_trim(lines->line[li]);
        if (s.n && s.p[0] == '[') {
            size_t e = 1;
            while (e < s.n && s.p[e] != ']') e++;
            if (e < s.n) {
                lum_span key = {s.p + 1, e - 1};
                lum_span val = {s.p + e + 1, s.n - e - 1};
                val = lum_trim(val);
                key = lum_trim(key);
                st = (key.n == 4 && lum_starts_ci(key, "MORE")) ? lum_keyword_more(L, val.p, val.n)
                                                              : lum_keyword_add(L, key.p, key.n, val.p, val.n);
                if (st != ALWAN_OK) return st;
            }
        }
    }
    if (!tilt.n) return ALWAN_E_INVALID;
    info->tilt = lum_equals(tilt, "NONE") ? ALWAN_LUMINAIRE_TILT_NONE
               : lum_equals(tilt, "INCLUDE") ? ALWAN_LUMINAIRE_TILT_INCLUDE : ALWAN_LUMINAIRE_TILT_FILE;
    if (info->tilt == ALWAN_LUMINAIRE_TILT_FILE) {
        st = lum_keyword_add(L, "TILT", 4, tilt.p, tilt.n);
        if (st != ALWAN_OK) return st;
    }

    /* Everything after the TILT line is one stream of numbers. */
    tk.p = buf;
    tk.n = len;
    tk.at = (size_t)((lines->line[li].p + lines->line[li].n) - buf);
    if (info->tilt == ALWAN_LUMINAIRE_TILT_INCLUDE) {
        long geometry, pairs;
        if (!lum_next_integer(&tk, 1, 3, &geometry)) return ALWAN_E_INVALID;
        if (!lum_next_integer(&tk, 0, (long)LUM_MAX_TILT, &pairs)) return ALWAN_E_INVALID;
        for (i = 0; i < 2 * (size_t)pairs; i++)
            if (!lum_next_number(&tk, &v)) return ALWAN_E_INVALID;
    }
    if (!lum_next_integer(&tk, 1, 100000, &lamps)) return ALWAN_E_INVALID;
    if (!lum_next_number(&tk, &info->lumens_per_lamp)) return ALWAN_E_INVALID;
    if (!lum_next_number(&tk, &info->multiplier)) return ALWAN_E_INVALID;
    if (!lum_next_integer(&tk, 1, (long)LUM_MAX_ANGLES, &nv)) return ALWAN_E_INVALID;
    if (!lum_next_integer(&tk, 1, (long)LUM_MAX_ANGLES, &nh)) return ALWAN_E_INVALID;
    if (!lum_next_integer(&tk, 1, 3, &ptype)) return ALWAN_E_INVALID;
    if (!lum_next_integer(&tk, 1, 2, &units)) return ALWAN_E_INVALID;
    if (!lum_next_number(&tk, &info->width) || !lum_next_number(&tk, &info->length) ||
        !lum_next_number(&tk, &info->height) || !lum_next_number(&tk, &info->ballast_factor) ||
        !lum_next_number(&tk, &v) || !lum_next_number(&tk, &info->input_watts))
        return ALWAN_E_INVALID;
    if (!(info->lumens_per_lamp > 0.0 || info->lumens_per_lamp == -1.0)) return ALWAN_E_RANGE;
    if (!(info->multiplier > 0.0) || info->ballast_factor < 0.0) return ALWAN_E_RANGE;
    if ((size_t)nv * (size_t)nh > LUM_MAX_VALUES) return ALWAN_E_RANGE;
    info->lamps = (int)lamps;
    info->lamp_sets = 1;
    info->lamp_flux = info->lumens_per_lamp < 0.0 ? -1.0 : info->lumens_per_lamp * (double)lamps;
    info->photometric_type = (int)ptype;
    info->units = (int)units;

    L->nv = (size_t)nv;
    L->nh = (size_t)nh;
    count = L->nv * L->nh;
    L->vertical = lum_array(L->nv);
    L->horizontal = lum_array(L->nh);
    L->values = lum_array(count);
    if (!L->vertical || !L->horizontal || !L->values) return ALWAN_E_NOMEM;
    for (i = 0; i < L->nv; i++)
        if (!lum_next_number(&tk, &L->vertical[i])) return ALWAN_E_INVALID;
    for (i = 0; i < L->nh; i++)
        if (!lum_next_number(&tk, &L->horizontal[i])) return ALWAN_E_INVALID;
    for (i = 0; i < count; i++)
        if (!lum_next_number(&tk, &L->values[i])) return ALWAN_E_INVALID;
    {
        lum_span extra;
        if (lum_next(&tk, &extra)) return ALWAN_E_INVALID;
    }
    if (!lum_strictly_increasing(L->vertical, L->nv) || !lum_strictly_increasing(L->horizontal, L->nh))
        return ALWAN_E_RANGE;
    for (i = 0; i < count; i++)
        if (L->values[i] < 0.0) return ALWAN_E_RANGE;

    if (ptype != 1) {
        info->symmetry = ALWAN_LUMINAIRE_SYMMETRY_NONE;
        return ALWAN_OK;
    }
    if (L->vertical[0] < 0.0 || L->vertical[L->nv - 1] > 180.0 || L->nv < 2) return ALWAN_E_RANGE;
    if (L->horizontal[0] < 0.0 || L->horizontal[L->nh - 1] > 360.0) return ALWAN_E_RANGE;
    {
        double h0 = L->horizontal[0], hl = L->horizontal[L->nh - 1];
        if (L->nh == 1) info->symmetry = ALWAN_LUMINAIRE_SYMMETRY_ROTATIONAL;
        else if (lum_near(h0, 0.0) && lum_near(hl, 90.0)) info->symmetry = ALWAN_LUMINAIRE_SYMMETRY_QUADRANT;
        else if (lum_near(h0, 0.0) && lum_near(hl, 180.0)) info->symmetry = ALWAN_LUMINAIRE_SYMMETRY_C0_C180;
        else if (lum_near(h0, 90.0) && lum_near(hl, 270.0)) info->symmetry = ALWAN_LUMINAIRE_SYMMETRY_C90_C270;
        else if (lum_near(h0, 0.0) && hl > 180.0) info->symmetry = ALWAN_LUMINAIRE_SYMMETRY_NONE;
        else return ALWAN_E_RANGE;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * EULUMDAT
 * ---------------------------------------------------------------- */

static alwan_status lum_ldt_line(lum_lines const *lines, size_t i, lum_span *out) {
    if (i >= lines->count) return ALWAN_E_INVALID;
    *out = lum_trim(lines->line[i]);
    return ALWAN_OK;
}

static alwan_status lum_ldt_number(lum_lines const *lines, size_t i, double *v) {
    lum_span s;
    if (lum_ldt_line(lines, i, &s) != ALWAN_OK || !lum_number(s, v)) return ALWAN_E_INVALID;
    return ALWAN_OK;
}

static alwan_status lum_ldt_integer(lum_lines const *lines, size_t i, long lo, long hi, long *v) {
    lum_span s;
    if (lum_ldt_line(lines, i, &s) != ALWAN_OK || !lum_integer(s, lo, hi, v)) return ALWAN_E_INVALID;
    return ALWAN_OK;
}

static alwan_status lum_ldt_text(struct alwan_luminaire *L, lum_lines const *lines, size_t i, char const *key) {
    lum_span s;
    alwan_status st = lum_ldt_line(lines, i, &s);
    if (st != ALWAN_OK) return st;
    return lum_keyword_add(L, key, strlen(key), s.p, s.n);
}

static alwan_status lum_parse_ldt(struct alwan_luminaire *L, lum_lines const *lines) {
    alwan_luminaire_info *info = &L->info;
    long ityp, isym, mc, ng, sets, lamps;
    double v, *c_all = NULL;
    size_t base, i, k, stored, first, count;
    alwan_status st;

    info->format = ALWAN_LUMINAIRE_EULUMDAT;
    info->photometric_type = 1;
    info->ballast_factor = 1.0;
    info->units = 0;
    if ((st = lum_ldt_text(L, lines, 0, "COMPANY")) != ALWAN_OK) return st;
    if ((st = lum_ldt_integer(lines, 1, 0, 3, &ityp)) != ALWAN_OK) return st;
    if ((st = lum_ldt_integer(lines, 2, 0, 4, &isym)) != ALWAN_OK) return st;
    if ((st = lum_ldt_integer(lines, 3, 1, (long)LUM_MAX_ANGLES, &mc)) != ALWAN_OK) return st;
    if ((st = lum_ldt_number(lines, 4, &v)) != ALWAN_OK) return st;
    if ((st = lum_ldt_integer(lines, 5, 1, (long)LUM_MAX_ANGLES, &ng)) != ALWAN_OK) return st;
    if ((st = lum_ldt_number(lines, 6, &v)) != ALWAN_OK) return st;
    if ((st = lum_ldt_text(L, lines, 7, "REPORT")) != ALWAN_OK) return st;
    if ((st = lum_ldt_text(L, lines, 8, "LUMINAIRE")) != ALWAN_OK) return st;
    if ((st = lum_ldt_text(L, lines, 9, "NUMBER")) != ALWAN_OK) return st;
    if ((st = lum_ldt_text(L, lines, 10, "FILENAME")) != ALWAN_OK) return st;
    if ((st = lum_ldt_text(L, lines, 11, "DATE")) != ALWAN_OK) return st;
    if ((st = lum_ldt_number(lines, 12, &info->length)) != ALWAN_OK) return st;
    if ((st = lum_ldt_number(lines, 13, &info->width)) != ALWAN_OK) return st;
    if ((st = lum_ldt_number(lines, 14, &info->height)) != ALWAN_OK) return st;
    for (i = 15; i <= 20; i++)
        if ((st = lum_ldt_number(lines, i, &v)) != ALWAN_OK) return st;
    if ((st = lum_ldt_number(lines, 21, &info->downward_flux_fraction)) != ALWAN_OK) return st;
    if ((st = lum_ldt_number(lines, 22, &info->light_output_ratio)) != ALWAN_OK) return st;
    if ((st = lum_ldt_number(lines, 23, &info->multiplier)) != ALWAN_OK) return st;
    if ((st = lum_ldt_number(lines, 24, &info->tilt_angle)) != ALWAN_OK) return st;
    if ((st = lum_ldt_integer(lines, 25, 1, (long)LUM_MAX_LAMP_SETS, &sets)) != ALWAN_OK) return st;
    if (!(info->multiplier > 0.0)) return ALWAN_E_RANGE;
    info->luminaire_type = (int)ityp;
    info->symmetry = (alwan_luminaire_symmetry)isym;
    info->lamp_sets = (size_t)sets;
    info->tilt = ALWAN_LUMINAIRE_TILT_NONE;
    for (k = 0; k < (size_t)sets; k++) {
        size_t b = 26 + 6 * k;
        double flux, watts;
        if ((st = lum_ldt_integer(lines, b, -100000, 100000, &lamps)) != ALWAN_OK) return st;
        if (k == 0) info->lamps = (int)lamps;
        if (k == 0 && (st = lum_ldt_text(L, lines, b + 1, "LAMP_TYPE")) != ALWAN_OK) return st;
        if ((st = lum_ldt_number(lines, b + 2, &flux)) != ALWAN_OK) return st;
        if (k == 0 && (st = lum_ldt_text(L, lines, b + 3, "CCT")) != ALWAN_OK) return st;
        if (k == 0 && (st = lum_ldt_text(L, lines, b + 4, "CRI")) != ALWAN_OK) return st;
        if ((st = lum_ldt_number(lines, b + 5, &watts)) != ALWAN_OK) return st;
        info->lamp_flux += flux;
        info->input_watts += watts;
        if (k != 0) {
            lum_span s;
            if ((st = lum_ldt_line(lines, b + 1, &s)) != ALWAN_OK) return st;
            if ((st = lum_ldt_line(lines, b + 3, &s)) != ALWAN_OK) return st;
            if ((st = lum_ldt_line(lines, b + 4, &s)) != ALWAN_OK) return st;
        }
    }
    base = 26 + 6 * (size_t)sets;
    for (i = 0; i < 10; i++)
        if ((st = lum_ldt_number(lines, base + i, &v)) != ALWAN_OK) return st;
    base += 10;

    switch (isym) {
    case 0: stored = (size_t)mc; first = 0; break;
    case 1: stored = 1; first = 0; break;
    case 2:
        if (mc % 2) return ALWAN_E_RANGE;
        stored = (size_t)mc / 2 + 1; first = 0; break;
    case 3:
        if (mc % 4) return ALWAN_E_RANGE;
        stored = (size_t)mc / 2 + 1; first = 3 * (size_t)mc / 4; break;
    default:
        if (mc % 4) return ALWAN_E_RANGE;
        stored = (size_t)mc / 4 + 1; first = 0; break;
    }
    if (stored * (size_t)ng > LUM_MAX_VALUES) return ALWAN_E_RANGE;
    L->nh = stored;
    L->nv = (size_t)ng;
    count = L->nh * L->nv;
    c_all = lum_array((size_t)mc);
    L->horizontal = lum_array(L->nh);
    L->vertical = lum_array(L->nv);
    L->values = lum_array(count);
    if (!c_all || !L->horizontal || !L->vertical || !L->values) { ALWAN_FREE(c_all); return ALWAN_E_NOMEM; }
    for (i = 0; i < (size_t)mc; i++)
        if ((st = lum_ldt_number(lines, base + i, &c_all[i])) != ALWAN_OK) { ALWAN_FREE(c_all); return st; }
    base += (size_t)mc;
    for (i = 0; i < L->nv; i++)
        if ((st = lum_ldt_number(lines, base + i, &L->vertical[i])) != ALWAN_OK) { ALWAN_FREE(c_all); return st; }
    base += L->nv;
    for (i = 0; i < count; i++)
        if ((st = lum_ldt_number(lines, base + i, &L->values[i])) != ALWAN_OK) { ALWAN_FREE(c_all); return st; }
    base += count;
    for (i = base; i < lines->count; i++)
        if (lum_trim(lines->line[i]).n) { ALWAN_FREE(c_all); return ALWAN_E_INVALID; }

    if (!lum_strictly_increasing(c_all, (size_t)mc) || c_all[0] < 0.0 || !(c_all[mc - 1] < 360.0) ||
        !lum_strictly_increasing(L->vertical, L->nv) || L->vertical[0] < 0.0 || L->vertical[L->nv - 1] > 180.0 ||
        L->nv < 2) {
        ALWAN_FREE(c_all);
        return ALWAN_E_RANGE;
    }
    for (i = 0; i < L->nh; i++) L->horizontal[i] = c_all[(first + i) % (size_t)mc];
    {
        int ok = 1;
        switch (isym) {
        case 0: ok = lum_near(c_all[0], 0.0); break;
        case 2: ok = lum_near(c_all[0], 0.0) && lum_near(c_all[mc / 2], 180.0); break;
        case 3: ok = lum_near(c_all[0], 0.0) && lum_near(c_all[3 * mc / 4], 270.0) && lum_near(c_all[mc / 4], 90.0); break;
        case 4: ok = lum_near(c_all[0], 0.0) && lum_near(c_all[mc / 4], 90.0); break;
        default: break;
        }
        ALWAN_FREE(c_all);
        if (!ok) return ALWAN_E_RANGE;
    }
    for (i = 0; i < count; i++)
        if (L->values[i] < 0.0) return ALWAN_E_RANGE;
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Loading
 * ---------------------------------------------------------------- */

alwan_status alwan_luminaire_load_buffer(alwan_luminaire **out, char const *buf, size_t len, alwan_ctx *ctx) {
    struct alwan_luminaire *L;
    lum_lines lines = {NULL, 0};
    alwan_status st;
    size_t i;
    int ies = 0;
    (void)ctx;
    if (out) *out = NULL;
    if (!out || !buf) return ALWAN_E_INVALID;
    if (len > LUM_MAX_FILE) return ALWAN_E_RANGE;
    if (len >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF) {
        buf += 3;
        len -= 3;
    }
    if (!len) return ALWAN_E_INVALID;
    st = lum_split(&lines, buf, len);
    if (st != ALWAN_OK) return st;
    {
        lum_span first = lum_trim(lines.line[0]), tilt;
        ies = lum_starts_ci(first, "IESNA") || lum_starts_ci(first, "IES:");
        for (i = 0; !ies && i < lines.count; i++)
            if (lum_is_tilt(lines.line[i], &tilt)) ies = 1;
    }
    L = (struct alwan_luminaire *)ALWAN_ALLOC(sizeof(*L), sizeof(void *));
    if (!L) {
        ALWAN_FREE(lines.line);
        return ALWAN_E_NOMEM;
    }
    memset(L, 0, sizeof(*L));
    st = ies ? lum_parse_ies(L, &lines, buf, len) : lum_parse_ldt(L, &lines);
    ALWAN_FREE(lines.line);
    if (st == ALWAN_OK && L->info.photometric_type == 1) st = lum_complete(L);
    if (st != ALWAN_OK) {
        alwan_luminaire_destroy(L, ctx);
        return st;
    }
    *out = L;
    return ALWAN_OK;
}

alwan_status alwan_luminaire_load(alwan_luminaire **out, char const *path, alwan_ctx *ctx) {
    FILE *fh;
    char *buf;
    long size;
    size_t got;
    alwan_status st;
    if (out) *out = NULL;
    if (!out || !path) return ALWAN_E_INVALID;
    fh = fopen(path, "rb");
    if (!fh) return ALWAN_E_INVALID;
    if (fseek(fh, 0, SEEK_END) != 0) { fclose(fh); return ALWAN_E_INVALID; }
    size = ftell(fh);
    if (size < 0 || (unsigned long)size > (unsigned long)LUM_MAX_FILE) { fclose(fh); return size < 0 ? ALWAN_E_INVALID : ALWAN_E_RANGE; }
    if (fseek(fh, 0, SEEK_SET) != 0) { fclose(fh); return ALWAN_E_INVALID; }
    buf = (char *)ALWAN_ALLOC((size_t)size + 1, 1);
    if (!buf) { fclose(fh); return ALWAN_E_NOMEM; }
    got = fread(buf, 1, (size_t)size, fh);
    fclose(fh);
    st = alwan_luminaire_load_buffer(out, buf, got, ctx);
    ALWAN_FREE(buf);
    return st;
}

/* ----------------------------------------------------------------
 * Accessors
 * ---------------------------------------------------------------- */

alwan_status alwan_luminaire_get_info(alwan_luminaire_info *info, alwan_luminaire const *lum) {
    if (!info || !lum) return ALWAN_E_INVALID;
    *info = lum->info;
    return ALWAN_OK;
}

alwan_status alwan_luminaire_angles(double const **vertical, size_t *nv, double const **horizontal, size_t *nh,
                                    alwan_luminaire const *lum) {
    if (!lum) return ALWAN_E_INVALID;
    if (vertical) *vertical = lum->vertical;
    if (nv) *nv = lum->nv;
    if (horizontal) *horizontal = lum->horizontal;
    if (nh) *nh = lum->nh;
    return ALWAN_OK;
}

alwan_status alwan_luminaire_values(double const **values, size_t *count, alwan_luminaire const *lum) {
    if (!lum || !values) return ALWAN_E_INVALID;
    *values = lum->values;
    if (count) *count = lum->nv * lum->nh;
    return ALWAN_OK;
}

alwan_status alwan_luminaire_map(double const **c_angles, size_t *nc, double const **gamma, size_t *ngamma,
                                 double const **values, alwan_luminaire const *lum) {
    if (!lum) return ALWAN_E_INVALID;
    if (!lum->map) return ALWAN_E_NODATA;
    if (c_angles) *c_angles = lum->phi;
    if (nc) *nc = lum->nphi;
    if (gamma) *gamma = lum->vertical;
    if (ngamma) *ngamma = lum->nv;
    if (values) *values = lum->map;
    return ALWAN_OK;
}

char const *alwan_luminaire_keyword(alwan_luminaire const *lum, char const *key) {
    size_t i;
    if (!lum || !key) return NULL;
    for (i = 0; i < lum->nkw; i++) {
        char const *a = lum->keys[i], *b = key;
        while (*a && *b && lum_upper(*a) == lum_upper(*b)) { a++; b++; }
        if (!*a && !*b) return lum->vals[i];
    }
    return NULL;
}

/* The interval [a[k], a[k + 1]] holding x, a ascending, n >= 2. */
static size_t lum_bracket(double const *a, size_t n, double x) {
    size_t lo = 0, hi = n - 1;
    while (hi - lo > 1) {
        size_t mid = lo + (hi - lo) / 2;
        if (a[mid] <= x) lo = mid; else hi = mid;
    }
    return lo;
}

alwan_status alwan_luminaire_intensity(double *out, alwan_luminaire const *lum, double c_deg, double gamma_deg) {
    size_t p, g, nv;
    double c, tp, tg, a, b;
    if (!out || !lum) return ALWAN_E_INVALID;
    if (c_deg != c_deg || gamma_deg != gamma_deg || !(fabs(c_deg) <= 1.0e12)) return ALWAN_E_INVALID;
    if (!lum->map) return ALWAN_E_NODATA;
    nv = lum->nv;
    if (gamma_deg < lum->vertical[0] - LUM_ANGLE_TOL || gamma_deg > lum->vertical[nv - 1] + LUM_ANGLE_TOL) {
        *out = 0.0;
        return ALWAN_OK;
    }
    if (gamma_deg < lum->vertical[0]) gamma_deg = lum->vertical[0];
    if (gamma_deg > lum->vertical[nv - 1]) gamma_deg = lum->vertical[nv - 1];
    c = lum_wrap(c_deg);
    p = lum_bracket(lum->phi, lum->nphi, c);
    g = lum_bracket(lum->vertical, nv, gamma_deg);
    tp = (c - lum->phi[p]) / (lum->phi[p + 1] - lum->phi[p]);
    tg = (gamma_deg - lum->vertical[g]) / (lum->vertical[g + 1] - lum->vertical[g]);
    a = lum->map[p * nv + g] + tg * (lum->map[p * nv + g + 1] - lum->map[p * nv + g]);
    b = lum->map[(p + 1) * nv + g] + tg * (lum->map[(p + 1) * nv + g + 1] - lum->map[(p + 1) * nv + g]);
    *out = a + tp * (b - a);
    return ALWAN_OK;
}

/* One plane's integral over gamma of I sin(gamma), in radians. LINEAR integrates the
 * straight line between samples exactly; TRAPEZOID applies the trapezoid rule to
 * I sin(gamma) at the samples, as scipy.integrate.trapezoid does. */
static double lum_plane_integral(double const *I, double const *gamma_deg, size_t nv, alwan_luminaire_flux_method method) {
    double const d2r = LUM_PI / 180.0;
    double sum = 0.0;
    size_t j;
    for (j = 0; j + 1 < nv; j++) {
        double a = gamma_deg[j] * d2r, b = gamma_deg[j + 1] * d2r, h = b - a;
        if (method == ALWAN_LUMINAIRE_FLUX_TRAPEZOID) {
            sum += h * (I[j] * sin(a) + I[j + 1] * sin(b)) * 0.5;
        } else {
            double const ca = cos(a), cb = cos(b), sa = sin(a), sb = sin(b);
            /* integral of (I_a + (I_b - I_a)(t - a)/h) sin t from a to b */
            sum += I[j] * (ca - cb) + (I[j + 1] - I[j]) / h * (sb - sa - h * cb);
        }
    }
    return sum;
}

alwan_status alwan_luminaire_flux(double *flux, alwan_luminaire const *lum, alwan_luminaire_flux_method method) {
    double const d2r = LUM_PI / 180.0;
    double sum = 0.0, prev;
    size_t k;
    if (!flux || !lum) return ALWAN_E_INVALID;
    if (method != ALWAN_LUMINAIRE_FLUX_LINEAR && method != ALWAN_LUMINAIRE_FLUX_TRAPEZOID) return ALWAN_E_INVALID;
    if (!lum->map) return ALWAN_E_NODATA;
    prev = lum_plane_integral(lum->map, lum->vertical, lum->nv, method);
    for (k = 0; k + 1 < lum->nphi; k++) {
        double next = lum_plane_integral(lum->map + (k + 1) * lum->nv, lum->vertical, lum->nv, method);
        sum += (lum->phi[k + 1] - lum->phi[k]) * d2r * (prev + next) * 0.5;
        prev = next;
    }
    *flux = sum;
    return ALWAN_OK;
}
