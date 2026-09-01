# Getting Started With Alwan

This guide covers the current library layout, the `{T}` documentation template,
and a minimal workflow from build to first conversion.

---

## The `{T}` Convention

This guide preserves `{T}` placeholders to avoid duplicating every example.

- `{T}` means `f32` or `f64`
- `alwan_scalar_{T}` means `alwan_f32` or `alwan_f64`
- `alwan_rgb_{T}`, `alwan_xyz_{T}`, `alwan_lab_{T}`, and similar names mean the
  matching semantic type with `_f32` or `_f64`

Example:

```c
alwan_xyz_{T}
```

means either:

```c
alwan_xyz_f32
```

or:

```c
alwan_xyz_f64
```

---

## Clone And Build

### 1. Clone the repo

```batch
git clone --recursive https://github.com/soufianekhiat/alwan.git
cd alwan
```

### 2. Build with CMake (all platforms)

```sh
cmake -S . -B build
cmake --build build
```

This is the portable route and what CI uses on every host target.

### 3. Optional: the Sharpmake / Visual Studio workflow

Sharpmake generates `Alwan_vs2022_Win64.sln` (needs the .NET 6+ SDK; the
generator runs on any OS, the produced solution builds with Visual Studio):

```sh
buildsystem/bootstrap.sh      # POSIX shells (also Git Bash on Windows)
```
```batch
buildsystem\bootstrap.bat     # Windows cmd
```

Then:

```batch
msbuild Alwan_vs2022_Win64.sln /p:Configuration=Release_f64
```

### 4. Tests

This repository ships the production library and headers. The test and benchmark
runner lives in the sibling `alwan_dev` repository; clone it as a peer of
`alwan/` and build:

```sh
# Layout: parent/
#   +-- alwan/         (this repo)
#   \-- alwan_dev/     (cloned next to it)
git clone https://github.com/soufianekhiat/alwan_dev.git ../alwan_dev
cd ../alwan_dev
cmake -S . -B build && cmake --build build --config Release
./build/tests/alwan_tests              # 107 suites, single binary
./build/tests/alwan_tests 04_srgb_tf   # filter by substring
./build/tests/alwan_tests --list       # list all suite names
```

For deterministic-mode validation:

```sh
cmake -S ../alwan_dev -B build_det -DALWAN_DETERMINISTIC=ON
cmake --build build_det --config Release
./build_det/det_regression/det_run_regression > out.txt
md5sum out.txt   # must match the same binary on every supported platform
```

---

## First Program

This example uses a context because RGB-space descriptor lookup is context-based.

```c
#include "alwan.h"
#include <stdio.h>

int main(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    if (!ctx) {
        fprintf(stderr, "failed to create Alwan context\n");
        return 1;
    }

    alwan_rgb_{T} rgb = {1.0, 0.5, 0.0};
    alwan_rgb_space_desc_{T} srgb_desc;
    alwan_xyz_{T} xyz;
    alwan_xyz_{T} d65;
    alwan_lab_{T} lab;

    if (alwan_rgb_get_space_descriptor_{T}(
            &srgb_desc, ALWAN_RGB_SPACE_SRGB, ctx) != ALWAN_OK) {
        alwan_destroy(ctx);
        return 1;
    }

    alwan_illuminant_white_point_{T}(
        &d65, ALWAN_ILLUMINANT_D65, ALWAN_OBSERVER_CIE_1931_2DEG);

    if (alwan_rgb_to_xyz_{T}(&xyz, &srgb_desc, &rgb) != ALWAN_OK) {
        alwan_destroy(ctx);
        return 1;
    }

    alwan_xyz_to_lab_{T}(&lab, &xyz, &d65);

    printf("L=%f a=%f b=%f\n", (double)lab.L, (double)lab.a, (double)lab.b);
    alwan_destroy(ctx);
    return 0;
}
```

---

## Core Concepts

### Contexts

Not every API needs `alwan_ctx`, but several important ones do:

- RGB-space descriptor lookup
- RGB-to-RGB conversion with descriptor-driven workflows
- LUT baking and CLF export
- video encode / decode helpers

Simple math transforms such as `alwan_xyz_to_lab_{T}` do not need a context.

```c
alwan_ctx *ctx = alwan_create(NULL);
alwan_destroy(ctx);
```

Custom allocators are configured through `alwan_config`:

```c
alwan_ctx *ctx = alwan_create(&(alwan_config){
    .alloc_cb = my_alloc,
    .free_cb = my_free,
    .runtime_data_root = NULL /* reserved today; runtime loading is not implemented */
});
```

