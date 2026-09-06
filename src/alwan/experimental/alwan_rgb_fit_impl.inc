/* alwan_rgb_fit_impl.inc: EXPERIMENTAL fit of an RGB encoding space to a dataset.
 *
 * Given colours in some RGB space, find primaries, a white point and a transfer function
 * (a free power, or the sRGB curve because its decode is free on a GPU) so that the dataset
 * survives quantisation to N bits with the least perceptual damage.
 *
 * Under experimental/ the usual alwan guarantees are RELAXED: inlined constants, no gendata,
 * not part of the constraint-tested surface. Compiled once per precision through the
 * ALWAN_CORE_* macros; every static is suffixed with ALWAN_CORE_FN so the two passes coexist
 * in one translation unit.
 *
 * The metric decides what "error" means: Oklab, ITP, dE2000, DISPLAY (the sRGB code values an
 * ordinary screen shows: Rec.709, clamped, sRGB-encoded), or LINEAR (the stored values, for
 * data textures whose channels are numbers rather than an appearance).
 *
 * The objective the solver descends is smooth: the expected quantisation error at a sample is
 * the code step times the norm of the Jacobian from encoded code to perceptual coordinates,
 * which is analytic for both transfer functions, plus what going out of gamut costs.
 *
 * That cost has two policies. FORBID charges the overshoot itself, per sample and once more on
 * the single worst, which drives the answer to a gamut that holds everything. PRICED charges
 * the damage instead: the metric distance between the colour and what it becomes once clamped,
 * in the same units as the quantisation error, so the two are directly comparable and the
 * solver finds the trade on its own. Under PRICED a gamut that clips the rare colours at the
 * border to buy precision for the frequent ones in the middle wins whenever that arithmetic
 * works out, which on a peaked distribution it usually does.
 * The scale is the linear value that encodes to code 1.0: the white's own luminance in the
 * usual Y = 1 convention, and free here because a dataset that never reaches the white, or
 * one whose bright colours sit off the white, cannot use the top codes without it.
 * The report always carries the TRUE quantised round trip, computed on the best candidate.
 * The search is Nelder-Mead with a fixed initial simplex: deterministic, resumable, and the
 * best vertex never gets worse, which is what lets a fit be continued or restarted.
 */

#define RF_NP 10                /* rx ry gx gy bx by wx wy log(gamma) log(scale) */
#define RF_INF ALWAN_CORE_LITERAL(1e30)      /* fits float too */

typedef struct ALWAN_CORE_FN(alwan_rgb_fit_state) {
    alwan_ctx *ctx;
    ALWAN_CORE_FN(alwan_rgb_fit_params) prm;
    size_t n;
    ALWAN_CORE_T *xyz;              /* 3n, dataset in XYZ */
    ALWAN_CORE_T *pc;               /* 3n, perceptual coordinates of the originals */
    ALWAN_CORE_T *jp;               /* 9n, d(perceptual)/d(XYZ) at each sample, row major */
    ALWAN_CORE_T *w;                /* n, weights summing to 1 */
    ALWAN_CORE_T *scratch;          /* n, per-sample errors for the percentile */
    int tf_kind;                    /* POWER or SRGB for this state */
    ALWAN_CORE_T disp[9];           /* XYZ -> Rec.709, for the DISPLAY metric */
    ALWAN_CORE_T data_prim[8];      /* the dataset's own primaries and white, a cold-start candidate */
    int have_data_prim;
    int free_idx[RF_NP]; int k;     /* which of the 10 move */
    ALWAN_CORE_T theta[RF_NP];      /* current best */
    ALWAN_CORE_T fbest;
    ALWAN_CORE_T simplex[RF_NP + 1][RF_NP];
    ALWAN_CORE_T fval[RF_NP + 1];
    int have_simplex;
    int iterations;
    int converged;
} ALWAN_CORE_FN(alwan_rgb_fit_state);

/* ---------------------------------------------------------------- */
/* small helpers                                                      */
/* ---------------------------------------------------------------- */

static ALWAN_CORE_T ALWAN_CORE_FN(rf_clamp01)(ALWAN_CORE_T v) {
    return v < ALWAN_CORE_ZERO ? ALWAN_CORE_ZERO : (v > ALWAN_CORE_ONE ? ALWAN_CORE_ONE : v);
}

/* Encoded value and d(linear)/d(encoded) for the two transfer functions. */
static ALWAN_CORE_T ALWAN_CORE_FN(rf_encode)(ALWAN_CORE_T lin, int kind, ALWAN_CORE_T gamma) {
    if (kind == ALWAN_FIT_TF_SRGB) {
        if (lin <= ALWAN_CORE_LITERAL(0.0031308)) return lin * ALWAN_CORE_LITERAL(12.92);
        return ALWAN_CORE_LITERAL(1.055) * ALWAN_CORE_POW(lin, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(2.4)) - ALWAN_CORE_LITERAL(0.055);
    }
    return ALWAN_CORE_POW(lin, ALWAN_CORE_ONE / gamma);
}
static ALWAN_CORE_T ALWAN_CORE_FN(rf_decode)(ALWAN_CORE_T enc, int kind, ALWAN_CORE_T gamma) {
    if (kind == ALWAN_FIT_TF_SRGB) {
        if (enc <= ALWAN_CORE_LITERAL(0.04045)) return enc / ALWAN_CORE_LITERAL(12.92);
        return ALWAN_CORE_POW((enc + ALWAN_CORE_LITERAL(0.055)) / ALWAN_CORE_LITERAL(1.055), ALWAN_CORE_LITERAL(2.4));
    }
    return ALWAN_CORE_POW(enc, gamma);
}
static ALWAN_CORE_T ALWAN_CORE_FN(rf_dlin_denc)(ALWAN_CORE_T enc, int kind, ALWAN_CORE_T gamma) {
    if (kind == ALWAN_FIT_TF_SRGB) {
        if (enc <= ALWAN_CORE_LITERAL(0.04045)) return ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(12.92);
        return ALWAN_CORE_LITERAL(2.4) / ALWAN_CORE_LITERAL(1.055)
             * ALWAN_CORE_POW((enc + ALWAN_CORE_LITERAL(0.055)) / ALWAN_CORE_LITERAL(1.055), ALWAN_CORE_LITERAL(1.4));
    }
    if (enc <= ALWAN_CORE_ZERO) return ALWAN_CORE_ZERO;
    return gamma * ALWAN_CORE_POW(enc, gamma - ALWAN_CORE_ONE);
}

/* Perceptual coordinates of an XYZ (dataset scale: white Y = 1). disp is XYZ -> Rec.709,
 * used by the DISPLAY metric only. */
