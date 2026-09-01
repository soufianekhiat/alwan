# Color Space Channel Ranges

Reference ranges for the public API.

This document is the human-readable companion to the finite range constants and
normalization macros in `src/alwan/alwan_platform.h`. When there is any doubt
about an exact bound or normalization formula, that header is the implementation
source of truth.

---

## Key Rules

The default convention is **`[0, 1]` for every bounded channel**. In the default
build (`ALWAN_NORMALIZE_RANGES=1`) the public C API rescales each bounded output
channel into `[0, 1]` and expects bounded inputs in `[0, 1]`.

The exceptions are the **opponent / chroma axes**: Lab / Hunter-Lab / ProLab
`a`, `b`; Luv `u`, `v`; the cylindrical chroma `C`; and Oklab `a`, `b`. These are
**mathematically unbounded** (the CIE formulas impose no limit, and saturated or
wide-gamut colours do exceed the usual range), even though encodings give
them a *conventional* span: CIE Lab `a`, `b` are stored as signed `[-128, 127]`
in 8-bit / ICC formats (so `/128` would map them to `~= ]-1, 1[`), and Oklab
`a`, `b` sit roughly in `[-0.4, 0.4]`. alwan leaves these axes in
their **native signed range** rather than rescaling against a conventional bound,
so out-of-gamut excursions past it are preserved exactly and reference
comparisons stay 1:1. Only `L` (`/100 -> [0, 1]`) and the channels with a true
fixed bound are normalized. Channels that *do* have a fixed bound are mapped into
`[0, 1]`: signed chroma and hue (degrees or radians).

YCoCg is the signed-chroma case: `Co`, `Cg` come out of the kernel in
`[-0.5, 0.5]` and the normalisation layer shifts them by `+0.5`. **Y'CbCr and
YcCbcCrc are not.** Their kernels already emit `Cb`, `Cr` in `[0, 1]` centred on
`0.5`, so normalising them is a no-op and `ALWAN_NORM_YCBCR` is deliberately
empty. Adding the shift there on top of the kernel's own centring offset chroma
by a full 1.0 in the shipped default build, which is what it did before this
was fixed. See `docs/alwan_decisions.md`.

1. Bounded channels are normalized to `[0, 1]` by the public API.
2. Unbounded channels stay in their native (often signed) range.
3. Video signal range is a separate concept from semantic channel range.
4. RGB-like values may still exceed `[0, 1]` in HDR or out-of-gamut workflows.

**Legend**

- `native` means the mathematical range used by the underlying formulas
- `public` means what the default C backend API exposes when `ALWAN_NORMALIZE_RANGES=1`
- `unbounded` means the channel is not normalized and has no fixed finite bound

---

## Range Normalization

On the C backend, `ALWAN_NORMALIZE_RANGES` defaults to `1`.

In that mode:

- bounded outputs are rescaled to `[0, 1]`
- bounded inputs are expected in `[0, 1]`
- unbounded channels are left untouched
- normalization happens at the public API boundary rather than inside the core formulas

Disable it if you want native mathematical ranges:

```c
#define ALWAN_NORMALIZE_RANGES 0
#include "alwan/alwan.h"
```

With normalization disabled, public APIs use the same native channel ranges
described in papers and specifications.

> **Compile-time, whole-program switch.** `ALWAN_NORMALIZE_RANGES` is baked into
> the library when *it* is compiled: the rescaling lives inside the compiled API
> functions. Defining it only in a consumer translation unit does **not** change a
> prebuilt `alwan` library; build the library and your own code with the same
> value.

### Why the tests and benchmarks disable it

The `alwan_dev` validation build (the unit tests, the benchmarks, and the
`alwan` library they link) compiles with `ALWAN_NORMALIZE_RANGES=0` so channels
come out in their **native mathematical ranges**. That is what the reference
libraries the suite compares against use (colour-science, OpenColorIO, ACES), so
disabling normalization makes the comparison apples-to-apples. For example
`alwan_xyz_to_lab_f64` then returns CIE `L ~= 53.24` (matching
`colour.XYZ_to_Lab`) instead of the normalized `~= 0.5324`. The switch is set in
`tests/CMakeLists.txt`, `bench/CMakeLists.txt`, and the Sharpmake `common.cs`.

---

## Quick Summary

