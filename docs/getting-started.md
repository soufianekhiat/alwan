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
    alwan_rgb_{T} rgb = {1.0, 0.5, 0.0};

    // Get the sRGB space descriptor
    alwan_rgb_space_desc_{T} srgb_desc;
    alwan_rgb_get_space_descriptor_{T}(&srgb_desc, ctx, ALWAN_RGB_SPACE_SRGB);

    // Convert sRGB → XYZ → Lab
    alwan_xyz_{T} xyz;
    alwan_lab_{T} lab;

    // First convert to XYZ (using standard sRGB descriptor)
    alwan_rgb_to_xyz_{T}(&xyz, &srgb_desc, &rgb);

    // Then convert XYZ to Lab (relative to D65)
    alwan_xyz_{T} d65;
    alwan_illuminant_white_point_{T}(&d65, ALWAN_ILLUMINANT_D65, ALWAN_OBSERVER_CIE_1931_2DEG);
    alwan_xyz_to_lab_{T}(&lab, &xyz, &d65);

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
    /* TODO: runtime_data_root is reserved — runtime data loading is not implemented.
     * Planned for alwan 3.0.0. This field is currently ignored. */
    .runtime_data_root = NULL
});
```

### 2. Color Space Conversions

Single-element conversions use semantic types (`alwan_xyz_{T}`, `alwan_lab_{T}`, etc.). Bulk operations use scalar arrays with explicit count and stride.

**Single color:**
```c
alwan_xyz_{T} xyz = {0.5, 0.6, 0.4};
alwan_lab_{T} lab;
alwan_xyz_{T} d65;
alwan_illuminant_white_point_{T}(&d65, ALWAN_ILLUMINANT_D65, ALWAN_OBSERVER_CIE_1931_2DEG);
alwan_xyz_to_lab_{T}(&lab, &xyz, &d65);
```

**Multiple colors (interleaved):**
```c
/* Interleaved map: XYZ triplets → Lab triplets */
alwan_xyz_{T} xyz_array[100];
alwan_lab_{T} lab_array[100];
alwan_xyz_{T} d65;
alwan_illuminant_white_point_{T}(&d65, ALWAN_ILLUMINANT_D65, ALWAN_OBSERVER_CIE_1931_2DEG);
alwan_xyz_to_lab_{T}_map_interleave((double*)lab_array, (double const*)xyz_array, &d65, 100,
                                    sizeof(alwan_xyz_{T}), sizeof(alwan_lab_{T}));
```

### 3. Standard Illuminants

Use `alwan_illuminant_white_point_{T}` to retrieve white points:

```c
alwan_xyz_{T} d65, d50;
alwan_illuminant_white_point_{T}(&d65, ALWAN_ILLUMINANT_D65, ALWAN_OBSERVER_CIE_1931_2DEG);
alwan_illuminant_white_point_{T}(&d50, ALWAN_ILLUMINANT_D50, ALWAN_OBSERVER_CIE_1931_2DEG);
```

Common illuminants: `ALWAN_ILLUMINANT_D65` (daylight 6504K), `ALWAN_ILLUMINANT_D50` (5003K),
`ALWAN_ILLUMINANT_A` (tungsten 2856K), `ALWAN_ILLUMINANT_E` (equal energy).

### 4. Stride for Interleaved Data

Process interleaved data without copying:

```c
// RGBA data: [R,G,B,A, R,G,B,A, ...]
double rgba[100 * 4];

// Process only RGB channels (3 per pixel), skipping alpha — stride = 4 doubles
// alwan_oetf_apply_{T}(out, transfer_function_enum, in, count, in_stride, out_stride)
alwan_oetf_apply_{T}(rgba,                     // Output (in-place)
                     ALWAN_TF_SRGB,            // Transfer function enum
                     rgba,                     // Input
                     100 * 3,                  // Count (3 channels x 100 pixels)
                     4 * sizeof(double),       // Input stride (skip alpha)
                     4 * sizeof(double));      // Output stride
```

---

## Common Workflows

### Workflow 1: sRGB to Display-Referred

Convert an sRGB image to linear light:

```c
/* Encoded sRGB pixels */
double srgb_encoded[1920 * 1080 * 3];

/* Allocate linear buffer */
double srgb_linear[1920 * 1080 * 3];

