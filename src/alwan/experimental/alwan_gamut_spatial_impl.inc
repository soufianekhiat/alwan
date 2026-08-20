/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Spatial picture-formation gamut mapping -- precision-agnostic implementation.
 * Included twice by alwan_gamut.c (f32 and f64 passes). No include guard.
 * See docs/gamut_spatial_formation.md.
 */

/* 1D squared Euclidean distance transform (Felzenszwalb-Huttenlocher).
 * f = cost (0 at a site, large elsewhere); d = squared distance to nearest site. */
static void ALWAN_CORE_FN(alwan__edt_1d)(ALWAN_CORE_T const *f, ALWAN_CORE_T *d, int n, int *v, ALWAN_CORE_T *z) {
    ALWAN_CORE_T const INF = ALWAN_CORE_LITERAL(1e30);
    int k = 0;
    v[0] = 0; z[0] = -INF; z[1] = INF;
    for (int q = 1; q < n; q++) {
        ALWAN_CORE_T s = ((f[q] + (ALWAN_CORE_T)q*q) - (f[v[k]] + (ALWAN_CORE_T)v[k]*v[k])) / (ALWAN_CORE_LITERAL(2.0)*q - ALWAN_CORE_LITERAL(2.0)*v[k]);
        while (s <= z[k]) {
            k--;
            s = ((f[q] + (ALWAN_CORE_T)q*q) - (f[v[k]] + (ALWAN_CORE_T)v[k]*v[k])) / (ALWAN_CORE_LITERAL(2.0)*q - ALWAN_CORE_LITERAL(2.0)*v[k]);
        }
        k++; v[k] = q; z[k] = s; z[k+1] = INF;
    }
    k = 0;
    for (int q = 0; q < n; q++) {
        while (z[k+1] < q) k++;
        d[q] = (ALWAN_CORE_T)(q - v[k])*(q - v[k]) + f[v[k]];
    }
}

/* Full 2D EDT: dist2[i] = squared spatial distance to the nearest site pixel
 * (site = 1 in `mask`). Columns then rows (separable). */
static void ALWAN_CORE_FN(alwan__edt_2d)(const unsigned char *mask, ALWAN_CORE_T *dist2, int W, int H,
                          ALWAN_CORE_T *fbuf, ALWAN_CORE_T *dbuf, int *v, ALWAN_CORE_T *z) {
    ALWAN_CORE_T const INF = ALWAN_CORE_LITERAL(1e30);
    /* pass 1: columns */
    for (int x = 0; x < W; x++) {
        for (int y = 0; y < H; y++) fbuf[y] = mask[y*W + x] ? ALWAN_CORE_LITERAL(0.0) : INF;
        ALWAN_CORE_FN(alwan__edt_1d)(fbuf, dbuf, H, v, z);
        for (int y = 0; y < H; y++) dist2[y*W + x] = dbuf[y];
    }
    /* pass 2: rows (feed column results as the cost) */
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) fbuf[x] = dist2[y*W + x];
        ALWAN_CORE_FN(alwan__edt_1d)(fbuf, dbuf, W, v, z);
        for (int x = 0; x < W; x++) dist2[y*W + x] = dbuf[x];
    }
}