static void ALWAN_CORE_FN(rf_pcoord)(ALWAN_CORE_T const xyz[3], int metric, ALWAN_CORE_T const *disp, ALWAN_CORE_T out[3]) {
    ALWAN_CORE_FN(alwan_xyz) X; X.x = xyz[0]; X.y = xyz[1]; X.z = xyz[2];
    if (metric == ALWAN_FIT_METRIC_LINEAR) {
        /* the stored values themselves: for data textures (a normal map, a roughness map) the
         * quantity that must survive is the number, not an appearance of it */
        for (int r = 0; r < 3; r++) out[r] = disp[r*3]*xyz[0] + disp[r*3+1]*xyz[1] + disp[r*3+2]*xyz[2];
        return;
    }
    if (metric == ALWAN_FIT_METRIC_DISPLAY) {
        /* what an sRGB screen actually shows: Rec.709 linear, clamped to the display's own
         * range, then the sRGB OETF. Colours the screen cannot show clamp for the original and
         * for the round trip alike, so precision spent outside Rec.709 counts for nothing. */
        for (int r = 0; r < 3; r++) {
            ALWAN_CORE_T v = disp[r*3]*xyz[0] + disp[r*3+1]*xyz[1] + disp[r*3+2]*xyz[2];
            out[r] = ALWAN_CORE_FN(rf_encode)(ALWAN_CORE_FN(rf_clamp01)(v), ALWAN_FIT_TF_SRGB, ALWAN_CORE_LITERAL(2.2));
        }
        return;
    }
    if (metric == ALWAN_FIT_METRIC_ITP) {
        ALWAN_CORE_FN(alwan_ictcp) c; ALWAN_CORE_FN(alwan_xyz) A = X;
        A.x *= ALWAN_CORE_LITERAL(100.0); A.y *= ALWAN_CORE_LITERAL(100.0); A.z *= ALWAN_CORE_LITERAL(100.0);   /* Y = 1 is 100 cd/m2 */
        ALWAN_CORE_FN(alwan_xyz_to_ictcp)(&c, &A, 1);
        out[0] = c.I; out[1] = c.Ct; out[2] = c.Cp; return;
    }
    if (metric == ALWAN_FIT_METRIC_DE2000) {
        ALWAN_CORE_FN(alwan_lab) l; ALWAN_CORE_FN(alwan_xyz) w;
        w.x = ALWAN_CORE_LITERAL(0.95047); w.y = ALWAN_CORE_ONE; w.z = ALWAN_CORE_LITERAL(1.08883);
        ALWAN_CORE_FN(alwan_xyz_to_lab)(&l, &X, &w);
        out[0] = l.L; out[1] = l.a; out[2] = l.b; return;
    }
    { ALWAN_CORE_FN(alwan_oklab) o; ALWAN_CORE_FN(alwan_xyz_to_oklab)(&o, &X);
      out[0] = o.L; out[1] = o.a; out[2] = o.b; }
}

/* Distance between two perceptual coordinate triples, in the metric's own units. */
static ALWAN_CORE_T ALWAN_CORE_FN(rf_pdist)(ALWAN_CORE_T const a[3], ALWAN_CORE_T const b[3], int metric) {
    if (metric == ALWAN_FIT_METRIC_ITP) {
        ALWAN_CORE_FN(alwan_ictcp) p, q; ALWAN_CORE_FN(alwan_delta_e_itp_params) pr;
        p.I = a[0]; p.Ct = a[1]; p.Cp = a[2]; q.I = b[0]; q.Ct = b[1]; q.Cp = b[2];
        pr.scalar_factor = ALWAN_CORE_LITERAL(720.0);
        return ALWAN_CORE_FN(alwan_delta_e_itp)(&p, &q, &pr);
    }
    if (metric == ALWAN_FIT_METRIC_DE2000) {
        ALWAN_CORE_FN(alwan_lab) p, q;
        p.L = a[0]; p.a = a[1]; p.b = a[2]; q.L = b[0]; q.a = b[1]; q.b = b[2];
        return ALWAN_CORE_FN(alwan_delta_e_2000)(&p, &q);
    }
    { ALWAN_CORE_T d0 = a[0]-b[0], d1 = a[1]-b[1], d2 = a[2]-b[2];
      return ALWAN_CORE_SQRT(d0*d0 + d1*d1 + d2*d2); }
}

/* The scale of one unit of the surrogate Jacobian: for ITP the metric multiplies the
 * coordinate distance by 720, for Oklab it is the coordinate distance, for DE2000 the
 * Lab distance is the smooth stand-in. */
static ALWAN_CORE_T ALWAN_CORE_FN(rf_metric_gain)(int metric) {
    return metric == ALWAN_FIT_METRIC_ITP ? ALWAN_CORE_LITERAL(720.0) : ALWAN_CORE_ONE;
}

/* Descriptor and matrices from a parameter vector. 0 when infeasible. */
static int ALWAN_CORE_FN(rf_desc_from_theta)(ALWAN_CORE_T const th[RF_NP], ALWAN_CORE_FN(alwan_rgb_space_desc) *d) {
    memset(d, 0, sizeof *d);
    for (int i = 0; i < 6; i++) d->primaries_xy[i] = th[i];
    d->white_xy[0] = th[6]; d->white_xy[1] = th[7];
    d->oetf = ALWAN_TF_LINEAR; d->eotf = ALWAN_TF_LINEAR;
    /* white strictly inside the triangle, triangle not degenerate */
    {
        ALWAN_CORE_T ax = th[0], ay = th[1], bx = th[2], by = th[3], cx = th[4], cy = th[5], px = th[6], py = th[7];
        ALWAN_CORE_T s1 = (bx-ax)*(py-ay) - (by-ay)*(px-ax);
        ALWAN_CORE_T s2 = (cx-bx)*(py-by) - (cy-by)*(px-bx);
        ALWAN_CORE_T s3 = (ax-cx)*(py-cy) - (ay-cy)*(px-cx);
        ALWAN_CORE_T area2 = (bx-ax)*(cy-ay) - (by-ay)*(cx-ax);
        int inside = (s1 > ALWAN_CORE_ZERO && s2 > ALWAN_CORE_ZERO && s3 > ALWAN_CORE_ZERO)
                  || (s1 < ALWAN_CORE_ZERO && s2 < ALWAN_CORE_ZERO && s3 < ALWAN_CORE_ZERO);
        if (!inside) return 0;
        if (ALWAN_CORE_ABS(area2) < ALWAN_CORE_LITERAL(1e-6)) return 0;   /* sides of a thousandth in xy */
        if (py <= ALWAN_CORE_LITERAL(1e-6) || ay <= ALWAN_CORE_LITERAL(1e-6) || by <= ALWAN_CORE_LITERAL(1e-6) || cy <= ALWAN_CORE_LITERAL(1e-6)) return 0;
    }
    if (ALWAN_CORE_FN(alwan_rgb_derive_matrices)(&d->rgb_to_xyz, &d->xyz_to_rgb, d) != ALWAN_OK) return 0;
    d->has_matrices = 1;
    return 1;
}

static ALWAN_CORE_T ALWAN_CORE_FN(rf_gamma_of)(ALWAN_CORE_FN(alwan_rgb_fit_state) const *s, ALWAN_CORE_T const th[RF_NP]) {
    return s->tf_kind == ALWAN_FIT_TF_SRGB ? ALWAN_CORE_LITERAL(2.2) : ALWAN_CORE_EXP(th[8]);
}
static ALWAN_CORE_T ALWAN_CORE_FN(rf_scale_of)(ALWAN_CORE_T const th[RF_NP]) { return ALWAN_CORE_EXP(th[9]); }
#define RF_LOG_SCALE_LIMIT ALWAN_CORE_LITERAL(20.0)   /* e^20 either way is far past any dataset */