| Family | Typical native convention | Public convention with normalization enabled |
|--------|----------------------------|----------------------------------------------|
| RGB / HSV / HSL / CMY-like spaces | already bounded near `[0, 1]` | unchanged unless a channel has a different native bound |
| CIE Lab / Luv cylindrical forms | `L` in `[0, 100]`, hue often in degrees | `L` and finite hue channels mapped to `[0, 1]` |
| Oklch / JzCzhz / IPTch / HCL | hue stored as radians | hue mapped to `[0, 1]` |
| CAM correlates | lightness in `[0, 100]`, hue in degrees, some `H` channels in `[0, 400]` | bounded lightness and hue-like channels mapped to `[0, 1]` |
| YCbCr / YcCbcCrc | luma bounded, chroma already centred on `0.5` in `[0, 1]` | unchanged; normalisation is a no-op |
| YCoCg | luma bounded, chroma signed in `[-0.5, 0.5]` | chroma shifted by `+0.5` into `[0, 1]` |

---

## Device-Dependent Spaces

| Space | Channel notes in native form | Public form with normalization enabled |
|-------|-------------------------------|----------------------------------------|
| `alwan_rgb_*` | `r`, `g`, `b` are usually discussed around `[0, 1]`, but may go negative or above `1` in real pipelines | unchanged; Alwan does not clamp HDR or gamut excursions |
| `alwan_hsv_*` | `h`, `s`, `v` in `[0, 1]` | unchanged |
| `alwan_hsl_*` | `h`, `s`, `l` in `[0, 1]` | unchanged |
| `alwan_hsp_*` | `h`, `s`, `p` in `[0, 1]` | unchanged |
| `alwan_hsplog_*` | `h`, `s`, `p` in `[0, 1]` | unchanged |
| `alwan_hsy_*` | `h`, `s`, `y` in `[0, 1]` | unchanged |
| `alwan_hwb_*` | `H`, `W`, `B` in `[0, 1]`; `W + B` may exceed `1` before normalization logic in the transform | unchanged |
| `alwan_cmy_*`, `alwan_cmyk_*` | bounded `[0, 1]` channels | unchanged |
| `alwan_ycbcr_*` | `Y` in `[0, 1]`, `Cb` / `Cr` centered around `0` with native `[-0.5, 0.5]` semantics | `Y` stays `[0, 1]`; `Cb` / `Cr` map to `[0, 1]` |
| `alwan_ycocg_*` | `Y` in `[0, 1]`, `Co` / `Cg` centered around `0` | `Y` stays `[0, 1]`; centered chroma maps to `[0, 1]` |
| `alwan_yccbccrc_*` | constant-luminance family with bounded luma and centered chroma | bounded channels exposed in `[0, 1]` |
| `alwan_prismatic_*` | `L`, `s`, `h` are bounded ratios, not angle-based hue/chroma channels | unchanged |

Notes:

- `HSV` / `HSL` hue is already normalized, so multiply by `360` if you want degrees.
- YCbCr-family spaces are about component coding rather than perceptual uniformity.
- For RGB-like spaces, `[0, 1]` is not a promise of clamping.

---

## CIE And Related Colorimetric Spaces

| Space | Native range shape | Public form with normalization enabled |
|-------|--------------------|----------------------------------------|
| `alwan_xyz_*` | tristimulus values, typically non-negative and unbounded above | unchanged |
| `alwan_xyy_*` | `x`, `y` bounded chromaticities, `Y` luminance-like and unbounded above | bounded chromaticities stay bounded; `Y` unchanged |
| `alwan_lab_*` | `L` in `[0, 100]`; `a` / `b` mathematically unbounded (8-bit / ICC convention `[-128, 127]`) | `L` maps to `[0, 1]`; `a` / `b` unchanged (native, not `/128`) |
| `alwan_luv_*` | `L` in `[0, 100]`; `u` / `v` mathematically unbounded | `L` maps to `[0, 1]`; `u` / `v` unchanged (native) |
| `alwan_lch_*` | `L` in `[0, 100]`, `C` unbounded, `h` in degrees `[0, 360)` | `L` and `h` map to `[0, 1]`; `C` unchanged |
| `alwan_lchuv_*` | `L` in `[0, 100]`, `C` unbounded, `h` in degrees `[0, 360)` | `L` and `h` map to `[0, 1]`; `C` unchanged |
| `alwan_ucs_*` | `U`, `V` bounded; `W` luminance-like | bounded coordinates remain bounded; `W` unchanged |
| `alwan_uvw_*` | CIE 1964: `W*` is a `[0, 100]` lightness factor (`25*Y^(1/3)-17`, like Lab `L*`); `U*` / `V*` are mathematically unbounded opponent axes | `W*` maps to `[0, 1]`; `U*` / `V*` unchanged (native) |
| `alwan_hunter_lab_*` | `L` bounded, `a` / `b` unbounded | `L` maps to `[0, 1]`; `a` / `b` unchanged |
| `alwan_din99_*` | `L99` bounded, opponent axes unbounded | `L99` maps to `[0, 1]`; opponent axes unchanged |
| `alwan_prolab_*` | `L` bounded, `a` / `b` unbounded | `L` maps to `[0, 1]`; `a` / `b` unchanged |
| `alwan_osa_ucs_*` | historical perceptual coordinates without a simple shared finite normalization story | treated as native-value channels |

