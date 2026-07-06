# Spatial picture-formation gamut mapping — formulation (DRAFT, pre-implementation)

Status: design only, not implemented. A new, image-aware gamut-mapping method
that fits out-of-gamut records into the display volume while preserving local
structure (no increment↔decrement polarity flips), derived from the Troy Sobotka
exchange and Kitaoka (2005) transparency model.

## 1. Problem

Input image `I`: pixels `x_i ∈ ℝ³` (linear RGB records), some outside the cube
`[0,1]³` (or `[0, peak]³` for HDR).

Output `I'` with every record inside the cube, such that:
- **P1 — no polarity flip (hard):** for every neighbour pair `i~j` and channel
  `k`, `sign(x'_i[k] − x'_j[k]) = sign(x_i[k] − x_j[k])` (equality/collapse
  allowed, reversal forbidden). This is the convex per-channel proxy for Troy's
  `max(RGB)`/`min(RGB)` envelope preservation.
- **P2 — structure fidelity:** local differences (the increments/decrements)
  stay as close as possible to the originals.
- **P3 — smoothness:** the correction spreads spatially instead of clipping one
  sample flat; controlled by a user knob `s`.
- **P4 — bounded:** `x'_i ∈ [0,1]³`; gradients collapse toward the bounds.

## 2. Energy

Solve per channel `k` (per-channel keeps polarity per-channel = the X-junction
per-edge rule):

```
E(u) =  Σ_i  λ_i · (u_i − x_i)²                          [fidelity, spatial weight]
     +  β · Σ_{i~j} w_ij · ((u_i − u_j) − c · g_ij)²      [gradient preservation]
```
subject to
```
u_i ∈ [0,1]                                              (box / gamut)
(u_i − u_j) · sign(g_ij) ≥ 0   ∀ i~j                     (P1, monotonicity)
```
with `g_ij = x_i[k] − x_j[k]` (input gradient) and `0 < c ≤ 1` a gradient
compression factor (`c = 1` = preserve; `< 1` = actively collapse — Kitaoka's
`α ≤ 1`).

Quadratic energy + linear/box constraints ⇒ **convex QP** ⇒ unique minimum.

### 2.1 The spatial spread weight `λ_i` (the `s` knob)

Let `Ω = { i : x_i ∉ [0,1]³ }` be the out-of-gamut set.
- `D_i` = spatial Euclidean (L2) distance from pixel `i` to the nearest pixel in
  `Ω` (exact EDT, deterministic, O(n)).
- Normalize: `D̂_i = min(D_i / R, 1)`, `R` = spatial reach (default: image
  diagonal, or a user radius in pixels).
- Weight:
  ```
  λ_i = λ0 · smoothstep( (D̂_i − (1 − s)) / feather )
  ```
  - `s = 1` → threshold at 0 → every in-gamut pixel locked → only `Ω` moves
    (clip-like, kinks).
  - `s = 0` → all pixels free → global smooth compression (film-like).
  - `0 < s < 1` → pixels within spatial band of the trouble are free; a
    `smoothstep`/Gaussian `feather` avoids a sharp transition.
- `Ω` pixels themselves get `λ_i = 0` (fully free — they must move).

`s` interpolates **clip (1) ↔ tone-form (0)**; Troy-valid results live below 1.

### 2.2 Weights and compression
- `w_ij` = base weight, optionally reduced across large input gradients
  (bilateral) to let genuine edges stay sharp. See 2.3 for depth gating.
- `β` = structure-vs-fidelity balance (default ~ a few × λ0).
- `c` = 1 for v1 (box + solve do the collapse); expose later.

### 2.3 Depth = the spatial envelope (user-provided, optional)

For opaque content, a user depth (or object-label) map `z_i` defines the
envelope boundaries. A gradient across an occlusion boundary is not a surface
gradient (two different volumes meeting), so we must not couple across it. Depth
gates two things:

- **Edge coupling:** `w_ij ← w_ij · f(|z_i − z_j|)`, `f → 0` across a depth jump
  (`exp(−(Δz/σ)²)` or a hard threshold). Across an occlusion boundary there is
  then no gradient-preservation and no polarity constraint — each object is
  formed independently.
- **Spread:** the spatial distance `D_i` (for `s`) is measured *within* a depth
  region — geodesic on the image but blocked by depth edges — so a correction on
  the foreground never bleeds onto the background it occludes.

