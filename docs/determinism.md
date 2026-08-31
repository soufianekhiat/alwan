# Deterministic mode

When you build Alwan with `-DALWAN_DETERMINISTIC=ON`, the build system defines
`ALWAN_DETERMINISTIC` and the library routes through the deterministic paths in
`alwan_platform.h`, `alwan_math.h`, the SIMD reduction layer, and the batch map
kernels.

In that mode the library produces
**byte-identical numerical output** across compilers, optimization levels,
operating systems, and CPU architectures. The same input gives the same
bytes on Windows MSVC x64, Linux gcc x64, Linux clang ARM, and macOS
Apple Silicon.

> **⚠️ Scope of the byte-identity guarantee: trig is not covered (yet).**
> `ALWAN_DETERMINISTIC` polynomial-replaces only `pow / exp / log / log2 /
> cbrt / fma` (see `alwan_math.h` and `core/alwan_deterministic.h`). It does
> **not** replace `atan2 / sin / cos / tan / tanh / log10`; those still expand
> to the platform `libm`, which differs in the last 1–3 ULPs between vendors.
>
> Consequently, any output that flows through a hue angle, a cylindrical/polar
> conversion, or another trig-dependent term is **not** structurally
> byte-identical across platforms: its cross-platform agreement is *empirical*
> (it happens to match on the runners we test) and is not *guaranteed* by the
> implementation. Specifically **not** covered by the byte-identity contract:
>
> - **all CAM hue correlates** (CAM16/CIECAM02/ZCAM/Hellwig2022/… `h`),
> - **all cylindrical / LCh-style conversions** (LCh, LCHuv, JzCzHz, Oklch,
>   HCL, and every `*_to_lch` / hue-angle channel),
> - **dE2000** and **dE CMC** (both use `atan2` / `sin` / `cos` on hue),
> - **ACES JMh** (the `M`/`h` appearance path),
> - **CSS gamut** mapping (Oklch hue),
> - **Barten** CSF `pupil` / contrast-sensitivity helpers, and
> - **Rayleigh scattering at non-zero latitude**: the latitude term multiplies
>   in a `cos(latitude)`, so only the default (zero-latitude) call is on the
>   contract; any non-zero latitude reintroduces a `libm cos` and forfeits
>   byte-identity.
>
> A deterministic trig layer (`det_atan2 / det_sin / det_cos / det_tan /
> det_tanh / det_log10`) is **not implemented**; what it would take is in
> [`alwan_future.md`](alwan_future.md). Until it exists, treat the angle and hue
> channels above as fast-mode even when the rest of the pipeline is
> deterministic.

The cross-platform regression harness for this mode lives in the sibling
`alwan_dev` repository; this repository contains the production implementation.

Use it when you need:

- Reproducible regression baselines for visual tests.
- Bit-exact replay of colour pipelines for audit / forensics / golden files.
- Cross-platform CI where a hash diff actually means something changed.
- Frame-accurate match between an offline reference and a live renderer
  on a different machine.

Skip it when you need:

- Maximum throughput on a single platform.
- Last-bit agreement with `libm pow` / `libm log` (we ship a polynomial
  approximation rather than a bit-perfect libm clone).

By default, `ALWAN_DETERMINISTIC=OFF` and the library uses libm + hardware
FMA + the full SIMD path. That is fast, but two different compilers may
produce results that differ in the last bit.

---

## What changes when you flip the switch

| Concern                         | Fast (default)                       | Deterministic                              |
| :------------------------------ | :----------------------------------- | :----------------------------------------- |
| `pow / exp / log / log2 / cbrt / fma` | libm                           | Polynomial: argument reduction + Chebyshev |
| `atan2 / sin / cos / tan / tanh / log10` | libm                      | **Still libm**; not replaced (see trig caveat above) |
| sRGB / BT.2020 / BT.709 OETF, EOTF | libm `pow`                        | Domain-split minimax polynomials           |
| FMA contraction (`a*b + c`)     | Compiler decides; hardware FMA on aarch64, often on x86 with `-mfma` | `-ffp-contract=off` / `/fp:precise`; never fused |
| SIMD horizontal sum             | Native pairwise (`vpaddq_pd`, `_mm_hadd_pd`, `vaddvq_f64`) | Canonical scalar left-to-right reduction |
| SIMD per-lane libm              | Lane-unpack to libm                  | Lane-unpack to deterministic polynomial    |
| `_oetf_apply` / `_eotf_apply` SIMD (sRGB/PQ/HLG) | Vectorised approximations (`pow24`, `pow_inv24`) | Lane-unpacked to canonical scalar polynomial |
| `_map_interleave` / `_map_planar` colorspace SIMD (25+ spaces) | Vectorised matrix muls + per-channel pow/cbrt | `ALWAN_MAP_SIMD_WIDTH=1`: every map kernel runs its scalar tail loop |
| Output stability across runs    | Last-bit may vary between platforms  | Byte-identical; verified by CI matrix      |
| Throughput cost                 |,                                    | ~5–20% slower depending on workload        |

The throughput cost is dominated by the polynomial `pow` (vs. a 1-instruction
hardware FMA chain) and by the SIMD lane-unpack penalty for kernels that
fall back to scalar. Pure matrix transforms barely change.

---

## Where last-bit differences leak in (and how we close each)

### 1. `libm` precision

Each platform ships its own `libm`. On the same input, Apple's `pow`,
glibc's `pow`, and Microsoft's `pow` may return values that differ in
the last 1–3 ULPs. They're all "correctly rounded" within their own
specs, but the specs aren't byte-exact.

