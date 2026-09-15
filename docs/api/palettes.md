# Named Palettes and HEX

Colours by name: two palettes with lookup and substring search, HEX notation, and the
CSS Color 3 keywords.

## Palettes

| Palette | Colours | Model | Source |
|---|---|---|---|
| `ALWAN_PALETTE_FREETONE` | 1,310 | device CMYK | Stuart Semple's Freetone bundle (`FREETONE.ase`); the bundle states no licence |
| `ALWAN_PALETTE_CSS_COLOR_3` | 147 | sRGB | CSS Color Module Level 3 keywords, as colour-science's `CSS_COLOR_3` |

Freetone's `FREETONE.ase` holds the palette twice, as two identical groups named
"SEMPLETONE+ COATED"; alwan keeps one. The names are the palette's own, spacing
included. Its values are device CMYK: no printing condition comes with them, so they
are not colorimetric until a CMYK characterisation is applied.

```c
size_t alwan_palette_size(alwan_palette palette);
alwan_status alwan_palette_entry_at(alwan_palette_entry *entry_out, alwan_palette palette, size_t index);
alwan_status alwan_palette_find(size_t *index_out, alwan_palette palette, char const *name);
alwan_status alwan_palette_search(size_t *match_count_out, size_t *indices_out, size_t capacity,
                                  alwan_palette palette, char const *text);
```

`alwan_palette_entry` carries the name, the model and up to four values: C, M, Y, K
in [0, 1], or sRGB-encoded R, G, B in [0, 1] with `value[3] = 0`.

Lookup and search compare names as ASCII lowercase with every space, tab and newline
removed, so "pinkest pink", "PINKEST PINK" and "PinkestPink" are the same name.
`alwan_palette_find` returns the first match or `ALWAN_E_NODATA`.
`alwan_palette_search` counts every name that contains the text and writes the first
`capacity` indices; call it with a capacity of 0 to size the buffer.

```c
size_t count = 0, idx[16];
alwan_palette_search(&count, idx, 16, ALWAN_PALETTE_FREETONE, "pink");
for (size_t i = 0; i < count && i < 16; i++) {
    alwan_palette_entry e;
    alwan_palette_entry_at(&e, ALWAN_PALETTE_FREETONE, idx[i]);
    printf("%s  C%.2f M%.2f Y%.2f K%.2f\n", e.name, e.value[0], e.value[1], e.value[2], e.value[3]);
}
```

## Nearest colour

```c
alwan_status alwan_palette_nearest_{T}(size_t *index_out, alwan_{T} *delta_e_out, alwan_palette palette,
                                       alwan_lab_{T} const *lab, alwan_cmyk_model const *cmyk_model);
```

The palette colour nearest a Lab value by CIEDE2000. The Lab is relative to the palette's white:
for a CMYK palette, the white of the printing characterisation it goes through (D50 for
FOGRA39, see [reference-data.md](reference-data.md#cmyk-printing-characterisations)), and
`cmyk_model` is required; for an sRGB palette, D65, and `cmyk_model` is ignored. Ties go to the
lower index; `delta_e_out` may be `NULL`.

```c
alwan_cmyk_model *fogra = NULL;
alwan_cmyk_model_fogra39(&fogra, ctx);
alwan_lab_f64 lab = { 60.0, 40.0, 20.0 };          /* D50 */
size_t idx;
alwan_f64 de;
alwan_palette_nearest_f64(&idx, &de, ALWAN_PALETTE_FREETONE, &lab, fogra);
alwan_cmyk_model_destroy(fogra, ctx);
```

## HEX notation

```c
alwan_status alwan_rgb_to_hex_{T}(char *hex_out, alwan_rgb_{T} const *rgb);   /* 8 chars */
alwan_status alwan_hex_to_rgb_{T}(alwan_rgb_{T} *rgb_out, char const *hex);
```

`alwan_rgb_to_hex` writes `#rrggbb` in lowercase with each channel truncated to 0-255,
as colour-science's `RGB_to_HEX` does. A channel outside [0, 1], or NaN, is
`ALWAN_E_INVALID`, where colour-science clips or rescales with a warning.

`alwan_hex_to_rgb` reads six digits as colour-science's `HEX_to_RGB` does, and three as
CSS reads them, each digit doubled, so `#abc` is `#aabbcc`. colour-science takes each
of three digits as the whole value (`#abc` gives 10/255 for red). The `#` is optional;
any other length, or a character that is not a hex digit, is `ALWAN_E_INVALID`.

## CSS Color 3 keywords

```c
alwan_status alwan_css_color_3_keyword_to_rgb_{T}(alwan_rgb_{T} *rgb_out, char const *keyword);
```

As colour-science's `keyword_to_RGB_CSSColor3`, with the palette's name comparison;
`ALWAN_E_NODATA` for an unknown keyword.

Validated in suite 136: the Freetone values against the bundle, the keywords and HEX
against colour-science.