So depth realizes Troy's "envelope = segmented region" for the opaque case, with
the segmentation supplied by the user. Optional: no depth ⇒ one global envelope.
Depth's *transparency* role (disambiguating bistable front/behind for overlapping
layers) is phase 2 (§6) — there depth cannot cut pixels spatially, it only
selects the decomposition.

## 3. Solver (deterministic)

Projected iterative solve, fixed iteration count (no data-dependent tolerance →
survives the determinism harness):

1. **Init** `u = clamp(x, 0, 1)`.
2. Repeat `N` times (fixed):
   a. **Jacobi** update of the unconstrained linear system (order-independent →
      bit-exact cross-platform; avoid Gauss-Seidel's traversal-order dependence,
      or use a fixed red-black ordering).
   b. **Box projection**: `u_i ← clamp(u_i, 0, 1)`.
   c. **Polarity projection**: for each pair `i~j` that violates P1, project onto
      the half-space `(u_i − u_j)·sign(g_ij) ≥ 0` (nearest point = set both to
      their mean when flipped).
3. Return `u` per channel, reassemble RGB.

Notes:
- Convex feasible set (box ∩ half-spaces) is non-empty (`u ≡ const` or the
  clamped input's monotone hull), so projected iteration converges.
- Optional multigrid / reduced-res gain field for 4K perf — keep single-scale
  for v1 to protect determinism.
- Per-channel independence can drift hue; acceptable for v1 (matches the
  per-edge polarity reasoning). A coupled/opponent-space or added chroma-gradient
  term is a later refinement.

## 4. API (new — image-aware, not a pixel/stride map)

```c
typedef struct {
    alwan_f32 s;          /* [0,1] spatial spread; 1=only OOG, 0=whole image */
    alwan_f32 reach;      /* R in pixels; <=0 => image diagonal */
    alwan_f32 beta;       /* structure vs fidelity */
    alwan_f32 compress;   /* c in (0,1]; 1 = preserve gradients */
    alwan_f32 depth_sigma;/* depth-jump softness; <=0 => ignore depth */
    int       iterations; /* fixed, for determinism */
    alwan_f32 peak;       /* cube upper bound; 1.0 for SDR */
} alwan_gamut_spatial_params_f32;   /* + _f64 twin */

/* `depth` is optional (NULL => single global envelope). One scalar per pixel
   (z or object id); used to gate edge coupling + spread across occlusion
   boundaries (see 2.3). */
int alwan_gamut_map_spatial_f32(alwan_f32 *out, const alwan_f32 *in,
                                const alwan_f32 *depth /* or NULL */,
                                int width, int height,
                                const alwan_gamut_spatial_params_f32 *params);
/* + _f64. Needs w,h (neighbours) => distinct from the pixel-independent
   alwan_gamut_map_method enum. */
```

## 5. Property tests (no external reference — validate by construction)

- **In-gamut:** every output record ∈ [0, peak]³.
- **Zero polarity flips:** per-channel neighbour sign-reversals == 0. Run the
  same counter on the 8 enum methods for a comparison table (expected: CLIP /
  HUE_PRESERVING safe; Oklab projections flip near the boundary — empirically
  confirms Troy's claim).
- **Energy monotone:** `E` never increases across iterations.
- **`s` endpoints:** `s=1` ⇒ in-gamut pixels unchanged (only `Ω` moves);
  `s=0` ⇒ global smooth compression, still monotone.
- **Identity:** fully in-gamut input ⇒ output == input (bit-exact).
- **Determinism:** bit-exact across platforms (Jacobi/red-black, fixed iters).
- **f32 vs f64** parity within budget.

## 6. Later (phase 2) — segmentation via Kitaoka X-junction test

For transparency/layered content, cut the image into coherent layers first,
using the luminance-arithmetic X-junction test at 4-region junctions
(`a,b` background, `p,q` seen-through):
```
α = (p − q)/(a − b),   t = (a·q − b·p)/(a − b)
valid layer  ⇔  0 ≤ α ≤ 1  and  t ≥ 0     (order-preserving affine contraction)
polarity preserved on 1 edge / both / none  ⇒  unique / bistable / opaque
```
Depth ordering for the bistable case is **user-provided** (authorial intent;
no closed-form solution — Kitaoka's experiment: vision prefers *object* over
*full-layer* transparency, driven by mean luminance + luminance difference, not
Michelson contrast). v1 skips segmentation: single envelope = whole image.
