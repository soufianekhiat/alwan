/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Iso-contours by marching squares, and Douglas-Peucker polygon simplification, as
 * scikit-image's skimage.measure.find_contours and approximate_polygon (BSD-3-Clause),
 * which suite 249 holds this to point for point.
 *
 * A 2 x 2 square walks the image row by row. Each corner above the level sets one bit
 * (upper left 1, upper right 2, lower left 4, lower right 8; a corner equal to the level
 * counts as below). The square's case gives the segment or segments that cross it, their
 * ends interpolated linearly along the edges, directed so the lower values lie on the left.
 * Cases 6 and 9, the saddles, join the high corners when fully_connected_high is set and
 * the low ones otherwise. A square with a NaN corner, or a corner outside the mask, is
 * skipped.
 *
 * The segments are joined in the order they are made. Each open contour is indexed by its
 * first and its last point; a segment whose ends meet both joins them (or closes one
 * contour, repeating its first point), and the contour made first keeps its place, so the
 * contours come out ordered by where they begin. Points are matched by value, which is
 * exact: neighbouring squares interpolate the same pair of values the same way. A segment
 * of zero length (a corner exactly at the level with the rest on one side) is dropped.
 *
 * positive_orientation_high reverses every contour. The level defaults to the midpoint of
 * the smallest and the largest value that is not NaN. On 8-bit data scikit-image forms that
 * sum in the image's own type, so it wraps past 255; alwan takes the true midpoint.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <string.h>

struct alwan_contours {
    size_t count;
    size_t *start;      /* count + 1 offsets into points, in points */
    double *points;     /* (row, col) pairs */
};

typedef struct {
    double r, c;
    size_t next;        /* node index, CT_NONE at the end */
} ct_node;

typedef struct {
    size_t first, last;
    int alive;
} ct_contour;

#define CT_NONE ((size_t)-1)

/* An open-addressed map from a point to a contour index, with tombstones. */
typedef struct {
    double *r, *c;
    size_t *val;
    unsigned char *state;   /* 0 empty, 1 used, 2 deleted */
    size_t cap, used, filled;
} ct_map;

typedef struct {
    ct_node *nodes;
    size_t nn, ncap;
    ct_contour *ct;
    size_t nc, ccap;
    ct_map starts, ends;
    int failed;
} ct_state;

static size_t ct_hash(double r, double c) {
    unsigned long long a, b, h;
    r += 0.0; c += 0.0;             /* -0.0 and 0.0 are the same point */
    memcpy(&a, &r, sizeof a);
    memcpy(&b, &c, sizeof b);
    h = a * 0x9E3779B97F4A7C15ull ^ (b + 0x632BE59BD9B4E019ull + (a << 6) + (a >> 2));
    h ^= h >> 31;
    h *= 0xBF58476D1CE4E5B9ull;
    h ^= h >> 29;
    return (size_t)h;
}

static int ct_map_init(ct_map *m, size_t cap) {
    m->cap = cap; m->used = 0; m->filled = 0;
    m->r = (double *)ALWAN_ALLOC(alwan_safe_array_size(cap, sizeof(double)), sizeof(double));
    m->c = (double *)ALWAN_ALLOC(alwan_safe_array_size(cap, sizeof(double)), sizeof(double));
    m->val = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(cap, sizeof(size_t)), sizeof(size_t));
    m->state = (unsigned char *)ALWAN_ALLOC(cap, sizeof(double));
    if (!m->r || !m->c || !m->val || !m->state) return 0;
    memset(m->state, 0, cap);
    return 1;
}

static void ct_map_free(ct_map *m) {
    ALWAN_FREE(m->r); ALWAN_FREE(m->c); ALWAN_FREE(m->val); ALWAN_FREE(m->state);
    m->r = m->c = NULL; m->val = NULL; m->state = NULL;
}

static size_t ct_map_find(ct_map const *m, double r, double c) {
    size_t i = ct_hash(r, c) & (m->cap - 1);
    for (;;) {
        if (m->state[i] == 0) return CT_NONE;
        if (m->state[i] == 1 && m->r[i] == r && m->c[i] == c) return i;
        i = (i + 1) & (m->cap - 1);
    }
}

static int ct_map_put(ct_map *m, double r, double c, size_t v);

static int ct_map_grow(ct_map *m) {
    ct_map n;
    size_t i, cap = m->cap;
    if (m->used * 4 >= cap) cap *= 2;   /* else rebuilding alone clears the tombstones */
    if (!ct_map_init(&n, cap)) { ct_map_free(&n); return 0; }
    for (i = 0; i < m->cap; i++)
        if (m->state[i] == 1) ct_map_put(&n, m->r[i], m->c[i], m->val[i]);
    ct_map_free(m);
    *m = n;
    return 1;
}

