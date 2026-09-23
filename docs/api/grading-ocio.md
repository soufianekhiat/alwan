# OpenColorIO Grading

The grading operators of OpenColorIO 2.x (BSD-3-Clause), for pipelines that carry an
OCIO grade and for the controls a grading panel exposes. The arithmetic follows OCIO
2.5.0's CPU renderer; the suites hold alwan to the installed PyOpenColorIO.

Each control is an `alwan_grading_rgbm`: red, green, blue and master. The channel's
value is combined with the master, added for the additive controls (brightness, offset,
exposure, lift) and multiplied for the multiplicative ones (contrast, gamma, gain).

```c
typedef enum { ALWAN_GRADING_LOG = 0, ALWAN_GRADING_LIN = 1, ALWAN_GRADING_VIDEO = 2 } alwan_grading_style;
typedef struct { alwan_f64 red, green, blue, master; } alwan_grading_rgbm;
```

## Primary

OCIO's `GradingPrimaryTransform`. Three styles, each a fixed chain, followed in every
style by saturation about Rec.709 luma and a clamp:

| Style | For | Chain |
|---|---|---|
| `ALWAN_GRADING_LOG` | log-encoded images | `+ brightness * 6.25 / 1023`, contrast about `0.5 + 0.5 * pivot`, gamma between the black and white pivots |
| `ALWAN_GRADING_LIN` | scene-linear images | `+ offset`, `* 2^exposure`, contrast as a power about `0.18 * 2^pivot` |
| `ALWAN_GRADING_VIDEO` | display-referred video | `+ offset + lift`, slope about the black pivot so that gain lands on the white one, gamma between the pivots |

```c
typedef struct {
    alwan_grading_rgbm brightness;   /* log */
    alwan_grading_rgbm contrast;     /* log, lin */
    alwan_grading_rgbm gamma;        /* log, video */
    alwan_grading_rgbm offset;       /* lin, video */
    alwan_grading_rgbm exposure;     /* lin, in stops */
    alwan_grading_rgbm lift;         /* video */
    alwan_grading_rgbm gain;         /* video */
    alwan_f64 saturation;
    alwan_f64 pivot;                 /* log: -1..1 around 0.5; lin: stops from 0.18 */
    alwan_f64 pivot_black, pivot_white;
    alwan_f64 clamp_black, clamp_white;   /* -DBL_MAX and DBL_MAX: no clamp */
} alwan_grading_primary;

void alwan_grading_primary_init(alwan_grading_primary *params, alwan_grading_style style);
alwan_status alwan_grading_primary_apply_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in,
                                             alwan_grading_style style,
                                             alwan_grading_primary const *params, int inverse);
alwan_status alwan_grading_primary_{T}_map_interleave(alwan_{T} *out, size_t out_stride,
                                                      alwan_{T} const *in, size_t in_stride,
                                                      size_t count, alwan_grading_style style,
                                                      alwan_grading_primary const *params,
                                                      int inverse);
```

`init` gives OCIO's defaults for the style, which are an identity grade. `inverse = 1`
runs the chain backwards with every step inverted; a clamp is not invertible, so a
graded and clamped value does not come back. A zero gain, zero log contrast or zero
saturation is treated as 1 where it would be divided by, as OCIO does.

**The linear pivot is in stops.** The pivot about which lin contrast turns is
`0.18 * 2^pivot`, and OCIO's default for the parameter is 0.18, so its default grade
turns about 0.2034, not 0.18. `init` reproduces that default so a default grade means
what it means in OCIO; set `pivot = 0` to turn about 0.18 itself.

