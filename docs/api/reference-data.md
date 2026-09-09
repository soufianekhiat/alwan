# Reference Data & Color Systems API

Functions for accessing standard color reference datasets and color notation systems.

---

## Overview

Reference data functions provide access to:

- **Munsell Renotation Data**: Munsell HVC to/from XYZ
- **Color Checker**: Standard color target patch data
- **NCS**: Natural Color System notation (approximate, forward only)
- **RGB Space Introspection**: Query primaries and transfer functions by enum
- **Illuminant Utilities**: White points, xy chromaticities, and D-series generation
- **Embedded Dataset Getters**: Raw illuminant-xy and sRGB-primaries arrays
- **Interpolation & LUTs**: 1D/3D table lookup and (extra)interpolation helpers

All numeric reference data is **CSV-embedded** at compile time (`ALWAN_EMBED_DATA=1`,
the only supported mode). The datasets live under `src/alwan/data/**` and are baked
into static C arrays by `src/alwan/api/alwan_data.c` (illuminants, primaries, matrices)
and `src/alwan/api/alwan_reference_data.c` (Munsell, NCS, ColorChecker). There is no
runtime/`/data/` loader.

---

## Precision Variants

Every function on this page that carries color data exists in two explicit precision
variants. `{T}` below is a placeholder for one of:

| `{T}` | Scalar type   | Color types                                      | Gate              |
|-------|---------------|--------------------------------------------------|-------------------|
| `f32` | `alwan_f32`   | `alwan_xyz_f32`, `alwan_vec2_f32`, `alwan_rgb_f32`, `alwan_spd_f32` | `ALWAN_WITH_F32` |
| `f64` | `alwan_f64`   | `alwan_xyz_f64`, `alwan_vec2_f64`, `alwan_rgb_f64`, `alwan_spd_f64` | `ALWAN_WITH_F64` |

By default **both** precisions compile. Declarations are always present.

> **f64-internal facades (by design).** The reference-data lookups on this page,
> namely Munsell (`alwan_munsell_to_xyz` / `alwan_xyz_to_munsell`), NCS
> (`alwan_ncs_to_xyz`), ColorChecker (`alwan_color_checker_data`), and RGB-space
> introspection (`alwan_rgb_space_by_enum` / `alwan_rgb_space_get_tfs`), store
> their renotation/patch/primaries tables in `double`. The `_f32` twins are **not**
> independent native-f32 paths: they run the f64 data + f64 lookup and narrow the
> result at the boundary. There is **no** `#if` precision gating in
> `alwan_reference_data.c`; the f32 accessors call straight through to the f64
> implementation. This is intentional (the table data is f64, and re-quantising it
> to f32 tables would only add error) and matches the f64-internal facade pattern
> already documented for ZCAM and the CCM fits (see
> [precision-and-limits.md](../precision-and-limits.md)). A consequence is that
> these `_f32` entry points stay available even in an `ALWAN_BUILD_ONLY_F32` build
> (gated by `ALWAN_WITH_F64_FACADE`, always `1`), rather than failing at link time.

For the colour-data accessors **not** in that list, a single-precision build
(`ALWAN_BUILD_ONLY_F32` / `ALWAN_BUILD_ONLY_F64`) defines only the matching twin
and calling the excluded precision fails at **link** time.

A few helpers are precision-independent and have **no** `_{T}` suffix
(e.g. `alwan_color_checker_num_patches`).

Examples below use `_f64`; replace with `_f32` for single precision.

---

## Munsell Color System

### alwan_munsell_to_xyz_{T}

```c
int alwan_munsell_to_xyz_f64(alwan_xyz_f64 *xyz,
                             alwan_f64 hue, alwan_f64 value, alwan_f64 chroma,
                             alwan_illuminant illuminant);
int alwan_munsell_to_xyz_f32(alwan_xyz_f32 *xyz,
                             alwan_f32 hue, alwan_f32 value, alwan_f32 chroma,
                             alwan_illuminant illuminant);
```

Convert Munsell notation (Hue, Value, Chroma) to XYZ using the Munsell Renotation Data (1943).

**Parameters:**
- `hue`: Munsell hue [0, 100] (continuous: 0=R, 10=YR, 20=Y, ..., 90=RP)
- `value`: Munsell value [0, 10] (lightness)
- `chroma`: Munsell chroma [0, 20+] (saturation)
- `illuminant`: Illuminant for XYZ calculation

### alwan_xyz_to_munsell_{T}

```c
int alwan_xyz_to_munsell_f64(alwan_f64 *hue, alwan_f64 *value, alwan_f64 *chroma,
                             alwan_xyz_f64 const *xyz, alwan_illuminant illuminant);
int alwan_xyz_to_munsell_f32(alwan_f32 *hue, alwan_f32 *value, alwan_f32 *chroma,
                             alwan_xyz_f32 const *xyz, alwan_illuminant illuminant);
```

Convert XYZ to Munsell notation (inverse lookup).

---

## Color Checker Targets

### alwan_color_checker_data_{T}

```c
int alwan_color_checker_data_f64(alwan_xyz_f64 *xyz,
                                 alwan_colorchecker_type type,
                                 alwan_illuminant illuminant,
                                 size_t patch_index);
int alwan_color_checker_data_f32(alwan_xyz_f32 *xyz,
                                 alwan_colorchecker_type type,
                                 alwan_illuminant illuminant,
                                 size_t patch_index);
```

Get XYZ tristimulus values for a specific Color Checker patch.

### alwan_color_checker_num_patches

```c
size_t alwan_color_checker_num_patches(alwan_colorchecker_type type);
```

Get the number of patches in a Color Checker target. Precision-independent
(no `_{T}` suffix). Returns 0 on error.

