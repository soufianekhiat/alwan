# Getting Started with Alwan

This guide will walk you through installing, configuring, and using Alwan for the first time.

---

## Installation

### 1. Clone the Repository

```batch
git clone --recursive https://github.com/yourusername/alwan.git
cd alwan
```

The `--recursive` flag ensures the Sharpmake submodule is cloned.

### 2. Bootstrap the Build

```batch
bootstrap.bat
```

This script:
- Builds Sharpmake from source
- Copies build tools to `tools/sharpmake/`
- Generates Visual Studio solution files

### 3. Build the Library

Open the generated solution:
```batch
Alwan_vs2022_win64.sln
```

Or build from command line:
```batch
msbuild Alwan_vs2022_win64.sln /p:Configuration=Debug_f64 /p:Platform=x64
```

### 4. Run Tests

Verify the installation:
```batch
working_dir\AlwanTests.exe
```

You should see all test suites pass.

---

## Your First Program

Create a simple program that converts sRGB to CIE Lab:

```c
#include "alwan.h"
#include <stdio.h>

int main(void) {
    // Initialize library context
    alwan_ctx *ctx = alwan_create(NULL);
    if (!ctx) {
        fprintf(stderr, "Failed to create Alwan context\n");
        return 1;
    }

    // Define an sRGB color (orange)
    alwan_rgb_f64 rgb = {1.0, 0.5, 0.0};

    // Get the sRGB space descriptor
    alwan_rgb_space_desc srgb_desc;
    alwan_rgb_get_space_descriptor(&srgb_desc, ctx, ALWAN_RGB_SPACE_SRGB);

    // Convert sRGB → XYZ → Lab
    alwan_xyz_f64 xyz;
    alwan_lab_f64 lab;

    // First convert to XYZ (using standard sRGB descriptor)
    alwan_rgb_to_xyz(&xyz, &srgb_desc, &rgb);

    // Then convert XYZ to Lab (relative to D65)
    alwan_xyz_to_lab(&lab, &xyz, &alwan_d65_xyz);

    printf("RGB: (%.3f, %.3f, %.3f)\n", rgb.r, rgb.g, rgb.b);
    printf("Lab: (L=%.2f, a=%.2f, b=%.2f)\n", lab.L, lab.a, lab.b);

    // Clean up
    alwan_destroy(ctx);
    return 0;
}
```

**Expected output:**
```
RGB: (1.000, 0.500, 0.000)
Lab: (L=74.93, a=23.93, b=78.95)
```

---

## Basic Concepts

### 1. Context Management

All Alwan operations require a context object:

```c
// Simple initialization
alwan_ctx *ctx = alwan_create(NULL);

// Always destroy when done
alwan_destroy(ctx);
```

For custom allocators or runtime data loading:
```c
alwan_ctx *ctx = alwan_create(&(alwan_config){
    .alloc_cb = my_alloc,
    .free_cb = my_free,
    .runtime_data_root = "data/"  // Only if ALWAN_EMBED_DATA=0
});
```

### 2. Color Space Conversions

Single-element conversions use semantic types (`alwan_xyz_f64`, `alwan_lab_f64`, etc.). Bulk operations use scalar arrays with explicit count and stride.

**Single color:**
```c
alwan_xyz_f64 xyz = {0.5, 0.6, 0.4};
alwan_lab_f64 lab;
alwan_xyz_to_lab(&lab, &xyz, &alwan_d65_xyz);
```

**Multiple colors (bulk, interleaved):**
```c
/* Interleaved map: XYZ triplets → Lab triplets */
alwan_xyz_f64 xyz_array[100];
alwan_lab_f64 lab_array[100];
alwan_xyz_to_lab_map_interleave(lab_array, xyz_array, &alwan_d65_xyz, 100,
                                sizeof(alwan_xyz_f64), sizeof(alwan_lab_f64));
```

### 3. Standard Illuminants

Alwan provides common illuminants as constants:

```c
alwan_d65_xyz  // D65 white point (daylight, 6504K)
alwan_d50_xyz  // D50 white point (photography, 5003K)
alwan_a_xyz    // Illuminant A (tungsten, 2856K)
alwan_e_xyz    // Equal energy illuminant
```

### 4. Stride for Interleaved Data

Process interleaved data without copying:

```c
// RGBA data: [R,G,B,A, R,G,B,A, ...]
alwan_scalar rgba[100 * 4];

// Process only RGB channels (3 per pixel), skipping alpha — stride = 4 scalars
// alwan_oetf_apply(out, transfer_function_enum, in, count, in_stride, out_stride)
alwan_oetf_apply(rgba,                         // Output (in-place)
                 ALWAN_TF_SRGB,                // Transfer function enum
                 rgba,                         // Input
                 100 * 3,                      // Count (3 channels × 100 pixels)
                 4 * sizeof(alwan_scalar),     // Input stride (skip alpha)
                 4 * sizeof(alwan_scalar));    // Output stride
```

---

## Common Workflows

### Workflow 1: sRGB to Display-Referred

Convert an sRGB image to linear light:

```c
// Encoded sRGB pixels
alwan_scalar srgb_encoded[1920 * 1080 * 3];

// Allocate linear buffer
alwan_scalar srgb_linear[1920 * 1080 * 3];

// Apply inverse sRGB transfer function (EOTF: encoded → linear)
alwan_eotf_apply(srgb_linear, ALWAN_TF_SRGB,
                 srgb_encoded, 1920 * 1080 * 3,
                 sizeof(alwan_scalar), sizeof(alwan_scalar));
```

### Workflow 2: Color Space Conversion

