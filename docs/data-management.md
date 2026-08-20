# Data Management

How Alwan handles its reference datasets today, and what "runtime loading"
still means in the roadmap.

---

## Current Reality

Alwan depends on a large body of reference data:

- colour-matching functions
- illuminants
- RGB space descriptors and matrices
- spectral basis data
- fixtures and lookup tables used by higher-level APIs

Today, the supported public mode is:

```c
#define ALWAN_EMBED_DATA 1
```

That means:

- CSV-derived data is compiled into the library
- public getters return pointers into embedded data or library-owned caches
- there is no supported file-backed runtime loader yet

`ALWAN_EMBED_DATA=0` remains a planned mode only.

---

## Where The Data Lives

The canonical source files live under `src/alwan/data/`.

Representative directories:

```text
src/alwan/data/cmf/
src/alwan/data/illuminants/
src/alwan/data/rgb_spaces/
src/alwan/data/rgb_matrices/
src/alwan/data/spectral_basis/
src/alwan/data/spectral_lut/
```

These CSV files are then embedded into the build through generated or included
tables in the library sources.

---

## Public Access Pattern

Data is accessed through API helpers in `alwan.h` rather than by reaching into
`src/alwan/data/` at runtime.

Examples:

```c
alwan_f64 *xy = NULL;
size_t count = 0;

if (alwan_data_get_illuminant_xy_f64(
        &xy, &count, ALWAN_ILLUMINANT_D65, ctx) == ALWAN_OK) {
    /* xy[0] = x, xy[1] = y */
}
```

```c
alwan_rgb_space_desc_f64 desc;
if (alwan_rgb_get_space_descriptor_f64(
        &desc, ALWAN_RGB_SPACE_DISPLAY_P3, ctx) == ALWAN_OK) {
    /* descriptor populated from the embedded registry */
}
```

For most applications, RGB-space lookup is the main practical consumer of the
embedded dataset.

---

## Table Readers

Tables that are *sampled* at a coordinate (curves, cubes, coefficient grids)
are read through a reader rather than by indexing the array. The readers are
declared in `alwan.h`; the definitions live beside them in
`src/alwan/data/alwan_data_tables*.c`.

```c
alwan_f64 v;
/* generic: caller-supplied table */
alwan_table1d_sample_f64(&v, table, size, 0.5, ALWAN_SAMPLE_LINEAR);

/* named: an embedded table, no size argument to get wrong */
alwan_agx_contrast_sample_f64(&v, 0, 0.5, ALWAN_SAMPLE_LINEAR);
```

The mode selects reconstruction: `ALWAN_SAMPLE_LINEAR` (0, the default, and
what a zero-initialized mode gives you), `NEAREST`, `TRILINEAR`,
`TETRAHEDRAL`. Rank decides which are accepted; `LINEAR` always resolves to
the linear reconstruction for that rank.

**Out-of-range and non-finite coordinates.** One addressing gate converts a
coordinate to a table cell, and it is the only place a float becomes an index.
A coordinate outside [0, 1] addresses the nearest edge, and a NaN coordinate
resolves to the low edge. This is an *address* clamp, not a value clamp: it
does not conflict with the no-silent-clamp policy on colour values, because an
address outside the table has no meaning, while an out-of-gamut colour does.

If a non-finite coordinate should be an error instead of an edge sample, OR in
`ALWAN_SAMPLE_STRICT`; the reader then returns `ALWAN_E_RANGE` and leaves the
result untouched.

Define `ALWAN_READ_DATA_NO_BOUND_CHECK=1` to compile the clamp out. Only do
that when every coordinate is known finite and in range: without it, a NaN is
an out-of-bounds read rather than a wrong colour.

---

## Embedded Mode Benefits

- no external file deployment
- no runtime CSV parsing
- deterministic data availability across platforms
- simpler sandboxed or bundled application deployment

The trade-off is binary size: the library ships the data with the code.

---

## Planned Runtime / On-Demand Loading

This topic stays documented because it is still part of the intended design, but
it is not a current feature.

What the code says today:

- `alwan_config.runtime_data_root` exists but is reserved
- building with `ALWAN_EMBED_DATA=0` is not supported
- docs should describe runtime loading as planned only

The expected future direction is:

- load datasets from a caller-provided root directory
- populate the same public descriptors/getters from file-backed data
- keep public API behavior as close as possible to embedded mode

Until that lands, do not write production code that expects file-backed data
loading to work.

---

## `runtime_data_root`

You may still see this field in `alwan_config`:

```c
alwan_ctx *ctx = alwan_create(&(alwan_config){
    .runtime_data_root = "data"
});
```

In the current release this is documentation of intent only. The field is
ignored because runtime data loading is not implemented yet.

---

## Impact On Higher-Level Features

The current embedded-data design feeds:

- RGB space lookup and conversion descriptors
- illuminant lookup helpers
- spectral integration workflows
- LUT / CLF generation that needs descriptor-driven transforms
- interop and video helpers that rely on the RGB-space registry

Because those features all depend on the same embedded registry, keeping
`ALWAN_EMBED_DATA=1` is the safe default for all public builds today.

---

## Guidance

- Use embedded mode for all current integrations.
- Treat runtime loading as roadmap material rather than a shipping workflow.
- If you need custom colour spaces today, build descriptors directly in memory
  with `alwan_rgb_space_desc_f32` / `alwan_rgb_space_desc_f64` rather than
  expecting a runtime registry extension.

See also:

- [configuration.md](configuration.md)
- [docs/api/context.md](api/context.md)
- [docs/api/reference-data.md](api/reference-data.md)
