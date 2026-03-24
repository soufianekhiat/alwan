# Library Memory Layouts

Reference document for how major image/color libraries store pixel data in memory. Informs alwan's bulk API design for zero-copy interop.

---

## Summary Table

| Library | Layout | Pixel Types | Stride Unit | Channel Order | Alpha | Notes |
|---------|--------|-------------|-------------|---------------|-------|-------|
| libraw | Interleaved | uint16 | row (bytes) | RGBG (Bayer) / RGB | optional | 4-channel Bayer array; postprocessed → RGB |
| OCIO | Both | float32 | bytes (row + pixel) | RGB / RGBA | optional | `PackedImageDesc` (AoS) + `PlanarImageDesc` (SoA) |
| OpenEXR | Planar | half, float32, uint32 | row (bytes) | named channels | per-channel | `FrameBuffer` maps channel name → typed slice |
| OIIO | Interleaved | uint8-float64 | bytes (row + pixel) | RGB(A) | optional | `ImageSpec` + `ImageBuf`; get/set_pixels can reformat |
| OpenCV | Interleaved | uint8, uint16, float32, float64 | row (bytes) | **BGR**(A) | optional | `cv::Mat::step` is row stride; continuous flag |
| stb_image | Interleaved | uint8, uint16, float32 | packed (no padding) | RGB(A) | optional | No stride; rows are contiguous |
| FFmpeg | Both | uint8, uint16, float32 | row (bytes) per plane | YUV planar / RGB packed | varies | `AVFrame::linesize[]` per plane |
| vImage (Apple) | Both | uint8, uint16, half, float | row (bytes) | ARGB / BGRA / planar | varies | `vImage_Buffer { data, height, width, rowBytes }` |
| babl (GIMP) | Interleaved | uint8, uint16, half, float32, float64 | pixel (bytes) | RGB(A) / any named model | optional | Format string model; runtime conversion |
| Halide | Planar | uint8, uint16, float32 | element per dim | planar (dim0=x, dim1=y, dim2=c) | per-channel | `Runtime::Buffer<T>` with strides per dimension |
| Nuke NDK | Planar | float32 | row (bytes) | named channels | per-channel | Per-channel `Row` access; scanline engine |
| OFX (Resolve, Nuke) | Interleaved | uint8, uint16, half, float32 | row (bytes) | RGBA | yes | `OfxImageEffectSuiteV1::clipGetImage` |

---

## Per-Library Details

### libraw

- **Struct**: `libraw_data_t::imgdata.image` — array of `ushort[4]` per pixel (RGBG Bayer mosaic)
- **Post-demosaic**: `dcraw_make_mem_image()` returns a contiguous RGB/RGBA buffer as `libraw_processed_image_t`
  - `type`: `LIBRAW_IMAGE_BITMAP`
  - `colors`: 3 (RGB)
  - `bits`: 8 or 16
  - `data_size`: total bytes
  - `data`: contiguous interleaved, no row padding
- **Stride**: packed (width × colors × (bits/8)), no row stride parameter — rows are contiguous
- **Typical use**: decode to uint16 (12-14 bit range), convert to float externally

### OpenColorIO (OCIO)

**PackedImageDesc (interleaved / AoS):**
```cpp
PackedImageDesc(void *data,
                long width, long height,
                long numChannels,
                long chanStrideBytes,   // between R and G within one pixel
                long xStrideBytes,      // between pixel N and pixel N+1
                long yStrideBytes);     // between row N and row N+1
```
- Always `float*` (float32)
- `chanStrideBytes` = `sizeof(float)` typically
- `xStrideBytes` = `numChannels * sizeof(float)` for packed RGBA
- `yStrideBytes` = `width * xStrideBytes` for contiguous

**PlanarImageDesc (planar / SoA):**
```cpp
PlanarImageDesc(float *rData, float *gData, float *bData, float *aData,
                long width, long height,
                long yStrideBytes);     // row stride (same for all channels)
```
- Separate base pointer per channel
- All channels share the same row stride

### OpenEXR

- **Model**: channel-based framebuffer — each channel is a separate typed array
- **Struct**: `Imf::FrameBuffer` populated with `Slice` objects:
```cpp
Slice(PixelType type,     // HALF, FLOAT, UINT
      char *base,          // base pointer
      size_t xStride,      // bytes between pixels in a row
      size_t yStride,      // bytes between rows
      int xSampling = 1,   // subsampling
      int ySampling = 1);
```
- Channels are named strings: `"R"`, `"G"`, `"B"`, `"A"`, `"Z"`, arbitrary
- Typical: half (float16) for color, float32 for depth/Z
- Supports tiled and scanline modes (tiling is I/O, not pixel layout)
- Each channel can have independent base/stride (fully planar)

### OpenImageIO (OIIO)

- **Struct**: `ImageSpec` describes format; `ImageBuf` holds data
- Default storage: interleaved (channels contiguous per pixel)
- `ImageSpec::nchannels`, `ImageSpec::format` (TypeDesc: UINT8, UINT16, HALF, FLOAT, DOUBLE)
- Row stride: `ImageSpec::scanline_bytes()` (may include padding for alignment)
- Pixel stride: `nchannels * format.size()`
- `get_pixels()` / `set_pixels()` accept explicit format + stride overrides — can convert on read

### OpenCV

- **Struct**: `cv::Mat`
  - `data`: pointer to first element
  - `step[0]`: row stride in bytes (may include padding for alignment)
  - `step[1]`: pixel stride in bytes = `elemSize()` = channels × sizeof(type)
  - `type()`: encodes depth + channels (e.g., `CV_8UC3`, `CV_32FC3`)