### White Points

Many colourimetric transforms take an explicit white point:

```c
alwan_xyz_{T} d50, d65;
alwan_illuminant_white_point_{T}(
    &d50, ALWAN_ILLUMINANT_D50, ALWAN_OBSERVER_CIE_1931_2DEG);
alwan_illuminant_white_point_{T}(
    &d65, ALWAN_ILLUMINANT_D65, ALWAN_OBSERVER_CIE_1931_2DEG);
```

### Transfer Functions

OETF/EOTF helpers use output-first, stride-adjacent signatures:

```c
alwan_scalar_{T} encoded[300];
alwan_scalar_{T} linear[300];

alwan_oetf_apply_{T}(
    encoded, sizeof(alwan_scalar_{T}),
    linear, sizeof(alwan_scalar_{T}),
    300, ALWAN_TF_SRGB);

alwan_eotf_apply_{T}(
    linear, sizeof(alwan_scalar_{T}),
    encoded, sizeof(alwan_scalar_{T}),
    300, ALWAN_TF_SRGB);
```

### Batch Processing

Typed interleaved map functions operate on raw scalar triplets:

```c
alwan_xyz_{T} xyz_pixels[100];
alwan_lab_{T} lab_pixels[100];
alwan_xyz_{T} d65;

alwan_illuminant_white_point_{T}(
    &d65, ALWAN_ILLUMINANT_D65, ALWAN_OBSERVER_CIE_1931_2DEG);

alwan_xyz_to_lab_{T}_map_interleave(
    (alwan_scalar_{T} *)lab_pixels, sizeof(alwan_lab_{T}),
    (alwan_scalar_{T} const *)xyz_pixels, sizeof(alwan_xyz_{T}),
    100, &d65);
```

If your source buffer is `U8`, `U16`, or `F16`, prefer the typed-pixel `_ex`
entry points instead of normalizing by hand.

---

## Common Workflows

### RGB space conversion

```c
alwan_rgb_space_desc_{T} src_desc, dst_desc;
alwan_rgb_{T} src = {1.0, 0.5, 0.0};
alwan_rgb_{T} dst;

alwan_rgb_get_space_descriptor_{T}(&src_desc, ALWAN_RGB_SPACE_SRGB, ctx);
alwan_rgb_get_space_descriptor_{T}(&dst_desc, ALWAN_RGB_SPACE_BT2020, ctx);

alwan_rgb_convert_{T}(&dst, &src_desc, &dst_desc, &src, ctx);
```

For arrays of pixels:

```c
alwan_rgb_{T} src_pixels[256];
alwan_rgb_{T} dst_pixels[256];

alwan_rgb_convert_map_interleave_{T}(
    dst_pixels, &src_desc, &dst_desc, src_pixels, 256, ctx);
```

### Chromatic adaptation

`alwan_xyz_adapt_{T}` is a batch function over XYZ triplets:

```c
alwan_xyz_{T} src_white, dst_white;
alwan_scalar_{T} xyz_in[3];
alwan_scalar_{T} xyz_out[3];

alwan_xyz_adapt_{T}(
    xyz_out, sizeof(alwan_xyz_{T}),
    xyz_in, sizeof(alwan_xyz_{T}),
    1, &src_white, &dst_white, ALWAN_CAT_BRADFORD);
```

### View transform over an image-sized buffer

```c
alwan_scalar_{T} rgb_in[1920 * 1080 * 3];
alwan_scalar_{T} rgb_out[1920 * 1080 * 3];

alwan_view_transform_apply_{T}(
    rgb_out, sizeof(alwan_scalar_{T}) * 3,
    rgb_in, sizeof(alwan_scalar_{T}) * 3,
    1920 * 1080, ALWAN_VIEW_AGX_ORIGINAL, ctx);
```

### Image conversion with typed pixels

```c
alwan_image_convert_{T}(
    dst_pixels, dst_row_stride,
    src_pixels, src_row_stride,
    width, height,
    ALWAN_PIXEL_U16, ALWAN_PIXEL_U8,
    &src_desc, &dst_desc, ctx);
```

---

## What To Read Next

- [examples.md](examples.md) for more workflows
- [map.md](map.md) for interleaved, planar, and `_ex` batch APIs
- [configuration.md](configuration.md) for allocator hooks, deterministic mode, and range normalization
- [docs/api/context.md](api/context.md) for context-specific details