/* Insert or overwrite, as a Python dict assignment. */
static int ct_map_put(ct_map *m, double r, double c, size_t v) {
    size_t i = ct_map_find(m, r, c);
    if (i != CT_NONE) { m->val[i] = v; return 1; }
    if ((m->filled + 1) * 2 > m->cap && !ct_map_grow(m)) return 0;
    i = ct_hash(r, c) & (m->cap - 1);
    while (m->state[i] == 1) i = (i + 1) & (m->cap - 1);
    if (m->state[i] == 0) m->filled++;
    m->state[i] = 1; m->r[i] = r; m->c[i] = c; m->val[i] = v;
    m->used++;
    return 1;
}

/* Remove and return, as dict.pop(key, None); CT_NONE when absent. */
static size_t ct_map_pop(ct_map *m, double r, double c) {
    size_t i = ct_map_find(m, r, c);
    if (i == CT_NONE) return CT_NONE;
    m->state[i] = 2;
    m->used--;
    return m->val[i];
}

static size_t ct_node_new(ct_state *s, double r, double c) {
    if (s->nn == s->ncap) {
        size_t cap = s->ncap ? s->ncap * 2 : 256;
        ct_node *n = (ct_node *)ALWAN_REALLOC(s->nodes, s->ncap * sizeof(ct_node),
                                              alwan_safe_array_size(cap, sizeof(ct_node)), sizeof(double));
        if (!n) { s->failed = 1; return CT_NONE; }
        s->nodes = n; s->ncap = cap;
    }
    s->nodes[s->nn].r = r; s->nodes[s->nn].c = c; s->nodes[s->nn].next = CT_NONE;
    return s->nn++;
}

static size_t ct_contour_new(ct_state *s) {
    if (s->nc == s->ccap) {
        size_t cap = s->ccap ? s->ccap * 2 : 64;
        ct_contour *n = (ct_contour *)ALWAN_REALLOC(s->ct, s->ccap * sizeof(ct_contour),
                                                    alwan_safe_array_size(cap, sizeof(ct_contour)), sizeof(double));
        if (!n) { s->failed = 1; return CT_NONE; }
        s->ct = n; s->ccap = cap;
    }
    s->ct[s->nc].first = s->ct[s->nc].last = CT_NONE;
    s->ct[s->nc].alive = 1;
    return s->nc++;
}

/* _assemble_contours, one segment at a time. */
static void ct_add_segment(ct_state *s, double fr, double fc, double tr, double tc) {
    size_t tail, head;
    if (s->failed) return;
    if (fr == tr && fc == tc) return;
    tail = ct_map_pop(&s->starts, tr, tc);
    head = ct_map_pop(&s->ends, fr, fc);
    if (tail != CT_NONE && head != CT_NONE) {
        if (tail == head) {
            size_t n = ct_node_new(s, tr, tc);
            if (n == CT_NONE) return;
            s->nodes[s->ct[head].last].next = n;
            s->ct[head].last = n;
        } else if (tail > head) {
            ct_contour *h = &s->ct[head], *t = &s->ct[tail];
            s->nodes[h->last].next = t->first;
            h->last = t->last;
            t->alive = 0;
            if (!ct_map_put(&s->starts, s->nodes[h->first].r, s->nodes[h->first].c, head) ||
                !ct_map_put(&s->ends, s->nodes[h->last].r, s->nodes[h->last].c, head)) s->failed = 1;
        } else {
            ct_contour *h = &s->ct[head], *t = &s->ct[tail];
            ct_map_pop(&s->starts, s->nodes[h->first].r, s->nodes[h->first].c);
            s->nodes[h->last].next = t->first;
            t->first = h->first;
            h->alive = 0;
            if (!ct_map_put(&s->starts, s->nodes[t->first].r, s->nodes[t->first].c, tail) ||
                !ct_map_put(&s->ends, s->nodes[t->last].r, s->nodes[t->last].c, tail)) s->failed = 1;
        }
    } else if (tail == CT_NONE && head == CT_NONE) {
        size_t k = ct_contour_new(s), a, b;
        if (k == CT_NONE) return;
        a = ct_node_new(s, fr, fc);
        b = ct_node_new(s, tr, tc);
        if (a == CT_NONE || b == CT_NONE) return;
        s->nodes[a].next = b;
        s->ct[k].first = a; s->ct[k].last = b;
        if (!ct_map_put(&s->starts, fr, fc, k) || !ct_map_put(&s->ends, tr, tc, k)) s->failed = 1;
    } else if (head == CT_NONE) {
        size_t n = ct_node_new(s, fr, fc);
        if (n == CT_NONE) return;
        s->nodes[n].next = s->ct[tail].first;
        s->ct[tail].first = n;
        if (!ct_map_put(&s->starts, fr, fc, tail)) s->failed = 1;
    } else {
        size_t n = ct_node_new(s, tr, tc);
        if (n == CT_NONE) return;
        s->nodes[s->ct[head].last].next = n;
        s->ct[head].last = n;
        if (!ct_map_put(&s->ends, tr, tc, head)) s->failed = 1;
    }
}

