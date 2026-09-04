/* alwan_form_global_exp_impl.inc: EXPERIMENTAL picture-formation global solver.
 *
 * Under experimental/ the usual alwan guarantees are RELAXED: this code may be non-deterministic and
 * uses inlined magic constants (no gendata). It is NOT part of the constraint-tested surface. Compiled
 * twice (f32/f64); every scalar is ALWAN_CORE_T. It builds on the shipped operators by calling the
 * PUBLIC spatial API (alwan_gamut_map_spatial), duplicating nothing.
 *
 * Central design (Troy's spec): the global stage solves ONLY a scalar exposure field e per pixel, and
 * the picture is forced through the fixed pointwise operator with the channel-integration gate q FROZEN
 * from the original input:   Y_i = F_base(2^{e_i} X_i ; q_i).   No free chroma / illuminant / purity
 * variable exists, so the solver cannot rotate hue -- pink failures are ruled out by construction.
 */

/* COMPLETE's log-logistic carrier tone (with the asymptotic high rail). dlo/drng precomputed. */
static ALWAN_CORE_T ALWAN_CORE_FN(alwan__exp_tone)(ALWAN_CORE_T car, ALWAN_CORE_T dlo, ALWAN_CORE_T drng){
    if (car <= ALWAN_CORE_LITERAL(1e-9)) return ALWAN_CORE_LITERAL(0.0);
    ALWAN_CORE_T vpos = (ALWAN_CORE_LOG2(car) - ALWAN_CORE_LITERAL(-12.47393119)) / ALWAN_CORE_LITERAL(16.5);
    ALWAN_CORE_T dv = ALWAN_CORE_LITERAL(1.0) / (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_EXP(-ALWAN_CORE_LITERAL(9.1419001) * (vpos - ALWAN_CORE_LITERAL(0.60606061))));
    ALWAN_CORE_T dn = (dv - dlo) / drng; if (dn < ALWAN_CORE_LITERAL(0.0)) dn = ALWAN_CORE_LITERAL(0.0);
    ALWAN_CORE_T ms = ALWAN_CORE_POW(dn, ALWAN_CORE_LITERAL(2.47393119));
    ALWAN_CORE_T rhi = ALWAN_CORE_LITERAL(0.02);
    if (ms > ALWAN_CORE_LITERAL(1.0) - rhi)
        ms = ALWAN_CORE_LITERAL(1.0) - rhi * ALWAN_CORE_EXP(((ALWAN_CORE_LITERAL(1.0) - ms) - rhi) / rhi);
    return ms;
}
static ALWAN_CORE_T ALWAN_CORE_FN(alwan__exp_sstep)(ALWAN_CORE_T a, ALWAN_CORE_T b, ALWAN_CORE_T x){
    ALWAN_CORE_T d = b - a; if (d < ALWAN_CORE_LITERAL(1e-12) && d > ALWAN_CORE_LITERAL(-1e-12)) d = ALWAN_CORE_LITERAL(1e-12);
    ALWAN_CORE_T t = (x - a) / d; if (t < ALWAN_CORE_LITERAL(0.0)) t = ALWAN_CORE_LITERAL(0.0); if (t > ALWAN_CORE_LITERAL(1.0)) t = ALWAN_CORE_LITERAL(1.0);
    return t * t * (ALWAN_CORE_LITERAL(3.0) - ALWAN_CORE_LITERAL(2.0) * t);
}
static ALWAN_CORE_T ALWAN_CORE_FN(alwan__exp_qgate)(ALWAN_CORE_T r, ALWAN_CORE_T g, ALWAN_CORE_T b,
                                                    ALWAN_CORE_T Tpmax, ALWAN_CORE_T dlo, ALWAN_CORE_T drng){
    ALWAN_CORE_T const eu = ALWAN_CORE_LITERAL(1.07177), ed = ALWAN_CORE_LITERAL(0.93303);
    ALWAN_CORE_T rk = r > ALWAN_CORE_LITERAL(1e-4) ? r : ALWAN_CORE_LITERAL(1e-4);
    ALWAN_CORE_T gk = g > ALWAN_CORE_LITERAL(1e-4) ? g : ALWAN_CORE_LITERAL(1e-4);
    ALWAN_CORE_T bk = b > ALWAN_CORE_LITERAL(1e-4) ? b : ALWAN_CORE_LITERAL(1e-4);
    ALWAN_CORE_T Tr = (ALWAN_CORE_FN(alwan__exp_tone)(rk*eu,dlo,drng) - ALWAN_CORE_FN(alwan__exp_tone)(rk*ed,dlo,drng)) / ALWAN_CORE_LITERAL(0.2);
    ALWAN_CORE_T Tg = (ALWAN_CORE_FN(alwan__exp_tone)(gk*eu,dlo,drng) - ALWAN_CORE_FN(alwan__exp_tone)(gk*ed,dlo,drng)) / ALWAN_CORE_LITERAL(0.2);
    ALWAN_CORE_T Tb = (ALWAN_CORE_FN(alwan__exp_tone)(bk*eu,dlo,drng) - ALWAN_CORE_FN(alwan__exp_tone)(bk*ed,dlo,drng)) / ALWAN_CORE_LITERAL(0.2);
    ALWAN_CORE_T tmax = Tr > Tg ? (Tr > Tb ? Tr : Tb) : (Tg > Tb ? Tg : Tb);
    ALWAN_CORE_T tmin = Tr < Tg ? (Tr < Tb ? Tr : Tb) : (Tg < Tb ? Tg : Tb);
    ALWAN_CORE_T mx = rk > gk ? (rk > bk ? rk : bk) : (gk > bk ? gk : bk);
    ALWAN_CORE_T maxz = ALWAN_CORE_LOG2(mx / ALWAN_CORE_LITERAL(0.18));
    ALWAN_CORE_T phi = ALWAN_CORE_FN(alwan__exp_sstep)(ALWAN_CORE_LITERAL(0.15)*Tpmax, ALWAN_CORE_LITERAL(0.55)*Tpmax, tmax - tmin);
    ALWAN_CORE_T psi = ALWAN_CORE_FN(alwan__exp_sstep)(ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(4.0), maxz);
    ALWAN_CORE_T q = phi * psi; if (q < ALWAN_CORE_LITERAL(0.0)) q = ALWAN_CORE_LITERAL(0.0); if (q > ALWAN_CORE_LITERAL(1.0)) q = ALWAN_CORE_LITERAL(1.0);
    return q;
}
/* frozen q field from the original record */
static void ALWAN_CORE_FN(alwan__exp_qfield)(ALWAN_CORE_T *qf, ALWAN_CORE_T const *in, int W, int H){
    ALWAN_CORE_T const gg = ALWAN_CORE_LITERAL(9.1419001), vp = ALWAN_CORE_LITERAL(0.60606061);
    ALWAN_CORE_T dlo = ALWAN_CORE_LITERAL(1.0) / (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_EXP(gg * vp));
    ALWAN_CORE_T dhi = ALWAN_CORE_LITERAL(1.0) / (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_EXP(-gg * (ALWAN_CORE_LITERAL(1.0) - vp)));
    ALWAN_CORE_T drng = dhi - dlo; if (drng < ALWAN_CORE_LITERAL(1e-6)) drng = ALWAN_CORE_LITERAL(1e-6);
    ALWAN_CORE_T Tpmax = ALWAN_CORE_LITERAL(1e-6);
    for (int i = 0; i < 400; i++) {
        ALWAN_CORE_T zz  = ALWAN_CORE_LITERAL(-14.0) + ALWAN_CORE_LITERAL(24.0) * ((ALWAN_CORE_T)i) / ALWAN_CORE_LITERAL(399.0);
        ALWAN_CORE_T car = ALWAN_CORE_LITERAL(0.18) * ALWAN_CORE_POW(ALWAN_CORE_LITERAL(2.0), zz);
        ALWAN_CORE_T d = (ALWAN_CORE_FN(alwan__exp_tone)(car*ALWAN_CORE_LITERAL(1.07177),dlo,drng) - ALWAN_CORE_FN(alwan__exp_tone)(car*ALWAN_CORE_LITERAL(0.93303),dlo,drng)) / ALWAN_CORE_LITERAL(0.2);
        if (d > Tpmax) Tpmax = d;
    }
    size_t N = (size_t)W * (size_t)H;
    for (size_t i = 0; i < N; i++)
        qf[i] = ALWAN_CORE_FN(alwan__exp_qgate)(in[i*3], in[i*3+1], in[i*3+2], Tpmax, dlo, drng);
}
/* Y = (1-q)*COMPLETE_HEMI_LOOK(xin) + q*CHANNEL(xin), q PRECOMPUTED (frozen). */
static int ALWAN_CORE_FN(alwan__exp_form_hybrid)(ALWAN_CORE_T *out, ALWAN_CORE_T const *xin, ALWAN_CORE_T const *qf, int W, int H, alwan_ctx *ctx){
    size_t N = (size_t)W * (size_t)H;
    ALWAN_CORE_T *cl = (ALWAN_CORE_T*)ctx->alloc_fn(N*3*sizeof(ALWAN_CORE_T), 16);
    ALWAN_CORE_T *ch = (ALWAN_CORE_T*)ctx->alloc_fn(N*3*sizeof(ALWAN_CORE_T), 16);
    if (!cl || !ch) { if (cl) ctx->free_fn(cl); if (ch) ctx->free_fn(ch); return ALWAN_E_NOMEM; }
    ALWAN_CORE_FN(alwan_gamut_spatial_params) p; memset(&p, 0, sizeof(p));
    p.s = ALWAN_CORE_LITERAL(1.0); p.beta = ALWAN_CORE_LITERAL(8.0); p.compress = ALWAN_CORE_LITERAL(1.0);
    p.peak = ALWAN_CORE_LITERAL(1.0); p.iterations = 120;
    p.method = ALWAN_GAMUT_FORM_COMPLETE_HEMI_LOOK;
    { int st = ALWAN_CORE_FN(alwan_gamut_map_spatial)(cl, xin, NULL, W, H, &p, ctx); if (st != ALWAN_OK) { ctx->free_fn(cl); ctx->free_fn(ch); return st; } }
    p.method = ALWAN_GAMUT_FORM_CHANNEL;
    { int st = ALWAN_CORE_FN(alwan_gamut_map_spatial)(ch, xin, NULL, W, H, &p, ctx); if (st != ALWAN_OK) { ctx->free_fn(cl); ctx->free_fn(ch); return st; } }
    for (size_t i = 0; i < N; i++) {
        ALWAN_CORE_T q = qf[i], om = ALWAN_CORE_LITERAL(1.0) - q;
        for (int k = 0; k < 3; k++) {
            ALWAN_CORE_T v = om * cl[i*3+k] + q * ch[i*3+k];
            if (v < ALWAN_CORE_LITERAL(0.0)) v = ALWAN_CORE_LITERAL(0.0); if (v > ALWAN_CORE_LITERAL(1.0)) v = ALWAN_CORE_LITERAL(1.0);
            out[i*3+k] = v;
        }
    }
    ctx->free_fn(cl); ctx->free_fn(ch);
    return ALWAN_OK;
}