Notes:

- `XYZ` and `xyY` remain colorimetric quantities rather than normalized UI sliders.
- `LCh` families use degree hue natively, unlike `Oklch` and some newer cylindrical spaces.

---

## Modern Perceptual And Specialized Spaces

| Space | Native range shape | Public form with normalization enabled |
|-------|--------------------|----------------------------------------|
| `alwan_oklab_*` | `L` lightness-like, native ~`[0, 1]`; `a` / `b` signed, native ~`[-0.4, 0.4]` | unchanged; Oklab has **no** normalization macro, so even `L` stays native (it is already ~`[0, 1]`) |
| `alwan_oklch_*` | `L` near `[0, 1]`, `C` unbounded, `h` in radians `[-pi, pi]` | `h` maps to `[0, 1]`; `L` remains `[0, 1]` |
| `alwan_jzazbz_*` | `Jz` lightness-like (native ~`[0, 0.17]`), `Az` / `Bz` signed | unchanged; no normalization macro; `Jz` stays native |
| `alwan_jzczhz_*` | `Jz` native ~`[0, 0.17]`, `Cz` unbounded, `hz` in radians `[-pi, pi]` | only `hz` maps to `[0, 1]`; `Jz` and `Cz` unchanged |
| `alwan_ictcp_*` | intensity-like `I`, centered opponent `Ct` / `Cp` | unchanged; no normalization macro; all channels native |
| `alwan_ipt_*` | `I` native ~`[0, 1]`, `P` / `T` signed | unchanged; no normalization macro; `I` stays native |
| `alwan_iptch_*` | `I` native ~`[0, 1]`, `C` unbounded, `h` in radians `[-pi, pi]` | only `h` maps to `[0, 1]`; `I` and `C` unchanged |
| `alwan_igpgtg_*` | intensity-like `Ig`, signed `Pg` / `Tg` | unchanged; no normalization macro; all channels native |
| `alwan_icacb_*` | intensity / chromatic axes, signed | unchanged; no normalization macro; all channels native |
| `alwan_hcl_*` | `H` in radians `[-pi, pi]`, `C` unbounded, `L` bounded | `H` maps to `[0, 1]`; `L` remains bounded |
| `alwan_ihls_*` | `H` in radians `[0, 2pi)`, `L` and `S` bounded | `H` maps to `[0, 1]`; `L` and `S` unchanged |

Notes:

- The cylindrical modern spaces are where hue-unit confusion happens most often.
- `Oklch`, `JzCzhz`, `IPTch`, and `HCL` do not use degrees natively.

---

## Color Appearance Model Correlates

The CAM structs expose perceptual correlates rather than simple geometric
coordinates. The important rule is consistent:

- bounded lightness-like channels such as `J` or `Jz` normalize to `[0, 1]`
- finite hue channels normalize to `[0, 1]`
- unbounded or viewing-condition-dependent channels such as chroma, brightness,
  colorfulness, and saturation remain in native units

Representative families:

