# Summed-Area Tables

Crow (1984), "Summed-area tables for texture mapping". One pass builds `S(x, y)`, the sum
of the image over the pixels above and to the left of corner `(x, y)`; the sum over any
axis-aligned rectangle is then four reads, whatever its size:

    sum over [x0, x1) x [y0, y1) = S(x1, y1) - S(x0, y1) - S(x1, y0) + S(x0, y0)

```c
typedef struct alwan_summed_area_table_s alwan_summed_area_table;

alwan_status alwan_summed_area_table_create(alwan_summed_area_table **out, void const *image, size_t row_stride,
                                            size_t width, size_t height, size_t channels, alwan_pixel_format format,
                                            alwan_ctx *ctx);
void alwan_summed_area_table_destroy(alwan_summed_area_table *table, alwan_ctx *ctx);

alwan_status alwan_summed_area_table_sum(alwan_f64 *sum_out, size_t x0, size_t y0, size_t x1, size_t y1,
                                         alwan_summed_area_table const *table);
alwan_status alwan_summed_area_table_mean(alwan_f64 *mean_out, size_t x0, size_t y0, size_t x1, size_t y1,
                                          alwan_summed_area_table const *table);
alwan_status alwan_summed_area_table_integrate(alwan_f64 *integral_out, alwan_f64 x0, alwan_f64 y0, alwan_f64 x1,
                                               alwan_f64 y1, alwan_summed_area_table const *table);
alwan_status alwan_summed_area_table_box_mean_{T}(alwan_scalar_{T} *out, size_t out_row_stride,
                                                  size_t radius_x, size_t radius_y,
                                                  alwan_summed_area_table const *table);
alwan_status alwan_summed_area_table_get_layout(alwan_summed_area_table_layout *out,
                                                alwan_summed_area_table const *table);
```

## The table

- `image`: `width x height` pixels of 1 to 4 channels in any `alwan_pixel_format`, rows
  `row_stride` bytes apart (0 for packed). A non-finite value is `ALWAN_E_INVALID`; a
  table of 2^31 elements or more `ALWAN_E_RANGE` (offsets are ints, for a shader).
- The table is double whatever the image. U8 and U16 are summed as their integer codes,
  exactly while a sum stays below 2^53 (a 16-bit image of 2^37 pixels), and reported in
  the format's units: code sum / 255, code sum / 65535, exactly.
- F16, F32 and F64 have each channel's mean, rounded to a multiple of 1/256, taken off
  every pixel before summing. The corners then hold sums of values near 0, so a small
  rectangle far from the origin no longer subtracts two nearly equal large numbers: on a
  2048 x 2048 image of values in [0.4, 0.6], small rectangles near the far corner come
  back to 2.2e-15, against 2.9e-10 for a plain prefix sum of the same values (suite 281).
  The rounding to 1/256 keeps an image holding integers exact.

## Queries

- `sum`, `mean`: the pixels `[x0, x1) x [y0, y1)`, one value per channel. `0 <= x0 <= x1
  <= width` and `0 <= y0 <= y1 <= height`, else `ALWAN_E_RANGE`; an empty rectangle sums
  to 0 and has no mean (`ALWAN_E_RANGE`).
- `integrate`: continuous bounds in pixel units, the image constant on each pixel, partial
  pixels counted by the area they cover. The table blended bilinearly between its integer
  corners is exactly the image's integral (inside a pixel `S` grows as
  `S00 + fx A + fy B + fx fy p`, which is what the blend of the pixel's corners gives), so
  the result is exact up to rounding: 4.4e-15 against the area-weighted sum (suite 281).
  Bounds outside `[0, width] x [0, height]`, or reversed, are `ALWAN_E_RANGE`.
- `box_mean`: every pixel the mean over the `(2 radius_x + 1) x (2 radius_y + 1)` window
  around it, the window cut at the image's edges and the mean taken over what is left: a
  box filter at any radius for the same cost, about 33 ms for a 1920 x 1080 RGB image at
  radius 1, 15 or 100. `out` holds `width x height x channels` values.

## On a GPU

`get_layout` hands back the table (`(width + 1) (height + 1) channels` corners, row by row,
then the channels' offsets and the divisor; `core/alwan_summed_area_core.inc`). The queries
are in `core/alwan_summed_area_reader.inc`, written against an accessor:

```hlsl
StructuredBuffer<float> Sat : register(t4);
#define ALWAN_SAT_NAME     img
#define ALWAN_SAT_READ(i)  Sat[i]
#include "core/alwan_summed_area_reader.inc"

float s = alwan_sat_sum_img(w, h, ch, c, x0, y0, x1, y1);
float t = alwan_sat_integral_img(w, h, ch, c, fx0, fy0, fx1, fy1);
```

A float copy of the table loses what the corners have in common: on suite 281's 61 x 47
image the error is 2.7e-7 per pixel of area. The mean offset keeps the corners small for
an image near its mean. The core compiles with dxc and fxc.

## Validation (suite 281)

No reference implementation: a rectangle's sum is the sum of its pixels, which the suite
adds up itself.

- 4000 random rectangles, empty ones included: an F64 image of integers, a U8 image and a
  4-channel U16 image all exact (U8 and U16 as code sum / 255 and / 65535).
- F32 and F16 images to rounding; the continuous integral against the area-weighted sum
  to 4.4e-15.
- The edge rectangles: empty in either direction, the last pixel, the whole image.
- The box mean against the window's mean at radii 0 to 100: 5.1e-15 in f64, 6e-8 in f32.
- The offset's gain on a 2048 x 2048 image, as above.
