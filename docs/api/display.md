# Display Characterisation

A display on a meter gives pairs of drive and reading. These fit the curve that
joins them, the model that predicts any signal, and the LUT that calibrates the
display to a source space. Berns (1996), "Methods for characterizing CRT
displays", Displays 16(4). Nothing in colour-science fits either model, so
suites 167 and 168 use the geometry instead: measurements are generated from a
known model and the fit has to return it, and calibrating a display that
already is the source space has to give the identity LUT.

---

## Tone response: GOG and GOGO

```c
alwan_status alwan_display_gog_fit_{T}(alwan_display_gog_{T} *out, alwan_{T} *rms_out,
                                       alwan_{T} const *digital, alwan_{T} const *luminance,
                                       size_t count, int with_flare);
alwan_status alwan_display_gog_eval_{T}(alwan_{T} *luminance_out, alwan_display_gog_{T} const *model, alwan_{T} digital);
alwan_status alwan_display_gog_invert_{T}(alwan_{T} *digital_out, alwan_display_gog_{T} const *model, alwan_{T} luminance);
```

    GOG    L = (gain d + offset)^gamma
    GOGO   L = (gain d + offset)^gamma + flare

with `d` normalised to [0, 1] and `L` normalised too. The flare is what the
room, the screen surface and the meter add to black, and it is why a measured
display almost never reads zero at zero; pass `with_flare` non-zero to fit it,
a GOG fit leaves it at zero.

`rms_out`, which may be NULL, is the root mean square luminance residual, the
quantity the fit minimises. The fit gets there in two stages: for a fixed gamma
and flare the model straightens into a line whose best gain and offset follow
in closed form, so the search is over one or two parameters rather than three
or four, and a short fixed-iteration simplex finishes on the real residual.
Both stages are deterministic, with a fixed iteration count and no convergence
test, so the answer does not move between machines. A GOG fit needs at least
four points and a GOGO fit five; fewer is `ALWAN_E_RANGE`. `invert` is closed
form; a luminance below the flare is `ALWAN_E_RANGE`, since the display cannot
go darker than its own black.

---

## A measured display, and the LUT that calibrates it

```c
alwan_status alwan_display_model_fit_{T}(alwan_display_model_{T} *out, alwan_{T} *rms_out,
                                         alwan_{T} const *drives, alwan_xyz_{T} const *ramp_red,
                                         alwan_xyz_{T} const *ramp_green, alwan_xyz_{T} const *ramp_blue,
                                         size_t count);
alwan_status alwan_display_model_forward_{T}(alwan_xyz_{T} *xyz_out, alwan_display_model_{T} const *model,
                                             alwan_rgb_{T} const *drive);
alwan_status alwan_display_model_invert_{T}(alwan_rgb_{T} *drive_out, alwan_{T} *excursion_out,
                                            alwan_display_model_{T} const *model, alwan_xyz_{T} const *xyz);
alwan_status alwan_display_calibration_lut_{T}(alwan_{T} *lut_out, int size, alwan_display_model_{T} const *model,
                                               alwan_rgb_space_desc_{T} const *source,
                                               alwan_transfer_function source_eotf, alwan_cat_method cat,
                                               alwan_{T} *worst_excursion);
```

Run a ramp up each channel with a meter on the display, and `fit` turns the
readings into a model of what the display does with any signal:

    XYZ = M [ t_r(d_r), t_g(d_g), t_b(d_b) ]^T + XYZ_black

`t_c` is that channel's tone curve, normalised to run 0 to 1 over the drive
range; `M`'s columns are the primaries at full drive with the black taken off;
the black is the reading at zero drive, carried once. Each channel is fitted as
a GOGO on the raw luminance ramp, not a black-subtracted one, and the order
matters: subtract the shared black first and a channel whose own curve has a
positive offset stops being a power law, since its emission at zero drive has
gone into the black and what is left is `(a d + b)^g - b^g`, which no GOG can
be. Fitted raw, the offset survives and the flare comes back as that channel's
reading of the display's black.

The model assumes channel independence and additivity: each channel depends
only on its own drive and the three add. Real displays deviate, LCDs most of
all, and ramps cannot fit that deviation. What alwan does instead is let you
measure it: the fit reports its residual, and measurements off the ramps can
be pushed through `forward` and compared. A model that says how wrong it is
beats one that does not.

`fit` takes one drive axis shared by three ramps of measured XYZ. The ramps
must start at drive 0, since the black reading is what everything else is
measured against, and the three readings there are averaged as three
measurements of one quantity. At least five points; `rms_out`, which may be
NULL, is the worst of the three tone residuals. `invert` gives the drive that
produces an XYZ, clamped to what the display can reach, with `excursion_out`,
which may be NULL, how far outside its gamut the request was, 0 when inside: a
reported clamp rather than a silent one.

`calibration_lut` bakes the whole chain into a 3D LUT: the source EOTF, the
source primaries, a chromatic adaptation from the source white onto the
display's measured white, and the model inverse. Feed it source-encoded signal
and it gives drive. `size` is the cube edge, 2 to 256, and the output is
`size^3 * 3` values R-fastest, which is what `alwan_cube_export_3d_{T}` and
`alwan_lut3d_sample_{T}` expect. `worst_excursion`, which may be NULL, is the
largest excursion over the whole cube: the number that says whether the display
can show the space you asked it to show.

---

## See Also

- [luts.md](luts.md) for the LUT the calibration bakes
- [color-spaces.md](color-spaces.md) for the source descriptors
- [transfer-functions.md](transfer-functions.md) for `source_eotf`
