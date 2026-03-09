# Data Management

How Alwan handles reference datasets (CMFs, illuminants, RGB spaces).

> **/!\ IMPLEMENTATION STATUS:**
> **Runtime Mode (`ALWAN_EMBED_DATA=0`):** NOT YET IMPLEMENTED in v0.1-alpha
> **Embedded Mode (`ALWAN_EMBED_DATA=1`):** SUPPORTED (default)
>
> This document describes both modes for future reference. Currently, only embedded mode works.

---

## Overview

Alwan requires extensive reference data for color science operations:
- **Color Matching Functions (CMFs):** CIE 1931, 1964, 2012, Stockman & Sharpe
- **Standard Illuminants:** A, B, C, D-series, E, F-series, LED series
- **RGB Color Spaces:** sRGB, BT.709, BT.2020, Display P3, ACES, and 100+ others
- **Spectral Data:** Test color samples, camera sensitivities, etc.

Alwan provides two modes for managing this data:

1. **Embedded Mode (default):** Data compiled into binary
2. **Runtime Mode:** Data loaded from CSV files

---

## Data Storage Format

All data is stored as **C-parsable CSV files** with maximum-precision numeric literals:

```csv
0.950470000000000,1.000000000000000,1.088830000000000
```

**Benefits:**
- Human-readable and editable
- Version control friendly
- Works in both embedded and runtime modes
- Maximum precision preserved (15-17 decimal digits)
- No binary format parsing needed

**Filename convention:**
```
{identity}_{wavelength_range}_{sampling}.csv
```

**Examples:**
- `cie_1931_2deg_x_360_830_1nm.csv` — CIE 1931 2° X_bar CMF, 360-830nm, 1nm steps
- `D65_360_830_1nm.csv` — D65 illuminant SPD, 360-830nm, 1nm steps
- `srgb.csv` — sRGB color space descriptor

---

## Embedded Mode (Default)

### Configuration

```c
#define ALWAN_EMBED_DATA 1  // Default
```

### How It Works

CSV data is included directly in C source files:

```c
static const alwan_scalar cie_1931_xbar[] = {
    #include "data/cmf/cie_1931_2deg_x_360_830_1nm.csv"
};
```

The compiler processes this as:
```c
static const alwan_scalar cie_1931_xbar[] = {
    0.000129900000, 0.000145847000, 0.000163802100, ...
};
```

### Advantages

Yes **Zero runtime I/O**
- No file system access required
- Instant initialization (< 1 μs)
- Works in sandboxed environments

Yes **Deterministic**
- Data always available
- No missing file errors
- Consistent across deployments

Yes **Self-contained**
- Single binary deployment
- No external data dependencies
- Works on systems without file access

Yes **Fast**
- Data in read-only memory
- No parsing overhead
- Cache-friendly access patterns

### Disadvantages

No **Larger binary**
- Adds ~1-2 MB to executable size
- May increase load times slightly
- More memory mapped at startup

No **No runtime updates**
- Data changes require recompilation
- Cannot update tables without rebuild

### Usage

```c
// Just create context, data is already in binary
alwan_ctx *ctx = alwan_create(NULL);

// Data immediately available (output first)
alwan_rgb_convert(&output, ctx, "srgb", "bt2020", &input, ...);

alwan_destroy(ctx);
```

### Build Impact

| Configuration | Binary Size Increase |
|--------------|---------------------|
| All data | ~2 MB |
| Minimal (D65 + sRGB only) | ~100 KB |

---

## Runtime Mode

### Configuration

```c
#define ALWAN_EMBED_DATA 0
```

### How It Works

Data is loaded from CSV files during `alwan_create()`:

1. Open CSV file
2. Parse numeric values
3. Allocate memory
4. Store in context
5. Cache for future use

### Advantages

Yes **Smaller binary**
- Executable ~2 MB smaller
- Faster linking during development
- Less disk space for distribution

Yes **Updatable data**
- Can update tables without recompiling
- Easy to patch incorrect data
- Support for custom datasets

Yes **Flexible deployment**
- Can provide different data sets per platform
- Easy to test with modified reference data

### Disadvantages