static int ALWAN_CORE_FN(rf_cmp)(void const *a, void const *b) {
    ALWAN_CORE_T x = *(ALWAN_CORE_T const *)a, y = *(ALWAN_CORE_T const *)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

/* Weighted mean and percentile of the per-sample errors held in s->scratch. */
static void ALWAN_CORE_FN(rf_stats)(ALWAN_CORE_FN(alwan_rgb_fit_state) *s, ALWAN_CORE_T *mean, ALWAN_CORE_T *tail, ALWAN_CORE_T *mx) {
    ALWAN_CORE_T m = ALWAN_CORE_ZERO, big = ALWAN_CORE_ZERO;
    for (size_t i = 0; i < s->n; i++) { m += s->w[i] * s->scratch[i]; if (s->scratch[i] > big) big = s->scratch[i]; }
    *mean = m; *mx = big;
    if (s->prm.percentile <= ALWAN_CORE_ZERO) { *tail = ALWAN_CORE_ZERO; return; }
    /* Uniform-weight percentile by sorting; weights only shape the mean. That keeps the
     * tail a property of the worst samples rather than of how they were weighted. */
    qsort(s->scratch, s->n, sizeof(ALWAN_CORE_T), ALWAN_CORE_FN(rf_cmp));
    { size_t idx = (size_t)((ALWAN_CORE_T)(s->n - 1) * s->prm.percentile + ALWAN_CORE_LITERAL(0.5));
      if (idx >= s->n) idx = s->n - 1; *tail = s->scratch[idx]; }
}

/* ---------------------------------------------------------------- */
/* the smooth objective                                               */
/* ---------------------------------------------------------------- */

static ALWAN_CORE_T ALWAN_CORE_FN(rf_objective)(ALWAN_CORE_FN(alwan_rgb_fit_state) *s, ALWAN_CORE_T const th[RF_NP]) {
    ALWAN_CORE_FN(alwan_rgb_space_desc) d;
    ALWAN_CORE_T gamma = ALWAN_CORE_FN(rf_gamma_of)(s, th);
    if (gamma < s->prm.gamma_min || gamma > s->prm.gamma_max) return RF_INF;
    if (ALWAN_CORE_ABS(th[9]) > RF_LOG_SCALE_LIMIT) return RF_INF;
    if (!ALWAN_CORE_FN(rf_desc_from_theta)(th, &d)) return RF_INF;
    ALWAN_CORE_T const *A = d.xyz_to_rgb.m, *B = d.rgb_to_xyz.m;
    ALWAN_CORE_T const scale = ALWAN_CORE_FN(rf_scale_of)(th), inv_scale = ALWAN_CORE_ONE / scale;
    ALWAN_CORE_T const step = ALWAN_CORE_ONE / (ALWAN_CORE_T)((1 << s->prm.bits) - 1);
    ALWAN_CORE_T const noise = step * step / ALWAN_CORE_LITERAL(12.0);
    ALWAN_CORE_T const gain = ALWAN_CORE_FN(rf_metric_gain)(s->prm.metric);
    int const priced = (s->prm.clip_policy == ALWAN_FIT_CLIP_PRICED);
    ALWAN_CORE_T worst_clip = ALWAN_CORE_ZERO;
    for (size_t i = 0; i < s->n; i++) {
        ALWAN_CORE_T const *X = s->xyz + i * 3, *J = s->jp + i * 9;
        ALWAN_CORE_T lin[3], dl[3], clip = ALWAN_CORE_ZERO;
        for (int r = 0; r < 3; r++) {
            /* channel value normalised so that the scale is 1: the hinge and the code step live there */
            lin[r] = (A[r*3]*X[0] + A[r*3+1]*X[1] + A[r*3+2]*X[2]) * inv_scale;
            if (lin[r] < ALWAN_CORE_ZERO) { clip -= lin[r]; lin[r] = ALWAN_CORE_ZERO; }
            else if (lin[r] > ALWAN_CORE_ONE) { clip += lin[r] - ALWAN_CORE_ONE; lin[r] = ALWAN_CORE_ONE; }
            dl[r] = scale * ALWAN_CORE_FN(rf_dlin_denc)(ALWAN_CORE_FN(rf_encode)(lin[r], s->tf_kind, gamma), s->tf_kind, gamma);
        }
        /* JF = J (3x3, dP/dXYZ) . B (3x3, RGB->XYZ) . diag(dl); squared Frobenius norm */
        ALWAN_CORE_T fro = ALWAN_CORE_ZERO;
        for (int c = 0; c < 3; c++) {
            ALWAN_CORE_T bx = B[c] * dl[c], by = B[3+c] * dl[c], bz = B[6+c] * dl[c];   /* column c of B.diag(dl) */
            for (int r = 0; r < 3; r++) {
                ALWAN_CORE_T v = J[r*3]*bx + J[r*3+1]*by + J[r*3+2]*bz;
                fro += v * v;
            }
        }
        ALWAN_CORE_T cost = clip;
        if (priced && clip > ALWAN_CORE_ZERO) {
            /* the part that quantisation cannot explain and no bit depth can fix: where this
             * colour lands once the encoder has clamped it into the gamut */
            ALWAN_CORE_T xyzc[3], pclip[3];
            for (int r = 0; r < 3; r++) xyzc[r] = (B[r*3]*lin[0] + B[r*3+1]*lin[1] + B[r*3+2]*lin[2]) * scale;
            ALWAN_CORE_FN(rf_pcoord)(xyzc, s->prm.metric, s->disp, pclip);
            cost = ALWAN_CORE_FN(rf_pdist)(s->pc + i * 3, pclip, s->prm.metric);
        }
        s->scratch[i] = gain * ALWAN_CORE_SQRT(noise * fro) + s->prm.clip_weight * cost;
        if (clip > worst_clip) worst_clip = clip;
    }
    /* Under FORBID the mean and the percentile are joined by the single worst overshoot,
     * because the percentile alone lets a fraction smaller than 1 - percentile clip for free.
     * Under PRICED the clip damage already rides in every sample's own cost, so the mean and
     * the percentile weigh it exactly as often as it happens: that is the point. */
    { ALWAN_CORE_T mean, tail, mx; ALWAN_CORE_FN(rf_stats)(s, &mean, &tail, &mx);
      return mean + tail + (priced ? ALWAN_CORE_ZERO : s->prm.clip_weight * worst_clip); }
}

/* ---------------------------------------------------------------- */
/* the true quantised round trip                                      */
/* ---------------------------------------------------------------- */

static alwan_status ALWAN_CORE_FN(rf_true_report)(ALWAN_CORE_FN(alwan_rgb_fit_state) *s, ALWAN_CORE_T const th[RF_NP],
                                                  ALWAN_CORE_FN(alwan_rgb_fit_report) *rep) {
    ALWAN_CORE_FN(alwan_rgb_space_desc) d;
    ALWAN_CORE_T gamma = ALWAN_CORE_FN(rf_gamma_of)(s, th);
    if (ALWAN_CORE_ABS(th[9]) > RF_LOG_SCALE_LIMIT) return ALWAN_E_RANGE;
    if (!ALWAN_CORE_FN(rf_desc_from_theta)(th, &d)) return ALWAN_E_RANGE;
    ALWAN_CORE_T const *A = d.xyz_to_rgb.m, *B = d.rgb_to_xyz.m;
    ALWAN_CORE_T const scale = ALWAN_CORE_FN(rf_scale_of)(th), inv_scale = ALWAN_CORE_ONE / scale;
    ALWAN_CORE_T const maxv = (ALWAN_CORE_T)((1 << s->prm.bits) - 1);
    ALWAN_CORE_T clipped = ALWAN_CORE_ZERO;
    for (size_t i = 0; i < s->n; i++) {
        ALWAN_CORE_T const *X = s->xyz + i * 3;
        ALWAN_CORE_T lin[3], back[3], xyz2[3], p2[3]; int any = 0;
        for (int r = 0; r < 3; r++) {
            lin[r] = (A[r*3]*X[0] + A[r*3+1]*X[1] + A[r*3+2]*X[2]) * inv_scale;
            if (lin[r] < -ALWAN_CORE_LITERAL(1e-9) || lin[r] > ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(1e-9)) any = 1;
            ALWAN_CORE_T enc = ALWAN_CORE_FN(rf_clamp01)(ALWAN_CORE_FN(rf_encode)(ALWAN_CORE_FN(rf_clamp01)(lin[r]), s->tf_kind, gamma));
            ALWAN_CORE_T q = ALWAN_CORE_FLOOR(enc * maxv + ALWAN_CORE_LITERAL(0.5)) / maxv;   /* the (2^N - 1) quantiser */
            back[r] = scale * ALWAN_CORE_FN(rf_decode)(q, s->tf_kind, gamma);
        }
        if (any) clipped += s->w[i];
        for (int r = 0; r < 3; r++) xyz2[r] = B[r*3]*back[0] + B[r*3+1]*back[1] + B[r*3+2]*back[2];
        ALWAN_CORE_FN(rf_pcoord)(xyz2, s->prm.metric, s->disp, p2);
        s->scratch[i] = ALWAN_CORE_FN(rf_pdist)(s->pc + i * 3, p2, s->prm.metric);
    }
    ALWAN_CORE_FN(rf_stats)(s, &rep->mean_error, &rep->tail_error, &rep->max_error);
    rep->clipped_fraction = clipped;
    return ALWAN_OK;
}

/* ---------------------------------------------------------------- */
/* dataset intake and the analytic start                              */
/* ---------------------------------------------------------------- */

static void ALWAN_CORE_FN(rf_free_state)(ALWAN_CORE_FN(alwan_rgb_fit_state) *s) {
    if (!s) return;
    alwan_ctx *c = s->ctx;
    if (s->xyz) c->free_fn(s->xyz); if (s->pc) c->free_fn(s->pc); if (s->jp) c->free_fn(s->jp);
    if (s->w) c->free_fn(s->w); if (s->scratch) c->free_fn(s->scratch);
    c->free_fn(s);
}

/* Convex hull of xy points (Andrew monotone chain) into hull[], returns count. Works in place on a copy. */
static int ALWAN_CORE_FN(rf_cross)(ALWAN_CORE_T const *o, ALWAN_CORE_T const *a, ALWAN_CORE_T const *b) {
    ALWAN_CORE_T v = (a[0]-o[0])*(b[1]-o[1]) - (a[1]-o[1])*(b[0]-o[0]);
    return v > ALWAN_CORE_ZERO ? 1 : (v < ALWAN_CORE_ZERO ? -1 : 0);
}
static int ALWAN_CORE_FN(rf_cmp_xy)(void const *a, void const *b) {
    ALWAN_CORE_T const *p = (ALWAN_CORE_T const *)a, *q = (ALWAN_CORE_T const *)b;
    if (p[0] < q[0]) return -1; if (p[0] > q[0]) return 1;
    return p[1] < q[1] ? -1 : (p[1] > q[1] ? 1 : 0);
}
static size_t ALWAN_CORE_FN(rf_hull)(ALWAN_CORE_T *pts, size_t n, ALWAN_CORE_T *hull) {
    qsort(pts, n, 2 * sizeof(ALWAN_CORE_T), ALWAN_CORE_FN(rf_cmp_xy));
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        while (k >= 2 && ALWAN_CORE_FN(rf_cross)(hull + 2*(k-2), hull + 2*(k-1), pts + 2*i) <= 0) k--;
        hull[2*k] = pts[2*i]; hull[2*k+1] = pts[2*i+1]; k++;
    }
    for (size_t i = n - 1, t = k + 1; i > 0; i--) {
        while (k >= t && ALWAN_CORE_FN(rf_cross)(hull + 2*(k-2), hull + 2*(k-1), pts + 2*(i-1)) <= 0) k--;
        hull[2*k] = pts[2*(i-1)]; hull[2*k+1] = pts[2*(i-1)+1]; k++;
    }
    return k > 1 ? k - 1 : k;
}

