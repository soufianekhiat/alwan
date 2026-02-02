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
    alwan_vec3 rgb = {1.0, 0.5, 0.0};

    // Convert sRGB → XYZ → Lab
    alwan_vec3 xyz, lab;

    // First convert to XYZ (using standard sRGB descriptor)
    alwan_rgb_to_xyz(&xyz, &rgb, 1, 0, 0);

    // Then convert XYZ to Lab (relative to D65)
    alwan_xyz_to_lab(&lab, &xyz, &alwan_d65_xyz, 1, 0, 0);

    printf("RGB: (%.3f, %.3f, %.3f)\n", rgb.x, rgb.y, rgb.z);
    printf("Lab: (L=%.2f, a=%.2f, b=%.2f)\n", lab.x, lab.y, lab.z);

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

Most conversions follow the pattern: `source_to_dest(output, input, config, count, in_stride, out_stride)`

**Single color:**
```c
alwan_vec3 xyz = {0.5, 0.6, 0.4};
alwan_vec3 lab;
alwan_xyz_to_lab(&lab, &xyz, &alwan_d65_xyz, 1, 0, 0);
```

**Multiple colors (bulk):**
```c
alwan_vec3 xyz_array[100];
alwan_vec3 lab_array[100];
alwan_xyz_to_lab(lab_array, xyz_array, &alwan_d65_xyz, 100,
                 sizeof(alwan_vec3), sizeof(alwan_vec3));
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
float rgba[100 * 4];

// Process only RGB, skip alpha (output first)
alwan_oetf_apply(rgba,                // Output
                 ctx, "srgb", rgba,   // Context, config, input
                 100,                 // Count
                 4 * sizeof(float),   // Input stride
                 4 * sizeof(float));  // Output stride
```

---

## Common Workflows

### Workflow 1: sRGB to Display-Referred

Convert an sRGB image to linear light:

```c
// Encoded sRGB pixels
float srgb_encoded[1920 * 1080 * 3];

// Allocate linear buffer
float srgb_linear[1920 * 1080 * 3];

// Apply inverse sRGB transfer function (EOTF)
alwan_eotf_apply(srgb_linear, ctx, "srgb",
                 srgb_encoded, 1920 * 1080,
                 3 * sizeof(float), 3 * sizeof(float));
```

### Workflow 2: Color Space Conversion

Convert between RGB color spaces:

```c
// Convert sRGB → BT.2020
alwan_rgb_convert(bt2020_data, ctx, "srgb", "bt2020",
                  srgb_data, pixel_count,
                  3 * sizeof(float), 3 * sizeof(float));
```

### Workflow 3: Chromatic Adaptation

Adapt colors from one white point to another:

```c
// Colors under D50 illuminant
alwan_vec3 xyz_d50[100];

// Adapt to D65 using Bradford transform
alwan_vec3 xyz_d65[100];
alwan_xyz_adapt(xyz_d65, ctx, ALWAN_CAT_BRADFORD,
                &alwan_d50_xyz, &alwan_d65_xyz,
                xyz_d50, 100,
                sizeof(alwan_vec3), sizeof(alwan_vec3));
```

### Workflow 4: Perceptual Color Difference

Calculate ΔE2000 between two colors:

```c
alwan_vec3 lab1 = {50.0, 20.0, 10.0};
alwan_vec3 lab2 = {51.0, 21.0, 11.0};

alwan_scalar delta_e;
alwan_delta_e_2000(&delta_e, &lab1, &lab2, 1, 0, 0);

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

alwan_view_transform_apply(display_rgb, ctx, "agx",
                           hdr_rgb, 1920 * 1080,
                           3 * sizeof(float), 3 * sizeof(float));
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
alwan_result result = alwan_rgb_convert(ctx, "srgb", "invalid_space", ...);

switch (result) {
    case ALWAN_SUCCESS:
        // Success
        break;
    case ALWAN_ERROR_NOT_FOUND:
        fprintf(stderr, "Color space not found\n");
        break;
    case ALWAN_ERROR_INVALID_PARAMETER:
        fprintf(stderr, "Invalid parameter\n");
        break;
    default:
        fprintf(stderr, "Unknown error\n");
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
- **RGB (linear)**: [0, 1] for SDR, [0, ∞) for HDR
- **RGB (encoded)**: [0, 1]
- **Lab**: L∈[0,100], a∈[-128,127], b∈[-128,127]
- **XYZ**: [0, ∞)

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
