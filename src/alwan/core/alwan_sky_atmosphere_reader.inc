/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Lookups into precomputed atmospheric scattering tables, written against an accessor
 * instead of a pointer. No include guard - included once per binding.
 *
 * The lookup functions are a C port of Eric Bruneton's atmosphere/functions.glsl
 * (Precomputed Atmospheric Scattering: a New Implementation, 2017), whose notice is:
 *
 *   Copyright (c) 2017 Eric Bruneton
 *   All rights reserved.
 *   Redistribution and use in source and binary forms, with or without modification,
 *   are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice, this
 *      list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice, this
 *      list of conditions and the following disclaimer in the documentation and/or
 *      other materials provided with the distribution.
 *   3. Neither the name of the copyright holder nor the names of its contributors may
 *      be used to endorse or promote products derived from this software without
 *      specific prior written permission.
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *   IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 *   INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *   NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE.
 *
 * ================================================================
 * THE TABLE
 *
 * One buffer of K channels per texel, channel fastest. A header of 16 values, then:
 *     transmittance  TW x TH x K   only when header[14] is 1 (spectral tables)
 *     sun            TW x TH x K   solar irradiance x transmittance to the top
 *     scattering     (NU MU_S) x MU x R x K   Rayleigh + multiple, over the Rayleigh phase
 *     single Mie     (NU MU_S) x MU x R x K
 *     irradiance     IW x IH x K   from the sky, on a horizontal surface
 * header: 0 bottom radius, 1 top radius (m), 2 cos of the largest sun zenith angle,
 * 3 Mie g, 4 sun angular radius (rad), 5 K, 6 TW, 7 TH, 8 R, 9 MU, 10 MU_S, 11 NU,
 * 12 IW, 13 IH, 14 transmittance present, 15 unused. Sizes are whole numbers held as
 * values (exact below 2^24).
 *
 * Sampling is clamp-to-edge bilinear (2D) and trilinear (3D) with texel centres at
 * (i + 0.5) / n, as the reference implementation's textures sample.
 *
 * ACCESSOR. Nothing here names a pointer. Tables are read through
 *     ALWAN_ATM_READ(i)
 * POINTER BINDING: alwan_sky_core.inc defines ALWAN_ATM_READER_POINTER and includes
 * this file where pointers exist; the table is then the leading parameter. NAMED
 * BINDING: a shader defines
 *
 *     StructuredBuffer<float> Atm : register(t4);
 *     #define ALWAN_ATM_NAME     earth
 *     #define ALWAN_ATM_READ(i)  Atm[i]
 *     #include "core/alwan_sky_atmosphere_reader.inc"
 *
 *     float L = alwan_sky_atm_sky_radiance_earth(c, r, mu, mu_s, nu);
 *
 * and uploads the table alwan_sky_get_layout_f32 hands back.
 * ================================================================ */

#if defined(ALWAN_ATM_READER_POINTER)

# define ALWAN_ATM_FN_(base)      ALWAN_CORE_FNVLIT(base)
# define ALWAN_ATM_PARAMS_        const ALWAN_CORE_T *tab,
# define ALWAN_ATM_ARGS_          tab,
# define ALWAN_ATM_READ(i)        tab[i]

#else

# if !defined(ALWAN_ATM_NAME) || !defined(ALWAN_ATM_READ)
#  error "alwan_sky_atmosphere_reader.inc: define ALWAN_ATM_NAME and ALWAN_ATM_READ before including it"
# endif
# define ALWAN_ATM_PASTE2_(a, b)  a##_##b
# define ALWAN_ATM_PASTE_(a, b)   ALWAN_ATM_PASTE2_(a, b)
# define ALWAN_ATM_FN_(base)      ALWAN_ATM_PASTE_(base, ALWAN_ATM_NAME)
# define ALWAN_ATM_PARAMS_
# define ALWAN_ATM_ARGS_

#endif

ALWAN_INLINE int ALWAN_ATM_FN_(alwan_sky_atm_int)(ALWAN_ATM_PARAMS_ int i) {
    return (int)(ALWAN_ATM_READ(i) + ALWAN_CORE_LITERAL(0.5));
}

/* Offsets of the five tables. which: 0 transmittance, 1 sun, 2 scattering, 3 Mie,
 * 4 irradiance. */