/* Smallest enclosing triangle over a family of orientations: three support lines 120 degrees
 * apart, swept through 60 angles. Not the true minimum-area triangle, but within a few
 * percent of it on real hulls, and the optimiser moves the corners from here anyway. */
static void ALWAN_CORE_FN(rf_enclosing_triangle)(ALWAN_CORE_T const *hull, size_t h, ALWAN_CORE_T margin, ALWAN_CORE_T tri[6]) {
    ALWAN_CORE_T best_area = RF_INF; int found = 0;
    for (int a = 0; a < 60; a++) {
        ALWAN_CORE_T ang = (ALWAN_CORE_T)a * (ALWAN_CORE_PI * ALWAN_CORE_LITERAL(2.0) / ALWAN_CORE_LITERAL(180.0));   /* 0..120 deg */
        ALWAN_CORE_T nx[3], ny[3], sv[3];
        for (int k = 0; k < 3; k++) {
            ALWAN_CORE_T t = ang + (ALWAN_CORE_T)k * (ALWAN_CORE_PI * ALWAN_CORE_LITERAL(2.0) / ALWAN_CORE_LITERAL(3.0));
            nx[k] = ALWAN_CORE_COS(t); ny[k] = ALWAN_CORE_SIN(t);
            ALWAN_CORE_T s = -RF_INF;
            for (size_t i = 0; i < h; i++) { ALWAN_CORE_T v = nx[k]*hull[2*i] + ny[k]*hull[2*i+1]; if (v > s) s = v; }
            sv[k] = s + margin;
        }
        /* corners: intersection of lines k and k+1 */
        ALWAN_CORE_T v[6]; int ok = 1;
        for (int k = 0; k < 3 && ok; k++) {
            int j = (k + 1) % 3;
            ALWAN_CORE_T det = nx[k]*ny[j] - ny[k]*nx[j];
            if (ALWAN_CORE_ABS(det) < ALWAN_CORE_LITERAL(1e-9)) { ok = 0; break; }
            v[2*k]   = (sv[k]*ny[j] - ny[k]*sv[j]) / det;
            v[2*k+1] = (nx[k]*sv[j] - sv[k]*nx[j]) / det;
        }
        if (!ok) continue;
        ALWAN_CORE_T area = ALWAN_CORE_ABS((v[2]-v[0])*(v[5]-v[1]) - (v[3]-v[1])*(v[4]-v[0])) * ALWAN_CORE_LITERAL(0.5);
        if (area < best_area) { best_area = area; memcpy(tri, v, sizeof v); found = 1; }
    }
    if (!found) { /* fall back to the sRGB triangle */
        ALWAN_CORE_T const srgb[6] = { ALWAN_CORE_LITERAL(0.64), ALWAN_CORE_LITERAL(0.33), ALWAN_CORE_LITERAL(0.30), ALWAN_CORE_LITERAL(0.60), ALWAN_CORE_LITERAL(0.15), ALWAN_CORE_LITERAL(0.06) };
        memcpy(tri, srgb, sizeof srgb);
    }
    /* name the corners: red is the largest x, green the largest y, blue the rest */
    {
        int ir = 0, ig = 0;
        for (int k = 1; k < 3; k++) { if (tri[2*k] > tri[2*ir]) ir = k; }
        for (int k = 0; k < 3; k++) { if (k != ir && (ig == ir || tri[2*k+1] > tri[2*ig+1])) ig = k; }
        if (ig == ir) ig = (ir + 1) % 3;
        int ib = 3 - ir - ig;
        ALWAN_CORE_T o[6] = { tri[2*ir], tri[2*ir+1], tri[2*ig], tri[2*ig+1], tri[2*ib], tri[2*ib+1] };
        memcpy(tri, o, sizeof o);
    }
}

/* Largest channel value the dataset takes in the triangle th[0..7]; 0 when the triangle is
 * infeasible or the dataset is black. */
static ALWAN_CORE_T ALWAN_CORE_FN(rf_data_peak)(ALWAN_CORE_FN(alwan_rgb_fit_state) const *s, ALWAN_CORE_T const th[RF_NP]) {
    ALWAN_CORE_FN(alwan_rgb_space_desc) d;
    if (!ALWAN_CORE_FN(rf_desc_from_theta)(th, &d)) return ALWAN_CORE_ZERO;
    ALWAN_CORE_T const *A = d.xyz_to_rgb.m; ALWAN_CORE_T big = ALWAN_CORE_ZERO;
    for (size_t i = 0; i < s->n; i++) {
        ALWAN_CORE_T const *X = s->xyz + i * 3;
        for (int r = 0; r < 3; r++) { ALWAN_CORE_T v = A[r*3]*X[0] + A[r*3+1]*X[1] + A[r*3+2]*X[2]; if (v > big) big = v; }
    }
    return big;
}

/* The exponent by a bracketed search on the objective with everything else held: th[8] in place. */
static void ALWAN_CORE_FN(rf_search_gamma)(ALWAN_CORE_FN(alwan_rgb_fit_state) *s, ALWAN_CORE_T th[RF_NP]) {
    ALWAN_CORE_T lo = ALWAN_CORE_LN(s->prm.gamma_min), hi = ALWAN_CORE_LN(s->prm.gamma_max);
    ALWAN_CORE_T const gr = ALWAN_CORE_LITERAL(0.6180339887);
    ALWAN_CORE_T a = hi - gr * (hi - lo), b = lo + gr * (hi - lo);
    th[8] = a; ALWAN_CORE_T fa = ALWAN_CORE_FN(rf_objective)(s, th);
    th[8] = b; ALWAN_CORE_T fb = ALWAN_CORE_FN(rf_objective)(s, th);
    for (int it = 0; it < 30; it++) {
        if (fa < fb) { hi = b; b = a; fb = fa; a = hi - gr * (hi - lo); th[8] = a; fa = ALWAN_CORE_FN(rf_objective)(s, th); }
        else         { lo = a; a = b; fa = fb; b = lo + gr * (hi - lo); th[8] = b; fb = ALWAN_CORE_FN(rf_objective)(s, th); }
    }
    th[8] = fa < fb ? a : b;
}

