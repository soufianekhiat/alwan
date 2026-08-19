# ColorInterop Status

Current status of Alwan's work relative to the ASWF Color Interop Forum
recommendations.

References:

- [Texture Asset Color Spaces](https://github.com/AcademySoftwareFoundation/ColorInterop/blob/main/Recommendations/01_TextureAssetColorSpaces/TextureAssetColorSpaces.md)
- [Display Color Spaces](https://github.com/AcademySoftwareFoundation/ColorInterop/blob/main/Recommendations/02_DisplayColorSpaces/DisplayColorSpaces.md)

This file has been updated against the current `src/alwan/alwan.h`. It is a
status/TODO document, not a promise that every recommendation is already fully
validated against external reference suites.

---

## Implemented In The Current Header

### Interop ID registry helpers

Implemented:

- `alwan_interop_parse_f32`
- `alwan_interop_parse_f64`
- `alwan_interop_format`
- `alwan_interop_count`
- `alwan_interop_entry_at_f32`
- `alwan_interop_entry_at_f64`

This covers the basic parse / format / enumerate workflow.

Surface inconsistency to fix: `parse` and `entry_at` carry `_f32`/`_f64`
suffixes, but they touch no float data; they operate purely on the
`alwan_rgb_space` enum and on string IDs. In `src/alwan/api/alwan_interop.c`
the `_f32` variants simply forward to the `_f64` ones
(`alwan_interop_parse_f32` -> `alwan_interop_parse_f64`,
`alwan_interop_entry_at_f32` -> `alwan_interop_entry_at_f64`). `format` and
`count` are already correctly un-suffixed. The interop surface should expose a
single un-suffixed function per operation; the dual-precision suffixes here are
spurious and make the registry API look precision-dependent when it is not.

### Integer normalization helpers

Implemented:

- `alwan_uint_to_float_f32`
- `alwan_uint_to_float_f64`
- `alwan_float_to_uint_f32`
- `alwan_float_to_uint_f64`

These are the current scalar normalization entry points for integer sample
workflows. Combined with the `collect3` / `scatter3` gather-scatter helpers and
the typed `image_convert` path, the integer<->float normalization story is
solid and natively dual-precision for the canonical `(2^N - 1)` U8/U16
mapping.

Open gap before claiming full F16 interop: the per-format support is
inconsistent across entry points. The typed `_ex` map path and the helpers it
shares (`alwan__pixel_stride`, `alwan__load3_typed`, `alwan__store3_typed` in
`src/alwan/map/`) all carry real `ALWAN_PIXEL_F16` cases, so
`alwan_image_convert_*` does in fact round-trip F16 through those shared
loaders. But the `alwan_image_convert` / `alwan_image_convert_rgba` doc comments
in `src/alwan/alwan.h` still advertise only `U8/U16/F32/F64` and omit F16, so
the documented contract and the implemented behavior disagree. Reconcile the
header documentation (and add an explicit F16 round-trip regression) before
claiming F16 interop is fully and uniformly supported across entry points.

### Video encode / decode helpers

Implemented:

- `alwan_video_encode_f32`
- `alwan_video_encode_f64`
- `alwan_video_decode_f32`
- `alwan_video_decode_f64`
- `alwan_video_range` with `ALWAN_VIDEO_RANGE_FULL` and
  `ALWAN_VIDEO_RANGE_NARROW`

### Data semantic enum

Implemented as a type:

```c
ALWAN_DATA_COLOR
ALWAN_DATA_NON_COLOR
ALWAN_DATA_UNKNOWN
```

Current limitation: the enum exists, but most public conversion entry points do
not yet take a data-semantic parameter or expose pass-through policy around it.

### Display-space enum coverage

The current `alwan_rgb_space` enum already includes the headline display spaces
that older TODOs marked as missing, including:

- `ALWAN_RGB_SPACE_REC1886_REC709`
- `ALWAN_RGB_SPACE_REC2100_PQ`
- `ALWAN_RGB_SPACE_REC2100_HLG`
- `ALWAN_RGB_SPACE_DISPLAY_P3_HDR`

### Display-characterization / HDR-metadata interop (implemented, undocumented)

These are Color-Interop-Forum-relevant HDR display-characterization and
metadata surfaces. They are implemented in the header and backed by core code,
but are not yet covered by this status doc (or by determinism coverage):

- `alwan_st2086_init_f32` / `_f64`: ST.2086 (SMPTE) mastering-display metadata
- `alwan_pq_normalize_peak_f32` / `_f64`: PQ peak normalization to a display
  peak luminance
- `alwan_content_light_level_compute_f32` / `_f64`: MaxCLL / MaxFALL-style
  content-light-level computation over an RGB buffer

(Backed by `src/alwan/core/alwan_hdr_core.{h,inc}` and
`src/alwan/api/alwan_hdr_impl.inc`.) Action: add user-facing doc coverage for
the ST.2086 / MaxCLL metadata helpers, and add them to determinism coverage so
the emitted metadata values are byte-stable.

### Pointer-gamut boundary accessor (implemented, f64-only, undocumented)

`alwan_pointer_gamut_boundary(size_t *count_out)` returns the Pointer's-gamut
boundary reference points (`src/alwan/api/alwan_gamut.c`). Two gaps:

- It returns `alwan_vec2_f64 const *` only; there is no `_f32` twin, so an f32
  interop consumer must cast the boundary element-by-element rather than reading
  a native f32 array.
- It is undocumented here. Add doc coverage and, ideally, an f32 accessor (or a
  documented note that the reference data is intentionally f64-only).

---

## Partially Implemented Or Needs Validation

### Extended-range behavior

The public transfer-function and conversion APIs exist, but this file still
needs a tighter verification pass against the ColorInterop recommendations for:

- negative-value handling in piecewise transfer functions
- values above `1.0`
- clamping expectations in descriptor-driven conversions

### Bradford-first interop guidance

Chromatic adaptation support exists, including Bradford. What remains is
documentation and validation guidance for interop-centric workflows, not the
core CAT implementation itself.

### OETF vs EOTF documentation

The public RGB descriptor already carries both `oetf` and `eotf`. The remaining
work is clearer companion documentation and examples around when each field is
expected to be meaningful.

### Semantic data-type / descriptor surface

The descriptor type (`alwan_rgb_space_desc_f32` / `_f64`) is the intended
vehicle for descriptor-driven, semantically-tagged interop. Both earlier
correctness gaps are now closed:

- `alwan_gamut_map_advanced_f64` honours the descriptor: the input expressed
  in `space` is converted before mapping (`src/alwan/api/alwan_gamut.c`).
- The descriptor-based `alwan_rgb_to_xyz_f32` / `alwan_xyz_to_rgb_f32` and
  `alwan_gamut_map_advanced_f32` are defined (`api/alwan_rgb.c`,
  `api/alwan_gamut.c`); descriptor-driven interop links and works in both
  precisions.

---

## Still Missing

### Rich metadata/query API

Not yet present in the public header:

- `alwan_get_colorspace_info(...)`
- `alwan_get_display_info(...)`
- `alwan_is_basic(...)`
- `alwan_is_hdr(...)`
- `alwan_is_display_referred(...)`
- scene/display/basic/HDR metadata structs for UI-facing introspection

### Data-semantic-aware conversions

Still missing:

- public conversion APIs that accept `ALWAN_DATA_*`
- documented pass-through behavior for non-colour channels
- explicit alpha/data handling helpers at the conversion-policy layer

### Reference-validation coverage

Still useful future work:

- direct fixture comparison against representative OCIO / ColorInterop examples
- explicit extended-range regression cases
- more guide material around interop naming and workflow expectations

### Determinism coverage for CLF / .cube file I/O

CLF and `.cube` I/O are present in the API tail of `src/alwan/alwan.h`
(`.cube`: `alwan_cube_export_3d_*` / `alwan_cube_export_1d_*` /
`alwan_cube_import_3d_*` plus buffer variants; CLF: `alwan_clf_export_*` /
`alwan_clf_export_view_*` plus buffer variants; note CLF is export-only in the
public header today, no CLF import), but these paths are absent from the
determinism regression dump. The determinism contract now explicitly covers
file-I/O *byte content* (per the recent "I/O byte content is now on the
determinism contract" change), so to honor that contract the CLF and `.cube`
read+write paths should be added to determinism coverage: their emitted bytes
(and, for `.cube` import, parsed values) must be byte-identical across
platforms and runs.

---

## Practical Near-Term TODOs

- [x] Keep interop parse / format / enumerate support in the public API
- [x] Keep integer normalization helpers in the public API
- [x] Keep video range encode / decode helpers in the public API
- [ ] Add richer metadata/query helpers around interop spaces
- [ ] Thread `ALWAN_DATA_*` through more public conversion workflows
- [ ] Add tighter tests and docs for extended-range behavior
- [ ] Add clearer docs for OETF vs EOTF usage in `alwan_rgb_space_desc_*`
- [ ] Reconcile `alwan_image_convert*` header docs with actual F16 support and
      add an F16 round-trip regression
- [ ] Drop the spurious `_f32`/`_f64` suffixes on `alwan_interop_parse` /
      `alwan_interop_entry_at` (expose single un-suffixed functions)
- [ ] Add CLF / `.cube` read+write paths to determinism (byte-content) coverage
- [x] Make `alwan_gamut_map_advanced` honor its space descriptor, and
      implement the f32 descriptor entry points
      `alwan_rgb_to_xyz_f32` / `alwan_xyz_to_rgb_f32` /
      `alwan_gamut_map_advanced_f32` *(done: defined in `api/alwan_rgb.c`
      / `api/alwan_gamut.c`)*
- [ ] Document ST.2086 / PQ-peak / MaxCLL HDR metadata helpers and add them to
      determinism coverage
- [ ] Document `alwan_pointer_gamut_boundary` and consider an f32 accessor

---

## Notes

- This document intentionally focuses on the delta between the current code and
  the ColorInterop recommendations.
- When updating this file, prefer marking implemented header features as done
  rather than leaving them in "missing" sections.