**Fix:** ship a polynomial implementation that does not call libm.
`alwan_det_log2 / exp2 / pow_pos / cbrt` use frexp/ldexp argument
reduction (which IS bit-deterministic on IEEE-754 platforms) plus a
Chebyshev minimax polynomial fitted to the normalised domain. Output
is bit-identical wherever IEEE-754 is honoured.

### 2. FMA contraction

`a * b + c` can compile to two operations (multiply, then add: two
roundings) or one fused-multiply-add (one rounding). The two-rounding
result and one-rounding result differ by up to 0.5 ULP. Worse, the
choice is platform- and compiler-dependent: aarch64 always fuses (FMA
is mandatory in the ISA), x86 fuses only with `-mfma`, MSVC defaults
to no fusion under `/fp:precise`.

**Fix:** the build adds `-ffp-contract=off` (gcc/clang) or `/fp:precise`
(MSVC), and `alwan_math.h` redefines `ALWAN_FMA(a,b,c)` to `((a)*(b)+(c))`
so the compiler can't re-fuse behind our back. Two roundings, everywhere.

### 3. SIMD reduction order

`hsum(v[0..N])` is computed differently per width:

```
SSE2 (4-lane f32):   ((v0+v1) + (v2+v3))
AVX  (8-lane f32):   (((v0+v1)+(v2+v3)) + ((v4+v5)+(v6+v7)))
NEON (4-lane f32):   vaddvq_f32: implementation-defined order
Scalar:              (((v0+v1)+v2)+v3)+...
```

Different rounding cascades, different last-bit. Difference is 1–2 ULPs
typically but compounds in long pipelines.

**Fix:** in det mode `alwan_simd_*_hsum` and `_hadd` route to a canonical
scalar reduction left-to-right, regardless of native vector width. Per-lane
operations (add, mul, fma, polynomial evaluation) stay vectorised because
each lane runs the same op; width can't perturb bit-exactness there.

### 4. SIMD vs scalar arithmetic divergence

Even when both paths use the same primitives, the SIMD kernels often
optimise differently from the scalar formulas. Two examples we've hit:

- **PQ**: scalar uses `linear / 10000.0`; SIMD precomputes `inv10k = 1.0 / 10000.0`
  and uses `linear * inv10k`. Mathematically equal, but `1/10000` is not
  exactly representable in f64, so the product differs by 1 ULP from
  the division. After two `pow` calls the divergence becomes ~1e-10 abs.

- **HLG**: scalar uses `c = 0.5 - a*ln(4a)` (computed at runtime);
  SIMD uses the literal `0.55991072952956202`. The polynomial `ln`
  is accurate to ~1e-12 vs the literal; the difference is small
  (~1e-12) but enough to break bit-exact.

**Fix:** det mode closes this in two layers.

1. The OETF/EOTF apply functions in `alwan_rgb.c` (legacy SIMD path
   used by `alwan_oetf_apply_f64` / `alwan_eotf_apply_f64`) lane-unpack
   to the canonical scalar:
   - sRGB / BT.2020 / BT.709: `alwan_det_*_oetf` / `_eotf` polynomials.
   - PQ / HLG: `alwan_pq_oetf` / `alwan_hlg_oetf`.
   - JzAzBz PQ: formula inlined (cross-TU header dependencies make
     linking the named scalar awkward).
   - Lab `f(t)` cube-root branch: `alwan_det_cbrt`.

2. The full colorspace conversion kernels (`alwan_xyz_to_lab_f64_map_interleave`,
   `alwan_xyz_to_oklab_f64_map_interleave`, `alwan_rgb_to_hsv_f64_map_interleave`,
   ...) all live in `*_map_kernels.inc` files that gate their SIMD body
   on `#if ALWAN_MAP_SIMD_WIDTH > 1`. In det mode `ALWAN_MAP_SIMD_WIDTH`
   is redefined to 1 in `alwan_map_simd_defs.h`, so every kernel falls
   through to its scalar tail loop and runs the public single-pixel
   function per pixel. This catches the matrix-multiply codegen drift
   too (scalar `+` vs SIMD `add+add` ordering on MSVC even with
   `/fp:precise`) without needing to enumerate every kernel.

   25 colorspace conversions (sRGB/PQ/HLG OETF/EOTF, JzAzBz, IPT,
   Oklab, Lab, Luv, HSV, HSL, HWB, HSY, HSP, LCh, JzCzHz, ICtCp,
   IgPgTg, ICaCb, OSA-UCS, Hunter-Lab, ProLab, DIN99) are verified
   bit-exact (0 ULP) in det mode by `alwan_dev/tests/88_simd_parity.c`.