**Errors:** `ALWAN_E_INVALID` for a NULL, a stride under three values, a style outside
the enum, a non-finite control, a gamma below 0.01 (log and video) or a lin contrast
below 0.01 (OCIO's bounds, checked on each component), `pivot_white` less than 0.01
above `pivot_black`, or `clamp_black` above `clamp_white`.

**Testing:** suite 184 renders six grades, two per style and one with clamps in each,
forward and inverse, on 129 pixels through PyOpenColorIO. OCIO renders in float32 and
raises to powers with an SSE approximation: grades that apply a gamma or a lin contrast
agree to 1.7e-5 relative, and the grade with no power agrees to 1.6e-7, float rounding.
Forward then inverse returns the input to 8.9e-16.

## Tone

OCIO's `GradingToneTransform`: five zones of the tonescale, each moved by its own
piecewise-quadratic curve, then an S-contrast. It is the zone-by-zone control a
split-tone or a "tone wheels" panel gives: warm the shadows, cool the highlights, lift
the blacks alone.

```c
typedef struct { alwan_f64 red, green, blue, master, start, width; } alwan_grading_rgbmsw;
typedef struct {
    alwan_grading_rgbmsw blacks, shadows, midtones, highlights, whites;
    alwan_f64 scontrast;
} alwan_grading_tone;

void alwan_grading_tone_init(alwan_grading_tone *tone, alwan_grading_style style);
alwan_status alwan_grading_tone_apply_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in,
                                          alwan_grading_style style,
                                          alwan_grading_tone const *params, int inverse);
alwan_status alwan_grading_tone_{T}_map_interleave(alwan_{T} *out, size_t out_stride,
                                                   alwan_{T} const *in, size_t in_stride,
                                                   size_t count, alwan_grading_style style,
                                                   alwan_grading_tone const *params, int inverse);
```

Each zone's red, green, blue and master sit around 1: the channel's curve is applied,
then the master's to all three. `start` and `width` place the zone, and mean different
things per zone, as they do in OCIO:

| Zone | Values | start, width |
|---|---|---|
| blacks, whites | 0.1 to 1.9: the slope at the bottom or top end | where the zone starts, its width |
| shadows | 0.2 to 1.8 | where the zone ends, and its **pivot**, at least 0.01 below start |
| highlights | 0.2 to 1.8 | where the zone begins, and its **pivot**, at least 0.01 above start |
| midtones | 0.1 to 1.9 | centre and width of the bump |
| scontrast | 0.01 to 1.99 | about 0.4 (log, video) or 0 (lin, in its log domain) |

The zones apply in the order midtones, highlights, whites, shadows, blacks, then the
S-contrast; the inverse runs backwards. The whites follow wherever the highlights curve
moves their range, and the blacks follow the shadows. The lin style converts to OCIO's
log domain (log2 of values relative to 0.18, with a linear toe below 0.0041) before the
zones and back after them. The result is clamped above at 65504, the half-float
maximum, as OCIO does; an identity grade is a pass-through with no clamp.

OCIO evaluates a single channel slightly differently from the master (the joins between
segments compare with `>` on one path and `>=` on the other, and the channel path of the
inverse midtones extrapolates above the top from the bottom anchor). The curve is built
so the two agree to rounding; both are reproduced as written.

**Errors:** `ALWAN_E_INVALID` for a NULL, a stride under three values, a style outside
the enum, a non-finite value, a value outside its zone's range, a width below 0.01,
shadows or highlights whose pivot crosses their start, or an S-contrast outside
[0.01, 1.99].

**Testing:** suite 185 renders nine grades through PyOpenColorIO, three per style
(every zone moved per channel and by master, slopes either side of 1, an S-contrast
with moved zones), forward and inverse. Log and video agree to 5.7e-7 relative, float
rounding. Lin agrees to 1.5e-4: OCIO's SSE build takes its log and exponential with
approximations, and an OCIO channel that no zone touches already comes back 8.6e-5 off
at 57 from that round trip alone. Forward then inverse closes to 1.1e-8.

## RGB curves

OCIO's `GradingRGBCurveTransform`: a curve per channel, then a master curve on all
three, each a monotone B-spline through control points: the curves tab of a grading
panel.

```c
#define ALWAN_GRADING_CURVE_MAX_POINTS 32
typedef struct { alwan_f64 x, y; } alwan_grading_point;
typedef struct {
    int count;
    alwan_grading_point points[ALWAN_GRADING_CURVE_MAX_POINTS];
    alwan_f64 slopes[ALWAN_GRADING_CURVE_MAX_POINTS];   /* all zero: estimated */
} alwan_grading_curve;
typedef struct { alwan_grading_curve red, green, blue, master; } alwan_grading_rgb_curve;

void alwan_grading_rgb_curve_init(alwan_grading_rgb_curve *curves, alwan_grading_style style);
alwan_status alwan_grading_rgb_curve_apply_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in,
                                               alwan_grading_style style,
                                               alwan_grading_rgb_curve const *curves, int inverse);
alwan_status alwan_grading_rgb_curve_{T}_map_interleave(alwan_{T} *out, size_t out_stride,
                                                        alwan_{T} const *in, size_t in_stride,
                                                        size_t count, alwan_grading_style style,
                                                        alwan_grading_rgb_curve const *curves,
                                                        int inverse);
```

The spline is OCIO's `GradingBSplineCurve`: each span between control points is one or
two quadratic pieces, the slopes at the points are estimated from their neighbours
unless the caller gives them (all zero means estimate), and a span whose middle would
turn back is refitted with gentler slopes, so the curve never decreases. Beyond its end
points it continues straight. The x and y of the points must each be non-decreasing,
since the inverse solves the curve.

**The fit is in float,** as OCIO's is: its slope estimation and knot placement branch
on float thresholds, and a fit in double could take the other branch on the same
points. Evaluation is in double. The lin style converts to OCIO's log domain first and
back after (see Tone), so its points are in that domain; the default curve runs from -7
to 7. A curve holds up to 32 points; OCIO also caps the four curves together at 120
knots, which alwan does not.

**Errors:** `ALWAN_E_INVALID` for a NULL, a stride under three values, a style outside
the enum, a curve with fewer than two or more than 32 points, a non-finite value, or a
decreasing x or y.

**Testing:** suite 186 renders five curve sets through PyOpenColorIO, forward and
inverse, chosen to take the fit's branches: estimated slopes, collinear runs, a jump
that forces the refit, caller slopes, two-point curves and the lin domain. Log and video
agree to 2.4e-7 relative, float rounding; lin to 8.8e-5, OCIO's approximate log and pow.
Forward then inverse closes to 7.7e-9. PyOpenColorIO 2.5.0's `red`, `green`, `blue` and
`master` setters copy a curve's control points and drop its slopes; the generator sets
slopes through the getter after assignment and checks they arrived.