/* Apply inverse sRGB transfer function (EOTF: encoded -> linear) */
alwan_eotf_apply_{T}(srgb_linear, ALWAN_TF_SRGB,
                     srgb_encoded, 1920 * 1080 * 3,
                     sizeof(double), sizeof(double));
```

### Workflow 2: Color Space Conversion

Convert between RGB color spaces:

```c
/* Get space descriptors */
alwan_rgb_space_desc_{T} srgb_desc, bt2020_desc;
alwan_rgb_get_space_descriptor_{T}(&srgb_desc,   ctx, ALWAN_RGB_SPACE_SRGB);
alwan_rgb_get_space_descriptor_{T}(&bt2020_desc, ctx, ALWAN_RGB_SPACE_BT2020);

/* Convert sRGB -> BT.2020 (interleaved) */
alwan_rgb_convert_{T}_map_interleave(bt2020_data, ctx,
                                     &srgb_desc, &bt2020_desc,
                                     srgb_data, pixel_count,
                                     sizeof(alwan_rgb_{T}), sizeof(alwan_rgb_{T}));
```

### Workflow 3: Chromatic Adaptation

Adapt colors from one white point to another:

```c
/* Colors under D50 illuminant (flat double triplets) */
double xyz_d50[100 * 3];

/* Get white points */
alwan_xyz_{T} wp_d50, wp_d65;
alwan_illuminant_white_point_{T}(&wp_d50, ALWAN_ILLUMINANT_D50, ALWAN_OBSERVER_CIE_1931_2DEG);
alwan_illuminant_white_point_{T}(&wp_d65, ALWAN_ILLUMINANT_D65, ALWAN_OBSERVER_CIE_1931_2DEG);

/* Adapt to D65 using Bradford transform */
double xyz_d65[100 * 3];
alwan_cat_adapt_{T}(xyz_d65, &wp_d50, &wp_d65,
                    ALWAN_CAT_BRADFORD,
                    xyz_d50, 100,
                    3 * sizeof(double), 3 * sizeof(double));
```

### Workflow 4: Perceptual Color Difference

Calculate dE2000 between two colors:

```c
alwan_lab_{T} lab1 = {50.0, 20.0, 10.0};
alwan_lab_{T} lab2 = {51.0, 21.0, 11.0};

double delta_e = alwan_delta_e_2000_{T}(&lab1, &lab2);

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
/* High dynamic range linear RGB */
double hdr_rgb[1920 * 1080 * 3];

/* Display-ready RGB */
double display_rgb[1920 * 1080 * 3];

alwan_view_transform_apply_{T}(display_rgb, ctx, ALWAN_VIEW_AGX,
                               hdr_rgb, 1920 * 1080,
                               3 * sizeof(double), 3 * sizeof(double));
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

**Data embedding:**
```c
#define ALWAN_EMBED_DATA 1  /* 1=embedded (default); 0=runtime (NOT implemented, planned for 3.0.0) */
```

**Custom allocators:**
```c
#define ALWAN_ALLOC(sz, align) my_alloc(sz, align)
#define ALWAN_FREE(ptr) my_free(ptr)
```

**Precision** is selected per call site using `_f32` or `_f64` function/type variants — there is no global precision toggle.

---

## Error Handling

Check return values for operations that can fail:

```c
alwan_rgb_space_desc_{T} src_desc, dst_desc;
alwan_rgb_get_space_descriptor_{T}(&src_desc, ctx, ALWAN_RGB_SPACE_SRGB);
alwan_rgb_get_space_descriptor_{T}(&dst_desc, ctx, ALWAN_RGB_SPACE_BT2020);

alwan_rgb_{T} src = {1.0, 0.5, 0.0};
alwan_rgb_{T} dst;
int result = alwan_rgb_convert_{T}(&dst, ctx, &src_desc, &dst_desc, &src);

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

Many conversions require an explicit white point. Always provide one:
```c
alwan_xyz_{T} d65;
alwan_illuminant_white_point_{T}(&d65, ALWAN_ILLUMINANT_D65, ALWAN_OBSERVER_CIE_1931_2DEG);

/* Correct */
alwan_xyz_to_lab_{T}(&lab, &xyz, &d65);

/* Wrong: NULL white point returns ALWAN_E_INVALID */
alwan_xyz_to_lab_{T}(&lab, &xyz, NULL);
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
