# Interop IDs, Half Floats And Video Signals API

Color Interop Forum ID strings, IEEE 754 binary16 conversion, and one-call video
signal encode/decode (transfer function + range scaling + quantisation).

> **Precision variants:** Every function and type shown as `name_{T}` exists in two forms:
> `name_f32` (single precision, `float`) and `name_f64` (double precision, `double`).
> `T = f32 | f64`.

---

## Overview

Three groups of functions in the last section of `alwan.h`. The first two sit
under Color Interop Forum banners ("Interop ID Strings", "float16 (half-float)
Conversion"). The video pair has its own "Video Signal Encoding / Decoding"
banner, with the CLF export block between them.

- **Interop IDs** -- string <-> `alwan_rgb_space` lookup over a 41-row static table
- **Half floats** -- batch binary16 <-> binary32 conversion, no lookup table
- **Video signals** -- linear RGB <-> packed video codes, with SMPTE narrow or full range

Two suffix exceptions. `alwan_interop_format` and `alwan_interop_count` take no
`{T}` suffix. The half-float pair takes the suffix without it meaning anything
(see [Half Floats](#half-floats)).

The three groups do not compose into a pipeline. The half converters cannot feed
the video functions, because `ALWAN_PIXEL_F16` is rejected by both of them.

---

## Interop IDs

Bidirectional lookup between `alwan_rgb_space` and Color Interop Forum string
identifiers. The whole implementation is a linear scan over one 41-row
file-scope `static const` table.

### alwan_interop_parse_{T}

```c
alwan_status alwan_interop_parse_{T}(alwan_rgb_space *space, char const *id);
```

Looks `id` up in the table and writes the matching enum through `space`.

**Parameters:**
- `space` -- output, receives the matched `alwan_rgb_space`. NULL is an error.
- `id` -- NUL-terminated ID string. NULL is an error.

Matching is `strcmp`. Byte-exact, case-sensitive, no whitespace trimming, no
case folding, no aliases, no prefix or namespace handling. `"SRGB_TEXTURE"`,
`"srgb_texture "` and `"lin-srgb"` all return `ALWAN_E_NODATA`.

The header documents two outcomes. There is a third: either pointer NULL
returns `ALWAN_E_INVALID`, checked before the scan. `ALWAN_E_NODATA` means "no
row matched", never "argument missing".

`alwan_interop_parse_f32` is a one-line forward to the `_f64` body. The two are
the same code.

**Example:**
```c
alwan_rgb_space space;
if (alwan_interop_parse_{T}(&space, "lin_ap1") == ALWAN_OK) {
    /* space == ALWAN_RGB_SPACE_ACESCG */
}
```

---

### alwan_interop_format

```c
char const *alwan_interop_format(alwan_rgb_space space);
```

Scans the same table for the first row whose enum equals `space` and returns its
string. No `{T}` suffix.

**Parameters:**
- `space` -- the enum to look up

The returned pointer is a string literal referenced by the file-scope
`static interop_entry const` table. It has static storage duration, is valid for
the process lifetime, and must never be freed. The header states no ownership.
The same applies to the `id` written by `alwan_interop_entry_at_{T}`.

> **NULL conflates two conditions.** The function returns NULL both for a valid
> space with no ID (63 of the 104 enum values) and for a completely out-of-range
> or garbage `alwan_rgb_space`. Unlike `alwan_rgb_get_space_descriptor_{T}`,
> which bounds-checks against `ALWAN_RGB_SPACE_COUNT` and reports
> `ALWAN_E_INVALID`, this function never validates the enum. It scans and falls
> off the end. You cannot distinguish "unregistered" from "corrupt input".

---

### alwan_interop_count

```c
size_t alwan_interop_count(void);
```

Returns 41, the row count of the static table. No `{T}` suffix.

`alwan_rgb_space` has 104 members (`ALWAN_RGB_SPACE_COUNT`, held by a
`_Static_assert` against the transfer-function table). 41 of them have an
interop ID; the other 63 do not. A caller enumerating spaces through
`alwan_interop_entry_at_{T}` sees that 41-row subset, not the library's space
list.

---

### alwan_interop_entry_at_{T}

```c
alwan_status alwan_interop_entry_at_{T}(alwan_rgb_space *space, char const **id, size_t index);
```

Reads row `index` of the table.

**Parameters:**
- `space` -- output, may be NULL
- `id` -- output, may be NULL; receives a pointer to a string literal owned by the library
- `index` -- `[0, 40]`; `index >= alwan_interop_count()` returns `ALWAN_E_RANGE`

Both outputs are optional. Passing NULL for both is a legal no-op returning
`ALWAN_OK`. `index` is `size_t`, so a negative `int` argument converts to a huge
value and lands in the `ALWAN_E_RANGE` branch instead of reading backwards.

`alwan_interop_entry_at_f32` forwards to the `_f64` body.

**Example:**
```c
for (size_t i = 0; i < alwan_interop_count(); i++) {
    alwan_rgb_space space;
    char const *id;
    alwan_interop_entry_at_{T}(&space, &id, i);
    /* id is owned by the library; do not free */
}
```

---

### The 41 registered IDs

Scene-referred linear: `lin_ap0` (ACES2065-1), `lin_ap1` (ACEScg), `lin_srgb`
(linear Rec.709), `lin_rec2020`, `lin_displayp3`, `lin_p3d65`.

ACES non-linear: `acescc`, `acescct`, `acesproxy`.

Camera log: `logc3_awg3`, `logc4_awg4`, `slog3_sgamut3`, `vlog_vgamut`,
`clog_cgamut`, `redlog_rwg`, `tlog_egamut`, `di_dwg`, `flog_fgamut`,
`nlog_ngamut`.

Camera linear gamuts: `lin_awg3`, `lin_awg4`, `lin_sgamut3`, `lin_sgamut3cine`,
`lin_vgamut`, `lin_cgamut`, `lin_rwg`, `lin_egamut`, `lin_dwg`, `lin_fgamut`,
`lin_ngamut`.

Display-referred: `srgb_texture`, `srgb_displayp3`, `rec1886_rec709`,
`rec2100_pq`, `rec2100_hlg`, `display_p3_hdr`.

Additional well-known spaces: `bt709`, `bt2020`, `dci_p3`, `adobergb`,
`prophoto`.

> **The last five sit outside the CIF sections of the table.** The source groups
> them under a comment reading "Additional well-known spaces", separate from the
> sections above, while the file's own preamble claims "Only spaces with
> official interop IDs are included" and the header calls every returned string
> "the canonical string". Nothing in the repo records whether `bt709`, `bt2020`,
> `dci_p3`, `adobergb` and `prophoto` are Color Interop Forum identifiers. Check
> them against the current CIF ID list before writing them into an interchange
> file.

---

## Half Floats

Batch conversion between IEEE 754 binary16 bit patterns held in `alwan_uint16`
and binary32 in `alwan_f32`. Per-sample bit manipulation, no lookup table.

> **The `_f64` suffix carries no double-precision meaning here.** Both twins are
> bit-identical and both operate entirely in float32. The shared core declares
> its parameters and returns as plain `float` and `alwan_half` (a `uint16_t`
> typedef), ignoring the precision macro, so the f32 and f64 passes emit the
> same instructions under different names. The core file says so itself: "Both
> f32 and f64 variants are identical." The public signatures already say
> `alwan_f32`; the suffix is the only thing suggesting otherwise.

`count` is measured in samples. Both buffers hold `count` elements, not
`count * 3`.

Neither `alwan_half.c` nor `alwan_interop.c` gates its public functions on
`ALWAN_WITH_F32` / `ALWAN_WITH_F64`. The two `#if` blocks in `alwan_half.c`
bracket only an implementation include whose entire content is comments; the
four functions sit outside them. An f32-only build still exports
`alwan_half_to_float_f64`, `alwan_float_to_half_f64`, `alwan_interop_parse_f64`
and `alwan_interop_entry_at_f64`, and they link. `alwan_video.c` does gate
properly.

### alwan_half_to_float_{T}

```c
alwan_status alwan_half_to_float_{T}(alwan_f32 *out, alwan_uint16 const *in, size_t count);
```

**Parameters:**
- `out` -- output, `count` float32 samples
- `in` -- input, `count` binary16 bit patterns
- `count` -- number of samples; `count == 0` returns `ALWAN_E_INVALID`

This direction is exact: every binary16 value is representable in binary32.
Zero, subnormal, Inf and NaN are all handled. Subnormal halves are renormalised
in a shift loop. `exp == 31` becomes `sign | 0x7F800000 | (mant << 13)`, so Inf
stays Inf and NaN stays NaN with the payload shifted up.

### alwan_float_to_half_{T}

```c
alwan_status alwan_float_to_half_{T}(alwan_uint16 *out, alwan_f32 const *in, size_t count);
```

**Parameters:**
- `out` -- output, `count` binary16 bit patterns
- `in` -- input, `count` float32 samples
- `count` -- number of samples; `count == 0` returns `ALWAN_E_INVALID`

Four branches on the unbiased exponent:

| Unbiased exponent | Result |
| --- | --- |
| `> 15` | Inf, or a shifted NaN payload |
| `-15 < exp <= 15` | normal, round-half-to-even; carry promotes to Inf at the top |
| `-24 <= exp <= -15` | subnormal, implicit-1 shift, round-half-to-even |
| `< -24` | signed zero |

Representable range: max finite 65504, magnitudes at or above 65520 become Inf,
normals down to `2^-14` = 6.104e-5, subnormals down to `2^-24` = 5.96e-8.

> **This is not IEEE round-to-nearest-even over the whole domain, despite the
> header saying "IEEE 754 binary16".** The subnormal branch is gated on
> `exp >= -24`, so every finite value with unbiased exponent -25 (magnitude in
> `[2^-25, 2^-24)`, i.e. `[2.98e-8, 5.96e-8)`) falls through to the
> flush-to-zero return. IEEE gives `0x0001` for anything strictly above
> `2^-25`. Example: `1.5 * 2^-25` = 4.470348e-08 converts to `0x0000` here and
> `0x0001` under IEEE. Half the subnormal underflow region is lost. Over 300,000
> random 32-bit patterns compared against `numpy.float16`, exponent -25 is the
> only finite class that disagrees; the expected population is 300000/256 = 1172
> and four seeds measured 1135 to 1223, so the exact count is sampling
> dependent. NaN inputs form a second disagreeing class, below.

> **A NaN can come back as Infinity.** The NaN branch returns
> `sign | 0x7C00 | (mant >> 13)`, keeping only the top 10 of the 23 mantissa
> bits. Any float NaN whose mantissa is below `0x2000` loses every payload bit
> and produces exactly `0x7C00` or `0xFC00`, positive or negative Infinity.
> `0x7F800001` and `0x7F801FFF` both become `0x7C00`. The quiet NaN
> `0x7FC00000` survives as `0x7E00`. Signalling NaNs and small-payload
> sentinels silently become Inf, and a `half_to_float` round trip will not
> report them as NaN. The same 300,000-pattern comparison finds 1 to 2 such
> cases per run.

---

## Video Signals

One call for transfer function plus range scaling plus quantisation. The `f32`
entry points are native-float twins with their own range and quantise math, not
a widen/narrow facade.

```c
/* Video signal range */
typedef enum {
    ALWAN_VIDEO_RANGE_FULL   = 0,  /* 0 to (2^N - 1) */
    ALWAN_VIDEO_RANGE_NARROW = 1   /* 16*2^(N-8) to 235*2^(N-8) (SMPTE) */
} alwan_video_range;

/* Pixel format (shared with the map API) */
typedef enum {
    ALWAN_PIXEL_U8  = 0,
    ALWAN_PIXEL_U16 = 1,
    ALWAN_PIXEL_F32 = 2,
    ALWAN_PIXEL_F64 = 3,
    ALWAN_PIXEL_F16 = 4   /* rejected by both video functions */
} alwan_pixel_format;
```

### Linear side units

The header names no unit anywhere. `space` selects a transfer function and
nothing else, so the unit of `rgb_linear` is whatever that curve expects.

| Spaces | Linear side |
| --- | --- |
| `rec2100_pq`, `display_p3_hdr` (`ALWAN_TF_PQ`) | absolute luminance in cd/m^2 over `[0, 10000]` |
| `srgb_texture`, `srgb_displayp3`, `rec1886_rec709`, `rec2100_hlg`, `bt709`, `bt2020`, `dci_p3`, `adobergb`, `prophoto` | display-referred, normalised `[0, 1]` |
| the 17 `lin_*` rows (`ALWAN_TF_LINEAR`) and the 13 ACES/camera-log rows | relative scene-linear, no upper bound |

The PQ OETF divides by 10000 before the m1 power and the PQ EOTF multiplies by
10000 on the way out. Feeding `1.0` to `REC2100_PQ` encodes 1 cd/m^2, PQ code
0.1499, near black. Feeding `10000.0` to sRGB is meaningless.

Scene-referred spaces have no upper bound on the linear side. LogC3 encodes
linear `1.0` to 0.5706 and reaches code 1.0 at scene-linear 55.08, so a `[0, 1]`
buffer uses 57% of the code range. ACEScc, ACEScct, S-Log3, V-Log and LogC4 have
the same shape, and the `ALWAN_TF_LINEAR` rows (`lin_ap0`, `lin_ap1`,
`lin_srgb`, `lin_awg3` and the rest) pass the value straight to the quantiser.
The library enforces none of this.

### alwan_video_encode_{T}

```c
alwan_status alwan_video_encode_{T}(void *out, alwan_{T} const *rgb_linear, size_t count,
                                    alwan_pixel_format out_fmt, alwan_rgb_space space,
                                    alwan_video_range range, int bit_depth, alwan_ctx *ctx);
```

**Parameters:**
- `out` -- output, `count * stride` bytes of 3-channel packed pixels
- `rgb_linear` -- input, `count * 3` elements. Unit depends on `space`; see
  [Linear side units](#linear-side-units).
- `count` -- number of pixels; `count == 0` returns `ALWAN_E_INVALID`
- `out_fmt` -- `ALWAN_PIXEL_U8`, `U16`, `F32` or `F64`. `ALWAN_PIXEL_F16` is
  rejected.
- `space` -- supplies the OETF only
- `range` -- `ALWAN_VIDEO_RANGE_FULL` or `ALWAN_VIDEO_RANGE_NARROW`
- `bit_depth` -- 8, 10, 12 or 16; `<= 0` derives from `out_fmt`
- `ctx` -- ignored. NULL is safe.

Per pixel: three scalar OETF calls, then a store. Integer stores round with
`+0.5` and truncate.

> **No gamut conversion happens.** The function reads `desc.oetf` from the space
> descriptor and nothing else. The primaries, white point, `rgb_to_xyz` and
> `xyz_to_rgb` are fetched and discarded. Passing ACEScg data with
> `space = ALWAN_RGB_SPACE_BT2020` applies the BT.2020 curve to ACEScg values
> and returns `ALWAN_OK`. Your data must already be in the target primaries.

### alwan_video_decode_{T}

```c
alwan_status alwan_video_decode_{T}(alwan_{T} *rgb_linear, void const *in, size_t count,
                                    alwan_pixel_format in_fmt, alwan_rgb_space space,
                                    alwan_video_range range, int bit_depth, alwan_ctx *ctx);
```

The mirror of encode, using `desc.eotf`.

**Parameters:**
- `rgb_linear` -- output, `count * 3` elements. Unit depends on `space`; see
  [Linear side units](#linear-side-units).
- `in` -- input, `count * stride` bytes of 3-channel packed pixels
- `count` -- number of pixels; `count == 0` returns `ALWAN_E_INVALID`
- `in_fmt` -- `ALWAN_PIXEL_U8`, `U16`, `F32` or `F64`. `ALWAN_PIXEL_F16` is
  rejected.
- `space` -- supplies the EOTF only. No gamut conversion happens.
- `range` -- `ALWAN_VIDEO_RANGE_FULL` or `ALWAN_VIDEO_RANGE_NARROW`
- `bit_depth` -- 8, 10, 12 or 16; `<= 0` derives from `in_fmt`
- `ctx` -- ignored. NULL is safe.

### Round trips

Encode followed by decode is not an identity for every space. Two shipped spaces
are deliberately asymmetric.

> **`ALWAN_RGB_SPACE_REC2100_HLG`.** Encode applies `alwan_hlg_oetf` with no
> OOTF. Decode applies `alwan_hlg_eotf`, which inverts that OETF and then raises
> the result to 1.2. A round trip therefore returns `x^1.2`: 0.5 comes back as
> 0.4353. The system gamma 1.2 is hard-coded with no `Lw` dependence.

> **`ALWAN_RGB_SPACE_REC1886_REC709`.** OETF `ALWAN_TF_BT709` (the piecewise
> camera curve, `1.099 * x^0.45 - 0.099` above 0.018; `ALWAN_TF_BT709` resolves
> to the same functions as `ALWAN_TF_BT2020`) is paired with EOTF
> `ALWAN_TF_BT1886` (pure `E^2.4`), so a round trip applies roughly the 1.2
> rendering gamma.

Only spaces whose OETF and EOTF are true inverses round-trip.

### bit_depth

`bit_depth <= 0` derives from the pixel format, then the result must be exactly
8, 10, 12 or 16 or the call fails with `ALWAN_E_INVALID`.

| Format | Derived bit depth |
| --- | --- |
| `ALWAN_PIXEL_U8` | 8 |
| `ALWAN_PIXEL_U16` | 16 |
| `ALWAN_PIXEL_F32` | 8 |
| `ALWAN_PIXEL_F64` | 8 |
| anything else | 8 |

> **F32 and F64 derive 8, not 10 or 12.** A float buffer under
> `ALWAN_VIDEO_RANGE_NARROW` with `bit_depth = 0` pins footroom and headroom to
> the 8-bit ratios 16/255 = 0.0627 and 235/255 = 0.9216. Passing `bit_depth = 10`
> for the same buffer gives 64/1023 = 0.0626 and 940/1023 = 0.9189, a different
> normalisation for identical storage. Pick the depth explicitly for float
> buffers.

> **`bit_depth` is never checked against the pixel format.** `max_code` comes
> from `bit_depth` alone as `(1 << bit_depth) - 1`. `ALWAN_PIXEL_U8` with
> `bit_depth = 16` under full range clamps to `max_code` and then evaluates
> `(uint8_t)cv` on a double of up to 65535.0 (the `+0.5` is applied before the
> clamp, so `max_code` is the ceiling), an out-of-range floating-to-integer
> conversion, undefined behaviour in C, returning `ALWAN_OK`. Under narrow range
> the clamp is to `[foot, head]` = `[4096, 60160]` at 16 bits, so every stored
> byte is an out-of-range conversion, the darkest included. Same for U8 with 10
> or 12 bits.

Two more edges in the same check:

- The derive sentinel is `bit_depth <= 0`, not `== 0`. `bit_depth = -1` silently
  derives instead of erroring.
- The check runs unconditionally, including in combinations where `bit_depth` is
  never used. `alwan_video_encode_{T}(out, in, n, ALWAN_PIXEL_F32, space, ALWAN_VIDEO_RANGE_FULL, 14, ctx)`
  fails with `ALWAN_E_INVALID` although full-range float output ignores
  `bit_depth` entirely. The in-source comment records why the check exists: an
  unchecked `bit_depth` used to leak uninitialised stack from the narrow-range
  helper while still returning `ALWAN_OK`.

The internal helper still carries a wider `8 <= bit_depth <= 16` guard whose
failure return both callers ignore. It is dead only because the public entry
points validate first.

### Range scaling

Narrow (SMPTE) range, N bits: `foot = 16 * 2^(N-8)`, `head = 235 * 2^(N-8)`,
`max_code = 2^N - 1`.

| N | foot | head | max_code |
| --- | --- | --- | --- |
| 8 | 16 | 235 | 255 |
| 10 | 64 | 940 | 1023 |
| 12 | 256 | 3760 | 4095 |
| 16 | 4096 | 60160 | 65535 |

Full range is `[0, 2^N - 1]` for integer formats. For F32/F64 under full range
there is no scaling at all: the OETF output is written verbatim.

### Clamping

Clamping is never mentioned in the header, and it differs across all four
format/range combinations.

| Format | Range | Encode behaviour |
| --- | --- | --- |
| U8, U16 | NARROW | scaled into `[foot, head]` and clamped there |
| U8, U16 | FULL | scaled by `max_code` and clamped to `[0, max_code]` |
| F32, F64 | NARROW | scaled to `[foot/max_code, head/max_code]`, no clamp |
| F32, F64 | FULL | raw OETF output, no scaling, no clamp |

> **The encoder and decoder disagree about the legal code range.** Narrow-range
> integer encode clamps hard to `[foot, head]`, so footroom and headroom codes
> are unreachable from the encoder. The decode path contains no clamp of any
> kind: an 8-bit narrow code of 8 unscales to `(8-16)/219` = -0.03653 and a code
> of 250 to 1.0685, both handed straight to the EOTF.

> **Whether sub-black survives depends on the curve.** PQ, HLG, BT.1886 and the
> pure-gamma EOTFs clamp negative encoded input to zero. The piecewise sRGB and
> BT.709/BT.2020 EOTFs do not: their linear segment maps the negative straight
> through, so an 8-bit narrow code of 8 under `ALWAN_RGB_SPACE_BT2020` returns
> -0.008118 linear. Camera-log EOTFs also return negatives; the same code under
> LogC3 gives -0.024096. Super-white survives as linear above 1 in every case.

> **Float output is never clamped.** Negative linear input, values above 1 and
> NaN all reach the caller's F32/F64 buffer unchanged. Under full range they
> arrive with no scaling either, so the effective output range is whatever the
> curve produces.

### Buffer sizing

Stated nowhere in the header.

- `rgb_linear` is `count * 3` elements of `alwan_{T}`.
- The packed side is `count * stride` bytes, where stride is
  `3 * sizeof(uint8_t)`, `3 * sizeof(uint16_t)`, `3 * sizeof(float)` or
  `3 * sizeof(double)`.

`ALWAN_PIXEL_F16` has no stride entry and yields 0, which both functions reject
with `ALWAN_E_INVALID`. `ALWAN_PIXEL_F16` is the format the half-float
converters in this cluster produce; they are declared in the preceding float16
block of the same header, with the CLF export block in between. The internal
store and load helpers also carry silent `default:` arms that write nothing or
zero the output, unreachable while the stride check stands.

### ctx

The header calls it "context for space descriptor lookup". It is passed to the
descriptor lookup and immediately discarded with `(void)ctx`. The only supported
build mode is `ALWAN_EMBED_DATA`, where descriptors come from static tables; the
alternative branch is an `#error`. NULL is safe and behaviour is identical with
or without a context.

**Example:**
```c
alwan_{T} linear[256 * 3];     /* normalised [0,1] for BT.2020 */
uint16_t  signal[256 * 3];

alwan_video_encode_{T}(
    signal,                     /* out */
    linear,                     /* rgb_linear */
    256,                        /* count */
    ALWAN_PIXEL_U16,            /* out_fmt, fourth, after count */
    ALWAN_RGB_SPACE_BT2020,
    ALWAN_VIDEO_RANGE_NARROW,
    10,
    NULL);

alwan_video_decode_{T}(
    linear,
    signal,
    256,
    ALWAN_PIXEL_U16,
    ALWAN_RGB_SPACE_BT2020,
    ALWAN_VIDEO_RANGE_NARROW,
    10,
    NULL);
```

**Example:** PQ, with the unit the header omits.

```c
alwan_{T} nits[3] = {100.0, 100.0, 100.0};   /* cd/m^2, not [0,1] */
uint16_t  pq[3];

alwan_video_encode_{T}(pq, nits, 1, ALWAN_PIXEL_U16,
                       ALWAN_RGB_SPACE_REC2100_PQ,
                       ALWAN_VIDEO_RANGE_FULL, 10, NULL);
/* PQ signal 0.5081, stored as full-range 10-bit code 520 */
```

> **Two other pages have the arguments in the wrong order.** The encode snippets
> in `docs/ranges.md` ("For video signal conversion") and `docs/examples.md`
> (section 12) both hoist the pixel format to the second argument. The decode
> snippet in `docs/examples.md` transposes `count` and `in_fmt`, putting the
> format third. The real orders are
> `(out, rgb_linear, count, out_fmt, space, range, bit_depth, ctx)` and
> `(rgb_linear, in, count, in_fmt, space, range, bit_depth, ctx)`.
> `docs/ranges.md` has no decode snippet. The `examples.md` snippet additionally
> feeds `REC2100_PQ` from a buffer of normalised values. Do not copy either.

---

## Error Codes

- `ALWAN_OK` (0) -- success, for all six status-returning functions here.
- `ALWAN_E_INVALID` (-1) -- undocumented in the header for every case below.
  `alwan_interop_parse_{T}`: `space` or `id` NULL. `alwan_half_to_float_{T}` and
  `alwan_float_to_half_{T}`: `out` NULL, `in` NULL, or `count == 0`.
  `alwan_video_encode_{T}` and `alwan_video_decode_{T}`: `out`/`in` NULL,
  `count == 0`, `space` outside `[0, ALWAN_RGB_SPACE_COUNT)`, `bit_depth` not in
  {8, 10, 12, 16} after the `<= 0` derive, or a pixel format with no stride
  (`ALWAN_PIXEL_F16` and anything unknown).
- `ALWAN_E_NODATA` (-2) -- `alwan_interop_parse_{T}` only, for a non-NULL `id`
  matching no row.
- `ALWAN_E_RANGE` (-3) -- `alwan_interop_entry_at_{T}` only, for
  `index >= alwan_interop_count()`.

`count == 0` is an error, not an empty-batch no-op. The four functions here that
take a `count` (`alwan_half_to_float_{T}`, `alwan_float_to_half_{T}`,
`alwan_video_encode_{T}`, `alwan_video_decode_{T}`) return `ALWAN_E_INVALID` for
zero count, so guard your loops before calling.
`alwan_interop_parse_{T}` and `alwan_interop_entry_at_{T}` have no `count`
parameter; `index >= alwan_interop_count()` is the analogous guard there, and it
returns `ALWAN_E_RANGE`.

Never returned by this cluster: `ALWAN_E_NOMEM`, `ALWAN_E_DIVZERO`. The video
and half functions never return `ALWAN_E_RANGE` despite `bit_depth` and `count`
being range-like parameters; an out-of-range `bit_depth` is `ALWAN_E_INVALID`.

The video functions check in this order: null pointers and zero count, then the
space descriptor lookup (whose status is returned verbatim), then the transfer
function resolver, then `bit_depth`, then the pixel stride. The resolver's
`ALWAN_E_INVALID` is defensive and unreachable through the public API: every
`alwan_transfer_function` the 104 space rows can name has a resolver entry, and
`space` is bounds-checked first, so `oetf`/`eotf` is never NULL.

---

## See also

- [transfer-functions.md](transfer-functions.md) -- the OETF/EOTF pairs the video functions dispatch to
- [color-spaces.md](color-spaces.md) -- `alwan_rgb_space` and the descriptor lookup
- [hdr.md](hdr.md) -- HLG OOTF with an explicit `Lw` and system gamma
- [luts.md](luts.md) -- CLF export, declared between the two blocks covered here
- [map.md](map.md) -- the other consumers of `alwan_pixel_format`
- [../ranges.md](../ranges.md) -- value ranges; its video snippet has the arguments in the wrong order
- [../examples.md](../examples.md) -- section 12 has the same wrong argument order
- [../precision-and-limits.md](../precision-and-limits.md) -- f32/f64 behaviour across the library