ALWAN_INLINE int ALWAN_ATM_FN_(alwan_sky_atm_offset)(ALWAN_ATM_PARAMS_ int which) {
    const int k = ALWAN_ATM_FN_(alwan_sky_atm_int)(ALWAN_ATM_ARGS_ 5);
    const int t2 = ALWAN_ATM_FN_(alwan_sky_atm_int)(ALWAN_ATM_ARGS_ 6) * ALWAN_ATM_FN_(alwan_sky_atm_int)(ALWAN_ATM_ARGS_ 7) * k;
    const int s3 = ALWAN_ATM_FN_(alwan_sky_atm_int)(ALWAN_ATM_ARGS_ 8) * ALWAN_ATM_FN_(alwan_sky_atm_int)(ALWAN_ATM_ARGS_ 9)
                 * ALWAN_ATM_FN_(alwan_sky_atm_int)(ALWAN_ATM_ARGS_ 10) * ALWAN_ATM_FN_(alwan_sky_atm_int)(ALWAN_ATM_ARGS_ 11) * k;
    const int has_t = ALWAN_ATM_FN_(alwan_sky_atm_int)(ALWAN_ATM_ARGS_ 14);
    int off = 16;
    if (which == 0) return off;
    off += has_t * t2;
    if (which == 1) return off;
    off += t2;
    if (which == 2) return off;
    off += s3;
    if (which == 3) return off;
    return off + s3;
}

