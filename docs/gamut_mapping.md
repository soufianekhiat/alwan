# Gamut mapping in alwan

## Policy: no silent gamut clamping

Raw conversion / simulation functions perform the **pure standard math** and
preserve out-of-gamut results. Functions that used to clamp implicitly now have
an explicit split:

```
name(...)                                  raw math, no gamut clamp
name_gamut_safe(..., alwan_gamut_map_method m)   raw + gamut mapping, output guaranteed in gamut
```

`ALWAN_GAMUT_MAP_CLIP` reproduces the old implicit clamping exactly.

Split functions (v2.0.0):

| raw function | gamut-safe twin | notes |
|---|---|---|
| `alwan_ycbcr_to_rgb` (+`_map_interleave`) | `alwan_ycbcr_to_rgb_gamut_safe` | raw preserves xvYCC super-black/-white excursions |
| `alwan_yccbccrc_to_rgb` (+`_map_interleave`) | `alwan_yccbccrc_to_rgb_gamut_safe` | BT.2020-CL inverse, raw output is linear |
| `alwan_simulate_cvd` / `_machado` / `_ex` (+`_map_interleave`) | `alwan_simulate_cvd[_machado/_ex]_gamut_safe` | Brettel / Machado math has no clamp; saturated inputs land outside [0,1] |

Kept-clamped by contract, with an explicit raw variant instead:

| display-referred function | raw variant |
|---|---|
| `alwan_view_transform_apply` (per-channel tone mappers) | `alwan_view_transform_apply_unclamped` |
| `alwan_aces2_output_transform_custom` | `alwan_aces2_output_transform_custom_display_linear` (pre-encode: no [0,peak] clamp, no OETF) |

Not split (the clamp *is* the operation): `alwan_gamut_map_advanced` (the
guarantee is the point), AgX picture formation (log guard-rail + sigmoid
codomain are definitional), OETF encode-domain clamps, u8/u16 quantization,
legal-range scaling, numerical guards.

## Choosing a method (`alwan_gamut_map_method`)

| method | mechanism | preserves | use when | cost |
|---|---|---|---|---|
| `CLIP` | per-channel clamp | nothing perceptual | final encode; content already ~in gamut | trivial |
| `HUE_PRESERVING` | RGB scale toward neutral | RGB channel ratios | real-time paths where Oklab cost is too high | low |
| `ADAPTIVE_L0` | Oklab project toward L=0.5 | hue; balances L/C | general-purpose photographic default | medium |
| `ADAPTIVE_CUSP` | Oklab project toward hue cusp | hue; max chroma at cusp | saturated graphics, logos, brand colors | medium |
| `CHROMA_COMPRESS` | reduce C, hold L and h | lightness + hue | hue fidelity paramount (CSS-like) | medium |
| `SGCK` | knee-compressed segment map | gradient smoothness | wide→narrow images with smooth ramps | medium |
| `HPMINDE` | min ΔE on hue leaf | colorimetric closeness | proofing / soft-proof (smallest visible error) | medium |
| `LIGHTNESS_PRESERVE` | hold L, sacrifice C | lightness contrast | text overlays, skin tones | medium |

Honesty notes:
1. The perceptual (Oklab) boundary model is **sRGB-anchored**
   (`core/alwan_gamut_core.inc`): for targets wider than sRGB the six Oklab
   methods over-compress. Use `alwan_css_gamut_space` for wide-gamut targets.
2. The methods are SDR-oriented. For HDR (absolute nits) use
   `alwan_hdr_gamut_map_ictcp`.

## The gamut-mapping family

| API | space / domain | algorithm |
|---|---|---|
| `alwan_gamut_map_advanced_{f32,f64}` | any RGB space desc, SDR | 8 methods above (Oklab model) |
| `alwan_gamut_{f32,f64}_map_interleave/_planar/_ex` | sRGB, SDR, bulk | same 8 methods |
| `alwan_css_gamut_*` (bulk) | sRGB target | CSS Color 4: Oklch chroma reduction, deltaEOK JND |
| `alwan_css_gamut_space_{f32,f64}` | **any D65 RGB target** (P3, Rec.2020…) | CSS Color 4 algorithm against the target cube |
| `alwan_hdr_gamut_map_ictcp_{f32,f64}` | linear BT.2020, **absolute nits**, peak 1–10000 | chroma reduction in ICtCp (PQ): I clamped to display range, hue angle preserved, binary search to the boundary, ΔE-ITP JND early-out |
| `alwan_aces_gamut_comp13` (+inv) | ACES AP1 | ACES 1.3 Reference Gamut Compression (per-CMY distances) |
| `alwan_aces_gamut_compress20` (+inv) | JMh, per-hue cusp table | ACES 2.0 output-transform compression |
| `alwan_gamut_map_xyz_to_rgb` | XYZ → any space | space-aware convenience |

### `alwan_hdr_gamut_map_ictcp` properties (tested, `tests/96_hdr_ictcp_gamut.c`)

- output always inside `[0, peak]³` (10k randomized inputs × peaks 100/1000/4000/10000);
- in-volume inputs pass through **bit-exactly**; mapping its own output is a fixpoint;
- hue angle `atan2(Ct, Cp)` preserved on the search path (Ct/Cp scaled jointly);
- chroma never increases;
- f32 tracks f64.

There is no external reference implementation for this mapper; it is
property-validated by construction (CSS-Color-4-shaped, in the BT.2100
appearance space, ΔE-ITP per BT.2124).
