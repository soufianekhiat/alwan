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
`src/alwan/api/alwan_munsell.c` (Munsell) and `src/alwan/api/alwan_reference_data.c`
(NCS, ColorChecker). There is no
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
alwan_status alwan_munsell_to_xyz_f64(alwan_xyz_f64 *xyz,
                                      alwan_f64 hue, alwan_f64 value, alwan_f64 chroma,
                                      alwan_illuminant illuminant);
alwan_status alwan_munsell_to_xyz_f32(alwan_xyz_f32 *xyz,
                                      alwan_f32 hue, alwan_f32 value, alwan_f32 chroma,
                                      alwan_illuminant illuminant);
```

Convert a Munsell specification to XYZ from the Munsell Renotation Data (Newhall,
Nickerson and Judd 1943, the "all" data). This is colour-science's
`munsell_specification_to_xyY` ported: the value by the ASTM D1535 quintic, the
chromaticity interpolated between the bounding renotation hues and chromas, linearly or
radially as colour's decision table says. It matches colour to 7.8e-16 (suite 259).

**Parameters:**
- `hue`: the hue number [0, 100) around the circle: R = 0, YR = 10, Y = 20, GY = 30,
  G = 40, BG = 50, B = 60, PB = 70, P = 80, RP = 90. 5R is 5, 2.5PB is 72.5, and 10R is
  the same hue as 0YR, 10. Ignored when `chroma` is 0.
- `value`: Munsell value [0, 10]. A chromatic colour needs a value of at least 1, where the
  renotation data stops.
- `chroma`: Munsell chroma, 0 or more, within the renotation data for that hue and value
- `illuminant`: the renotation is under illuminant C; any other illuminant adapts the
  result to it by Bradford

The result has Y on a 0..1 scale: N5 gives Y = 0.1927, N10 gives 1.

**Returns:** `ALWAN_OK`; `ALWAN_E_INVALID` on a null pointer; `ALWAN_E_RANGE` where colour
refuses: a specification outside the renotation data (5R 5/40, say), a value outside
[0, 10], a negative chroma or a NaN.

### alwan_xyz_to_munsell_{T}

```c
alwan_status alwan_xyz_to_munsell_f64(alwan_f64 *hue, alwan_f64 *value, alwan_f64 *chroma,
                                      alwan_xyz_f64 const *xyz, alwan_illuminant illuminant);
alwan_status alwan_xyz_to_munsell_f32(alwan_f32 *hue, alwan_f32 *value, alwan_f32 *chroma,
                                      alwan_xyz_f32 const *xyz, alwan_illuminant illuminant);
```

Convert XYZ (Y on 0..1, under `illuminant`) to a Munsell specification, colour-science's
`xyY_to_munsell_specification` ported, with its iteration on hue and chroma converging to
1e-7 in xy. One step differs: alwan's value is the exact inverse of the ASTM D1535
quintic, where colour interpolates a 0.001 table of it, and the two differ by up to 2.1e-7.
With that step replaced in colour, the two agree to 1.2e-13 in hue, 1.8e-15 in value and
1.4e-14 in chroma (suite 259).

`hue` comes back as the hue number in (0, 100], the forward's convention with 10RP
written as 100. A colour within 1e-3 of illuminant C in xy is neutral: hue 0, chroma 0.
Returns `ALWAN_E_RANGE` where colour refuses (a colour outside the renotation data, a
value below 1 off the neutral axis, or no convergence).

---

## Color Checker Targets

### alwan_color_checker_data_{T}

```c
alwan_status alwan_color_checker_data_f64(alwan_xyz_f64 *xyz,
                                          alwan_colorchecker_type type,
                                          alwan_illuminant illuminant,
                                          size_t patch_index);