| Family | Native bounded channels | Public normalized channels |
|--------|--------------------------|-----------------------------|
| `alwan_ciecam02_correlates_*` | `J` in `[0, 100]`, `h` in `[0, 360)`, `H` in `[0, 400]` | `J`, `h`, `H` map to `[0, 1]` |
| `alwan_cam16_correlates_*` | `J` in `[0, 100]`, `h` in `[0, 360)`, `H` in `[0, 400]` | `J`, `h`, `H` map to `[0, 1]` |
| `alwan_zcam_correlates_*` | `Jz`, `hz`, `Kz`, `Wz` are bounded | bounded channels map to `[0, 1]` |
| `alwan_hellwig2022_correlates_*` | `J`, `h`, `H` bounded | those bounded channels map to `[0, 1]` |
| `alwan_hunt_correlates_*`, `alwan_kim2009_correlates_*`, `alwan_llab_correlates_*`, `alwan_rlab_correlates_*` | bounded lightness and finite hue channels | bounded channels map to `[0, 1]` |
| `alwan_atd95_correlates_*` | `H` bounded; other responses vary | bounded hue channel maps to `[0, 1]` |
| `alwan_nayatani95_correlates_*` | `L_star_N` in `[0,100]`, `theta` in radians `[0,2pi)`; `C` / `S` / `B_r` / `L_star_P` native | `L_star_N` and `theta` map to `[0,1]`; `C` / `S` / `B_r` / `L_star_P` unchanged |
| `alwan_cam18sl_correlates_*`, `alwan_cam20u_correlates_*` | hue `h` in `[0, 360)` | `h` maps to `[0, 1]` |
| CAM02-UCS / CAM16-UCS `J'a'b'` output | `J'` in `[0, 100]`, `a'` / `b'` signed | `J'` maps to `[0, 1]`; `a'` / `b'` unchanged |

If you are comparing against a paper, it is usually easier to disable public
range normalization so the reported correlates stay in native units.

---

## Video Range Is Separate

Do not mix up semantic channel ranges with encoded signal ranges.

The video helpers use:

```c
ALWAN_VIDEO_RANGE_FULL
ALWAN_VIDEO_RANGE_NARROW
```

Those apply to:

- `alwan_video_encode_{T}`
- `alwan_video_decode_{T}`
- `alwan_ycbcr_full_to_legal_{T}`
- `alwan_ycbcr_legal_to_full_{T}`
- the matching map and `_ex` variants

These APIs deal with signal scaling and quantization. They are separate from
the compile-time `ALWAN_NORMALIZE_RANGES` switch.

Useful mental model:

- `ALWAN_NORMALIZE_RANGES` changes how bounded semantic channels appear at the API boundary
- `ALWAN_VIDEO_RANGE_*` changes how encoded video values are scaled for full or narrow/legal signal range

---

## Usage Examples

With normalization enabled:

```c
alwan_lab_f64 lab;
alwan_xyz_to_lab_f64(&lab, &xyz, &white_xyz);
/* lab.L is in [0, 1], lab.a and lab.b remain native */

alwan_lch_f64 lch;
alwan_lab_to_lch_f64(&lch, &lab);
/* lch.h is normalized to [0, 1] on the public API */
```

With normalization disabled:

```c
#define ALWAN_NORMALIZE_RANGES 0
#include "alwan/alwan.h"

alwan_lab_f64 lab;
alwan_xyz_to_lab_f64(&lab, &xyz, &white_xyz);
/* lab.L is in [0, 100] */
```

For video signal conversion:

```c
alwan_video_encode_f64(
    dst,
    ALWAN_PIXEL_U16,
    rgb_linear,
    count,
    ALWAN_RGB_SPACE_BT2020,
    ALWAN_VIDEO_RANGE_NARROW,
    10,
    ctx);
```

---

## Hue Convention Summary

| Native hue convention | Common spaces |
|-----------------------|---------------|
| normalized `[0, 1]` | `HSV`, `HSL`, `HSP`, `HSPLog`, `HSY`, `HWB` |
| degrees `[0, 360)` | `LCh`, `LChuv`, many CAM hue channels |
| degrees `[0, 400]` | CAM hue quadrature `H` channels |
| radians `[-pi, pi]` | `Oklch`, `JzCzhz`, `IPTch`, `HCL` |
| radians `[0, 2pi)` | `IHLS`, `Nayatani95` hue-like angle |

---

## Practical Guidance

- Leave normalization enabled for UI-facing, graphics-facing, or mixed-engineering code.
- Disable it for paper matching, scientific validation, or formula-by-formula comparisons.
- Be explicit in tests about whether you expect native or normalized values.
- Treat video legal/full range handling as a separate pipeline concern.

See also:

- [configuration.md](configuration.md)
- [precision-and-limits.md](precision-and-limits.md)
- [determinism.md](determinism.md)
