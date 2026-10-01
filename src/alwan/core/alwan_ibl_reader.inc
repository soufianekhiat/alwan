/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Image-based lighting table readers, written against an accessor instead of a
 * pointer. No include guard - included once per binding.
 *
 * SH COEFFICIENTS. channels values per coefficient, coefficient-major: coefficient k
 * (k = l^2 + l + m) of channel c at k * channels + c, as alwan_sh_project_equirect
 * writes them. alwan_ibl_sh_eval sums them against the basis of alwan_ibl_core.inc at a
 * unit direction; fed the coefficients of alwan_sh_irradiance_coefficients it is the
 * irradiance.
 *
 * SPLIT-SUM TABLE. Two values per texel (scale, bias), width texels of n . v by height
 * of perceptual roughness, row-major, texel (i, j) at (j * width + i) * 2, centres at
 * ((i + 0.5) / width, (j + 0.5) / height), as alwan_ibl_brdf_lut writes it. The lookup
 * is bilinear with clamp to edge, what a shader's sampler does with the table as a
 * texture.
 *
 * ACCESSOR. The tables are read through ALWAN_IBL_READ(i). A shader defines
 *     StructuredBuffer<float> Sh : register(t6);
 *     #define ALWAN_IBL_NAME     probe
 *     #define ALWAN_IBL_READ(i)  Sh[i]
 *     #include "core/alwan_ibl_reader.inc"
 */

#if defined(ALWAN_IBL_READER_POINTER)

# define ALWAN_IBL_FN_(base)      ALWAN_CORE_FNVLIT(base)
# define ALWAN_IBL_PARAMS_        const ALWAN_CORE_T *tab,
# define ALWAN_IBL_READ(i)        tab[i]

#else

# if !defined(ALWAN_IBL_NAME) || !defined(ALWAN_IBL_READ)
#  error "alwan_ibl_reader.inc: define ALWAN_IBL_NAME and ALWAN_IBL_READ before including it"
# endif
# define ALWAN_IBL_PASTE2_(a, b)  a##_##b
# define ALWAN_IBL_PASTE_(a, b)   ALWAN_IBL_PASTE2_(a, b)
# define ALWAN_IBL_FN_(base)      ALWAN_IBL_PASTE_(base, ALWAN_IBL_NAME)
# define ALWAN_IBL_PARAMS_

#endif

/* Channel c of the SH expansion up to band `band` at the unit direction (dx, dy, dz). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_IBL_FN_(alwan_ibl_sh_eval)(ALWAN_IBL_PARAMS_ int band, int channels, int c,
                                                          ALWAN_CORE_T dx, ALWAN_CORE_T dy, ALWAN_CORE_T dz) {
    ALWAN_CORE_T sum = ALWAN_CORE_LITERAL(0.0);
    int l, m;
    for (l = 0; l <= band; l++) {
        for (m = -l; m <= l; m++) {
            const int k = ALWAN_CORE_FNVLIT(alwan_sh_index)(l, m);
            sum = sum + ALWAN_IBL_READ(k * channels + c) * ALWAN_CORE_FNVLIT(alwan_sh_basis)(l, m, dx, dy, dz);
        }
    }
    return sum;
}

/* The split-sum table at (n . v, perceptual roughness), bilinear: (scale, bias) in
 * v[0], v[1]; the specular is F0 * scale + bias times the prefiltered radiance. */
ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_IBL_FN_(alwan_ibl_lut_lookup)(ALWAN_IBL_PARAMS_ int width, int height,
                                                                ALWAN_CORE_T nov, ALWAN_CORE_T roughness) {
    ALWAN_CORE_VEC2 r;
    const ALWAN_CORE_T x = ALWAN_CORE_FNVLIT(alwan_ibl_lut_texel)(nov, width);
    const ALWAN_CORE_T y = ALWAN_CORE_FNVLIT(alwan_ibl_lut_texel)(roughness, height);
    const int i0 = ALWAN_CORE_FNVLIT(alwan_ibl_lut_lower)(x, width);
    const int j0 = ALWAN_CORE_FNVLIT(alwan_ibl_lut_lower)(y, height);
    const int i1 = width > 1 ? i0 + 1 : i0;
    const int j1 = height > 1 ? j0 + 1 : j0;
    const ALWAN_CORE_T fx = width > 1 ? x - (ALWAN_CORE_T)i0 : ALWAN_CORE_LITERAL(0.0);
    const ALWAN_CORE_T fy = height > 1 ? y - (ALWAN_CORE_T)j0 : ALWAN_CORE_LITERAL(0.0);
    int c;
    for (c = 0; c < 2; c++) {
        const ALWAN_CORE_T a = ALWAN_IBL_READ((j0 * width + i0) * 2 + c);
        const ALWAN_CORE_T b = ALWAN_IBL_READ((j0 * width + i1) * 2 + c);
        const ALWAN_CORE_T d = ALWAN_IBL_READ((j1 * width + i0) * 2 + c);
        const ALWAN_CORE_T e = ALWAN_IBL_READ((j1 * width + i1) * 2 + c);
        const ALWAN_CORE_T top = a + (b - a) * fx;
        const ALWAN_CORE_T bot = d + (e - d) * fx;
        r.v[c] = top + (bot - top) * fy;
    }
    return r;
}

#undef ALWAN_IBL_FN_
#undef ALWAN_IBL_PARAMS_
#undef ALWAN_IBL_READ
#if !defined(ALWAN_IBL_READER_POINTER)
# undef ALWAN_IBL_PASTE2_
# undef ALWAN_IBL_PASTE_
# undef ALWAN_IBL_NAME
#endif
#undef ALWAN_IBL_READER_POINTER