/* Public: HYBRID-point base operator (e = 0). */
alwan_status ALWAN_CORE_FN(alwan_picture_form_hybrid_exp)(ALWAN_CORE_T *out, ALWAN_CORE_T const *in, int W, int H, alwan_ctx *ctx){
    if (!out || !in || W <= 0 || H <= 0 || !ctx) return ALWAN_E_INVALID;
    size_t N = (size_t)W * (size_t)H;
    ALWAN_CORE_T *qf = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T), 16); if (!qf) return ALWAN_E_NOMEM;
    ALWAN_CORE_FN(alwan__exp_qfield)(qf, in, W, H);
    int st = ALWAN_CORE_FN(alwan__exp_form_hybrid)(out, in, qf, W, H, ctx);
    ctx->free_fn(qf);
    return st;
}

/* Single-carrier solver base: COMPLETE_HEMI_LOOK. Measured exactly hue-stable under exposure
 * (0.000 deg opponent drift at any e), so the scalar-e solver on this base is provably pink-immune;
 * it is also a full 13/13 constraint row. (CHANNEL, by contrast, rotates hue ~3.8 deg per 0.1 EV,
 * which is why the HYBRID blend is NOT used as the solver base.) */
static int ALWAN_CORE_FN(alwan__exp_form_base)(ALWAN_CORE_T *out, ALWAN_CORE_T const *xin, int W, int H, alwan_ctx *ctx){
    ALWAN_CORE_FN(alwan_gamut_spatial_params) p; memset(&p, 0, sizeof(p));
    p.s = ALWAN_CORE_LITERAL(1.0); p.beta = ALWAN_CORE_LITERAL(8.0); p.compress = ALWAN_CORE_LITERAL(1.0);
    p.peak = ALWAN_CORE_LITERAL(1.0); p.iterations = 120; p.method = ALWAN_GAMUT_FORM_COMPLETE_HEMI_LOOK;
    return ALWAN_CORE_FN(alwan_gamut_map_spatial)(out, xin, NULL, W, H, &p, ctx);
}

/* separable box blur (edge-clamped), 'passes' of half-width r; ~Gaussian sigma=r*sqrt(passes/... ). */
static void ALWAN_CORE_FN(alwan__exp_boxblur)(ALWAN_CORE_T *a, ALWAN_CORE_T *tmp, int W, int H, int r, int passes){
    ALWAN_CORE_T inv = ALWAN_CORE_LITERAL(1.0) / (ALWAN_CORE_T)(2*r + 1);
    for (int p = 0; p < passes; p++) {
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            ALWAN_CORE_T s = ALWAN_CORE_LITERAL(0.0);
            for (int dx = -r; dx <= r; dx++) { int xx = x+dx; if (xx < 0) xx = 0; if (xx >= W) xx = W-1; s += a[(size_t)y*W+xx]; }
            tmp[(size_t)y*W+x] = s * inv;
        }
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            ALWAN_CORE_T s = ALWAN_CORE_LITERAL(0.0);
            for (int dy = -r; dy <= r; dy++) { int yy = y+dy; if (yy < 0) yy = 0; if (yy >= H) yy = H-1; s += tmp[(size_t)yy*W+x]; }
            a[(size_t)y*W+x] = s * inv;
        }
    }
}

/* separable grey-scale erosion (min filter, square window of half-width r, edge-clamped) */
static void ALWAN_CORE_FN(alwan__exp_erode)(ALWAN_CORE_T *dst, ALWAN_CORE_T const *src, ALWAN_CORE_T *tmp, int W, int H, int r){
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        ALWAN_CORE_T m = src[(size_t)y*W + x];
        for (int dx = -r; dx <= r; dx++) { int xx = x+dx; if (xx < 0) xx = 0; if (xx >= W) xx = W-1;
            ALWAN_CORE_T v = src[(size_t)y*W + xx]; if (v < m) m = v; }
        tmp[(size_t)y*W + x] = m;
    }
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        ALWAN_CORE_T m = tmp[(size_t)y*W + x];
        for (int dy = -r; dy <= r; dy++) { int yy = y+dy; if (yy < 0) yy = 0; if (yy >= H) yy = H-1;
            ALWAN_CORE_T v = tmp[(size_t)yy*W + x]; if (v < m) m = v; }
        dst[(size_t)y*W + x] = m;
    }
}

/* occlusion evidence: P_O = sstep(0.35,1.1, |central-difference carrier gradient|), borders 0. */
static void ALWAN_CORE_FN(alwan__exp_occlusion)(ALWAN_CORE_T *PO, ALWAN_CORE_T const *zc, int W, int H){
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        size_t i = (size_t)y*W + x;
        ALWAN_CORE_T gx = (x > 0 && x < W-1) ? ALWAN_CORE_LITERAL(0.5)*ALWAN_CORE_ABS(zc[i+1]-zc[i-1]) : ALWAN_CORE_LITERAL(0.0);
        ALWAN_CORE_T gy = (y > 0 && y < H-1) ? ALWAN_CORE_LITERAL(0.5)*ALWAN_CORE_ABS(zc[i+W]-zc[i-W]) : ALWAN_CORE_LITERAL(0.0);
        ALWAN_CORE_T edge = ALWAN_CORE_SQRT(gx*gx + gy*gy);
        PO[i] = ALWAN_CORE_FN(alwan__exp_sstep)(ALWAN_CORE_LITERAL(0.35), ALWAN_CORE_LITERAL(1.1), edge);
    }
}