static double ct_fraction(double from, double to, double level) {
    if (to == from) return 0.0;
    return (level - from) / (to - from);
}

static alwan_status ct_find(alwan_contours **out, double const *a, size_t w, size_t h,
                            alwan_contour_params const *params) {
    alwan_contour_params p;
    ct_state s;
    double level;
    size_t r0, c0, i, n_alive, n_pts;
    alwan_contours *res;
    alwan_status st = ALWAN_OK;

    if (params) p = *params; else memset(&p, 0, sizeof p);
    if (p.level_given) {
        level = p.level;
    } else {
        double lo = HUGE_VAL, hi = -HUGE_VAL;
        int any = 0;
        for (i = 0; i < w * h; i++) {
            if (isnan(a[i])) continue;
            if (a[i] < lo) lo = a[i];
            if (a[i] > hi) hi = a[i];
            any = 1;
        }
        level = any ? (lo + hi) / 2.0 : NAN;
    }

    memset(&s, 0, sizeof s);
    if (!ct_map_init(&s.starts, 256) || !ct_map_init(&s.ends, 256)) { st = ALWAN_E_NOMEM; goto done; }

    for (r0 = 0; r0 + 1 < h; r0++) {
        for (c0 = 0; c0 + 1 < w; c0++) {
            size_t const r1 = r0 + 1, c1 = c0 + 1;
            double ul, ur, ll, lr;
            double tr_, tc_, br_, bc_, lr_, lc_, rr_, rc_;
            int sq = 0;
            if (p.mask) {
                unsigned char const *m0 = p.mask + r0 * p.mask_row_stride;
                unsigned char const *m1 = p.mask + r1 * p.mask_row_stride;
                if (!(m0[c0] && m0[c1] && m1[c0] && m1[c1])) continue;
            }
            ul = a[r0 * w + c0]; ur = a[r0 * w + c1];
            ll = a[r1 * w + c0]; lr = a[r1 * w + c1];
            if (isnan(ul) || isnan(ur) || isnan(ll) || isnan(lr)) continue;
            if (ul > level) sq += 1;
            if (ur > level) sq += 2;
            if (ll > level) sq += 4;
            if (lr > level) sq += 8;
            if (sq == 0 || sq == 15) continue;
            tr_ = (double)r0; tc_ = (double)c0 + ct_fraction(ul, ur, level);
            br_ = (double)r1; bc_ = (double)c0 + ct_fraction(ll, lr, level);
            lr_ = (double)r0 + ct_fraction(ul, ll, level); lc_ = (double)c0;
            rr_ = (double)r0 + ct_fraction(ur, lr, level); rc_ = (double)c1;
            switch (sq) {
            case 1: ct_add_segment(&s, tr_, tc_, lr_, lc_); break;
            case 2: ct_add_segment(&s, rr_, rc_, tr_, tc_); break;
            case 3: ct_add_segment(&s, rr_, rc_, lr_, lc_); break;
            case 4: ct_add_segment(&s, lr_, lc_, br_, bc_); break;
            case 5: ct_add_segment(&s, tr_, tc_, br_, bc_); break;
            case 6:
                if (p.fully_connected_high) {
                    ct_add_segment(&s, lr_, lc_, tr_, tc_);
                    ct_add_segment(&s, rr_, rc_, br_, bc_);
                } else {
                    ct_add_segment(&s, rr_, rc_, tr_, tc_);
                    ct_add_segment(&s, lr_, lc_, br_, bc_);
                }
                break;
            case 7: ct_add_segment(&s, rr_, rc_, br_, bc_); break;
            case 8: ct_add_segment(&s, br_, bc_, rr_, rc_); break;
            case 9:
                if (p.fully_connected_high) {
                    ct_add_segment(&s, tr_, tc_, rr_, rc_);
                    ct_add_segment(&s, br_, bc_, lr_, lc_);
                } else {
                    ct_add_segment(&s, tr_, tc_, lr_, lc_);
                    ct_add_segment(&s, br_, bc_, rr_, rc_);
                }
                break;
            case 10: ct_add_segment(&s, br_, bc_, tr_, tc_); break;
            case 11: ct_add_segment(&s, br_, bc_, lr_, lc_); break;
            case 12: ct_add_segment(&s, lr_, lc_, rr_, rc_); break;
            case 13: ct_add_segment(&s, tr_, tc_, rr_, rc_); break;
            default: ct_add_segment(&s, lr_, lc_, tr_, tc_); break;   /* 14 */
            }
            if (s.failed) { st = ALWAN_E_NOMEM; goto done; }
        }
    }

    n_alive = 0; n_pts = 0;
    for (i = 0; i < s.nc; i++) {
        size_t k;
        if (!s.ct[i].alive) continue;
        n_alive++;
        for (k = s.ct[i].first; k != CT_NONE; k = s.nodes[k].next) n_pts++;
    }
    res = (alwan_contours *)ALWAN_ALLOC(sizeof(*res), sizeof(double));
    if (!res) { st = ALWAN_E_NOMEM; goto done; }
    res->count = n_alive;
    res->start = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(n_alive + 1, sizeof(size_t)), sizeof(size_t));
    res->points = (double *)ALWAN_ALLOC(alwan_safe_array_size(n_pts ? 2 * n_pts : 2, sizeof(double)), sizeof(double));
    if (!res->start || !res->points) { alwan_contours_destroy(res, NULL); st = ALWAN_E_NOMEM; goto done; }
    {
        size_t j = 0, q = 0;
        res->start[0] = 0;
        for (i = 0; i < s.nc; i++) {
            size_t k, b;
            if (!s.ct[i].alive) continue;
            b = q;
            for (k = s.ct[i].first; k != CT_NONE; k = s.nodes[k].next) {
                res->points[2 * q] = s.nodes[k].r;
                res->points[2 * q + 1] = s.nodes[k].c;
                q++;
            }
            if (p.positive_orientation_high) {
                size_t x = b, y = q - 1;
                while (x < y) {
                    double t0 = res->points[2 * x], t1 = res->points[2 * x + 1];
                    res->points[2 * x] = res->points[2 * y]; res->points[2 * x + 1] = res->points[2 * y + 1];
                    res->points[2 * y] = t0; res->points[2 * y + 1] = t1;
                    x++; y--;
                }
            }
            res->start[++j] = q;
        }
    }
    *out = res;

