# Data Management

How Alwan holds its reference datasets: compiled in by default, or loaded at
run time from a data pack (`ALWAN_EMBED_DATA=0`).

---

## Current Reality

Alwan depends on a large body of reference data:

- colour-matching functions
- illuminants
- RGB space descriptors and matrices
- spectral basis data
- fixtures and lookup tables used by higher-level APIs

By default (`ALWAN_EMBED_DATA=1`) the CSV-derived data is compiled into the
library, and public getters return pointers into embedded data or
library-owned caches. A library built with `ALWAN_EMBED_DATA=0` keeps the
small tables compiled in and loads the large switchable ones at run time; see
[Embedded or loaded at run time](#embedded-or-loaded-at-run-time).

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

## Compiling Tables Out

Most of the embedded bytes are a few large tables. `ALWAN_DATA_TABLES_MINIMAL=1`
and the per-table switches leave them out, and the functions that read them
return `ALWAN_E_NODATA` instead; see [configuration.md](configuration.md).

---

## Third-Party Data

Tables are written by alwan_dev's `gendata/` scripts, and each source's licence
travels with it:

| Tables | Source | Licence |
|---|---|---|
| `camera_sensitivities/rawtoaces/`: 52 cameras, 190 training reflectances, ISO 7589 tungsten | Academy Software Foundation rawtoaces-data, commit e9b8503 | Apache-2.0; `LICENSE.txt` and `SOURCE.txt` sit beside the tables |
| `camera_sensitivities/aces_ricd_{r,g,b}.csv` | colour-science `MSDS_ACES_RICD`, after the Academy's tables | BSD-3-Clause |

Datasets whose licence does not allow redistribution are not embedded. alwan_dev
fetches some of them to validate against, and they stay there.

---

## Embedded Or Loaded At Run Time

`ALWAN_EMBED_DATA` picks where the switchable tables live, when the library is
compiled:

| | `ALWAN_EMBED_DATA=1` (default) | `ALWAN_EMBED_DATA=0` |
|---|---|---|
| switchable tables | compiled in | loaded from a data pack on first use, or up front; freed on request |
| a reader's access | the array, an immediate address | one pointer load and a branch |
| a table not there | switched off: `ALWAN_E_NODATA` | no pack at the data root: `ALWAN_E_NODATA` |
| size (alwan_dev's statically linked test binary, Release x64) | 100.4 MB | 18.7 MB, plus a 76.9 MB pack |
| matrices, chromaticities, RGB spaces, CAT data | compiled in | compiled in |

The switchable tables are every table `data/alwan_data_tables_config.h` can
switch off: illuminant and light-source SPDs, observers and photopic functions,
the NPL and rawtoaces cameras, the film stocks, the Jakob 2019, Smits, Mallett
and Otsu upsampling tables, the AgX and Tony McMapface look-up tables, the
light-quality reference sets, and the refractiveindex.info database's
wavelength, value and coefficient arrays. Everything else, the small constant tables read
at fixed indices, is compiled into every build.

The readers do not change shape between the two: they index `table[i]` on the
embedded array and on the loaded pointer alike, so a loaded table gives the
same results, bit for bit. In runtime mode each table's name is a macro over a
hidden pointer that loads the table the first time anything reads it, so no
function needs a load call first. Where the pack cannot be found, each reader
answers as it does for a switched-off table (`ALWAN_TABLE_REQUIRE` in
`data/alwan_data_tables.h`). An embedded build compiles all of this away.

### Loading: on demand, or when you choose

Nothing has to be called. The first function that reads a table opens
`alwan_data.pack` in the data root, reads that one table out of it (both
precisions) and keeps it; one illuminant read loads 5,652 bytes and leaves the
other 297 tables on disk. To load at a moment of your choosing instead, at
initialisation say, call `alwan_data_preload(groups, ctx)`.

The data root is, in order: the folder given to `alwan_data_set_root`, the
`ALWAN_DATA_ROOT` environment variable, the current directory.
`alwan_data_preload` with a context created with a `runtime_data_root` reads
that folder instead. Creating a context never changes the process's root: an
early version did, and a test that made a context with a made-up folder sent
every later on-demand load looking there. A library built with `-DALWAN_DATA_ROOT_DEFAULT="..."` changes the
last of these. When a table's on-demand load fails, it is not retried until the
root changes or a load or free runs, so a missing pack costs one file open per
table, not one per call.

### The data pack

An embedded build writes the pack; a runtime build loads it:

```c
/* In a tool linked against the default (embedded) library. */
size_t size = 0;
alwan_data_pack_write(NULL, 0, &size, ALWAN_DATA_GROUP_ALL);   /* size only */
void *pack = malloc(size);
alwan_data_pack_write(pack, size, &size, ALWAN_DATA_GROUP_ALL);
/* ... save it ... */

/* In the application, linked against an ALWAN_EMBED_DATA=0 library. */
alwan_data_set_root("/opt/myapp/data");    /* optional: where alwan_data.pack lives */
                                           /* (or set ALWAN_DATA_ROOT in the environment) */
/* ... use the library: tables load as they are first read ... */
alwan_data_free(ALWAN_DATA_GROUP_FILM);    /* done with film: its tables go back */
alwan_data_free(ALWAN_DATA_GROUP_ALL);     /* at exit */

/* Or load everything at start-up, and know then whether the pack is complete. */
if (alwan_data_preload(ALWAN_DATA_GROUP_ALL, ctx) != ALWAN_OK) { ... }
if (alwan_data_missing(ALWAN_DATA_GROUP_ALL) != 0) { ... }   /* a pack from an older build */
```

- **Groups.** Tables load and free by group, the same groups as the
  `ALWAN_TABLES_*` switches: `AGX`, `SPECTRAL`, `SPD`, `QUALITY`, `CAMERAS`,
  `LIGHT_SOURCES`, `FILM`, `OTHER`, plus `REFRACTIVE` for the
  refractiveindex.info database (`ALWAN_WITH_REFRACTIVE_DATA`). A pack can hold any of them, and a load
  takes only the groups it is asked for.
- **Memory.** Each loaded table is copied out of the pack with the context's
  allocator (NULL: the default one), so the pack buffer can be freed after the
  load. `alwan_data_loaded_bytes` reports what is held.
- **Validation.** The pack is untrusted input. It is read whole before
  anything changes: a malformed pack, or a table whose extent differs from the
  library's, is `ALWAN_E_INVALID`, an allocation failure is `ALWAN_E_NOMEM`,
  and in both cases no table changes. A table already loaded is replaced. Tables
  the library does not have are skipped, so a pack written by a newer build
  loads into an older one.
- **Threads.** On-demand loads are safe from any number of threads: two threads
  that race for a table both read it and one copy is kept, published with an
  atomic compare-and-swap. The explicit calls (`alwan_data_preload`,
  `alwan_data_load`, `alwan_data_free`, `alwan_data_set_root`) write
  process-wide state unsynchronised with readers: make them before other
  threads start, or when none is inside the library.
- **Format.** Little-endian, versioned (version 1, described at the head of
  `api/alwan_data_runtime.c`). A table with both precisions is
  stored once, in f64; the loader narrows it to the f32 twin, which is what the
  compiler does to the CSV literal. Where that would not reproduce the f32
  twin, the writer stores the f32 values too.
- **Embedded builds.** `alwan_data_load` validates a pack and loads nothing,
  `alwan_data_free` does nothing, and `alwan_data_loaded_bytes` and
  `alwan_data_missing` are 0, so the same application code runs against both
  builds. `alwan_get_build_info` reports which one it has (`embed_data`).

The GPU backends read tables they are given, bound by the caller, and are not
affected by either mode.

### `runtime_data_root`

The folder `alwan_data_preload(groups, ctx)` reads when given that context. The
tables are process-wide, so on-demand loads, which have no context, use the
process's root (`alwan_data_set_root`); a context's root never changes it.

---

## Guidance

- Embedded (the default) for most integrations: nothing to deploy.
- `ALWAN_EMBED_DATA=0` when binary size matters and the data can ship beside
  the library, or when an application uses a group (film, light sources, the
  camera pack) for a while and wants the memory back afterwards.
- `ALWAN_DATA_TABLES_MINIMAL` and the per-table switches when a table is never
  needed at all: those leave the reader's code out too.
- If you need custom colour spaces, build descriptors directly in memory with
  `alwan_rgb_space_desc_f32` / `alwan_rgb_space_desc_f64` rather than
  expecting a runtime registry extension.

See also:

- [configuration.md](configuration.md)
- [docs/api/context.md](api/context.md)
- [docs/api/reference-data.md](api/reference-data.md)
