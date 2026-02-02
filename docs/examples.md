# Examples

Practical examples demonstrating common Alwan use cases.

---

## Table of Contents

1. [Basic Color Conversions](#basic-color-conversions)
2. [Image Processing](#image-processing)
3. [HDR Workflow](#hdr-workflow)
4. [Color Grading](#color-grading)
5. [Perceptual Operations](#perceptual-operations)
6. [Custom Allocators](#custom-allocators)
7. [Multi-threading](#multi-threading)
8. [Error Handling](#error-handling)

---

## Basic Color Conversions

### Example 1: RGB to Lab

Convert an sRGB color to perceptual Lab for color analysis.

```c
#include "alwan.h"
#include <stdio.h>

int main(void) {
    alwan_ctx *ctx = alwan_create(NULL);

    // Orange color in sRGB
    alwan_vec3 rgb = {1.0, 0.5, 0.0};

    // Convert sRGB (linear) → XYZ → Lab
    alwan_vec3 xyz, lab;
    alwan_rgb_to_xyz(&xyz, &rgb, 1, 0, 0);
    alwan_xyz_to_lab(&lab, &xyz, &alwan_d65_xyz, 1, 0, 0);

    printf("RGB: (%.3f, %.3f, %.3f)\n", rgb.x, rgb.y, rgb.z);
    printf("Lab: L=%.2f, a=%.2f, b=%.2f\n", lab.x, lab.y, lab.z);
    printf("Lightness: %.0f%%\n", lab.x);
    printf("Chroma: %.2f\n", sqrt(lab.y*lab.y + lab.z*lab.z));

    alwan_destroy(ctx);
    return 0;
}
```

**Output:**
```
RGB: (1.000, 0.500, 0.000)
Lab: L=74.93, a=23.93, b=78.95
Lightness: 75%
Chroma: 82.51
```

---

### Example 2: Color Space Gamut Mapping

Convert wide-gamut BT.2020 to sRGB with clipping.

```c
#include "alwan.h"
#include <stdio.h>

void clip_to_gamut(alwan_vec3 *rgb) {
    rgb->x = fmax(0.0, fmin(1.0, rgb->x));
    rgb->y = fmax(0.0, fmin(1.0, rgb->y));
    rgb->z = fmax(0.0, fmin(1.0, rgb->z));
}

int main(void) {
    alwan_ctx *ctx = alwan_create(NULL);

    // Wide-gamut BT.2020 color
    alwan_vec3 bt2020 = {0.9, 0.3, 0.1};

    // Convert BT.2020 → sRGB
    alwan_vec3 srgb;
    alwan_rgb_convert(&srgb, ctx, "bt2020", "srgb", &bt2020, 1, 0, 0);

    printf("BT.2020: (%.3f, %.3f, %.3f)\n", bt2020.x, bt2020.y, bt2020.z);
    printf("sRGB (raw): (%.3f, %.3f, %.3f)\n", srgb.x, srgb.y, srgb.z);

    if (srgb.x < 0 || srgb.x > 1 ||
        srgb.y < 0 || srgb.y > 1 ||
        srgb.z < 0 || srgb.z > 1) {
        printf("⚠ Out of sRGB gamut! Clipping...\n");
        clip_to_gamut(&srgb);
    }

    printf("sRGB (clipped): (%.3f, %.3f, %.3f)\n", srgb.x, srgb.y, srgb.z);

    alwan_destroy(ctx);
    return 0;
}
```

---

## Image Processing

### Example 3: Convert Image from sRGB to Linear

Process a full image from encoded sRGB to linear light.

```c
#include "alwan.h"
#include <stdlib.h>
#include <stdio.h>

// Assume image loading function
typedef struct {
    float *data;  // RGB data
    int width;
    int height;
} Image;

Image* load_image(const char *path);
void save_image(const char *path, const Image *img);

int main(void) {
    alwan_ctx *ctx = alwan_create(NULL);

    // Load encoded sRGB image
    Image *srgb_encoded = load_image("input.png");
    int pixel_count = srgb_encoded->width * srgb_encoded->height;

    // Allocate linear image
    Image linear = {
        .data = malloc(pixel_count * 3 * sizeof(float)),
        .width = srgb_encoded->width,
        .height = srgb_encoded->height
    };

    // Apply inverse sRGB transfer function (EOTF)
    // Converts encoded [0,1] → linear [0,1]
    alwan_eotf_apply(linear.data, ctx, "srgb",
                     srgb_encoded->data, pixel_count,
                     3 * sizeof(float), 3 * sizeof(float));

    printf("Converted %d pixels to linear light\n", pixel_count);

    // Process linear image...

    // Convert back to sRGB for display
    alwan_oetf_apply(srgb_encoded->data, ctx, "srgb",
                     linear.data, pixel_count,
                     3 * sizeof(float), 3 * sizeof(float));

    save_image("output.png", srgb_encoded);

    free(linear.data);
    // free srgb_encoded...

    alwan_destroy(ctx);
    return 0;
}
```

---

### Example 4: Process RGBA with Stride

Work with RGBA data without unpacking RGB.

```c
#include "alwan.h"
#include <stdlib.h>

int main(void) {
    alwan_ctx *ctx = alwan_create(NULL);

    // RGBA image data: [R,G,B,A, R,G,B,A, ...]
    int width = 1920, height = 1080;
    float *rgba = malloc(width * height * 4 * sizeof(float));

    // Load RGBA data...

    // Apply sRGB EOTF to RGB only, preserving alpha
    // Stride = 4 floats = 16 bytes (skip alpha)
    alwan_eotf_apply(rgba, ctx, "srgb",
                     rgba, width * height,
                     4 * sizeof(float), 4 * sizeof(float));

    printf("Processed %dx%d RGBA image\n", width, height);
    printf("Alpha channel untouched\n");

    free(rgba);
    alwan_destroy(ctx);
    return 0;
}
```

---

## HDR Workflow

### Example 5: HDR Tone Mapping with AgX

Apply AgX view transform for HDR to SDR mapping.

```c
#include "alwan.h"
#include <stdlib.h>
#include <stdio.h>

int main(void) {
    alwan_ctx *ctx = alwan_create(NULL);

    // HDR linear RGB (scene-referred, can be > 1.0)
    alwan_vec3 hdr_colors[] = {
        {0.5, 0.3, 0.2},    // Normal exposure
        {2.5, 1.8, 1.2},    // Bright area
        {10.0, 8.0, 6.0},   // Very bright (sunlight)
        {0.01, 0.008, 0.005} // Dark shadow
    };
    int count = sizeof(hdr_colors) / sizeof(hdr_colors[0]);

    alwan_vec3 *display_rgb = malloc(count * sizeof(alwan_vec3));

    // Apply AgX view transform
    alwan_view_transform_apply(display_rgb, ctx, "agx",
                               hdr_colors, count,
                               sizeof(alwan_vec3), sizeof(alwan_vec3));

    printf("HDR → Display (AgX)\n");
    for (int i = 0; i < count; i++) {
        printf("  [%.2f, %.2f, %.2f] → [%.3f, %.3f, %.3f]\n",
               hdr_colors[i].x, hdr_colors[i].y, hdr_colors[i].z,
               display_rgb[i].x, display_rgb[i].y, display_rgb[i].z);
    }

    free(display_rgb);
    alwan_destroy(ctx);
    return 0;
}
```

---

### Example 6: HDR ICtCp Conversion

Use ICtCp for HDR color difference calculations.

```c
#include "alwan.h"
#include <math.h>
#include <stdio.h>

int main(void) {
    alwan_ctx *ctx = alwan_create(NULL);

    // HDR colors in XYZ (high luminance)
    alwan_vec3 xyz1 = {50.0, 60.0, 40.0};  // Bright
    alwan_vec3 xyz2 = {51.0, 61.0, 41.0};  // Slightly different

    // Convert to ICtCp (PQ transfer for HDR)
    alwan_vec3 ictcp1, ictcp2;
    alwan_xyz_to_ictcp(&ictcp1, &xyz1, "pq", 1, 0, 0);
    alwan_xyz_to_ictcp(&ictcp2, &xyz2, "pq", 1, 0, 0);

    // Calculate ΔE in ICtCp
    float dI = ictcp1.x - ictcp2.x;
    float dCt = ictcp1.y - ictcp2.y;
    float dCp = ictcp1.z - ictcp2.z;
    float delta_e_ictcp = sqrt(dI*dI + dCt*dCt + dCp*dCp);

    printf("HDR Color Difference (ΔE ICtCp): %.4f\n", delta_e_ictcp);

    alwan_destroy(ctx);
    return 0;
}
```

---

## Color Grading

### Example 7: Lift-Gamma-Gain Adjustment

Implement color grading controls.

```c
#include "alwan.h"

// Lift-Gamma-Gain adjustment
void apply_lgg(alwan_vec3 *rgb, alwan_vec3 lift, alwan_vec3 gamma, alwan_vec3 gain) {
    // Lift: add offset (affects shadows)
    rgb->x = rgb->x + lift.x;
    rgb->y = rgb->y + lift.y;
    rgb->z = rgb->z + lift.z;

    // Gamma: power function (affects midtones)
    rgb->x = pow(fmax(0, rgb->x), 1.0 / gamma.x);
    rgb->y = pow(fmax(0, rgb->y), 1.0 / gamma.y);
    rgb->z = pow(fmax(0, rgb->z), 1.0 / gamma.z);

    // Gain: multiply (affects highlights)
    rgb->x *= gain.x;
    rgb->y *= gain.y;
    rgb->z *= gain.z;
}

int main(void) {
    alwan_ctx *ctx = alwan_create(NULL);

    // Sample image data (linear RGB)
    alwan_vec3 image[1920 * 1080];
    // ... load image ...

    // Color grading parameters
    alwan_vec3 lift = {0.05, 0.02, 0.0};    // Warm shadows
    alwan_vec3 gamma = {1.1, 1.0, 0.9};     // Desaturate midtones
    alwan_vec3 gain = {1.1, 1.0, 0.95};     // Warm highlights

    // Apply grading
    for (int i = 0; i < 1920 * 1080; i++) {
        apply_lgg(&image[i], lift, gamma, gain);
    }

    printf("Applied lift-gamma-gain color grading\n");

    alwan_destroy(ctx);
    return 0;
}
```

---

### Example 8: Hue Shift in LCh

Shift hue by a specific angle in perceptual space.

```c
#include "alwan.h"
#include <math.h>

void shift_hue(alwan_vec3 *rgb, float hue_shift_degrees) {
    // Convert RGB → XYZ → Lab → LCh
    alwan_vec3 xyz, lab, lch;
    alwan_rgb_to_xyz(&xyz, rgb, 1, 0, 0);
    alwan_xyz_to_lab(&lab, &xyz, &alwan_d65_xyz, 1, 0, 0);
    alwan_lab_to_lch(&lch, &lab, 1, 0, 0);

    // Shift hue
    lch.z += hue_shift_degrees;
    if (lch.z >= 360.0) lch.z -= 360.0;
    if (lch.z < 0.0) lch.z += 360.0;

    // Convert back: LCh → Lab → XYZ → RGB
    alwan_lch_to_lab(&lab, &lch, 1, 0, 0);
    alwan_lab_to_xyz(&xyz, &lab, &alwan_d65_xyz, 1, 0, 0);
    alwan_xyz_to_rgb(rgb, &xyz, 1, 0, 0);
}

int main(void) {
    alwan_vec3 color = {1.0, 0.0, 0.0};  // Red

    printf("Original: (%.3f, %.3f, %.3f)\n", color.x, color.y, color.z);

    shift_hue(&color, 60.0);  // Shift by 60° (red → yellow)

    printf("Shifted:  (%.3f, %.3f, %.3f)\n", color.x, color.y, color.z);

    return 0;
}
```

---

## Perceptual Operations

### Example 9: Perceptual Color Difference

Calculate ΔE2000 between two colors.

```c
#include "alwan.h"
#include <stdio.h>

int main(void) {
    alwan_ctx *ctx = alwan_create(NULL);

    // Two colors in RGB
    alwan_vec3 rgb1 = {0.8, 0.2, 0.1};
    alwan_vec3 rgb2 = {0.82, 0.21, 0.11};

    // Convert to Lab
    alwan_vec3 xyz1, xyz2, lab1, lab2;
    alwan_rgb_to_xyz(&xyz1, &rgb1, 1, 0, 0);
    alwan_rgb_to_xyz(&xyz2, &rgb2, 1, 0, 0);
    alwan_xyz_to_lab(&lab1, &xyz1, &alwan_d65_xyz, 1, 0, 0);
    alwan_xyz_to_lab(&lab2, &xyz2, &alwan_d65_xyz, 1, 0, 0);

    // Calculate ΔE2000
    alwan_scalar delta_e;
    alwan_delta_e_2000(&delta_e, &lab1, &lab2, 1, 0, 0);

    printf("ΔE2000 = %.4f\n", delta_e);

    if (delta_e < 1.0) {
        printf("Colors are perceptually identical\n");
    } else if (delta_e < 2.3) {
        printf("Just noticeable difference\n");
    } else if (delta_e < 5.0) {
        printf("Clearly different\n");
    } else {
        printf("Very different colors\n");
    }

    alwan_destroy(ctx);
    return 0;
}
```

---

### Example 10: Find Closest Palette Color

Find the perceptually closest color from a palette.

```c
#include "alwan.h"
#include <float.h>
#include <stdio.h>

int find_closest_color(const alwan_vec3 *target_lab,
                       const alwan_vec3 *palette_lab,
                       int palette_size) {
    int closest_idx = 0;
    alwan_scalar min_delta_e = FLT_MAX;

    for (int i = 0; i < palette_size; i++) {
        alwan_scalar delta_e;
        alwan_delta_e_2000(&delta_e, target_lab, &palette_lab[i], 1, 0, 0);

        if (delta_e < min_delta_e) {
            min_delta_e = delta_e;
            closest_idx = i;
        }
    }

    return closest_idx;
}

int main(void) {
    alwan_ctx *ctx = alwan_create(NULL);

    // Target color (RGB)
    alwan_vec3 target_rgb = {0.7, 0.3, 0.2};

    // Palette (RGB)
    alwan_vec3 palette_rgb[] = {
        {1.0, 0.0, 0.0},  // Red
        {0.0, 1.0, 0.0},  // Green
        {0.0, 0.0, 1.0},  // Blue
        {1.0, 1.0, 0.0},  // Yellow
        {1.0, 0.5, 0.0},  // Orange
        {0.5, 0.0, 0.5}   // Purple
    };
    int palette_size = sizeof(palette_rgb) / sizeof(palette_rgb[0]);

    // Convert all to Lab
    alwan_vec3 target_xyz, target_lab;
    alwan_rgb_to_xyz(&target_xyz, &target_rgb, 1, 0, 0);
    alwan_xyz_to_lab(&target_lab, &target_xyz, &alwan_d65_xyz, 1, 0, 0);

    alwan_vec3 palette_lab[palette_size];
    for (int i = 0; i < palette_size; i++) {
        alwan_vec3 xyz;
        alwan_rgb_to_xyz(&xyz, &palette_rgb[i], 1, 0, 0);
        alwan_xyz_to_lab(&palette_lab[i], &xyz, &alwan_d65_xyz, 1, 0, 0);
    }

    // Find closest
    int closest = find_closest_color(&target_lab, palette_lab, palette_size);

    printf("Closest palette color: #%d\n", closest);
    printf("RGB: (%.3f, %.3f, %.3f)\n",
           palette_rgb[closest].x,
           palette_rgb[closest].y,
           palette_rgb[closest].z);

    alwan_destroy(ctx);
    return 0;
}
```

---

## Custom Allocators

### Example 11: Arena Allocator

Use an arena allocator for deterministic memory usage.

```c
#include "alwan.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    void *base;
    size_t size;
    size_t used;
} Arena;

Arena g_arena;

void* arena_alloc(size_t size, size_t align) {
    size_t aligned_used = (g_arena.used + align - 1) & ~(align - 1);
    if (aligned_used + size > g_arena.size) {
        return NULL;  // Out of arena space
    }
    void *ptr = (char*)g_arena.base + aligned_used;
    g_arena.used = aligned_used + size;
    return ptr;
}

void arena_free(void *ptr) {
    // No-op for arena (free all at once)
    (void)ptr;
}

int main(void) {
    // Allocate arena (1 MB)
    g_arena.base = malloc(1024 * 1024);
    g_arena.size = 1024 * 1024;
    g_arena.used = 0;

    // Create Alwan context with arena allocator
    alwan_ctx *ctx = alwan_create(&(alwan_config){
        .alloc_cb = arena_alloc,
        .free_cb = arena_free
    });

    if (!ctx) {
        fprintf(stderr, "Failed to create context (arena too small?)\n");
        free(g_arena.base);
        return 1;
    }

    printf("Arena used: %zu bytes\n", g_arena.used);

    // Use context...

    alwan_destroy(ctx);

    // Free entire arena at once
    free(g_arena.base);

    return 0;
}
```

---

## Multi-threading

### Example 12: Parallel Image Processing

Process image tiles in parallel with OpenMP.

```c
#include "alwan.h"
#include <omp.h>
#include <stdio.h>

int main(void) {
    // Create one context per thread
    int num_threads = omp_get_max_threads();
    alwan_ctx **contexts = malloc(num_threads * sizeof(alwan_ctx*));

    for (int i = 0; i < num_threads; i++) {
        contexts[i] = alwan_create(NULL);
    }

    // Image data
    int width = 3840, height = 2160;
    float *rgb_linear = malloc(width * height * 3 * sizeof(float));
    float *rgb_bt2020 = malloc(width * height * 3 * sizeof(float));

    // Load image...

    // Process in parallel
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        alwan_ctx *ctx = contexts[tid];

        #pragma omp for schedule(dynamic, 1024)
        for (int i = 0; i < width * height; i++) {
            alwan_vec3 *src = (alwan_vec3*)&rgb_linear[i * 3];
            alwan_vec3 *dst = (alwan_vec3*)&rgb_bt2020[i * 3];

            alwan_rgb_convert(dst, ctx, "srgb", "bt2020", src, 1, 0, 0);
        }
    }

    printf("Processed %dx%d image with %d threads\n", width, height, num_threads);

    // Cleanup
    for (int i = 0; i < num_threads; i++) {
        alwan_destroy(contexts[i]);
    }
    free(contexts);
    free(rgb_linear);
    free(rgb_bt2020);

    return 0;
}
```

---

## Error Handling

### Example 13: Comprehensive Error Checking

Handle all possible error conditions.

```c
#include "alwan.h"
#include <stdio.h>

const char* alwan_error_string(alwan_result result) {
    switch (result) {
        case ALWAN_SUCCESS: return "Success";
        case ALWAN_ERROR_INVALID_PARAMETER: return "Invalid parameter";
        case ALWAN_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case ALWAN_ERROR_NOT_FOUND: return "Not found";
        case ALWAN_ERROR_UNSUPPORTED: return "Unsupported";
        default: return "Unknown error";
    }
}

int main(void) {
    // 1. Check context creation
    alwan_ctx *ctx = alwan_create(NULL);
    if (!ctx) {
        fprintf(stderr, "Error: Failed to create Alwan context\n");
        return 1;
    }

    // 2. Check color space conversion
    alwan_vec3 rgb = {1.0, 0.5, 0.0};
    alwan_vec3 converted;
    alwan_result result = alwan_rgb_convert(&converted, ctx, "srgb", "bt2020",
                                            &rgb, 1, 0, 0);
    if (result != ALWAN_SUCCESS) {
        fprintf(stderr, "Error: RGB conversion failed: %s\n",
                alwan_error_string(result));
        alwan_destroy(ctx);
        return 1;
    }

    // 3. Check for invalid color space name
    result = alwan_rgb_convert(&converted, ctx, "srgb", "invalid_space",
                               &rgb, 1, 0, 0);
    if (result == ALWAN_ERROR_NOT_FOUND) {
        printf("Expected error: Color space 'invalid_space' not found\n");
    }

    // 4. Check for NULL parameters
    result = alwan_rgb_convert(&converted, ctx, "srgb", "bt2020",
                               NULL, 1, 0, 0);
    if (result == ALWAN_ERROR_INVALID_PARAMETER) {
        printf("Expected error: NULL input pointer\n");
    }

    alwan_destroy(ctx);
    return 0;
}
```

---

## See Also

- [API Reference](api/) — Detailed function documentation
- [Getting Started](getting-started.md) — Basic usage guide
- [Configuration](configuration.md) — Build options
- [Precision & Limits](precision-and-limits.md) — Numerical details