/* Public: per-pixel junction evidence P(Continuation / Transmission / Occlusion).  A DIAGNOSTIC, not
 * a picture: it reports how the local gradient structure explains each pixel, from the validated
 * gradient-formation reframe (Troy). Occlusion = strong carrier boundary (silhouette); Transmission =
 * additive light lifting the dark channel (glow/veil), gated to non-occluding pixels; Continuation =
 * one integrated illuminated volume (a warm-lit face reads here, NOT as a pink layer to discount).
 * out is W*H*3 interleaved = (P_C, P_T, P_O) per pixel; P_C+P_T+P_O = 1. Does not modify the picture. */
alwan_status ALWAN_CORE_FN(alwan_picture_form_evidence)(ALWAN_CORE_T *out, ALWAN_CORE_T const *in, int W, int H, alwan_ctx *ctx){
    if (!out || !in || W <= 0 || H <= 0 || !ctx) return ALWAN_E_INVALID;
    size_t N = (size_t)W * (size_t)H;
    ALWAN_CORE_T *zc  = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *mn  = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *tmp = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *PO  = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    int ok = zc && mn && tmp && PO;
    if (!ok) { if (zc) ctx->free_fn(zc); if (mn) ctx->free_fn(mn); if (tmp) ctx->free_fn(tmp); if (PO) ctx->free_fn(PO); return ALWAN_E_NOMEM; }
    for (size_t i = 0; i < N; i++) {
        ALWAN_CORE_T r = in[i*3], g = in[i*3+1], b = in[i*3+2];
        ALWAN_CORE_T mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
        if (mx < ALWAN_CORE_LITERAL(1e-6)) mx = ALWAN_CORE_LITERAL(1e-6);
        zc[i] = ALWAN_CORE_LOG2(mx);
        ALWAN_CORE_T rc = r > ALWAN_CORE_LITERAL(0.0) ? r : ALWAN_CORE_LITERAL(0.0);
        ALWAN_CORE_T gc = g > ALWAN_CORE_LITERAL(0.0) ? g : ALWAN_CORE_LITERAL(0.0);
        ALWAN_CORE_T bc = b > ALWAN_CORE_LITERAL(0.0) ? b : ALWAN_CORE_LITERAL(0.0);
        mn[i] = rc < gc ? (rc < bc ? rc : bc) : (gc < bc ? gc : bc);
    }
    ALWAN_CORE_FN(alwan__exp_occlusion)(PO, zc, W, H);
    /* additive light lifts the (low-passed) dark channel; 5 box passes ~ Gaussian sigma ~ 3.2 */
    ALWAN_CORE_FN(alwan__exp_boxblur)(mn, tmp, W, H, 2, 5);
    for (size_t i = 0; i < N; i++) {
        ALWAN_CORE_T po = PO[i];
        ALWAN_CORE_T pt = (ALWAN_CORE_LITERAL(1.0) - po) * ALWAN_CORE_FN(alwan__exp_sstep)(ALWAN_CORE_LITERAL(0.30), ALWAN_CORE_LITERAL(1.4), mn[i]);
        ALWAN_CORE_T pc = ALWAN_CORE_LITERAL(1.0) - po - pt; if (pc < ALWAN_CORE_LITERAL(0.0)) pc = ALWAN_CORE_LITERAL(0.0); if (pc > ALWAN_CORE_LITERAL(1.0)) pc = ALWAN_CORE_LITERAL(1.0);
        out[i*3+0] = pc; out[i*3+1] = pt; out[i*3+2] = po;
    }
    ctx->free_fn(zc); ctx->free_fn(mn); ctx->free_fn(tmp); ctx->free_fn(PO);
    return ALWAN_OK;
}

/* Public: global exposure operator.  Y = F_base(2^e0 X), with a scalar per-pixel exposure e0.
 * Structurally hue-preserving (only a scalar exposure reaches the picture; the base is hue-stable),
 * and MONOTONE by construction.
 *
 * strength = the compression gain k. strength<=0 => e0=0 => the EXACT pointwise baseline. Otherwise
 * e0 = -cap*tanh(k*(z-zbar)/cap) is a global soft toe+shoulder (z=log2(max x); zbar = mean of the
 * non-near-black carrier, so deep background/letterbox does not drag the pivot). d(z+e0)/dz = 1 -
 * k*sech^2 >= 1-k > 0, so z -> z+e0 is STRICTLY increasing: the output carrier tone(2^(z+e0)) is
 * monotone in the scene carrier z -- no carrier-order inversion, no halo, no enclosed-region lift.
 * It burns highlights (recovering blown chroma) and gently lifts shadows within +/-cap.
 *
 * NOTE: strict monotonicity in z means the output is a function of z alone -- i.e. a GLOBAL tone
 * curve; genuine local (surround-dependent) adaptation is mathematically incompatible with it, so
 * this operator is intentionally global. 'iterations' is reserved for a future NON-monotone,
 * evidence-driven target (which would re-introduce an iterative order-repair solve).
 *
 * pivot (linear carrier units): > 0 => FIXED pivot at log2(pivot) (e.g. 0.18 = the mid-grey anchor)
 * -- the curve becomes an absolute per-pixel map, frame-independent: TCONST/INVR-clean (no framewise
 * auto-exposure, no flicker). <= 0 => scene-adaptive pivot (mean of the non-near-black carrier);
 * better centring per frame, but the pivot shifts with content, so video wants the fixed mode. */
