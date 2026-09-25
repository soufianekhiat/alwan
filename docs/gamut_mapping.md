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
| `ADAPTIVE_L0` | Ottosson's gamut_clip_adaptive_L0_0_5 (alpha 0.05): Oklab, toward an L0 that moves from 0.5 toward the colour's own lightness as chroma falls | Oklab hue; balances L/C | general-purpose photographic default | medium |
| `ADAPTIVE_CUSP` | Ottosson's gamut_clip_adaptive_L0_L_cusp (alpha 0.05): the same about the cusp's lightness | Oklab hue; more chroma | saturated graphics, logos, brand colors | medium |
| `CHROMA_COMPRESS` | the same projection as `LIGHTNESS_PRESERVE` | Oklab lightness + hue | kept for the enum | medium |
| `SGCK` | CIE 156:2004 SGCK in CIELAB, as a clip along its mapping line toward the cusp's lightness (a single colour carries no source gamut for the knee) | CIELAB hue | CIE 156 comparisons | high (a search) |
| `HPMINDE` | CIE 156:2004: the in-gamut colour of the same CIELAB hue with the least dE*ab | CIELAB hue; colorimetric closeness | proofing (smallest visible error) | high (a search) |
| `LIGHTNESS_PRESERVE` | Ottosson's gamut_clip_preserve_chroma: hold Oklab L, cut chroma | Oklab lightness + hue | text overlays, skin tones | medium |
| `RAYTRACE` | Oklch chroma reduction, rays cast to the target's linear cube | Oklch lightness + hue | any target, wide gamuts included; ColorAide's default | medium |
| `CSS4` | CSS Color 4 binary search on Oklch chroma, JND 0.02 | Oklch lightness + hue, within a JND | matching browsers' CSS gamut mapping | medium-high |

Honesty notes:
1. Ottosson's four methods (2, 3, 4, 7) use his **sRGB** boundary model
   (`core/alwan_gamut_core.inc`): for targets wider than sRGB they over-compress.
   `SGCK`, `HPMINDE`, `RAYTRACE` and `CSS4` work in the target's own cube.
   Until 2026-09-25 methods 3, 5 and 6 were one projection toward the Oklab cusp and
   4 and 7 another, the alpha values were unused and the boundary intersection's
   Halley step had wrong derivatives (suite 264).
2. The methods are SDR-oriented. For HDR (absolute nits) use
   `alwan_hdr_gamut_map_ictcp`.

## The gamut-mapping family

| API | space / domain | algorithm |
|---|---|---|
| `alwan_gamut_map_advanced_{f32,f64}` | any RGB space desc, SDR | 10 methods above (Oklab model; RAYTRACE and CSS4 in the target's cube) |
| `alwan_gamut_{f32,f64}_map_interleave/_planar/_ex` | sRGB, SDR, bulk | CLIP and HUE_PRESERVING |
| `alwan_css_gamut_*` (bulk) | sRGB target | CSS Color 4: Oklch chroma reduction, deltaEOK JND |
| `alwan_css_gamut_space_{f32,f64}` | any RGB target (P3, Rec.2020...), Bradford from D65 | CSS Color 4 algorithm against the target cube, = `ALWAN_GAMUT_MAP_CSS4` |
| `alwan_hdr_gamut_map_ictcp_{f32,f64}` | linear BT.2020, **absolute nits**, peak 1-10000 | chroma reduction in ICtCp (PQ): I clamped to display range, hue angle preserved, binary search to the boundary, DeltaE-ITP JND early-out |
| `alwan_aces_gamut_comp13` (+inv) | ACES AP1 | ACES 1.3 Reference Gamut Compression (per-CMY distances) |
| `alwan_aces_gamut_compress20` (+inv) | JMh, per-hue cusp table | ACES 2.0 output-transform compression |
| `alwan_gamut_map_xyz_to_rgb` | XYZ -> any space | space-aware convenience |

### `alwan_hdr_gamut_map_ictcp` properties (tested, `tests/96_hdr_ictcp_gamut.c`)

- output always inside `[0, peak]^3` (10k randomized inputs x peaks 100/1000/4000/10000);
- in-volume inputs pass through **bit-exactly**; mapping its own output is a fixpoint;
- hue angle `atan2(Ct, Cp)` preserved on the search path (Ct/Cp scaled jointly);
- chroma never increases;
- f32 tracks f64.

There is no external reference implementation for this mapper; it is
property-validated by construction (CSS-Color-4-shaped, in the BT.2100
appearance space, DeltaE-ITP per BT.2124).