**Target types:**
```c
typedef enum {
    ALWAN_COLORCHECKER_CLASSIC = 0,      /* ColorChecker Classic 24-patch */
    ALWAN_COLORCHECKER_SG,                /* ColorChecker SG 140-patch */
    ALWAN_COLORCHECKER_DIGITAL_SG,        /* ColorChecker Digital SG */
    ALWAN_BABELCOLOR_AVERAGE,             /* BabelColor Average */
    ALWAN_BABELCOLOR_HCT                  /* BabelColor HCT */
} alwan_colorchecker_type;
```

**Example:**
```c
/* Iterate all patches of a ColorChecker Classic */
size_t n = alwan_color_checker_num_patches(ALWAN_COLORCHECKER_CLASSIC);
for (size_t i = 0; i < n; i++) {
    alwan_xyz_f64 xyz;
    alwan_color_checker_data_f64(&xyz, ALWAN_COLORCHECKER_CLASSIC,
                                 ALWAN_ILLUMINANT_D65, i);
    printf("Patch %zu: X=%.3f Y=%.3f Z=%.3f\n", i, xyz.x, xyz.y, xyz.z);
}
```

---

## Natural Color System (NCS)

### alwan_ncs_to_xyz_{T}

```c
int alwan_ncs_to_xyz_f64(alwan_xyz_f64 *xyz, char const *ncs_notation);
int alwan_ncs_to_xyz_f32(alwan_xyz_f32 *xyz, char const *ncs_notation);
```

Convert an NCS notation string to XYZ. Example notation: `"S 1050-Y90R"`.
Output XYZ is on the Y = 0-100 scale, D65.

> **Approximate.** Uses published elementary-hue chromaticities (Hard & Sivik 1981)
> with linear hue interpolation; it does **not** reproduce the proprietary NCS atlas.

### alwan_xyz_to_ncs_{T}

```c
int alwan_xyz_to_ncs_f64(char *ncs_notation, size_t notation_size,
                         alwan_xyz_f64 const *xyz);
int alwan_xyz_to_ncs_f32(char *ncs_notation, size_t notation_size,
                         alwan_xyz_f32 const *xyz);
```

> **Inverse unsupported.** These always return `ALWAN_E_INVALID`: recovering NCS
> notation requires the proprietary NCS colour atlas. Reserve `notation_size >= 32`
> for any future support.

---

## RGB Space Introspection

### alwan_rgb_space_by_enum_{T}

```c
int alwan_rgb_space_by_enum_f64(alwan_f64 primaries[6],
                                alwan_vec2_f64 *white_point,
                                alwan_rgb_space space);
int alwan_rgb_space_by_enum_f32(alwan_f32 primaries[6],
                                alwan_vec2_f32 *white_point,
                                alwan_rgb_space space);
```

Get RGB primaries (rx, ry, gx, gy, bx, by) and white-point xy by enum.
Does not require a context. Returns `ALWAN_E_INVALID` if `space` is invalid.

### alwan_rgb_space_get_tfs_{T}

```c
int alwan_rgb_space_get_tfs_f64(alwan_transfer_function *oetf,
                                alwan_transfer_function *eotf,
                                alwan_rgb_space space);
int alwan_rgb_space_get_tfs_f32(alwan_transfer_function *oetf,
                                alwan_transfer_function *eotf,
                                alwan_rgb_space space);
```

Get the OETF and EOTF associated with an RGB color space enum. The `oetf`/`eotf`
outputs are precision-independent enums; the `_{T}` suffix matches the calling
convention only.

**Example:**
```c
alwan_f64 primaries[6];
alwan_vec2_f64 white;
alwan_transfer_function oetf, eotf;

alwan_rgb_space_by_enum_f64(primaries, &white, ALWAN_RGB_SPACE_DISPLAY_P3);
alwan_rgb_space_get_tfs_f64(&oetf, &eotf, ALWAN_RGB_SPACE_DISPLAY_P3);

printf("White: (%.4f, %.4f)\n", white.x, white.y);
printf("OETF: %d, EOTF: %d\n", oetf, eotf);
```

---

## Illuminant White Points

### alwan_illuminant_white_point_{T}

```c
int alwan_illuminant_white_point_f64(alwan_xyz_f64 *out_xyz,
                                     alwan_illuminant illuminant,
                                     alwan_observer_type observer);
int alwan_illuminant_white_point_f32(alwan_xyz_f32 *out_xyz,
                                     alwan_illuminant illuminant,
                                     alwan_observer_type observer);
```

Get the XYZ white point for a standard illuminant, normalized to Y = 1.0.
Returns `ALWAN_E_INVALID` if the illuminant is not supported.

**Example:**
```c
alwan_xyz_f64 d65_white;
alwan_illuminant_white_point_f64(&d65_white, ALWAN_ILLUMINANT_D65,
                                 ALWAN_OBSERVER_CIE_1931_2DEG);
/* d65_white ~= {0.9505, 1.0000, 1.0890} */
```

---

## Embedded Dataset Getters

These return a pointer into the **embedded** static dataset plus its element `count`.
In embedded mode (the only supported mode) the data is owned by the library; do not
free it. (`alwan_data_free_{T}` is declared only for the unimplemented runtime mode,
reserved for a future release.)

### alwan_data_get_illuminant_xy_{T}

```c
int alwan_data_get_illuminant_xy_f64(alwan_f64 **data, size_t *count,
                                     alwan_illuminant illuminant, alwan_ctx *ctx);
int alwan_data_get_illuminant_xy_f32(alwan_f32 **data, size_t *count,
                                     alwan_illuminant illuminant, alwan_ctx *ctx);
```

Enum-based illuminant xy chromaticity accessor. Returns 2 values (x, y).
Returns `ALWAN_E_INVALID` if the illuminant is not supported or has no xy data.

### alwan_data_get_srgb_primaries_{T}

```c
int alwan_data_get_srgb_primaries_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
int alwan_data_get_srgb_primaries_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);
```

Get the sRGB primaries as 6 values: rx, ry, gx, gy, bx, by.
---

## Per-Illuminant xy Getters

Twenty-nine per-illuminant accessors are defined in `alwan_data.c`; twenty-six are
exported. Each hands back a pointer to a 2-element static table holding `x` then `y`,
CIE 1931 2-degree. There is no observer parameter anywhere in this family.

