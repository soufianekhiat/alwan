# Stain Separation

Histology slides are coloured by stains: haematoxylin turns nuclei blue-purple, eosin turns
cytoplasm pink, DAB marks an antibody brown. Light through the slide loses, stain by stain,
an amount proportional to how much of the stain is there, so in optical density
(OD = -log(I / I0)) the stains add. Colour deconvolution (Ruifrok and Johnston 2001) undoes
the mixture: given each stain's OD vector, it recovers how much of each stain a pixel holds,
and puts the amounts back together into RGB. alwan follows scikit-image's `separate_stains`,
`combine_stains` and `rgb2hed` (suite 245).

## alwan_stain_set

```c
typedef enum {
    ALWAN_STAIN_HED = 0,    /* haematoxylin, eosin, DAB (Ruifrok and Johnston 2001) */
    ALWAN_STAIN_HDX = 1,    /* haematoxylin, DAB, their cross product */
    ALWAN_STAIN_FGX = 2,    /* Feulgen, light green */
    ALWAN_STAIN_BEX = 3,    /* Giemsa: methyl blue, eosin */
    ALWAN_STAIN_RBD = 4,    /* FastRed, FastBlue, DAB */
    ALWAN_STAIN_GDX = 5,    /* methyl green, DAB */
    ALWAN_STAIN_HAX = 6,    /* haematoxylin, AEC */
    ALWAN_STAIN_BRO = 7,    /* aniline blue, azocarmine, orange G */
    ALWAN_STAIN_BPX = 8,    /* methyl blue, ponceau fuchsin */
    ALWAN_STAIN_AHX = 9,    /* alcian blue, haematoxylin */
    ALWAN_STAIN_HPX = 10,   /* haematoxylin, PAS */
    ALWAN_STAIN_CUSTOM = 11 /* the caller's rgb_from */
} alwan_stain_set;
```

The presets are scikit-image's matrices: H&E-DAB from the paper, the other ten adapted there
from G. Landini's colour deconvolution plugin. A set whose name ends in X has two stains, and
its third vector is the cross product of the first two, not normalised, as scikit-image
builds it. It soaks up whatever the two stains do not explain.

## alwan_stain_matrix_{T}

```c
alwan_status alwan_stain_matrix_{T}(alwan_mat3x3_{T} *rgb_from, alwan_mat3x3_{T} *from_rgb,
                                    alwan_stain_set set, alwan_mat3x3_{T} const *custom);
```

The set's `rgb_from`, whose rows are the stains' OD vectors, and its inverse `from_rgb`.
Either output may be `NULL`, not both. For `ALWAN_STAIN_CUSTOM`, `custom` is the caller's
`rgb_from`, rows = stains; a third row of zeros is replaced by the cross product of the first
two, as the presets are built. The vectors are used as given: scikit-image does not normalise
them, and neither does alwan.

**Returns:** `ALWAN_OK`; `ALWAN_E_INVALID` for an unknown set, `CUSTOM` without a matrix, a
non-finite entry or two `NULL` outputs; `ALWAN_E_RANGE` for a singular custom matrix.

## alwan_rgb_to_stains, alwan_stains_to_rgb

```c
alwan_status alwan_rgb_to_stains_{T}_map_interleave(alwan_{T} *out, size_t out_stride,
                                                    alwan_{T} const *rgb_in, size_t in_stride,
                                                    size_t count, alwan_stain_set set,
                                                    alwan_mat3x3_{T} const *custom);
alwan_status alwan_stains_to_rgb_{T}_map_interleave(alwan_{T} *rgb_out, size_t out_stride,
                                                    alwan_{T} const *in, size_t in_stride,
                                                    size_t count, alwan_stain_set set,
                                                    alwan_mat3x3_{T} const *custom);
alwan_status alwan_rgb_to_stains_{T}_map_planar(alwan_{T} *out_ch0, size_t out_stride,
                                                alwan_{T} *out_ch1, alwan_{T} *out_ch2,
                                                alwan_{T} const *in_ch0, size_t in_stride,
                                                alwan_{T} const *in_ch1, alwan_{T} const *in_ch2,
                                                size_t count, alwan_stain_set set,
                                                alwan_mat3x3_{T} const *custom);
alwan_status alwan_stains_to_rgb_{T}_map_planar(/* the same shape */);
alwan_status alwan_rgb_to_stains_map_interleave_ex(void *out, size_t out_stride, void const *rgb_in,
                                                   size_t in_stride, size_t count,
                                                   alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
                                                   alwan_stain_set set, alwan_mat3x3_f64 const *custom);
alwan_status alwan_stains_to_rgb_map_interleave_ex(/* the same shape */);
alwan_status alwan_rgb_to_stains_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2,
                                               void const *in0, size_t in_stride, void const *in1,
                                               void const *in2, size_t count,
                                               alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
                                               alwan_stain_set set, alwan_mat3x3_f64 const *custom);
alwan_status alwan_stains_to_rgb_map_planar_ex(/* the same shape */);
```

With `rgb` a row vector in `[0, 1]`, as the scanner delivered it (scikit-image applies no
transfer curve, and neither does alwan):

```
stains = (ln(max(rgb, 1e-6)) / ln(1e-6)) from_rgb,   then stains = max(stains, 0)
rgb    = exp(-(stains * -ln(1e-6)) rgb_from),        clipped to [0, 1]
```

OD is measured in units of `-ln(1e-6)`, the floor put under every channel: a white pixel
holds 0 of every stain, a channel at or below `1e-6` reads 1. Amounts below 0 are clamped
in the forward direction, so a pixel the stain vectors cannot explain does not round-trip
exactly; scikit-image's round trip does the same. Strides are in bytes. The typed forms read
each pixel through the library's format loader (u8 as value / 255, which is how
scikit-image's `rgb2hed` reads a u8 scan) and compute in double.

Suite 245 holds every preset and a custom two-stain set to scikit-image: within 3.3e-16 in
both directions in f64, 8.9e-16 on scikit-image's own round trip, 2.8e-17 on u8 pixels of its
immunohistochemistry sample into `rgb2hed`. f32 lands within 1.5e-7 of f64; planar and typed
forms equal the interleave map to the bit.

**Returns:** `ALWAN_OK`; `ALWAN_E_INVALID` for a `NULL` buffer, `count` 0, an unknown pixel
format, or a set `alwan_stain_matrix_{T}` refuses; `ALWAN_E_RANGE` for a singular custom
matrix.