/* Clamp-to-edge index of texel coordinate i in [0, n). */
ALWAN_INLINE int ALWAN_ATM_FN_(alwan_sky_atm_clampi)(int i, int n) {
    return i < 0 ? 0 : (i > n - 1 ? n - 1 : i);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_ATM_FN_(alwan_sky_atm_tex2d)(ALWAN_ATM_PARAMS_ int base, int w, int h, int k, int c,
                                                             ALWAN_CORE_T x, ALWAN_CORE_T y) {
    ALWAN_CORE_T u = x * (ALWAN_CORE_T)w - ALWAN_CORE_LITERAL(0.5);
    ALWAN_CORE_T v = y * (ALWAN_CORE_T)h - ALWAN_CORE_LITERAL(0.5);
    const ALWAN_CORE_T fu = ALWAN_CORE_FLOOR(u), fv = ALWAN_CORE_FLOOR(v);
    const int i = (int)fu, j = (int)fv;
    const int i0 = ALWAN_ATM_FN_(alwan_sky_atm_clampi)(i, w);
    const int i1 = ALWAN_ATM_FN_(alwan_sky_atm_clampi)(i + 1, w);
    const int j0 = ALWAN_ATM_FN_(alwan_sky_atm_clampi)(j, h);
    const int j1 = ALWAN_ATM_FN_(alwan_sky_atm_clampi)(j + 1, h);
    u -= fu;
    v -= fv;
    return ALWAN_ATM_READ(base + (i0 + j0 * w) * k + c) * ((ALWAN_CORE_LITERAL(1.0) - u) * (ALWAN_CORE_LITERAL(1.0) - v))
         + ALWAN_ATM_READ(base + (i1 + j0 * w) * k + c) * (u * (ALWAN_CORE_LITERAL(1.0) - v))
         + ALWAN_ATM_READ(base + (i0 + j1 * w) * k + c) * ((ALWAN_CORE_LITERAL(1.0) - u) * v)
         + ALWAN_ATM_READ(base + (i1 + j1 * w) * k + c) * (u * v);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_ATM_FN_(alwan_sky_atm_tex3d)(ALWAN_ATM_PARAMS_ int base, int w, int h, int d, int k, int c,
                                                             ALWAN_CORE_T x, ALWAN_CORE_T y, ALWAN_CORE_T z) {
    ALWAN_CORE_T u = x * (ALWAN_CORE_T)w - ALWAN_CORE_LITERAL(0.5);
    ALWAN_CORE_T v = y * (ALWAN_CORE_T)h - ALWAN_CORE_LITERAL(0.5);
    ALWAN_CORE_T s = z * (ALWAN_CORE_T)d - ALWAN_CORE_LITERAL(0.5);
    const ALWAN_CORE_T fu = ALWAN_CORE_FLOOR(u), fv = ALWAN_CORE_FLOOR(v), fs = ALWAN_CORE_FLOOR(s);
    const int i = (int)fu, j = (int)fv, l = (int)fs;
    const int i0 = ALWAN_ATM_FN_(alwan_sky_atm_clampi)(i, w);
    const int i1 = ALWAN_ATM_FN_(alwan_sky_atm_clampi)(i + 1, w);
    const int j0 = ALWAN_ATM_FN_(alwan_sky_atm_clampi)(j, h);
    const int j1 = ALWAN_ATM_FN_(alwan_sky_atm_clampi)(j + 1, h);
    const int l0 = ALWAN_ATM_FN_(alwan_sky_atm_clampi)(l, d);
    const int l1 = ALWAN_ATM_FN_(alwan_sky_atm_clampi)(l + 1, d);
    const int wh = w * h;
    u -= fu;
    v -= fv;
    s -= fs;
    return ALWAN_ATM_READ(base + (i0 + j0 * w + l0 * wh) * k + c) * ((ALWAN_CORE_LITERAL(1.0) - u) * (ALWAN_CORE_LITERAL(1.0) - v) * (ALWAN_CORE_LITERAL(1.0) - s))
         + ALWAN_ATM_READ(base + (i1 + j0 * w + l0 * wh) * k + c) * (u * (ALWAN_CORE_LITERAL(1.0) - v) * (ALWAN_CORE_LITERAL(1.0) - s))
         + ALWAN_ATM_READ(base + (i0 + j1 * w + l0 * wh) * k + c) * ((ALWAN_CORE_LITERAL(1.0) - u) * v * (ALWAN_CORE_LITERAL(1.0) - s))
         + ALWAN_ATM_READ(base + (i1 + j1 * w + l0 * wh) * k + c) * (u * v * (ALWAN_CORE_LITERAL(1.0) - s))
         + ALWAN_ATM_READ(base + (i0 + j0 * w + l1 * wh) * k + c) * ((ALWAN_CORE_LITERAL(1.0) - u) * (ALWAN_CORE_LITERAL(1.0) - v) * s)
         + ALWAN_ATM_READ(base + (i1 + j0 * w + l1 * wh) * k + c) * (u * (ALWAN_CORE_LITERAL(1.0) - v) * s)
         + ALWAN_ATM_READ(base + (i0 + j1 * w + l1 * wh) * k + c) * ((ALWAN_CORE_LITERAL(1.0) - u) * v * s)
         + ALWAN_ATM_READ(base + (i1 + j1 * w + l1 * wh) * k + c) * (u * v * s);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_ATM_FN_(alwan_sky_atm_coord)(ALWAN_CORE_T x, int n) {
    return ALWAN_CORE_LITERAL(0.5) / (ALWAN_CORE_T)n + x * (ALWAN_CORE_LITERAL(1.0) - ALWAN_CORE_LITERAL(1.0) / (ALWAN_CORE_T)n);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_ATM_FN_(alwan_sky_atm_safe_sqrt)(ALWAN_CORE_T a) {
    return ALWAN_CORE_SQRT(a > ALWAN_CORE_LITERAL(0.0) ? a : ALWAN_CORE_LITERAL(0.0));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_ATM_FN_(alwan_sky_atm_distance_to_top)(ALWAN_ATM_PARAMS_ ALWAN_CORE_T r, ALWAN_CORE_T mu) {
    const ALWAN_CORE_T top = ALWAN_ATM_READ(1);
    const ALWAN_CORE_T disc = r * r * (mu * mu - ALWAN_CORE_LITERAL(1.0)) + top * top;
    const ALWAN_CORE_T d = -r * mu + ALWAN_ATM_FN_(alwan_sky_atm_safe_sqrt)(disc);
    return d > ALWAN_CORE_LITERAL(0.0) ? d : ALWAN_CORE_LITERAL(0.0);
}

ALWAN_INLINE int ALWAN_ATM_FN_(alwan_sky_atm_hits_ground)(ALWAN_ATM_PARAMS_ ALWAN_CORE_T r, ALWAN_CORE_T mu) {
    const ALWAN_CORE_T bottom = ALWAN_ATM_READ(0);
    return (mu < ALWAN_CORE_LITERAL(0.0) &&
            r * r * (mu * mu - ALWAN_CORE_LITERAL(1.0)) + bottom * bottom >= ALWAN_CORE_LITERAL(0.0)) ? 1 : 0;
}

/* GetTransmittanceToTopAtmosphereBoundary, from the table at base (0 transmittance,
 * 1 sun: the solar irradiance times it). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_ATM_FN_(alwan_sky_atm_to_top)(ALWAN_ATM_PARAMS_ int which, int c, ALWAN_CORE_T r, ALWAN_CORE_T mu) {
    const ALWAN_CORE_T bottom = ALWAN_ATM_READ(0), top = ALWAN_ATM_READ(1);
    const int k = ALWAN_ATM_FN_(alwan_sky_atm_int)(ALWAN_ATM_ARGS_ 5);
    const int tw = ALWAN_ATM_FN_(alwan_sky_atm_int)(ALWAN_ATM_ARGS_ 6);
    const int th = ALWAN_ATM_FN_(alwan_sky_atm_int)(ALWAN_ATM_ARGS_ 7);
    const ALWAN_CORE_T hh = ALWAN_CORE_SQRT(top * top - bottom * bottom);
    const ALWAN_CORE_T rho = ALWAN_ATM_FN_(alwan_sky_atm_safe_sqrt)(r * r - bottom * bottom);
    const ALWAN_CORE_T d = ALWAN_ATM_FN_(alwan_sky_atm_distance_to_top)(ALWAN_ATM_ARGS_ r, mu);
    const ALWAN_CORE_T d_min = top - r;
    const ALWAN_CORE_T d_max = rho + hh;
    const ALWAN_CORE_T x_mu = (d - d_min) / (d_max - d_min);
    const ALWAN_CORE_T x_r = rho / hh;
    return ALWAN_ATM_FN_(alwan_sky_atm_tex2d)(ALWAN_ATM_ARGS_ ALWAN_ATM_FN_(alwan_sky_atm_offset)(ALWAN_ATM_ARGS_ which),
                                              tw, th, k, c,
                                              ALWAN_ATM_FN_(alwan_sky_atm_coord)(x_mu, tw),
                                              ALWAN_ATM_FN_(alwan_sky_atm_coord)(x_r, th));
}

/* GetCombinedScattering without combined textures: which 2 reads the scattering
 * table, 3 the single Mie table, at (r, mu, mu_s, nu). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_ATM_FN_(alwan_sky_atm_scattering)(ALWAN_ATM_PARAMS_ int which, int c, ALWAN_CORE_T r,
                                                                 ALWAN_CORE_T mu, ALWAN_CORE_T mu_s, ALWAN_CORE_T nu,
                                                                 int ground) {
    const ALWAN_CORE_T bottom = ALWAN_ATM_READ(0), top = ALWAN_ATM_READ(1);
    const ALWAN_CORE_T mu_s_min = ALWAN_ATM_READ(2);
    const int k = ALWAN_ATM_FN_(alwan_sky_atm_int)(ALWAN_ATM_ARGS_ 5);
    const int nr = ALWAN_ATM_FN_(alwan_sky_atm_int)(ALWAN_ATM_ARGS_ 8);
    const int nmu = ALWAN_ATM_FN_(alwan_sky_atm_int)(ALWAN_ATM_ARGS_ 9);
    const int nmus = ALWAN_ATM_FN_(alwan_sky_atm_int)(ALWAN_ATM_ARGS_ 10);
    const int nnu = ALWAN_ATM_FN_(alwan_sky_atm_int)(ALWAN_ATM_ARGS_ 11);
    const ALWAN_CORE_T hh = ALWAN_CORE_SQRT(top * top - bottom * bottom);
    const ALWAN_CORE_T rho = ALWAN_ATM_FN_(alwan_sky_atm_safe_sqrt)(r * r - bottom * bottom);
    const ALWAN_CORE_T u_r = ALWAN_ATM_FN_(alwan_sky_atm_coord)(rho / hh, nr);
    const ALWAN_CORE_T r_mu = r * mu;
    const ALWAN_CORE_T disc = r_mu * r_mu - r * r + bottom * bottom;
    ALWAN_CORE_T u_mu, d, d_min, d_max, a, big_d, big_a, u_mu_s, u_nu, tex_coord_x, tex_x, lerp;
    if (ground) {
        d = -r_mu - ALWAN_ATM_FN_(alwan_sky_atm_safe_sqrt)(disc);
        d_min = r - bottom;
        d_max = rho;
        u_mu = ALWAN_CORE_LITERAL(0.5) - ALWAN_CORE_LITERAL(0.5) * ALWAN_ATM_FN_(alwan_sky_atm_coord)(d_max == d_min ? ALWAN_CORE_LITERAL(0.0) : (d - d_min) / (d_max - d_min), nmu / 2);
    } else {
        d = -r_mu + ALWAN_ATM_FN_(alwan_sky_atm_safe_sqrt)(disc + hh * hh);
        d_min = top - r;
        d_max = rho + hh;
        u_mu = ALWAN_CORE_LITERAL(0.5) + ALWAN_CORE_LITERAL(0.5) * ALWAN_ATM_FN_(alwan_sky_atm_coord)((d - d_min) / (d_max - d_min), nmu / 2);
    }
    d = ALWAN_ATM_FN_(alwan_sky_atm_distance_to_top)(ALWAN_ATM_ARGS_ bottom, mu_s);
    d_min = top - bottom;
    d_max = hh;
    a = (d - d_min) / (d_max - d_min);
    big_d = ALWAN_ATM_FN_(alwan_sky_atm_distance_to_top)(ALWAN_ATM_ARGS_ bottom, mu_s_min);
    big_a = (big_d - d_min) / (d_max - d_min);
    {
        ALWAN_CORE_T m = ALWAN_CORE_LITERAL(1.0) - a / big_a;
        if (m < ALWAN_CORE_LITERAL(0.0)) m = ALWAN_CORE_LITERAL(0.0);
        u_mu_s = ALWAN_ATM_FN_(alwan_sky_atm_coord)(m / (ALWAN_CORE_LITERAL(1.0) + a), nmus);
    }
    u_nu = (nu + ALWAN_CORE_LITERAL(1.0)) / ALWAN_CORE_LITERAL(2.0);
    tex_coord_x = u_nu * (ALWAN_CORE_T)(nnu - 1);
    tex_x = ALWAN_CORE_FLOOR(tex_coord_x);
    lerp = tex_coord_x - tex_x;
    {
        const int base = ALWAN_ATM_FN_(alwan_sky_atm_offset)(ALWAN_ATM_ARGS_ which);
        const int w = nnu * nmus;
        const ALWAN_CORE_T x0 = (tex_x + u_mu_s) / (ALWAN_CORE_T)nnu;
        const ALWAN_CORE_T x1 = (tex_x + ALWAN_CORE_LITERAL(1.0) + u_mu_s) / (ALWAN_CORE_T)nnu;
        return ALWAN_ATM_FN_(alwan_sky_atm_tex3d)(ALWAN_ATM_ARGS_ base, w, nmu, nr, k, c, x0, u_mu, u_r)
                   * (ALWAN_CORE_LITERAL(1.0) - lerp)
             + ALWAN_ATM_FN_(alwan_sky_atm_tex3d)(ALWAN_ATM_ARGS_ base, w, nmu, nr, k, c, x1, u_mu, u_r) * lerp;
    }
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_ATM_FN_(alwan_sky_atm_rayleigh_phase)(ALWAN_CORE_T nu) {
    return ALWAN_CORE_LITERAL(3.0) / (ALWAN_CORE_LITERAL(16.0) * ALWAN_CORE_PI) * (ALWAN_CORE_LITERAL(1.0) + nu * nu);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_ATM_FN_(alwan_sky_atm_mie_phase)(ALWAN_CORE_T g, ALWAN_CORE_T nu) {
    const ALWAN_CORE_T k = ALWAN_CORE_LITERAL(3.0) / (ALWAN_CORE_LITERAL(8.0) * ALWAN_CORE_PI)
                         * (ALWAN_CORE_LITERAL(1.0) - g * g) / (ALWAN_CORE_LITERAL(2.0) + g * g);
    return k * (ALWAN_CORE_LITERAL(1.0) + nu * nu)
         / ALWAN_CORE_POW(ALWAN_CORE_LITERAL(1.0) + g * g - ALWAN_CORE_LITERAL(2.0) * g * nu, ALWAN_CORE_LITERAL(1.5));
}

/* GetSkyRadiance with no shadow: the in-scattered radiance of channel c reaching a
 * camera at radius r looking along a ray with cosine mu to the zenith, the sun at
 * cosine mu_s to the zenith and nu to the ray. A camera above the atmosphere is moved
 * to where the ray enters it; a ray that misses it gets 0. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_ATM_FN_(alwan_sky_atm_sky_radiance)(ALWAN_ATM_PARAMS_ int c, ALWAN_CORE_T r, ALWAN_CORE_T mu,
                                                                   ALWAN_CORE_T mu_s, ALWAN_CORE_T nu) {
    const ALWAN_CORE_T top = ALWAN_ATM_READ(1);
    ALWAN_CORE_T rmu = r * mu;
    ALWAN_CORE_T rmu_s = r * mu_s;
    const ALWAN_CORE_T dtop = -rmu - ALWAN_CORE_SQRT(rmu * rmu - r * r + top * top);
    int ground;
    ALWAN_CORE_T scat, mie;
    if (dtop > ALWAN_CORE_LITERAL(0.0)) {
        rmu_s = rmu_s + dtop * nu;
        r = top;
        rmu = rmu + dtop;
    } else if (r > top) {
        return ALWAN_CORE_LITERAL(0.0);
    }
    mu = rmu / r;
    mu_s = rmu_s / r;
    ground = ALWAN_ATM_FN_(alwan_sky_atm_hits_ground)(ALWAN_ATM_ARGS_ r, mu);
    scat = ALWAN_ATM_FN_(alwan_sky_atm_scattering)(ALWAN_ATM_ARGS_ 2, c, r, mu, mu_s, nu, ground);
    mie = ALWAN_ATM_FN_(alwan_sky_atm_scattering)(ALWAN_ATM_ARGS_ 3, c, r, mu, mu_s, nu, ground);
    return scat * ALWAN_ATM_FN_(alwan_sky_atm_rayleigh_phase)(nu)
         + mie * ALWAN_ATM_FN_(alwan_sky_atm_mie_phase)(ALWAN_ATM_READ(3), nu);
}

/* The radiance of the solar disc along a ray at cosine mu from a camera at radius r:
 * the solar irradiance over the disc's solid angle, times the transmittance to the top
 * of the atmosphere; 0 for a ray that meets the ground. The caller decides whether the
 * ray points into the disc. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_ATM_FN_(alwan_sky_atm_sun_radiance)(ALWAN_ATM_PARAMS_ int c, ALWAN_CORE_T r, ALWAN_CORE_T mu) {
    const ALWAN_CORE_T top = ALWAN_ATM_READ(1);
    const ALWAN_CORE_T alpha = ALWAN_ATM_READ(4);
    const ALWAN_CORE_T omega = ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI * (ALWAN_CORE_LITERAL(1.0) - ALWAN_CORE_COS(alpha));
    if (r > top) {
        const ALWAN_CORE_T rmu = r * mu;
        const ALWAN_CORE_T dtop = -rmu - ALWAN_CORE_SQRT(rmu * rmu - r * r + top * top);
        if (!(dtop > ALWAN_CORE_LITERAL(0.0))) {
            /* the ray misses the atmosphere: the sun unattenuated, which the table
             * holds at the top looking up (transmittance 1) */
            return ALWAN_ATM_FN_(alwan_sky_atm_to_top)(ALWAN_ATM_ARGS_ 1, c, top, ALWAN_CORE_LITERAL(1.0)) / omega;
        }
        mu = (rmu + dtop) / top;
        r = top;
    }
    if (ALWAN_ATM_FN_(alwan_sky_atm_hits_ground)(ALWAN_ATM_ARGS_ r, mu)) return ALWAN_CORE_LITERAL(0.0);
    return ALWAN_ATM_FN_(alwan_sky_atm_to_top)(ALWAN_ATM_ARGS_ 1, c, r, mu) / omega;
}

/* The share of the solar disc above the horizon at radius r: GetTransmittanceToSun's
 * smoothstep. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_ATM_FN_(alwan_sky_atm_sun_visible)(ALWAN_ATM_PARAMS_ ALWAN_CORE_T r, ALWAN_CORE_T mu_s) {
    const ALWAN_CORE_T bottom = ALWAN_ATM_READ(0);
    const ALWAN_CORE_T alpha = ALWAN_ATM_READ(4);
    const ALWAN_CORE_T sin_h = bottom / r;
    const ALWAN_CORE_T one_m = ALWAN_CORE_LITERAL(1.0) - sin_h * sin_h;
    const ALWAN_CORE_T cos_h = -ALWAN_CORE_SQRT(one_m > ALWAN_CORE_LITERAL(0.0) ? one_m : ALWAN_CORE_LITERAL(0.0));
    const ALWAN_CORE_T e0 = -sin_h * alpha, e1 = sin_h * alpha;
    ALWAN_CORE_T t = (mu_s - cos_h - e0) / (e1 - e0);
    if (t < ALWAN_CORE_LITERAL(0.0)) t = ALWAN_CORE_LITERAL(0.0);
    if (t > ALWAN_CORE_LITERAL(1.0)) t = ALWAN_CORE_LITERAL(1.0);
    return t * t * (ALWAN_CORE_LITERAL(3.0) - ALWAN_CORE_LITERAL(2.0) * t);
}

/* GetSunAndSkyIrradiance at radius r for a surface whose normal has cosine n_up to
 * the zenith and n_sun to the sun, the sun at cosine mu_s to the zenith. which 0: the
 * irradiance from the sky, 1: from the sun. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_ATM_FN_(alwan_sky_atm_irradiance)(ALWAN_ATM_PARAMS_ int which, int c, ALWAN_CORE_T r,
                                                                 ALWAN_CORE_T mu_s, ALWAN_CORE_T n_up, ALWAN_CORE_T n_sun) {
    const ALWAN_CORE_T bottom = ALWAN_ATM_READ(0), top = ALWAN_ATM_READ(1);
    if (which == 0) {
        const int k = ALWAN_ATM_FN_(alwan_sky_atm_int)(ALWAN_ATM_ARGS_ 5);
        const int iw = ALWAN_ATM_FN_(alwan_sky_atm_int)(ALWAN_ATM_ARGS_ 12);
        const int ih = ALWAN_ATM_FN_(alwan_sky_atm_int)(ALWAN_ATM_ARGS_ 13);
        const ALWAN_CORE_T x_r = (r - bottom) / (top - bottom);
        const ALWAN_CORE_T x_mu_s = mu_s * ALWAN_CORE_LITERAL(0.5) + ALWAN_CORE_LITERAL(0.5);
        return ALWAN_ATM_FN_(alwan_sky_atm_tex2d)(ALWAN_ATM_ARGS_ ALWAN_ATM_FN_(alwan_sky_atm_offset)(ALWAN_ATM_ARGS_ 4),
                                                  iw, ih, k, c,
                                                  ALWAN_ATM_FN_(alwan_sky_atm_coord)(x_mu_s, iw),
                                                  ALWAN_ATM_FN_(alwan_sky_atm_coord)(x_r, ih))
             * (ALWAN_CORE_LITERAL(1.0) + n_up) * ALWAN_CORE_LITERAL(0.5);
    }
    return ALWAN_ATM_FN_(alwan_sky_atm_to_top)(ALWAN_ATM_ARGS_ 1, c, r, mu_s)
         * ALWAN_ATM_FN_(alwan_sky_atm_sun_visible)(ALWAN_ATM_ARGS_ r, mu_s)
         * (n_sun > ALWAN_CORE_LITERAL(0.0) ? n_sun : ALWAN_CORE_LITERAL(0.0));
}

/* GetTransmittance (spectral tables only): the transmittance of channel c along the
 * segment of length d from radius r at cosine mu. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_ATM_FN_(alwan_sky_atm_transmittance)(ALWAN_ATM_PARAMS_ int c, ALWAN_CORE_T r, ALWAN_CORE_T mu,
                                                                    ALWAN_CORE_T d, int ground) {
    const ALWAN_CORE_T bottom = ALWAN_ATM_READ(0), top = ALWAN_ATM_READ(1);
    ALWAN_CORE_T r_d = ALWAN_CORE_SQRT(d * d + ALWAN_CORE_LITERAL(2.0) * r * mu * d + r * r);
    ALWAN_CORE_T mu_d, q;
    if (r_d < bottom) r_d = bottom;
    if (r_d > top) r_d = top;
    mu_d = (r * mu + d) / r_d;
    if (mu_d < ALWAN_CORE_LITERAL(-1.0)) mu_d = ALWAN_CORE_LITERAL(-1.0);
    if (mu_d > ALWAN_CORE_LITERAL(1.0)) mu_d = ALWAN_CORE_LITERAL(1.0);
    if (ground) {
        q = ALWAN_ATM_FN_(alwan_sky_atm_to_top)(ALWAN_ATM_ARGS_ 0, c, r_d, -mu_d)
          / ALWAN_ATM_FN_(alwan_sky_atm_to_top)(ALWAN_ATM_ARGS_ 0, c, r, -mu);
    } else {
        q = ALWAN_ATM_FN_(alwan_sky_atm_to_top)(ALWAN_ATM_ARGS_ 0, c, r, mu)
          / ALWAN_ATM_FN_(alwan_sky_atm_to_top)(ALWAN_ATM_ARGS_ 0, c, r_d, mu_d);
    }
    return q < ALWAN_CORE_LITERAL(1.0) ? q : ALWAN_CORE_LITERAL(1.0);
}

/* GetSkyRadianceToPoint with no shadow (spectral tables only): the radiance of channel
 * c scattered into the segment from a camera at radius r (cosine mu, sun at mu_s and
 * nu) to a point at distance d. Its transmittance is alwan_sky_atm_transmittance. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_ATM_FN_(alwan_sky_atm_radiance_to_point)(ALWAN_ATM_PARAMS_ int c, ALWAN_CORE_T r,
                                                                        ALWAN_CORE_T mu, ALWAN_CORE_T mu_s,
                                                                        ALWAN_CORE_T nu, ALWAN_CORE_T d) {
    const ALWAN_CORE_T bottom = ALWAN_ATM_READ(0), top = ALWAN_ATM_READ(1);
    ALWAN_CORE_T rmu = r * mu;
    ALWAN_CORE_T rmu_s = r * mu_s;
    const ALWAN_CORE_T dtop = -rmu - ALWAN_CORE_SQRT(rmu * rmu - r * r + top * top);
    int ground;
    ALWAN_CORE_T t, scat, mie, r_p, mu_p, mu_s_p, scat_p, mie_p, sm;
    if (dtop > ALWAN_CORE_LITERAL(0.0)) {
        rmu_s = rmu_s + dtop * nu;
        r = top;
        rmu = rmu + dtop;
        d = d - dtop;
    }
    mu = rmu / r;
    mu_s = rmu_s / r;
    ground = ALWAN_ATM_FN_(alwan_sky_atm_hits_ground)(ALWAN_ATM_ARGS_ r, mu);
    t = ALWAN_ATM_FN_(alwan_sky_atm_transmittance)(ALWAN_ATM_ARGS_ c, r, mu, d, ground);
    scat = ALWAN_ATM_FN_(alwan_sky_atm_scattering)(ALWAN_ATM_ARGS_ 2, c, r, mu, mu_s, nu, ground);
    mie = ALWAN_ATM_FN_(alwan_sky_atm_scattering)(ALWAN_ATM_ARGS_ 3, c, r, mu, mu_s, nu, ground);
    if (d < ALWAN_CORE_LITERAL(0.0)) d = ALWAN_CORE_LITERAL(0.0);
    r_p = ALWAN_CORE_SQRT(d * d + ALWAN_CORE_LITERAL(2.0) * r * mu * d + r * r);
    if (r_p < bottom) r_p = bottom;
    if (r_p > top) r_p = top;
    mu_p = (r * mu + d) / r_p;
    mu_s_p = (r * mu_s + d * nu) / r_p;
    scat_p = ALWAN_ATM_FN_(alwan_sky_atm_scattering)(ALWAN_ATM_ARGS_ 2, c, r_p, mu_p, mu_s_p, nu, ground);
    mie_p = ALWAN_ATM_FN_(alwan_sky_atm_scattering)(ALWAN_ATM_ARGS_ 3, c, r_p, mu_p, mu_s_p, nu, ground);
    scat = scat - t * scat_p;
    mie = mie - t * mie_p;
    {
        ALWAN_CORE_T s = mu_s / ALWAN_CORE_LITERAL(0.01);
        if (s < ALWAN_CORE_LITERAL(0.0)) s = ALWAN_CORE_LITERAL(0.0);
        if (s > ALWAN_CORE_LITERAL(1.0)) s = ALWAN_CORE_LITERAL(1.0);
        sm = s * s * (ALWAN_CORE_LITERAL(3.0) - ALWAN_CORE_LITERAL(2.0) * s);
    }
    mie = mie * sm;
    return scat * ALWAN_ATM_FN_(alwan_sky_atm_rayleigh_phase)(nu)
         + mie * ALWAN_ATM_FN_(alwan_sky_atm_mie_phase)(ALWAN_ATM_READ(3), nu);
}

#undef ALWAN_ATM_FN_
#undef ALWAN_ATM_PARAMS_
#undef ALWAN_ATM_ARGS_
#undef ALWAN_ATM_READ
#if !defined(ALWAN_ATM_READER_POINTER)
# undef ALWAN_ATM_PASTE2_
# undef ALWAN_ATM_PASTE_
# undef ALWAN_ATM_NAME
#endif