Nine of the twenty-nine carry `_f32` / `_f64` twins (`{T}` = `f32` | `f64`). The other
twenty are `f64`-only and have no `_{T}` suffix: seventeen are exported (see below), and
`_f2`, `_f7`, `_f11` are defined with external linkage but appear in no header and in no
export list.

These do not return a spectrum. `alwan_data_get_illuminant_d65_{T}` returns two
chromaticity numbers and sets `*count = 2`. The 471-sample SPD (360-830 nm at 1 nm) comes
from `alwan_spd_illuminant_{T}`, documented in [spectral.md](spectral.md).

The returned pointer has had `const` cast off read-only storage. The tables are declared
`static alwan_f64 const` / `static alwan_f32 const`; the getter casts to `alwan_{T} *` to
satisfy the out-parameter type. Writing through the pointer is undefined behaviour and
faults on any platform that maps `.rodata` read-only. Treat the result as read-only
regardless of the type the API hands you. The same cast applies to
`alwan_data_get_srgb_primaries_{T}`.

> **Passing `NULL` crashes, and the enum dispatcher is not the safe route.** No getter in
> this family checks `data` or `count`. After `(void)ctx;` the very next two statements
> dereference both out-parameters, and every one of them returns `ALWAN_OK`
> unconditionally. `alwan_data_get_illuminant_xy_{T}`, declared above, behaves the same
> way: `_f64` delegates to the un-checking per-illuminant getters and `_f32` inlines the
> same stores, so both dereference `data` and `count` on every non-`default` branch. What
> the dispatcher adds is a `default: return ALWAN_E_INVALID` arm for an illuminant with no
> xy row, and nothing else. `alwan_color_checker_grid`,
> `alwan_color_checker_reflectance_{T}` and `alwan_color_checker_native_illuminant` do
> null-check their outputs.

`ctx` is discarded by every one of the twenty-nine (`(void)ctx;` on the first line).
Nothing is allocated, so nothing is freed. `NULL` is a correct argument and is what the
library itself passes: `alwan_illuminant_white_point_f64` calls the xy dispatcher with
`NULL`. The dispatcher itself does not discard `ctx`; it forwards it to whichever
per-illuminant getter it delegates to.

### alwan_data_get_illuminant_a_{T} through _e_{T} (nine declared)

```c
alwan_status alwan_data_get_illuminant_a_{T}(alwan_{T} **data, size_t *count, alwan_ctx *ctx);
```

Nine names are declared in `alwan.h`, all with that shape: `_a`, `_b`, `_c`, `_d50`,
`_d55`, `_d60`, `_d65`, `_d75`, `_e`.

**Parameters:**
- `data`: receives a pointer into embedded static storage, 2 elements, `x` then `y`.
  Never `NULL`-checked. Do not free. Do not write through it.
- `count`: receives `2`, for every illuminant in this family. Never `NULL`-checked.
- `ctx`: discarded. Pass `NULL`.

**Returns:** `ALWAN_OK` (0), always. These have no failure path.

| Getter | x | y |
|--------|---|---|
| `_a` (incandescent tungsten) | 0.44758 | 0.40745 |
| `_b` (direct sunlight) | 0.34842 | 0.35161 |
| `_c` (average daylight) | 0.31006 | 0.31616 |
| `_d50` (horizon daylight) | 0.3457 | 0.3585 |
| `_d55` (mid-morning daylight) | 0.33243 | 0.34744 |
| `_d60` (CIE daylight locus) | 0.321616709705268 | 0.337619916550817 |
| `_d65` (noon daylight) | 0.3127 | 0.3290 |
| `_d75` (daylight 7500 K) | 0.29903 | 0.31488 |
| `_e` (equal energy) | 1/3 | 1/3 |

A, B, C, D50, D55, D65 and D75 carry the published rounded values (4 decimals for D50 and
D65, 5 for the other five). So D65 is 0.3127 / 0.3290, not the ASTM E308 recomputation
0.312727 / 0.329023, and D50 is 0.3457 / 0.3585, not the ICC derived white. D60 (with
D40, D45 and D93 below) is carried to full double precision from the daylight-locus
formula.

The `f32` twin is fed by the same CSV as the `f64` twin (`alwan_data.c:29-32`), so the
`f32` value is the same decimal literal converted by the compiler. There is no separate
`_f32` CSV.

The whole `f64` side of this family sits inside `#if ALWAN_WITH_F64_FACADE`, which
`alwan_build_config.h:72` defines unconditionally to `1`, so it is present in every build.

> **`ALWAN_ILLUMINANT_D60` is the CIE locus D60, not the ACES white point.** The table
> holds 0.321616709705268 / 0.337619916550817. ACES rounds its white to 0.32168 / 0.33767,
> a fifth-decimal difference. For ACES work use `ALWAN_ACES_WHITE_x` /
> `ALWAN_ACES_WHITE_y` from `alwan_platform.h`. The public header comments this getter
> only as `/* Illuminant D60 (daylight) */`; the warning lives in `alwan_platform.h:697`
> and never surfaces where the getter is declared.

### alwan_data_get_illuminant_d40 through _led_v2 (seventeen undeclared)

Seventeen more: `d40`, `d45`, `d93`, `hp1`-`hp5`, `led_b1`-`led_b5`, `led_bh1`,
`led_rgb1`, `led_v1`, `led_v2`.

> **Exported from the shared library, declared in no header.** These are defined in
> `alwan_data.c` and listed in `buildsystem/alwan_exports.def`, so they are part of the
> ABI, but `alwan.h` declares only the nine above. To call one you must write the
> prototype yourself, and it must match exactly:

```c
/* Write this yourself. Plain int, no _f64 suffix, no f32 twin. */
int alwan_data_get_illuminant_d40(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
```

They break three of the library's conventions. They return plain `int` instead of
`alwan_status`. They carry no `_f64` suffix even though they are `f64`-only. They have no
`f32` twin, so an `f32` caller reaches these values only through
`alwan_data_get_illuminant_xy_f32`.

