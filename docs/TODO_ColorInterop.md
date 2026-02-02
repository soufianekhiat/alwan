# ColorInterop Compliance TODO

This document tracks compliance with the ASWF Color Interop Forum recommendations for color space interoperability.

**References:**
- [Texture Asset Color Spaces](https://github.com/AcademySoftwareFoundation/ColorInterop/blob/main/Recommendations/01_TextureAssetColorSpaces/TextureAssetColorSpaces.md)
- [Display Color Spaces](https://github.com/AcademySoftwareFoundation/ColorInterop/blob/main/Recommendations/02_DisplayColorSpaces/DisplayColorSpaces.md)

---

## 1. Texture Asset Color Spaces Compliance

### 1.1 Core Scene-Referred Color Spaces

**Status:** ✅ Partially Implemented

Alwan already has most of these spaces, but needs interop ID mapping:

| Space | Interop ID | Current Alwan Enum | Status |
|-------|-----------|-------------------|--------|
| ACEScg | `lin_ap1_scene` | `ALWAN_RGB_SPACE_ACESCG` | ✅ Exists |
| ACES2065-1 | `lin_ap0_scene` | `ALWAN_RGB_SPACE_ACES2065_1` | ✅ Exists |
| Linear Rec.709 | `lin_rec709_scene` | `ALWAN_RGB_SPACE_LINEAR_SRGB` | ✅ Exists |
| Linear P3-D65 | `lin_p3d65_scene` | `ALWAN_RGB_SPACE_LINEAR_P3_D65` | ✅ Exists |
| sRGB Rec.709 | `srgb_rec709_scene` | `ALWAN_RGB_SPACE_SRGB` | ✅ Exists |

**TODO:**
- [ ] Add interop ID to enum mapping system
- [ ] Add function: `alwan_interop_id_to_rgb_space(const char *interop_id)`
- [ ] Add function: `const char* alwan_rgb_space_to_interop_id(alwan_rgb_space space)`
- [ ] Document which spaces are scene-referred vs display-referred

### 1.2 Extended Range Value Handling

**Status:** ⚠️ Needs Verification

The spec requires:
- Values > 1.0: Evaluate transfer function directly (no clamping)
- Values < 0.0: For power functions pass through unchanged, for piecewise functions multiply by origin slope

**TODO:**
- [ ] Audit all transfer function implementations for extended range support
- [ ] Verify sRGB handles negative values correctly (multiply by 12.92 for negative linear)
- [ ] Add unit tests for extended range: test values [-0.5, -0.1, 0.0, 0.5, 1.0, 2.0, 10.0]
- [ ] Document extended range behavior in API docs

### 1.3 Chromatic Adaptation

**Status:** ✅ Implemented (needs verification)

The spec requires: **Bradford cone primaries for von Kries adaptation**

Current implementation:
- `alwan_xyz_adapt()` with `ALWAN_CAT_BRADFORD`

**TODO:**
- [ ] Verify Bradford adaptation matches ColorInterop reference
- [ ] Add test cases from OCIO config
- [ ] Set Bradford as the default/recommended CAT for interop workflows

### 1.4 Non-Color Data Handling

**Status:** ❌ Not Implemented

The spec requires two special designations:
- **"Data"**: For non-color data (normals, displacement, roughness, etc.) - no conversions applied
- **"Unknown"**: For undetermined color space - prevents incorrect defaults

**TODO:**
- [ ] Add `alwan_data_type` enum:
  ```c
  typedef enum {
      ALWAN_DATA_COLOR,      // Color data, apply transforms
      ALWAN_DATA_NON_COLOR,  // Non-color (normals, displacement, etc.), no transform
      ALWAN_DATA_UNKNOWN     // Unknown, prevent defaults
  } alwan_data_type;
  ```
- [ ] Add data type parameter to conversion functions (optional, default: COLOR)
- [ ] Add `alwan_is_data()` / `alwan_is_color()` query functions
- [ ] Document that alpha channels should always be treated as data

### 1.5 Integer to Float Normalization

**Status:** ❌ Not Implemented

The spec requires: Normalize by dividing by (2^N - 1), not 2^N

**TODO:**
- [ ] Add integer normalization functions:
  ```c
  void alwan_uint_to_float(alwan_scalar *out, uint16_t const *in,
                           int bit_depth, size_t count);
  void alwan_float_to_uint(uint16_t *out, alwan_scalar const *in,
                           int bit_depth, size_t count);
  ```
- [ ] Support bit depths: 8, 10, 12, 16
- [ ] Use (2^N - 1) normalization: 255 for 8-bit, 1023 for 10-bit, etc.
- [ ] Document that color management applies after normalization

### 1.6 Naming Convention Support

**Status:** ❌ Not Implemented

The spec uses compact names: `[transfer]_[primaries]_[imagestate]`

Examples:
- `lin_ap1_scene` (Linear ACEScg)
- `srgb_rec709_scene` (sRGB)
- `g22_adobergb_scene` (Gamma 2.2 Adobe RGB)

**TODO:**
- [ ] Add `alwan_colorspace_info` structure:
  ```c
  typedef struct {
      const char *interop_id;       // "lin_ap1_scene"
      const char *user_name;        // "ACEScg"
      const char *full_name;        // "Linear ACES CG (AP1)"
      alwan_rgb_space space;        // ALWAN_RGB_SPACE_ACESCG
      int is_basic;                 // 1 for basic, 0 for advanced
      int is_scene_referred;        // 1 for scene, 0 for display
  } alwan_colorspace_info;
  ```
- [ ] Add query function: `const alwan_colorspace_info* alwan_get_colorspace_info(alwan_rgb_space)`
- [ ] Add lookup function: `alwan_rgb_space alwan_find_by_interop_id(const char *id)`
- [ ] Generate full interop ID table for all supported spaces

---

## 2. Display Color Spaces Compliance

### 2.1 Core Display-Referred Color Spaces

**Status:** ⚠️ Partially Implemented

| Space | Interop ID | Current Alwan Enum | Status |
|-------|-----------|-------------------|--------|
| sRGB | `srgb_rec709_display` | `ALWAN_RGB_SPACE_SRGB` | ✅ Exists |
| Display P3 | `srgb_p3d65_display` | `ALWAN_RGB_SPACE_DISPLAY_P3` | ✅ Exists |
| Rec.1886 Rec.709 | `rec1886_rec709_display` | ❌ Missing | Need to add |
| Linear Rec.709 | `lin_rec709_display` | `ALWAN_RGB_SPACE_LINEAR_SRGB` | ✅ Exists |
| Rec.2100-PQ | `pq_rec2020_display` | ❌ Missing | Need to add |
| Rec.2100-HLG | `hlg_rec2020_display` | ❌ Missing | Need to add |
| Display P3 HDR | `pq_p3d65_display` | ❌ Missing | Need to add |
| Linear P3-D65 | `lin_p3d65_display` | `ALWAN_RGB_SPACE_LINEAR_DISPLAY_P3` | ✅ Exists |
| Linear Rec.2020 | `lin_rec2020_display` | `ALWAN_RGB_SPACE_LINEAR_REC2020` | ✅ Exists |

**TODO:**
- [ ] Add missing display color spaces:
  ```c
  ALWAN_RGB_SPACE_REC1886_REC709,      // Rec.709 primaries + BT.1886 EOTF
  ALWAN_RGB_SPACE_REC2100_PQ,          // Rec.2020 primaries + PQ
  ALWAN_RGB_SPACE_REC2100_HLG,         // Rec.2020 primaries + HLG
  ALWAN_RGB_SPACE_DISPLAY_P3_HDR,      // P3 primaries + PQ
  ```
- [ ] Implement their descriptors in `alwan_rgb.c`
- [ ] Add interop ID mapping for display spaces

### 2.2 OETF vs EOTF Clarification

**Status:** ⚠️ Needs Documentation

The spec emphasizes:
- **Display spaces use EOTFs** (Electro-Optical Transfer Functions)
- **Camera/scene spaces use OETFs** (Opto-Electronic Transfer Functions)
- **OETF and EOTF must be exactly symmetric inverses**

Current issue: `alwan_rgb_space_desc` has both `oetf` and `eotf` fields, but usage is not always clear.

**TODO:**
- [ ] Add comprehensive API documentation explaining OETF vs EOTF
- [ ] Add validation function:
  ```c
  int alwan_transfer_function_validate_symmetry(
      alwan_transfer_function tf, alwan_scalar tolerance);
  ```
- [ ] Add unit tests verifying OETF(EOTF(x)) ≈ x for all transfer functions
- [ ] Document which spaces are display vs scene vs camera
- [ ] Update `alwan_rgb_space_desc` documentation with usage examples

### 2.3 Integer Video Signal Encoding

**Status:** ❌ Not Implemented

The spec requires support for both:
- **Full range**: 0-255 for 8-bit
- **Narrow range** (SMPTE): 16-235 for 8-bit, with extended range using mirrored transfer functions

**TODO:**
- [ ] Add video range enum:
  ```c
  typedef enum {
      ALWAN_RANGE_FULL,      // 0-255 for 8-bit
      ALWAN_RANGE_NARROW,    // 16-235 for 8-bit (SMPTE)
      ALWAN_RANGE_FULL_EXT,  // Full with extended range mirroring
  } alwan_video_range;
  ```
- [ ] Add encoding/decoding functions:
  ```c
  void alwan_encode_video_signal(uint16_t *out, alwan_scalar const *in,
                                   int bit_depth, alwan_video_range range,
                                   alwan_transfer_function tf, size_t count);
  void alwan_decode_video_signal(alwan_scalar *out, uint16_t const *in,
                                   int bit_depth, alwan_video_range range,
                                   alwan_transfer_function tf, size_t count);
  ```
- [ ] Support bit depths: 8, 10, 12
- [ ] Implement extended range mirroring for narrow range

### 2.4 Display Space Metadata

**Status:** ❌ Not Implemented

The spec designates certain spaces as "Basic" for novice users:
- **SDR Basic**: sRGB, Rec.1886 Rec.709, Display P3
- **HDR Basic**: Display P3 HDR, ST2084-P3-D65, Rec.2100-PQ, Rec.2100-HLG

**TODO:**
- [ ] Add display colorspace info API:
  ```c
  typedef struct {
      alwan_rgb_space space;
      const char *user_name;        // "sRGB"
      const char *interop_id;       // "srgb_rec709_display"
      int is_basic;                 // 1 for basic, 0 for advanced
      int is_hdr;                   // 1 for HDR, 0 for SDR
      int is_display_referred;      // Always 1 for display spaces
  } alwan_display_colorspace_info;

  const alwan_display_colorspace_info* alwan_get_display_info(alwan_rgb_space);
  ```
- [ ] Populate metadata for all display spaces
- [ ] Add query functions: `alwan_is_basic()`, `alwan_is_hdr()`, `alwan_is_display_referred()`

### 2.5 Extended Range Documentation

**Status:** ⚠️ Needs Documentation

The spec states:
- Linear display spaces support values < 0.0 (out-of-gamut) and > 1.0 (HDR)
- 1.0 represents SDR white (100 nits) when HDR content is present
- No clamping should occur during conversions

**TODO:**
- [ ] Add API documentation explaining extended range semantics
- [ ] Document HDR reference levels (1.0 = 100 nits, 10.0 = 1000 nits for PQ)
- [ ] Add examples showing HDR workflow
- [ ] Verify no clamping occurs in conversion functions

---

## 3. Interoperability API (New)

### 3.1 Unified Interop ID System

**Status:** ❌ Not Implemented

Both specs use the same interop ID naming convention. Need unified API.

**TODO:**
- [ ] Add unified query API:
  ```c
  // Parse any interop ID to Alwan space
  int alwan_parse_interop_id(alwan_rgb_space *out, const char *interop_id);

  // Generate interop ID from Alwan space
  const char* alwan_format_interop_id(alwan_rgb_space space);

  // Get full metadata for any space
  const alwan_colorspace_info* alwan_get_colorspace_info_by_id(const char *id);

  // List all spaces matching criteria
  int alwan_list_colorspaces(alwan_rgb_space *out, size_t max_count,
                             int filter_basic, int filter_scene, int filter_display);
  ```

### 3.2 OpenColorIO Compatibility

**Status:** ❌ Not Implemented

The specs reference OCIO configs as reference implementations.

**TODO:**
- [ ] Add OCIO config export:
  ```c
  int alwan_export_ocio_config(const char *filepath);
  ```
- [ ] Verify numerical accuracy against OCIO (with `OCIO_OPTIMIZATION_FLAGS=0`)
- [ ] Add test cases comparing Alwan results to OCIO for all interop spaces
- [ ] Document any intentional differences

### 3.3 Custom Color Space Support

**Status:** ✅ Implemented

The spec states: "It is highly recommended that projects support the ability to define and interchange custom color space encodings."

Alwan already supports this via:
- `alwan_rgb_derive_matrices()` - custom primaries
- `alwan_rgb_space_desc` - custom transfer functions

**TODO:**
- [ ] Document custom color space workflow with examples
- [ ] Add helper to create interop-style naming for custom spaces
- [ ] Consider adding JSON/XML import/export for custom space definitions

---

## 4. Testing & Validation

### 4.1 Compliance Test Suite

**TODO:**
- [ ] Add ColorInterop compliance test suite:
  - [ ] Test all interop ID mappings
  - [ ] Test extended range handling (negative and >1 values)
  - [ ] Test OETF/EOTF symmetry for all transfer functions
  - [ ] Test Bradford adaptation against reference
  - [ ] Test integer normalization (2^N - 1)
  - [ ] Test video signal encoding (full/narrow range)
  - [ ] Test non-color data handling (should be identity)

### 4.2 Reference Data Validation

**TODO:**
- [ ] Download OCIO reference configs
- [ ] Generate reference data from OCIO for all interop spaces
- [ ] Add tests comparing Alwan to OCIO reference data
- [ ] Document acceptable tolerance levels

### 4.3 Documentation

**TODO:**
- [ ] Add ColorInterop compliance section to README
- [ ] Add interop workflow examples
- [ ] Add migration guide for existing code
- [ ] Document differences from reference implementation (if any)

---

## 5. Priority Roadmap

### Phase 1: Foundation (High Priority)
1. Add missing display spaces (Rec.2100-PQ, Rec.2100-HLG, Rec.1886 Rec.709)
2. Add interop ID mapping system
3. Add non-color data type enum
4. Document OETF vs EOTF usage

### Phase 2: Core Features (Medium Priority)
5. Add integer normalization functions (2^N - 1)
6. Add video signal encoding (full/narrow range)
7. Add extended range unit tests
8. Verify Bradford adaptation correctness

### Phase 3: Metadata & Polish (Lower Priority)
9. Add colorspace metadata API (basic, HDR, scene/display flags)
10. Add OCIO config export
11. Add comprehensive documentation
12. Add compliance test suite

### Phase 4: Advanced Features (Optional)
13. Add custom color space JSON import/export
14. Add OCIO numerical validation tests
15. Add extended example applications
16. Consider runtime color space registry

---

## 6. Breaking Changes

**API Changes Required:**
- Adding `alwan_data_type` parameter to conversion functions (can default to COLOR)
- Potentially renaming some enums to clarify scene vs display
- May need to version the API for major changes

**Mitigation:**
- Add new functions alongside old ones initially
- Deprecate old API in next major version
- Provide migration guide

---

## 7. References & Resources

**ASWF ColorInterop:**
- Repository: https://github.com/AcademySoftwareFoundation/ColorInterop
- Texture Assets: [01_TextureAssetColorSpaces](https://github.com/AcademySoftwareFoundation/ColorInterop/blob/main/Recommendations/01_TextureAssetColorSpaces/TextureAssetColorSpaces.md)
- Display: [02_DisplayColorSpaces](https://github.com/AcademySoftwareFoundation/ColorInterop/blob/main/Recommendations/02_DisplayColorSpaces/DisplayColorSpaces.md)

**Reference Implementations:**
- OpenColorIO: https://opencolorio.org/
- OCIO Config: Check ColorInterop repo for reference configs

**Standards:**
- ITU-R BT.709: HD television color space
- ITU-R BT.2020: UHD television color space
- ITU-R BT.2100: HDR television (PQ and HLG)
- SMPTE ST 2084: PQ EOTF
- SMPTE ST 2065-1: ACES2065-1
- IEC 61966-2-1: sRGB

---