alwan_status alwan_color_checker_data_f32(alwan_xyz_f32 *xyz,
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
alwan_status alwan_ncs_to_xyz_f64(alwan_xyz_f64 *xyz, char const *ncs_notation);
alwan_status alwan_ncs_to_xyz_f32(alwan_xyz_f32 *xyz, char const *ncs_notation);
```

Convert an NCS notation string to XYZ. Example notation: `"S 1050-Y90R"`.
Output XYZ is on the Y = 0-100 scale, D65.

> **Approximate.** Uses published elementary-hue chromaticities (Hard & Sivik 1981)
> with linear hue interpolation; it does **not** reproduce the proprietary NCS atlas.
> There is no external reference to test it against: its tests pin the approximation
> as documented (Y = (1 - s)^2 x 100, the elementary hues at full chromaticness mixed
> toward D65) and the rejection of malformed notations.

### alwan_xyz_to_ncs_{T}

```c
alwan_status alwan_xyz_to_ncs_f64(char *ncs_notation, size_t notation_size,
                                  alwan_xyz_f64 const *xyz);
alwan_status alwan_xyz_to_ncs_f32(char *ncs_notation, size_t notation_size,
                                  alwan_xyz_f32 const *xyz);
```

> **Inverse unsupported.** These always return `ALWAN_E_INVALID`: recovering NCS
> notation requires the proprietary NCS colour atlas. Reserve `notation_size >= 32`
> for any future support.

---

## RGB Space Introspection

### alwan_rgb_space_by_enum_{T}

```c
alwan_status alwan_rgb_space_by_enum_f64(alwan_f64 primaries[6],
                                         alwan_vec2_f64 *white_point,
                                         alwan_rgb_space space);
alwan_status alwan_rgb_space_by_enum_f32(alwan_f32 primaries[6],
                                         alwan_vec2_f32 *white_point,
                                         alwan_rgb_space space);
```

Get RGB primaries (rx, ry, gx, gy, bx, by) and white-point xy by enum.
Does not require a context. Returns `ALWAN_E_INVALID` if `space` is invalid.

### alwan_rgb_space_get_tfs_{T}

```c
alwan_status alwan_rgb_space_get_tfs_f64(alwan_transfer_function *oetf,
                                         alwan_transfer_function *eotf,
                                         alwan_rgb_space space);
alwan_status alwan_rgb_space_get_tfs_f32(alwan_transfer_function *oetf,
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
alwan_status alwan_illuminant_white_point_f64(alwan_xyz_f64 *out_xyz,
                                              alwan_illuminant illuminant,
                                              alwan_observer_type observer);
alwan_status alwan_illuminant_white_point_f32(alwan_xyz_f32 *out_xyz,
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
alwan_status alwan_data_get_illuminant_xy_f64(alwan_f64 **data, size_t *count,
                                              alwan_illuminant illuminant, alwan_ctx *ctx);
alwan_status alwan_data_get_illuminant_xy_f32(alwan_f32 **data, size_t *count,
                                              alwan_illuminant illuminant, alwan_ctx *ctx);
```

Enum-based illuminant xy chromaticity accessor. Returns 2 values (x, y).
Returns `ALWAN_E_INVALID` if the illuminant is not supported or has no xy data.

### alwan_data_get_srgb_primaries_{T}

```c
alwan_status alwan_data_get_srgb_primaries_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
alwan_status alwan_data_get_srgb_primaries_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);
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

### xy coverage: 29 of 111 illuminants

`alwan_illuminant` has 111 enumerators (0 = `ALWAN_ILLUMINANT_A` through
110 = `ALWAN_ILLUMINANT_LS_KINOTON_75P`). The xy switch covers 29 of them.

- **Has xy:** A, B, C, D40, D45, D50, D55, D60, D65, D75, D93, E, F2, F7, F11,
  LED_B1-LED_B5, LED_BH1, LED_RGB1, LED_V1, LED_V2, HP1-HP5.
- **No xy:** F1, F3, F4, F5, F6, F8, F9, F10, F12, F3_1-F3_15, ID50, ID65, and
  all 56 `LS_` measured light sources.

`alwan_data_get_illuminant_xy_{T}` returns `ALWAN_E_INVALID` for the other 82. That is
the accessor's own answer, and it is narrow: it means there is no pinned chromaticity
pair, not that the illuminant is unknown.

An illuminant with no xy still has data. `alwan_spd_illuminant_{T}` carries a full
471-sample SPD (360-830 nm at 1 nm) for all 111 values.

`alwan_illuminant_white_point_{T}` reads the xy pair when
`observer == ALWAN_OBSERVER_CIE_1931_2DEG` and there is one, because two divisions beat
integrating 471 samples. When there is not, it integrates the SPD against the observer,
which is what it does for every other observer anyway. So the white point is available
for all 111 illuminants under all observers, and nothing downstream is
observer-dependent: both
`alwan_color_checker_data_{T}(&xyz, ALWAN_COLORCHECKER_CLASSIC, ALWAN_ILLUMINANT_F1, i)`
and its `_OHTA` twin answer.

Before 3.0.0 the 2-degree branch returned `ALWAN_E_INVALID` rather than falling through,
so those 82 illuminants failed under the most common observer while working under every
other one. Suite 153 now holds all 111 white points to a reference.

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
alwan_status alwan_interpolate_f64(alwan_f64 const *x_in, alwan_f64 const *y_in, size_t count_in,
                                   alwan_f64 const *x_out, alwan_f64 *y_out, size_t count_out,
                                   alwan_interp_method method);
alwan_status alwan_interpolate_f32(alwan_f32 const *x_in, alwan_f32 const *y_in, size_t count_in,
                                   alwan_f32 const *x_out, alwan_f32 *y_out, size_t count_out,
                                   alwan_interp_method method);
```

Interpolate data points using the specified method (`x_in` must be sorted ascending).

Outside the range of `x_in` the result is **clamped to the end value**, not extrapolated.
scipy's interpolators continue their polynomial instead, so the two agree inside the data
and diverge outside it by design. Use `alwan_extrapolate_{T}` when you want the other
behaviour.

**Methods:**
```c
typedef enum {
    ALWAN_INTERP_LINEAR = 0,     /* Linear */
    ALWAN_INTERP_CUBIC,           /* Catmull-Rom, four points, smooth but overshoots */
    ALWAN_INTERP_LANCZOS,         /* Lanczos windowed sinc */
    ALWAN_INTERP_SPRAGUE,         /* Sprague 5th order, colour-science's */
    ALWAN_INTERP_LAGRANGE,        /* Four-point Lagrange */
    ALWAN_INTERP_AKIMA,           /* Akima, scipy's Akima1DInterpolator */
    ALWAN_INTERP_PCHIP,           /* Fritsch-Carlson monotone cubic */
    ALWAN_INTERP_MAKIMA,          /* modified Akima, scipy's method="makima" */
    ALWAN_INTERP_NATURAL_SPLINE,  /* C2 spline, zero second derivative at the ends */
    ALWAN_INTERP_CLAMPED_SPLINE,  /* C2 spline, zero first derivative at the ends */
    ALWAN_INTERP_NEAREST,         /* the nearest sample, a midpoint to the lower */
    ALWAN_INTERP_PREVIOUS         /* the last sample at or before x: a step */
} alwan_interp_method;
```

The five added on 2026-10-02 match scipy (suite 296): `MAKIMA` is
`Akima1DInterpolator(method="makima")` to 4.6e-15, where Akima's weights gain half the
absolute sum of the two secants, so a flat run beside a slope keeps its flatness without the
plain Akima's ringing on repeated values. `NATURAL_SPLINE` and `CLAMPED_SPLINE` are
`CubicSpline(bc_type="natural")` and `bc_type="clamped"`, to 1.6e-14 and 7.6e-13 (the latter
on three samples whose flat ends swing the cubic to 480: rounding, amplified). Being global,
the two splines solve for every node at once and allocate twice `count_in` values from the
default allocator; the readers that pass a window of samples (the refractive-index tables,
the spectral film's CMFs) refuse them with `ALWAN_E_INVALID`, since a spline over a window is
not the spline over the table. `NEAREST` and `PREVIOUS` are `interp1d(kind="nearest")` and
`kind="previous"` exactly; a point half way between two samples takes the lower one, as
scipy rounds a midpoint down. `alwan_interpolate_cubic_spline_{T}` takes the new
`ALWAN_SPLINE_CLAMPED` boundary too.

`ALWAN_INTERP_CUBIC` is Catmull-Rom, a local four-point curve, not a cubic spline. It is
smooth and it overshoots: where the data turns, the interpolant swings past the samples.

`ALWAN_INTERP_PCHIP` reads the same four points and chooses the node derivatives so it
cannot overshoot, flattening where the data turns instead. On a reflectance, a density
curve or a transfer function that difference matters, because an overshoot puts values
outside a range the data never left. It matches scipy's `PchipInterpolator`, which is what
colour-science wraps, to 8.9e-16 including the endpoints.

`ALWAN_INTERP_SPRAGUE` is Sprague (1880), CIE 167's method for uniformly spaced spectra,
as colour-science's `SpragueInterpolator`: two points extrapolated at each end with
Sprague's coefficients, then the fifth-order polynomial on each interval. It assumes a
uniform grid, and below six samples falls back to Catmull-Rom. `ALWAN_INTERP_AKIMA`
matches scipy's `Akima1DInterpolator` (method "akima"), two secants extrapolated at each
end and the mean of the neighbouring secants where both of Akima's weights vanish.
`ALWAN_INTERP_LAGRANGE` is the cubic through the two samples on each side, shifted inward
at the ends. Suite 259 holds Sprague to colour and the rest to scipy (Catmull-Rom as a
`CubicHermiteSpline` with its tangents, Lagrange as a `BarycentricInterpolator` on the same
four points), all within 1e-13.

For a cubic spline, use `alwan_interpolate_cubic_spline_{T}` below. It is not an
`alwan_interp_method` because it is a different kind of method: every node slope depends
on every sample, so it needs a solve before any point can be evaluated, where everything
above reads a bounded stencil.

### alwan_interpolate_cubic_spline_{T}

```c
typedef enum {
    ALWAN_SPLINE_NOT_A_KNOT = 0,
    ALWAN_SPLINE_NATURAL = 1
} alwan_spline_boundary;

alwan_status alwan_interpolate_cubic_spline_f64(alwan_f64 const *x_in, alwan_f64 const *y_in, size_t count_in,
                                                alwan_f64 const *x_out, alwan_f64 *y_out, size_t count_out,
                                                alwan_spline_boundary boundary, alwan_ctx *ctx);
alwan_status alwan_interpolate_cubic_spline_f32(alwan_f32 const *x_in, alwan_f32 const *y_in, size_t count_in,
                                                alwan_f32 const *x_out, alwan_f32 *y_out, size_t count_out,
                                                alwan_spline_boundary boundary, alwan_ctx *ctx);
```

The C2 cubic spline through every sample. The node slopes come from one tridiagonal solve,
and each interval is then the cubic Hermite polynomial on its samples and slopes.

**Which boundary.** `ALWAN_SPLINE_NOT_A_KNOT` is what scipy's `CubicSpline` does by
default and what colour-science's `CubicSplineInterpolator` does: the third derivative is
continuous across the second and second-to-last samples. Use it to match either.
`ALWAN_SPLINE_NATURAL` sets the second derivative to zero at both ends. They are different
curves near the ends, 5e-2 to 0.12 apart on the noisy spectra measured, so a textbook
natural spline does not reproduce either reference. Two samples give the straight line
under both; three give the parabola under not-a-knot, as scipy does.

**Scratch.** The solve needs `2 * count_in` values of scratch, taken from `ctx`'s allocator,
or from the default allocator when `ctx` is NULL. `ALWAN_E_NOMEM` if that fails.

**Contract.** `x_in` strictly increasing: a repeated value, a descending pair or a NaN is
`ALWAN_E_INVALID`, as are NULL buffers, `count_in < 2`, `count_out == 0` and an unknown
boundary. Outside `[x_in[0], x_in[count_in - 1]]` the end sample is held, like
`alwan_interpolate_{T}`; scipy extrapolates the end cubic instead.

**Accuracy.** Against scipy (suite 154): 1.2e-14 relative worst on well-conditioned grids,
uniform, non-uniform and clustered, in f64, and 2e-5 in f32. On a deliberately
ill-conditioned grid, a 1e-6 interval among unit ones, the two differ by 6.4e-11, and there
alwan is the more accurate: against the same system solved at 50 digits, alwan is 4.8e-11
away and scipy 1.1e-10. The solve does not pivot. That was checked at spacing ratios up to
1e12, where the smallest pivot reaches 3e-13 and the result still tracks scipy's pivoted
solver, because a small pivot here always comes with the small interval that caused it.

### alwan_extrapolate_{T}

```c
alwan_status alwan_extrapolate_f64(alwan_f64 const *x_in, alwan_f64 const *y_in, size_t count_in,
                                   alwan_f64 const *x_out, alwan_f64 *y_out, size_t count_out,
                                   alwan_extrap_method method);
alwan_status alwan_extrapolate_f32(alwan_f32 const *x_in, alwan_f32 const *y_in, size_t count_in,
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
alwan_status alwan_table_interp_3d_trilinear_f64(alwan_rgb_f64 *rgb_out,
                                                 alwan_f64 const *table, size_t const sizes[3],
                                                 alwan_rgb_f64 const *rgb_in);

/* 3D tetrahedral interpolation (more accurate for color transforms) */
alwan_status alwan_table_interp_3d_tetrahedral_f64(alwan_rgb_f64 *rgb_out,
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
alwan_status alwan_optimize_spectrum_for_xyz_f64(alwan_spd_f64 *spd_out,
                                                 alwan_xyz_f64 const *target_xyz,
                                                 alwan_observer_type observer,
                                                 alwan_ctx *ctx);
alwan_status alwan_optimize_spectrum_for_xyz_f32(alwan_spd_f32 *spd_out,
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
alwan_status alwan_hero_wavelength_sample_f64(alwan_f64 *lambda_out, alwan_f64 u);
alwan_status alwan_hero_wavelength_sample_f32(alwan_f32 *lambda_out, alwan_f32 u);
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
alwan_status alwan_hero_wavelength_batch_f64(alwan_f64 *lambda_out, alwan_xyz_f64 *xyz_weights,
                                             size_t count, alwan_f64 seed);
alwan_status alwan_hero_wavelength_batch_f32(alwan_f32 *lambda_out, alwan_xyz_f32 *xyz_weights,
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

It also reads **CxF3** (ISO 17972-1:2015), which is what the spectrophotometer vendors write.
The loader looks at the first byte that is not whitespace: an XML buffer goes to the CxF reader
and everything else to the CGATS one, so there is nothing to select and no format argument. A
CxF loads into the same `alwan_chart_{T}`, which is the whole reason to read a second format:
`alwan_chart_xyz_{T}`, `alwan_chart_reflectance_{T}` and `alwan_cmyk_model_from_chart_{T}` do
not grow a variant, and code written against a CGATS file reads a CxF file unchanged. Bytes
that open with `<` but are not a CxF fall through to the CGATS reader rather than being claimed,
so the error you get describes what is actually wrong with them.

What is read from a CxF: each `Object` becomes a patch, named by its `Name`, else its `Id`, else
its position. `ReflectanceSpectrum`, `ColorCIELab`, `ColorCIEXYZ`, `ColorCMYK` and `ColorRGB`
carry the colorimetry and the device values, in the same order of preference the columns have
above. The `ColorSpecification` supplies the illuminant and the observer, and the wavelength
grid when the spectra do not state their own `StartWL` and `Increment`. `FileInformation`
becomes header keys, so `alwan_chart_header_{T}` answers for a CxF the way it does for a CGATS
file. Element names are matched on the local name, so `cc:`, `cxf:` and no prefix at all all
read. Two spectra of different lengths in one file is `ALWAN_E_INVALID`: `alwan_spd_{T}` is one
uniform grid, and resampling one onto the other would be a guess about which you meant.

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

**Device columns and the file's own Lab.** A characterisation data set pairs device values with
colorimetry. `CMYK_C` / `CMYK_M` / `CMYK_Y` / `CMYK_K` (percent) or `RGB_R` / `RGB_G` / `RGB_B`
(the file's own scale) are kept as written: `alwan_chart_device_model_{T}` says which,
`alwan_chart_device_values_{T}` returns four values per patch (three and a zero for RGB), and
both are `ALWAN_CHART_DEVICE_NONE` / `ALWAN_E_NODATA` for a file without them. When the file has
`LAB_*` columns, `alwan_chart_lab_{T}` returns them as written, beside the XYZ the reader derived.
The writer emits both families, so a write and read back keeps them.

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

## The Planckian Table and Ohno 2013

```c
typedef struct alwan_planckian_table_s alwan_planckian_table;

alwan_status alwan_planckian_table_create_{T}(alwan_planckian_table **out, alwan_observer_type observer,
                                              alwan_{T} start, alwan_{T} end, alwan_{T} spacing, alwan_ctx *ctx);
void   alwan_planckian_table_destroy(alwan_planckian_table *table, alwan_ctx *ctx);
size_t alwan_planckian_table_size(alwan_planckian_table const *table);

alwan_status alwan_uv_to_cct_ohno2013_{T}(alwan_{T} *cct_out, alwan_{T} *duv_out, alwan_vec2_{T} const *uv,
                                          alwan_planckian_table const *table);
```

`alwan_cct_to_uv_planck1900` sums Planck's law against the observer's colour matching
functions every time it is called, and Ohno's method wants a few thousand locus points for
a single query. So the points are taken once and kept: the table loads the CMFs one time
and integrates for each temperature. The temperatures rise geometrically from `start` to
`end`, the step easing off towards the top so the table stays dense where the locus turns,
which is how colour-science builds its own. Zero for any of `start`, `end` or `spacing`
takes Ohno's values, 1000 K, 100000 K and 1.001; `spacing` must be greater than 1.

`alwan_uv_to_cct_ohno2013_{T}` reads the table. The nearest entry and its two neighbours
give a triangle whose apex is the answer near the locus; from |Duv| = 0.002 outwards the
three distances are fitted with a parabola instead, which is where colour-science changes
over. Duv is signed, positive above the locus, per Ohno 2013 and ANSI C78.377, and
`duv_out` may be `NULL`. A point whose nearest entry is an end of the table is
`ALWAN_E_RANGE`, since neither solution has the neighbour it needs.

Against colour-science's own Ohno 2013, over six points on the locus and four off it
either side of the 0.002 switch, alwan agrees to 2.4e-09 K and 4.7e-13 in Duv (suite 133).

This is the exact solve, and it is the one to reach for when the number matters.
`alwan_cct_duv_optimize` remains what it has always been: a search over a closed-form
approximation of the locus, with a CCT residual of about 28 K on the locus that comes from
that approximation rather than from the minimiser.

---

## CMYK Printing Characterisations

```c
alwan_status alwan_cmyk_model_fogra39(alwan_cmyk_model **out, alwan_ctx *ctx);
alwan_status alwan_cmyk_model_from_chart_{T}(alwan_cmyk_model **out, alwan_chart_{T} const *chart, alwan_ctx *ctx);
void alwan_cmyk_model_destroy(alwan_cmyk_model *model, alwan_ctx *ctx);
alwan_status alwan_cmyk_to_lab_{T}(alwan_lab_{T} *lab_out, alwan_cmyk_{T} const *cmyk, alwan_cmyk_model const *model);
alwan_status alwan_lab_to_cmyk_{T}(alwan_cmyk_{T} *cmyk_out, alwan_{T} *delta_e_out, alwan_lab_{T} const *lab,
                                   alwan_{T} k, alwan_cmyk_model const *model);
```

Device CMYK means nothing colorimetric until a printing condition is attached. A model turns
CMYK in [0, 1] into CIELAB relative to the data's white, D50 for the ISO printing conditions,
through an ISO 12642-2 (IT8.7/4) characterisation data set.

The IT8.7/4 target lays out complete CMY cubes on six K planes: K = 0 on nine levels, 20 on six,
40 and 60 on five, 80 on four, 100 on two. The model interpolates Lab multilinearly inside a
plane's cube, taking cells as scipy's `RegularGridInterpolator` does, then linearly in K between
the two planes around the query. Patches the target repeats are averaged. It interpolates Lab
rather than XYZ because Lab measured better: on the 321 FOGRA39 patches the model does not use,
dE2000 is mean 0.13 and maximum 1.21 in Lab, 0.24 and 1.52 in XYZ (suite 137).

`alwan_cmyk_model_fogra39` builds from FOGRA39, which alwan embeds: Fogra's FOGRA39L data
(ISO 12647-2:2004/Amd 1, coated paper), byte for byte. Fogra's terms allow redistribution in
software when the data are unmodified and Fogra is named as the source, so the file goes in
as Fogra publishes it and alwan's own CGATS reader parses it at run time. The FOGRAxx name
identifies the data set only and implies no certification by Fogra.

`alwan_cmyk_model_from_chart_{T}` builds from any loaded IT8.7/4 set with CMYK columns, a
CGATS.21 CRPC file for instance, using its `LAB_*` columns when it has them and its XYZ
otherwise. It is `ALWAN_E_NODATA` for a chart without CMYK, or without every node of the six
cubes. CMYK outside [0, 1] is `ALWAN_E_INVALID`.

### The other way, Lab to CMYK

`alwan_lab_to_cmyk_{T}` asks the characterisation which colorants print closest to a Lab
value. There is no single answer: the same colour can be printed with more ink and less
black or the reverse, which is what grey component replacement trades. So the black is an
input, not something the call solves for. Fix `k` in [0, 1] and the search returns the C, M
and Y nearest the target by CIEDE2000, with that black carried through to `cmyk_out.k`.

`delta_e_out`, which may be `NULL`, is the difference the search could not close. Inside
the gamut it is nothing an instrument would read. Outside it, the CMYK is the closest the
press can reach and the difference says how far short it fell, which is what a caller needs
in order to decide whether to gamut map first. A colour the press cannot print is not an
error and is not quietly clamped.

The search scans the target's densest CMY cube, the nine levels of the K = 0 plane, then
walks downhill from the best several of those nodes on a step that halves. It starts from
several rather than one because well outside the gamut the difference has more than one
dip, and the deepest is not always the one under the best node. Every candidate is scored
by the forward model itself, so the two directions cannot drift apart.

Suite 142 holds it to scipy's own inversion of the same model, on real FOGRA39 patches,
colours sampled from the model, and three that no coated press can print: every colour
inside the gamut comes back within 0.0001 dE2000 of the target, and no target, in gamut or
out, is reached less closely than scipy reaches it.

```c
alwan_cmyk_model *fogra = NULL;
alwan_cmyk_model_fogra39(&fogra, ctx);

alwan_lab_f64 want = { 46.0, -5.0, -27.0 };        /* D50 */
alwan_cmyk_f64 ink;
alwan_f64 missed;
alwan_lab_to_cmyk_f64(&ink, &missed, &want, 0.2, fogra);   /* 20 % black */
if (missed > 2.0) {
    /* out of reach for this press: gamut map, or try another black */
}
alwan_cmyk_model_destroy(fogra, ctx);
```

---

## The other illuminant tables

```c
alwan_status alwan_data_get_illuminant_b_{T}(alwan_{T} **data, size_t *count, alwan_ctx *ctx);
alwan_status alwan_data_get_illuminant_c_{T}(alwan_{T} **data, size_t *count, alwan_ctx *ctx);
alwan_status alwan_data_get_illuminant_d50_{T}(alwan_{T} **data, size_t *count, alwan_ctx *ctx);
alwan_status alwan_data_get_illuminant_d55_{T}(alwan_{T} **data, size_t *count, alwan_ctx *ctx);
alwan_status alwan_data_get_illuminant_d60_{T}(alwan_{T} **data, size_t *count, alwan_ctx *ctx);
alwan_status alwan_data_get_illuminant_d75_{T}(alwan_{T} **data, size_t *count, alwan_ctx *ctx);
alwan_status alwan_data_get_illuminant_e_{T}(alwan_{T} **data, size_t *count, alwan_ctx *ctx);
```

The xy chromaticity of one illuminant, as `alwan_data_get_illuminant_xy_{T}`
above gives it for D65: B (direct sunlight), C (average daylight), D50, D55,
D60, D75 and E (equal energy). `data` receives a pointer to the embedded,
read-only two-value table (x then y) and `count` receives 2; the pointer is
not the caller's to free and the values are not the caller's to write. For the
illuminant's spectrum use `alwan_spd_illuminant_{T}`; these are the
chromaticities alone. A NULL `data` or `count` is `ALWAN_E_INVALID`, a check
every accessor on this page gained on 2026-09-22 (none had one).

---

## alwan_chart_write_cgats17_{T}, alwan_chart_write_cgats17_buffer_{T}

```c
alwan_status alwan_chart_write_cgats17_{T}(char const *path, alwan_chart_{T} const *chart);
alwan_status alwan_chart_write_cgats17_buffer_{T}(char *buf, size_t *bytes_written, size_t buf_size,
                                                  alwan_chart_{T} const *chart);
```

The chart written back out in the CGATS.17 dialect, to a path or into a
buffer. The two dialects `alwan_chart_write` and these produce differ in
exactly two places: the identifier on the first line, and how a reflectance
column names its wavelength (`SPECTRAL_NM560` against `SPEC_560`); the rest is
the same text. Spectra are written when the chart has them. The buffer form
reports the length it needs when called with a NULL buffer and returns
`ALWAN_E_RANGE` if the buffer cannot hold the result and its terminator.
Neither writer invents a header key the source chart did not carry, so a reload
through `alwan_chart_load` is an identity, and that round trip is the test.

---

## The CMYK inverse, cached

```c
alwan_status alwan_cmyk_inverse_create(alwan_cmyk_inverse **out, alwan_cmyk_model const *model,
                                       alwan_f64 k, int size, alwan_ctx *ctx);
void         alwan_cmyk_inverse_destroy(alwan_cmyk_inverse *inv, alwan_ctx *ctx);
size_t       alwan_cmyk_inverse_size(alwan_cmyk_inverse const *inv);
alwan_status alwan_cmyk_inverse_eval_{T}(alwan_cmyk_{T} *cmyk_out, alwan_{T} *delta_e_out,
                                         alwan_lab_{T} const *lab, alwan_cmyk_inverse const *inv);
alwan_status alwan_cmyk_inverse_map_interleave_{T}(alwan_{T} *cmyk_out, size_t out_stride,
                                                   alwan_{T} const *lab_in, size_t in_stride, size_t count,
                                                   alwan_{T} *worst_delta_e_out, alwan_cmyk_inverse const *inv);
```

The same inverse as `alwan_lab_to_cmyk_{T}`, cached, so CMYK can be a per-pixel
target. `alwan_lab_to_cmyk` scans a 729-node CMY cube and walks downhill from
the best eight of its nodes: 1.2 ms a call on the embedded FOGRA39, which makes
a 1920 x 1080 frame 42 minutes. `create` runs that exact search once per node
of a regular grid over the Lab the characterisation can reach at one fixed
black `k`; a query then interpolates the grid, scores the result and the eight
corner inks of the cell it landed in, and takes a short walk from the best: 25
us rather than 1.2 ms, the same frame 53 seconds. Held to the exact search over
729 in-gamut targets, the worst it falls behind is 0.0415 dE and the worst
single ink differs by 0.0010.

The build is `size^3` exact searches, about 5 seconds at size 17; `size` is 4
to 64, outside that `ALWAN_E_RANGE`. The black is fixed at build because it is
an input to the exact search: a colour can be printed with more ink and less
black or the reverse, and a cache spanning `k` would interpolate between two
ink-sharing decisions and print neither.

A Lab outside the box the characterisation reaches runs the exact search
instead and returns results bit-identical to calling it directly; the cache
holds no data out there and does not approximate where it has none. The cost
is real, so know which pixels pay it: on a photograph a third fall outside and
it converts at about 4x rather than 45x, and the reason is not saturated
colour: 29 per cent of that frame is below the printable black (L 22.89 at k =
0), 4.9 per cent above the ceiling, none outside on a or b. What a press
cannot hold in a photograph is the darkness. Flat saturated artwork is the
opposite case, and has a handful of distinct colours: convert its palette.

`delta_e_out` is measured, not interpolated: the inks are put back through the
forward model and compared to the target by CIEDE2000, so the number includes
the cache's own error. The map form reports the worst over the run.

Not testable against an external reference: no library publishes an
intermediate for this (Argyll and littleCMS bake a B2A table into an ICC
profile, the same idea in a different container). Suite 143 holds it to
alwan's own exact search, which is the function it exists to approximate.

---

## See Also

- [Spectral Operations](spectral.md): SPD creation and XYZ integration
- [Color Correction](color-correction.md): Camera profiling with ColorChecker
- [Color Spaces](color-spaces.md): RGB space conversions