/* Finish a cold-start candidate whose chromaticities are in th[0..7]: scale from the data's
 * peak in that triangle, exponent 2.2 or searched when it is free. Returns the objective
 * there, RF_INF when the candidate is unusable. */
static ALWAN_CORE_T ALWAN_CORE_FN(rf_complete_start)(ALWAN_CORE_FN(alwan_rgb_fit_state) *s, ALWAN_CORE_T th[RF_NP]) {
    ALWAN_CORE_T peak = ALWAN_CORE_FN(rf_data_peak)(s, th);
    if (peak <= ALWAN_CORE_LITERAL(1e-9)) return RF_INF;
    th[8] = ALWAN_CORE_LN(ALWAN_CORE_LITERAL(2.2));
    th[9] = ALWAN_CORE_LN(peak);
    if (s->tf_kind == ALWAN_FIT_TF_POWER && !(s->prm.lock & ALWAN_FIT_LOCK_TF)) ALWAN_CORE_FN(rf_search_gamma)(s, th);
    return ALWAN_CORE_FN(rf_objective)(s, th);
}

/* Build the parameter vector from a descriptor and a transfer function, or from the data. */
static alwan_status ALWAN_CORE_FN(rf_init_theta)(ALWAN_CORE_FN(alwan_rgb_fit_state) *s,
                                                 ALWAN_CORE_FN(alwan_rgb_space_desc) const *space,
                                                 ALWAN_CORE_FN(alwan_fit_tf) const *tf) {
    alwan_ctx *c = s->ctx;
    ALWAN_CORE_T th[RF_NP];
    if (space) {
        /* the warm start: what was handed in, scale 1 (the Y = 1 convention) unless given */
        for (int i = 0; i < 6; i++) th[i] = space->primaries_xy[i];
        th[6] = space->white_xy[0]; th[7] = space->white_xy[1];
        th[8] = ALWAN_CORE_LN(tf && tf->gamma > ALWAN_CORE_ZERO ? tf->gamma : ALWAN_CORE_LITERAL(2.2));
        th[9] = (tf && tf->scale > ALWAN_CORE_ZERO) ? ALWAN_CORE_LN(tf->scale) : ALWAN_CORE_ZERO;
        if (s->tf_kind == ALWAN_FIT_TF_POWER && !(s->prm.lock & ALWAN_FIT_LOCK_TF) && !(tf && tf->gamma > ALWAN_CORE_ZERO))
            ALWAN_CORE_FN(rf_search_gamma)(s, th);
        memcpy(s->theta, th, sizeof th);
        return ALWAN_OK;
    }
    if (s->prm.lock & (ALWAN_FIT_LOCK_WHITE | ALWAN_FIT_LOCK_PRIMARIES | ALWAN_FIT_LOCK_SCALE)) return ALWAN_E_INVALID;  /* nothing to lock to */

    /* candidate one: the enclosing triangle of the dataset's chromaticities, weighted white */
    ALWAN_CORE_T *pts = (ALWAN_CORE_T *)c->alloc_fn(s->n * 2 * sizeof(ALWAN_CORE_T), 16);
    ALWAN_CORE_T *hull = (ALWAN_CORE_T *)c->alloc_fn((s->n + 1) * 2 * sizeof(ALWAN_CORE_T), 16);
    if (!pts || !hull) { if (pts) c->free_fn(pts); if (hull) c->free_fn(hull); return ALWAN_E_NOMEM; }
    size_t m = 0; ALWAN_CORE_T wx = ALWAN_CORE_ZERO, wy = ALWAN_CORE_ZERO, ws = ALWAN_CORE_ZERO;
    for (size_t i = 0; i < s->n; i++) {
        ALWAN_CORE_T const *X = s->xyz + i * 3, sum = X[0] + X[1] + X[2];
        if (sum <= ALWAN_CORE_LITERAL(1e-9)) continue;
        pts[2*m] = X[0] / sum; pts[2*m+1] = X[1] / sum; m++;
        ALWAN_CORE_T wgt = s->w[i] * X[1];
        wx += wgt * X[0] / sum; wy += wgt * X[1] / sum; ws += wgt;
    }
    if (m < 3) { c->free_fn(pts); c->free_fn(hull); return ALWAN_E_INVALID; }
    size_t h = ALWAN_CORE_FN(rf_hull)(pts, m, hull);
    /* margin: two percent of the hull's extent, never under a thousandth in xy, doubled
     * until the triangle is one the matrices can be built from */
    ALWAN_CORE_T minx = RF_INF, maxx = -RF_INF, miny = RF_INF, maxy = -RF_INF;
    for (size_t i = 0; i < h; i++) { if (hull[2*i] < minx) minx = hull[2*i]; if (hull[2*i] > maxx) maxx = hull[2*i];
                                     if (hull[2*i+1] < miny) miny = hull[2*i+1]; if (hull[2*i+1] > maxy) maxy = hull[2*i+1]; }
    ALWAN_CORE_T ext = (maxx - minx) > (maxy - miny) ? (maxx - minx) : (maxy - miny);
    ALWAN_CORE_T margin = ext * ALWAN_CORE_LITERAL(0.02); if (margin < ALWAN_CORE_LITERAL(1e-3)) margin = ALWAN_CORE_LITERAL(1e-3);
    ALWAN_CORE_T f_hull = RF_INF;
    th[6] = ws > ALWAN_CORE_ZERO ? wx / ws : (minx + maxx) * ALWAN_CORE_LITERAL(0.5);
    th[7] = ws > ALWAN_CORE_ZERO ? wy / ws : (miny + maxy) * ALWAN_CORE_LITERAL(0.5);
    for (int grow = 0; grow < 8 && f_hull >= RF_INF; grow++, margin *= ALWAN_CORE_LITERAL(2.0)) {
        ALWAN_CORE_FN(rf_enclosing_triangle)(hull, h, margin, th);
        f_hull = ALWAN_CORE_FN(rf_complete_start)(s, th);
    }
    c->free_fn(pts); c->free_fn(hull);

    /* candidate two: the dataset's own space, when it has chromaticities. A dataset that
     * already fills its space is best served near it, and the hull start can sit in a
     * different basin. */
    if (s->have_data_prim) {
        ALWAN_CORE_T td[RF_NP]; memcpy(td, s->data_prim, 8 * sizeof(ALWAN_CORE_T));
        ALWAN_CORE_T f_data = ALWAN_CORE_FN(rf_complete_start)(s, td);
        if (f_data < f_hull) { memcpy(th, td, sizeof th); f_hull = f_data; }
    }
    if (f_hull >= RF_INF) return ALWAN_E_RANGE;
    memcpy(s->theta, th, sizeof th);
    return ALWAN_OK;
}

static void ALWAN_CORE_FN(rf_build_simplex)(ALWAN_CORE_FN(alwan_rgb_fit_state) *s, ALWAN_CORE_T step_xy, ALWAN_CORE_T step_lg) {
    s->k = 0;
    if (!(s->prm.lock & ALWAN_FIT_LOCK_PRIMARIES)) for (int i = 0; i < 6; i++) s->free_idx[s->k++] = i;
    if (!(s->prm.lock & ALWAN_FIT_LOCK_WHITE)) { s->free_idx[s->k++] = 6; s->free_idx[s->k++] = 7; }
    if (s->tf_kind == ALWAN_FIT_TF_POWER && !(s->prm.lock & ALWAN_FIT_LOCK_TF)) s->free_idx[s->k++] = 8;
    if (!(s->prm.lock & ALWAN_FIT_LOCK_SCALE)) s->free_idx[s->k++] = 9;
    for (int v = 0; v <= s->k; v++) {
        memcpy(s->simplex[v], s->theta, sizeof s->theta);
        if (v > 0) { int j = s->free_idx[v - 1]; s->simplex[v][j] += (j >= 8) ? step_lg : step_xy; }
        s->fval[v] = ALWAN_CORE_FN(rf_objective)(s, s->simplex[v]);
    }
    s->have_simplex = 1; s->converged = 0;
    /* the best vertex is the current answer */
    { int bi = 0; for (int v = 1; v <= s->k; v++) if (s->fval[v] < s->fval[bi]) bi = v;
      memcpy(s->theta, s->simplex[bi], sizeof s->theta); s->fbest = s->fval[bi]; }
}

