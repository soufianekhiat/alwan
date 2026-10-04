# CSS colour (CSS Color 4/5), Okhwb and HCT

Colours in the CSS colour spaces, CSS Color 5's `color-mix()`, evenly spaced steps, two
lightness contrasts, Okhwb and Google's HCT. Suite 303 holds them to color.js 0.5.2,
Culori 4.0.1 and material-color-utilities 0.3.0, run with node.

## Colours and spaces

`alwan_css_color_{T}` is a space, three coordinates and an alpha. A coordinate or the alpha
may be NaN, which is CSS's `none`. `alwan_css_space` lists Oklab, Oklch, sRGB, linear sRGB,
Display P3, A98 RGB, ProPhoto RGB, Rec.2020, Lab (D50), LCH, XYZ D50, XYZ D65, HSL and HWB,
in each space's CSS units (RGB 0..1, Lab L 0..100, Oklab L 0..1, hues in degrees, HSL and HWB
percentages 0..100, XYZ with Y = 1 for the white).

`alwan_css_color_convert_{T}` converts as color.js does: up the tree of base spaces to the
common base and down again. Converting between two different spaces turns missing
components into 0, and a polar space reports a powerless hue as missing.

## color-mix() and steps

`alwan_css_color_mix_{T}(out, a, b, p, params)` interpolates from a (p = 0) to b (p = 1).
`alwan_css_mix_params` (zero it) picks the interpolation space (0 = Oklab, CSS Color 5's
default), the output space (0 = the interpolation space), the hue method (shorter, longer,
increasing, decreasing; 0 = shorter) and premultiplied alpha. A component missing in one
colour takes the other's value before the hue arc is chosen. Premultiplied mixes follow CSS
Color 4 section 12.3 and leave the hue alone (color.js 0.5.2 also premultiplies the hue in a
polar space). Endpoints are not gamut mapped first.

`alwan_css_color_mix_percent_{T}` takes CSS's two percentages (NaN for an omitted one): both
omitted is 50 / 50, one omitted is 100 minus the other, and a sum below 100 scales the
result's alpha. `alwan_css_color_steps_{T}` writes count evenly spaced colours.

## Contrast

`alwan_css_contrast_lstar_{T}` is the absolute difference of CIE L*.
`alwan_css_contrast_delta_phi_{T}` is Somers' DeltaPhi*, | L1^phi - L2^phi |^(1/phi) sqrt(2)
- 40 on L* with a D65 white, reported as 0 below 7.5. alwan does not provide APCA.

## Okhwb

`alwan_srgb_to_okhwb_{T}` and `alwan_okhwb_to_srgb_{T}`: HWB built on Ottosson's Okhsv the
way CSS's hwb() is built on hsv (w = (1 - s) v, b = 1 - v; w + b >= 1 is the grey
w / (w + b)). Okhsv itself round-trips sRGB to about 2e-6 (its toe and cusp approximations);
Okhwb adds nothing to that.

## HCT and tonal palettes

`alwan_hct_from_argb_{T}` gives the hue, chroma and tone of an 8-bit 0xAARRGGBB colour (CAM16
hue and chroma in Material's viewing conditions, tone = CIE L*). `alwan_hct_to_argb_{T}`
solves for the in-gamut colour of that hue and tone with the chroma closest to the one asked
for. `alwan_hct_tonal_palette_{T}` gives one hue and chroma at several tones, as
TonalPalette.tone does. All 549 solved colours in suite 303 equal material-color-utilities'.
