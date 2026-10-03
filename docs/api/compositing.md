# Compositing and blend modes

`alwan_composite_{f64,f32,u8,u16}` puts one image over another: the twelve Porter-Duff
operators (Porter and Duff, "Compositing Digital Images", SIGGRAPH 1984) plus plus-lighter, and
the sixteen blend modes of W3C Compositing and Blending Level 1, with three other soft lights
that applications use. It is written from those two documents. The four entry points are
`alwan_composite_f64`, `alwan_composite_f32`, `alwan_composite_u8` and `alwan_composite_u16`,
identical but for the element type.

```c
alwan_status alwan_composite_f64(alwan_f64 *out, size_t out_row_stride,
                                 alwan_f64 const *src, size_t src_row_stride,
                                 alwan_f64 const *dst, size_t dst_row_stride,
                                 size_t channels, size_t width, size_t height,
                                 alwan_composite_params const *params);
```

`src` is the source (the layer on top), `dst` the backdrop. `channels` is 4 (RGBA) or 3 (RGB,
every alpha 1, so `out` has no alpha). Strides are in bytes. `out` may be `src` or `dst`. The
integer forms read a code as code / (2^n - 1) and store the nearest code.

## The formula

Each pixel is blended, then composited, as W3C section 5 defines:

```
Cs' = (1 - ab) Cs + ab B(Cb, Cs)          the source after blending with the backdrop
co  = as Fa Cs' + ab Fb Cb                premultiplied result colour
ao  = as Fa + ab Fb                       result alpha
```

Where the backdrop is transparent (ab = 0) the source shows unblended. `B` is the blend mode;
`Fa` and `Fb` are the operator's fractions. NORMAL with SOURCE_OVER is ordinary alpha
compositing.

| `alwan_composite_operator` | Fa | Fb |
|---|---|---|
| `SOURCE_OVER` (0, default) | 1 | 1 - as |
| `CLEAR` | 0 | 0 |
| `COPY` | 1 | 0 |
| `DESTINATION` | 0 | 1 |
| `DESTINATION_OVER` | 1 - ab | 1 |
| `SOURCE_IN` | ab | 0 |
| `DESTINATION_IN` | 0 | as |
| `SOURCE_OUT` | 1 - ab | 0 |
| `DESTINATION_OUT` | 0 | 1 - as |
| `SOURCE_ATOP` | ab | 1 - as |
| `DESTINATION_ATOP` | 1 - ab | as |
| `XOR` | 1 - ab | 1 - as |
| `LIGHTER` | 1 | 1, colour and alpha each clamped to 1 (plus-lighter) |

| `alwan_blend_mode` | B(Cb, Cs) |
|---|---|
| `NORMAL` (0, default) | Cs |
| `MULTIPLY` | Cb Cs |
| `SCREEN` | Cb + Cs - Cb Cs |
| `OVERLAY` | HARD_LIGHT with the inputs swapped |
| `DARKEN`, `LIGHTEN` | min, max |
| `COLOR_DODGE` | 0 if Cb = 0, 1 if Cs = 1, else min(1, Cb / (1 - Cs)) |
| `COLOR_BURN` | 1 if Cb = 1, 0 if Cs = 0, else 1 - min(1, (1 - Cb) / Cs) |
| `HARD_LIGHT` | Multiply(Cb, 2 Cs) for Cs <= 0.5, else Screen(Cb, 2 Cs - 1) |
| `SOFT_LIGHT` | W3C's: D(Cb) is ((16 Cb - 12) Cb + 4) Cb up to 0.25, sqrt(Cb) above |
| `DIFFERENCE` | \|Cb - Cs\| |
| `EXCLUSION` | Cb + Cs - 2 Cb Cs |
| `HUE`, `SATURATION`, `COLOR`, `LUMINOSITY` | W3C's non-separable modes on the RGB triple, Lum = 0.3 R + 0.59 G + 0.11 B, with SetLum, SetSat and ClipColor |
| `SOFT_LIGHT_PHOTOSHOP` | 2 Cb Cs + Cb^2 (1 - 2 Cs) below Cs 0.5, 2 Cb (1 - Cs) + sqrt(Cb) (2 Cs - 1) above |
| `SOFT_LIGHT_PEGTOP` | (1 - 2 Cs) Cb^2 + 2 Cs Cb, continuous |
| `SOFT_LIGHT_ILLUSIONS` | Cb^(2^(2 (0.5 - Cs))) |

SetSat breaks ties between equal channels by index (the first equal maximum is the maximum,
the last equal minimum the minimum), so every channel has one role; the result is the same
either way.

## Parameters

```c
typedef struct {
    alwan_composite_operator op;        /* [SOURCE_OVER] */
    alwan_blend_mode blend;             /* [NORMAL] */
    int premultiplied;                  /* src, dst and out premultiplied [0: straight] */
    alwan_transfer_function transfer;   /* decode before, re-encode after [LINEAR: as given] */
} alwan_composite_params;
```

Zero every field you do not set; NULL params is NORMAL over, straight alpha, encoded values.

**Encoded or linear.** By default the values are blended as given. For sRGB-encoded pixels
that is what CSS, SVG and canvas do. Set `transfer` to the pixels' encoding (`ALWAN_TF_SRGB`)
to decode both inputs with its EOTF, blend and composite in linear light, and re-encode the
result with its OETF. Alpha is never decoded. The two differ visibly: a 50 % sRGB grey
screened with itself gives 0.75 blended encoded but 0.65 (encoded) when the screen happens in
linear light, since the grey is 0.21 there. With premultiplied input the colour is unpremultiplied before decoding and premultiplied again
after encoding.

**Range.** The blend functions are defined on [0, 1]. Values outside it go through the same
formulas and are not clamped; SOFT_LIGHT takes its square root only above 0.25, so a negative
backdrop stays finite. Integer outputs clamp to their range, as storing must.

## Errors

`ALWAN_E_INVALID` for a NULL buffer, a zero size, `channels` other than 3 or 4, a stride
smaller than a row, or an unknown operator or mode.

## Accuracy

Suite 301 checks every operator against every mode, straight and premultiplied, encoded and
sRGB-linear, on 40 RGBA pairs whose components and alphas include 0, 0.25, 0.5, 0.75 and 1,
against the W3C and Porter-Duff formulas written out in numpy: 1.1e-16 in f64 (1.5e-15 in the
deterministic build, whose `pow` is a polynomial), 3.0e-8 in f32, and the 16-bit form is the
nearest code to the f64 result.

Against Pillow's `ImageChops` on 8-bit RGB (backdrop as image1): difference, darker, lighter
and add are identical; multiply and screen are within one code because Pillow divides in
integers and truncates; Pillow's overlay and hard light divide by 127 rather than 255 (one
code); and Pillow's soft light is not W3C's but the pegtop formula with 65536 in place of
255^2, within two codes of `SOFT_LIGHT_PEGTOP`.

Porter-Duff identities hold exactly (a transparent source over a backdrop is the backdrop;
COPY, DESTINATION and CLEAR), XOR's alpha is as + ab - 2 as ab, and source-over is
associative on premultiplied values to 5.6e-17.
