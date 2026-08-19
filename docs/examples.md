# Examples

Representative Alwan workflows using the current public API.

This file keeps `{T}` placeholders:

- `{T}` means `f32` or `f64`
- `alwan_scalar_{T}` means `alwan_f32` or `alwan_f64`

For the exact signatures behind these examples, see `src/alwan/alwan.h`.

---

## 1. Single RGB To Lab Conversion

```c
alwan_ctx *ctx = alwan_create(NULL);

alwan_rgb_{T} rgb = {0.9, 0.2, 0.1};
alwan_rgb_space_desc_{T} srgb_desc;
alwan_xyz_{T} d65;
alwan_xyz_{T} xyz;
alwan_lab_{T} lab;

alwan_rgb_get_space_descriptor_{T}(&srgb_desc, ALWAN_RGB_SPACE_SRGB, ctx);
alwan_illuminant_white_point_{T}(
    &d65, ALWAN_ILLUMINANT_D65, ALWAN_OBSERVER_CIE_1931_2DEG);

alwan_rgb_to_xyz_{T}(&xyz, &srgb_desc, &rgb);
alwan_xyz_to_lab_{T}(&lab, &xyz, &d65);

alwan_destroy(ctx);
```

---

## 2. Interleaved Batch Conversion

Convert an array of XYZ triplets to Lab triplets in place-compatible layout.

```c
alwan_xyz_{T} xyz_pixels[256];
alwan_lab_{T} lab_pixels[256];
alwan_xyz_{T} d65;

alwan_illuminant_white_point_{T}(
    &d65, ALWAN_ILLUMINANT_D65, ALWAN_OBSERVER_CIE_1931_2DEG);

alwan_xyz_to_lab_{T}_map_interleave(
    (alwan_scalar_{T} *)lab_pixels, sizeof(alwan_lab_{T}),
    (alwan_scalar_{T} const *)xyz_pixels, sizeof(alwan_xyz_{T}),
    256, &d65);
```

---

## 3. Typed-Pixel `_ex` Entry Point

Use `_map_interleave_ex` when your buffers are already in packed image types.

```c
uint8_t rgb_u8[1920 * 1080 * 3];
float oklab_f32[1920 * 1080 * 3];

alwan_srgb_to_oklab_map_interleave_ex(
    oklab_f32, sizeof(float) * 3,
    rgb_u8, 3,
    1920 * 1080,
    ALWAN_PIXEL_F32,
    ALWAN_PIXEL_U8);
```

That call performs typed-pixel load, normalization, conversion, and store in
one pass.

---

## 4. Planar Workflow

Planar APIs are useful when the host already stores channels separately.

```c
alwan_scalar_{T} X[4096], Y[4096], Z[4096];
alwan_scalar_{T} L[4096], a[4096], b[4096];
alwan_xyz_{T} d65;

alwan_illuminant_white_point_{T}(
    &d65, ALWAN_ILLUMINANT_D65, ALWAN_OBSERVER_CIE_1931_2DEG);

alwan_xyz_to_lab_{T}_map_planar(
    L, sizeof(alwan_scalar_{T}),
    a, b,
    X, sizeof(alwan_scalar_{T}),
    Y, Z,
    4096, &d65);
```

For typed planar source data, use `alwan_xyz_to_lab_map_planar_ex(...)`.

---

## 5. RGB Space Conversion

```c
alwan_ctx *ctx = alwan_create(NULL);
alwan_rgb_space_desc_{T} src_desc, dst_desc;
alwan_rgb_{T} src_pixels[1024];
alwan_rgb_{T} dst_pixels[1024];

alwan_rgb_get_space_descriptor_{T}(&src_desc, ALWAN_RGB_SPACE_SRGB, ctx);
alwan_rgb_get_space_descriptor_{T}(&dst_desc, ALWAN_RGB_SPACE_BT2020, ctx);

alwan_rgb_convert_map_interleave_{T}(
    dst_pixels, &src_desc, &dst_desc, src_pixels, 1024, ctx);

alwan_destroy(ctx);
```

This is the descriptor-driven path that handles transfer functions and
chromatic adaptation for you.

---

## 6. View Transform Over A Pixel Buffer

```c
alwan_ctx *ctx = alwan_create(NULL);
alwan_scalar_{T} src_rgb[512 * 512 * 3];
alwan_scalar_{T} dst_rgb[512 * 512 * 3];

alwan_view_transform_apply_{T}(
    dst_rgb, sizeof(alwan_scalar_{T}) * 3,
    src_rgb, sizeof(alwan_scalar_{T}) * 3,
    512 * 512,
    ALWAN_VIEW_AGX_ORIGINAL,
    ctx);

alwan_destroy(ctx);
```

---

## 7. Image Conversion

Image helpers operate on rows instead of per-pixel strides.

