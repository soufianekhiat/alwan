# Library Memory Layouts

Reference notes for how common image and colour libraries store pixel data, and
how that maps onto Alwan's current batch APIs.

---

## Summary Table

| Library / host style | Typical layout | Common pixel types | Stride unit | Channel order notes | Best Alwan fit |
|----------------------|----------------|--------------------|-------------|---------------------|----------------|
| OpenColorIO CPU-style buffers | packed or planar | mostly `F32` | bytes | semantic channel order is caller-defined | typed `_f32` map APIs, `_map_planar`, `_map_planar_ex` |
| OpenEXR frame buffers | usually planar-by-channel | `F16`, `F32`, sometimes `U32` | bytes | channels are named, not implied RGB-only | `ALWAN_PIXEL_F16`, planar APIs, half helpers |
| OpenImageIO image buffers | packed or planar views | `U8`, `U16`, `F16`, `F32` | bytes | RGB/BGR depends on source and plugin | `_map_interleave_ex`, `_map_planar_ex`, image helpers |
| OpenCV `Mat` | interleaved rows | `U8`, `U16`, `F32`, `F64` | bytes via row step | often BGR/BGRA rather than RGB/RGBA | `_map_interleave_ex`, `alwan_image_convert_{T}` |
| stb_image buffers | tightly packed interleaved | `U8`, `F32` | implicit row packing | RGB/RGBA order is explicit in decode call | `_map_interleave_ex`, image helpers |
| FFmpeg / libswscale-style planes | packed or planar | `U8`, `U16`, `F32` depending on format | bytes per line | RGB and YUV family formats both occur | `_map_planar_ex`, `_map_interleave_ex`, video helpers |
| Nuke NDK / OFX / compositing hosts | planar channels are common | mostly `F32`, sometimes `U16` | bytes or element-sized row steps from host | R/G/B/A planes often separate | `_map_planar`, `_map_planar_ex` |
| vImage / Accelerate-style image buffers | row-strided packed images | `U8`, `U16`, `F16`, `F32` | bytes | channel order depends on chosen buffer type | `_map_interleave_ex`, image helpers |
| Halide-style pipelines | either planar or packed by schedule | varied | explicit stride metadata | layout is pipeline-defined | `_map_planar_ex`, `_map_interleave_ex`, collect/scatter staging |

Alwan does not impose a single image container type. The practical choices are:

- typed interleaved float/double triplets
- typed-pixel interleaved buffers via `_map_interleave_ex`
- typed and typed-pixel planar buffers via `_map_planar*`
- row-strided image conversion via `alwan_image_convert_{T}` and `alwan_image_convert_rgba_{T}`
- explicit staging via `alwan_collect3_*` and `alwan_scatter3_*`

All Alwan strides in these APIs are byte strides.

---

## Per-Library Notes

### OpenColorIO

OpenColorIO integrations usually expose either packed float RGB(A) buffers or a
descriptor that can be interpreted as planar channel data. The common case is
`F32`, with the host already managing allocation and row traversal.

This maps cleanly to Alwan in two ways:

- use typed `_f32` map functions when the data is already interleaved RGB triplets
- use `_map_planar` or `_map_planar_ex` when the host exposes one pointer per channel

If the host buffer is RGBA rather than RGB, Alwan's image helpers are usually
the least awkward way to preserve alpha while running an RGB-space conversion.

### OpenEXR

OpenEXR frame buffers are commonly channel-oriented. `HALF` and `FLOAT` are the
important cases for Alwan, and channel names often matter more than physical
ordering in memory.

Current Alwan mapping:

- `ALWAN_PIXEL_F16` for direct typed-pixel access
- planar APIs when the EXR buffer is one channel per slice
- `alwan_half_to_float_{T}` and `alwan_float_to_half_{T}` when the host needs
  explicit expansion or repacking around a larger processing graph

This is also a good fit for `alwan_collect3_*` / `alwan_scatter3_*` if the EXR
integration wants to stage channels into a temporary RGB working set once and
then run several operations.

### OpenImageIO

OpenImageIO can present both tightly packed and more abstract strided image
buffers, depending on the plugin and image cache path. Pixel types vary widely:
`U8`, `U16`, `F16`, and `F32` are all normal.

For Alwan, the usual choice is:

- `_map_interleave_ex` for packed typed RGB buffers
- `_map_planar_ex` for channel-separated data
- `alwan_image_convert_{T}` or `alwan_image_convert_rgba_{T}` when the job is
  fundamentally an image-to-image RGB transform with explicit row strides

### OpenCV

`cv::Mat` is usually interleaved and row-strided. The main hazard is the
channel convention rather than the memory layout: many pipelines are BGR or
BGRA instead of RGB or RGBA.

Alwan can consume the storage directly, but the semantic swizzle remains the
caller's responsibility. The natural entry points are:

- `_map_interleave_ex` for RGB triplets already in the desired semantic order
- `alwan_image_convert_{T}` for row-strided image conversion
- `alwan_image_convert_rgba_{T}` when preserving alpha in four-channel images

### stb_image