static ALWAN_CORE_T ALWAN_CORE_FN(alwan__smoothstep)(ALWAN_CORE_T t) {
    if (t <= ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(0.0);
    if (t >= ALWAN_CORE_LITERAL(1.0)) return ALWAN_CORE_LITERAL(1.0);
    return t*t*(ALWAN_CORE_LITERAL(3.0) - ALWAN_CORE_LITERAL(2.0)*t);
}

/* border-aware separable box mean (radius r); tmp is scratch of size W*H */
static void ALWAN_CORE_FN(alwan__boxfilter)(ALWAN_CORE_T const *src, ALWAN_CORE_T *dst, ALWAN_CORE_T *tmp, int W, int H, int r) {
    for (int y = 0; y < H; y++) { ALWAN_CORE_T const *row = src + (size_t)y*W; ALWAN_CORE_T *trow = tmp + (size_t)y*W;
        for (int x = 0; x < W; x++) { ALWAN_CORE_T sum = ALWAN_CORE_LITERAL(0.0); int x0 = x-r, x1 = x+r; if (x0<0)x0=0; if (x1>=W)x1=W-1;
            for (int xx = x0; xx <= x1; xx++) sum += row[xx]; trow[x] = sum / (ALWAN_CORE_T)(x1-x0+1); } }
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { ALWAN_CORE_T sum = ALWAN_CORE_LITERAL(0.0); int y0 = y-r, y1 = y+r; if (y0<0)y0=0; if (y1>=H)y1=H-1;
        for (int yy = y0; yy <= y1; yy++) sum += tmp[(size_t)yy*W + x]; dst[(size_t)y*W + x] = sum / (ALWAN_CORE_T)(y1-y0+1); }
}

static int ALWAN_CORE_FN(alwan__gamut_spatial_core)(ALWAN_CORE_T *out, ALWAN_CORE_T const *in, ALWAN_CORE_T const *depth,
                                     int W, int H, ALWAN_CORE_FN(alwan_gamut_spatial_params) const *p, alwan_ctx *ctx) {
    if (!out || !in || !p || W <= 0 || H <= 0 || !ctx) return ALWAN_E_INVALID;
    size_t N = (size_t)W * (size_t)H;

    ALWAN_CORE_T peak   = p->peak > ALWAN_CORE_LITERAL(0.0) ? p->peak : ALWAN_CORE_LITERAL(1.0);
    ALWAN_CORE_T reach  = p->reach > ALWAN_CORE_LITERAL(0.0) ? p->reach : ALWAN_CORE_SQRT((ALWAN_CORE_T)W*W + (ALWAN_CORE_T)H*H);
    ALWAN_CORE_T s      = p->s < ALWAN_CORE_LITERAL(0.0) ? ALWAN_CORE_LITERAL(0.0) : (p->s > ALWAN_CORE_LITERAL(1.0) ? ALWAN_CORE_LITERAL(1.0) : p->s);
    ALWAN_CORE_T beta   = p->beta > ALWAN_CORE_LITERAL(0.0) ? p->beta : ALWAN_CORE_LITERAL(8.0);
    ALWAN_CORE_T c      = p->compress > ALWAN_CORE_LITERAL(0.0) ? (p->compress > ALWAN_CORE_LITERAL(1.0) ? ALWAN_CORE_LITERAL(1.0) : p->compress) : ALWAN_CORE_LITERAL(1.0);
    ALWAN_CORE_T dsig   = p->depth_sigma;
    int iters        = p->iterations > 0 ? p->iterations : 200;
    ALWAN_CORE_T feather = ALWAN_CORE_LITERAL(0.15);              /* transition width of the s band */
    ALWAN_CORE_T lam0    = ALWAN_CORE_LITERAL(1.0);               /* base fidelity weight (locked pixels) */
    ALWAN_CORE_T eps     = peak * ALWAN_CORE_LITERAL(1e-9);
    (void)eps;

    /* ---- DENSITY: luminance-preserving chroma collapse (per pixel) ----
     * Keep luma (the surface); desaturate toward the equal-luma neutral until
     * every channel fits [0,peak]. lerp(rgb, (L,L,L), t) preserves luma exactly,
     * so the luminance gradient (and its sign) is untouched -> zero luma flips. */
    if (p->method == ALWAN_GAMUT_FORM_DENSITY) {
        ALWAN_CORE_T const wr = ALWAN_CORE_LUMA_KR_BT709, wg = ALWAN_CORE_LUMA_KG_BT709, wb = ALWAN_CORE_LUMA_KB_BT709;
        for (size_t i = 0; i < N; i++) {
            ALWAN_CORE_T r = in[i*3+0], g = in[i*3+1], b = in[i*3+2];
            ALWAN_CORE_T L = wr*r + wg*g + wb*b;
            if (L < ALWAN_CORE_LITERAL(0.0)) L = ALWAN_CORE_LITERAL(0.0);
            if (L > peak) { ALWAN_CORE_T sc = peak / L; r*=sc; g*=sc; b*=sc; L = peak; }
            ALWAN_CORE_T m  = r>g ? (r>b?r:b) : (g>b?g:b);
            ALWAN_CORE_T mn = r<g ? (r<b?r:b) : (g<b?g:b);
            ALWAN_CORE_T t = ALWAN_CORE_LITERAL(0.0), tt;
            if (m > peak && m > L)  { tt = (m - peak)/(m - L);  if (tt > t) t = tt; }
            if (mn < ALWAN_CORE_LITERAL(0.0) && L > mn) { tt = (ALWAN_CORE_LITERAL(0.0) - mn)/(L - mn); if (tt > t) t = tt; }
            if (t > ALWAN_CORE_LITERAL(1.0)) t = ALWAN_CORE_LITERAL(1.0);
            if (t > ALWAN_CORE_LITERAL(0.0)) t += s * (ALWAN_CORE_LITERAL(1.0) - t);       /* s = additive whitening: 0 = minimal fit (keep colour), 1 = full neutral */
            r += (L - r)*t; g += (L - g)*t; b += (L - b)*t;
            out[i*3+0] = r<0?0:(r>peak?peak:r);
            out[i*3+1] = g<0?0:(g>peak?peak:g);
            out[i*3+2] = b<0?0:(b>peak?peak:b);
        }
        return ALWAN_OK;
    }

    /* ---- SURFACE: DENSITY + surface-uniform chroma collapse ----
     * Same luminance-preserving chroma collapse as DENSITY (0 luma flips), but
     * the per-pixel chroma scale is smoothed *within* a surface (depth/segment
     * gated) so a surface desaturates uniformly instead of only at its hot
     * spots. Each pixel's scale is capped by its own gamut limit so the result
     * stays in gamut. A luminance shoulder handles over-range luma smoothly. */
    if (p->method == ALWAN_GAMUT_FORM_SURFACE) {
        ALWAN_CORE_T const wr = ALWAN_CORE_LUMA_KR_BT709, wg = ALWAN_CORE_LUMA_KG_BT709, wb = ALWAN_CORE_LUMA_KB_BT709;
        ALWAN_CORE_T *lsc   = (ALWAN_CORE_T *)ctx->alloc_fn(N * sizeof(ALWAN_CORE_T), 16);
        ALWAN_CORE_T *smaxf = (ALWAN_CORE_T *)ctx->alloc_fn(N * sizeof(ALWAN_CORE_T), 16);
        ALWAN_CORE_T *sf    = (ALWAN_CORE_T *)ctx->alloc_fn(N * sizeof(ALWAN_CORE_T), 16);
        ALWAN_CORE_T *sn    = (ALWAN_CORE_T *)ctx->alloc_fn(N * sizeof(ALWAN_CORE_T), 16);
        if (!lsc || !smaxf || !sf || !sn) { ctx->free_fn(lsc); ctx->free_fn(smaxf); ctx->free_fn(sf); ctx->free_fn(sn); return ALWAN_E_NOMEM; }
        for (size_t i = 0; i < N; i++) {
            ALWAN_CORE_T r = in[i*3+0], g = in[i*3+1], b = in[i*3+2];
            ALWAN_CORE_T L = wr*r + wg*g + wb*b;
            if (L < ALWAN_CORE_LITERAL(0.0)) L = ALWAN_CORE_LITERAL(0.0);
            /* luminance shoulder: monotone rolloff of over-range luma toward peak
             * (extended Reinhard), so the luma gradient stays ordered. */
            ALWAN_CORE_T lscale = ALWAN_CORE_LITERAL(1.0);
            if (L > peak) { ALWAN_CORE_T Lt = peak * (ALWAN_CORE_LITERAL(1.0) + L/(peak*peak)) / (ALWAN_CORE_LITERAL(1.0) + L/peak); lscale = Lt / L; }
            ALWAN_CORE_T r2 = r*lscale, g2 = g*lscale, b2 = b*lscale, L2 = L*lscale;
            ALWAN_CORE_T cr = r2-L2, cg = g2-L2, cb = b2-L2;
            ALWAN_CORE_T smax = ALWAN_CORE_LITERAL(1e30), l;
            if (cr >  ALWAN_CORE_LITERAL(1e-12)) { l = (peak-L2)/cr; if (l<smax) smax=l; } else if (cr < -ALWAN_CORE_LITERAL(1e-12)) { l = (ALWAN_CORE_LITERAL(0.0)-L2)/cr; if (l<smax) smax=l; }
            if (cg >  ALWAN_CORE_LITERAL(1e-12)) { l = (peak-L2)/cg; if (l<smax) smax=l; } else if (cg < -ALWAN_CORE_LITERAL(1e-12)) { l = (ALWAN_CORE_LITERAL(0.0)-L2)/cg; if (l<smax) smax=l; }
            if (cb >  ALWAN_CORE_LITERAL(1e-12)) { l = (peak-L2)/cb; if (l<smax) smax=l; } else if (cb < -ALWAN_CORE_LITERAL(1e-12)) { l = (ALWAN_CORE_LITERAL(0.0)-L2)/cb; if (l<smax) smax=l; }
            if (smax < ALWAN_CORE_LITERAL(0.0)) smax = ALWAN_CORE_LITERAL(0.0);
            lsc[i] = lscale; smaxf[i] = smax; sf[i] = smax < ALWAN_CORE_LITERAL(1.0) ? smax : ALWAN_CORE_LITERAL(1.0);
        }
        {
            int const dx4[4] = {-1,1,0,0}, dy4[4] = {0,0,-1,1};
            int sm = (int)((ALWAN_CORE_LITERAL(1.0) - s) * (ALWAN_CORE_T)(iters > 0 ? iters : 200));
            for (int it = 0; it < sm; it++) {
                for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
                    size_t i = (size_t)y*W + x;
                    ALWAN_CORE_T zi = depth ? depth[i] : ALWAN_CORE_LITERAL(0.0);
                    ALWAN_CORE_T acc = sf[i], wsum = ALWAN_CORE_LITERAL(1.0);    /* include self -> gentle box smoothing */
                    for (int n = 0; n < 4; n++) {
                        int nx=x+dx4[n], ny=y+dy4[n];
                        if (nx<0||nx>=W||ny<0||ny>=H) continue;
                        size_t j = (size_t)ny*W + nx;
                        ALWAN_CORE_T w = ALWAN_CORE_LITERAL(1.0);
                        if (dsig > ALWAN_CORE_LITERAL(0.0) && depth) { ALWAN_CORE_T dz = (zi - depth[j])/dsig; w = ALWAN_CORE_EXP(-dz*dz); }
                        acc += w*sf[j]; wsum += w;
                    }
                    ALWAN_CORE_T v = acc/wsum;
                    if (v > smaxf[i]) v = smaxf[i];       /* stay in gamut */
                    sn[i] = v;
                }
                { ALWAN_CORE_T *tmp = sf; sf = sn; sn = tmp; }
            }
        }
        for (size_t i = 0; i < N; i++) {
            ALWAN_CORE_T r = in[i*3+0]*lsc[i], g = in[i*3+1]*lsc[i], b = in[i*3+2]*lsc[i];
            ALWAN_CORE_T L2 = wr*r + wg*g + wb*b, ss = sf[i];
            ALWAN_CORE_T or_ = L2 + (r-L2)*ss, og = L2 + (g-L2)*ss, ob = L2 + (b-L2)*ss;
            out[i*3+0] = or_<0?0:(or_>peak?peak:or_);
            out[i*3+1] = og <0?0:(og >peak?peak:og);
            out[i*3+2] = ob <0?0:(ob >peak?peak:ob);
        }
        ctx->free_fn(lsc); ctx->free_fn(smaxf); ctx->free_fn(sf); ctx->free_fn(sn);
        return ALWAN_OK;
    }

    /* ---- CARRIER: carrier-conserving (Hamiltonian) ----
     * max(RGB) = luminance + chrominance is a conserved carrier. A per-pixel
     * monotone shoulder brings the carrier into gamut (order preserved -> 0
     * max(RGB) flips), then the non-max channels are refilled by blending a
     * ratio-preserving target (keep hue) with a neutral target (white); the
     * blend `a` is the additive/multiplicative split (bright + desaturated =
     * increment -> white, saturated = decrement -> keep hue). Both targets have
     * max == carrier, so the split never changes the carrier. Per-pixel: no
     * spatial coupling, so the envelope is never reversed. */
    if (p->method == ALWAN_GAMUT_FORM_CARRIER) {
        ALWAN_CORE_T const knee = ALWAN_CORE_LITERAL(0.80) * peak;
        ALWAN_CORE_T mmax = peak;                                   /* actual maximal emission in the image */
        for (size_t i = 0; i < N; i++) { ALWAN_CORE_T r=in[i*3],g=in[i*3+1],b=in[i*3+2]; ALWAN_CORE_T mi=r>g?(r>b?r:b):(g>b?g:b); if (mi>mmax) mmax=mi; }
        ALWAN_CORE_T span = mmax - peak; if (span < ALWAN_CORE_LITERAL(1e-6)) span = ALWAN_CORE_LITERAL(1e-6);
        for (size_t i = 0; i < N; i++) {
            ALWAN_CORE_T r = in[i*3+0], g = in[i*3+1], b = in[i*3+2];
            ALWAN_CORE_T m = r>g?(r>b?r:b):(g>b?g:b);
            if (m <= ALWAN_CORE_LITERAL(1e-9)) { out[i*3+0]=r<0?0:r; out[i*3+1]=g<0?0:g; out[i*3+2]=b<0?0:b; continue; }
            ALWAN_CORE_T mn = r<g?(r<b?r:b):(g<b?g:b); if (mn < ALWAN_CORE_LITERAL(0.0)) mn = ALWAN_CORE_LITERAL(0.0);
            ALWAN_CORE_T sat = (m - mn) / m;                            /* excitation purity (carrier-based, hue-agnostic) */
            /* monotone carrier shoulder: identity below knee, asymptotes to peak */
            ALWAN_CORE_T mp;
            if (m <= knee) mp = m;
            else mp = knee + (peak - knee) * (ALWAN_CORE_LITERAL(1.0) - ALWAN_CORE_EXP(-(m - knee)/(peak - knee)));
            /* increment weight: bright AND desaturated -> white */
            ALWAN_CORE_T bw = ALWAN_CORE_FN(alwan__smoothstep)((m - ALWAN_CORE_LITERAL(0.55)) / ALWAN_CORE_LITERAL(0.5));
            /* purity attenuation ramps LINEARLY 0 (at the gamut boundary) -> 1 (at the actual maximal
             * emission in the image), so the gradient collapses to R=G=B only at true max emission and
             * the collapse spreads over the whole flux range (gradual, not sharp; adaptive to how far
             * the input actually spans). Invariant depiction of the spatial volume (Troy). */
            ALWAN_CORE_T comp = (m - peak) / span; comp = comp<ALWAN_CORE_LITERAL(0.0)?ALWAN_CORE_LITERAL(0.0):(comp>ALWAN_CORE_LITERAL(1.0)?ALWAN_CORE_LITERAL(1.0):comp);
            ALWAN_CORE_T a = comp + s * bw * (ALWAN_CORE_LITERAL(1.0) - sat);               /* baseline purity attenuation + s*airlight bias */
            if (a > ALWAN_CORE_LITERAL(1.0)) a = ALWAN_CORE_LITERAL(1.0);
            ALWAN_CORE_T sc = mp / m;                                   /* ratio-preserving scale */
            ALWAN_CORE_T or_ = (r*sc)*(ALWAN_CORE_LITERAL(1.0)-a) + mp*a;
            ALWAN_CORE_T og  = (g*sc)*(ALWAN_CORE_LITERAL(1.0)-a) + mp*a;
            ALWAN_CORE_T ob  = (b*sc)*(ALWAN_CORE_LITERAL(1.0)-a) + mp*a;
            out[i*3+0] = or_<0?0:(or_>peak?peak:or_);
            out[i*3+1] = og <0?0:(og >peak?peak:og);
            out[i*3+2] = ob <0?0:(ob >peak?peak:ob);
        }
        return ALWAN_OK;
    }

    /* ---- XJUNCTION: CARRIER + real Kitaoka additive-veil (dark-channel) split ----
     * Same carrier-conserving core as CARRIER (per-pixel monotone shoulder -> 0
     * max(RGB) flips). The increment weight is a genuine spatial estimate: the
     * additive veil at a pixel = the windowed dark channel (min over a
     * neighbourhood of min(RGB)) = the Koschmieder airlight. High veil relative
     * to the carrier -> additive light -> white; near-zero veil -> multiplicative
     * absorption -> keep hue. Spatial, but only refills the non-max channels, so
     * the carrier order is untouched. */
    if (p->method == ALWAN_GAMUT_FORM_XJUNCTION) {
        ALWAN_CORE_T const knee = ALWAN_CORE_LITERAL(0.80) * peak; int const RAD = 6;
        ALWAN_CORE_T *minc = (ALWAN_CORE_T *)ctx->alloc_fn(N * sizeof(ALWAN_CORE_T), 16);
        ALWAN_CORE_T *veil = (ALWAN_CORE_T *)ctx->alloc_fn(N * sizeof(ALWAN_CORE_T), 16);
        if (!minc || !veil) { ctx->free_fn(minc); ctx->free_fn(veil); return ALWAN_E_NOMEM; }
        for (size_t i = 0; i < N; i++) {
            ALWAN_CORE_T r = in[i*3+0], g = in[i*3+1], b = in[i*3+2];
            if (r < 0) r = 0; if (g < 0) g = 0; if (b < 0) b = 0;
            ALWAN_CORE_T mn = r<g?(r<b?r:b):(g<b?g:b); minc[i] = mn;
        }
        ALWAN_CORE_T mmax = peak;
        for (size_t i = 0; i < N; i++) { ALWAN_CORE_T r=in[i*3],g=in[i*3+1],b=in[i*3+2]; ALWAN_CORE_T mi=r>g?(r>b?r:b):(g>b?g:b); if (mi>mmax) mmax=mi; }
        ALWAN_CORE_T span = mmax - peak; if (span < ALWAN_CORE_LITERAL(1e-6)) span = ALWAN_CORE_LITERAL(1e-6);
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            ALWAN_CORE_T v = ALWAN_CORE_LITERAL(1e30); int y0=y-RAD,y1=y+RAD,x0=x-RAD,x1=x+RAD;
            if (y0<0)y0=0; if (y1>=H)y1=H-1; if (x0<0)x0=0; if (x1>=W)x1=W-1;
            for (int yy=y0; yy<=y1; yy++) { ALWAN_CORE_T const *row = minc + (size_t)yy*W;
                for (int xx=x0; xx<=x1; xx++) if (row[xx] < v) v = row[xx]; }
            veil[(size_t)y*W + x] = v;
        }
        for (size_t i = 0; i < N; i++) {
            ALWAN_CORE_T r = in[i*3+0], g = in[i*3+1], b = in[i*3+2];
            ALWAN_CORE_T m = r>g?(r>b?r:b):(g>b?g:b);
            if (m <= ALWAN_CORE_LITERAL(1e-9)) { out[i*3+0]=r<0?0:r; out[i*3+1]=g<0?0:g; out[i*3+2]=b<0?0:b; continue; }
            ALWAN_CORE_T mp;
            if (m <= knee) mp = m;
            else mp = knee + (peak - knee) * (ALWAN_CORE_LITERAL(1.0) - ALWAN_CORE_EXP(-(m - knee)/(peak - knee)));
            ALWAN_CORE_T bw = ALWAN_CORE_FN(alwan__smoothstep)((m - knee) / ALWAN_CORE_LITERAL(0.6));
            ALWAN_CORE_T comp = (m - peak) / span; comp = comp<ALWAN_CORE_LITERAL(0.0)?ALWAN_CORE_LITERAL(0.0):(comp>ALWAN_CORE_LITERAL(1.0)?ALWAN_CORE_LITERAL(1.0):comp); /* linear -> 1 at actual maximal emission (R=G=B), gradual */
            ALWAN_CORE_T a = comp + s * bw * (veil[i] / m); if (a < ALWAN_CORE_LITERAL(0.0)) a = ALWAN_CORE_LITERAL(0.0); if (a > ALWAN_CORE_LITERAL(1.0)) a = ALWAN_CORE_LITERAL(1.0); /* purity attenuation + s*airlight */
            ALWAN_CORE_T sc = mp / m;
            ALWAN_CORE_T or_ = (r*sc)*(ALWAN_CORE_LITERAL(1.0)-a) + mp*a, og = (g*sc)*(ALWAN_CORE_LITERAL(1.0)-a) + mp*a, ob = (b*sc)*(ALWAN_CORE_LITERAL(1.0)-a) + mp*a;
            out[i*3+0] = or_<0?0:(or_>peak?peak:or_);
            out[i*3+1] = og <0?0:(og >peak?peak:og);
            out[i*3+2] = ob <0?0:(ob >peak?peak:ob);
        }
        ctx->free_fn(minc); ctx->free_fn(veil);
        return ALWAN_OK;
    }

    /* ---- FLUX: CARRIER + diffused-field additive veil (Grady-style) ----
     * Same carrier-conserving core as XJUNCTION, but the veil is not a box-window
     * min: it is the dark channel refined by a guided filter (a fast matting-
     * Laplacian / edge-aware diffusion). The veil becomes the smooth envelope the
     * picture sits on; the residual is the multiplicative surface. No boundary is
     * detected, so it reads boundary-free flux (clouds) natively. The diffused
     * veil only drives the L/C split, so the carrier stays per-pixel monotone
     * (0 max(RGB) flips). */
    if (p->method == ALWAN_GAMUT_FORM_FLUX) {
        ALWAN_CORE_T const knee = ALWAN_CORE_LITERAL(0.80) * peak; int const r = 12; ALWAN_CORE_T const geps = ALWAN_CORE_LITERAL(1e-3);
        ALWAN_CORE_T *minc=(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16), *I =(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        ALWAN_CORE_T *tmp =(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16), *mean_i=(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        ALWAN_CORE_T *mp =(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16), *mean_ip=(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        ALWAN_CORE_T *mean_ii =(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16), *A =(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        ALWAN_CORE_T *B  =(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16), *veil=(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        if (!minc||!I||!tmp||!mean_i||!mp||!mean_ip||!mean_ii||!A||!B||!veil) {
            ctx->free_fn(minc);ctx->free_fn(I);ctx->free_fn(tmp);ctx->free_fn(mean_i);ctx->free_fn(mp);
            ctx->free_fn(mean_ip);ctx->free_fn(mean_ii);ctx->free_fn(A);ctx->free_fn(B);ctx->free_fn(veil); return ALWAN_E_NOMEM; }
        for (size_t i=0;i<N;i++){ ALWAN_CORE_T rr=in[i*3],gg=in[i*3+1],bb=in[i*3+2];
            if(rr<0)rr=0; if(gg<0)gg=0; if(bb<0)bb=0; minc[i]=rr<gg?(rr<bb?rr:bb):(gg<bb?gg:bb); I[i]=rr>gg?(rr>bb?rr:bb):(gg>bb?gg:bb); }  /* veil guided by CARRIER max(RGB), not luminance (hue-agnostic) */
        /* guided filter: refine dark channel (minc) guided by luminance (I) */
        ALWAN_CORE_FN(alwan__boxfilter)(I, mean_i, tmp, W, H, r);
        ALWAN_CORE_FN(alwan__boxfilter)(minc, mp, tmp, W, H, r);
        for (size_t i=0;i<N;i++) veil[i]=I[i]*minc[i]; ALWAN_CORE_FN(alwan__boxfilter)(veil, mean_ip, tmp, W, H, r);
        for (size_t i=0;i<N;i++) veil[i]=I[i]*I[i];    ALWAN_CORE_FN(alwan__boxfilter)(veil, mean_ii,  tmp, W, H, r);
        for (size_t i=0;i<N;i++){ ALWAN_CORE_T var_i=mean_ii[i]-mean_i[i]*mean_i[i], cov=mean_ip[i]-mean_i[i]*mp[i];
            ALWAN_CORE_T aa=cov/(var_i+geps); A[i]=aa; B[i]=mp[i]-aa*mean_i[i]; }
        ALWAN_CORE_FN(alwan__boxfilter)(A, mean_ip, tmp, W, H, r);        /* mean_a -> mean_ip */
        ALWAN_CORE_FN(alwan__boxfilter)(B, mean_ii,  tmp, W, H, r);        /* mean_b -> mean_ii  */
        for (size_t i=0;i<N;i++){ ALWAN_CORE_T v=mean_ip[i]*I[i]+mean_ii[i]; veil[i]=v<0?0:v; }
        ALWAN_CORE_T mmax=peak;
        for (size_t i=0;i<N;i++){ ALWAN_CORE_T rr=in[i*3],gg=in[i*3+1],bb=in[i*3+2]; ALWAN_CORE_T mi=rr>gg?(rr>bb?rr:bb):(gg>bb?gg:bb); if(mi>mmax)mmax=mi; }
        ALWAN_CORE_T span=mmax-peak; if(span<ALWAN_CORE_LITERAL(1e-6))span=ALWAN_CORE_LITERAL(1e-6);
        for (size_t i=0;i<N;i++){ ALWAN_CORE_T rr=in[i*3],gg=in[i*3+1],bb=in[i*3+2];
            ALWAN_CORE_T m = rr>gg?(rr>bb?rr:bb):(gg>bb?gg:bb);
            if (m<=ALWAN_CORE_LITERAL(1e-9)){ out[i*3]=rr<0?0:rr; out[i*3+1]=gg<0?0:gg; out[i*3+2]=bb<0?0:bb; continue; }
            ALWAN_CORE_T ms; if (m<=knee) ms=m; else ms=knee+(peak-knee)*(ALWAN_CORE_LITERAL(1.0)-ALWAN_CORE_EXP(-(m-knee)/(peak-knee)));
            ALWAN_CORE_T bw = ALWAN_CORE_FN(alwan__smoothstep)((m-knee)/ALWAN_CORE_LITERAL(0.6));
            ALWAN_CORE_T comp=(m-peak)/span; comp=comp<0?0:(comp>1?1:comp); /* linear -> 1 at actual max emission, gradual */
            ALWAN_CORE_T a=comp + s*bw*(veil[i]/m); if(a<0)a=0; if(a>1)a=1; /* purity attenuation + s*airlight */
            ALWAN_CORE_T sc=ms/m;
            ALWAN_CORE_T or_=(rr*sc)*(ALWAN_CORE_LITERAL(1.0)-a)+ms*a, og=(gg*sc)*(ALWAN_CORE_LITERAL(1.0)-a)+ms*a, ob=(bb*sc)*(ALWAN_CORE_LITERAL(1.0)-a)+ms*a;
            out[i*3]=or_<0?0:(or_>peak?peak:or_); out[i*3+1]=og<0?0:(og>peak?peak:og); out[i*3+2]=ob<0?0:(ob>peak?peak:ob); }
        ctx->free_fn(minc);ctx->free_fn(I);ctx->free_fn(tmp);ctx->free_fn(mean_i);ctx->free_fn(mp);
        ctx->free_fn(mean_ip);ctx->free_fn(mean_ii);ctx->free_fn(A);ctx->free_fn(B);ctx->free_fn(veil);
        return ALWAN_OK;
    }

    /* ---- DENSITY_LOG: both-end density S-curve (carrier-safe toe + shoulder) ----
     * Monotone shoulder on the carrier, plus a chroma-keep factor that is 0 at black
     * and at maximal emission and 1 in the mid. The non-max channels are refilled
     * toward the shouldered carrier ms, so max(out)=ms stays monotone -> 0 flips,
     * yet chroma collapses at BOTH ends (toe -> black, shoulder -> white). */
    if (p->method == ALWAN_GAMUT_FORM_DENSITY_LOG) {
        ALWAN_CORE_T const knee = ALWAN_CORE_LITERAL(0.80) * peak;
        ALWAN_CORE_T mmax = peak;
        for (size_t i=0;i<N;i++){ ALWAN_CORE_T r=in[i*3],g=in[i*3+1],b=in[i*3+2]; ALWAN_CORE_T mi=r>g?(r>b?r:b):(g>b?g:b); if(mi>mmax)mmax=mi; }
        ALWAN_CORE_T span = mmax - peak; if (span < ALWAN_CORE_LITERAL(1e-6)) span = ALWAN_CORE_LITERAL(1e-6);
        for (size_t i=0;i<N;i++){
            ALWAN_CORE_T r=in[i*3],g=in[i*3+1],b=in[i*3+2];
            ALWAN_CORE_T m = r>g?(r>b?r:b):(g>b?g:b);
            if (m <= ALWAN_CORE_LITERAL(1e-9)){ out[i*3]=r<0?0:r; out[i*3+1]=g<0?0:g; out[i*3+2]=b<0?0:b; continue; }
            ALWAN_CORE_T ms = (m<=knee)? m : knee+(peak-knee)*(ALWAN_CORE_LITERAL(1.0)-ALWAN_CORE_EXP(-(m-knee)/(peak-knee)));
            ALWAN_CORE_T thi = ALWAN_CORE_LITERAL(1.0) - (m-peak)/span; thi = thi<0?0:(thi>1?1:thi);                      /* shoulder: chroma->white only near max emission */
            ALWAN_CORE_T cf = thi;                                                                    /* no chroma toe: dark stays coloured, reaches black via luminance */
            ALWAN_CORE_T sc = ms/m;
            ALWAN_CORE_T or_ = ms + (r*sc - ms)*cf, og = ms + (g*sc - ms)*cf, ob = ms + (b*sc - ms)*cf;
            out[i*3]=or_<0?0:(or_>peak?peak:or_); out[i*3+1]=og<0?0:(og>peak?peak:og); out[i*3+2]=ob<0?0:(ob>peak?peak:ob);
        }
        return ALWAN_OK;
    }

    /* ---- COMPLETION: FLUX + volumetric (depth-scaled) veil ----
     * Same as FLUX, but the diffused dark-channel veil is scaled by path length
     * through the volume (Koschmieder airlight): far pixels (low depth-near value)
     * integrate more veil -> a diffuse layer filling the volume. Carrier-safe. */
    if (p->method == ALWAN_GAMUT_FORM_COMPLETION) {
        ALWAN_CORE_T const knee = ALWAN_CORE_LITERAL(0.80) * peak; int const r = 12; ALWAN_CORE_T const geps = ALWAN_CORE_LITERAL(1e-3);
        ALWAN_CORE_T *minc=(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16), *I =(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        ALWAN_CORE_T *tmp =(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16), *mean_i=(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        ALWAN_CORE_T *mp =(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16), *mean_ip=(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        ALWAN_CORE_T *mean_ii =(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16), *A =(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        ALWAN_CORE_T *B  =(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16), *veil=(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        if (!minc||!I||!tmp||!mean_i||!mp||!mean_ip||!mean_ii||!A||!B||!veil) {
            ctx->free_fn(minc);ctx->free_fn(I);ctx->free_fn(tmp);ctx->free_fn(mean_i);ctx->free_fn(mp);
            ctx->free_fn(mean_ip);ctx->free_fn(mean_ii);ctx->free_fn(A);ctx->free_fn(B);ctx->free_fn(veil); return ALWAN_E_NOMEM; }
        for (size_t i=0;i<N;i++){ ALWAN_CORE_T rr=in[i*3],gg=in[i*3+1],bb=in[i*3+2];
            if(rr<0)rr=0; if(gg<0)gg=0; if(bb<0)bb=0; minc[i]=rr<gg?(rr<bb?rr:bb):(gg<bb?gg:bb); I[i]=rr>gg?(rr>bb?rr:bb):(gg>bb?gg:bb); }  /* veil guided by CARRIER max(RGB), not luminance (hue-agnostic) */
        ALWAN_CORE_FN(alwan__boxfilter)(I, mean_i, tmp, W, H, r);
        ALWAN_CORE_FN(alwan__boxfilter)(minc, mp, tmp, W, H, r);
        for (size_t i=0;i<N;i++) veil[i]=I[i]*minc[i]; ALWAN_CORE_FN(alwan__boxfilter)(veil, mean_ip, tmp, W, H, r);
        for (size_t i=0;i<N;i++) veil[i]=I[i]*I[i];    ALWAN_CORE_FN(alwan__boxfilter)(veil, mean_ii,  tmp, W, H, r);
        for (size_t i=0;i<N;i++){ ALWAN_CORE_T var_i=mean_ii[i]-mean_i[i]*mean_i[i], cov=mean_ip[i]-mean_i[i]*mp[i]; ALWAN_CORE_T aa=cov/(var_i+geps); A[i]=aa; B[i]=mp[i]-aa*mean_i[i]; }
        ALWAN_CORE_FN(alwan__boxfilter)(A, mean_ip, tmp, W, H, r);
        ALWAN_CORE_FN(alwan__boxfilter)(B, mean_ii,  tmp, W, H, r);
        for (size_t i=0;i<N;i++){ ALWAN_CORE_T v=mean_ip[i]*I[i]+mean_ii[i]; if(v<0)v=0;
            ALWAN_CORE_T df = depth ? (ALWAN_CORE_LITERAL(0.3) + ALWAN_CORE_LITERAL(0.7)*(ALWAN_CORE_LITERAL(1.0) - depth[i])) : ALWAN_CORE_LITERAL(1.0);   /* volumetric: more veil where deeper (far) */
            veil[i] = v * df; }
        ALWAN_CORE_T mmax=peak;
        for (size_t i=0;i<N;i++){ ALWAN_CORE_T rr=in[i*3],gg=in[i*3+1],bb=in[i*3+2]; ALWAN_CORE_T mi=rr>gg?(rr>bb?rr:bb):(gg>bb?gg:bb); if(mi>mmax)mmax=mi; }
        ALWAN_CORE_T span=mmax-peak; if(span<ALWAN_CORE_LITERAL(1e-6))span=ALWAN_CORE_LITERAL(1e-6);
        for (size_t i=0;i<N;i++){ ALWAN_CORE_T rr=in[i*3],gg=in[i*3+1],bb=in[i*3+2];
            ALWAN_CORE_T m = rr>gg?(rr>bb?rr:bb):(gg>bb?gg:bb);
            if (m<=ALWAN_CORE_LITERAL(1e-9)){ out[i*3]=rr<0?0:rr; out[i*3+1]=gg<0?0:gg; out[i*3+2]=bb<0?0:bb; continue; }
            ALWAN_CORE_T ms; if (m<=knee) ms=m; else ms=knee+(peak-knee)*(ALWAN_CORE_LITERAL(1.0)-ALWAN_CORE_EXP(-(m-knee)/(peak-knee)));
            ALWAN_CORE_T bw = ALWAN_CORE_FN(alwan__smoothstep)((m-knee)/ALWAN_CORE_LITERAL(0.6));
            ALWAN_CORE_T comp=(m-peak)/span; comp=comp<0?0:(comp>1?1:comp);
            ALWAN_CORE_T a=comp + s*bw*(veil[i]/m); if(a<0)a=0; if(a>1)a=1;
            ALWAN_CORE_T sc=ms/m;
            ALWAN_CORE_T or_=(rr*sc)*(ALWAN_CORE_LITERAL(1.0)-a)+ms*a, og=(gg*sc)*(ALWAN_CORE_LITERAL(1.0)-a)+ms*a, ob=(bb*sc)*(ALWAN_CORE_LITERAL(1.0)-a)+ms*a;
            out[i*3]=or_<0?0:(or_>peak?peak:or_); out[i*3+1]=og<0?0:(og>peak?peak:og); out[i*3+2]=ob<0?0:(ob>peak?peak:ob); }
        ctx->free_fn(minc);ctx->free_fn(I);ctx->free_fn(tmp);ctx->free_fn(mean_i);ctx->free_fn(mp);
        ctx->free_fn(mean_ip);ctx->free_fn(mean_ii);ctx->free_fn(A);ctx->free_fn(B);ctx->free_fn(veil);
        return ALWAN_OK;
    }

    /* ---- PICTURE: the synthesis (carrier + density S + field veil) ----
     * Per-pixel monotone carrier shoulder ms (0 flips). Non-max channels are
     * reconstructed as lerp(white(ms), ratio-preserve(ms), keep) where the single
     * keep term folds in the whole theory:
     *   keep = cf * (1 - a),  cf = toe(m)*shoulder(m),  a = diffused-veil / m.
     * cf collapses chroma at BOTH ends (-> black in shadow, -> R=G=B at the scene's
     * actual maximal emission; adaptive + gradual); a is the additive airlight
     * (light -> white) vs multiplicative surface (-> hue) split from FLUX's diffused
     * dark channel. Self-contained (no depth/seg), carrier-safe. */
    if (p->method == ALWAN_GAMUT_FORM_PICTURE) {
        ALWAN_CORE_T const knee = ALWAN_CORE_LITERAL(0.80) * peak; int const r = 12; ALWAN_CORE_T const geps = ALWAN_CORE_LITERAL(1e-3);
        ALWAN_CORE_T *minc=(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16), *I =(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        ALWAN_CORE_T *tmp =(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16), *mean_i=(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        ALWAN_CORE_T *mp =(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16), *mean_ip=(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        ALWAN_CORE_T *mean_ii =(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16), *A =(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        ALWAN_CORE_T *B  =(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16), *veil=(ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        if (!minc||!I||!tmp||!mean_i||!mp||!mean_ip||!mean_ii||!A||!B||!veil) {
            ctx->free_fn(minc);ctx->free_fn(I);ctx->free_fn(tmp);ctx->free_fn(mean_i);ctx->free_fn(mp);
            ctx->free_fn(mean_ip);ctx->free_fn(mean_ii);ctx->free_fn(A);ctx->free_fn(B);ctx->free_fn(veil); return ALWAN_E_NOMEM; }
        for (size_t i=0;i<N;i++){ ALWAN_CORE_T rr=in[i*3],gg=in[i*3+1],bb=in[i*3+2];
            if(rr<0)rr=0; if(gg<0)gg=0; if(bb<0)bb=0; minc[i]=rr<gg?(rr<bb?rr:bb):(gg<bb?gg:bb); I[i]=rr>gg?(rr>bb?rr:bb):(gg>bb?gg:bb); }  /* veil guided by CARRIER max(RGB), not luminance (hue-agnostic) */
        ALWAN_CORE_FN(alwan__boxfilter)(I, mean_i, tmp, W, H, r);              /* guided-filter diffused dark-channel veil (FLUX) */
        ALWAN_CORE_FN(alwan__boxfilter)(minc, mp, tmp, W, H, r);
        for (size_t i=0;i<N;i++) veil[i]=I[i]*minc[i]; ALWAN_CORE_FN(alwan__boxfilter)(veil, mean_ip, tmp, W, H, r);
        for (size_t i=0;i<N;i++) veil[i]=I[i]*I[i];    ALWAN_CORE_FN(alwan__boxfilter)(veil, mean_ii,  tmp, W, H, r);
        for (size_t i=0;i<N;i++){ ALWAN_CORE_T var_i=mean_ii[i]-mean_i[i]*mean_i[i], cov=mean_ip[i]-mean_i[i]*mp[i]; ALWAN_CORE_T aa=cov/(var_i+geps); A[i]=aa; B[i]=mp[i]-aa*mean_i[i]; }
        ALWAN_CORE_FN(alwan__boxfilter)(A, mean_ip, tmp, W, H, r);
        ALWAN_CORE_FN(alwan__boxfilter)(B, mean_ii,  tmp, W, H, r);
        for (size_t i=0;i<N;i++){ ALWAN_CORE_T v=mean_ip[i]*I[i]+mean_ii[i]; veil[i]=v<0?0:v; }
        ALWAN_CORE_T mmax=peak;
        for (size_t i=0;i<N;i++){ ALWAN_CORE_T rr=in[i*3],gg=in[i*3+1],bb=in[i*3+2]; ALWAN_CORE_T mi=rr>gg?(rr>bb?rr:bb):(gg>bb?gg:bb); if(mi>mmax)mmax=mi; }
        ALWAN_CORE_T span=mmax-peak; if(span<ALWAN_CORE_LITERAL(1e-6))span=ALWAN_CORE_LITERAL(1e-6);
        for (size_t i=0;i<N;i++){ ALWAN_CORE_T rr=in[i*3],gg=in[i*3+1],bb=in[i*3+2];
            ALWAN_CORE_T m = rr>gg?(rr>bb?rr:bb):(gg>bb?gg:bb);
            if (m<=ALWAN_CORE_LITERAL(1e-9)){ out[i*3]=rr<0?0:rr; out[i*3+1]=gg<0?0:gg; out[i*3+2]=bb<0?0:bb; continue; }
            ALWAN_CORE_T ms; if (m<=knee) ms=m; else ms=knee+(peak-knee)*(ALWAN_CORE_LITERAL(1.0)-ALWAN_CORE_EXP(-(m-knee)/(peak-knee)));   /* carrier shoulder */
            ALWAN_CORE_T thi=ALWAN_CORE_LITERAL(1.0)-(m-peak)/span; thi=thi<0?0:(thi>1?1:thi);                                     /* shoulder -> white only near max emission */
            ALWAN_CORE_T bw = ALWAN_CORE_FN(alwan__smoothstep)((m-ALWAN_CORE_LITERAL(0.45))/ALWAN_CORE_LITERAL(0.55));                       /* airlight only whitens BRIGHT (no global wash) */
            ALWAN_CORE_T a=bw*(veil[i]/m); if(a<0)a=0; if(a>1)a=1;                                             /* airlight fraction */
            ALWAN_CORE_T keep=thi*(ALWAN_CORE_LITERAL(1.0)-a);                                                                    /* no chroma toe: dark stays coloured */
            ALWAN_CORE_T sc=ms/m;
            ALWAN_CORE_T or_=ms+(rr*sc-ms)*keep, og=ms+(gg*sc-ms)*keep, ob=ms+(bb*sc-ms)*keep;
            out[i*3]=or_<0?0:(or_>peak?peak:or_); out[i*3+1]=og<0?0:(og>peak?peak:og); out[i*3+2]=ob<0?0:(ob>peak?peak:ob); }
        ctx->free_fn(minc);ctx->free_fn(I);ctx->free_fn(tmp);ctx->free_fn(mean_i);ctx->free_fn(mp);
        ctx->free_fn(mean_ip);ctx->free_fn(mean_ii);ctx->free_fn(A);ctx->free_fn(B);ctx->free_fn(veil);
        return ALWAN_OK;
    }

    /* ---- MOMENT: energy-rotation (MacAdam moment + Troy's rotation) ----
     * Carrier max(RGB) conserved (0 flips). The non-max channels are a rotation between the
     * ratio-preserve colour and the achromatic ms, by a purity weight P = cos^2 of the phase
     * toward the ACTUAL max emission: full chroma while in-gamut, collapsing to R=G=B (white)
     * only as the carrier approaches the scene's maximal emission (Troy's white infinity). No
     * chroma toe -- dark colour stays coloured and reaches black via luminance, not grey.
     * Per-pixel, self-contained, linear (no Lab/Oklab). Ratio-preserve hue (Abney left to cognition). */
    if (p->method == ALWAN_GAMUT_FORM_MOMENT) {
        ALWAN_CORE_T const knee = ALWAN_CORE_LITERAL(0.80) * peak, HALFPI = ALWAN_CORE_LITERAL(1.5707963267948966);
        ALWAN_CORE_T mmax = peak;                                  /* adaptive white infinity = actual max emission */
        for (size_t i=0;i<N;i++){ ALWAN_CORE_T rr=in[i*3],gg=in[i*3+1],bb=in[i*3+2]; ALWAN_CORE_T mi=rr>gg?(rr>bb?rr:bb):(gg>bb?gg:bb); if(mi>mmax)mmax=mi; }
        ALWAN_CORE_T span = mmax-peak; if(span<ALWAN_CORE_LITERAL(1e-6))span=ALWAN_CORE_LITERAL(1e-6);
        for (size_t i=0;i<N;i++){ ALWAN_CORE_T rr=in[i*3],gg=in[i*3+1],bb=in[i*3+2];
            ALWAN_CORE_T m = rr>gg?(rr>bb?rr:bb):(gg>bb?gg:bb);
            if (m<=ALWAN_CORE_LITERAL(1e-9)){ out[i*3]=rr<0?0:rr; out[i*3+1]=gg<0?0:gg; out[i*3+2]=bb<0?0:bb; continue; }
            ALWAN_CORE_T ms; if (m<=knee) ms=m; else ms=knee+(peak-knee)*(ALWAN_CORE_LITERAL(1.0)-ALWAN_CORE_EXP(-(m-knee)/(peak-knee)));   /* carrier shoulder */
            ALWAN_CORE_T sc = ms/m;
            /* purity envelope: FULL chroma while in-gamut; quadrature collapse (cos^2) only toward the
             * actual max emission (the white infinity), plus a toe toward black. So bright in-gamut
             * content keeps its chroma -- no white wash -- and only true max-emission goes to R=G=B. */
            ALWAN_CORE_T sh = (m-peak)/span; if(sh<0)sh=0; if(sh>1)sh=1;
            ALWAN_CORE_T cs = ALWAN_CORE_COS(HALFPI*sh); ALWAN_CORE_T P = cs*cs;   /* full chroma; collapse toward WHITE only near max emission. No chroma toe: dark colour stays coloured and reaches black via luminance (tone), not grey. */
            ALWAN_CORE_T or_ = ms + (rr*sc - ms)*P;                /* rotate achromatic(ms) <-> ratio-preserve(rp) */
            ALWAN_CORE_T og  = ms + (gg*sc - ms)*P;
            ALWAN_CORE_T ob  = ms + (bb*sc - ms)*P;                /* max channel -> ms: carrier-safe */
            out[i*3]=or_<0?0:(or_>peak?peak:or_); out[i*3+1]=og<0?0:(og>peak?peak:og); out[i*3+2]=ob<0?0:(ob>peak?peak:ob); }
        return ALWAN_OK;
    }

    /* ---- HEMISPHERE: parameter-free C2 log-logistic on the integration (Troy) ----
     * Integration I = max(RGB) mapped by ms = peak / (1 + exp(-k*(log2 I - pivot))). The slope
     * (gradient gain) is a C2 bump: maximal at the exposure pivot, -> 0 at both energy infinities.
     * pivot = mean of log2 I (log geometric-mean carrier; scene adaptation, invariant); k = 2*ln2
     * from requiring unit log-log gamma at the pivot (contrast preserved at the exposure). Nothing
     * tuned. Hue-agnostic purity collapse only near display max. Per-pixel monotone => global
     * integration ordering preserved (0 carrier flips). */
    if (p->method == ALWAN_GAMUT_FORM_HEMISPHERE) {
        ALWAN_CORE_T const k_gain = ALWAN_CORE_LITERAL(1.3862943611198906);   /* 2*ln2: unit gamma at pivot */
        ALWAN_CORE_T const i_eps  = ALWAN_CORE_LITERAL(1e-4) * peak;
        ALWAN_CORE_T pivot = 0; size_t n_pos = 0;
        for (size_t i=0;i<N;i++){ ALWAN_CORE_T rr=in[i*3],gg=in[i*3+1],bb=in[i*3+2];
            if(rr<0)rr=0; if(gg<0)gg=0; if(bb<0)bb=0;
            ALWAN_CORE_T m = rr>gg?(rr>bb?rr:bb):(gg>bb?gg:bb);
            if (m > i_eps){ pivot += ALWAN_CORE_LOG2(m); n_pos++; } }
        pivot = n_pos ? pivot/(ALWAN_CORE_T)n_pos : 0;
        for (size_t i=0;i<N;i++){ ALWAN_CORE_T rr=in[i*3],gg=in[i*3+1],bb=in[i*3+2];
            ALWAN_CORE_T m = rr>gg?(rr>bb?rr:bb):(gg>bb?gg:bb);
            if (m<=ALWAN_CORE_LITERAL(1e-9)){ out[i*3]=rr<0?0:rr; out[i*3+1]=gg<0?0:gg; out[i*3+2]=bb<0?0:bb; continue; }
            ALWAN_CORE_T ms = peak/(ALWAN_CORE_LITERAL(1.0)+ALWAN_CORE_EXP(-k_gain*(ALWAN_CORE_LOG2(m)-pivot)));
            ALWAN_CORE_T sc = ms/m;
            ALWAN_CORE_T keep = ALWAN_CORE_LITERAL(1.0)
                - ALWAN_CORE_FN(alwan__smoothstep)((ms/peak-ALWAN_CORE_LITERAL(0.75))/ALWAN_CORE_LITERAL(0.25));
            ALWAN_CORE_T or_ = ms + (rr*sc - ms)*keep;
            ALWAN_CORE_T og  = ms + (gg*sc - ms)*keep;
            ALWAN_CORE_T ob  = ms + (bb*sc - ms)*keep;                 /* max channel -> ms: carrier-safe */
            out[i*3]=or_<0?0:(or_>peak?peak:or_); out[i*3+1]=og<0?0:(og>peak?peak:og); out[i*3+2]=ob<0?0:(ob>peak?peak:ob); }
        return ALWAN_OK;
    }

    /* ---- CHANNEL: derived channel integration (AgX architecture, Troy Sobotka) ----
     * inset (SB2383 per-primary chromaticity matrix, neutral-preserving) -> per-channel C2
     * log-logistic on an INVARIANT absolute window -> matched inverse outset (per-primary
     * complementary restore) -> soft guard rails. Every constant is a display standard, none
     * scene-derived (temporal constancy -- "not a creative decision"):
     *   window  = log2(0.18) +/- (10, 6.5) stops                 (18% grey camera exposure)
     *   slope   = ln(254)/min(vp,1-vp)                           (sigmoid spans 8-bit display)
     *   affine-norm sigmoid: window bottom -> 0, top -> 1        (neutral DC respected + span)
     *   gamma   = log(0.18)/log(0.5)                             (mid-grey OUTPUT anchor -> 0.18)
     *   outset  = inset^-1                                       (negative lobes; per-primary)
     *   rails   = 0.004 (black) / 0.02 (white) asymptotic toes   (no clamp contour)
     * Per-pixel; whiteness/hue-sweep emerge from staggered channel saturation. Output is
     * display-referred [0,1]. Not carrier-monotone by design (AgX-class inset crosstalk). */
    if (p->method == ALWAN_GAMUT_FORM_CHANNEL) {
        /* SB2383 inset (rows sum to 1 -> neutral preserved) and its exact inverse (the outset) */
        static ALWAN_CORE_T const M_in[9] = {
            ALWAN_CORE_LITERAL(0.9135007950224077), ALWAN_CORE_LITERAL(0.04474362438024326), ALWAN_CORE_LITERAL(0.04175558059734905),
            ALWAN_CORE_LITERAL(0.08087188214320866), ALWAN_CORE_LITERAL(0.8154742873412193), ALWAN_CORE_LITERAL(0.1036538305155724),
            ALWAN_CORE_LITERAL(0.03593972317761818), ALWAN_CORE_LITERAL(0.03428460395090557), ALWAN_CORE_LITERAL(0.9297756728714763) };
        static ALWAN_CORE_T const M_out[9] = {
            ALWAN_CORE_LITERAL(1.101569971768266),  ALWAN_CORE_LITERAL(-0.05863613890954103), ALWAN_CORE_LITERAL(-0.04293383285872466),
            ALWAN_CORE_LITERAL(-0.1043210771512591), ALWAN_CORE_LITERAL(1.237607860014678),   ALWAN_CORE_LITERAL(-0.1332867828634192),
            ALWAN_CORE_LITERAL(-0.03873355055746177), ALWAN_CORE_LITERAL(-0.0433690941837448), ALWAN_CORE_LITERAL(1.082102644741207) };
        ALWAN_CORE_T const mid   = ALWAN_CORE_LOG2(ALWAN_CORE_LITERAL(0.18));
        ALWAN_CORE_T const lo    = mid - ALWAN_CORE_LITERAL(10.0);
        ALWAN_CORE_T const w_win = ALWAN_CORE_LITERAL(16.5);          /* (10 + 6.5) stops */
        ALWAN_CORE_T const vp    = ALWAN_CORE_LITERAL(10.0)/w_win;    /* pivot position of log2(0.18) */
        ALWAN_CORE_T const g_slope = ALWAN_CORE_LITERAL(5.54)/(vp > ALWAN_CORE_LITERAL(1.0)-vp ? vp : ALWAN_CORE_LITERAL(1.0)-vp);
        ALWAN_CORE_T const gamma = -ALWAN_CORE_LOG2(ALWAN_CORE_LITERAL(0.18));   /* log(0.18)/log(0.5) = -log2(0.18) ~ 2.474 */
        ALWAN_CORE_T const rlo = ALWAN_CORE_LITERAL(0.004), rhi = ALWAN_CORE_LITERAL(0.02);
        ALWAN_CORE_T const d_lo = ALWAN_CORE_LITERAL(1.0)/(ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_EXP(-g_slope*(ALWAN_CORE_LITERAL(0.0)-vp)));
        ALWAN_CORE_T const d_hi = ALWAN_CORE_LITERAL(1.0)/(ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_EXP(-g_slope*(ALWAN_CORE_LITERAL(1.0)-vp)));
        ALWAN_CORE_T d_rng = d_hi - d_lo; if (d_rng < ALWAN_CORE_LITERAL(1e-6)) d_rng = ALWAN_CORE_LITERAL(1e-6);
        for (size_t i = 0; i < N; i++) {
            ALWAN_CORE_T rr = in[i*3], gg = in[i*3+1], bb = in[i*3+2];
            if (rr < 0) rr = 0; if (gg < 0) gg = 0; if (bb < 0) bb = 0;
            /* inset -> working channels */
            ALWAN_CORE_T c0 = M_in[0]*rr + M_in[1]*gg + M_in[2]*bb;
            ALWAN_CORE_T c1 = M_in[3]*rr + M_in[4]*gg + M_in[5]*bb;
            ALWAN_CORE_T c2 = M_in[6]*rr + M_in[7]*gg + M_in[8]*bb;
            if (c0 < ALWAN_CORE_LITERAL(1e-12)) c0 = ALWAN_CORE_LITERAL(1e-12);
            if (c1 < ALWAN_CORE_LITERAL(1e-12)) c1 = ALWAN_CORE_LITERAL(1e-12);
            if (c2 < ALWAN_CORE_LITERAL(1e-12)) c2 = ALWAN_CORE_LITERAL(1e-12);
            /* per-channel sigmoid, affine-normalized, 0.18 mid-grey anchor */
            ALWAN_CORE_T d[3], cc[3]; cc[0]=c0; cc[1]=c1; cc[2]=c2;
            for (int k = 0; k < 3; k++) {
                ALWAN_CORE_T dv = ALWAN_CORE_LITERAL(1.0)/(ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_EXP(-g_slope*((ALWAN_CORE_LOG2(cc[k]) - lo)/w_win - vp)));
                ALWAN_CORE_T dn = (dv - d_lo)/d_rng; if (dn < 0) dn = 0;
                d[k] = ALWAN_CORE_POW(dn, gamma);
            }
            /* matched inverse outset (per-primary complementary restore) */
            ALWAN_CORE_T o[3];
            o[0] = M_out[0]*d[0] + M_out[1]*d[1] + M_out[2]*d[2];
            o[1] = M_out[3]*d[0] + M_out[4]*d[1] + M_out[5]*d[2];
            o[2] = M_out[6]*d[0] + M_out[7]*d[1] + M_out[8]*d[2];
            for (int k = 0; k < 3; k++) {
                ALWAN_CORE_T ov = o[k];
                if (ov < rlo)      ov = rlo*ALWAN_CORE_EXP((ov - rlo)/rlo);
                else if (ov > ALWAN_CORE_LITERAL(1.0) - rhi) ov = ALWAN_CORE_LITERAL(1.0) - rhi*ALWAN_CORE_EXP(((ALWAN_CORE_LITERAL(1.0)-ov) - rhi)/rhi);
                out[i*3+k] = ov;
            }
        }
        return ALWAN_OK;
    }

    /* ---- HEMISPHERE_ABS: the Absolute Hemisphere ----
     * HEMISPHERE's carrier tone made temporally invariant: the C2 log-logistic runs on the
     * carrier I = max(RGB) over CHANNEL's FIXED absolute window, affine-normalized (DC respected),
     * 18% mid-grey anchored, with asymptotic rails; hue-agnostic ratio-preserve reconstruction,
     * purity collapse only at maximal emission. Per-pixel monotone in the carrier (out max = ms)
     * => 0 flips; a function of max(RGB) + ratios => hue-agnostic; fixed window => scene-invariant.
     * Satisfies every numerically-testable constraint (see docs/picture_formation.md). */
    if (p->method == ALWAN_GAMUT_FORM_HEMISPHERE_ABS) {
        ALWAN_CORE_T const mid   = ALWAN_CORE_LOG2(ALWAN_CORE_LITERAL(0.18));
        ALWAN_CORE_T const lo    = mid - ALWAN_CORE_LITERAL(10.0);
        ALWAN_CORE_T const w_win = ALWAN_CORE_LITERAL(16.5);
        ALWAN_CORE_T const vp    = ALWAN_CORE_LITERAL(10.0)/w_win;
        ALWAN_CORE_T const g_slope = ALWAN_CORE_LITERAL(5.54)/(vp > ALWAN_CORE_LITERAL(1.0)-vp ? vp : ALWAN_CORE_LITERAL(1.0)-vp);
        ALWAN_CORE_T const gamma = -ALWAN_CORE_LOG2(ALWAN_CORE_LITERAL(0.18));
        ALWAN_CORE_T const rlo = ALWAN_CORE_LITERAL(0.004), rhi = ALWAN_CORE_LITERAL(0.02);
        ALWAN_CORE_T const d_lo = ALWAN_CORE_LITERAL(1.0)/(ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_EXP(-g_slope*(ALWAN_CORE_LITERAL(0.0)-vp)));
        ALWAN_CORE_T const d_hi = ALWAN_CORE_LITERAL(1.0)/(ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_EXP(-g_slope*(ALWAN_CORE_LITERAL(1.0)-vp)));
        ALWAN_CORE_T d_rng = d_hi - d_lo; if (d_rng < ALWAN_CORE_LITERAL(1e-6)) d_rng = ALWAN_CORE_LITERAL(1e-6);
        for (size_t i = 0; i < N; i++) {
            ALWAN_CORE_T rr = in[i*3], gg = in[i*3+1], bb = in[i*3+2];
            if (rr < 0) rr = 0; if (gg < 0) gg = 0; if (bb < 0) bb = 0;
            ALWAN_CORE_T m = rr > gg ? (rr > bb ? rr : bb) : (gg > bb ? gg : bb);
            if (m <= ALWAN_CORE_LITERAL(1e-9)) { out[i*3]=0; out[i*3+1]=0; out[i*3+2]=0; continue; }
            ALWAN_CORE_T vpos = (ALWAN_CORE_LOG2(m) - lo)/w_win;   /* scene carrier position: 0 = window floor, 1 = window top */
            ALWAN_CORE_T dv = ALWAN_CORE_LITERAL(1.0)/(ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_EXP(-g_slope*(vpos - vp)));
            ALWAN_CORE_T dn = (dv - d_lo)/d_rng; if (dn < 0) dn = 0;
            ALWAN_CORE_T ms = ALWAN_CORE_POW(dn, gamma) * peak;
            ALWAN_CORE_T sc = ms/m;
            /* hue-agnostic purity rolloff -- a GENTLE, CONTINUOUS desaturation with the SCENE carrier
             * position (vpos), emulating the staggered per-channel whitening of channel integration
             * without any hue decision. Two failure modes bracket it: keyed off the compressed display
             * value (ms/peak) it bleaches ordinary primaries; frozen (keep=1 until the top) it leaves a
             * flat plateau where the tonescale has saturated and turns additive overlaps into hard
             * saturated seams. A raised cosine on vpos threads between them: chroma is drawn toward the
             * achromatic level ms progressively -- untouched through mids/shadows (vpos < 0.72, carrier
             * < ~0.7x mid so SPAN's saturated patches survive), ~half by carrier ~10x (a live gradient
             * where luminance has already saturated => no plateau, and softened overlaps), completing to
             * R=G=B only for genuine white-infinity emission. For a pure primary sat == keep, so PURE
             * holds iff keep(carrier 1e4) < 0.15. C1 (no manufactured colour contour); keep never
             * touches neutrals (channel*sc == ms there) so C2/NEUT are unaffected; carrier-safe
             * (out max = ms) => MONO; a function of max(RGB) => HUEA. */
            ALWAN_CORE_T t = (vpos - ALWAN_CORE_LITERAL(0.72))/ALWAN_CORE_LITERAL(0.48);
            if (t < 0) t = 0; if (t > 1) t = 1;
            ALWAN_CORE_T keep = ALWAN_CORE_LITERAL(0.5)*(ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_COS(ALWAN_CORE_LITERAL(3.141592653589793)*t));
            ALWAN_CORE_T o[3];
            o[0] = ms + (rr*sc - ms)*keep;
            o[1] = ms + (gg*sc - ms)*keep;
            o[2] = ms + (bb*sc - ms)*keep;   /* max channel -> ms: carrier-safe */
            for (int k = 0; k < 3; k++) {
                ALWAN_CORE_T ov = o[k];
                if (ov < rlo)      ov = rlo*ALWAN_CORE_EXP((ov - rlo)/rlo);
                else if (ov > ALWAN_CORE_LITERAL(1.0) - rhi) ov = ALWAN_CORE_LITERAL(1.0) - rhi*ALWAN_CORE_EXP(((ALWAN_CORE_LITERAL(1.0)-ov) - rhi)/rhi);
                out[i*3+k] = ov;
            }
        }
        return ALWAN_OK;
    }

    /* ---- COMPLETE / COMPLETE_HEMI_LOOK: the 13-of-13 operator ----
     * HEMISPHERE_ABS is 12/13: it misses only RATIO because its *per-channel* asymptotic rails lift
     * the min channel of a saturated pixel disproportionately, rotating its hue near 0/1. COMPLETE is
     * HEMISPHERE_ABS's carrier tone with two changes that clear RATIO while keeping every other row:
     *   (1) the asymptotic HIGH rail is applied to the CARRIER ms (a single scalar) BEFORE
     *       reconstruction; all three channels then take the SAME scale ms/m. A per-channel rail is
     *       never used, so out_chroma stays || in_chroma (direction preserved => RATIO).
     *   (2) a DESATURATION FLOOR replaces the per-channel LOW rail: the purity-keep is capped at
     *       KEEPMAX (< 1), drawing every saturated pixel a hair toward its achromatic level ms. This
     *       lifts the min channel off 0 (RAIL) as a UNIFORM chroma scale (1-keep) -- direction is
     *       again preserved (RATIO), min = (1-keep)*ms > 0, and sat ~ KEEPMAX > 0.85 (SPAN). No hard
     *       clamp is reached at either end (carrier high-rail below 1, floor above 0), so the final
     *       [0,1] is a safety net, not a plateau.
     * Pointwise + carrier-monotone (MONO) + a function of max(RGB)+ratios (HUEA) + a fixed window
     * (INVR) + now RATIO-clean: the FIRST method to satisfy all 13 numerically-testable constraints
     * (resolves docs/picture_formation.md's open question). Two constant sets, same operator:
     * COMPLETE_HEMI_LOOK keeps HEMISPHERE_ABS's exact window (look/contrast unchanged); COMPLETE takes
     * the isophote-through-highlight-max point of the 13-feasible set (wider window: brighter/softer). */
    if (p->method == ALWAN_GAMUT_FORM_COMPLETE || p->method == ALWAN_GAMUT_FORM_COMPLETE_HEMI_LOOK) {
        int const look = (p->method == ALWAN_GAMUT_FORM_COMPLETE_HEMI_LOOK);
        ALWAN_CORE_T const w_win   = look ? ALWAN_CORE_LITERAL(16.5)  : ALWAN_CORE_LITERAL(19.85);
        ALWAN_CORE_T const keepmax = look ? ALWAN_CORE_LITERAL(0.997) : ALWAN_CORE_LITERAL(0.861);
        ALWAN_CORE_T const rhi     = look ? ALWAN_CORE_LITERAL(0.02)  : ALWAN_CORE_LITERAL(0.005);
        ALWAN_CORE_T const mid   = ALWAN_CORE_LOG2(ALWAN_CORE_LITERAL(0.18));
        ALWAN_CORE_T const lo    = mid - ALWAN_CORE_LITERAL(10.0);
        ALWAN_CORE_T const vp    = ALWAN_CORE_LITERAL(10.0)/w_win;
        ALWAN_CORE_T const g_slope = ALWAN_CORE_LITERAL(5.54)/(vp > ALWAN_CORE_LITERAL(1.0)-vp ? vp : ALWAN_CORE_LITERAL(1.0)-vp);
        ALWAN_CORE_T const gamma = -ALWAN_CORE_LOG2(ALWAN_CORE_LITERAL(0.18));
        ALWAN_CORE_T const d_lo = ALWAN_CORE_LITERAL(1.0)/(ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_EXP(-g_slope*(ALWAN_CORE_LITERAL(0.0)-vp)));
        ALWAN_CORE_T const d_hi = ALWAN_CORE_LITERAL(1.0)/(ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_EXP(-g_slope*(ALWAN_CORE_LITERAL(1.0)-vp)));
        ALWAN_CORE_T d_rng = d_hi - d_lo; if (d_rng < ALWAN_CORE_LITERAL(1e-6)) d_rng = ALWAN_CORE_LITERAL(1e-6);
        for (size_t i = 0; i < N; i++) {
            ALWAN_CORE_T rr = in[i*3], gg = in[i*3+1], bb = in[i*3+2];
            if (rr < 0) rr = 0; if (gg < 0) gg = 0; if (bb < 0) bb = 0;   /* NEG: negative wattage -> 0 */
            ALWAN_CORE_T m = rr > gg ? (rr > bb ? rr : bb) : (gg > bb ? gg : bb);
            if (m <= ALWAN_CORE_LITERAL(1e-9)) { out[i*3]=0; out[i*3+1]=0; out[i*3+2]=0; continue; }
            ALWAN_CORE_T vpos = (ALWAN_CORE_LOG2(m) - lo)/w_win;
            ALWAN_CORE_T dv = ALWAN_CORE_LITERAL(1.0)/(ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_EXP(-g_slope*(vpos - vp)));
            ALWAN_CORE_T dn = (dv - d_lo)/d_rng; if (dn < 0) dn = 0;
            ALWAN_CORE_T ms = ALWAN_CORE_POW(dn, gamma) * peak;
            /* (1) asymptotic HIGH rail on the CARRIER (scalar), applied ONCE so the reconstruction
             *     scale ms/m below is uniform across channels -> RATIO-safe, and no plateau at 1. */
            if (ms > ALWAN_CORE_LITERAL(1.0) - rhi) ms = ALWAN_CORE_LITERAL(1.0) - rhi*ALWAN_CORE_EXP(((ALWAN_CORE_LITERAL(1.0)-ms) - rhi)/rhi);
            ALWAN_CORE_T sc = ms/m;
            ALWAN_CORE_T t = (vpos - ALWAN_CORE_LITERAL(0.72))/ALWAN_CORE_LITERAL(0.48);
            if (t < 0) t = 0; if (t > 1) t = 1;
            ALWAN_CORE_T keep = ALWAN_CORE_LITERAL(0.5)*(ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_COS(ALWAN_CORE_LITERAL(3.141592653589793)*t));
            /* (2) desaturation FLOOR: cap keep at KEEPMAX so the min channel lifts off 0 as a uniform
             *     chroma scale (RAIL without a per-channel rail => RATIO-safe, no plateau at 0). */
            if (keep > keepmax) keep = keepmax;
            ALWAN_CORE_T o0 = ms + (rr*sc - ms)*keep;   /* max channel -> ms (carrier-safe => MONO) */
            ALWAN_CORE_T o1 = ms + (gg*sc - ms)*keep;
            ALWAN_CORE_T o2 = ms + (bb*sc - ms)*keep;
            if (o0 < 0) o0 = 0; else if (o0 > ALWAN_CORE_LITERAL(1.0)) o0 = ALWAN_CORE_LITERAL(1.0);
            if (o1 < 0) o1 = 0; else if (o1 > ALWAN_CORE_LITERAL(1.0)) o1 = ALWAN_CORE_LITERAL(1.0);
            if (o2 < 0) o2 = 0; else if (o2 > ALWAN_CORE_LITERAL(1.0)) o2 = ALWAN_CORE_LITERAL(1.0);
            out[i*3]=o0; out[i*3+1]=o1; out[i*3+2]=o2;
        }
        return ALWAN_OK;
    }

    /* ---- WARP: monotone smooth local-contrast refinement of HEMISPHERE_ABS ----
     * Closed-form (NO solver). On the HEMISPHERE_ABS base, restore the scene curvature the tonescale
     * flattened, as a monotone smooth warp of the base log-carrier:
     *   f_out = m + gamma*(f_H - m),   m = box*3 smooth pivot of f_H = log2 max(base),
     *   gamma = 1 + BETA_LC*blur(|mean-curvature of log2 max(scene)|) normalized to [0,1].
     * gamma >= 0 and smooth => monotone at every significant carrier edge (no flip); analytic => no
     * pooling/staircase. Reconstruct by a per-pixel POSITIVE scale r = 2^(f_out - f_H) applied to all
     * three channels (ratio-preserving), capped so the carrier stays <= the base rail ceiling
     * (RATIO-safe: a single uniform scale, never a per-channel clamp -> out_chroma stays || base_chroma,
     * so WARP inherits HEMISPHERE_ABS's constraint row rather than rotating hue). Lifts
     * isophote-through-highlight over HEMISPHERE_ABS and removes its highlight curvature steepening. */
    if (p->method == ALWAN_GAMUT_FORM_WARP) {
        ALWAN_CORE_T const BETA_LC = ALWAN_CORE_LITERAL(0.9);   /* local-contrast strength (validated default) */
        ALWAN_CORE_T const GLIM    = ALWAN_CORE_LITERAL(0.30);  /* max |log2 gain|: bounds contrast, ring/clip-free */
        ALWAN_CORE_T const rhi     = ALWAN_CORE_LITERAL(0.02);  /* HEMISPHERE_ABS high-rail width (asymptotic, no plateau) */
        int rp = W/40; if (rp < 1) rp = 1;                      /* pivot radius     ~ 0.025 W */
        int rg = W/20; if (rg < 1) rg = 1;                      /* gain-map radius  ~ 0.05  W */
        /* 1) base = HEMISPHERE_ABS (recurse into out) */
        ALWAN_CORE_FN(alwan_gamut_spatial_params) pb = *p; pb.method = ALWAN_GAMUT_FORM_HEMISPHERE_ABS;
        { int st = ALWAN_CORE_FN(alwan__gamut_spatial_core)(out, in, depth, W, H, &pb, ctx); if (st != ALWAN_OK) return st; }
        /* 2) scratch */
        ALWAN_CORE_T *fH  = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        ALWAN_CORE_T *pv  = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        ALWAN_CORE_T *ll  = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        ALWAN_CORE_T *kap = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        ALWAN_CORE_T *gm  = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        ALWAN_CORE_T *tmp = (ALWAN_CORE_T*)ctx->alloc_fn(N*sizeof(ALWAN_CORE_T),16);
        if (!fH||!pv||!ll||!kap||!gm||!tmp){ ctx->free_fn(fH);ctx->free_fn(pv);ctx->free_fn(ll);ctx->free_fn(kap);ctx->free_fn(gm);ctx->free_fn(tmp); return ALWAN_E_NOMEM; }
        /* 3) log2 carriers: fH = base, ll = scene */
        for (size_t i=0;i<N;i++){
            ALWAN_CORE_T bc = out[i*3]; if(out[i*3+1]>bc)bc=out[i*3+1]; if(out[i*3+2]>bc)bc=out[i*3+2];
            fH[i] = ALWAN_CORE_LOG2(bc > ALWAN_CORE_LITERAL(1e-4) ? bc : ALWAN_CORE_LITERAL(1e-4));
            ALWAN_CORE_T sr=in[i*3],sg=in[i*3+1],sb=in[i*3+2]; if(sr<0)sr=0;if(sg<0)sg=0;if(sb<0)sb=0;
            ALWAN_CORE_T sc = sr>sg?(sr>sb?sr:sb):(sg>sb?sg:sb);
            ll[i] = ALWAN_CORE_LOG2(sc > ALWAN_CORE_LITERAL(1e-4) ? sc : ALWAN_CORE_LITERAL(1e-4));
        }
        /* 4) mean-curvature magnitude of the scene log carrier (0 at border) */
        for (size_t i=0;i<N;i++) kap[i]=ALWAN_CORE_LITERAL(0.0);
        for (int y=1;y<H-1;y++) for (int x=1;x<W-1;x++){ size_t i=(size_t)y*W+x;
            ALWAN_CORE_T fx=(ll[i+1]-ll[i-1])*ALWAN_CORE_LITERAL(0.5), fy=(ll[i+W]-ll[i-W])*ALWAN_CORE_LITERAL(0.5);
            ALWAN_CORE_T fxx=ll[i+1]-ALWAN_CORE_LITERAL(2.0)*ll[i]+ll[i-1], fyy=ll[i+W]-ALWAN_CORE_LITERAL(2.0)*ll[i]+ll[i-W];
            ALWAN_CORE_T fxy=(ll[i+1+W]-ll[i-1+W]-ll[i+1-W]+ll[i-1-W])*ALWAN_CORE_LITERAL(0.25);
            ALWAN_CORE_T gmg=fx*fx+fy*fy;
            ALWAN_CORE_T Hc=((ALWAN_CORE_LITERAL(1.0)+fx*fx)*fyy - ALWAN_CORE_LITERAL(2.0)*fx*fy*fxy + (ALWAN_CORE_LITERAL(1.0)+fy*fy)*fxx)
                            /(ALWAN_CORE_LITERAL(2.0)*ALWAN_CORE_POW(ALWAN_CORE_LITERAL(1.0)+gmg,ALWAN_CORE_LITERAL(1.5)));
            kap[i] = Hc < ALWAN_CORE_LITERAL(0.0) ? -Hc : Hc;
        }
        /* 5) winsorize the curvature to its p90 BEFORE blurring, then normalize to [0,1] -- concentrate the
         *    boost on the strongest structural curvature and saturate the /|grad|-style spikes (a raw
         *    max-normalize would let one spike flatten gamma to 1). Spike-robust p90 via a 256-bin histogram
         *    (sort-free, precision-agnostic). Then gain map gm = box*3, normalized to [0,1]. */
        { ALWAN_CORE_T mx=ALWAN_CORE_LITERAL(1e-12); for(size_t i=0;i<N;i++) if(kap[i]>mx)mx=kap[i];
          int hist[256]; for(int b=0;b<256;b++) hist[b]=0;
          ALWAN_CORE_T sc255=ALWAN_CORE_LITERAL(255.0)/mx;
          for(size_t i=0;i<N;i++){ int b=(int)(kap[i]*sc255); if(b<0)b=0; if(b>255)b=255; hist[b]++; }
          size_t target=(N/10)*9, cum=0; int pbin=255;   /* ~90th percentile (integer, no float cast) */
          for(int b=0;b<256;b++){ cum+=(size_t)hist[b]; if(cum>=target){ pbin=b; break; } }
          ALWAN_CORE_T K=((ALWAN_CORE_T)pbin+ALWAN_CORE_LITERAL(0.5))/ALWAN_CORE_LITERAL(256.0)*mx; if(K<ALWAN_CORE_LITERAL(1e-9))K=ALWAN_CORE_LITERAL(1.0);
          for(size_t i=0;i<N;i++){ ALWAN_CORE_T v=kap[i]/K; if(v>ALWAN_CORE_LITERAL(1.0))v=ALWAN_CORE_LITERAL(1.0); kap[i]=v; } }
        ALWAN_CORE_FN(alwan__boxfilter)(kap, gm,  tmp, W,H, rg);
        ALWAN_CORE_FN(alwan__boxfilter)(gm,  kap, tmp, W,H, rg);
        ALWAN_CORE_FN(alwan__boxfilter)(kap, gm,  tmp, W,H, rg);
        { ALWAN_CORE_T mx=ALWAN_CORE_LITERAL(1e-12); for(size_t i=0;i<N;i++) if(gm[i]>mx)mx=gm[i];
          ALWAN_CORE_T inv=ALWAN_CORE_LITERAL(1.0)/mx; for(size_t i=0;i<N;i++) gm[i]*=inv; }
        /* 6) smooth pivot pv = box*3(fH) */
        ALWAN_CORE_FN(alwan__boxfilter)(fH, pv,  tmp, W,H, rp);
        ALWAN_CORE_FN(alwan__boxfilter)(pv, kap, tmp, W,H, rp);
        ALWAN_CORE_FN(alwan__boxfilter)(kap,pv,  tmp, W,H, rp);
        /* 7) warp + ratio-preserving scale with RATIO-safe carrier cap */
        for (size_t i=0;i<N;i++){
            ALWAN_CORE_T gamma = ALWAN_CORE_LITERAL(1.0) + BETA_LC*gm[i];
            ALWAN_CORE_T fo = pv[i] + gamma*(fH[i]-pv[i]);
            ALWAN_CORE_T dg = fo - fH[i]; if(dg>GLIM)dg=GLIM; if(dg<-GLIM)dg=-GLIM;
            ALWAN_CORE_T r = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(2.0), dg);
            ALWAN_CORE_T bc = out[i*3]; if(out[i*3+1]>bc)bc=out[i*3+1]; if(out[i*3+2]>bc)bc=out[i*3+2];
            ALWAN_CORE_T co = bc*r;   /* boosted carrier -> asymptotic high rail (no plateau) -> uniform scale */
            if (co > ALWAN_CORE_LITERAL(1.0) - rhi) co = ALWAN_CORE_LITERAL(1.0) - rhi*ALWAN_CORE_EXP(((ALWAN_CORE_LITERAL(1.0)-co) - rhi)/rhi);
            r = bc > ALWAN_CORE_LITERAL(1e-9) ? co/bc : ALWAN_CORE_LITERAL(1.0);
            for (int k=0;k<3;k++){ ALWAN_CORE_T v=out[i*3+k]*r; if(v<0)v=0; if(v>ALWAN_CORE_LITERAL(1.0))v=ALWAN_CORE_LITERAL(1.0); out[i*3+k]=v; }
        }
        ctx->free_fn(fH);ctx->free_fn(pv);ctx->free_fn(ll);ctx->free_fn(kap);ctx->free_fn(gm);ctx->free_fn(tmp);
        return ALWAN_OK;
    }

    /* ---- OPTICAL: SURFACE + Kitaoka additive/multiplicative split ----
     * Density model. Luminance preserved (0 luma flips). The chroma-collapse
     * target depends on whether the pixel is an ADDITIVE increment (bright /
     * light -> desaturate toward white) or a MULTIPLICATIVE decrement (coloured
     * absorptive surface -> keep hue, collapse only as much as gamut needs).
     * Increment weight is taken from luminance (Troy: increments = air/light =
     * bright; decrements = colour/shade), a tractable proxy for the full
     * X-junction analysis. Then smoothed within the surface envelope. */
    if (p->method == ALWAN_GAMUT_FORM_OPTICAL) {
        ALWAN_CORE_T const wr = ALWAN_CORE_LUMA_KR_BT709, wg = ALWAN_CORE_LUMA_KG_BT709, wb = ALWAN_CORE_LUMA_KB_BT709;
        ALWAN_CORE_T *lsc   = (ALWAN_CORE_T *)ctx->alloc_fn(N * sizeof(ALWAN_CORE_T), 16);
        ALWAN_CORE_T *smaxf = (ALWAN_CORE_T *)ctx->alloc_fn(N * sizeof(ALWAN_CORE_T), 16);
        ALWAN_CORE_T *sf    = (ALWAN_CORE_T *)ctx->alloc_fn(N * sizeof(ALWAN_CORE_T), 16);
        ALWAN_CORE_T *sn    = (ALWAN_CORE_T *)ctx->alloc_fn(N * sizeof(ALWAN_CORE_T), 16);
        if (!lsc || !smaxf || !sf || !sn) { ctx->free_fn(lsc); ctx->free_fn(smaxf); ctx->free_fn(sf); ctx->free_fn(sn); return ALWAN_E_NOMEM; }
        for (size_t i = 0; i < N; i++) {
            ALWAN_CORE_T r = in[i*3+0], g = in[i*3+1], b = in[i*3+2];
            ALWAN_CORE_T L = wr*r + wg*g + wb*b; if (L < ALWAN_CORE_LITERAL(0.0)) L = ALWAN_CORE_LITERAL(0.0);
            ALWAN_CORE_T lscale = ALWAN_CORE_LITERAL(1.0);
            if (L > peak) { ALWAN_CORE_T Lt = peak * (ALWAN_CORE_LITERAL(1.0) + L/(peak*peak)) / (ALWAN_CORE_LITERAL(1.0) + L/peak); lscale = Lt / L; }
            ALWAN_CORE_T r2 = r*lscale, g2 = g*lscale, b2 = b*lscale, L2 = L*lscale;
            ALWAN_CORE_T cr = r2-L2, cg = g2-L2, cb = b2-L2;
            ALWAN_CORE_T smax = ALWAN_CORE_LITERAL(1e30), l;
            if (cr >  ALWAN_CORE_LITERAL(1e-12)) { l = (peak-L2)/cr; if (l<smax) smax=l; } else if (cr < -ALWAN_CORE_LITERAL(1e-12)) { l = (ALWAN_CORE_LITERAL(0.0)-L2)/cr; if (l<smax) smax=l; }
            if (cg >  ALWAN_CORE_LITERAL(1e-12)) { l = (peak-L2)/cg; if (l<smax) smax=l; } else if (cg < -ALWAN_CORE_LITERAL(1e-12)) { l = (ALWAN_CORE_LITERAL(0.0)-L2)/cg; if (l<smax) smax=l; }
            if (cb >  ALWAN_CORE_LITERAL(1e-12)) { l = (peak-L2)/cb; if (l<smax) smax=l; } else if (cb < -ALWAN_CORE_LITERAL(1e-12)) { l = (ALWAN_CORE_LITERAL(0.0)-L2)/cb; if (l<smax) smax=l; }
            if (smax < ALWAN_CORE_LITERAL(0.0)) smax = ALWAN_CORE_LITERAL(0.0);
            /* additive (increment) weight from luminance -> extra desaturation */
            ALWAN_CORE_T a = ALWAN_CORE_FN(alwan__smoothstep)((L2/peak - ALWAN_CORE_LITERAL(0.5)) / ALWAN_CORE_LITERAL(0.45));
            ALWAN_CORE_T target = (smax < ALWAN_CORE_LITERAL(1.0)) ? smax * (ALWAN_CORE_LITERAL(1.0) - a*ALWAN_CORE_LITERAL(0.55)) : ALWAN_CORE_LITERAL(1.0);   /* light -> whiter, surface -> keep colour */
            lsc[i] = lscale; smaxf[i] = smax; sf[i] = target;
        }
        {
            int const dx4[4] = {-1,1,0,0}, dy4[4] = {0,0,-1,1};
            int sm = (int)((ALWAN_CORE_LITERAL(1.0) - s) * (ALWAN_CORE_T)(iters > 0 ? iters : 200));
            for (int it = 0; it < sm; it++) {
                for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
                    size_t i = (size_t)y*W + x; ALWAN_CORE_T zi = depth ? depth[i] : ALWAN_CORE_LITERAL(0.0);
                    ALWAN_CORE_T acc = sf[i], wsum = ALWAN_CORE_LITERAL(1.0);
                    for (int n = 0; n < 4; n++) {
                        int nx=x+dx4[n], ny=y+dy4[n]; if (nx<0||nx>=W||ny<0||ny>=H) continue;
                        size_t j = (size_t)ny*W + nx; ALWAN_CORE_T w = ALWAN_CORE_LITERAL(1.0);
                        if (dsig > ALWAN_CORE_LITERAL(0.0) && depth) { ALWAN_CORE_T dz = (zi - depth[j])/dsig; w = ALWAN_CORE_EXP(-dz*dz); }
                        acc += w*sf[j]; wsum += w;
                    }
                    ALWAN_CORE_T v = acc/wsum; if (v > smaxf[i]) v = smaxf[i]; sn[i] = v;
                }
                { ALWAN_CORE_T *tmp = sf; sf = sn; sn = tmp; }
            }
        }
        for (size_t i = 0; i < N; i++) {
            ALWAN_CORE_T r = in[i*3+0]*lsc[i], g = in[i*3+1]*lsc[i], b = in[i*3+2]*lsc[i];
            ALWAN_CORE_T L2 = wr*r + wg*g + wb*b, ss = sf[i];
            ALWAN_CORE_T or_ = L2 + (r-L2)*ss, og = L2 + (g-L2)*ss, ob = L2 + (b-L2)*ss;
            out[i*3+0] = or_<0?0:(or_>peak?peak:or_);
            out[i*3+1] = og <0?0:(og >peak?peak:og);
            out[i*3+2] = ob <0?0:(ob >peak?peak:ob);
        }
        ctx->free_fn(lsc); ctx->free_fn(smaxf); ctx->free_fn(sf); ctx->free_fn(sn);
        return ALWAN_OK;
    }

    /* ---- GAIN: ratio-preserving smooth-gain field ----
     * Per pixel gain gi = peak/max(rgb) when the pixel overshoots, else 1, so
     * in*gi fits with hue/chroma preserved. Optionally diffuse the gain (capped
     * by each pixel's ceiling) so the darkening spreads instead of being purely
     * local; `s` controls the spread (s=1 -> per-pixel, halo-free). */
    if (p->method == ALWAN_GAMUT_FORM_GAIN) {
        ALWAN_CORE_T *ceil_ = (ALWAN_CORE_T *)ctx->alloc_fn(N * sizeof(ALWAN_CORE_T), 16);
        ALWAN_CORE_T *gg    = (ALWAN_CORE_T *)ctx->alloc_fn(N * sizeof(ALWAN_CORE_T), 16);
        ALWAN_CORE_T *gn    = (ALWAN_CORE_T *)ctx->alloc_fn(N * sizeof(ALWAN_CORE_T), 16);
        if (!ceil_ || !gg || !gn) { ctx->free_fn(ceil_); ctx->free_fn(gg); ctx->free_fn(gn); return ALWAN_E_NOMEM; }
        for (size_t i = 0; i < N; i++) {
            ALWAN_CORE_T r = in[i*3+0], g = in[i*3+1], b = in[i*3+2];
            ALWAN_CORE_T m = r>g ? (r>b?r:b) : (g>b?g:b);
            ALWAN_CORE_T cg = (m > peak) ? peak/m : ALWAN_CORE_LITERAL(1.0);
            if (cg < ALWAN_CORE_LITERAL(0.0)) cg = ALWAN_CORE_LITERAL(0.0);
            ceil_[i] = cg; gg[i] = cg;
        }
        {
            int const dx4[4] = {-1,1,0,0}, dy4[4] = {0,0,-1,1};
            int sm = (int)((ALWAN_CORE_LITERAL(1.0) - s) * (ALWAN_CORE_T)(iters > 0 ? iters : 200));
            for (int it = 0; it < sm; it++) {
                for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
                    size_t i = (size_t)y*W + x;
                    ALWAN_CORE_T acc = ALWAN_CORE_LITERAL(0.0); int cnt = 0;
                    for (int n = 0; n < 4; n++) {
                        int nx=x+dx4[n], ny=y+dy4[n];
                        if (nx<0||nx>=W||ny<0||ny>=H) continue;
                        acc += gg[(size_t)ny*W+nx]; cnt++;
                    }
                    ALWAN_CORE_T v = cnt ? acc/cnt : gg[i];
                    if (v > ceil_[i]) v = ceil_[i];   /* obstacle: never exceed the pixel ceiling */
                    gn[i] = v;
                }
                { ALWAN_CORE_T *tmp = gg; gg = gn; gn = tmp; }
            }
        }
        for (size_t i = 0; i < N; i++) {
            for (int k = 0; k < 3; k++) {
                ALWAN_CORE_T v = in[i*3+k] * gg[i];
                out[i*3+k] = v<0?0:(v>peak?peak:v);
            }
        }
        ctx->free_fn(ceil_); ctx->free_fn(gg); ctx->free_fn(gn);
        return ALWAN_OK;
    }

    /* ---- GRADIENT (default): per-channel gradient-domain field ---- */
    /* scratch */
    unsigned char *mask = (unsigned char *)ctx->alloc_fn(N, 16);
    unsigned char *fixed = (unsigned char *)ctx->alloc_fn(N, 16);
    ALWAN_CORE_T *dist2 = (ALWAN_CORE_T *)ctx->alloc_fn(N * sizeof(ALWAN_CORE_T), 16);
    ALWAN_CORE_T *lam   = (ALWAN_CORE_T *)ctx->alloc_fn(N * sizeof(ALWAN_CORE_T), 16);
    ALWAN_CORE_T *u     = (ALWAN_CORE_T *)ctx->alloc_fn(3 * N * sizeof(ALWAN_CORE_T), 16);
    ALWAN_CORE_T *un    = (ALWAN_CORE_T *)ctx->alloc_fn(3 * N * sizeof(ALWAN_CORE_T), 16);
    int mxwh = W > H ? W : H;
    ALWAN_CORE_T *fbuf  = (ALWAN_CORE_T *)ctx->alloc_fn((size_t)mxwh * sizeof(ALWAN_CORE_T), 16);
    ALWAN_CORE_T *dbuf  = (ALWAN_CORE_T *)ctx->alloc_fn((size_t)mxwh * sizeof(ALWAN_CORE_T), 16);
    int       *vbuf  = (int *)      ctx->alloc_fn((size_t)(mxwh) * sizeof(int), 16);
    ALWAN_CORE_T *zbuf  = (ALWAN_CORE_T *)ctx->alloc_fn((size_t)(mxwh + 1) * sizeof(ALWAN_CORE_T), 16);
    if (!mask || !fixed || !dist2 || !lam || !u || !un || !fbuf || !dbuf || !vbuf || !zbuf) {
        ctx->free_fn(mask); ctx->free_fn(fixed); ctx->free_fn(dist2); ctx->free_fn(lam); ctx->free_fn(u);
        ctx->free_fn(un); ctx->free_fn(fbuf); ctx->free_fn(dbuf); ctx->free_fn(vbuf); ctx->free_fn(zbuf);
        return ALWAN_E_NOMEM;
    }

    /* out-of-gamut mask + init u = clamp(in) */
    for (size_t i = 0; i < N; i++) {
        int oog = 0;
        for (int k = 0; k < 3; k++) {
            ALWAN_CORE_T x = in[i*3 + k];
            if (x < -eps || x > peak + eps) oog = 1;
            u[i*3 + k] = x < ALWAN_CORE_LITERAL(0.0) ? ALWAN_CORE_LITERAL(0.0) : (x > peak ? peak : x);
        }
        mask[i] = (unsigned char)oog;
    }

    /* spatial distance to the OOG set -> per-pixel fidelity weight lambda */
    ALWAN_CORE_FN(alwan__edt_2d)(mask, dist2, W, H, fbuf, dbuf, vbuf, zbuf);
    for (size_t i = 0; i < N; i++) {
        if (mask[i]) { lam[i] = ALWAN_CORE_LITERAL(0.0); fixed[i] = 0; continue; }   /* OOG pixels fully free */
        ALWAN_CORE_T D = ALWAN_CORE_SQRT(dist2[i]) / reach;            /* normalized spatial distance */
        if (D > ALWAN_CORE_LITERAL(1.0)) D = ALWAN_CORE_LITERAL(1.0);
        /* far (D large) -> locked; near (D small) -> free (0). s shifts the band:
         * s=1 -> band at 0 -> almost everything locked; s=0 -> band at 1 -> all free. */
        ALWAN_CORE_T t = (D - (ALWAN_CORE_LITERAL(1.0) - s)) / feather;
        lam[i] = lam0 * ALWAN_CORE_FN(alwan__smoothstep)(t);
        /* Beyond the transition band, hard-fix as a Dirichlet boundary condition
         * (an in-gamut pixel = its input) so `s` crisply bounds what can move. */
        fixed[i] = (t >= ALWAN_CORE_LITERAL(1.0)) ? 1 : 0;
    }

    /* projected Jacobi: minimize  sum lam(u-x)^2 + beta sum w((ui-uj)-c*gij)^2
     * subject to box [0,peak] and per-channel monotonicity (polarity). */
    int const dx[4] = {-1, 1, 0, 0};
    int const dy[4] = {0, 0, -1, 1};
    for (int it = 0; it < iters; it++) {
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                size_t i = (size_t)y*W + x;
                if (fixed[i]) { for (int k = 0; k < 3; k++) un[i*3+k] = u[i*3+k]; continue; }
                ALWAN_CORE_T zi = depth ? depth[i] : ALWAN_CORE_LITERAL(0.0);
                for (int k = 0; k < 3; k++) {
                    ALWAN_CORE_T acc = lam[i] * in[i*3 + k];
                    ALWAN_CORE_T den = lam[i];
                    for (int n = 0; n < 4; n++) {
                        int nx = x + dx[n], ny = y + dy[n];
                        if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
                        size_t j = (size_t)ny*W + nx;
                        ALWAN_CORE_T w = beta;
                        if (dsig > ALWAN_CORE_LITERAL(0.0) && depth) {          /* gate coupling across depth jumps */
                            ALWAN_CORE_T dz = (zi - depth[j]) / dsig;
                            w *= ALWAN_CORE_EXP(-dz*dz);
                        }
                        ALWAN_CORE_T gij = in[i*3 + k] - in[j*3 + k];   /* input gradient */
                        acc += w * (u[j*3 + k] + c * gij);
                        den += w;
                    }
                    ALWAN_CORE_T val = den > ALWAN_CORE_LITERAL(0.0) ? acc / den : u[i*3 + k];
                    if (val < ALWAN_CORE_LITERAL(0.0)) val = ALWAN_CORE_LITERAL(0.0); else if (val > peak) val = peak;   /* box */
                    un[i*3 + k] = val;
                }
            }
        }
        /* swap */
        ALWAN_CORE_T *tmp = u; u = un; un = tmp;

        /* polarity projection: repair per-channel sign reversals vs the input
         * (a few fixed-order pairwise midpoint projections; deterministic). */
        for (int rep = 0; rep < 2; rep++) {
            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    size_t i = (size_t)y*W + x;
                    for (int n = 1; n < 4; n += 2) {       /* right (dx=1) and down (dy=1) edges only */
                        int nx = x + dx[n], ny = y + dy[n];
                        if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
                        size_t j = (size_t)ny*W + nx;
                        if (fixed[i] && fixed[j]) continue;    /* both pinned: nothing to do */
                        for (int k = 0; k < 3; k++) {
                            ALWAN_CORE_T gin = in[i*3+k] - in[j*3+k];
                            ALWAN_CORE_T gout = u[i*3+k] - u[j*3+k];
                            if (gin > ALWAN_CORE_LITERAL(0.0) ? gout < ALWAN_CORE_LITERAL(0.0) : (gin < ALWAN_CORE_LITERAL(0.0) ? gout > ALWAN_CORE_LITERAL(0.0) : 0)) {
                                if (fixed[i])      u[j*3+k] = u[i*3+k];   /* move only the free side */
                                else if (fixed[j]) u[i*3+k] = u[j*3+k];
                                else { ALWAN_CORE_T m = ALWAN_CORE_LITERAL(0.5)*(u[i*3+k]+u[j*3+k]); u[i*3+k]=m; u[j*3+k]=m; }
                            }
                        }
                    }
                }
            }
        }
    }

    /* Final polarity repair to convergence: cyclic projection (POCS) onto the
     * per-edge monotone half-spaces. A monotone/flat field is always feasible
     * (clip is one), so this terminates; alternate sweep direction to reduce
     * order bias. Guarantees ~0 per-channel flips even on aggressive content. */
    for (int pass = 0; pass < 400; pass++) {
        int changed = 0;
        int y0 = (pass & 1) ? H - 1 : 0;
        int y1 = (pass & 1) ? -1 : H;
        int ys = (pass & 1) ? -1 : 1;
        for (int y = y0; y != y1; y += ys) {
            for (int x = 0; x < W; x++) {
                size_t i = (size_t)y*W + x;
                for (int n = 1; n < 4; n += 2) {
                    int nx = x + dx[n], ny = y + dy[n];
                    if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
                    size_t j = (size_t)ny*W + nx;
                    if (fixed[i] && fixed[j]) continue;
                    for (int k = 0; k < 3; k++) {
                        ALWAN_CORE_T gin = in[i*3+k] - in[j*3+k];
                        ALWAN_CORE_T gout = u[i*3+k] - u[j*3+k];
                        if (gin > ALWAN_CORE_LITERAL(0.0) ? gout < ALWAN_CORE_LITERAL(0.0) : (gin < ALWAN_CORE_LITERAL(0.0) ? gout > ALWAN_CORE_LITERAL(0.0) : 0)) {
                            if (fixed[i])      u[j*3+k] = u[i*3+k];
                            else if (fixed[j]) u[i*3+k] = u[j*3+k];
                            else { ALWAN_CORE_T m = ALWAN_CORE_LITERAL(0.5)*(u[i*3+k]+u[j*3+k]); u[i*3+k]=m; u[j*3+k]=m; }
                            changed = 1;
                        }
                    }
                }
            }
        }
        if (!changed) break;
    }

    for (size_t i = 0; i < 3*N; i++) out[i] = u[i];

    ctx->free_fn(mask); ctx->free_fn(fixed); ctx->free_fn(dist2); ctx->free_fn(lam); ctx->free_fn(u);
    ctx->free_fn(un); ctx->free_fn(fbuf); ctx->free_fn(dbuf); ctx->free_fn(vbuf); ctx->free_fn(zbuf);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FN(alwan_gamut_map_spatial)(ALWAN_CORE_T *out, ALWAN_CORE_T const *in, ALWAN_CORE_T const *depth,
                                           int width, int height,
                                           ALWAN_CORE_FN(alwan_gamut_spatial_params) const *params, alwan_ctx *ctx) {
    return ALWAN_CORE_FN(alwan__gamut_spatial_core)(out, in, depth, width, height, params, ctx);
}
