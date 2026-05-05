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

### Integer normalization helpers

Implemented:

- `alwan_uint_to_float_f32`
- `alwan_uint_to_float_f64`
- `alwan_float_to_uint_f32`
- `alwan_float_to_uint_f64`

These are the current scalar normalization entry points for integer sample
workflows.

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

---

## Practical Near-Term TODOs

- [x] Keep interop parse / format / enumerate support in the public API
- [x] Keep integer normalization helpers in the public API
- [x] Keep video range encode / decode helpers in the public API
- [ ] Add richer metadata/query helpers around interop spaces
- [ ] Thread `ALWAN_DATA_*` through more public conversion workflows
- [ ] Add tighter tests and docs for extended-range behavior
- [ ] Add clearer docs for OETF vs EOTF usage in `alwan_rgb_space_desc_*`

---

## Notes

- This document intentionally focuses on the delta between the current code and
  the ColorInterop recommendations.
- When updating this file, prefer marking implemented header features as done
  rather than leaving them in "missing" sections.
