# Alwan Future

Forward-looking notes for work that is still intentionally outside the current
public release.

This file is not an API reference. It records roadmap themes after accounting
for what already exists in the current repository.

---

## What Already Exists

The current codebase is further along than older roadmap drafts assumed.

Implemented foundations already in this repo include:

- `alwan_platform.h` backend detection and shared math/normalization layer
- bootstrap headers for `C`, `HLSL`, `GLSL`, and `Halide`
- header-only core modules under `src/alwan/core/`
- explicit `_f32` / `_f64` public API in `alwan.h`
- interleaved, planar, and typed-pixel batch frontends
- image-level RGB conversion helpers
- LUT bake / sample / import / export helpers
- CLF export
- interop ID parsing / formatting / enumeration
- integer normalization helpers
- video signal encode / decode helpers

Because of that, future work should focus on the remaining gaps instead of
re-describing the existing foundation.

---

## Remaining Roadmap Themes

### 1. Runtime / On-Demand Data Loading

Still planned, not implemented.

Desired end state:

- `ALWAN_EMBED_DATA=0` becomes a supported build
- `runtime_data_root` becomes meaningful
- embedded and runtime data paths expose the same public descriptors/getters

### 2. Richer Interop Metadata

The current interop layer can already:

- parse IDs
- format IDs
- enumerate IDs

Still missing:

- richer metadata queries such as scene/display/basic/HDR classification
- registry-style helper structs for UI-facing introspection
- clearer data-semantic propagation through conversion entry points

### 3. Backend-Facing Examples And Guidance

The repo already includes:

- `alwan_hlsl.h`
- `alwan_glsl.h`
- `alwan_halide.h`

Future work here is mostly documentation and workflow polish:

- end-to-end shader examples using the current bootstraps
- clearer guidance on which `*_core.h` modules are stable to share across
  backends
- a narrower story around what is public-facing versus experimental

### 4. Convenience Queries And Ergonomics

Possible additions:

- richer colorspace metadata helpers
- display/scene/HDR/basic query helpers
- more convenience wrappers around common descriptor-driven workflows

These are ergonomic improvements, not architectural prerequisites.

---

## Things This File No Longer Treats As Future Work

These items used to appear in older planning notes, but they are already
present in the current codebase:

- platform abstraction headers
- typed map frontends
- interop ID parsing
- integer normalization helpers
- video range helpers
- LUT/CLF export surface

Any new planning should start from the current header, not from older drafts
that predate those implementations.

---

## Working Checklist

- [ ] Support `ALWAN_EMBED_DATA=0` as a real runtime-data mode
- [ ] Add richer interop metadata/query APIs
- [ ] Thread data semantics through more public conversion workflows
- [ ] Publish tighter GPU/backend examples around the current bootstrap headers
- [ ] Add ergonomic helpers only where they reduce real call-site boilerplate

---

## Rule For Future Roadmap Updates

When updating this file:

1. check `src/alwan/alwan.h` first
2. move implemented items out of the roadmap
3. keep this file focused on real remaining gaps

That keeps `docs/` aligned with the code instead of preserving stale plans.