`stb_image` outputs tightly packed interleaved buffers with no separate per-row
metadata beyond what the caller computes from width and channel count.

That makes it a straightforward case for:

- `_map_interleave_ex` with `ALWAN_PIXEL_U8`, `ALWAN_PIXEL_F32`, or
  another matching format after the caller computes the byte stride
- image helpers when the work is naturally framed as whole-image RGB or RGBA conversion

### FFmpeg

FFmpeg-family integrations encounter both packed RGB-like formats and planar
video-family layouts. Per-plane line sizes are explicit, and colour pipelines
often mix transfer/range work with format conversion.

Relevant Alwan entry points today:

- `_map_planar_ex` for plane-oriented channel processing
- `_map_interleave_ex` for packed RGB-style buffers
- `alwan_video_encode_{T}` / `alwan_video_decode_{T}` for full/legal range and
  YCbCr-family encode/decode primitives

When a format bridge is awkward, staging through `alwan_collect3_*` can reduce
glue complexity before a sequence of colour transforms.

### Nuke NDK / OFX / Similar Compositing Hosts

Many compositor SDKs expose separate channel buffers or per-channel iterators.
Float pipelines dominate, and alpha often lives alongside RGB but is processed
independently.

The clean Alwan mapping is:

- `_map_planar` for native float or double planar channels
- `_map_planar_ex` for non-float channel storage
- image helpers only when the host can already present a packed image view

This is one of the cases where Alwan's planar APIs are a closer fit than the
image helpers.

### vImage / Accelerate-Style Buffers

vImage commonly uses row-strided packed buffers with explicit pixel formats.
Depending on the chosen buffer type, the layout can resemble packed RGB,
packed RGBA, or single-plane image data.

In Alwan terms:

- `_map_interleave_ex` works well for packed 3-channel buffers
- `alwan_image_convert_{T}` and `alwan_image_convert_rgba_{T}` are the better
  fit for row-strided image conversion where alpha and width/height matter

### Halide-Style Pipelines

Halide does not force one canonical physical layout. The schedule determines
whether memory behaves more like planar channels, packed pixels, tiles, or
something hybrid.

For Alwan integration that usually means:

- use `_map_planar_ex` when the scheduled layout exposes clean channel planes
- use `_map_interleave_ex` when the scheduled output is packed RGB
- use `alwan_collect3_*` / `alwan_scatter3_*` when one staging step makes the
  rest of the colour graph simpler than adapting every kernel boundary

---

## How This Maps To Alwan Today

### 1. Packed typed pixels

Use `_map_interleave_ex` when the host already has a packed RGB buffer in one
of Alwan's supported storage formats:

- `ALWAN_PIXEL_U8`
- `ALWAN_PIXEL_U16`
- `ALWAN_PIXEL_F16`
- `ALWAN_PIXEL_F32`
- `ALWAN_PIXEL_F64`

Example shape:

```c
alwan_srgb_to_oklab_map_interleave_ex(
    dst, dst_stride,
    src, src_stride,
    count,
    ALWAN_PIXEL_F32,
    ALWAN_PIXEL_U8);
```

### 2. Native float or double triplets

Use typed `_f32` / `_f64` map functions when the host already has interleaved
triplets in `alwan_f32` or `alwan_f64` and you do not need pixel-format
conversion in the same call.

### 3. Planar channels

Use `_map_planar` for native floating-point planes and `_map_planar_ex` when
the plane storage is typed pixels rather than `alwan_f32` / `alwan_f64`.

Example shape:

```c
alwan_xyz_to_lab_f64_map_planar(
    L, sizeof(alwan_f64),
    a, b,
    X, sizeof(alwan_f64),
    Y, Z,
    count,
    &white_xyz);
```

### 4. Image-to-image conversion

Use `alwan_image_convert_{T}` or `alwan_image_convert_rgba_{T}` when the real
problem is row-strided image conversion between RGB space descriptors rather
than a generic channel-wise map.

Example shape:

```c
alwan_image_convert_f64(
    dst, dst_row_stride,
    src, src_row_stride,
    width, height,
    ALWAN_PIXEL_U16, ALWAN_PIXEL_U8,
    &src_desc, &dst_desc,
    ctx);
```

### 5. Staging and half conversion

`alwan_collect3_*` and `alwan_scatter3_*` are useful when the host layout is
technically supported but inconvenient for a multi-step processing graph.

Half support exists in two forms:

- direct typed-pixel processing through `ALWAN_PIXEL_F16`
- explicit conversion through `alwan_half_to_float_{T}` and `alwan_float_to_half_{T}`

---

## Design Implications For Alwan

1. Interleaved packed buffers remain the common denominator for general host interoperability.
2. Planar support matters because EXR, compositors, and video pipelines often do not want repacking.
3. Row-strided image helpers are the better abstraction when alpha and image dimensions are part of the contract.
4. Channel order is still a caller-level semantic concern. A BGR buffer is not automatically treated as RGB.
5. Half support needs to work both as a first-class pixel format and as an explicit bridge to float processing.

See also:

- [map.md](map.md)
- [ranges.md](ranges.md)
- [examples.md](examples.md)