The portable route to all of them is `alwan_data_get_illuminant_xy_{T}`, which switches on
the enum, exists in both precisions, and returns `alwan_status`. Its enum argument sits
third, between `count` and `ctx`, unlike the three-argument getters here.

| Getter | Enum | x | y |
|--------|------|---|---|
| `alwan_data_get_illuminant_d40` | `ALWAN_ILLUMINANT_D40` | 0.382343625 | 0.383766261015578 |
| `alwan_data_get_illuminant_d45` | `ALWAN_ILLUMINANT_D45` | 0.362088541838134 | 0.370869778684046 |
| `alwan_data_get_illuminant_d93` | `ALWAN_ILLUMINANT_D93` | 0.283145007105054 | 0.297112885245942 |
| `alwan_data_get_illuminant_hp1` | `ALWAN_ILLUMINANT_HP1` | 0.533 | 0.415 |
| `alwan_data_get_illuminant_hp2` | `ALWAN_ILLUMINANT_HP2` | 0.4778 | 0.4158 |
| `alwan_data_get_illuminant_hp3` | `ALWAN_ILLUMINANT_HP3` | 0.4302 | 0.4075 |
| `alwan_data_get_illuminant_hp4` | `ALWAN_ILLUMINANT_HP4` | 0.3812 | 0.3797 |
| `alwan_data_get_illuminant_hp5` | `ALWAN_ILLUMINANT_HP5` | 0.3776 | 0.3713 |
| `alwan_data_get_illuminant_led_b1` | `ALWAN_ILLUMINANT_LED_B1` | 0.456 | 0.4078 |
| `alwan_data_get_illuminant_led_b2` | `ALWAN_ILLUMINANT_LED_B2` | 0.4357 | 0.4012 |
| `alwan_data_get_illuminant_led_b3` | `ALWAN_ILLUMINANT_LED_B3` | 0.3756 | 0.3723 |
| `alwan_data_get_illuminant_led_b4` | `ALWAN_ILLUMINANT_LED_B4` | 0.3422 | 0.3502 |
| `alwan_data_get_illuminant_led_b5` | `ALWAN_ILLUMINANT_LED_B5` | 0.3118 | 0.3236 |
| `alwan_data_get_illuminant_led_bh1` | `ALWAN_ILLUMINANT_LED_BH1` | 0.4474 | 0.4066 |
| `alwan_data_get_illuminant_led_rgb1` | `ALWAN_ILLUMINANT_LED_RGB1` | 0.4557 | 0.4211 |
| `alwan_data_get_illuminant_led_v1` | `ALWAN_ILLUMINANT_LED_V1` | 0.4548 | 0.4044 |
| `alwan_data_get_illuminant_led_v2` | `ALWAN_ILLUMINANT_LED_V2` | 0.3781 | 0.3775 |

The symbol name and the CSV file name differ for the LED entries: the symbol is `led_b1`
(underscore), the file is `data/illuminants_xy/led-b1_xy.csv` (hyphen).

`alwan_data_get_illuminant_f2`, `_f7` and `_f11` are defined with external linkage in the
same file, but they are neither declared in `alwan.h` nor listed in `alwan_exports.def`.
They are unreachable from a DLL build and dead public symbols in a static one. Their data
is reachable only through `alwan_data_get_illuminant_xy_{T}`.

### xy coverage: 29 of 38 illuminants

`alwan_illuminant` has 38 enumerators (0 = `ALWAN_ILLUMINANT_A` through
37 = `ALWAN_ILLUMINANT_HP5`). The xy switch covers 29 of them.

- **Has xy:** A, B, C, D40, D45, D50, D55, D60, D65, D75, D93, E, F2, F7, F11,
  LED_B1-LED_B5, LED_BH1, LED_RGB1, LED_V1, LED_V2, HP1-HP5.
- **No xy (`ALWAN_E_INVALID`):** F1, F3, F4, F5, F6, F8, F9, F10, F12.

An illuminant with no xy still has data. `alwan_spd_illuminant_{T}` carries a full
471-sample SPD (360-830 nm at 1 nm) for all 38 values, F1 through F12 included.

The failure is observer-dependent and it propagates.
`alwan_illuminant_white_point_{T}` takes the xy path when
`observer == ALWAN_OBSERVER_CIE_1931_2DEG` and the SPD-integration path for every other
observer, so the nine fluorescents without xy return `ALWAN_E_INVALID` for the 2-degree
observer and succeed for a 10-degree one. Downstream,
`alwan_color_checker_data_{T}(&xyz, ALWAN_COLORCHECKER_CLASSIC, ALWAN_ILLUMINANT_F1, i)`
fails with `ALWAN_E_INVALID` (the tristimulus path needs a 2-degree white point to build
the adaptation), while
`alwan_color_checker_data_{T}(&xyz, ALWAN_COLORCHECKER_CLASSIC_OHTA, ALWAN_ILLUMINANT_F1, i)`
succeeds (the spectral path needs only the SPD). The same public call answers or refuses
depending on which target you asked about.

---

## Color Checker Layout, Spectra and Native Illuminant