No **Requires file I/O**
- Initialization takes 10-50 ms
- Can fail if files missing/corrupt
- Needs file system access

No **Deployment complexity**
- Must ship CSV files alongside binary
- Need to handle data directory paths
- Potential for version mismatches

### Usage

```c
// Specify data root directory
alwan_ctx *ctx = alwan_create(&(alwan_config){
    .runtime_data_root = "C:/ProgramData/MyApp/alwan/data"
});

if (!ctx) {
    fprintf(stderr, "Failed to load data\n");
    // Check path, file permissions, file integrity
    return -1;
}

// Data now loaded and cached (output first)
alwan_rgb_convert(&output, ctx, "srgb", "bt2020", &input, ...);

alwan_destroy(ctx);
```

### Data Directory Structure

```
data/
├── cmf/
│   ├── cie_1931_2deg_x_360_830_1nm.csv
│   ├── cie_1931_2deg_y_360_830_1nm.csv
│   ├── cie_1931_2deg_z_360_830_1nm.csv
│   ├── cie_1964_10deg_x_360_830_1nm.csv
│   └── ...
├── illuminants/
│   ├── A_360_830_1nm.csv
│   ├── D50_360_830_1nm.csv
│   ├── D65_360_830_1nm.csv
│   └── ...
├── rgb_spaces/
│   ├── srgb.csv
│   ├── bt2020.csv
│   ├── display_p3.csv
│   └── ...
└── spectral_basis/
    └── ...
```

### Error Handling

```c
alwan_ctx *ctx = alwan_create(&(alwan_config){
    .runtime_data_root = "./data"
});

if (!ctx) {
    // Possible errors:
    // - Directory does not exist
    // - Files missing or unreadable
    // - CSV parse error (corrupt file)
    // - Out of memory

    // Debugging:
    #ifdef _WIN32
    if (GetFileAttributes("./data") == INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "Data directory './data' not found\n");
    }
    #endif
}
```

---

## Comparison

| Aspect | Embedded | Runtime |
|--------|----------|---------|
| **Binary size** | +2 MB | Baseline |
| **Init time** | < 1 μs | 10-50 ms |
| **File dependencies** | None | CSV files |
| **Updatability** | Recompile | Replace files |
| **Error potential** | None | Missing files |
| **Sandboxing** | Works | May fail |
| **Memory usage** | Same | Same |

---

## Mode Selection Guide

### Choose Embedded Mode When:
- Yes Deployment simplicity is important
- Yes Binary size < 10 MB is acceptable
- Yes Data never needs updates
- Yes Running in sandboxed environment
- Yes Startup time is critical

### Choose Runtime Mode When:
- Yes Binary size must be minimal
- Yes Data may need updates
- Yes Development iteration speed matters
- Yes Supporting custom datasets
- Yes 50 ms init time is acceptable

---

## Data Contents

### Color Matching Functions (CMFs)

**Size:** ~500 KB embedded, 18 files

**Sets:**
- CIE 1931 2° (X_bar, Y_bar, Z_bar)
- CIE 1964 10° (X_bar_10, Y_bar_10, Z_bar_10)
- CIE 2012 2° (updated 1931)
- CIE 2012 10° (updated 1964)
- CIE 2015 2°/10° (final revision)
- Stockman & Sharpe 2° (cone fundamentals)

**Wavelength range:** 360-830 nm, 1 nm steps (471 samples each)

---

### Standard Illuminants

**Size:** ~300 KB embedded, 30+ files

**Series:**
- **A:** Tungsten (2856 K)
- **B, C:** Deprecated daylight simulators
- **D-series:** D40, D45, D50, D55, D60, D65, D75, D93
- **E:** Equal energy
- **F-series:** F1-F12 (fluorescent)
- **LED series:** LED-B1 to B5, LED-V1, V2, LED-RGB1, LED-BH1
- **HP-series:** HP1-HP5 (high-pressure discharge)

**Wavelength range:** 360-830 nm, 1 nm steps

---

### RGB Color Spaces

**Size:** ~100 KB embedded, 100+ descriptors

