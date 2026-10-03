# Colour maps

`alwan_colormap_apply_{f64,f32}` turns one scalar per pixel into a colour, for false colour
and data display.

| `alwan_colormap` | Source | Licence |
|---|---|---|
| `VIRIDIS` (0), `MAGMA`, `INFERNO`, `PLASMA` | Nathaniel Smith and Stefan van der Walt, mpl-colormaps (github.com/BIDS/colormap) | CC0 1.0 |
| `TURBO` | Anton Mikhailov, Google's improved rainbow | Apache-2.0, Copyright 2019 Google LLC |

The first four are perceptually uniform in CAM02-UCS and readable in greyscale and by most
colour-vision deficiencies; turbo is a rainbow whose lightness rises and falls, for when hue
discrimination matters more than ordering by lightness. Each is the 256-entry table matplotlib
ships, sRGB encoded. twilight and cividis are not shipped: their upstream states no licence
beyond matplotlib's own, and alwan ships only what is plainly compatible with MIT.

```c
alwan_status alwan_colormap_apply_f64(void *out, size_t out_row_stride,
                                      alwan_pixel_format out_fmt, size_t out_channels,
                                      alwan_f64 const *in, size_t in_row_stride,
                                      size_t width, size_t height,
                                      alwan_colormap map, alwan_colormap_params const *params);
alwan_status alwan_colormap_table(alwan_f64 *rgb_out, size_t *count, alwan_colormap map);
```

`out` holds `out_channels` (3 RGB, or 4 RGBA with alpha 1 for a mapped value) in `out_fmt`
(U8, U16, F32 or F64). `in` holds one scalar per pixel. Strides are in bytes.
`alwan_colormap_table` hands back the table itself (with `rgb_out` NULL it reports the count,
256).

## Lookup

- **`LOOKUP_LINEAR`** (0, default): linear interpolation between the entries, which sit at
  x = i / 255. A continuous input gives a continuous colour.
- **`LOOKUP_MATPLOTLIB`**: as matplotlib's `ListedColormap` reads it. The entry is
  floor(x * 256), x = 1 gives the last entry, and x * 256 is computed in the input's precision
  (in float for `_f32`, as numpy scales a float32 array). Integer outputs truncate, as
  matplotlib's `bytes=True` does. This reproduces `matplotlib.colormaps[name](x)` value for
  value.

## Parameters

```c
typedef struct {
    alwan_colormap_lookup lookup;   /* [LINEAR] */
    alwan_f64 vmin, vmax;           /* mapped to [0, 1] [0 and 0: the input is x] */
    int linear_output;              /* sRGB-decode the colours [0: encoded, as the table] */
    int use_under, use_over, use_bad;
    alwan_f64 under[4], over[4], bad[4];
} alwan_colormap_params;
```

x is (value - vmin) / (vmax - vmin), the identity when both are 0. Below 0 gives the under
colour, above 1 the over colour, NaN the bad colour. The defaults are matplotlib's: the first
entry, the last entry and transparent black (0, 0, 0, 0). `linear_output` decodes the table's
colours with the sRGB EOTF, for compositing or rendering in linear light; under, over and bad
colours you set are written exactly as given.

## Errors

`ALWAN_E_INVALID` for a NULL, a zero size, an unknown map, lookup or format (F16 included),
`out_channels` other than 3 or 4, a stride smaller than a row, or `vmin` equal to `vmax`
(other than both 0).

## Accuracy

Suite 302, on 1545 inputs that include both sides of every one of the 256 bin edges,
out-of-range values and NaN: `LOOKUP_MATPLOTLIB` equals matplotlib 3.10's floats and bytes
exactly for f64 and float32 inputs, all five maps; `LOOKUP_LINEAR` equals `numpy.interp` over
the table to 1.1e-16; the sRGB-decoded output equals colour's `eotf_sRGB` to 5.6e-16 (6.7e-16
in the deterministic build).