/* Core shared by the picture call and the field call: out and e_out may each be NULL, not both. */
static alwan_status ALWAN_CORE_FN(alwan__global_exp_core)(ALWAN_CORE_T *out, ALWAN_CORE_T *e_out, ALWAN_CORE_T const *in, int W, int H, int iterations, ALWAN_CORE_T strength, ALWAN_CORE_T pivot, alwan_ctx *ctx){
    if ((!out && !e_out) || !in || W <= 0 || H <= 0 || !ctx) return ALWAN_E_INVALID;
    (void)iterations;                                  /* reserved (see note above) */
    size_t N = (size_t)W * (size_t)H;
    ALWAN_CORE_T *xe = (ALWAN_CORE_T*)ctx->alloc_fn(N*3*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *zM = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *e0 = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *own = out ? NULL : (ALWAN_CORE_T*)ctx->alloc_fn(N*3*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *dst = out ? out : own;
    int ok = xe && zM && e0 && dst, st = ALWAN_OK;
    if (!ok) { st = ALWAN_E_NOMEM; goto done; }

    for (size_t i = 0; i < N; i++) {
        ALWAN_CORE_T m = in[i*3]; if (in[i*3+1] > m) m = in[i*3+1]; if (in[i*3+2] > m) m = in[i*3+2];
        if (m < ALWAN_CORE_LITERAL(1e-6)) m = ALWAN_CORE_LITERAL(1e-6);
        zM[i] = ALWAN_CORE_LOG2(m); e0[i] = ALWAN_CORE_LITERAL(0.0);
    }
    {
    ALWAN_CORE_T kad = (ALWAN_CORE_T)strength; if (kad < ALWAN_CORE_LITERAL(0.0)) kad = ALWAN_CORE_LITERAL(0.0);
    if (kad > ALWAN_CORE_LITERAL(1e-6)) {
        ALWAN_CORE_T const e0cap = ALWAN_CORE_LITERAL(1.0);          /* bound on the drive, in stops */
        ALWAN_CORE_T const zfloor = ALWAN_CORE_LITERAL(-7.64);      /* ~ log2(5e-3): below = deep black, excluded from pivot */
        double sum = 0.0; size_t cnt = 0, i;
        ALWAN_CORE_T zbar;
        if (pivot > 0.0) zbar = ALWAN_CORE_LOG2((ALWAN_CORE_T)pivot);      /* fixed anchor: TCONST-clean */
        else {
        for (i = 0; i < N; i++) if (zM[i] > zfloor) { sum += (double)zM[i]; cnt++; }
        if (cnt > 0) zbar = (ALWAN_CORE_T)(sum / (double)cnt);
        else { double s2 = 0.0; for (i = 0; i < N; i++) s2 += (double)zM[i]; zbar = (ALWAN_CORE_T)(s2 / (double)N); }
        }
        for (i = 0; i < N; i++) {
            ALWAN_CORE_T u = kad * (zM[i] - zbar) / e0cap;
            if (u > ALWAN_CORE_LITERAL(20.0)) u = ALWAN_CORE_LITERAL(20.0); if (u < ALWAN_CORE_LITERAL(-20.0)) u = ALWAN_CORE_LITERAL(-20.0);
            ALWAN_CORE_T ex = ALWAN_CORE_EXP(ALWAN_CORE_LITERAL(2.0) * u);
            ALWAN_CORE_T th = (ex - ALWAN_CORE_LITERAL(1.0)) / (ex + ALWAN_CORE_LITERAL(1.0)); /* tanh(u) */
            e0[i] = -e0cap * th;
        }
    }
    }
    for (size_t i = 0; i < N; i++) {
        ALWAN_CORE_T s = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(2.0), e0[i]);
        for (int k = 0; k < 3; k++) xe[i*3+k] = s * in[i*3+k];
    }
    st = ALWAN_CORE_FN(alwan__exp_form_base)(dst, xe, W, H, ctx);
    if (st == ALWAN_OK && e_out) memcpy(e_out, e0, N*sizeof(ALWAN_CORE_T));
done:
    ctx->free_fn(xe); ctx->free_fn(zM); ctx->free_fn(e0); if (own) ctx->free_fn(own);
    return st;
}
alwan_status ALWAN_CORE_FN(alwan_picture_form_global_exp)(ALWAN_CORE_T *out, ALWAN_CORE_T const *in, int W, int H, int iterations, ALWAN_CORE_T strength, ALWAN_CORE_T pivot, alwan_ctx *ctx){
    if (!out) return ALWAN_E_INVALID;
    return ALWAN_CORE_FN(alwan__global_exp_core)(out, NULL, in, W, H, iterations, strength, pivot, ctx);
}
/* Same solve, and the exposure field it used: e_out is W*H scalars in stops. out may be NULL. */
alwan_status ALWAN_CORE_FN(alwan_picture_form_global_exp_field)(ALWAN_CORE_T *e_out, ALWAN_CORE_T *out, ALWAN_CORE_T const *in, int W, int H, int iterations, ALWAN_CORE_T strength, ALWAN_CORE_T pivot, alwan_ctx *ctx){
    if (!e_out) return ALWAN_E_INVALID;
    return ALWAN_CORE_FN(alwan__global_exp_core)(out, e_out, in, W, H, iterations, strength, pivot, ctx);
}

/* Public: EVIDENCE-GATED local exposure operator.  Y = F_base(2^e X), where the exposure field e
 * adapts REGION-locally, with region structure taken from the junction evidence (occlusion P_O), and
 * carrier order on reliable edges repaired by an increasing-penalty solve.
 *
 * This is the deliberate NON-monotone counterpart of alwan_picture_form_global_exp: strict
 * z-monotonicity (= a global curve) is given up so that adaptation can follow SCENE ORGANIZATION --
 * the smoothing that produces the adaptation base is cut at occlusion boundaries (continuation
 * weights 1-P_O), so each illuminated volume adapts coherently and adaptation does not bleed across
 * silhouettes. What replaces monotonicity as the safety rail is the explicit CARRIER-ORDER constraint:
 * sign(dz)*dC >= 0 on reliable edges (|dz| > 0.05 stop), enforced by a penalty solve with schedule
 * rho: 30 -> 3000 (the Phase-2 machinery, now driven by evidence-gated structure). Hue safety is
 * unchanged and structural: only a scalar exposure reaches the hue-stable base.
 *
 * strength = adaptation gain k (e0 = -k*(base - mean), |e0| <= 1 stop); <= 0 => exact baseline.
 * iterations = order-repair iterations (<= 0 -> default 60). pivot: > 0 => the drive is centred at
 * the FIXED log2(pivot) (removes the frame-derived pivot -- the GLOBAL half of the TCONST gap;
 * the operator stays spatially local, so full TCONST additionally needs temporal machinery);
 * <= 0 => scene-adaptive centring. Interleaved RGB, W*H*3. */
static alwan_status ALWAN_CORE_FN(alwan__local_exp_core)(ALWAN_CORE_T *out, ALWAN_CORE_T *e_out, ALWAN_CORE_T const *in, int W, int H, int iterations, ALWAN_CORE_T strength, ALWAN_CORE_T pivot, alwan_ctx *ctx){
    if ((!out && !e_out) || !in || W <= 0 || H <= 0 || !ctx) return ALWAN_E_INVALID;
    if (iterations <= 0) iterations = 60;
    size_t N = (size_t)W * (size_t)H;
    ALWAN_CORE_T *own = out ? NULL : (ALWAN_CORE_T*)ctx->alloc_fn(N*3*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *dst = out ? out : own;
    ALWAN_CORE_T *zM  = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *PO  = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *csh = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *csv = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *tmp = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *bs  = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *bs2 = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *e0  = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *e   = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *xe  = (ALWAN_CORE_T*)ctx->alloc_fn(N*3*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *Y   = (ALWAN_CORE_T*)ctx->alloc_fn(N*3*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *C   = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *Cp  = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *Cm  = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *Jc  = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *g   = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    int ok = zM && PO && csh && csv && tmp && bs && bs2 && e0 && e && xe && Y && C && Cp && Cm && Jc && g && dst, st = ALWAN_OK;
    if (!ok) { st = ALWAN_E_NOMEM; goto done; }

    for (size_t i = 0; i < N; i++) {
        ALWAN_CORE_T m = in[i*3]; if (in[i*3+1] > m) m = in[i*3+1]; if (in[i*3+2] > m) m = in[i*3+2];
        if (m < ALWAN_CORE_LITERAL(1e-6)) m = ALWAN_CORE_LITERAL(1e-6);
        zM[i] = ALWAN_CORE_LOG2(m); e0[i] = ALWAN_CORE_LITERAL(0.0);
    }
    {
    ALWAN_CORE_T kad = (ALWAN_CORE_T)strength; if (kad < ALWAN_CORE_LITERAL(0.0)) kad = ALWAN_CORE_LITERAL(0.0);
    if (kad > ALWAN_CORE_LITERAL(1e-6)) {
        /* --- evidence: occlusion field, and continuation weights on lattice edges --- */
        ALWAN_CORE_FN(alwan__exp_occlusion)(PO, zM, W, H);
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            size_t i = (size_t)y*W + x;
            ALWAN_CORE_T ph = (x < W-1) ? (PO[i] > PO[i+1] ? PO[i] : PO[i+1]) : ALWAN_CORE_LITERAL(1.0);
            ALWAN_CORE_T pv = (y < H-1) ? (PO[i] > PO[i+W] ? PO[i] : PO[i+W]) : ALWAN_CORE_LITERAL(1.0);
            csh[i] = ALWAN_CORE_LITERAL(1.0) - ph; csv[i] = ALWAN_CORE_LITERAL(1.0) - pv;
        }
        for (int p = 0; p < 3; p++) {   /* soften the gates a touch (0.6*cs + 0.4*blur(cs)) */
            memcpy(bs, csh, N*sizeof(ALWAN_CORE_T)); ALWAN_CORE_FN(alwan__exp_boxblur)(bs, tmp, W, H, 1, 1);
            for (size_t i = 0; i < N; i++) csh[i] = ALWAN_CORE_LITERAL(0.6)*csh[i] + ALWAN_CORE_LITERAL(0.4)*bs[i];
            memcpy(bs, csv, N*sizeof(ALWAN_CORE_T)); ALWAN_CORE_FN(alwan__exp_boxblur)(bs, tmp, W, H, 1, 1);
            for (size_t i = 0; i < N; i++) csv[i] = ALWAN_CORE_LITERAL(0.6)*csv[i] + ALWAN_CORE_LITERAL(0.4)*bs[i];
        }
        /* --- evidence-gated adaptation base: weighted Jacobi smoothing of z, cut at occlusion --- */
        {
        ALWAN_CORE_T const ld = ALWAN_CORE_LITERAL(1.0), ls = ALWAN_CORE_LITERAL(16.0);
        memcpy(bs, zM, N*sizeof(ALWAN_CORE_T));
        for (int s2 = 0; s2 < 200; s2++) {
            for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
                size_t i = (size_t)y*W + x;
                ALWAN_CORE_T acc = ALWAN_CORE_LITERAL(0.0), wsum = ALWAN_CORE_LITERAL(0.0), w;
                if (x > 0)   { w = csh[i-1]; w *= w; acc += w*bs[i-1]; wsum += w; }
                if (x < W-1) { w = csh[i];   w *= w; acc += w*bs[i+1]; wsum += w; }
                if (y > 0)   { w = csv[i-W]; w *= w; acc += w*bs[i-W]; wsum += w; }
                if (y < H-1) { w = csv[i];   w *= w; acc += w*bs[i+W]; wsum += w; }
                bs2[i] = (ld*zM[i] + ls*acc) / (ld + ls*wsum);
            }
            { ALWAN_CORE_T *t2 = bs; bs = bs2; bs2 = t2; }
        }
        }
        /* --- e0 = -k*(base - pivot), capped; pivot fixed (TCONST) or robust scene mean --- */
        {
        ALWAN_CORE_T const e0cap = ALWAN_CORE_LITERAL(1.0), zfloor = ALWAN_CORE_LITERAL(-7.64);
        double sum = 0.0; size_t cnt = 0, i;
        if (pivot <= 0.0) for (i = 0; i < N; i++) if (zM[i] > zfloor) { sum += (double)bs[i]; cnt++; }
        ALWAN_CORE_T bbar;
        if (pivot > 0.0) { bbar = ALWAN_CORE_LOG2((ALWAN_CORE_T)pivot); goto have_bbar; }
        if (cnt > 0) bbar = (ALWAN_CORE_T)(sum / (double)cnt);
        else { double s3 = 0.0; for (i = 0; i < N; i++) s3 += (double)bs[i]; bbar = (ALWAN_CORE_T)(s3 / (double)N); }
have_bbar:
        for (i = 0; i < N; i++) {
            ALWAN_CORE_T v = -kad * (bs[i] - bbar);
            if (v > e0cap) v = e0cap; if (v < -e0cap) v = -e0cap; e0[i] = v;
        }
        }
    }
    }
    /* --- carrier-order repair: increasing-penalty gradient solve, anchored to e0 --- */
    for (size_t i = 0; i < N; i++) e[i] = e0[i];
    {
    ALWAN_CORE_T const hh = ALWAN_CORE_LITERAL(0.05), aw = ALWAN_CORE_LITERAL(2.0);
    ALWAN_CORE_T const lr = ALWAN_CORE_LITERAL(0.02), step_cap = ALWAN_CORE_LITERAL(0.15);
    for (int it = 0; it < iterations; it++) {
        ALWAN_CORE_T lrho = ALWAN_CORE_LITERAL(30.0) * ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0),
                            ALWAN_CORE_LITERAL(2.0) * ((ALWAN_CORE_T)it) / (ALWAN_CORE_T)(iterations > 1 ? iterations - 1 : 1)); /* 30 -> 3000 */
        for (size_t i = 0; i < N; i++) { ALWAN_CORE_T s = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(2.0), e[i]);    for (int k=0;k<3;k++) xe[i*3+k]=s*in[i*3+k]; }
        st = ALWAN_CORE_FN(alwan__exp_form_base)(Y, xe, W, H, ctx); if (st != ALWAN_OK) goto done;
        for (size_t i = 0; i < N; i++) { ALWAN_CORE_T m = Y[i*3]; if (Y[i*3+1]>m) m=Y[i*3+1]; if (Y[i*3+2]>m) m=Y[i*3+2]; C[i]=m; }
        for (size_t i = 0; i < N; i++) { ALWAN_CORE_T s = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(2.0), e[i]+hh); for (int k=0;k<3;k++) xe[i*3+k]=s*in[i*3+k]; }
        st = ALWAN_CORE_FN(alwan__exp_form_base)(Y, xe, W, H, ctx); if (st != ALWAN_OK) goto done;
        for (size_t i = 0; i < N; i++) { ALWAN_CORE_T m = Y[i*3]; if (Y[i*3+1]>m) m=Y[i*3+1]; if (Y[i*3+2]>m) m=Y[i*3+2]; Cp[i]=m; }
        for (size_t i = 0; i < N; i++) { ALWAN_CORE_T s = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(2.0), e[i]-hh); for (int k=0;k<3;k++) xe[i*3+k]=s*in[i*3+k]; }
        st = ALWAN_CORE_FN(alwan__exp_form_base)(Y, xe, W, H, ctx); if (st != ALWAN_OK) goto done;
        for (size_t i = 0; i < N; i++) { ALWAN_CORE_T m = Y[i*3]; if (Y[i*3+1]>m) m=Y[i*3+1]; if (Y[i*3+2]>m) m=Y[i*3+2]; Cm[i]=m; }
        for (size_t i = 0; i < N; i++) Jc[i] = (Cp[i] - Cm[i]) / (ALWAN_CORE_LITERAL(2.0)*hh);
        /* weak anchor to the evidence target */
        for (size_t i = 0; i < N; i++) g[i] = aw*(e[i]-e0[i]);
        /* carrier-order on reliable edges: sign(dz)*dC >= 0 */
        for (int y = 0; y < H; y++) for (int x = 0; x < W-1; x++) {
            size_t i = (size_t)y*W + x, j = i+1; ALWAN_CORE_T dz = zM[i]-zM[j];
            if (dz > ALWAN_CORE_LITERAL(0.05) || dz < ALWAN_CORE_LITERAL(-0.05)) {
                ALWAN_CORE_T s = dz > ALWAN_CORE_LITERAL(0.0) ? ALWAN_CORE_LITERAL(1.0) : ALWAN_CORE_LITERAL(-1.0);
                ALWAN_CORE_T gr = s*(C[i]-C[j]);
                if (gr < ALWAN_CORE_LITERAL(0.0)) { ALWAN_CORE_T co = ALWAN_CORE_LITERAL(2.0)*lrho*gr*s; g[i]+=co*Jc[i]; g[j]-=co*Jc[j]; }
            }
        }
        for (int y = 0; y < H-1; y++) for (int x = 0; x < W; x++) {
            size_t i = (size_t)y*W + x, j = i+W; ALWAN_CORE_T dz = zM[i]-zM[j];
            if (dz > ALWAN_CORE_LITERAL(0.05) || dz < ALWAN_CORE_LITERAL(-0.05)) {
                ALWAN_CORE_T s = dz > ALWAN_CORE_LITERAL(0.0) ? ALWAN_CORE_LITERAL(1.0) : ALWAN_CORE_LITERAL(-1.0);
                ALWAN_CORE_T gr = s*(C[i]-C[j]);
                if (gr < ALWAN_CORE_LITERAL(0.0)) { ALWAN_CORE_T co = ALWAN_CORE_LITERAL(2.0)*lrho*gr*s; g[i]+=co*Jc[i]; g[j]-=co*Jc[j]; }
            }
        }
        for (size_t i = 0; i < N; i++) {
            ALWAN_CORE_T d = lr*g[i]; if (d > step_cap) d = step_cap; if (d < -step_cap) d = -step_cap;
            ALWAN_CORE_T ne = e[i] - d; if (ne < ALWAN_CORE_LITERAL(-3.0)) ne = ALWAN_CORE_LITERAL(-3.0); if (ne > ALWAN_CORE_LITERAL(3.0)) ne = ALWAN_CORE_LITERAL(3.0); e[i] = ne;
        }
    }
    }
    for (size_t i = 0; i < N; i++) { ALWAN_CORE_T s = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(2.0), e[i]); for (int k=0;k<3;k++) xe[i*3+k]=s*in[i*3+k]; }
    st = ALWAN_CORE_FN(alwan__exp_form_base)(dst, xe, W, H, ctx);
    if (st == ALWAN_OK && e_out) memcpy(e_out, e, N*sizeof(ALWAN_CORE_T));