Convert between RGB color spaces:

```c
// Get space descriptors
alwan_rgb_space_desc srgb_desc, bt2020_desc;
alwan_rgb_get_space_descriptor(&srgb_desc,   ctx, ALWAN_RGB_SPACE_SRGB);
alwan_rgb_get_space_descriptor(&bt2020_desc, ctx, ALWAN_RGB_SPACE_BT2020);

// Convert sRGB → BT.2020 (bulk)
alwan_rgb_convert_map_interleave(bt2020_data, ctx,
                                 &srgb_desc, &bt2020_desc,
                                 srgb_data, pixel_count);
```

### Workflow 3: Chromatic Adaptation

Adapt colors from one white point to another:

```c
// Colors under D50 illuminant (flat scalar triplets)
alwan_scalar xyz_d50[100 * 3];

// Adapt to D65 using Bradford transform
// Signature: (out, src_white, dst_white, method, in, count, in_stride, out_stride)
alwan_scalar xyz_d65[100 * 3];
alwan_xyz_adapt(xyz_d65,
                &alwan_d50_xyz, &alwan_d65_xyz,
                ALWAN_CAT_BRADFORD,
                xyz_d50, 100,
                3 * sizeof(alwan_scalar), 3 * sizeof(alwan_scalar));
```

### Workflow 4: Perceptual Color Difference

Calculate ΔE2000 between two colors:

```c
alwan_lab_f64 lab1 = {50.0, 20.0, 10.0};
alwan_lab_f64 lab2 = {51.0, 21.0, 11.0};

double delta_e = alwan_delta_e_2000(&lab1, &lab2);

if (delta_e < 1.0) {
    printf("Colors are perceptually identical\n");
} else if (delta_e < 2.3) {
    printf("Colors are just noticeably different\n");
} else {
    printf("Colors are clearly different\n");
}
```

### Workflow 5: HDR Tone Mapping

Apply AgX view transform for HDR:

```c
// High dynamic range linear RGB
float hdr_rgb[1920 * 1080 * 3];

// Display-ready RGB
float display_rgb[1920 * 1080 * 3];

alwan_view_transform_apply(display_rgb, ctx, ALWAN_VIEW_AGX,
                           hdr_rgb, 1920 * 1080,
                           3 * sizeof(alwan_scalar), 3 * sizeof(alwan_scalar));
```

---

## Configuration Options

### Build Configurations

Choose precision and optimization:

| Configuration | Precision | Optimization | Use Case |
|--------------|-----------|-------------|----------|
| **Debug_f32** | float | Debug | Fast iteration, graphics |
| **Release_f32** | float | Release | Production graphics |
| **Debug_f64** | double | Debug | Scientific validation |
| **Release_f64** | double | Release | High-precision color science |

### Compile-Time Flags

Edit `alwan_config.h` or define at compile time:

**Scalar precision:**
```c
#define ALWAN_SCALAR_IS_FLOAT 0  // 0=double (default), 1=float
```

**Data embedding:**
```c
#define ALWAN_EMBED_DATA 1  // 1=embedded (default), 0=runtime
```

**Custom allocators:**
```c
#define ALWAN_ALLOC(sz, align) my_alloc(sz, align)
#define ALWAN_FREE(ptr) my_free(ptr)
```

---

## Error Handling

Check return values for operations that can fail:

```c
alwan_rgb_space_desc src_desc, dst_desc;
alwan_rgb_get_space_descriptor(&src_desc, ctx, ALWAN_RGB_SPACE_SRGB);
alwan_rgb_get_space_descriptor(&dst_desc, ctx, ALWAN_RGB_SPACE_BT2020);

alwan_rgb_f64 src = {1.0, 0.5, 0.0};
alwan_rgb_f64 dst;
int result = alwan_rgb_convert(&dst, ctx, &src_desc, &dst_desc, &src);

switch (result) {
    case ALWAN_OK:
        break;
    case ALWAN_E_INVALID:
        fprintf(stderr, "Invalid parameter\n");
        break;
    case ALWAN_E_NODATA:
        fprintf(stderr, "Color space data not available\n");
        break;
    default:
        fprintf(stderr, "Error: %d\n", result);
        break;
}
```

---

## Debugging Tips

### 1. Verify Test Suite Passes

If you encounter issues, first verify tests pass:
```batch
working_dir\AlwanTests.exe
```

### 2. Check White Points

Many conversions require explicit white points. Always specify:
```c
// Correct (output first)
alwan_xyz_to_lab(&lab, &xyz, &alwan_d65_xyz, 1, 0, 0);

// Wrong (will use incorrect white point)
alwan_xyz_to_lab(&lab, &xyz, NULL, 1, 0, 0);  // Error!
```

### 3. Validate Input Ranges

Color values should be in expected ranges:
- **RGB (linear)**: [0, 1] for SDR, [0, inf) for HDR
- **RGB (encoded)**: [0, 1]
- **Lab**: L in [0,100], a in [-128,127], b in [-128,127]
- **XYZ**: [0, inf)

### 4. Check Stride Calculations

Stride is in **bytes**, not elements:
```c
// Correct
size_t stride = 3 * sizeof(float);  // 12 bytes

// Wrong
size_t stride = 3;  // Only 3 bytes!
```

---

## Next Steps

Now that you understand the basics:

- **Explore the [API Reference](api/)** for detailed function documentation
- **See [Examples](examples.md)** for more complete use cases
- **Read [Configuration](configuration.md)** for advanced build options
- **Check [Precision & Limits](precision-and-limits.md)** for numerical accuracy details
