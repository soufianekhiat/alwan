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
9. [Shader Usage (HLSL/GLSL)](#shader-usage-hlslglsl)
10. [Library Integrations](#library-integrations)

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
    rgb->x = alwan_clamp(rgb->x, 0.0, 1.0);
    rgb->y = alwan_clamp(rgb->y, 0.0, 1.0);
    rgb->z = alwan_clamp(rgb->z, 0.0, 1.0);
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

    if (srgb.x < 0.0 || srgb.x > 1.0 ||
        srgb.y < 0.0 || srgb.y > 1.0 ||
        srgb.z < 0.0 || srgb.z > 1.0) {
        printf("/!\\ Out of sRGB gamut! Clipping...\n");
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
    alwan_scalar dI = ictcp1.x - ictcp2.x;
    alwan_scalar dCt = ictcp1.y - ictcp2.y;
    alwan_scalar dCp = ictcp1.z - ictcp2.z;
    alwan_scalar delta_e_ictcp = ALWAN_SQRT(dI*dI + dCt*dCt + dCp*dCp);

    printf("HDR Color Difference (ΔE ICtCp): %.4f\n", (double)delta_e_ictcp);

    alwan_destroy(ctx);
    return 0;
}
```

---

## Color Grading

### Example 7: Lift-Gamma-Gain Adjustment

Apply color grading controls using the batch API.

```c
#include "alwan.h"
#include <stdlib.h>
#include <stdio.h>

int main(void) {
    // Sample image data (linear RGB)
    size_t n = 1920 * 1080;
    size_t stride = 3 * sizeof(alwan_scalar);
    alwan_scalar *image = malloc(n * stride);

    // ... load image ...

    // Color grading parameters
    alwan_rgb lift  = {0.05, 0.02, 0.0};     // Warm shadows
    alwan_rgb gamma = {1.1,  1.0,  0.9};     // Adjust midtones
    alwan_rgb gain  = {1.1,  1.0,  0.95};    // Warm highlights

    // Apply LGG in one batch call (in-place)
    alwan_lgg_apply_map_interleave(
        image, image, &lift, &gamma, &gain,
        n, stride, stride
    );

    printf("Applied lift-gamma-gain color grading\n");

    free(image);
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

## Shader Usage (HLSL/GLSL)

Alwan's core headers (`*_core.h`) are GPU-compatible: all functions are `ALWAN_INLINE`, branchless via `ALWAN_SELECT`, and use portable math macros (`ALWAN_POW`, `ALWAN_SQRT`, `ALWAN_LITERAL`, etc.). Include the backend bootstrap (`alwan_hlsl.h` or `alwan_glsl.h`) followed by whichever `*_core.h` modules you need.

In HLSL/GLSL, `alwan_scalar` = `float`, `alwan_vec3` maps to `float3`/`vec3`, `alwan_mat3x3` maps to `float3x3`/`mat3`, and all semantic types (`alwan_rgb`, `alwan_oklab`, ...) are plain structs with named members.

---

### HLSL — sRGB Decode + OkLab in a Compute Shader

```hlsl
#include "alwan_hlsl.h"
#include "core/alwan_core.h"
#include "core/alwan_oklab_core.h"
#include "core/alwan_colorspace_core.h"

RWTexture2D<float4> InputTex  : register(u0);
RWTexture2D<float4> OutputTex : register(u1);

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    float4 rgba = InputTex[id.xy];

    /* sRGB EOTF per channel (encoded -> linear) */
    alwan_rgb linear_rgb;
    linear_rgb.r = alwan_srgb_eotf(rgba.r);
    linear_rgb.g = alwan_srgb_eotf(rgba.g);
    linear_rgb.b = alwan_srgb_eotf(rgba.b);

    /* Linear sRGB -> XYZ (D65) via NPM (from CSV data) */
    alwan_vec3 rgb_v = {{linear_rgb.r, linear_rgb.g, linear_rgb.b}};
    ALWAN_CONSTEXPR alwan_mat3x3 srgb_npm = {{
    #include "data/matrices/aces_rec709_to_xyz.csv"
    }};
    alwan_vec3 xyz_v = alwan_mat3_mulv_v(srgb_npm, rgb_v);

    /* XYZ -> OkLab */
    alwan_xyz xyz = {xyz_v.v[0], xyz_v.v[1], xyz_v.v[2]};
    alwan_oklab lab = alwan_xyz_to_oklab_v(xyz);

    OutputTex[id.xy] = float4(lab.L, lab.a, lab.b, rgba.a);
}
```

### HLSL — AgX Tone Mapping (Full-Screen Pass)

```hlsl
#include "alwan_hlsl.h"
#include "core/alwan_core.h"
#include "core/alwan_view_core.h"

Texture2D<float4> SceneHDR : register(t0);
RWTexture2D<float4> Display : register(u0);

/* AgX matrices from CSV data */
ALWAN_CONSTEXPR alwan_mat3x3 AGX_INSET = {{
#include "data/matrices/agx_inset.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 AGX_OUTSET = {{
#include "data/matrices/agx_outset.csv"
}};

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    float4 hdr = SceneHDR[id.xy];
    alwan_vec3 rgb = {{alwan_max(hdr.r, ALWAN_LITERAL(0.0)),
                       alwan_max(hdr.g, ALWAN_LITERAL(0.0)),
                       alwan_max(hdr.b, ALWAN_LITERAL(0.0))}};

    /* 1. Inset matrix (rotate into AgX working space) */
    alwan_vec3 agx = alwan_mat3_mulv_v(AGX_INSET, rgb);

    /* 2. Log2 encoding to normalized [0,1] */
    alwan_scalar min_ev = ALWAN_LITERAL(-12.47393);
    alwan_scalar max_ev = ALWAN_LITERAL(4.026069);
    agx.v[0] = alwan_agx_log_encode_v(agx.v[0], min_ev, max_ev);
    agx.v[1] = alwan_agx_log_encode_v(agx.v[1], min_ev, max_ev);
    agx.v[2] = alwan_agx_log_encode_v(agx.v[2], min_ev, max_ev);

    /* 3. Sigmoid curve (per channel) */
    agx.v[0] = alwan_agx_curve_v(agx.v[0]);
    agx.v[1] = alwan_agx_curve_v(agx.v[1]);
    agx.v[2] = alwan_agx_curve_v(agx.v[2]);

    /* 4. Outset matrix (recover display-referred RGB) */
    alwan_vec3 display_rgb = alwan_mat3_mulv_v(AGX_OUTSET, agx);

    /* 5. sRGB OETF for display */
    display_rgb.v[0] = alwan_srgb_oetf(alwan_clamp(display_rgb.v[0], ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)));
    display_rgb.v[1] = alwan_srgb_oetf(alwan_clamp(display_rgb.v[1], ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)));
    display_rgb.v[2] = alwan_srgb_oetf(alwan_clamp(display_rgb.v[2], ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)));

    Display[id.xy] = float4(display_rgb.v[0], display_rgb.v[1], display_rgb.v[2], hdr.a);
}
```

### HLSL — Color Grading (LGG + White Balance)

```hlsl
#include "alwan_hlsl.h"
#include "core/alwan_core.h"
#include "core/alwan_color_correction_core.h"

Texture2D<float4> LinearInput : register(t0);
RWTexture2D<float4> GradedOutput : register(u0);

cbuffer GradeParams : register(b0)
{
    float3 cb_lift;
    float3 cb_gamma;
    float3 cb_gain;
    float3 cb_wb_multipliers;
};

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    float4 px = LinearInput[id.xy];

    /* White balance (multiply by per-channel factors) */
    alwan_rgb rgb = {px.r * cb_wb_multipliers.x,
                     px.g * cb_wb_multipliers.y,
                     px.b * cb_wb_multipliers.z};

    /* Lift/Gamma/Gain (core function — branchless via ALWAN_SELECT) */
    alwan_rgb lift  = {cb_lift.x,  cb_lift.y,  cb_lift.z};
    alwan_rgb gamma = {cb_gamma.x, cb_gamma.y, cb_gamma.z};
    alwan_rgb gain  = {cb_gain.x,  cb_gain.y,  cb_gain.z};

    alwan_rgb graded = alwan_lgg_apply_v(rgb, lift, gamma, gain);

    GradedOutput[id.xy] = float4(graded.r, graded.g, graded.b, px.a);
}
```

### HLSL — PQ (HDR10) Encoding

```hlsl
#include "alwan_hlsl.h"
#include "core/alwan_core.h"

Texture2D<float4> LinearHDR : register(t0);
RWTexture2D<float4> PQOutput : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    float4 px = LinearHDR[id.xy];

    /* Scene-linear (0-10000 cd/m2) -> PQ code values */
    float r_pq = alwan_pq_oetf(px.r);
    float g_pq = alwan_pq_oetf(px.g);
    float b_pq = alwan_pq_oetf(px.b);

    PQOutput[id.xy] = float4(r_pq, g_pq, b_pq, px.a);
}
```

### HLSL — Color Vision Deficiency Simulation

```hlsl
#include "alwan_hlsl.h"
#include "core/alwan_core.h"
#include "core/alwan_vision_core.h"

Texture2D<float4> InputTex : register(t0);
RWTexture2D<float4> OutputTex : register(u0);

cbuffer CVDParams : register(b0)
{
    float severity;  /* 0.0 = normal, 1.0 = full dichromacy */
    int cvd_type;    /* 0 = protanopia, 1 = deuteranopia, 2 = tritanopia */
};

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    float4 px = InputTex[id.xy];

    /* Decode sRGB -> linear */
    alwan_rgb linear_rgb;
    linear_rgb.r = alwan_srgb_eotf(px.r);
    linear_rgb.g = alwan_srgb_eotf(px.g);
    linear_rgb.b = alwan_srgb_eotf(px.b);

    /* Simulate CVD on linear RGB (select type by constant buffer) */
    alwan_rgb simulated = linear_rgb;
    if (cvd_type == 0) simulated = alwan_simulate_protanopia_v(linear_rgb, severity);
    if (cvd_type == 1) simulated = alwan_simulate_deuteranopia_v(linear_rgb, severity);
    if (cvd_type == 2) simulated = alwan_simulate_tritanopia_v(linear_rgb, severity);

    /* Re-encode to sRGB */
    simulated.r = alwan_srgb_oetf(simulated.r);
    simulated.g = alwan_srgb_oetf(simulated.g);
    simulated.b = alwan_srgb_oetf(simulated.b);

    OutputTex[id.xy] = float4(simulated.r, simulated.g, simulated.b, px.a);
}
```

### GLSL — Fragment Shader OkLab Hue Shift

The same core headers work in GLSL. Include `alwan_glsl.h` instead.

```glsl
#include "alwan_glsl.h"
#include "core/alwan_core.h"
#include "core/alwan_oklab_core.h"

uniform sampler2D u_texture;
uniform float u_hue_shift;  /* radians */

in vec2 v_texcoord;
out vec4 frag_color;

void main()
{
    vec4 px = texture(u_texture, v_texcoord);

    /* sRGB EOTF (decode) */
    alwan_rgb linear_rgb;
    linear_rgb.r = alwan_srgb_eotf(px.r);
    linear_rgb.g = alwan_srgb_eotf(px.g);
    linear_rgb.b = alwan_srgb_eotf(px.b);

    /* Linear sRGB -> XYZ -> OkLab -> OkLCh */
    alwan_vec3 rgb_v;
    rgb_v.v[0] = linear_rgb.r;
    rgb_v.v[1] = linear_rgb.g;
    rgb_v.v[2] = linear_rgb.b;

    ALWAN_CONSTEXPR alwan_mat3x3 srgb_npm = {{
    #include "data/matrices/aces_rec709_to_xyz.csv"
    }};
    alwan_vec3 xyz_v = alwan_mat3_mulv_v(srgb_npm, rgb_v);

    alwan_xyz xyz;
    xyz.x = xyz_v.v[0]; xyz.y = xyz_v.v[1]; xyz.z = xyz_v.v[2];

    alwan_oklab lab = alwan_xyz_to_oklab_v(xyz);
    alwan_oklch lch = alwan_oklab_to_oklch_v(lab);

    /* Shift hue */
    lch.h = lch.h + u_hue_shift;

    /* OkLCh -> OkLab -> XYZ -> linear sRGB */
    lab = alwan_oklch_to_oklab_v(lch);
    xyz = alwan_oklab_to_xyz_v(lab);

    xyz_v.v[0] = xyz.x; xyz_v.v[1] = xyz.y; xyz_v.v[2] = xyz.z;

    ALWAN_CONSTEXPR alwan_mat3x3 srgb_npm_inv = {{
    #include "data/matrices/aces_xyz_to_rec709.csv"
    }};
    alwan_vec3 out_v = alwan_mat3_mulv_v(srgb_npm_inv, xyz_v);

    /* Gamut clamp + sRGB OETF */
    float r = alwan_srgb_oetf(alwan_clamp(out_v.v[0], ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)));
    float g = alwan_srgb_oetf(alwan_clamp(out_v.v[1], ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)));
    float b = alwan_srgb_oetf(alwan_clamp(out_v.v[2], ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)));

    frag_color = vec4(r, g, b, px.a);
}
```

---

## Library Integrations

Real-world examples showing zero-copy interop between alwan and common image/media libraries, using alwan's stride-based batch API.

---

### stb_image — Load JPEG and Convert to OkLab

`stbi_load` returns contiguous interleaved RGB with no padding. Use `_map_interleave_ex` for direct U8 input.

```c
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <alwan/alwan.h>
#include <stdlib.h>

int main(void)
{
    int w, h, ch;
    unsigned char *img = stbi_load("photo.jpg", &w, &h, &ch, 3);
    if (!img) return 1;

    size_t n = (size_t)w * h;

    /* stb_image: packed RGB u8, stride = 3 bytes */
    float *oklab = malloc(n * 3 * sizeof(float));

    alwan_srgb_to_oklab_map_interleave_ex(
        oklab, ALWAN_PIXEL_F32,
        img,   ALWAN_PIXEL_U8,
        n, 3, 12  /* in_stride=3 (u8 RGB), out_stride=12 (f32 triplet) */
    );

    /* oklab[] now contains OkLab for every pixel */

    free(oklab);
    stbi_image_free(img);
    return 0;
}
```

#### stb_image — HDR to JzAzBz

`stbi_loadf` returns packed float32 RGB. Use `ALWAN_PIXEL_F32` with 12-byte strides.

```c
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <alwan/alwan.h>
#include <stdlib.h>

int main(void)
{
    int w, h, ch;
    float *hdr = stbi_loadf("scene.hdr", &w, &h, &ch, 3);
    if (!hdr) return 1;

    size_t n = (size_t)w * h;

    /* HDR is scene-linear. Go straight to JzAzBz (perceptual HDR space).
     * First: linear sRGB/BT.709 -> XYZ, then XYZ -> JzAzBz */
    float *xyz = malloc(n * 3 * sizeof(float));
    float *jab = malloc(n * 3 * sizeof(float));

    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    alwan_mat3x3 srgb_npm = {{
    #include "data/matrices/aces_rec709_to_xyz.csv"
    }};
    ALWAN_DIAG_POP

    alwan_mat3_transform_map_interleave_ex(
        xyz, ALWAN_PIXEL_F32,
        &srgb_npm,
        hdr, ALWAN_PIXEL_F32,
        n, 12, 12
    );
    alwan_xyz_to_jzazbz_map_interleave_ex(
        jab, ALWAN_PIXEL_F32,
        xyz, ALWAN_PIXEL_F32,
        n, 12, 12
    );

    free(xyz);
    free(jab);
    stbi_image_free(hdr);
    return 0;
}
```

---

### libraw — RAW Decode to CIE Lab

libraw's `dcraw_make_mem_image()` returns contiguous interleaved RGB (8 or 16 bit). Use `ALWAN_PIXEL_U16` for 16-bit output.

```c
#include <libraw/libraw.h>
#include <alwan/alwan.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    libraw_data_t *raw = libraw_init(0);
    libraw_open_file(raw, argv[1]);
    libraw_unpack(raw);

    /* Output 16-bit linear RGB (no gamma curve applied) */
    raw->params.output_bps = 16;
    raw->params.gamm[0] = raw->params.gamm[1] = 1.0;  /* linear */
    raw->params.no_auto_bright = 1;

    libraw_dcraw_process(raw);
    libraw_processed_image_t *img = libraw_dcraw_make_mem_image(raw, NULL);

    size_t n = (size_t)img->width * img->height;

    /* Packed RGB u16, stride = 6 bytes */
    alwan_xyz white = { 0.95047, 1.0, 1.08883 };
    float *lab = malloc(n * 3 * sizeof(float));

    /* u16 sRGB -> Lab in one step */
    alwan_srgb_to_lab_map_interleave_ex(
        lab,        ALWAN_PIXEL_F32,
        img->data,  ALWAN_PIXEL_U16,
        n, 6, 12
    );

    /* Apply white balance correction using camera multipliers */
    float *xyz = malloc(n * 3 * sizeof(float));
    alwan_srgb_to_xyz_map_interleave_ex(
        xyz,        ALWAN_PIXEL_F32,
        img->data,  ALWAN_PIXEL_U16,
        n, 6, 12
    );

    alwan_rgb wb = {
        raw->color.cam_mul[0] / raw->color.cam_mul[1],
        1.0,
        raw->color.cam_mul[2] / raw->color.cam_mul[1]
    };
    alwan_white_balance_apply_map_interleave(
        xyz, xyz, &wb, n,
        3 * sizeof(float), 3 * sizeof(float)
    );

    free(xyz);
    free(lab);
    libraw_dcraw_free_mem(img);
    libraw_recycle(raw);
    libraw_close(raw);
    return 0;
}
```

---

### OpenColorIO — OCIO + Alwan Pipeline

OCIO's `PackedImageDesc` uses float32 with configurable strides. Run the OCIO transform first, then feed into alwan for perceptual analysis.

```cpp
#include <OpenColorIO/OpenColorIO.h>
#include <alwan/alwan.h>
#include <cstdlib>

namespace OCIO = OCIO_NAMESPACE;

int main()
{
    int w = 1920, h = 1080;
    size_t n = (size_t)w * h;
    float *pixels = (float *)calloc(n * 4, sizeof(float));  /* RGBA f32 */

    /* -- OCIO: ACEScg -> sRGB ------------------------------------- */
    OCIO::ConstConfigRcPtr config = OCIO::Config::CreateFromEnv();
    OCIO::ConstProcessorRcPtr proc =
        config->getProcessor("ACES - ACEScg", "Output - sRGB");
    OCIO::ConstCPUProcessorRcPtr cpu = proc->getDefaultCPUProcessor();

    OCIO::PackedImageDesc desc(
        pixels, w, h, 4,
        OCIO::AutoStride, OCIO::AutoStride, OCIO::AutoStride
    );
    cpu->apply(desc);  /* in-place ACEScg -> sRGB */

    /* -- Alwan: sRGB -> OkLab with RGBA stride (skip alpha) ------- */
    float *oklab = (float *)malloc(n * 3 * sizeof(float));

    alwan_srgb_to_oklab_map_interleave_ex(
        oklab,  ALWAN_PIXEL_F32,
        pixels, ALWAN_PIXEL_F32,
        n,
        4 * sizeof(float),   /* in_stride: RGBA, alpha skipped */
        3 * sizeof(float)    /* out_stride: packed OkLab */
    );

    /* oklab[] ready for gamut analysis, delta E, etc. */

    free(oklab);
    free(pixels);
    return 0;
}
```

#### OCIO PlanarImageDesc — Alwan Planar API

For planar workflows (compositing, Nuke-style), combine OCIO `PlanarImageDesc` with alwan's `_map_planar` functions.

```cpp
#include <OpenColorIO/OpenColorIO.h>
#include <alwan/alwan.h>
#include <cstdlib>

namespace OCIO = OCIO_NAMESPACE;

int main()
{
    int w = 1920, h = 1080;
    size_t n = (size_t)w * h;
    size_t plane_bytes = n * sizeof(float);

    float *r = (float *)malloc(plane_bytes);
    float *g = (float *)malloc(plane_bytes);
    float *b = (float *)malloc(plane_bytes);

    /* ... fill r, g, b planes ... */

    /* OCIO planar transform: linear -> sRGB */
    OCIO::ConstConfigRcPtr config = OCIO::Config::CreateFromEnv();
    OCIO::ConstProcessorRcPtr proc = config->getProcessor("linear", "sRGB");
    OCIO::ConstCPUProcessorRcPtr cpu = proc->getDefaultCPUProcessor();

    OCIO::PlanarImageDesc desc(r, g, b, nullptr, w, h);
    cpu->apply(desc);

    /* Alwan planar: sRGB -> Lab */
    float *lp = (float *)malloc(plane_bytes);
    float *ap = (float *)malloc(plane_bytes);
    float *bp = (float *)malloc(plane_bytes);

    alwan_srgb_to_lab_map_planar_ex(
        lp, ap, bp,  ALWAN_PIXEL_F32,
        r,  g,  b,   ALWAN_PIXEL_F32,
        n,
        sizeof(float), sizeof(float)
    );

    free(lp); free(ap); free(bp);
    free(r); free(g); free(b);
    return 0;
}
```

---

### OpenEXR — Per-Channel Reading with Planar Conversion

OpenEXR stores channels independently. Read each channel plane, then use alwan's planar API.

```cpp
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <alwan/alwan.h>
#include <vector>

int main()
{
    Imf::InputFile file("scene.exr");
    Imath::Box2i dw = file.header().dataWindow();
    int w = dw.max.x - dw.min.x + 1;
    int h = dw.max.y - dw.min.y + 1;
    size_t n = (size_t)w * h;

    /* Allocate per-channel float buffers (OpenEXR half -> float on read) */
    std::vector<float> r(n), g(n), b(n);

    Imf::FrameBuffer fb;
    size_t xs = sizeof(float);
    size_t ys = sizeof(float) * w;
    float *base_r = r.data() - dw.min.x - (size_t)dw.min.y * w;
    float *base_g = g.data() - dw.min.x - (size_t)dw.min.y * w;
    float *base_b = b.data() - dw.min.x - (size_t)dw.min.y * w;

    fb.insert("R", Imf::Slice(Imf::FLOAT, (char *)base_r, xs, ys));
    fb.insert("G", Imf::Slice(Imf::FLOAT, (char *)base_g, xs, ys));
    fb.insert("B", Imf::Slice(Imf::FLOAT, (char *)base_b, xs, ys));

    file.setFrameBuffer(fb);
    file.readPixels(dw.min.y, dw.max.y);

    /* EXR data is scene-linear. Convert to ICtCp (PQ) for HDR analysis.
     * Assumes BT.2020 primaries (common for VFX EXR). */
    std::vector<float> ic(n), ct(n), cp(n);

    alwan_rgb_to_ictcp_map_planar(
        (alwan_scalar *)ic.data(),
        (alwan_scalar *)ct.data(),
        (alwan_scalar *)cp.data(),
        (alwan_scalar const *)r.data(),
        (alwan_scalar const *)g.data(),
        (alwan_scalar const *)b.data(),
        1,   /* use_pq = true (ST 2084) */
        n,
        sizeof(float), sizeof(float)
    );

    return 0;
}
```

#### OpenEXR — Writing Lab as EXR Channels

```cpp
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfChannelList.h>
#include <alwan/alwan.h>
#include <vector>

void write_lab_exr(const char *path, const float *lab, int w, int h)
{
    size_t n = (size_t)w * h;

    /* Deinterleave Lab triplets into separate planes for EXR */
    std::vector<float> lp(n), ap(n), bp(n);
    for (size_t i = 0; i < n; ++i) {
        lp[i] = lab[i * 3 + 0];
        ap[i] = lab[i * 3 + 1];
        bp[i] = lab[i * 3 + 2];
    }

    Imf::Header hdr(w, h);
    hdr.channels().insert("L", Imf::Channel(Imf::FLOAT));
    hdr.channels().insert("a", Imf::Channel(Imf::FLOAT));
    hdr.channels().insert("b", Imf::Channel(Imf::FLOAT));

    Imf::FrameBuffer fb;
    size_t xs = sizeof(float), ys = sizeof(float) * w;
    fb.insert("L", Imf::Slice(Imf::FLOAT, (char *)lp.data(), xs, ys));
    fb.insert("a", Imf::Slice(Imf::FLOAT, (char *)ap.data(), xs, ys));
    fb.insert("b", Imf::Slice(Imf::FLOAT, (char *)bp.data(), xs, ys));

    Imf::OutputFile out(path, hdr);
    out.setFrameBuffer(fb);
    out.writePixels(h);
}
```

#### OpenEXR — Tiled Reading with Per-Tile Processing

EXR tiles are an I/O concept (compression blocks), not a pixel layout. Each tile decompresses into a contiguous region addressed through the same `FrameBuffer` slices. Process tiles individually for cache efficiency.

```cpp
#include <OpenEXR/ImfTiledInputFile.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <alwan/alwan.h>
#include <vector>
#include <cstring>

int main()
{
    Imf::TiledInputFile file("scene_tiled.exr");
    Imath::Box2i dw = file.header().dataWindow();
    int img_w = dw.max.x - dw.min.x + 1;
    int img_h = dw.max.y - dw.min.y + 1;
    int tw = file.tileXSize();
    int th = file.tileYSize();

    /* Full-image planar buffers for reading */
    std::vector<float> r(img_w * img_h), g(img_w * img_h), b(img_w * img_h);

    Imf::FrameBuffer fb;
    size_t xs = sizeof(float);
    size_t ys = sizeof(float) * img_w;
    float *base_r = r.data() - dw.min.x - (size_t)dw.min.y * img_w;
    float *base_g = g.data() - dw.min.x - (size_t)dw.min.y * img_w;
    float *base_b = b.data() - dw.min.x - (size_t)dw.min.y * img_w;

    fb.insert("R", Imf::Slice(Imf::FLOAT, (char *)base_r, xs, ys));
    fb.insert("G", Imf::Slice(Imf::FLOAT, (char *)base_g, xs, ys));
    fb.insert("B", Imf::Slice(Imf::FLOAT, (char *)base_b, xs, ys));

    file.setFrameBuffer(fb);
    file.readTiles(0, file.numXTiles(0) - 1, 0, file.numYTiles(0) - 1);

    /* Process tile by tile — better cache locality for large images */
    std::vector<float> oklab_l(img_w * img_h);
    std::vector<float> oklab_a(img_w * img_h);
    std::vector<float> oklab_b(img_w * img_h);

    int nx = file.numXTiles(0);
    int ny = file.numYTiles(0);

    for (int ty = 0; ty < ny; ++ty) {
        for (int tx = 0; tx < nx; ++tx) {
            /* Compute tile bounds, clamped to image edges */
            int x0 = tx * tw;
            int y0 = ty * th;
            int x1 = alwan_min(x0 + tw, img_w);
            int y1 = alwan_min(y0 + th, img_h);
            int tile_w = x1 - x0;

            /* Process each row of the tile */
            for (int y = y0; y < y1; ++y) {
                size_t off = (size_t)y * img_w + x0;

                alwan_srgb_to_oklab_map_planar(
                    (alwan_scalar *)&oklab_l[off],
                    (alwan_scalar *)&oklab_a[off],
                    (alwan_scalar *)&oklab_b[off],
                    (alwan_scalar const *)&r[off],
                    (alwan_scalar const *)&g[off],
                    (alwan_scalar const *)&b[off],
                    tile_w,
                    sizeof(float), sizeof(float)
                );
            }
        }
    }

    return 0;
}
```

---

### OpenCV — BGR Mat Processing

OpenCV uses BGR order and may pad rows. Swap B/R first, then use `cv::Mat::step[0]` as row stride.

```cpp
#include <opencv2/opencv.hpp>
#include <alwan/alwan.h>
#include <vector>

int main()
{
    cv::Mat img = cv::imread("photo.png", cv::IMREAD_COLOR);   /* BGR u8 */
    cv::Mat rgb;
    cv::cvtColor(img, rgb, cv::COLOR_BGR2RGB);                  /* -> RGB u8 */

    int w = rgb.cols, h = rgb.rows;
    size_t n = (size_t)w * h;

    std::vector<float> oklab(n * 3);

    if (rgb.isContinuous()) {
        /* No row padding — process entire image at once */
        alwan_srgb_to_oklab_map_interleave_ex(
            oklab.data(), ALWAN_PIXEL_F32,
            rgb.data,     ALWAN_PIXEL_U8,
            n, 3, 12
        );
    } else {
        /* Row padding present — process row by row */
        for (int y = 0; y < h; ++y) {
            alwan_srgb_to_oklab_map_interleave_ex(
                oklab.data() + y * w * 3, ALWAN_PIXEL_F32,
                rgb.ptr(y),               ALWAN_PIXEL_U8,
                w, 3, 12
            );
        }
    }

    return 0;
}
```

#### OpenCV — Delta E 2000 Heatmap

Compute per-pixel color difference between two images.

```cpp
#include <opencv2/opencv.hpp>
#include <alwan/alwan.h>
#include <vector>

int main()
{
    cv::Mat ref_bgr  = cv::imread("reference.png");
    cv::Mat test_bgr = cv::imread("test.png");
    cv::Mat ref_rgb, test_rgb;
    cv::cvtColor(ref_bgr,  ref_rgb,  cv::COLOR_BGR2RGB);
    cv::cvtColor(test_bgr, test_rgb, cv::COLOR_BGR2RGB);

    int w = ref_rgb.cols, h = ref_rgb.rows;
    size_t n = (size_t)w * h;

    /* sRGB u8 -> Lab */
    std::vector<alwan_scalar> lab_ref(n * 3), lab_test(n * 3);
    size_t s = 3 * sizeof(alwan_scalar);

    alwan_srgb_to_lab_map_interleave_ex(
        lab_ref.data(),  ALWAN_PIXEL_SCALAR,
        ref_rgb.data,    ALWAN_PIXEL_U8,
        n, 3, s
    );
    alwan_srgb_to_lab_map_interleave_ex(
        lab_test.data(), ALWAN_PIXEL_SCALAR,
        test_rgb.data,   ALWAN_PIXEL_U8,
        n, 3, s
    );

    /* Per-pixel delta E 2000 */
    cv::Mat de_map(h, w, CV_32FC1);
    for (size_t i = 0; i < n; ++i) {
        alwan_lab a = { lab_ref[i*3],  lab_ref[i*3+1],  lab_ref[i*3+2] };
        alwan_lab b = { lab_test[i*3], lab_test[i*3+1], lab_test[i*3+2] };
        de_map.at<float>((int)(i / w), (int)(i % w)) =
            (float)alwan_delta_e_2000(&a, &b);
    }

    /* Visualize */
    cv::normalize(de_map, de_map, 0, 255, cv::NORM_MINMAX);
    de_map.convertTo(de_map, CV_8UC1);
    cv::Mat heatmap;
    cv::applyColorMap(de_map, heatmap, cv::COLORMAP_JET);
    cv::imwrite("delta_e_heatmap.png", heatmap);

    return 0;
}
```

---

### FFmpeg — Video Frame Color Pipeline

FFmpeg frames use per-plane `data[]`/`linesize[]`. For planar formats, process row-by-row with alwan's planar API.

#### Planar GBR Float (AV_PIX_FMT_GBRPF32)

```c
#include <libavutil/frame.h>
#include <alwan/alwan.h>
#include <stdlib.h>

void process_gbrp_frame(AVFrame *frame)
{
    int w = frame->width, h = frame->height;
    size_t n = (size_t)w * h;

    /* GBRP order: data[0]=G, data[1]=B, data[2]=R */
    float *oklab_l = malloc(n * sizeof(float));
    float *oklab_a = malloc(n * sizeof(float));
    float *oklab_b = malloc(n * sizeof(float));

    for (int y = 0; y < h; ++y) {
        float *row_g = (float *)((char *)frame->data[0] + y * frame->linesize[0]);
        float *row_b = (float *)((char *)frame->data[1] + y * frame->linesize[1]);
        float *row_r = (float *)((char *)frame->data[2] + y * frame->linesize[2]);

        /* Note: reorder G,B,R -> R,G,B for alwan */
        alwan_srgb_to_oklab_map_planar(
            (alwan_scalar *)(oklab_l + y * w),
            (alwan_scalar *)(oklab_a + y * w),
            (alwan_scalar *)(oklab_b + y * w),
            (alwan_scalar const *)row_r,
            (alwan_scalar const *)row_g,
            (alwan_scalar const *)row_b,
            w, sizeof(float), sizeof(float)
        );
    }

    free(oklab_l); free(oklab_a); free(oklab_b);
}
```

#### Packed RGB24 (AV_PIX_FMT_RGB24)

```c
#include <libavutil/frame.h>
#include <alwan/alwan.h>
#include <stdlib.h>

void convert_rgb24_to_lab(AVFrame *frame)
{
    int w = frame->width, h = frame->height;
    int linesize = frame->linesize[0];  /* may include padding */
    uint8_t *data = frame->data[0];

    float *lab = malloc((size_t)w * h * 3 * sizeof(float));

    if (linesize == w * 3) {
        /* No padding: process entire frame at once */
        alwan_srgb_to_lab_map_interleave_ex(
            lab,  ALWAN_PIXEL_F32,
            data, ALWAN_PIXEL_U8,
            (size_t)w * h, 3, 12
        );
    } else {
        /* Row padding: process row by row */
        for (int y = 0; y < h; ++y) {
            alwan_srgb_to_lab_map_interleave_ex(
                lab + y * w * 3,     ALWAN_PIXEL_F32,
                data + y * linesize, ALWAN_PIXEL_U8,
                w, 3, 12
            );
        }
    }

    free(lab);
}
```

---

### Halide — Planar Buffer Pipeline

Halide stores pixels in planar layout by default: `dim(0)=x`, `dim(1)=y`, `dim(2)=channel`. Strides are in elements, not bytes.

```cpp
#include <Halide.h>
#include <HalideBuffer.h>
#include <alwan/alwan.h>

int main()
{
    Halide::Runtime::Buffer<float> input =
        Halide::Tools::load_image("input.png");

    int w = input.width(), h = input.height();
    size_t n = (size_t)w * h;

    /* Default planar: stride(0)=1, stride(1)=w, stride(2)=w*h
     * Each channel plane is contiguous in memory */
    float *r_in = &input(0, 0, 0);
    float *g_in = &input(0, 0, 1);
    float *b_in = &input(0, 0, 2);

    /* Output: same planar layout */
    Halide::Runtime::Buffer<float> output(w, h, 3);
    float *r_out = &output(0, 0, 0);
    float *g_out = &output(0, 0, 1);
    float *b_out = &output(0, 0, 2);

    /* sRGB -> OkLab (planar API)
     * Halide strides are elements; alwan expects bytes */
    alwan_srgb_to_oklab_map_planar_ex(
        r_out, g_out, b_out, ALWAN_PIXEL_F32,
        r_in,  g_in,  b_in,  ALWAN_PIXEL_F32,
        n,
        sizeof(float),  /* contiguous within each plane */
        sizeof(float)
    );

    /* CVD simulation (deuteranopia, 80% severity) on linear RGB planes */
    Halide::Runtime::Buffer<float> cvd(w, h, 3);

    alwan_simulate_deuteranopia_map_planar(
        (alwan_scalar *)&cvd(0, 0, 0),
        (alwan_scalar *)&cvd(0, 0, 1),
        (alwan_scalar *)&cvd(0, 0, 2),
        (alwan_scalar const *)r_in,
        (alwan_scalar const *)g_in,
        (alwan_scalar const *)b_in,
        0.8,   /* severity */
        n, sizeof(float), sizeof(float)
    );

    Halide::Tools::save_image(cvd, "cvd_output.png");
    return 0;
}
```

---

### Complete Pipeline — ACES VFX Workflow

End-to-end: read EXR (ACEScg), grade with alwan, output through ACES view transform.

```cpp
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <alwan/alwan.h>
#include <cstdlib>
#include <cstring>

int main()
{
    /* -- Read EXR (interleaved) ------------------------------------ */
    Imf::InputFile file("shot.exr");
    Imath::Box2i dw = file.header().dataWindow();
    int w = dw.max.x - dw.min.x + 1;
    int h = dw.max.y - dw.min.y + 1;
    size_t n = (size_t)w * h;
    size_t stride = 3 * sizeof(alwan_scalar);

    alwan_scalar *pixels = (alwan_scalar *)malloc(n * stride);
    char *base = (char *)(pixels) - stride * (dw.min.x + (size_t)dw.min.y * w);

    Imf::FrameBuffer fb;
    fb.insert("R", Imf::Slice(Imf::FLOAT, base,                         stride, stride * w));
    fb.insert("G", Imf::Slice(Imf::FLOAT, base + sizeof(alwan_scalar),   stride, stride * w));
    fb.insert("B", Imf::Slice(Imf::FLOAT, base + 2*sizeof(alwan_scalar), stride, stride * w));
    file.setFrameBuffer(fb);
    file.readPixels(dw.min.y, dw.max.y);

    /* -- Create alwan context -------------------------------------- */
    alwan_ctx *ctx = alwan_create(NULL);

    /* -- White balance --------------------------------------------- */
    alwan_rgb wb = { 1.05, 1.0, 0.92 };
    alwan_white_balance_apply_map_interleave(
        pixels, pixels, &wb, n, stride, stride
    );

    /* -- Lift/Gamma/Gain color grade ------------------------------- */
    alwan_rgb lift  = { 0.01,  0.005, 0.02 };
    alwan_rgb gamma = { 0.98,  1.0,   1.02 };
    alwan_rgb gain  = { 1.1,   1.05,  0.95 };

    alwan_lgg_apply_map_interleave(
        pixels, pixels, &lift, &gamma, &gain, n, stride, stride
    );

    /* -- ACEScg -> ACES 2065-1 via matrix -------------------------- */
    alwan_rgb_space_desc acescg, aces2065;
    alwan_rgb_get_space_descriptor(&acescg,   ctx, ALWAN_RGB_SPACE_ACESCG);
    alwan_rgb_get_space_descriptor(&aces2065, ctx, ALWAN_RGB_SPACE_ACES2065_1);

    alwan_mat3x3 acescg_to_aces;
    alwan_mat3_mul(&acescg_to_aces, &aces2065.xyz_to_rgb, &acescg.rgb_to_xyz);
    alwan_mat3_transform_map_interleave(
        pixels, &acescg_to_aces, pixels, n, stride, stride
    );

    /* -- ACES view transform (RRT+ODT) ----------------------------- */
    alwan_view_transform_apply(
        pixels, ctx, ALWAN_VIEW_ACES_SDR, pixels, n, stride, stride
    );

    /* -- sRGB OETF for display output ------------------------------ */
    alwan_oetf_apply(
        pixels, ALWAN_TF_SRGB, pixels,
        n * 3, sizeof(alwan_scalar), sizeof(alwan_scalar)
    );

    /* pixels[] is now display-referred sRGB */

    free(pixels);
    alwan_destroy(ctx);
    return 0;
}
```

---

### Collect/Scatter — Custom Format Pipelines

Use `alwan_collect3` and `alwan_scatter3` to bridge typed pixel formats in multi-step pipelines.

```c
#include <alwan/alwan.h>
#include <stdlib.h>

/* Pipeline: u8 sRGB input -> Lab analysis -> gamut map -> u16 sRGB output */
void process_pipeline(void *out_u16, const void *in_u8, size_t count)
{
    size_t s = 3 * sizeof(alwan_scalar);
    alwan_scalar *rgb = malloc(count * s);
    alwan_scalar *lab = malloc(count * s);
    alwan_xyz white = { 0.95047, 1.0, 1.08883 };

    /* 1. Collect u8 -> alwan_scalar */
    alwan_collect3(rgb, in_u8, ALWAN_PIXEL_U8, count, 3, s);

    /* 2. sRGB EOTF (decode gamma) */
    alwan_eotf_apply(rgb, ALWAN_TF_SRGB, rgb,
                     count * 3, sizeof(alwan_scalar), sizeof(alwan_scalar));

    /* 3. Linear RGB -> XYZ -> Lab */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    alwan_mat3x3 npm = {{
    #include "data/matrices/aces_rec709_to_xyz.csv"
    }};
    ALWAN_DIAG_POP
    alwan_mat3_transform_map_interleave(rgb, &npm, rgb, count, s, s);
    alwan_xyz_to_lab_map_interleave(lab, rgb, &white, count, s, s);

    /* 4. Lab -> XYZ -> linear RGB -> gamut clip */
    alwan_lab_to_xyz_map_interleave(rgb, lab, &white, count, s, s);
    alwan_mat3x3 npm_inv;
    alwan_mat3_inv(&npm_inv, &npm);
    alwan_mat3_transform_map_interleave(rgb, &npm_inv, rgb, count, s, s);
    alwan_gamut_map_interleave(rgb, ALWAN_GAMUT_MAP_CLIP, rgb, count, s, s);

    /* 5. sRGB OETF (encode gamma) */
    alwan_oetf_apply(rgb, ALWAN_TF_SRGB, rgb,
                     count * 3, sizeof(alwan_scalar), sizeof(alwan_scalar));

    /* 6. Scatter alwan_scalar -> u16 */
    alwan_scatter3(out_u16, ALWAN_PIXEL_U16, rgb, count, s, 6);

    free(lab);
    free(rgb);
}
```

---

## See Also

- [API Reference](api/) — Detailed function documentation
- [Map API](api/map.md) — Batch processing and stride details
- [Getting Started](getting-started.md) — Basic usage guide
- [Configuration](configuration.md) — Build options
- [Library Memory Layouts](lib_mem.md) — Per-library pixel layout reference
- [Precision & Limits](precision-and-limits.md) — Numerical details