/* One Nelder-Mead iteration over the free coordinates. */
static void ALWAN_CORE_FN(rf_nm_step)(ALWAN_CORE_FN(alwan_rgb_fit_state) *s) {
    int const k = s->k; if (k == 0) { s->converged = 1; return; }
    /* order: find worst, second worst, best */
    int iw = 0, ib = 0;
    for (int v = 1; v <= k; v++) { if (s->fval[v] > s->fval[iw]) iw = v; if (s->fval[v] < s->fval[ib]) ib = v; }
    ALWAN_CORE_T fsecond = -RF_INF;
    for (int v = 0; v <= k; v++) if (v != iw && s->fval[v] > fsecond) fsecond = s->fval[v];
    /* centroid of all but the worst, on the free coordinates */
    ALWAN_CORE_T cen[RF_NP]; memcpy(cen, s->theta, sizeof cen);
    for (int j = 0; j < k; j++) {
        int idx = s->free_idx[j]; ALWAN_CORE_T acc = ALWAN_CORE_ZERO;
        for (int v = 0; v <= k; v++) if (v != iw) acc += s->simplex[v][idx];
        cen[idx] = acc / (ALWAN_CORE_T)k;
    }
    ALWAN_CORE_T xr[RF_NP], xe[RF_NP], xc[RF_NP];
    memcpy(xr, cen, sizeof xr); memcpy(xe, cen, sizeof xe); memcpy(xc, cen, sizeof xc);
    for (int j = 0; j < k; j++) { int idx = s->free_idx[j]; xr[idx] = cen[idx] + (cen[idx] - s->simplex[iw][idx]); }
    ALWAN_CORE_T fr = ALWAN_CORE_FN(rf_objective)(s, xr);
    if (fr < s->fval[ib]) {
        for (int j = 0; j < k; j++) { int idx = s->free_idx[j]; xe[idx] = cen[idx] + ALWAN_CORE_LITERAL(2.0) * (xr[idx] - cen[idx]); }
        ALWAN_CORE_T fe = ALWAN_CORE_FN(rf_objective)(s, xe);
        if (fe < fr) { memcpy(s->simplex[iw], xe, sizeof xe); s->fval[iw] = fe; }
        else         { memcpy(s->simplex[iw], xr, sizeof xr); s->fval[iw] = fr; }
    } else if (fr < fsecond) {
        memcpy(s->simplex[iw], xr, sizeof xr); s->fval[iw] = fr;
    } else {
        int outside = fr < s->fval[iw];
        for (int j = 0; j < k; j++) { int idx = s->free_idx[j];
            xc[idx] = outside ? cen[idx] + ALWAN_CORE_LITERAL(0.5) * (xr[idx] - cen[idx])
                              : cen[idx] + ALWAN_CORE_LITERAL(0.5) * (s->simplex[iw][idx] - cen[idx]); }
        ALWAN_CORE_T fc = ALWAN_CORE_FN(rf_objective)(s, xc);
        if (fc < (outside ? fr : s->fval[iw])) { memcpy(s->simplex[iw], xc, sizeof xc); s->fval[iw] = fc; }
        else {   /* shrink toward the best */
            for (int v = 0; v <= k; v++) if (v != ib) {
                for (int j = 0; j < k; j++) { int idx = s->free_idx[j];
                    s->simplex[v][idx] = s->simplex[ib][idx] + ALWAN_CORE_LITERAL(0.5) * (s->simplex[v][idx] - s->simplex[ib][idx]); }
                s->fval[v] = ALWAN_CORE_FN(rf_objective)(s, s->simplex[v]);
            }
        }
    }
    /* best vertex, and convergence on the spread of values and positions */
    ib = 0; for (int v = 1; v <= k; v++) if (s->fval[v] < s->fval[ib]) ib = v;
    if (s->fval[ib] <= s->fbest) { memcpy(s->theta, s->simplex[ib], sizeof s->theta); s->fbest = s->fval[ib]; }
    {
        ALWAN_CORE_T fs = ALWAN_CORE_ZERO, xs = ALWAN_CORE_ZERO;
        for (int v = 0; v <= k; v++) {
            if (s->fval[v] < RF_INF) { ALWAN_CORE_T dfv = s->fval[v] - s->fval[ib]; if (dfv > fs) fs = dfv; }
            for (int j = 0; j < k; j++) { int idx = s->free_idx[j];
                ALWAN_CORE_T dx = ALWAN_CORE_ABS(s->simplex[v][idx] - s->simplex[ib][idx]); if (dx > xs) xs = dx; }
        }
        ALWAN_CORE_T scale = ALWAN_CORE_ABS(s->fval[ib]) > ALWAN_CORE_ONE ? ALWAN_CORE_ABS(s->fval[ib]) : ALWAN_CORE_ONE;
        if (fs <= s->prm.tolerance * scale || xs <= ALWAN_CORE_LITERAL(1e-7)) s->converged = 1;
    }
    s->iterations++;
}

static void ALWAN_CORE_FN(rf_write_answer)(ALWAN_CORE_FN(alwan_rgb_fit_state) *s,
                                           ALWAN_CORE_FN(alwan_rgb_space_desc) *space, ALWAN_CORE_FN(alwan_fit_tf) *tf) {
    ALWAN_CORE_FN(rf_desc_from_theta)(s->theta, space);
    if (tf) tf->scale = ALWAN_CORE_FN(rf_scale_of)(s->theta);
    if (s->tf_kind == ALWAN_FIT_TF_SRGB) {
        space->oetf = ALWAN_TF_SRGB; space->eotf = ALWAN_TF_SRGB;
        if (tf) { tf->kind = ALWAN_FIT_TF_SRGB; tf->gamma = ALWAN_CORE_ZERO; }
    } else {
        ALWAN_CORE_T g = ALWAN_CORE_EXP(s->theta[8]);
        if (tf) { tf->kind = ALWAN_FIT_TF_POWER; tf->gamma = g; }
        /* when the exponent lands on one the descriptor can name, name it */
        struct { ALWAN_CORE_T g; alwan_transfer_function t; } const named[4] = {
            { ALWAN_CORE_LITERAL(2.2), ALWAN_TF_GAMMA22 }, { ALWAN_CORE_LITERAL(2.4), ALWAN_TF_GAMMA24 },
            { ALWAN_CORE_LITERAL(2.6), ALWAN_TF_GAMMA26 }, { ALWAN_CORE_LITERAL(2.8), ALWAN_TF_GAMMA28 } };
        for (int i = 0; i < 4; i++) if (ALWAN_CORE_ABS(g - named[i].g) < ALWAN_CORE_LITERAL(5e-4)) { space->oetf = named[i].t; space->eotf = named[i].t; }
    }
}

/* ---------------------------------------------------------------- */
/* public                                                             */
/* ---------------------------------------------------------------- */