**Categories:**
- **Standard:** sRGB, Adobe RGB, ProPhoto RGB
- **Broadcast:** BT.601, BT.709, BT.2020, PAL/SECAM
- **Cinema:** DCI-P3, Display P3, ACES (AP0, AP1)
- **Camera:** Canon Log, Sony S-Gamut, ARRI Wide Gamut, RED Wide Gamut
- **Legacy:** Apple RGB, ColorMatch RGB, NTSC 1953, CIE RGB

Each descriptor includes:
- Primary chromaticities (R, G, B)
- White point chromaticity
- Transfer function reference
- Derivation metadata

---

### Additional Data

- **Spectral locus:** 360-830 nm boundary
- **Pointer gamut:** Real surface colors boundary
- **Color checker patches:** Classic, SG, etc.
- **Test color samples:** CIE TCS, CES, VS
- **Camera sensitivities:** Nikon, Canon, Sony, etc.

---

## Custom Data

### Adding Custom Illuminants

**1. Create CSV file:**
```csv
# File: custom_led_360_830_1nm.csv
# Relative SPD, arbitrary units
12.5,13.2,14.1,15.3,...
```

**2. Place in data directory:**
```
data/illuminants/custom_led_360_830_1nm.csv
```

**3. Use in runtime mode:**
```c
alwan_ctx *ctx = alwan_create(&(alwan_config){
    .runtime_data_root = "data"
});

// Access via API (if exposed)
alwan_spd custom_spd;
alwan_spd_illuminant(&custom_spd, ctx, ALWAN_ILLUMINANT_CUSTOM);  // Or load custom data
```

---

### Modifying Existing Data

**Embedded mode:**
1. Edit CSV file in source tree
2. Rebuild library

**Runtime mode:**
1. Edit CSV file in deployment directory
2. Restart application

---

## Data Validation

### CSV Format Requirements

**Valid:**
```csv
1.0,2.5,3.7
```

**Invalid:**
```csv
1.0, 2.5, 3.7    # Spaces not allowed
1.0;2.5;3.7      # Must use commas
"1.0","2.5"      # No quotes
```

### Precision

All values stored with maximum precision:
- Float: 9 decimal digits (FLT_DECIMAL_DIG)
- Double: 17 decimal digits (DBL_DECIMAL_DIG)

**Example:**
```csv
0.950470000000000,1.000000000000000,1.088830000000000
```

---

## Performance Implications

### Memory Usage

**Embedded mode:**
- Data in read-only `.rodata` section
- Shared across all processes (OS dependent)
- No runtime allocation for data

**Runtime mode:**
- Data allocated on heap during init
- Per-process memory cost
- ~200-400 KB per context

---

### Startup Time

**Embedded mode:**
```c
alwan_ctx *ctx = alwan_create(NULL);
// < 1 microsecond (allocation only)
```

**Runtime mode:**
```c
alwan_ctx *ctx = alwan_create(&config);
// 10-50 milliseconds (I/O + parsing + allocation)
```

---

### Access Speed

Both modes have identical runtime performance:
- Data accessed via same pointers
- No lookup overhead
- Cache behavior identical

---

## Migration Between Modes

### From Embedded to Runtime

1. Define `ALWAN_EMBED_DATA 0`
2. Rebuild library
3. Deploy CSV files
4. Update code to set `runtime_data_root`

### From Runtime to Embedded

1. Ensure all CSV files in source tree
2. Define `ALWAN_EMBED_DATA 1`
3. Rebuild library
4. Remove CSV files from deployment

---

## Best Practices

### For Production

**Embedded mode:**
- Simpler deployment
- No file permission issues
- No data version mismatches

### For Development

**Runtime mode:**
- Faster iteration (no recompile for data changes)
- Easier to test custom data
- Smaller link times

### Hybrid Approach

Use both modes in different build configurations:
```c
#ifdef DEVELOPMENT_BUILD
    #define ALWAN_EMBED_DATA 0  // Runtime mode for dev
#else
    #define ALWAN_EMBED_DATA 1  // Embedded for release
#endif
```

---

## See Also

- [Configuration](configuration.md) — Setting ALWAN_EMBED_DATA
- [Context Management](api/context.md) — Runtime data loading
- [Getting Started](getting-started.md) — Build instructions