done:
    ctx->free_fn(zM); ctx->free_fn(PO); ctx->free_fn(csh); ctx->free_fn(csv); ctx->free_fn(tmp);
    ctx->free_fn(bs); ctx->free_fn(bs2); ctx->free_fn(e0); ctx->free_fn(e); ctx->free_fn(xe);
    ctx->free_fn(Y); ctx->free_fn(C); ctx->free_fn(Cp); ctx->free_fn(Cm); ctx->free_fn(Jc); ctx->free_fn(g);
    if (own) ctx->free_fn(own);
    return st;
}
alwan_status ALWAN_CORE_FN(alwan_picture_form_local_exp)(ALWAN_CORE_T *out, ALWAN_CORE_T const *in, int W, int H, int iterations, ALWAN_CORE_T strength, ALWAN_CORE_T pivot, alwan_ctx *ctx){
    if (!out) return ALWAN_E_INVALID;
    return ALWAN_CORE_FN(alwan__local_exp_core)(out, NULL, in, W, H, iterations, strength, pivot, ctx);
}
/* Same solve, and the exposure field it converged to: e_out is W*H scalars in stops. out may be NULL. */
alwan_status ALWAN_CORE_FN(alwan_picture_form_local_exp_field)(ALWAN_CORE_T *e_out, ALWAN_CORE_T *out, ALWAN_CORE_T const *in, int W, int H, int iterations, ALWAN_CORE_T strength, ALWAN_CORE_T pivot, alwan_ctx *ctx){
    if (!e_out) return ALWAN_E_INVALID;
    return ALWAN_CORE_FN(alwan__local_exp_core)(out, e_out, in, W, H, iterations, strength, pivot, ctx);
}