void ALWAN_CORE_FN(alwan_rgb_fit_params_init)(ALWAN_CORE_FN(alwan_rgb_fit_params) *p) {
    if (!p) return;
    memset(p, 0, sizeof *p);
    p->bits = 8;
    p->tf = ALWAN_FIT_TF_POWER;
    p->metric = ALWAN_FIT_METRIC_OKLAB;
    p->percentile = ALWAN_CORE_LITERAL(0.999);
    p->clip_weight = ALWAN_CORE_LITERAL(1.0);
    p->clip_policy = ALWAN_FIT_CLIP_FORBID;
    p->srgb_margin = ALWAN_CORE_LITERAL(0.10);
    p->gamma_min = ALWAN_CORE_LITERAL(1.0);
    p->gamma_max = ALWAN_CORE_LITERAL(3.0);
    p->step_xy = ALWAN_CORE_LITERAL(0.01);
    p->step_log_gamma = ALWAN_CORE_LITERAL(0.1);
    p->tolerance = ALWAN_CORE_LITERAL(1e-6);
    p->weights = NULL;
}

alwan_status ALWAN_CORE_FN(alwan_rgb_fit_begin)(ALWAN_CORE_FN(alwan_rgb_fit_state) **state,
                                                ALWAN_CORE_T const *data, size_t stride, size_t count,
                                                ALWAN_CORE_FN(alwan_rgb_space_desc) const *data_space,
                                                ALWAN_CORE_FN(alwan_rgb_space_desc) const *space,
                                                ALWAN_CORE_FN(alwan_fit_tf) const *tf,
                                                ALWAN_CORE_FN(alwan_rgb_fit_params) const *params, alwan_ctx *ctx) {
    if (!state || !data || !data_space || !params || !ctx || count < 3) return ALWAN_E_INVALID;
    if (params->bits < 2 || params->bits > 16) return ALWAN_E_INVALID;
    if (params->tf != ALWAN_FIT_TF_POWER && params->tf != ALWAN_FIT_TF_SRGB) return ALWAN_E_INVALID;   /* AUTO is solve's */
    if (params->gamma_min <= ALWAN_CORE_ZERO || params->gamma_max < params->gamma_min) return ALWAN_E_INVALID;
    if (tf && tf->scale > ALWAN_CORE_ZERO && (tf->scale < ALWAN_CORE_LITERAL(1e-8) || tf->scale > ALWAN_CORE_LITERAL(1e8))) return ALWAN_E_INVALID;
    *state = NULL;
    ALWAN_CORE_FN(alwan_rgb_fit_state) *s = (ALWAN_CORE_FN(alwan_rgb_fit_state) *)ctx->alloc_fn(sizeof *s, 16);
    if (!s) return ALWAN_E_NOMEM;
    memset(s, 0, sizeof *s);
    s->ctx = ctx; s->prm = *params; s->n = count; s->tf_kind = params->tf;
    if (params->metric == ALWAN_FIT_METRIC_DISPLAY || params->metric == ALWAN_FIT_METRIC_LINEAR) {
        ALWAN_CORE_FN(alwan_rgb_space_desc) d709;
        if (ALWAN_CORE_FN(alwan_rgb_get_space_descriptor)(&d709, ALWAN_RGB_SPACE_SRGB, ctx) != ALWAN_OK) { ctx->free_fn(s); return ALWAN_E_NODATA; }
        if (!d709.has_matrices && ALWAN_CORE_FN(alwan_rgb_derive_matrices)(&d709.rgb_to_xyz, &d709.xyz_to_rgb, &d709) != ALWAN_OK) { ctx->free_fn(s); return ALWAN_E_RANGE; }
        memcpy(s->disp, d709.xyz_to_rgb.m, sizeof s->disp);
    }
    if (data_space->primaries_xy[1] > ALWAN_CORE_ZERO && data_space->white_xy[1] > ALWAN_CORE_ZERO) {
        for (int i = 0; i < 6; i++) s->data_prim[i] = data_space->primaries_xy[i];
        s->data_prim[6] = data_space->white_xy[0]; s->data_prim[7] = data_space->white_xy[1];
        s->have_data_prim = 1;
    }
    s->xyz = (ALWAN_CORE_T *)ctx->alloc_fn(count * 3 * sizeof(ALWAN_CORE_T), 16);
    s->pc  = (ALWAN_CORE_T *)ctx->alloc_fn(count * 3 * sizeof(ALWAN_CORE_T), 16);
    s->jp  = (ALWAN_CORE_T *)ctx->alloc_fn(count * 9 * sizeof(ALWAN_CORE_T), 16);
    s->w   = (ALWAN_CORE_T *)ctx->alloc_fn(count * sizeof(ALWAN_CORE_T), 16);
    s->scratch = (ALWAN_CORE_T *)ctx->alloc_fn(count * sizeof(ALWAN_CORE_T), 16);
    if (!s->xyz || !s->pc || !s->jp || !s->w || !s->scratch) { ALWAN_CORE_FN(rf_free_state)(s); return ALWAN_E_NOMEM; }

    /* the dataset into XYZ, through the caller's space */
    ALWAN_CORE_FN(alwan_rgb_space_desc) src = *data_space;
    if (!src.has_matrices) {
        if (ALWAN_CORE_FN(alwan_rgb_derive_matrices)(&src.rgb_to_xyz, &src.xyz_to_rgb, &src) != ALWAN_OK) { ALWAN_CORE_FN(rf_free_state)(s); return ALWAN_E_RANGE; }
        src.has_matrices = 1;
    }
    ALWAN_CORE_T wsum = ALWAN_CORE_ZERO;
    for (size_t i = 0; i < count; i++) {
        ALWAN_CORE_T const *px = (ALWAN_CORE_T const *)((char const *)data + i * stride);
        ALWAN_CORE_FN(alwan_rgb) rgb; rgb.r = px[0]; rgb.g = px[1]; rgb.b = px[2];
        ALWAN_CORE_FN(alwan_xyz) X;
        if (ALWAN_CORE_FN(alwan_rgb_to_xyz)(&X, &src, &rgb) != ALWAN_OK) { ALWAN_CORE_FN(rf_free_state)(s); return ALWAN_E_INVALID; }
        s->xyz[i*3] = X.x; s->xyz[i*3+1] = X.y; s->xyz[i*3+2] = X.z;
        s->w[i] = params->weights ? params->weights[i] : ALWAN_CORE_ONE;
        if (s->w[i] < ALWAN_CORE_ZERO) s->w[i] = ALWAN_CORE_ZERO;
        wsum += s->w[i];
    }
    if (wsum <= ALWAN_CORE_ZERO) { ALWAN_CORE_FN(rf_free_state)(s); return ALWAN_E_INVALID; }
    for (size_t i = 0; i < count; i++) s->w[i] /= wsum;

    /* perceptual coordinates and their Jacobian, once */
    for (size_t i = 0; i < count; i++) {
        ALWAN_CORE_T const *X = s->xyz + i * 3;
        ALWAN_CORE_FN(rf_pcoord)(X, params->metric, s->disp, s->pc + i * 3);
        for (int c = 0; c < 3; c++) {
            ALWAN_CORE_T h = ALWAN_CORE_LITERAL(1e-4) * (ALWAN_CORE_ABS(X[c]) > ALWAN_CORE_LITERAL(1e-2) ? ALWAN_CORE_ABS(X[c]) : ALWAN_CORE_LITERAL(1e-2));
            ALWAN_CORE_T xp[3] = { X[0], X[1], X[2] }, xm[3] = { X[0], X[1], X[2] }, pp[3], pm[3];
            xp[c] += h; xm[c] -= h; if (xm[c] < ALWAN_CORE_ZERO) xm[c] = ALWAN_CORE_ZERO;
            ALWAN_CORE_FN(rf_pcoord)(xp, params->metric, s->disp, pp); ALWAN_CORE_FN(rf_pcoord)(xm, params->metric, s->disp, pm);
            ALWAN_CORE_T den = xp[c] - xm[c]; if (den <= ALWAN_CORE_ZERO) den = h;
            for (int r = 0; r < 3; r++) s->jp[i*9 + r*3 + c] = (pp[r] - pm[r]) / den;
        }
    }

    alwan_status st = ALWAN_CORE_FN(rf_init_theta)(s, space, tf);
    if (st != ALWAN_OK) { ALWAN_CORE_FN(rf_free_state)(s); return st; }
    ALWAN_CORE_FN(rf_build_simplex)(s, params->step_xy, params->step_log_gamma);
    if (s->fbest >= RF_INF) { ALWAN_CORE_FN(rf_free_state)(s); return ALWAN_E_RANGE; }   /* the start itself is infeasible */
    *state = s;
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FN(alwan_rgb_fit_step)(ALWAN_CORE_FN(alwan_rgb_space_desc) *space, ALWAN_CORE_FN(alwan_fit_tf) *tf,
                                               ALWAN_CORE_FN(alwan_rgb_fit_report) *report,
                                               ALWAN_CORE_FN(alwan_rgb_fit_state) *state) {
    if (!state || !space) return ALWAN_E_INVALID;
    if (!state->converged) ALWAN_CORE_FN(rf_nm_step)(state);
    ALWAN_CORE_FN(rf_write_answer)(state, space, tf);
    if (report) {
        memset(report, 0, sizeof *report);
        alwan_status st = ALWAN_CORE_FN(rf_true_report)(state, state->theta, report);
        if (st != ALWAN_OK) return st;
        report->objective = state->fbest; report->iterations = state->iterations; report->converged = state->converged;
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FN(alwan_rgb_fit_restart)(ALWAN_CORE_FN(alwan_rgb_fit_state) *state, ALWAN_CORE_T step_xy, ALWAN_CORE_T step_log_gamma) {
    if (!state || step_xy <= ALWAN_CORE_ZERO || step_log_gamma <= ALWAN_CORE_ZERO) return ALWAN_E_INVALID;
    ALWAN_CORE_FN(rf_build_simplex)(state, step_xy, step_log_gamma);
    return ALWAN_OK;
}

void ALWAN_CORE_FN(alwan_rgb_fit_end)(ALWAN_CORE_FN(alwan_rgb_fit_state) *state) { ALWAN_CORE_FN(rf_free_state)(state); }

alwan_status ALWAN_CORE_FN(alwan_rgb_fit_evaluate)(ALWAN_CORE_FN(alwan_rgb_fit_report) *report,
                                                   ALWAN_CORE_FN(alwan_rgb_space_desc) const *space, ALWAN_CORE_FN(alwan_fit_tf) const *tf,
                                                   ALWAN_CORE_T const *data, size_t stride, size_t count,
                                                   ALWAN_CORE_FN(alwan_rgb_space_desc) const *data_space,
                                                   ALWAN_CORE_FN(alwan_rgb_fit_params) const *params, alwan_ctx *ctx) {
    if (!report || !space || !tf || !params) return ALWAN_E_INVALID;
    ALWAN_CORE_FN(alwan_rgb_fit_params) p = *params;
    p.tf = tf->kind == ALWAN_FIT_TF_SRGB ? ALWAN_FIT_TF_SRGB : ALWAN_FIT_TF_POWER;
    p.lock = ALWAN_FIT_LOCK_WHITE | ALWAN_FIT_LOCK_PRIMARIES | ALWAN_FIT_LOCK_TF;
    /* the exponent must fit the bounds only to be evaluated, so widen them around it */
    if (p.tf == ALWAN_FIT_TF_POWER && tf->gamma > ALWAN_CORE_ZERO) {
        if (tf->gamma < p.gamma_min) p.gamma_min = tf->gamma; if (tf->gamma > p.gamma_max) p.gamma_max = tf->gamma; }
    ALWAN_CORE_FN(alwan_rgb_fit_state) *s = NULL;
    alwan_status st = ALWAN_CORE_FN(alwan_rgb_fit_begin)(&s, data, stride, count, data_space, space, tf, &p, ctx);
    if (st != ALWAN_OK) return st;
    memset(report, 0, sizeof *report);
    st = ALWAN_CORE_FN(rf_true_report)(s, s->theta, report);
    report->objective = s->fbest;
    ALWAN_CORE_FN(rf_free_state)(s);
    return st;
}

static alwan_status ALWAN_CORE_FN(rf_solve_one)(ALWAN_CORE_FN(alwan_rgb_space_desc) *space, ALWAN_CORE_FN(alwan_fit_tf) *tf,
                                                ALWAN_CORE_FN(alwan_rgb_fit_report) *report,
                                                ALWAN_CORE_T const *data, size_t stride, size_t count,
                                                ALWAN_CORE_FN(alwan_rgb_space_desc) const *data_space,
                                                ALWAN_CORE_FN(alwan_rgb_fit_params) const *params, int max_iterations,
                                                int warm, alwan_ctx *ctx) {
    ALWAN_CORE_FN(alwan_rgb_fit_state) *s = NULL;
    alwan_status st = ALWAN_CORE_FN(alwan_rgb_fit_begin)(&s, data, stride, count, data_space,
                                                         warm ? space : NULL, warm ? tf : NULL, params, ctx);
    if (st != ALWAN_OK) return st;
    for (int it = 0; it < max_iterations && !s->converged; it++) ALWAN_CORE_FN(rf_nm_step)(s);
    ALWAN_CORE_FN(rf_write_answer)(s, space, tf);
    memset(report, 0, sizeof *report);
    st = ALWAN_CORE_FN(rf_true_report)(s, s->theta, report);
    report->objective = s->fbest; report->iterations = s->iterations; report->converged = s->converged;
    ALWAN_CORE_FN(rf_free_state)(s);
    return st;
}

alwan_status ALWAN_CORE_FN(alwan_rgb_fit_solve)(ALWAN_CORE_FN(alwan_rgb_space_desc) *space, ALWAN_CORE_FN(alwan_fit_tf) *tf,
                                                ALWAN_CORE_FN(alwan_rgb_fit_report) *report,
                                                ALWAN_CORE_T const *data, size_t stride, size_t count,
                                                ALWAN_CORE_FN(alwan_rgb_space_desc) const *data_space,
                                                ALWAN_CORE_FN(alwan_rgb_fit_params) const *params, int max_iterations, alwan_ctx *ctx) {
    if (!space || !tf || !report || !params || max_iterations < 0) return ALWAN_E_INVALID;
    /* a zeroed descriptor means start from the data; anything else is a warm start */
    int warm = (space->primaries_xy[1] > ALWAN_CORE_ZERO && space->white_xy[1] > ALWAN_CORE_ZERO);
    if (params->tf != ALWAN_FIT_TF_AUTO) {
        ALWAN_CORE_FN(alwan_rgb_fit_params) p = *params;
        return ALWAN_CORE_FN(rf_solve_one)(space, tf, report, data, stride, count, data_space, &p, max_iterations, warm, ctx);
    }
    /* AUTO: both branches from the same start, sRGB unless the power wins by the margin */
    ALWAN_CORE_FN(alwan_rgb_space_desc) s_pow = *space, s_srgb = *space;
    ALWAN_CORE_FN(alwan_fit_tf) t_pow = *tf, t_srgb = *tf;
    ALWAN_CORE_FN(alwan_rgb_fit_report) r_pow, r_srgb;
    ALWAN_CORE_FN(alwan_rgb_fit_params) p = *params;
    p.tf = ALWAN_FIT_TF_POWER;
    alwan_status st = ALWAN_CORE_FN(rf_solve_one)(&s_pow, &t_pow, &r_pow, data, stride, count, data_space, &p, max_iterations, warm, ctx);
    if (st != ALWAN_OK) return st;
    p.tf = ALWAN_FIT_TF_SRGB;
    st = ALWAN_CORE_FN(rf_solve_one)(&s_srgb, &t_srgb, &r_srgb, data, stride, count, data_space, &p, max_iterations, warm, ctx);
    if (st != ALWAN_OK) return st;
    ALWAN_CORE_T pow_score = r_pow.mean_error + r_pow.tail_error, srgb_score = r_srgb.mean_error + r_srgb.tail_error;
    if (pow_score < srgb_score * (ALWAN_CORE_ONE - params->srgb_margin)) { *space = s_pow; *tf = t_pow; *report = r_pow; }
    else { *space = s_srgb; *tf = t_srgb; *report = r_srgb; }
    return ALWAN_OK;
}

#undef RF_NP
#undef RF_INF
#undef RF_LOG_SCALE_LIMIT