This is the largest source of perf cost in det mode. The map kernels
that fall back to scalar pay roughly the difference between their
vectorised body and the scalar tail loop, typically 4–10× slower
on per-pixel throughput depending on lane width. Element-wise SIMD
outside the kernel files (alwan_rgb.c's apply functions) keeps the
vectorised path with lane-unpacked math primitives.

### 5. Denormal flushing

Apple Silicon (and some x86 settings) flush denormals to zero by default
(FZ=1 in FPCR). A value of `1e-38` becomes `0` on those platforms but
stays `1e-38` on Linux x86. After a multiplication chain the flushed-vs-not
divergence becomes user-visible.

**Status:** alwan does not currently set FZ explicitly. Almost all
colour-science values stay well above the denormal threshold, so this
hasn't bitten us in CI yet. If you see a det-mode hash diff that's
isolated to the smallest-magnitude inputs, this is the prime suspect;
file an issue.

---

## ULP-distance testing

Throughout the test suite, "is this output close enough?" is measured
in **ULPs** (Units in the Last Place) rather than absolute or relative
tolerance. Two `double`s differ by *N* ULPs if and only if their bit
representations (after canonicalising the sign) differ by *N* when
read as `int64_t`.

```c
static inline uint64_t alwan_ulps_f64(double a, double b) {
    if (a == b) return 0;                      // covers +0 vs -0
    if (isnan(a) || isnan(b)) return UINT64_MAX;
    int64_t ia, ib;
    memcpy(&ia, &a, sizeof(ia));
    memcpy(&ib, &b, sizeof(ib));
    if (ia < 0) ia = (int64_t)0x8000000000000000LL - ia;
    if (ib < 0) ib = (int64_t)0x8000000000000000LL - ib;
    return (uint64_t)((ia > ib) ? (ia - ib) : (ib - ia));
}
```

Why ULP-distance instead of `fabs(a - b) < eps`?

- **Self-documenting.** "16 ULP budget" tells you exactly how many
  bits of mantissa you've lost; "1e-12" tells you nothing without the
  value's magnitude.
- **Magnitude-aware automatically.** A 4-ULP delta near 1.0 is ~1e-15;
  a 4-ULP delta near 100 is ~1e-13; a 4-ULP delta near 1e-6 is ~1e-21.
  Same budget tracks all of them.
- **Platform-portable.** No need to widen the threshold "because Apple
  libm". The budget tracks accumulated rounding regardless of where
  it came from.
- **Bit-exact is "0 ULP".** No special syntax, no separate macro.

Test macros: `TEST_ASSERT_CLOSE_ULP_F64(actual, expected, max_ulps, msg)`,
`TEST_ASSERT_BITEXACT_F64(actual, expected, msg)`, plus `_F32` and
buffer-pointwise variants. See `alwan_dev/tests/test_common.h`.

The reference for the technique is Bruce Dawson, [*Comparing
Floating Point Numbers, 2012 Edition*](https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/).

---

## Compiler and platform support

Tested in CI on:

- Linux x86_64: gcc 11+, clang 14+
- Linux aarch64: gcc 11+, clang 14+
- macOS x86_64: Apple clang
- macOS aarch64 (Apple Silicon): Apple clang
- Windows x86_64: MSVC 19+
- Windows aarch64: MSVC 19+

The CI workflow `.github/workflows/determinism.yml` runs the
`det_run_regression` tool (407,365 lines of hex-dumped IEEE-754 bytes
per platform) on every runner and asserts that all artefacts match.
If any byte differs, the build fails and the diff is printed. The
current reference hash on the local Windows MSVC x64 build is
`7d41e9af28d5acf2fc6bbffa54b39ea7` (this is informational only: the
CI diffs all platforms against each other rather than against a
committed reference).

Coverage in the regression dump (manifest v49):

- f64 + f32 deterministic math primitives: log2, exp2, pow_pos(., 1/2.4), cbrt.
- f64 + f32 OETF/EOTF for sRGB and BT.2020 / BT.709.
- 24 public colorspace forward conversions: xyz_to_{lab, luv, oklab,
  jzazbz, ipt, igpgtg, icacb, osa_ucs, hunter_lab, prolab, xyy, ucs,
  hdr_cielab, hdr_ipt, ictcp_pq}; rgb_to_{hsv, hsl, hwb, hsy, hsp,
  ictcp_pq}; lab_to_{lch, din99}; jzazbz_to_jzczhz.
- 23 inverse conversions: {lab, oklab, ipt, jzazbz, igpgtg, icacb,
  hunter_lab, prolab, xyy, ucs, hdr_cielab, hdr_ipt}_to_xyz;
  {hsv, hsl, hwb, hsy, hsp, ictcp_pq}_to_rgb;
  oklab<->oklch; lch_to_lab; jzczhz_to_jzazbz; din99_to_lab.
- 10 CAM forward models (per-pixel correlates): cam16, zcam,
  ciecam02, hellwig2022, kim2009, hunt, llab, atd95, cam18sl,
  cam20u, all under fixed viewing conditions.
- 7 CAM inverses (cam16, zcam, ciecam02, hellwig2022, kim2009,
  cam18sl, cam20u) driven via forward+inverse round-trip XYZ.
- 18 view transforms via `alwan_view_transform_apply_f32` /
  `alwan_view_transform_apply_f64`: AgX (original/
  punchy/golden/SB2383/Blender 57³ 3D LUT), BT.2446 (Method A HDR↔SDR,
  B SDR→HDR, C HDR→SDR), BT.2390 HDR→SDR, Tony McMapface, Reinhard
  (Extended + Calibrated), Khronos PBR Neutral, Uchimura, Lottes,
  Exposure, ACES Rec.709. Plus JP2499 (parameterized analytic DRT via
  `alwan_jp2499_apply_f32/f64`).
- 16 ACES output transforms forward + 2 inverses: ACES 1.x (Rec.709,
  sRGB, Rec.2020 100nit/1000nit_PQ/4000nit_PQ, P3-D65, DCDM) + ACES
  2.0 (Rec.709, sRGB, P3-D65 100nit_sRGB/1000nit_PQ, Rec.2100
  1000nit_PQ/4000nit_PQ/1000nit_HLG, DCDM, P3-DCI).
- 8 gamut-mapping algorithms: clip, hue-preserving, adaptive_l0,
  adaptive_cusp, chroma_compress, sgck, hpminde, lightness_preserve.
- 19 transfer-function round-trips (oetf+eotf): ACEScc, ACEScct,
  S-Log / S-Log2 / S-Log3, C-Log / C-Log2 / C-Log3, V-Log, N-Log,
  REDLog / REDLogFilm, Protune, Cineon, BT.1886, Gamma 2.2 / 2.4 /
  2.6 / 2.8.
- 14 colour-difference metrics: delta_e_76 / 94 / 2000 / hyab / cmc /
  ok / din99 / itp / cam02_lcd / cam02_scd / cam02_ucs / cam16_lcd /
  cam16_scd / cam16_ucs.
- 4 CCT estimators (McCamy / Robertson / Hernandez-Andres / Kang)
  over an xy chromaticity sweep + cct_duv_optimize.
- 6 chromatic-adaptation transforms: XYZ scaling / Bradford / CAT02 /
  CAT16 / Sharp / Zhai2018 (two-step via E baseline).
- Hero wavelength sample + XYZ for spectral-rendering pipelines.
- Vision: 6 dichromacy / anomaly simulations (Machado et al. CVD
  model): protanopia, deuteranopia, tritanopia, and their
  *_anomaly weak variants.
- Prismatic colour {L, s, h} forward + roundtrip.
- Contrast Sensitivity Functions (CSF and Barten 1999) over
  spatial-frequency sweeps.
- SPD shape analysis: blackbody 2000K → 10000K → (peak_wavelength,
  peak_value, fwhm, centroid, bandwidth).
- Rayleigh scattering: cross_section + optical_depth across
  λ ∈ [360, 830] nm with default atmosphere parameters.
- Linear-sRGB convenience kernels: linear_srgb_to_{HSV, HSL}.
- Relative luminance (BT.709 default + custom kr/kb for BT.2020).
- White-balance multiply with asymmetric per-channel gains.
- f32 colorspace conversions (forward): xyz_to_{oklab, jzazbz, ipt,
  xyy, lab, luv}_f32 and rgb_to_{hsv, hsl}_f32; validates the f32
  polynomial path (alwan_det_pow_pos_f32) stays bit-stable across
  platforms.
- f32 colorspace conversions (inverse): {lab, oklab, jzazbz, ipt}_to_xyz_f32
  and hsv_to_rgb_f32.
- ACES LMT (Look Modification Transform): per-channel slope/offset/
  power + saturation, the ASC-CDL grading interface in the ACES
  pipeline.
- 5 colour-matrix presets: sepia, vintage, bleach_bypass, cool, warm;
  both coefficient lookup and applied output.
- Printer-lights apply: log-domain density adjustment (cinema dailies).
- TM-30 Rf colour fidelity index for D65 / A / F11 / E illuminants.
- Per-channel correction: lgg_apply (lift/gamma/gain) and
  color_matrix_apply (sepia preset).
- 9 RGB-to-RGB cross-space conversions: sRGB ↔ Display P3 / BT.2020 /
  ACEScg, ACEScg ↔ BT.2020, ACES2065-1 → ACEScg, BT.2020 → Display P3.
  Full EOTF → primaries → XYZ → primaries → OETF chain.
- SPD: Planckian blackbody integration. 256 temperatures (1500K
  to 12000K), each integrated against D65 over 360-830 nm with
  Simpson's rule and the CIE 1931 2deg observer.
- SPD round-trip: RGB → {Smits1999, Mallett2019, Jakob2019_sRGB}
  upsample → integrate against D65 → XYZ. Three different
  upsampler formulations all under cross-platform check.
- SPD resample (v36): linear and Catmull-Rom interpolation crossed
  with all three extrapolation modes (zero / constant / linear) over
  a 5000K blackbody source resampled onto a 350-740 nm 41-sample
  observer-aligned grid. Both the resample kernel and the
  extrapolation branch are individually pinned.
- SPD bandpass correction (v36): same blackbody sweep at 1, 5, and
  10 nm bandpass corrections; the polynomial bandpass branch inside
  `alwan_xyz_from_spd_f64` was previously only exercised at 0 nm.
- Camera sensitivities (v36): the embedded 471-sample 360-830 nm
  R/G/B spectral curves themselves for both `ALWAN_CAMERA_NIKON_5100`
  and `ALWAN_CAMERA_SIGMA_SDMERILL`, plus camera-space XYZ over a CCT
  sweep via `alwan_xyz_from_spd_camera_f64` with Simpson integration.
- Extended observers (v37): all 8 observers (CIE 1931 2deg, CIE
  1964 10deg, CIE 2012 2deg/10deg, Stockman & Sharpe 2deg, CIE 2015
  2deg/10deg, Wright & Guild 1931), each integrated against D65 over
  a 2000..10000K blackbody sweep with Simpson's rule. Pins each
  observer's embedded CMF table independently of illuminant choice.
- Additional illuminants (v37): 5 illuminants beyond D65 (A, D50,
  D55, F11, E). Both the raw illuminant SPDs (embedded CSV bytes)
  and full SPD->XYZ integration against the CIE 1931 2deg observer
  are pinned. A is incandescent, D50/D55 non-D65 daylight, F11
  tri-band fluorescent, E equal-energy; together they exercise
  visibly different spectral shapes through the same kernel.
- Quality metrics (v38): CRI Ra, CQS, CIE 224:2017 Rf, SSI, and
  metamerism index, joining the existing TM-30 Rf coverage so
  every entry in the alwan_quality module is now on the contract.
- LUT sampling + interpolation (v39): alwan_lut1d_sample_f64,
  alwan_lut3d_sample_f64, table_interp_3d trilinear + tetrahedral,
  table_interp_1d (LINEAR + CUBIC); alwan_interpolate_f64 over all
  6 methods (linear / cubic / lanczos / sprague / lagrange / akima)
  and alwan_extrapolate_f64 over all 6 modes; ColorChecker reference
  data (CLASSIC 24 patches; SG / DIGITAL_SG / BABELCOLOR_AVG / HCT
  enumerated for forward compatibility); Munsell renotation HVC grid.
- Machado CVD (v40): alwan_simulate_cvd_machado_f64 over all 6 CVD
  types at 3 severities, plus single-pixel direct + cvd_ex through
  both Brettel and Machado dispatch models.
- Photometry (v40): luminous_efficiency curves V(λ)/V'(λ)/V_mesopic
  over 360-830nm at 1nm; photopic / scotopic / mesopic luminance
  integration over the blackbody sweep; Barten 1999 sub-helpers
  (pupil_diameter, retinal_illuminance ± Stiles-Crawford, sigma,
  optical_mtf, maximum_angular_size).
- Image / video pipeline (v41): alwan_image_convert_f64 (sRGB to
  Display P3 + BT.2020 over a 16x16 F64 frame); image_convert_rgba
  with both ALWAN_ALPHA_STRAIGHT and ALWAN_ALPHA_PREMULTIPLIED;
  alwan_video_encode/decode_f64 over BT.709 8-bit and BT.2020 10-bit
  in both narrow and full range; alwan_bake_1dlut_f64 over six TFs
  (sRGB, BT.2020, PQ, HLG, BT.1886, gamma 2.2) at 256-entry
  resolution OETF + EOTF; alwan_bake_2dlut_f64 sRGB→P3 at edge=17;
  alwan_lut3d_to_2d / alwan_lut2d_to_3d round-trip on identity 17³.
- Extended transfer functions (v42): ACESproxy, ARRI LogC3 + LogC4,
  RED Log3G10, Blackmagic Film Gen 5 + Gen 4, FilmLight T-Log + Olympus E-Log,
  Apple Log, Fujifilm F-Log + F-Log2, Leica L-Log, DJI D-Log, DCDM,
  ADX 10-bit + 16-bit. Combined with the v18 set, the contract now
  covers every TF enum in alwan.h.
- Interop registry (v42): alwan_interop_count + alwan_interop_entry_at_f64
  walk over every registered RGB-space entry; for each entry pins
  (index, parsed_enum, first_id_char, parse_round_trip_ok,
  format_round_trip_ok). Catches drift in the registry layout +
  string-based lookup.
- f32 SPD pipeline (v42): alwan_spd_blackbody_f32 +
  alwan_xyz_from_spd_f32 over the same 1500..12000K sweep that
  pinned the f64 path in v33; alwan_spd_resample_f32 in linear
  / zero-extrapolate config. Independent f32 path (alwan_det_pow_pos_f32
  + scalar Simpson in float).
- Direct LCh + LCHuv ↔ XYZ (v43): alwan_xyz_to_lch_f64 +
  alwan_lch_to_xyz_f64 + alwan_xyz_to_lchuv_f64 +
  alwan_lchuv_to_xyz_f64. The single-call entry points that bypass
  the Lab/Luv intermediate were not previously on the contract.
- HLC roundtrip (v43): alwan_lch_to_hlc_f64 + alwan_hlc_to_lch_f64
  pure component reorder, round-trip pinned for regression-detection.
- NCS notation lookup (v43): alwan_ncs_to_xyz_f64 over 8 canonical
  NCS strings spanning the elementary-hue interpolation grid; on
  parse failure the dump still pins the return code so even
  malformed-string behavior is cross-platform stable.
- Missing colorspaces (v44): HCL, IHLS, Cubehelix, HSLuv, HPLuv,
  OkHSL, OkHSV, IPTCh, UVW, YCbCr (BT.601/709/2020), YCoCg,
  YccCbcCrc (8/10/12-bit), and RLAB (3 surrounds × forward+inverse).
  Every color-type declared in alwan.h is now reached by at least
  one conversion in the contract.
- f32 parity (v45): f32 versions of the v38–v44 surfaces (quality
  metrics, luminous_efficiency / photopic / scotopic luminance,
  LUT sampling, image_convert + bake_1dlut + video encode/decode,
  ColorChecker / Munsell / NCS reference data, direct LCh/LCHuv↔XYZ
  + HLC, and all 14 v44 colorspaces). Each is its own f32
  polynomial chain (alwan_det_pow_pos_f32 etc.); pinning these
  confirms cross-platform stability of the f32 surface in addition
  to f64.
- Pixel-format _ex paths (v46): alwan_srgb_to_xyz / sRGB→Lab /
  sRGB→Oklab via map_interleave_ex with full input-format sweep
  (U8/U16/F16/F32/F64) and output-format sweep on the store side.
  alwan_collect3_f64 + alwan_scatter3_f64 pinned across all five
  formats. This puts the load/normalize and store/quantize layer
  under contract: algorithmically deterministic by construction
  (integer arithmetic + division), but the dispatch table and
  per-format rounding rules are now pinned for regression
  detection.
- Newly-implemented f32 LUT interpolation (v47):
  `alwan_table_interp_3d_trilinear_f32` and
  `alwan_table_interp_3d_tetrahedral_f32` were declared in alwan.h
  but only the f64 variants existed; v47 ships the f32
  implementations (mirror of the f64 algorithm: pure arithmetic,
  inherently deterministic) and adds them to the contract.
- CAM viewing-condition sweep (v48): for CAM16, CIECAM02, ZCAM, and
  Hellwig2022, every parameter combination of (3 surrounds × 2
  discount_illuminant × 3 La values) = 18 viewing conditions per
  model, each driven across a 5-point XYZ test grid (white,
  neutral 50%, primary RGB). For RLAB: 3 surrounds × 4 D_factor
  variants. Surround DIM/DARK selects different exponent constants;
  discount_illuminant=1 toggles the chromatic-adaptation matrix;
  varied La changes FL adaptation scaling; all distinct VC code
  paths are now on the contract.
- _map_planar / _map_planar_ex dispatch (v49): typed planar
  variants of sRGB→XYZ, XYZ↔Lab (D65), XYZ↔Oklab, RGB↔HSV, and
  RGB↔YCbCr (BT.709). Plus _map_planar_ex with format conversion:
  sRGB→XYZ from U16 planes to F64 output (legacy in_fmt/out_fmt
  arg order), and XYZ→Lab from F64 to F32 (canonical
  out_fmt/in_fmt arg order). Confirms the planar entry points
  dispatch into the deterministic scalar kernel rather than
  specializing.
- ICtCp HLG (v34): forward + inverse with `use_pq=0` (the HLG
  transfer-function branch; PQ was already covered upstream).
- Integer normalization (v34): `uint_to_float` and `float_to_uint`
  at 8/10/12/16 bit, plus `float_to_half` + half-to-float roundtrip.
- f32 hot path (v35): `alwan_view_transform_apply_f32` for AgX and
  Tony McMapface, `alwan_aces1_output_transform_f32(REC709_100NIT)`,
  `alwan_aces2_output_transform_f32(SRGB_100NIT)`, and
  `alwan_cam16_forward_f32` (J / C / h). Independent of the f64
  surface; exercises `alwan_det_pow_pos_f32` and the f32
  trig/log primitives directly.

Adding a new conversion to the determinism contract: extend the dump
in `alwan_dev/det_regression/det_run_regression.c`, bump the manifest
version (currently v49), and include in the CI matrix. The
`alwan_dev/tests/88_simd_parity` suite is the local-iteration counterpart: it
asserts SIMD-vs-scalar parity on the same machine over 58 conversions,
while `det_run_regression` asserts cross-platform stability of the
scalar path across the full public API surface (~700 entry points,
407,365 dumped lines): 10 CAMs forward, 7 CAM
inverses, 14 dE metrics, 4 CCT estimators + duv optimisation, 6 CAT
transforms, 8 gamut-mapping algorithms, 18 view transforms, 5 ACES
output transforms + 2 inverses, 19 transfer-function families with
roundtrip, 9 RGB-to-RGB conversions, 3 spectrum upsamplers, blackbody
SPD integration + shape analysis, hero wavelength sampling, 2
contrast-sensitivity functions, 6 colour-vision deficiency
simulations, prismatic colour roundtrip, Rayleigh scattering,
linear-sRGB convenience wrappers, white-balance, relative luminance,
and the full TF + colorspace forward/inverse map-interleave grid.

Optimization levels: `-O0` through `-O3` all produce the same output.
The polynomials are written so that LICM, common-subexpression
elimination, and re-association under `-O3` cannot perturb the result,
because `-ffp-contract=off` blocks the rewrites that could.

Out of scope:

- **`-ffast-math` / `/fp:fast`.** Re-enables associativity rewrites,
  NaN/Inf shortcuts, and FTZ changes that break IEEE semantics. We
  don't try to recover those; if you compile alwan with `-ffast-math`,
  determinism is forfeit.
- **f128 / long double.** Stays f32/f64 only.
- **GPU backends (HLSL/GLSL/Halide/CUDA).** Out of scope.
- **Filesystem-level concerns**: filesystem timestamps, atime/mtime,
  ACLs, sparse-file behaviour, and arbitrary path encoding stay out
  of scope; those are OS-level concerns, independent of byte content.

  The byte content written by `alwan_clf_export_*` /
  `alwan_cube_export_*` *is* on the contract. Three pieces of
  hardening land that:
  1. All file output uses `"wb"` (binary fopen mode) to disable
     MSVC's text-mode CRLF translation.
  2. All numeric formatting runs under `LC_NUMERIC="C"` so a host
     locale that uses `,` as the decimal separator does not produce
     unparseable `.cube` / `.clf` output. The exporters save the
     caller's `LC_NUMERIC`, switch to `"C"` for the export block,
     and restore on every exit path.
  3. Numeric data uses `%.17g` (f64) / `%.9g` (f32), the
     round-trip-preserving format for IEEE 754, so the
     `cube_export → cube_import` cycle is exact.

  CI verifies the on-disk bytes match across the matrix: the
  determinism workflow builds a sibling `det_file_export` tool that
  writes a fixed test scene and asserts MD5 equality of each output
  file across all 6 runners (Linux x86_64, Linux aarch64, macOS
  x86_64, macOS aarch64, Windows x86_64, Windows aarch64). The
  scene exercises both precisions and both the path and in-memory
  buffer exporters:

  - `alwan_cube_export_3d_f64`: sRGB→BT.2020 3D LUT at edge 17
    (`%.17g`), plus the `_buffer_f64` variant.
  - **`alwan_cube_export_3d_f32`**: a fixed f32 ramp at edge 17,
    pinning the **native-f32 cube writer** (`%.9g`) on the contract,
    plus the `_buffer_f32` variant.
  - `alwan_cube_export_1d_f64`: identity 1D LUT at 64 entries.
  - `alwan_clf_export_*`: two CLF ProcessLists.

  So byte-identical `.cube` / `.clf` export (including native-f32
  cube export) is part of the determinism contract, gated by MD5
  equality of the on-disk bytes in addition to the in-memory
  numeric guarantee.

---

## Performance cost

Indicative numbers from the in-tree microbenchmark (`alwan_dev/bench`)
on a 4-core x86_64 / SSE2 build:

| Operation                   | Fast mode  | Det mode  | Cost  |
| :-------------------------- | :--------- | :-------- | :---- |
| `srgb_oetf` (1M pixels)     | 11.2 ms    | 12.8 ms   | +14%  |
| `xyz_to_oklab` (1M pixels)  | 18.3 ms    | 24.1 ms   | +32%  |
| `xyz_to_jzazbz` (1M pixels) | 21.7 ms    | 23.9 ms   | +10%  |
| `pq_oetf` (1M pixels)       | 14.6 ms    | 17.2 ms   | +18%  |
| Matrix-only conversions     | unchanged  | unchanged | 0%    |

(Numbers are ballpark; your hardware may vary.)

Most user pipelines see <20% total slowdown. Pipelines dominated by
Oklab cube-root or IPT (which fall back to per-pixel scalar in det mode)
see closer to 30–40%.

If perf in det mode matters more than the contract: the design space
is open for "deterministic-with-vectorised-kernel" variants that route
SIMD reductions canonically but keep the vectorised polynomial in the
hot path. We chose the strict path because (a) the contract surface is
simpler to reason about, and (b) the kernels that lose SIMD aren't on
the critical path for most workloads.

---

## Design decisions

Four choices, with the consequence a caller can actually observe. None is
reversible without breaking the bit-exact contract for existing users.

**Own polynomials, not a deterministic libm.** A wrapper around CRlibm or musl
would tie the build to a libm fork that may not exist on the target, and
CRlibm's licence is viral for MIT consumers. The Chebyshev minimax fits give
1e-12-class accuracy in a fraction of the code. The consequence to know:
`alwan_det_pow_pos` is not bit-equal to libm `pow`, so a baseline generated
against libm (a Python script using `math.pow`, say) shifts by a fixed,
reproducible amount when deterministic mode is enabled.

**Polynomials are fitted in a normalised basis.** Fitting in the user domain
and converting to the power basis produced coefficients up to 3.7e17: fine in
f64, lost in an f32 mantissa. Pre-normalising x to `u` in `[-1, 1]` before the
fit puts the coefficients in the 0.2 to 0.8 range and the f32 path agrees with
f64 to 1.8e-7. Anyone regenerating the tables has to keep that normalisation.

**Reductions are plain left-to-right, not Kahan.** The arrays are 4 to 16 lanes
wide, and even a 1024-sample SPD reduction accumulates about 1e-13 absolute,
far below any tolerance here. Kahan would cost around 30% on the reduction-bound
paths to fix an error nothing measures.

**Approximate SIMD kernels unpack to the scalar polynomial.** In deterministic
mode `pow24` and `cbrt_fast` go lane by lane through the same polynomial the
scalar path uses, rather than each SIMD backend carrying its own deterministic
implementation. One implementation, one regression target, and the cost is paid
only by callers who opted in. A vectorised-deterministic mode could re-emit the
polynomial per backend later; the architecture allows it.

---

## Portable checklist: applying this to another library

The mechanics above are alwan-specific, but the underlying recipe is
general. If you are making any C/C++ numerical library produce
byte-identical output across compilers, optimization levels, and
IEEE-754 CPU architectures, this is the distilled checklist. Each
rule has a concrete observable failure mode if you skip it.

### The math layer

1. **Replace `libm` transcendentals with polynomials.** `pow`, `exp`,
   `log`, `log2`, `cbrt`, `sin`, `cos`, `atan2`: every platform's
   `libm` differs in the last 1–3 ULPs. Ship your own: `frexp` /
   `ldexp` argument reduction (bit-deterministic on IEEE-754) plus a
   Chebyshev minimax polynomial fitted to the *normalised* domain
   (pre-normalise x to `[-1, 1]` before fitting, or f32 coefficients
   blow up; see *Polynomial fitting in normalised basis* above).
2. **Domain-split minimax polynomials for piecewise transfer
   functions** (sRGB / BT.2020 / BT.709 / PQ / HLG). One global
   polynomial loses precision near the segment boundary.
3. **Disable FMA contraction.** `a*b + c` rounds twice as `mul`+`add`,
   once as a fused FMA, and the choice is platform-dependent
   (mandatory on aarch64, opt-in on x86 with `-mfma`). Force two
   roundings with `-ffp-contract=off` (gcc/clang) or `/fp:precise`
   (MSVC), and define your `FMA(a,b,c)` macro as `((a)*(b)+(c))` so
   the compiler can't re-fuse.
4. **Route all math through one macro layer** (`POW / EXP / LOG2 / …`)
   and add a CI grep that fails the build on raw `libm` calls in your
   `*.c` (alwan: `check_no_raw_libm.py`). Otherwise a contributor
   eventually calls `libm` directly and the contract silently regresses.

### The SIMD layer

5. **Canonical scalar reduction order for horizontal sums.** `hsum`
   differs by lane width: SSE2 `((v0+v1)+(v2+v3))`, AVX hierarchical
   8-way, NEON `vaddvq_*` implementation-defined. In det mode route
   every horizontal sum to a scalar left-to-right `(((v0+v1)+v2)+v3)+…`.
6. **Lane-unpack instead of re-vectorising.** Same-op-per-lane work
   (add, mul, polynomial evaluation) *stays vectorised*: same op per
   lane = same bits regardless of width. Cross-lane work (reductions,
   branching, matrix-mul codegen drift) *collapses to scalar*: set
   `MAP_SIMD_WIDTH = 1` so kernels fall through their scalar tail loop.
7. **Keep arithmetic forms identical between SIMD and scalar.** If
   scalar uses `x / 10000.0`, SIMD must too: `x * (1.0/10000.0)`
   differs by 1 ULP because `1/10000` isn't exactly representable.
   Same hazard for literal constants the scalar path computes at
   runtime (`c = 0.5 - a*ln(4a)` vs a baked `0.55991072952956202`).
   The scalar formula is the source of truth; the SIMD path matches it
   bit-for-bit or it doesn't run.
8. **Provide explicit `mask_and` / `mask_or` helpers.** On SSE/AVX the
   mask type and value type are both `__m128`/`__m256`, so misusing
   `select(mask, mask, value)` as a mask AND compiles by accident. On
   NEON `float32x4_t` and `uint32x4_t` are distinct; treat the NEON
   build as a free static analyser for SIMD-type misuse (this is how
   the gamut-kernel select-vs-mask bug surfaced).

### The file-I/O layer

9. **Binary-mode I/O** (`fopen(path, "wb"/"rb")`, write `"\n"`
   explicitly) to defeat MSVC text-mode CRLF translation.
10. **Locale guard.** Save `LC_NUMERIC`, set `"C"`, restore on every
    exit path; a host `LC_NUMERIC=de_DE` writes `"0,5"` and poisons
    every consumer.
11. **Lossless numeric formatting**: `%.17g` for f64, `%.9g` for f32,
    so `parse(format(x)) == x`.
12. **No timestamps, hostnames, or PIDs** in output formats. Audit
    `time(NULL)` / `__DATE__` / `gethostname` in formatters.

### Platform-specific watch-list

- **aarch64 hardware FMA is mandatory**: `-ffp-contract=off` is what
  forces the source-level `(a*b)+c` to a separate `fmul`+`fadd`.
- **Apple Silicon flush-to-zero** (FPCR FZ=1) flushes denormals; a
  small-magnitude-only hash diff is the prime suspect (see
  *Denormal flushing* above).
- **MSVC ARM** has no NEON path; it builds `MAP_SIMD_WIDTH=1` always.
  Watch `C4189`/`C4101` on locals only used in the (now-dead) SIMD
  branch; `/WX` will fail the build. Suppress with a `DIAG_PUSH/POP`
  pragma or `(void)var;`.

### Failure modes mapped to fix

| Hazard | Symptom | Fix |
| :--- | :--- | :--- |
| `libm pow`/`log` precision | 1–3 ULP cross-platform drift | Polynomial + `frexp`/`ldexp` + Chebyshev minimax |
| FMA contraction | 0.5 ULP delta on FMA hardware | `-ffp-contract=off`; non-fusing `FMA` macro |
| SIMD horizontal-sum order | 1–2 ULP delta varying with lane width | Canonical scalar left-to-right reduction |
| SIMD per-lane libm | 1–3 ULP delta | Lane-unpack to deterministic polynomial |
| Pre-computed reciprocal | 1 ULP delta (`mul·inv` vs `div`) | Same form in both paths, or lane-unpack |
| Locale-dependent `%g` | `,` vs `.` in output files | `setlocale(LC_NUMERIC,"C")` save/restore |
| MSVC text-mode CRLF | `\r\n` instead of `\n` | `fopen(…, "wb")`; write `"\n"` explicitly |
| Non-lossless `%g` precision | round-trip parse/format loses bits | `%.17g` (f64) / `%.9g` (f32) |
| Apple Silicon FZ | subnormal inputs become `0` | Clear FZ at init, or stay above denormal threshold |
| NEON type strictness | x86 compiles, NEON fails | Explicit `mask_and` helpers; NEON as static analyser |

### A reasonable order of execution from scratch

1. **Stand up the CI matrix first**: you need same-input cross-platform
   diffs to know what's actually broken.
2. **Add the math-macro layer + raw-libm-call lint** to lock the surface
   before swapping implementations.
3. **Replace libm primitives** under a build flag; verify against libm
   in fast mode within a few-ULP budget, against itself in det mode at
   0 ULP.
4. **Disable FMA contraction**: one flag, easy regression to spot.
5. **Collapse SIMD to scalar in det mode** (`MAP_SIMD_WIDTH=1`):
   largest perf hit but trivially correct.
6. **Audit horizontal-sum sites and SIMD apply functions**: canonical
   reductions; lane-unpack to scalar polynomial.
7. **Harden file-I/O surfaces**: `fopen` mode, locale guard, precision,
   no timestamps.
8. **Cross-platform diff sweep** until the matrix is empty; each
   remaining diff is a hazard you missed.
9. **Add SIMD-vs-scalar parity tests** for the workhorse kernels.
10. **Document what's out of scope** (see above) so users compiling
    with `-ffast-math` or targeting GPU backends know the contract
    doesn't hold there.

---

## References

Background reading on the techniques used:

- Bruce Dawson, *Comparing Floating Point Numbers, 2012 Edition*. The
  ULP-distance via integer-cast canonicalization comes from here.
  [randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition](https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/)
- N. J. Higham, *Accuracy and Stability of Numerical Algorithms* (2002).
  Chapter 2: ULP/eps reasoning. Chapter 4: summation accuracy. The
  rule of thumb `N ≈ 2·sqrt(operations) + 4` for ULP budgets is from
  Chapter 2.
- William Kahan, *Lecture Notes on the Status of IEEE Standard 754*.
  The original argument for taking floating-point determinism seriously.
- Sollya, [sollya.org](https://www.sollya.org/). Industrial-strength
  minimax polynomial generator. We don't ship a Sollya dependency
  (the gendata scripts use scipy as a fallback), but Sollya is the
  reference if you ever want to re-derive the coefficients with
  proven minimax error.
- ARM Architecture Reference Manual, Section A1.5.6 (FPCR / FZ flag).
  Why Apple Silicon flushes denormals.
- The engineering history lives in
  [`road_to_determinism.md`](../road_to_determinism.md) at the repo root: how
  each source of drift was found and closed, in the order it happened.