/* Public: PURE-corrected formation.  COMPLETE_HEMI_LOOK's reconstruction with the purity rolloff
 * GATED BY EMISSION EVIDENCE: white-infinity is applied only where the record is evidentially
 * additive/emissive (junction evidence P_T = dark-channel lift, gated to non-occluding pixels); an
 * intensely LIT SURFACE (bright but with a dark channel -- e.g. red skin under strong light) keeps
 * its chroma instead of collapsing pastel. This implements Troy's correction "PURE = emission only"
 * (energy != cause != role): the per-record purity shelf was the pastel failure; the additive
 * component is what approaches the white-infinity.
 *
 * Structural guarantees, EXACT by construction:
 *   - carrier: max(out) = ms = tone(max(in)) regardless of the gate (keep only scales chroma), so
 *     ORDER/MONO are identical to COMPLETE_HEMI_LOOK's;
 *   - hue: out - ms*1 = keep*sc*(in - max(in)*1) is parallel to the input chroma direction for any
 *     keep, so DRIFT/RATIO hold exactly;
 *   - below the purity shelf (carrier under the rolloff window) keep == km either way, so the
 *     picture equals COMPLETE_HEMI_LOOK exactly -- the gate only acts on bright records.
 * Honest limit: P_T detects NEUTRAL-ISH additive light (glow/veil/flare lifting the dark channel);
 * a fully saturated emitter with no glow keeps chroma until its bloom lifts the dark channel --
 * which is exactly the evidence a real emitter provides.
 *
 * REGIONAL COHERENCE (the INTG/Phase-4 lesson): the raw per-pixel P_T flips verdict where one object
 * crosses another (a coloured blade crossing a background tube light picks up the tube's dark-channel
 * lift -> a white hole mid-blade). Emission-vs-surface is a verdict about the OBJECT, not the pixel:
 * the gate field is therefore SMOOTHED WITHIN CONTINUATION REGIONS, cut at occlusion boundaries
 * (weights 1-P_O), so each coherent structure receives ONE verdict along its length -- the blade's
 * dominant surface-like evidence carries through the crossing, and a genuine glow region keeps its
 * emissive verdict. Carrier and hue guarantees are untouched (the gate still only scales chroma).
 * Interleaved RGB, W*H*3. */