done:
    ct_map_free(&s.starts);
    ct_map_free(&s.ends);
    ALWAN_FREE(s.nodes);
    ALWAN_FREE(s.ct);
    return st;
}

static alwan_status ct_check(alwan_contours **out, void const *src, size_t row_stride, size_t elem,
                             size_t width, size_t height, alwan_contour_params const *params) {
    if (!out) return ALWAN_E_INVALID;
    *out = NULL;
    if (!src || width < 2 || height < 2) return ALWAN_E_INVALID;
    if (row_stride < width * elem) return ALWAN_E_INVALID;
    if (params && params->mask && params->mask_row_stride < width) return ALWAN_E_INVALID;
    return ALWAN_OK;
}

#define CT_ENTRY(NAME, T)                                                                          \
alwan_status NAME(alwan_contours **out, T const *src, size_t row_stride, size_t width,             \
                  size_t height, alwan_contour_params const *params, alwan_ctx *ctx) {             \
    double *a;                                                                                     \
    size_t r, c;                                                                                   \
    alwan_status st = ct_check(out, src, row_stride, sizeof(T), width, height, params);            \
    (void)ctx;                                                                                     \
    if (st != ALWAN_OK) return st;                                                                 \
    a = (double *)ALWAN_ALLOC(alwan_safe_array_size(width * height, sizeof(double)), sizeof(double)); \
    if (!a) return ALWAN_E_NOMEM;                                                                  \
    for (r = 0; r < height; r++) {                                                                 \
        T const *row = (T const *)((unsigned char const *)src + r * row_stride);                   \
        for (c = 0; c < width; c++) a[r * width + c] = (double)row[c];                             \
    }                                                                                              \
    st = ct_find(out, a, width, height, params);                                                   \
    ALWAN_FREE(a);                                                                                 \
    return st;                                                                                     \
}

CT_ENTRY(alwan_find_contours_f32, alwan_f32)
CT_ENTRY(alwan_find_contours_f64, alwan_f64)
CT_ENTRY(alwan_find_contours_u8, unsigned char)

