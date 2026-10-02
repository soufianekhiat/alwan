# Optical flow

Dense motion between two frames: for every pixel of the reference image, the displacement
(dx, dy) to the matching point of the moving image, `reference(x, y) ~ moving(x + dx, y + dy)`.
Global alignment (one warp for the whole frame, ECC or phase correlation) lives in
[`alwan_register`](image-tools.md#registration).

```c
typedef enum {
    ALWAN_OPTICAL_FLOW_TVL1 = 0,
    ALWAN_OPTICAL_FLOW_ILK = 1,
    ALWAN_OPTICAL_FLOW_FARNEBACK = 2
} alwan_optical_flow_method;

alwan_status alwan_optical_flow_{T}(alwan_{T} *flow, size_t flow_row_stride,
                                    alwan_{T} const *reference, size_t reference_row_stride,
                                    alwan_{T} const *moving, size_t moving_row_stride,
                                    size_t width, size_t height,
                                    alwan_optical_flow_method method, alwan_optical_flow_params const *params);
alwan_status alwan_optical_flow_u8(alwan_f32 *flow, size_t flow_row_stride,
                                   unsigned char const *reference, size_t reference_row_stride,
                                   unsigned char const *moving, size_t moving_row_stride,
                                   size_t width, size_t height,
                                   alwan_optical_flow_method method, alwan_optical_flow_params const *params);
```

Both images are one channel, at least 2 x 2, row strides in bytes. `flow` receives width x height
pairs (dx, dy); its row stride may be 0 for packed rows. This is `cv2.calcOpticalFlowFarneback`'s
flow, and scikit-image's (row, column) flow with its two components swapped.

| Method | Source | Notes |
|---|---|---|
| `TVL1` | scikit-image 0.26 `registration.optical_flow_tvl1` (Zach, Pock and Bischof 2007; Wedel et al. 2009; Perez, Meinhardt-Llopis and Facciolo 2013) | TV-L1 on scikit-image's coarse-to-fine pyramid (factor 2, at most 10 levels, reduced while the smaller side is over 32) |
| `ILK` | scikit-image `optical_flow_ilk` (Le Besnerais and Champagnat 2005) | iterative Lucas-Kanade, a uniform or Gaussian window, on the same pyramid |
| `FARNEBACK` | OpenCV 5.0.0 `calcOpticalFlowFarneback` (Farneback 2003) | polynomial expansion, its own Gaussian pyramid |

The ports keep their sources' arithmetic and match them to the bit (suite 300): TVL1 and ILK
against scikit-image at float64 (`_f64`) and float32 (`_f32`, scikit-image's default dtype); the
8-bit entry point reads `v * (1/255)` in float32, as scikit-image's `img_as_float32` does.
FARNEBACK always runs in float, as OpenCV does, and reads 8-bit values as they are (0 to 255).
Its 2 x 2 solve adds 1e-3 to each determinant, so its result depends on the intensity scale:
on images in [0, 1] that term dominates and the flow is poor (an endpoint error of about 1 pixel
on suite 300's pairs, against 0.03 on the same pairs as 8-bit). Feed it 0..255.

### Parameters

A zero field is its default (the scikit-image and OpenCV defaults). Zero the struct first.

| Field | Methods | Zero reads as |
|---|---|---|
| `attachment` | TVL1 | 15 (lambda) |
| `tightness` | TVL1 | 0.3 (theta) |
| `tolerance` | TVL1 | 1e-4, the stopping test on the squared change of the flow per pixel |
| `num_iter` | TVL1 | 10 fixed-point iterations per warp |
| `num_warp` | TVL1, ILK | 5 (TVL1), 10 (ILK) warps per level |
| `prefilter` | TVL1, ILK | off; non-zero takes a 3 x 3 median of the flow before each warp |
| `radius` | ILK | 7 |
| `gaussian` | ILK | a uniform window; non-zero a Gaussian of sigma (2 radius + 1) / 4 |
| `pyr_scale` | FARNEBACK | 0.5 |
| `levels` | FARNEBACK | 5 levels above the image; -1 is none |
| `window_size` | FARNEBACK | 13, odd |
| `iterations` | FARNEBACK | 10 per level |
| `poly_n` | FARNEBACK | 5 (5 or 7) |
| `poly_sigma` | FARNEBACK | 1.1 |
| `gaussian_window` | FARNEBACK | a box window; non-zero `OPTFLOW_FARNEBACK_GAUSSIAN` |
| `use_initial_flow` | FARNEBACK | off; non-zero reads `flow` as the starting estimate |

**Returns:** `ALWAN_OK`; `ALWAN_E_INVALID` for a NULL pointer, a size under 2 x 2, a stride
shorter than a row, an unknown method, a `pyr_scale` outside (0, 1), an even window, a `poly_n`
other than 5 or 7, or a negative attachment, tightness or tolerance; `ALWAN_E_RANGE` for a value
that is not finite; `ALWAN_E_NOMEM`.

## Warping by a flow

```c
alwan_status alwan_optical_flow_warp_{T}(alwan_{T} *out, size_t out_row_stride,
                                         alwan_{T} const *src, size_t src_row_stride, size_t channels,
                                         size_t width, size_t height,
                                         alwan_{T} const *flow, size_t flow_row_stride);
```

`out(x, y) = src(x + dx, y + dy)`: warping the moving image by its flow brings it onto the
reference. The points go through `alwan_warp`'s FIELD map, one lattice point a pixel, clamped
to the image as scikit-image's `warp(mode='edge')` clamps them, read bilinearly. On suite 300's
first pair this takes the mean absolute difference to the reference from 0.031 to 0.007; on a
random flow it agrees with scikit-image's `warp` to 1.3e-15.

## Flow as colour

```c
alwan_status alwan_optical_flow_to_rgb_{T}(alwan_{T} *out, size_t out_row_stride,
                                           alwan_{T} const *flow, size_t flow_row_stride,
                                           size_t width, size_t height, double max_radius);
```

The Middlebury colour wheel (Baker, Scharstein, Lewis, Roth, Black and Szeliski, "A Database and
Evaluation Methodology for Optical Flow", IJCV 92(1), 2011), implemented from the paper's
description: the direction picks the hue on a wheel of 55 steps (red to yellow 15, yellow to
green 6, green to cyan 4, cyan to blue 11, blue to magenta 13, magenta to red 6); the length over
`max_radius` sets the saturation, white at rest; past `max_radius` the colour is darkened to 75%.
Motion to +x is red, to -x cyan-blue. `max_radius` 0 reads as the field's longest vector (1 for
an all-zero field). Three values a pixel in [0, 1], display-encoded as the wheel's 8-bit table
is; a non-finite vector is black.