alwan_status ALWAN_CORE_FN(alwan_picture_form_pure_exp)(ALWAN_CORE_T *out, ALWAN_CORE_T const *in, int W, int H, alwan_ctx *ctx){
    if (!out || !in || W <= 0 || H <= 0 || !ctx) return ALWAN_E_INVALID;
    size_t N = (size_t)W * (size_t)H;
    ALWAN_CORE_T *ev  = (ALWAN_CORE_T*)ctx->alloc_fn(N*3*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *aT  = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *aB  = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *csh = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *csv = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *tmp = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *coh = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *em  = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    ALWAN_CORE_T *eb  = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
    if (!ev || !aT || !aB || !csh || !csv || !tmp || !coh || !em || !eb) {
        if (ev) ctx->free_fn(ev); if (aT) ctx->free_fn(aT); if (aB) ctx->free_fn(aB);
        if (csh) ctx->free_fn(csh); if (csv) ctx->free_fn(csv); if (tmp) ctx->free_fn(tmp);
        if (coh) ctx->free_fn(coh); if (em) ctx->free_fn(em); if (eb) ctx->free_fn(eb);
        return ALWAN_E_NOMEM;
    }
    {
        int st = ALWAN_CORE_FN(alwan_picture_form_evidence)(ev, in, W, H, ctx);
        if (st != ALWAN_OK) { ctx->free_fn(ev); ctx->free_fn(aT); ctx->free_fn(aB); ctx->free_fn(csh); ctx->free_fn(csv); ctx->free_fn(tmp); ctx->free_fn(coh); ctx->free_fn(em); ctx->free_fn(eb); return st; }
    }
    /* --- PER-CHANNEL EMISSIVE COHERENCE (the diagnostic battery's missing measure) ---
     * A saturated pure-primary emitter is invisible to the dark-channel evidence (min ~ 0). Its
     * physical signature is elsewhere: the glow it radiates lifts the DARKEST nearby pixels in its
     * own channels, and that floor DECAYS with distance (a flat ambient field does not decay; a lit
     * surface has no halo of its own). Two factors, both required:
     *   Ex   = sum_k clip(erode_r12(x_k) - erode_r30(x_k), 0) * (x_k / max)   decaying own-colour floor
     *   head = log2(max / max_k erode_r12(x_k))                               SOURCE towers over its floor
     *   coh  = sstep(0.03, 0.15, Ex) * sstep(2.5, 4.0, head)
     * (thresholds calibrated on the saber/kids frames: blades coh~1, lit warm surfaces ~0 -- surfaces
     * lit BY a glow inherit the decaying floor but sit IN it, ~1.6 stops of headroom vs blades' ~4.7). */
    {
        for (size_t i = 0; i < N; i++) { coh[i] = ALWAN_CORE_LITERAL(0.0); em[i] = ALWAN_CORE_LITERAL(0.0); }
        for (int k = 0; k < 3; k++) {
            for (size_t i = 0; i < N; i++) { ALWAN_CORE_T v = in[i*3+k]; aT[i] = v > ALWAN_CORE_LITERAL(0.0) ? v : ALWAN_CORE_LITERAL(0.0); }
            ALWAN_CORE_FN(alwan__exp_erode)(aB, aT, tmp, W, H, 12);   /* e12_k */
            ALWAN_CORE_FN(alwan__exp_erode)(eb, aT, tmp, W, H, 30);   /* e30_k */
            for (size_t i = 0; i < N; i++) {
                ALWAN_CORE_T r = in[i*3] > ALWAN_CORE_LITERAL(0.0) ? in[i*3] : ALWAN_CORE_LITERAL(0.0);
                ALWAN_CORE_T g = in[i*3+1] > ALWAN_CORE_LITERAL(0.0) ? in[i*3+1] : ALWAN_CORE_LITERAL(0.0);
                ALWAN_CORE_T b = in[i*3+2] > ALWAN_CORE_LITERAL(0.0) ? in[i*3+2] : ALWAN_CORE_LITERAL(0.0);
                ALWAN_CORE_T mx = r > g ? (r > b ? r : b) : (g > b ? g : b); if (mx < ALWAN_CORE_LITERAL(1e-6)) mx = ALWAN_CORE_LITERAL(1e-6);
                ALWAN_CORE_T d = aB[i] - eb[i]; if (d < ALWAN_CORE_LITERAL(0.0)) d = ALWAN_CORE_LITERAL(0.0);
                coh[i] += d * (aT[i] / mx);
                if (aB[i] > em[i]) em[i] = aB[i];
            }
        }
        for (size_t i = 0; i < N; i++) {
            ALWAN_CORE_T r = in[i*3] > ALWAN_CORE_LITERAL(0.0) ? in[i*3] : ALWAN_CORE_LITERAL(0.0);
            ALWAN_CORE_T g = in[i*3+1] > ALWAN_CORE_LITERAL(0.0) ? in[i*3+1] : ALWAN_CORE_LITERAL(0.0);
            ALWAN_CORE_T b = in[i*3+2] > ALWAN_CORE_LITERAL(0.0) ? in[i*3+2] : ALWAN_CORE_LITERAL(0.0);
            ALWAN_CORE_T mx = r > g ? (r > b ? r : b) : (g > b ? g : b); if (mx < ALWAN_CORE_LITERAL(1e-6)) mx = ALWAN_CORE_LITERAL(1e-6);
            ALWAN_CORE_T fl = em[i]; if (fl < ALWAN_CORE_LITERAL(1e-4)) fl = ALWAN_CORE_LITERAL(1e-4);
            ALWAN_CORE_T head = ALWAN_CORE_LOG2(mx / fl);
            coh[i] = ALWAN_CORE_FN(alwan__exp_sstep)(ALWAN_CORE_LITERAL(0.03), ALWAN_CORE_LITERAL(0.15), coh[i])
                   * ALWAN_CORE_FN(alwan__exp_sstep)(ALWAN_CORE_LITERAL(2.5),  ALWAN_CORE_LITERAL(4.0),  head);
        }
    }
    /* --- regionally-coherent emission gate: smooth P_T within continuation, cut at occlusion ---
     * Gate refinement (the v4 transparency algebra): an ADDITIVE layer vanishes from boundary
     * evidence. A carrier step fully explained by the dark-channel (additive) lift is NOT an object
     * boundary -- the surface continues underneath. So edges are opened where the GLOW-DISCOUNTED
     * carriers zs = log2(max - blurred_min) agree on both sides AND both sides have a real surface
     * component (zs above a floor); a coloured blade therefore stays ONE region where it crosses a
     * background tube light. Neutral emitters (bulbs/tubes) have no surface component after the
     * discount, keep their raw occlusion boundary, and remain their own emissive regions. */
    {
        /* Discounted carrier zs = log2(max - min), UNBLURRED: the additive layer lifts the local
         * minimum pixel-wise, so max-min removes it exactly at the pixel where the boundary is being
         * judged (a blurred estimate under-discounts near zone edges and leaves a phantom step). */
        for (size_t i = 0; i < N; i++) {
            ALWAN_CORE_T r = in[i*3] > ALWAN_CORE_LITERAL(0.0) ? in[i*3] : ALWAN_CORE_LITERAL(0.0);
            ALWAN_CORE_T g = in[i*3+1] > ALWAN_CORE_LITERAL(0.0) ? in[i*3+1] : ALWAN_CORE_LITERAL(0.0);
            ALWAN_CORE_T b = in[i*3+2] > ALWAN_CORE_LITERAL(0.0) ? in[i*3+2] : ALWAN_CORE_LITERAL(0.0);
            ALWAN_CORE_T m = r > g ? (r > b ? r : b) : (g > b ? g : b);
            ALWAN_CORE_T mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
            ALWAN_CORE_T sm = m - mn; if (sm < ALWAN_CORE_LITERAL(1e-6)) sm = ALWAN_CORE_LITERAL(1e-6);
            aB[i] = ALWAN_CORE_LOG2(sm);                                   /* discounted carrier, in aB */
        }
        for (size_t i = 0; i < N; i++) aT[i] = ev[i*3+1];
        /* Edge verdict from the SURFACE-IDENTITY of the two sides (zs = discounted carrier):
         *   - both sides have a surface component and their zs agree  -> the step (if any) is
         *     additive-explained: OPEN (one object, the layer crosses it);
         *   - exactly one side has a surface component -> the surface ENDS there (an object over /
         *     under pure light): HARD boundary, CLOSE -- this is what stops a glow region's emissive
         *     verdict flooding into an object it merely crosses;
         *   - both sides surface but zs disagree strongly -> a real object boundary: CLOSE;
         *   - neither side has surface (neutral light on both sides) -> the raw occlusion gate. */
        /* raw occlusion gates first, softened -- then the surface-identity verdicts applied as HARD
         * overrides (softening a sealed identity boundary leaks the neighbouring basin's verdict
         * through cs^2 conductance and swamps the weak anchor -- measured on the emitter-bar test) */
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            size_t i = (size_t)y*W + x;
            ALWAN_CORE_T po_i = ev[i*3+2];
            ALWAN_CORE_T ph = (x < W-1) ? (po_i > ev[(i+1)*3+2] ? po_i : ev[(i+1)*3+2]) : ALWAN_CORE_LITERAL(1.0);
            ALWAN_CORE_T pv = (y < H-1) ? (po_i > ev[(i+(size_t)W)*3+2] ? po_i : ev[(i+(size_t)W)*3+2]) : ALWAN_CORE_LITERAL(1.0);
            csh[i] = ALWAN_CORE_LITERAL(1.0) - ph; csv[i] = ALWAN_CORE_LITERAL(1.0) - pv;
        }
        {
        ALWAN_CORE_T *sc2 = em;   /* coherence scratch fields are free now */
        for (int p = 0; p < 3; p++) {
            memcpy(sc2, csh, N*sizeof(ALWAN_CORE_T)); ALWAN_CORE_FN(alwan__exp_boxblur)(sc2, tmp, W, H, 1, 1);
            for (size_t i = 0; i < N; i++) csh[i] = ALWAN_CORE_LITERAL(0.6)*csh[i] + ALWAN_CORE_LITERAL(0.4)*sc2[i];
            memcpy(sc2, csv, N*sizeof(ALWAN_CORE_T)); ALWAN_CORE_FN(alwan__exp_boxblur)(sc2, tmp, W, H, 1, 1);
            for (size_t i = 0; i < N; i++) csv[i] = ALWAN_CORE_LITERAL(0.6)*csv[i] + ALWAN_CORE_LITERAL(0.4)*sc2[i];
        }
        }
        /* Identity verdicts as STEEP CLAMPED smoothsteps, not hard 0/1: sstep is exactly 0 / 1
         * outside its band, so clear cases stay hard-sealed (a blade boundary at dzs ~ 3.4 is
         * exactly 0), while a pixel NEAR a threshold gets a continuous, frame-stable intermediate
         * value instead of flipping verdict under sub-pixel content shifts (TCONST: hard gates
         * measured up to 0.20 of temporal flicker under a synthetic pan; continuity removes the
         * flip, not the seal).
         *   s_i    = sstep(zs floor band)                     surface-ness of each side
         *   agree  = 1 - sstep(0.55, 0.85, |dzs|)             additive-explained agreement
         *   cs     = s_i*s_j*agree + (1-s_i)*(1-s_j)*cs_raw   mixed identity contributes 0 (sealed) */
        {
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            size_t i = (size_t)y*W + x;
            ALWAN_CORE_T si = ALWAN_CORE_FN(alwan__exp_sstep)(ALWAN_CORE_LITERAL(-3.85), ALWAN_CORE_LITERAL(-2.85), aB[i]);
            if (x < W-1) {
                ALWAN_CORE_T sj = ALWAN_CORE_FN(alwan__exp_sstep)(ALWAN_CORE_LITERAL(-3.85), ALWAN_CORE_LITERAL(-2.85), aB[i+1]);
                ALWAN_CORE_T d = aB[i]-aB[i+1]; if (d < ALWAN_CORE_LITERAL(0.0)) d = -d;
                ALWAN_CORE_T agree = ALWAN_CORE_LITERAL(1.0) - ALWAN_CORE_FN(alwan__exp_sstep)(ALWAN_CORE_LITERAL(0.55), ALWAN_CORE_LITERAL(0.85), d);
                csh[i] = si*sj*agree + (ALWAN_CORE_LITERAL(1.0)-si)*(ALWAN_CORE_LITERAL(1.0)-sj)*csh[i];
            }
            if (y < H-1) {
                ALWAN_CORE_T sj = ALWAN_CORE_FN(alwan__exp_sstep)(ALWAN_CORE_LITERAL(-3.85), ALWAN_CORE_LITERAL(-2.85), aB[i+(size_t)W]);
                ALWAN_CORE_T d = aB[i]-aB[i+(size_t)W]; if (d < ALWAN_CORE_LITERAL(0.0)) d = -d;
                ALWAN_CORE_T agree = ALWAN_CORE_LITERAL(1.0) - ALWAN_CORE_FN(alwan__exp_sstep)(ALWAN_CORE_LITERAL(0.55), ALWAN_CORE_LITERAL(0.85), d);
                csv[i] = si*sj*agree + (ALWAN_CORE_LITERAL(1.0)-si)*(ALWAN_CORE_LITERAL(1.0)-sj)*csv[i];
            }
        }
        }
        /* Anchor field: transmission evidence from the UNBLURRED min. The blurred min (right for
         * detection robustness in P_T) smears the emissive verdict ~sigma px into a crossed object,
         * contaminating the anchors on the object side of a sealed boundary; the pixel-wise min does
         * not. Anchor = (1 - P_O) * sstep(0.30, 1.4, min RGB). */
        for (size_t i = 0; i < N; i++) {
            ALWAN_CORE_T r = in[i*3] > ALWAN_CORE_LITERAL(0.0) ? in[i*3] : ALWAN_CORE_LITERAL(0.0);
            ALWAN_CORE_T g = in[i*3+1] > ALWAN_CORE_LITERAL(0.0) ? in[i*3+1] : ALWAN_CORE_LITERAL(0.0);
            ALWAN_CORE_T b = in[i*3+2] > ALWAN_CORE_LITERAL(0.0) ? in[i*3+2] : ALWAN_CORE_LITERAL(0.0);
            ALWAN_CORE_T mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
            ALWAN_CORE_T pt = (ALWAN_CORE_LITERAL(1.0) - ev[i*3+2]) * ALWAN_CORE_FN(alwan__exp_sstep)(ALWAN_CORE_LITERAL(0.30), ALWAN_CORE_LITERAL(1.4), mn);
            tmp[i] = pt > coh[i] ? pt : coh[i];   /* additive-glow OR per-channel emissive coherence */
        }
        /* weighted Jacobi: weak data anchor (long consensus range -- safe, boundaries are sealed)
         * so the REGIONAL consensus wins over per-pixel flips */
        {
        ALWAN_CORE_T const ld = ALWAN_CORE_LITERAL(0.05), ls = ALWAN_CORE_LITERAL(16.0);
        ALWAN_CORE_T *pa = aT, *pb = aB;
        memcpy(aT, tmp, N*sizeof(ALWAN_CORE_T));
        for (int s2 = 0; s2 < 200; s2++) {
            for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
                size_t i = (size_t)y*W + x;
                ALWAN_CORE_T acc = ALWAN_CORE_LITERAL(0.0), wsum = ALWAN_CORE_LITERAL(0.0), w;
                if (x > 0)   { w = csh[i-1]; w *= w; acc += w*pa[i-1]; wsum += w; }
                if (x < W-1) { w = csh[i];   w *= w; acc += w*pa[i+1]; wsum += w; }
                if (y > 0)   { w = csv[i-W]; w *= w; acc += w*pa[i-W]; wsum += w; }
                if (y < H-1) { w = csv[i];   w *= w; acc += w*pa[i+W]; wsum += w; }
                pb[i] = (ld*tmp[i] + ls*acc) / (ld + ls*wsum);
            }
            { ALWAN_CORE_T *t2 = pa; pa = pb; pb = t2; }
        }
        if (pa != aT) memcpy(aT, pa, N*sizeof(ALWAN_CORE_T));
        for (size_t i = 0; i < N; i++) { if (aT[i] < ALWAN_CORE_LITERAL(0.0)) aT[i] = ALWAN_CORE_LITERAL(0.0); if (aT[i] > ALWAN_CORE_LITERAL(1.0)) aT[i] = ALWAN_CORE_LITERAL(1.0); }
        }
    }
    {
    /* COMPLETE_HEMI_LOOK tone window (same constants as alwan__exp_tone) */
    ALWAN_CORE_T const gg = ALWAN_CORE_LITERAL(9.1419001), vp = ALWAN_CORE_LITERAL(0.60606061);
    ALWAN_CORE_T const wlo = ALWAN_CORE_LITERAL(-12.47393119), ww = ALWAN_CORE_LITERAL(16.5);
    ALWAN_CORE_T const km = ALWAN_CORE_LITERAL(0.997);
    ALWAN_CORE_T dlo = ALWAN_CORE_LITERAL(1.0) / (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_EXP(gg * vp));
    ALWAN_CORE_T dhi = ALWAN_CORE_LITERAL(1.0) / (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_EXP(-gg * (ALWAN_CORE_LITERAL(1.0) - vp)));
    ALWAN_CORE_T drng = dhi - dlo; if (drng < ALWAN_CORE_LITERAL(1e-6)) drng = ALWAN_CORE_LITERAL(1e-6);
    for (size_t i = 0; i < N; i++) {
        ALWAN_CORE_T r = in[i*3] > ALWAN_CORE_LITERAL(0.0) ? in[i*3] : ALWAN_CORE_LITERAL(0.0);
        ALWAN_CORE_T g = in[i*3+1] > ALWAN_CORE_LITERAL(0.0) ? in[i*3+1] : ALWAN_CORE_LITERAL(0.0);
        ALWAN_CORE_T b = in[i*3+2] > ALWAN_CORE_LITERAL(0.0) ? in[i*3+2] : ALWAN_CORE_LITERAL(0.0);
        ALWAN_CORE_T m = r > g ? (r > b ? r : b) : (g > b ? g : b);
        if (m <= ALWAN_CORE_LITERAL(1e-9)) { out[i*3]=ALWAN_CORE_LITERAL(0.0); out[i*3+1]=ALWAN_CORE_LITERAL(0.0); out[i*3+2]=ALWAN_CORE_LITERAL(0.0); continue; }
        ALWAN_CORE_T ms = ALWAN_CORE_FN(alwan__exp_tone)(m, dlo, drng);
        ALWAN_CORE_T sc = ms / m;
        /* per-record purity shelf (COMPLETE_HEMI_LOOK's) */
        ALWAN_CORE_T vpos = (ALWAN_CORE_LOG2(m) - wlo) / ww;
        ALWAN_CORE_T tt = (vpos - ALWAN_CORE_LITERAL(0.72)) / ALWAN_CORE_LITERAL(0.48);
        if (tt < ALWAN_CORE_LITERAL(0.0)) tt = ALWAN_CORE_LITERAL(0.0); if (tt > ALWAN_CORE_LITERAL(1.0)) tt = ALWAN_CORE_LITERAL(1.0);
        ALWAN_CORE_T kroll = ALWAN_CORE_LITERAL(0.5) * (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_COS(ALWAN_CORE_LITERAL(3.14159265358979) * tt));
        if (kroll > km) kroll = km;
        /* emission-evidence gate (REGIONALLY COHERENT): surfaces keep chroma, emissive regions roll */
        ALWAN_CORE_T pt = aT[i];
        ALWAN_CORE_T keep = (ALWAN_CORE_LITERAL(1.0) - pt) * km + pt * kroll;
        ALWAN_CORE_T o0 = ms + (r*sc - ms) * keep;
        ALWAN_CORE_T o1 = ms + (g*sc - ms) * keep;
        ALWAN_CORE_T o2 = ms + (b*sc - ms) * keep;
        if (o0 < ALWAN_CORE_LITERAL(0.0)) o0 = ALWAN_CORE_LITERAL(0.0); if (o0 > ALWAN_CORE_LITERAL(1.0)) o0 = ALWAN_CORE_LITERAL(1.0);
        if (o1 < ALWAN_CORE_LITERAL(0.0)) o1 = ALWAN_CORE_LITERAL(0.0); if (o1 > ALWAN_CORE_LITERAL(1.0)) o1 = ALWAN_CORE_LITERAL(1.0);
        if (o2 < ALWAN_CORE_LITERAL(0.0)) o2 = ALWAN_CORE_LITERAL(0.0); if (o2 > ALWAN_CORE_LITERAL(1.0)) o2 = ALWAN_CORE_LITERAL(1.0);
        out[i*3] = o0; out[i*3+1] = o1; out[i*3+2] = o2;
    }
    }
    ctx->free_fn(ev); ctx->free_fn(aT); ctx->free_fn(aB); ctx->free_fn(csh); ctx->free_fn(csv); ctx->free_fn(tmp);
    ctx->free_fn(coh); ctx->free_fn(em); ctx->free_fn(eb);
    return ALWAN_OK;
}