size_t alwan_contours_count(alwan_contours const *contours) {
    return contours ? contours->count : 0;
}

alwan_status alwan_contours_get(double const **points, size_t *count, alwan_contours const *contours, size_t index) {
    if (!points || !count || !contours) return ALWAN_E_INVALID;
    if (index >= contours->count) return ALWAN_E_RANGE;
    *points = contours->points + 2 * contours->start[index];
    *count = contours->start[index + 1] - contours->start[index];
    return ALWAN_OK;
}

void alwan_contours_destroy(alwan_contours *contours, alwan_ctx *ctx) {
    (void)ctx;
    if (!contours) return;
    ALWAN_FREE(contours->start);
    ALWAN_FREE(contours->points);
    ALWAN_FREE(contours);
}

/* approximate_polygon: Douglas-Peucker as scikit-image writes it, a stack of spans. For each
 * span the in-between points are measured against the line through its ends: the
 * perpendicular distance where a point projects inside the span, else the distance to the
 * nearer end. The farthest point beyond tolerance splits the span, the left half first. */
alwan_status alwan_approximate_polygon(double *out, size_t *out_count, double const *points, size_t count,
                                       double tolerance) {
    unsigned char *chain;
    double *dists;
    size_t *stack, sp = 0, i, k;
    if (!out || !out_count || (!points && count)) return ALWAN_E_INVALID;
    if (count == 0) { *out_count = 0; return ALWAN_OK; }
    if (!(tolerance > 0.0)) {
        if (out != points) memmove(out, points, 2 * count * sizeof(double));
        *out_count = count;
        return ALWAN_OK;
    }
    chain = (unsigned char *)ALWAN_ALLOC(count, sizeof(double));
    dists = (double *)ALWAN_ALLOC(alwan_safe_array_size(count, sizeof(double)), sizeof(double));
    stack = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(2 * count + 2, sizeof(size_t)), sizeof(size_t));
    if (!chain || !dists || !stack) {
        ALWAN_FREE(chain); ALWAN_FREE(dists); ALWAN_FREE(stack);
        return ALWAN_E_NOMEM;
    }
    memset(chain, 0, count);
    for (i = 0; i < count; i++) dists[i] = 0.0;
    chain[0] = 1;
    chain[count - 1] = 1;
    stack[sp++] = 0; stack[sp++] = count - 1;
    while (sp) {
        size_t const end = stack[--sp], start = stack[--sp];
        double const r0 = points[2 * start], c0 = points[2 * start + 1];
        double const r1 = points[2 * end], c1 = points[2 * end + 1];
        double const dr = r1 - r0, dc = c1 - c0;
        double const ang = -ALWAN_ATAN2_F64(dr, dc);
        double const ca = ALWAN_COS_F64(ang), sa = ALWAN_SIN_F64(ang);
        double const sdist = c0 * sa + r0 * ca;
        size_t best = CT_NONE;
        int any = 0;
        for (k = start + 1; k < end; k++) {
            double const r = points[2 * k], c = points[2 * k + 1];
            double const dr0 = r - r0, dc0 = c - c0, dr1 = r - r1, dc1 = c - c1;
            double const pl0 = dr0 * dr + dc0 * dc;
            double const pl1 = -dr1 * dr - dc1 * dc;
            double d;
            if (pl0 > 0 && pl1 > 0) {
                d = ALWAN_ABS_F64(r * ca + c * sa - sdist);
            } else {
                double const e0 = ALWAN_SQRT_F64(dc0 * dc0 + dr0 * dr0), e1 = ALWAN_SQRT_F64(dc1 * dc1 + dr1 * dr1);
                d = e0 < e1 ? e0 : e1;       /* np.minimum; NaN cannot occur on finite input */
            }
            dists[k] = d;
            if (d > tolerance) any = 1;
            if (best == CT_NONE || d > dists[best]) best = k;   /* np.argmax: the first maximum */
        }
        if (any) {
            stack[sp++] = best; stack[sp++] = end;
            stack[sp++] = start; stack[sp++] = best;
            chain[best] = 1;
        }
    }
    for (i = 0, k = 0; i < count; i++) {
        if (!chain[i]) continue;
        out[2 * k] = points[2 * i];
        out[2 * k + 1] = points[2 * i + 1];
        k++;
    }
    *out_count = k;
    ALWAN_FREE(chain); ALWAN_FREE(dists); ALWAN_FREE(stack);
    return ALWAN_OK;
}