- **Channel order**: **BGR** by default (historical), not RGB
- Supported depths: `CV_8U`, `CV_16U`, `CV_16S`, `CV_32S`, `CV_32F`, `CV_64F`
- `isContinuous()`: true if no row padding
- No native planar support (use `cv::split()` / `cv::merge()` to convert)

### stb_image

- `stbi_load()` → `unsigned char*` (uint8), `stbi_load_16()` → `unsigned short*`, `stbi_loadf()` → `float*`
- Always interleaved, always contiguous (no row stride / no padding)
- Requested channels: 1 (grey), 2 (grey+alpha), 3 (RGB), 4 (RGBA)
- No BGR, no planar, no half

### FFmpeg (libavutil)

- **Struct**: `AVFrame`
  - `data[AV_NUM_DATA_POINTERS]`: base pointer per plane (up to 8)
  - `linesize[AV_NUM_DATA_POINTERS]`: row stride per plane (bytes, may be negative)
- **Planar YUV** (most common): `data[0]`=Y, `data[1]`=U, `data[2]`=V; each plane has its own linesize
- **Packed RGB**: `data[0]` = interleaved, `linesize[0]` = row stride
- Pixel formats enumerated in `AVPixelFormat`: `AV_PIX_FMT_RGB24`, `AV_PIX_FMT_GBRPF32` (planar float32 GBR), etc.
- Supports uint8, uint16 (10/12/16 bit), float32
- `AV_PIX_FMT_GBRP` family: planar but channel order is GBR, not RGB

### vImage (Apple Accelerate)

- **Struct**: `vImage_Buffer { void *data; vImagePixelCount height, width; size_t rowBytes; }`
- Separate functions for interleaved vs planar:
  - `vImageConvert_Planar8toRGB888` (3 planar → 1 interleaved)
  - `vImageConvert_RGB888toPlanar8` (1 interleaved → 3 planar)
- Pixel types: `Planar8`, `PlanarF`, `ARGB8888`, `ARGB16U`, `RGBAFFFF` (float), etc.
- Channel order varies by function: ARGB (alpha-first) is common on macOS
- `rowBytes` must be ≥ width × pixel_size (padding allowed)

### babl (GIMP)

- Pixel formats described as strings: `"R'G'B' u8"`, `"CIE Lab float"`, `"Y'CbCr u8"`, etc.
- Components: model (RGB, Lab, YCbCr, ...) + type (u8, u16, half, float, double) + linearity (linear vs perceptual `'`)
- Storage: always interleaved at the pixel level
- `babl_process()` converts between any two formats — does type conversion + color space conversion in one pass
- SIMD: has hand-written SSE2/AVX2 fast paths for common conversions (e.g., linear float → sRGB u8), selected at runtime via `babl_init()`
- Stride: `babl_format_get_bytes_per_pixel()` — no explicit row stride (operates on flat arrays)

### Halide

- **Struct**: `Halide::Runtime::Buffer<T>`
- Dimensions are explicit: typically `dim(0)` = x (columns), `dim(1)` = y (rows), `dim(2)` = channels
- **Default: planar** — channel is the outermost dimension, so all R values are contiguous, then all G, then all B
- Can be reordered to interleaved via `set_stride(2, 1)` + `set_stride(0, channels)`
- Strides are in **elements** (not bytes)
- `host_ptr()` gives raw access

### Nuke NDK (Foundry)

- Scanline engine: processes one row at a time per channel
- `DD::Image::Row` provides per-channel `float*` arrays
- Fully planar: each channel is a separate float32 scanline buffer
- Channel naming: `Chan_Red`, `Chan_Green`, `Chan_Blue`, `Chan_Alpha`, arbitrary custom channels
- No interleaved mode in the internal engine

### OFX (OpenFX — Resolve, Nuke, Flame)

- `OfxImageEffectSuiteV1::clipGetImage` returns `OfxPropertySetHandle`
- Properties: `kOfxImagePropRowBytes` (row stride), `kOfxImagePropBounds`, `kOfxImagePropPixelDepth`
- Pixel depths: `kOfxBitDepthByte` (uint8), `kOfxBitDepthShort` (uint16), `kOfxBitDepthHalf`, `kOfxBitDepthFloat`
- **Always interleaved RGBA** (4 channels, alpha always present)
- Pixel components: `kOfxImageComponentRGBA`, `kOfxImageComponentRGB`, `kOfxImageComponentAlpha`
- Row stride in bytes; pixels are packed within a row

---

## Design Implications for Alwan

1. **Interleaved is the dominant format** — libraw, OpenCV, stb, OIIO, OFX, OCIO PackedImageDesc all default to it. Must be a first-class input.
2. **Planar matters for**: OpenEXR, Halide, Nuke, OCIO PlanarImageDesc, FFmpeg, vImage. Cannot be ignored.
3. **float32 is the universal processing type** — every library supports it. OCIO requires it. This should be the internal SIMD lane type.
4. **uint16 with sub-16-bit depth** is common (libraw 12/14-bit, FFmpeg 10/12-bit). Need bit_depth parameter for correct normalization.
5. **half (float16)** is critical for OpenEXR interop. Hardware F16C conversion (SSE2+) on load/store.
6. **BGR order** is OpenCV-only. Not worth a flag — let callers swizzle or provide a channel index array.
7. **Row stride** (in bytes) is universal. Pixel stride is implicit from channel count + type for interleaved.
8. **No library uses tiling as a pixel layout** — EXR/TIFF tiles are I/O concepts. Alwan tiles internally for cache efficiency, not as a user-facing layout.
