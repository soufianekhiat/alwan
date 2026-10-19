# Moving from alwan 2.0.0 to 3.0.0

Most of 3.0.0 is additions: 976 new functions against 1,442 in 2.0.0. What breaks a 2.0.0
caller is short, and listed here. `api_v2_v3.json` beside this file is the same inventory,
machine-readable, with every added, removed, renamed and changed declaration, enum, struct and
macro, each with its old and new signature and the CHANGELOG entry behind it.

No enum value was renumbered (only the `_COUNT` sentinels moved) and no 2.0.0 struct changed
layout.

## Source shims

`#include "alwan_v2_compat.h"` after `alwan.h` keeps these 2.0.0 spellings compiling, with
no runtime cost:

| 2.0.0 | 3.0.0 |
|---|---|
| `alwan_float_to_half_f32`, `_f64` | `alwan_float_to_half` |
| `alwan_half_to_float_f32`, `_f64` | `alwan_half_to_float` |
| `alwan_interop_entry_at_f32`, `_f64` | `alwan_interop_entry_at` |
| `alwan_interop_parse_f32`, `_f64` | `alwan_interop_parse` |
| `alwan_lut2d_dimensions_f32`, `_f64` | `alwan_lut2d_dimensions` |
| `alwan_xyz_adapt_{T}` | `alwan_xyz_adapt_{T}_map_interleave` |
| `alwan_lut1d_sample_{T}(r, lut, t, n)` | `alwan_table1d_sample_{T}(r, lut, n, t, ALWAN_SAMPLE_LINEAR)` |
| `alwan_lut2d_sample_{T}(r, strip, rgb, n)` | `alwan_table2d_sample_{T}(r, strip, n, rgb, ALWAN_SAMPLE_TRILINEAR)` |
| `alwan_lut3d_sample_{T}(r, cube, rgb, n)` | `alwan_table3d_sample_{T}(r, cube, n, rgb, ALWAN_SAMPLE_TRILINEAR)` |

The shims are a porting aid. Rename the calls and drop the header.

## Changes a shim cannot hide

These keep their name and change their signature, or lose a process global. The compiler
reports each one.

| Function | 2.0.0 | 3.0.0 |
|---|---|---|
| `alwan_set_aces_interp`, `alwan_get_aces_interp` | a process-wide setting | gone: `alwan_ctx_set_aces_interp(ctx, m)` / `alwan_ctx_get_aces_interp(ctx)`, or the parameter below |
| `alwan_aces1_output_transform_{T}` | `(out, in, output)` | `(out, in, output, interp)`; pass `ALWAN_ACES_INTERP_BSPLINE` for the 2.0.0 default |
| `alwan_whiteness_astm_e313_{T}`, `alwan_whiteness_cie2004_{T}`, `alwan_yellowness_astm_e313_{T}` | returned the index, -1 on error | return `alwan_status`, the index through a new first out parameter |
| `alwan_delta_e_cmc_batch_ex` | `..., l, c)` | `..., alwan_delta_e_cmc_params_f64 const *params)` |
| `alwan_xyz_from_spd_camera_{T}` | camera enum; camera RGB returned in an `alwan_xyz` | `alwan_camera_rgb_from_spd_{T}`, camera registry index, the same numbers in an `alwan_rgb` |
| `alwan_spd_camera_sensitivity_{T}` | camera enum | `alwan_camera_sensitivities_{T}` with registry index 52 or 53 |
| `alwan_apca_contrast_{T}` | APCA-W3 | removed: its licence does not allow redistribution |

The CLF, `.cube`, gamut-spatial and picture-formation functions are unchanged but now sit
behind feature switches (`ALWAN_WITH_FILE_FORMATS`, `ALWAN_WITH_FORMATION`), on by default. A
build that turns a family off has no declaration for its functions (`configuration.md`).

## Results that changed

3.0.0 fixes many functions that 2.0.0 computed wrongly; their outputs move. The ones a 2.0.0
caller is most likely to notice:

- The ACES 1.x presets now follow their ODTs, and the ACES 2.0 cinema presets and inverse are
  the Academy's.
- Hunt, CIECAM02 and CAM16 surround terms, and the CAM hue quadrature.
- Tony McMapface, Lottes, Brettel CVD, HyAB, Barten CSF, Munsell, Sprague and Akima, BT.2446 A
  and C, ZCAM: each now computes the published model.
- F16 stores round to nearest even (they truncated).
- With `ALWAN_NORMALIZE_RANGES`, a signed radian hue normalises to h/pi on [-1, 1].
- An unknown camera index returns `ALWAN_E_RANGE` (was `ALWAN_E_INVALID`).
- `alwan_create` refuses non-zero `flags` and an allocator with only one of its two callbacks.

`api_v2_v3.json` (`default_behaviour_changes`) lists all 100, each with its CHANGELOG line.

## Image processing

Image-processing functions that appeared in 3.0.0 snapshots moved to Suwar before the release.
No 2.0.0 function moved. The rule for snapshot users is `alwan_x` to `suwar_x` and `ALWAN_X` to
`SUWAR_X`, with no shims.