```c
alwan_ctx *ctx = alwan_create(NULL);
alwan_rgb_space_desc_{T} src_desc, dst_desc;

alwan_rgb_get_space_descriptor_{T}(&src_desc, ALWAN_RGB_SPACE_SRGB, ctx);
alwan_rgb_get_space_descriptor_{T}(&dst_desc, ALWAN_RGB_SPACE_DISPLAY_P3, ctx);

alwan_image_convert_{T}(
    dst_pixels, dst_row_stride,
    src_pixels, src_row_stride,
    width, height,
    ALWAN_PIXEL_U16,
    ALWAN_PIXEL_U8,
    &src_desc, &dst_desc,
    ctx);

alwan_destroy(ctx);
```

RGBA images use `alwan_image_convert_rgba_{T}` and add `ALWAN_ALPHA_STRAIGHT`
or `ALWAN_ALPHA_PREMULTIPLIED`.

---

## 8. Collect / Scatter For Host Buffers

These helpers are useful when you want to normalize once and run several Alwan
ops over the same typed image.

```c
uint16_t rgb_u16[2048 * 3];
alwan_f64 work[2048 * 3];

alwan_collect3_f64(
    work, sizeof(alwan_f64) * 3,
    rgb_u16, sizeof(uint16_t) * 3,
    2048, ALWAN_PIXEL_U16);

/* ... run typed f64 map functions over work ... */

alwan_scatter3_f64(
    rgb_u16, sizeof(uint16_t) * 3,
    work, sizeof(alwan_f64) * 3,
    2048, ALWAN_PIXEL_U16);
```

---

## 9. LUT Baking And Export

```c
alwan_ctx *ctx = alwan_create(NULL);
alwan_rgb_space_desc_{T} src_desc, dst_desc;
alwan_scalar_{T} lut3d[33 * 33 * 33 * 3];

alwan_rgb_get_space_descriptor_{T}(&src_desc, ALWAN_RGB_SPACE_ACESCG, ctx);
alwan_rgb_get_space_descriptor_{T}(&dst_desc, ALWAN_RGB_SPACE_DISPLAY_P3, ctx);

alwan_bake_3dlut_{T}(lut3d, 33, &src_desc, &dst_desc, ctx);
alwan_cube_export_3d_{T}("acescg_to_p3.cube", lut3d, 33, "ACEScg to Display P3");

alwan_destroy(ctx);
```

For display rendering baked into the LUT, use `alwan_bake_3dlut_view_{T}`.

CLF export works at the descriptor level:

```c
alwan_clf_export_view_{T}(
    "acescg_to_p3_agx.clf",
    &src_desc, &dst_desc,
    ALWAN_VIEW_AGX_ORIGINAL,
    "acescg_to_p3_agx",
    "ACEScg to Display P3 with AgX",
    4096,
    ctx);
```

---

## 10. Interop IDs

```c
alwan_rgb_space space;

if (alwan_interop_parse_{T}(&space, "lin_ap1") == ALWAN_OK) {
    char const *id = alwan_interop_format(space);
    /* id is the canonical interop string for that enum */
}

for (size_t i = 0; i < alwan_interop_count(); ++i) {
    alwan_rgb_space s;
    char const *id = NULL;
    if (alwan_interop_entry_at_{T}(&s, &id, i) == ALWAN_OK) {
        /* enumerate registry */
    }
}
```

---

## 11. Half Conversion

Half helpers expand to and from `alwan_f32`, because IEEE-754
binary16 is typically used as a transport/storage format rather than a compute
format.

```c
alwan_uint16 half_pixels[1024];
alwan_f32 linear_f32[1024];

alwan_half_to_float_{T}(linear_f32, half_pixels, 1024);
alwan_float_to_half_{T}(half_pixels, linear_f32, 1024);
```

---

## 12. Integer Normalization And Video Encode / Decode

```c
alwan_f64 linear_rgb[256 * 3];
uint16_t yuv_like_signal[256 * 3];

alwan_video_encode_f64(
    yuv_like_signal,
    ALWAN_PIXEL_U16,
    linear_rgb,
    256,
    ALWAN_RGB_SPACE_REC2100_PQ,
    ALWAN_VIDEO_RANGE_NARROW,
    10,
    ctx);

alwan_video_decode_f64(
    linear_rgb,
    yuv_like_signal,
    ALWAN_PIXEL_U16,
    256,
    ALWAN_RGB_SPACE_REC2100_PQ,
    ALWAN_VIDEO_RANGE_NARROW,
    10,
    ctx);
```

If you only need scalar normalization without transfer functions or RGB-space
lookups, use:

```c
alwan_uint_to_float_f64(out_f64, in_u16, 10, sample_count);
alwan_float_to_uint_f64(out_u16, out_f64, 10, sample_count);
```

---

## Related Guides

- [getting-started.md](getting-started.md)
- [map.md](map.md)
- [configuration.md](configuration.md)
- [docs/api/backends.md](api/backends.md)