Four more functions answer four different questions about a target documented under
[Color Checker Targets](#color-checker-targets) above: how many patches it has, what
illuminant its values are published under, what its reflectance spectra are, and how its
colour field is laid out.

### alwan_colorchecker_type

`alwan_colorchecker_type` has 13 enumerators. The block earlier on this page shows the
first 5.

```c
typedef enum {
    ALWAN_COLORCHECKER_CLASSIC = 0,
    ALWAN_COLORCHECKER_SG = 1,
    ALWAN_COLORCHECKER_DIGITAL_SG = 2,   /* same physical target as SG */
    ALWAN_BABELCOLOR_AVERAGE = 3,
    ALWAN_BABELCOLOR_HCT = 4,            /* no measurement data */
    ALWAN_COLORCHECKER_CLASSIC_1976 = 5, /* published under Illuminant C */
    ALWAN_COLORCHECKER_CLASSIC_PRE2014 = 6,
    ALWAN_COLORCHECKER_CLASSIC_POST2014 = 7,
    ALWAN_COLORCHECKER_SG_PRE2014 = 8,
    ALWAN_TE226_V2 = 9,                   /* published under D65 */
    ALWAN_COLORCHECKER_CLASSIC_OHTA = 10, /* spectral, 380-780 nm at 5 nm */
    ALWAN_COLORCHECKER_PMC = 11,          /* spectral, 400-700 nm at 10 nm */
    ALWAN_IT8_7_2 = 12                    /* layout only, no colorimetry */
} alwan_colorchecker_type;
```

### Per-target counts

| Type | `num_patches` | `num_reflectances` | `num_patch_names` | grid |
|------|---------------|--------------------|-------------------|------|
| `CLASSIC` | 24 | 0 | 24 | 6 x 4 |
| `SG`, `DIGITAL_SG`, `SG_PRE2014` | 140 | 0 | 140 | 14 x 10 |
| `BABELCOLOR_AVERAGE` | 24 | 24 | 24 | 6 x 4 |
| `BABELCOLOR_HCT` | 0 | 0 | 24 | `ALWAN_E_NODATA` |
| `CLASSIC_1976`, `_PRE2014`, `_POST2014` | 24 | 0 | 24 | 6 x 4 |
| `TE226_V2` | 45 | 0 | 45 | `ALWAN_E_NODATA` |
| `CLASSIC_OHTA` | 24 (via spectra) | 24 | 24 | 6 x 4 |
| `PMC` | 30 (via spectra) | 30 | 30 | `ALWAN_E_NODATA` |
| `IT8_7_2` | 0 | 0 | 288 | 22 x 12 |

`alwan_color_checker_num_patches` counts what alwan can hand you and falls back to the
reflectance count when there is no tristimulus table, which is how `CLASSIC_OHTA` reports
24 and `PMC` reports 30. `alwan_color_checker_num_patch_names` counts the target's
published layout, which alwan carries even where it carries no colorimetry.

`alwan_color_checker_num_patches`, `_num_reflectances` and `_num_patch_names` all return 0
for both "valid type, no data" and "not a type at all". The two cases are
indistinguishable from the return value.

### alwan_color_checker_native_illuminant

```c
alwan_status alwan_color_checker_native_illuminant(alwan_illuminant *illuminant,
                                                   alwan_colorchecker_type type);
```

Writes the illuminant the target's tristimulus table is published under, which is what
`alwan_color_checker_data_{T}` adapts away from. Precision-independent.

- `ALWAN_ILLUMINANT_D50`: `CLASSIC`, `SG`, `DIGITAL_SG`, `BABELCOLOR_AVERAGE`,
  `CLASSIC_PRE2014`, `CLASSIC_POST2014`, `SG_PRE2014`
- `ALWAN_ILLUMINANT_C`: `CLASSIC_1976`
- `ALWAN_ILLUMINANT_D65`: `TE226_V2`

**Returns:**
- `ALWAN_OK` (0) on success.
- `ALWAN_E_INVALID` (-1) when `illuminant` is `NULL`, or when `type > ALWAN_IT8_7_2`.
- `ALWAN_E_NODATA` (-2) when `type <= ALWAN_IT8_7_2` and there is no tristimulus table.

`ALWAN_E_NODATA` here means the target has no tristimulus table. `CLASSIC_OHTA` and `PMC`
get it even though alwan carries full spectra for them,
`alwan_color_checker_num_patches` reports 24 and 30, and `alwan_color_checker_data_{T}`
answers for any illuminant you ask about: a spectral target has no single native
illuminant by construction. `BABELCOLOR_HCT` and `IT8_7_2` get the same code because alwan
has nothing for them.

### alwan_color_checker_reflectance_{T}

```c
alwan_status alwan_color_checker_reflectance_{T}(alwan_spd_{T} *out,
                                                 alwan_colorchecker_type type,
                                                 size_t patch_index, alwan_ctx *ctx);
```

Creates an SPD holding one patch's spectral reflectance factor on the grid it was measured
on. The caller destroys it with `alwan_spd_destroy_{T}`.

**Parameters:**
- `out`: receives a created SPD. Must not be `NULL`.
- `type`: only the three targets in the table below have spectra.
- `patch_index`: `[0, alwan_color_checker_num_reflectances(type) - 1]`. The range check is
  against `num_reflectances`, and index out of range is rejected with `ALWAN_E_RANGE`,
  never clamped. For `BABELCOLOR_AVERAGE` the reflectance and patch counts happen to agree
  at 24; the check is on the spectral count either way.
- `ctx`: threaded into `alwan_spd_create_{T}`, which discards it. `ALWAN_ALLOC` is a
  compile-time macro from `alwan_config.h`, so there is no per-context allocator.
  Pass `NULL`.

| Type | Range | Bands | Step |
|------|-------|-------|------|
| `ALWAN_COLORCHECKER_CLASSIC_OHTA` | 380-780 nm | 81 | 5 nm |
| `ALWAN_BABELCOLOR_AVERAGE` | 380-730 nm | 36 | 10 nm |
| `ALWAN_COLORCHECKER_PMC` | 400-700 nm | 31 | 10 nm |

The grid is fixed per target and cannot be requested. Values are a reflectance factor in
roughly [0, 1]. They are not percentages; the regression suite bounds them at [0.0, 1.5].

**Returns:**
- `ALWAN_OK` (0) on success.
- `ALWAN_E_INVALID` (-1) when `out` is `NULL`, or when `type > ALWAN_IT8_7_2`.
- `ALWAN_E_NODATA` (-2) when `type <= ALWAN_IT8_7_2` and alwan has no spectra for it.
- `ALWAN_E_RANGE` (-3) when `patch_index >= alwan_color_checker_num_reflectances(type)`.
- `ALWAN_E_NOMEM` (-4) propagated from `alwan_spd_create_{T}`.

The `_f32` form is a facade over `_f64` and allocates twice: it builds the complete `f64`
SPD, creates a second `f32` SPD, narrows element by element, then destroys the `f64` one.
Both live unguarded in the same translation unit (`alwan_reference_data.c` contains no
preprocessor conditionals), so the `f32` form carries no extra build-config requirement.
`alwan_color_checker_data_f32` is a thinner facade of the same kind: it computes in `f64`
and narrows three scalars.

### alwan_color_checker_num_reflectances

```c
size_t alwan_color_checker_num_reflectances(alwan_colorchecker_type type);
```

24 for `CLASSIC_OHTA`, 24 for `BABELCOLOR_AVERAGE`, 30 for `PMC`, 0 for everything else
including out-of-enum values. Cannot fail. Precision-independent.

`BABELCOLOR_AVERAGE` carries both forms: a D50 xyY table and a 24 x 36 reflectance table.
`alwan_color_checker_data_{T}` tests for the tristimulus table first, so for this one type
it always takes the Bradford adaptation path and never touches the spectra. To get the
spectral answer, call `alwan_color_checker_reflectance_{T}` and drive
`alwan_xyz_from_spd_{T}` yourself.

### alwan_color_checker_patch_name / alwan_color_checker_num_patch_names

```c
char const *alwan_color_checker_patch_name(alwan_colorchecker_type type,
                                           size_t patch_index);
size_t alwan_color_checker_num_patch_names(alwan_colorchecker_type type);
```

`alwan_color_checker_patch_name` returns a static string in the order the data tables are
stored, or `NULL` when alwan does not know the target's layout or when
`patch_index >= alwan_color_checker_num_patch_names(type)`. Nothing is allocated. Both are
precision-independent.

Name tables:
- 24 Classic names ("dark skin", "light skin", ... "black 2 (1.5 D)"), shared by
  `CLASSIC`, `CLASSIC_1976`, `CLASSIC_PRE2014`, `CLASSIC_POST2014`, `CLASSIC_OHTA`,
  `BABELCOLOR_AVERAGE` and `BABELCOLOR_HCT`.
- 140 SG names, shared by `SG`, `DIGITAL_SG` and `SG_PRE2014`.
- 45 TE226 names.
- 30 PMC names.
- 288 IT8.7/2 names: the 22 x 12 field followed by the 24-step grey ramp `GS0`-`GS23`,
  22 * 12 + 24 = 288.

> **The letter means opposite things in the two lettered targets.** Both are stored
> row-major over their grid, but an SG's letter is its column and an IT8's letter is its
> row. SG storage order runs `A1, B1, ... N1, A2, ...`; IT8.7/2 storage order runs
> `A1..A22, B1..B22, ...`. Indexing an SG as if it were an IT8 transposes the chart
> silently.

### alwan_color_checker_grid

```c
alwan_status alwan_color_checker_grid(int *columns, int *rows,
                                      alwan_colorchecker_type type);
```

The rectangular colour field of a target. Precision-independent.

- 6 x 4: `CLASSIC`, `CLASSIC_1976`, `CLASSIC_PRE2014`, `CLASSIC_POST2014`, `CLASSIC_OHTA`,
  `BABELCOLOR_AVERAGE`
- 14 x 10: `SG`, `DIGITAL_SG`, `SG_PRE2014`
- 22 x 12: `IT8_7_2` (its 24 GS greys sit outside the field and follow it in name order)

**Returns:**
- `ALWAN_OK` (0) with `*columns` and `*rows` written.
- `ALWAN_E_INVALID` (-1) when `columns` or `rows` is `NULL`. Nothing is written.
- `ALWAN_E_NODATA` (-2) for every other type, including `TE226_V2` (45 patches, data
  present), `PMC` (30 patches, spectra present), `BABELCOLOR_HCT`, and any value outside
  the enum. The `ALWAN_E_NODATA` path still writes both out-parameters, storing
  `*columns = 0` and `*rows = 0`, so a caller that ignores the status ends up with a 0 x 0
  grid instead of keeping its previous values.

### Interaction with alwan_color_checker_data_{T}

The header for `alwan_color_checker_data_{T}` documents `ALWAN_OK` and `ALWAN_E_INVALID`.
Five error codes actually come back:

- `ALWAN_E_RANGE` (-3): `patch_index` past the end, on either branch.
- `ALWAN_E_NODATA` (-2): a known type with neither table nor spectra (`BABELCOLOR_HCT`,
  `IT8_7_2`).
- `ALWAN_E_INVALID` (-1): `xyz` is `NULL`, `type > ALWAN_IT8_7_2`, a table row with
  `y <= 0`, or a failed white-point lookup.
- `ALWAN_E_DIVZERO` (-5): the spectral branch, when the perfect diffuser integrates to
  `Y <= 0` under the requested light.
- `ALWAN_E_NOMEM` (-4): propagated from the SPD allocations on the spectral branch.

XYZ comes back on a Y = 1 scale. A white patch lands near Y = 0.91 (the Classic dark-skin
patch is Y = 0.1008). The neighbouring `alwan_ncs_to_xyz_{T}` is documented as Y = 0-100
instead. The spectral branch normalises by a perfect diffuser under the same light, so
both branches land on the same scale.

The adaptation is hardcoded Bradford between two white points rebuilt from the rounded xy
tables above. Both are built with `ALWAN_OBSERVER_CIE_1931_2DEG` and normalised to Y = 1,
so the D50 source white is 0.964296 / 1 / 0.825105 against ICC's 0.96420 / 1 / 0.82491.
When the requested illuminant equals the target's native illuminant, no adaptation runs
and the stored xyY converts straight through. Ask for the native illuminant (via
`alwan_color_checker_native_illuminant`) when you want the published numbers untouched.

The spectral branch is fixed at 2-degree / Simpson / no bandpass correction.
`alwan_color_checker_data_{T}` has no observer parameter. It hardcodes
`ALWAN_OBSERVER_CIE_1931_2DEG`, `ALWAN_INTEGRATE_SIMPSON` and `bandpass_nm = 0`, which
skips the Stearns & Stearns correction even though the Ohta grid is 5 nm and the PMC and
BabelColor grids are 10 nm, the sampling the correction exists for. The header's claim
that spectral targets "answer for any illuminant and observer" holds only if you take the
reflectance SPD and call `alwan_xyz_from_spd_{T}` yourself.

The integral runs on the reflectance grid. `alwan_xyz_from_spd_{T}` resamples the CMFs
onto the reflectance SPD's range with `ALWAN_EXTRAPOLATE_CONSTANT`, then samples the
illuminant only at that grid's wavelengths. A PMC patch is integrated over 400-700 nm, and
the illuminant's power outside that window never enters the sum. Ohta gets 380-780 nm,
BabelColor 380-730 nm. The `ALWAN_EXTRAPOLATE_ZERO` mode the illuminant lookup passes is
inert for these three targets, since `alwan_spd_illuminant_{T}` always spans 360-830 nm,
which contains all three grids.

Three functions in this cluster choose between `ALWAN_E_NODATA` and `ALWAN_E_INVALID` with
the test `type <= ALWAN_IT8_7_2`: `alwan_color_checker_native_illuminant`,
`alwan_color_checker_reflectance_{T}` and `alwan_color_checker_data_{T}`.
`alwan_colorchecker_type` has only non-negative enumerators, so a compiler may give it an
unsigned underlying type: passing `(alwan_colorchecker_type)-1` yields `ALWAN_E_INVALID`
under one compiler and `ALWAN_E_NODATA` under another. Do not depend on which code an
out-of-range type produces. `alwan_color_checker_grid` is the exception: its `default:`
arm returns `ALWAN_E_NODATA` for any type it does not recognise, regardless of the enum's
underlying signedness.


---

## Interpolation & Table Lookup Utilities

### alwan_interpolate_{T}

```c
int alwan_interpolate_f64(alwan_f64 const *x_in, alwan_f64 const *y_in, size_t count_in,
                          alwan_f64 const *x_out, alwan_f64 *y_out, size_t count_out,
                          alwan_interp_method method);
int alwan_interpolate_f32(alwan_f32 const *x_in, alwan_f32 const *y_in, size_t count_in,
                          alwan_f32 const *x_out, alwan_f32 *y_out, size_t count_out,
                          alwan_interp_method method);
```

Interpolate data points using the specified method (`x_in` must be sorted ascending).

**Methods:**
```c
typedef enum {
    ALWAN_INTERP_LINEAR = 0,     /* Linear */
    ALWAN_INTERP_CUBIC,           /* Cubic */
    ALWAN_INTERP_LANCZOS,         /* Lanczos windowed sinc */
    ALWAN_INTERP_SPRAGUE,         /* Sprague 5th order */
    ALWAN_INTERP_LAGRANGE,        /* Lagrange polynomial */
    ALWAN_INTERP_AKIMA            /* Akima spline (non-overshooting) */
} alwan_interp_method;
```

### alwan_extrapolate_{T}

```c
int alwan_extrapolate_f64(alwan_f64 const *x_in, alwan_f64 const *y_in, size_t count_in,
                          alwan_f64 const *x_out, alwan_f64 *y_out, size_t count_out,
                          alwan_extrap_method method);
int alwan_extrapolate_f32(alwan_f32 const *x_in, alwan_f32 const *y_in, size_t count_in,
                          alwan_f32 const *x_out, alwan_f32 *y_out, size_t count_out,
                          alwan_extrap_method method);
```

**Methods:**
```c
typedef enum {
    ALWAN_EXTRAP_CONSTANT = 0,    /* Use boundary value */
    ALWAN_EXTRAP_LINEAR,          /* Linear extrapolation */
    ALWAN_EXTRAP_POLYNOMIAL,      /* Polynomial extrapolation */
    ALWAN_EXTRAP_EXPONENTIAL,     /* Exponential decay */
    ALWAN_EXTRAP_REFLECT,         /* Reflective boundary */
    ALWAN_EXTRAP_NATURAL          /* Natural neighbor */
} alwan_extrap_method;
```

### Table Interpolation

```c
/* 1D table interpolation (returns the interpolated value) */
alwan_f64 alwan_table_interp_1d_f64(alwan_f64 const *table, size_t size,
                                    alwan_f64 x, alwan_interp_method method);

/* 3D trilinear interpolation */
int alwan_table_interp_3d_trilinear_f64(alwan_rgb_f64 *rgb_out,
                                        alwan_f64 const *table, size_t const sizes[3],
                                        alwan_rgb_f64 const *rgb_in);

/* 3D tetrahedral interpolation (more accurate for color transforms) */
int alwan_table_interp_3d_tetrahedral_f64(alwan_rgb_f64 *rgb_out,
                                          alwan_f64 const *table, size_t const sizes[3],
                                          alwan_rgb_f64 const *rgb_in);
```

(`_f32` twins exist for all three.)

**Parameters for 3D LUT:**
- `table`: 3D LUT array (R-major: `table[r][g][b]`, 3 values per entry)
- `sizes`: Dimensions [size_r, size_g, size_b]
- `rgb_in`: Input RGB coordinates [0, 1] (normalized)

---

## Tristimulus Optimization

### alwan_optimize_spectrum_for_xyz_{T}

```c
int alwan_optimize_spectrum_for_xyz_f64(alwan_spd_f64 *spd_out,
                                        alwan_xyz_f64 const *target_xyz,
                                        alwan_observer_type observer,
                                        alwan_ctx *ctx);
int alwan_optimize_spectrum_for_xyz_f32(alwan_spd_f32 *spd_out,
                                        alwan_xyz_f32 const *target_xyz,
                                        alwan_observer_type observer,
                                        alwan_ctx *ctx);
```

Find a spectral power distribution that matches target XYZ tristimulus values
(`spd_out` must be pre-allocated with the desired wavelength range; `ctx` is last).
Due to metamerism, multiple SPDs can match the same XYZ; this finds one valid solution.

---

## Hero Wavelength Spectral Sampling

### alwan_hero_wavelength_sample_{T}

```c
int alwan_hero_wavelength_sample_f64(alwan_f64 *lambda_out, alwan_f64 u);
int alwan_hero_wavelength_sample_f32(alwan_f32 *lambda_out, alwan_f32 u);
```

Sample a hero wavelength from uniform variable `u` in [0,1], mapped to [380, 780] nm.

### alwan_hero_wavelength_to_xyz_{T}

```c
void alwan_hero_wavelength_to_xyz_f64(alwan_xyz_f64 *xyz_out, alwan_f64 lambda);
void alwan_hero_wavelength_to_xyz_f32(alwan_xyz_f32 *xyz_out, alwan_f32 lambda);
```

Convert a single wavelength to XYZ via the Wyman 2013 analytic CMF fit. Returns `void`.

### alwan_hero_wavelength_batch_{T}

```c
int alwan_hero_wavelength_batch_f64(alwan_f64 *lambda_out, alwan_xyz_f64 *xyz_weights,
                                    size_t count, alwan_f64 seed);
int alwan_hero_wavelength_batch_f32(alwan_f32 *lambda_out, alwan_xyz_f32 *xyz_weights,
                                    size_t count, alwan_f32 seed);
```

Stratified batch sampling: generates `count` wavelengths from `seed`. `xyz_weights`
receives per-sample XYZ importance weights and may be `NULL`.

---

## Error Codes

- `ALWAN_OK` (0): Success
- `ALWAN_E_INVALID` (-1): Invalid parameter or notation (also the NCS inverse contract)
- `ALWAN_E_NODATA` (-2): Reference data not found

---

## Measured Charts

The `alwan_color_checker_*` functions above answer for a target as a **product**, from values
alwan embeds. The `alwan_chart_*` functions answer for a target as an **object**, from the
measurement file that shipped with it.

That split follows the targets themselves. A ColorChecker has published values because every
one is meant to be the same chart. A professional target does not: an IT8 or a DT NGT2 is
measured per sheet or per batch, two off the same press differ, and a chart fades. ISO 12641
fixes an IT8's layout and leaves its colorimetry to the manufacturer, which is why
`ALWAN_IT8_7_2` carries 288 patch names and no numbers. The numbers live in a file, and this is
how they get in.

alwan reads OpenQualia's Measurement File Standard, which is CGATS.17-2009 with a fixed set of
header keys, carried as `.oqm.txt` beside the target or downloaded from the QR label on it. The
same parser reads a plain CGATS batch reference file, since it is the same structure.

```c
alwan_chart_f64 *chart = NULL;
if (alwan_chart_load_f64(&chart, "DT-AR-2023088.oqm.txt", ctx) == ALWAN_OK) {
    for (size_t i = 0; i < alwan_chart_num_patches_f64(chart); i++) {
        alwan_xyz_f64 xyz;
        alwan_chart_xyz_f64(&xyz, chart, i);
        /* alwan_chart_patch_name_f64(chart, i) names it */
    }
    alwan_chart_destroy_f64(chart, ctx);
}
```

**Column families**, in the order the reader prefers them. `XYZ_X` / `XYZ_Y` / `XYZ_Z` are used
as written, rescaled from the standard's Y = 100 to alwan's Y = 1. Failing those, `LAB_L` /
`LAB_A` / `LAB_B` are converted under the file's own illuminant. Failing those, reflectance
columns spelled `SPEC_560`, `SPECTRAL_NM560` or `nm560` are integrated against the file's
`ILLUMINANT` and `OBSERVER` and normalised so a perfect diffuser reads Y = 1, which is the
convention `alwan_color_checker_data_{T}` uses for its own spectral targets. A file carrying
both keeps its spectra: `alwan_chart_reflectance_{T}` still returns them, so you can integrate
under a different light. `alwan_chart_get_source_{T}` reports which family was used.

Reflectance may be written as `[0, 1]` or as percent, and the standard marks neither, so the
reader decides per row on the magnitude.

**Serial lookup is the application's job**, and deliberately so: alwan does no network access.
`SERIAL` is a header key like any other, so the pieces for the workflow are all here. Read the
QR code on the target, scan a directory of measurement files, compare
`alwan_chart_header_{T}(chart, "SERIAL")`, and on a miss send the user to the vendor's
measurement page to download it.

**Functions.** `alwan_chart_load_{T}` and `alwan_chart_load_buffer_{T}` read a file or bytes
you already hold; the buffer form does no I/O and neither takes ownership of nor writes to what
it is given. `alwan_chart_destroy_{T}` releases the chart. `alwan_chart_xyz_{T}`,
`alwan_chart_patch_name_{T}`, `alwan_chart_num_patches_{T}`,
`alwan_chart_native_illuminant_{T}`, `alwan_chart_reflectance_{T}` and
`alwan_chart_num_bands_{T}` read it. `alwan_chart_header_{T}`, `alwan_chart_num_headers_{T}` and
`alwan_chart_header_at_{T}` reach the header keys. `alwan_chart_write_{T}` and
`alwan_chart_write_buffer_{T}` write the chart back out as OQM; the buffer form reports the
length it needs when handed a `NULL` buffer, and returns `ALWAN_E_RANGE` rather than truncating.

**Errors.** `ALWAN_E_INVALID` for an unreadable or malformed file: no `DATA_FORMAT` block, no
`DATA` block, a `NUMBER_OF_SETS` that disagrees with the row count, a row whose width does not
match the format, or a cell that is not a number. `ALWAN_E_NODATA` for a well-formed file that
carries no colorimetry alwan can use.

---

## See Also

- [Spectral Operations](spectral.md): SPD creation and XYZ integration
- [Color Correction](color-correction.md): Camera profiling with ColorChecker
- [Color Spaces](color-spaces.md): RGB space conversions
