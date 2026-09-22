# Colour Selection

Soft masks that say how much each pixel belongs to a chosen set of colours: 1 inside
the selection, 0 outside it, and a soft edge between. Four approaches return the same
kind of mask, so they can be combined with ordinary arithmetic (`min` for an
intersection, `max` for a union, `1 - m` to invert).

| Approach | Selects | Parameters |
|---|---|---|
| Qualifier | a range of hue, chroma and lightness | ranges and softness, in HSV, OkLCh or CIE LCh |
| Distance | colours near one key colour | key, metric, tolerance, softness |
| Example | colours like a handful of sampled pixels | samples, ridge, tolerance in Mahalanobis units |
| Chroma key | everything that is not the green or blue screen | balance, gain, despill |

All of them take the space's **encoded** pixel values, as an image arrives, with byte
strides that must be at least one element wide (a stride of 0 is refused). The work is
done in double; the f32 forms widen the pixels and narrow the mask.

## Units

The units are the same in every build, and are the normalised ones, so a parameter
means the same thing whether or not `ALWAN_NORMALIZE_RANGES` is set:

| Quantity | Unit |
|---|---|
| Hue | a fraction of a turn in [0, 1), measured from the +a axis (HSV's own convention) |
| Lightness | [0, 1]: HSV V, Oklab L, or L* / 100 |
| Chroma | its own scale: HSV S in 0..1, Oklab C about 0..0.4, C*ab about 0..150 |

A hue of 0 is the +a axis in all three models. A normalised CIE LCh hue
(`h / 360`) is already in turns. A normalised OkLCh hue is `h / pi` on `[-1, 1]`
(see [ranges](../ranges.md)): half of it, plus 1 when negative, is the turn.

Soft edges are linear: 1 inside the range (or below the tolerance), falling to 0 over
`softness` outside it. Softness 0 is a hard edge.

## Qualifier

```c
typedef enum {
    ALWAN_SELECT_HSV = 0,     /* hue, saturation, value of the encoded RGB */
    ALWAN_SELECT_OKLCH = 1,   /* Oklab L, C, h */
    ALWAN_SELECT_CIELCH = 2   /* CIE L*, C*ab, h_ab under the space's white */
} alwan_select_model;

typedef struct {
    alwan_select_model model;
    alwan_f64 hue_center, hue_width, hue_softness;       /* turns */
    alwan_f64 chroma_min, chroma_max, chroma_softness;
    alwan_f64 light_min, light_max, light_softness;      /* [0, 1] */
    int invert;
} alwan_select_qualifier_params;

void alwan_select_qualifier_params_init(alwan_select_qualifier_params *params);
alwan_status alwan_select_qualifier_{T}(alwan_{T} *mask_out, size_t mask_stride,
                                        alwan_{T} const *rgb, size_t rgb_stride, size_t count,
                                        alwan_rgb_space_desc_{T} const *space,
                                        alwan_select_qualifier_params const *params);
```

The secondary-grading "HSL qualifier". The mask is the smallest of three weights:
lightness within `[light_min, light_max]`, chroma within `[chroma_min, chroma_max]`, and
hue within `hue_width / 2` of `hue_center` on the circle. A width of 1 or more takes every
hue. `params_init` gives every colour in OkLCh with hard edges, a mask of 1 everywhere,
to narrow from.

OkLCh and CIE LCh are the models to reach for: a hue range there is a range of perceived
hue, where an HSV range crowds the yellows and stretches the blues. HSV is there because
it is what grading software shows.

A neutral pixel has no hue (all three models report 0 for it), so a hue range near red
also takes greys unless `chroma_min` keeps them out.

**Errors:** `ALWAN_E_INVALID` for a NULL, a stride too small, a model outside the enum, a
non-finite or negative width or softness, or a min above its max.

## Distance to a key colour

```c
typedef enum {
    ALWAN_SELECT_METRIC_OKLAB = 0,   /* Euclidean in Oklab */
    ALWAN_SELECT_METRIC_DE76 = 1,    /* CIE 1976 */
    ALWAN_SELECT_METRIC_DE2000 = 2   /* CIEDE2000 */
} alwan_select_metric;

alwan_status alwan_select_distance_{T}(alwan_{T} *mask_out, size_t mask_stride,
                                       alwan_{T} const *rgb, size_t rgb_stride, size_t count,
                                       alwan_rgb_space_desc_{T} const *space,
                                       alwan_{T} const key_rgb[3], alwan_select_metric metric,
                                       alwan_{T} tolerance, alwan_{T} softness);
```

1 for pixels within `tolerance` of `key_rgb` (encoded, in the same space), falling to 0
at `tolerance + softness`. About 0.02 in Oklab, or 2 in Delta E, is a just-noticeable
step. The Lab metrics use the space's own white; Oklab is evaluated on the space's XYZ
without adaptation, which suits D65 spaces.

## Select by example

```c
typedef struct {
    alwan_f64 mean[3];           /* Oklab L, a, b */
    alwan_f64 covariance[9];     /* row-major, ridge included */
    alwan_f64 inv_covariance[9];
} alwan_select_example;

alwan_status alwan_select_example_fit_{T}(alwan_select_example *model_out,
                                          alwan_{T} const *samples, size_t sample_stride, size_t count,
                                          alwan_rgb_space_desc_{T} const *space, alwan_f64 ridge);
alwan_status alwan_select_example_{T}(alwan_{T} *mask_out, size_t mask_stride,
                                      alwan_{T} const *rgb, size_t rgb_stride, size_t count,
                                      alwan_rgb_space_desc_{T} const *space,
                                      alwan_select_example const *model,
                                      alwan_{T} tolerance, alwan_{T} softness);
```

Pick a few pixels of a thing (skin, a sky, a costume) and select its kind. The fit
converts the samples to Oklab and keeps their mean and sample covariance (divided by
n - 1), with `ridge` added to the diagonal. The mask is 1 within `tolerance` Mahalanobis
units of the mean and falls to 0 over `softness`; 2 to 3 takes most of what the samples
represent. Because the Gaussian is shaped by the samples, the selection is wide along the
directions the samples vary in and narrow across them, which a single tolerance around
one key colour cannot do.

A ridge above zero is what lets a handful of near-identical samples fit; about 1e-5 is a
sensible floor in Oklab units. `ALWAN_E_DIVZERO` when the covariance is singular (samples
on a line or a plane with ridge 0), `ALWAN_E_INVALID` for fewer than two samples.

## Chroma key

```c
typedef enum { ALWAN_KEY_GREEN = 0, ALWAN_KEY_BLUE = 1 } alwan_key_screen;

alwan_status alwan_key_chroma_{T}(alwan_{T} *alpha_out, size_t alpha_stride,
                                  alwan_{T} *fg_out, size_t fg_stride,
                                  alwan_{T} const *rgb, size_t rgb_stride, size_t count,
                                  alwan_key_screen screen, alwan_{T} balance, alwan_{T} gain,
                                  int despill);
```

The difference matte of the Vlahos patents, as Smith and Blinn write it in "Blue Screen
Matting" (SIGGRAPH 1996). With S the screen channel and the reference
`R = balance * red + (1 - balance) * other` (other is blue for a green screen, green for a
blue one):

    alpha = clamp(1 - gain * (S - R), 0, 1)

The screen keys to 0 and anything with no excess of the screen channel to 1. `fg_out`
(optional) receives the pixels, and with `despill` the screen channel limited to `R`,
which removes screen light spilled onto the subject and leaves every other pixel alone.
Values are taken as given, without decoding: key in the space the plate was shot in.

**Errors:** `ALWAN_E_INVALID` for a NULL, a stride too small, a screen outside the enum, a
balance outside [0, 1] or a negative gain.

## Testing

No published reference defines these masks. Suite 181 holds everything they are built
from to colour-science (sRGB decoding, the conversions to Lab, Oklab, LCh and HSV, and
the CIE 1976 and CIEDE2000 differences) and numpy (the example fit), and the masks to the
definitions above: 1.6e-14 worst on 355 pixels over seven qualifiers, four distance keys
and three example keys, 1e-6 through the f32 forms. The chroma key has no reference that
implements this exact matte, so its properties are what the suite checks.
