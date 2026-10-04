# Third-party notices

alwan is MIT licensed (see `LICENSE`). Parts of it are ported from, or generated with, other
projects under permissive licences. This file lists each one: what alwan takes from it, the
copyright holder, the licence, and where its full text lives. The full texts are in
`licenses/` at the repository root, and the data directories carry their own `SOURCE.txt`
(and, where they ship a licence, `LICENSE*.txt`) beside the files they describe.

Projects that alwan only tests against, without taking code or data (luxpy, spektrafilm,
color-matcher, MATLAB toolboxes, the Jp-DRT DCTL run as a reference, and others named in
`alwan_dev/gendata/datasets.py`), carry no notice here, because nothing of theirs ships.
GPL code is never ported into alwan.

---

## OpenColorIO

- Upstream: https://github.com/AcademySoftwareFoundation/OpenColorIO, v2.5.0.
- Copyright Contributors to the OpenColorIO Project.
- Licence: BSD-3-Clause, `licenses/OpenColorIO-BSD-3-Clause.txt`.

What alwan takes:
- `src/alwan/api/alwan_aces_ff.c`: the ACES 2.0 fixed functions after
  `ops/fixedfunction/ACES2/Transform.cpp` and `Common.h`, including the gamut cusp,
  reach and hue table builders, which follow OCIO's arithmetic step for step.
- `src/alwan/api/alwan_grading_ocio.c`: the grading operators (GradingPrimary, GradingTone,
  GradingRGBCurve, GradingHueCurve) and the fixed functions of `FixedFunctionOpCPU.cpp`
  (RGB_TO_HSY, LIN_TO_GAMMA_LOG, LIN_TO_DOUBLE_LOG).
- `src/alwan/api/alwan_clf.c`, `alwan_clf_import.c`: CLF node semantics as OCIO reads them.

OCIO's matrix inverse is Imath's `Matrix44::gjInverse`, reproduced in `alwan_aces_ff.c`:
Imath, Copyright Contributors to the OpenEXR Project, BSD-3-Clause,
`licenses/Imath-BSD-3-Clause.txt`.

## colour-science (colour, colour-hdri)

- Upstream: https://github.com/colour-science.
- colour: Copyright 2013 Colour Developers, BSD-3-Clause,
  `src/alwan/data/LICENSE-colour-science.txt`.
- colour-hdri: Copyright 2015 Colour Developers, BSD-3-Clause,
  `licenses/colour-hdri-BSD-3-Clause.txt`.

What alwan takes:
- Data generated with colour 0.4.7 and shipped under `src/alwan/data/`: colour matching
  functions (`cmf/`), illuminant and light source spectra (`illuminants/`,
  `illuminants_spd/`, `illuminants_xy/`), ColorChecker references (`colorchecker/`), Pointer's
  gamut and the spectral locus (`gamut/`), the Munsell renotation tables (`munsell/`, with
  Paul Centore's data as colour carries it), the CQS and CIE 2017 samples and daylight basis
  (`quality/`), the spectral recovery bases (`spectral_basis/`), the luminous efficiency
  functions and display primaries (`vision/`), Breneman 1987 (`corresponding/`), MacAdam 1942
  (`macadam/`) and the CSS Color 3 keywords (`palettes/css_color_3.inc`). Each directory's
  `SOURCE.txt` names the colour object and the generator.
- Code that follows colour's implementation step for step, the Munsell conversion
  (`src/alwan/api/alwan_munsell.c`) first among them.
- The global tone mapping operators (`src/alwan/api/alwan_tonemap*`) and the DNG colour model
  (`src/alwan/api/alwan_dng.c`) follow colour-hdri's implementations.

Highlight recovery and the colour checker detector moved to suwar with their notices.

## scikit-image

- Upstream: https://github.com/scikit-image/scikit-image, v0.26.0.
- Copyright 2009-2022 the scikit-image team, with the per-file holders listed in its licence
  (Massachusetts Institute of Technology, Broad Institute, Lee Kamentsky, Peter J. Verveer,
  University of Wisconsin-Madison and others).
- Licence: BSD-3-Clause, `licenses/scikit-image-BSD-3-Clause.txt`.

What alwan takes: the stain and video matrices in `src/alwan/data/stain/` and
`src/alwan/data/video/`, read from `skimage/color/colorconv.py`.

## Bjorn Ottosson: Oklab gamut clipping, Okhsl and Okhsv

- Upstream: https://bottosson.github.io/posts/gamutclipping/ and
  https://bottosson.github.io/posts/colorpicker/.
- Copyright (c) 2021 Bjorn Ottosson.
- Licence: MIT, `licenses/Ottosson-MIT.txt`.

What alwan takes: `compute_max_saturation`, `find_gamut_intersection` and the adaptive
clipping functions (`src/alwan/core/alwan_gamut_core.h`, `.inc`, `src/alwan/api/alwan_gamut.c`),
and the Okhsl and Okhsv conversions (`src/alwan/core/alwan_extended_core.inc`).

## rgb2spec (Jakob and Hanika 2019)

- Upstream: https://github.com/mitsuba-renderer/rgb2spec.
- Copyright (c) 2020 Wenzel Jakob.
- Licence: BSD-3-Clause, `licenses/rgb2spec-BSD-3-Clause.txt`.

What alwan takes: the coefficient tables in `src/alwan/data/spectral_lut/jakob2019/`, written
by `rgb2spec_opt` built from its source, and the `rgb2spec_fetch` lookup
(`src/alwan/api/alwan_jakob2019_fetch.inc`).

## refractiveindex.info database (Mikhail N. Polyanskiy)

- Upstream: https://refractiveindex.info, https://github.com/polyanskiy/refractiveindex.info-database.
- Release used: rii-database-2026-05-24.zip (pinned by sha256 in alwan_dev's gendata/datasets.py).
- Licence: CC0 1.0 Universal Public Domain Dedication. No notice is required; the maintainer
  asks that use be acknowledged, and this is that acknowledgement. Recommended citation:
  M. N. Polyanskiy, "Refractiveindex.info database of optical constants", Sci. Data 11, 94
  (2024), https://doi.org/10.1038/s41597-023-02898-2.

What alwan takes: every page of the n,k catalogue, as tables and dispersion-formula
coefficients in `src/alwan/data/refractive/`, generated by alwan_dev's
`gendata/data/refractive_index.py`. Each page keeps its own reference
(`alwan_refractive_index_info.reference`), the publication the data comes from. Built in unless
`ALWAN_WITH_REFRACTIVE_DATA` is 0.

## Hosek-Wilkie sky model (Lukas Hosek and Alexander Wilkie)

- Upstream: https://cgg.mff.cuni.cz/projects/SkylightModelling/, C source 1.4a (2013).
- Copyright (c) 2012 - 2013, Lukas Hosek and Alexander Wilkie.
- Licence: BSD-3-Clause, `licenses/hosek-wilkie-BSD-3-Clause.txt`.

What alwan takes: the spectral model's coefficient tables (`src/alwan/data/sky/hosek_wilkie_*.csv`,
from `ArHosekSkyModelData_Spectral.h`), the cooking of the model state, the radiance function and
the solar radiance function with limb darkening (`src/alwan/api/alwan_sky.c`,
`src/alwan/core/alwan_sky_core.inc`).

## Precomputed Atmospheric Scattering (Eric Bruneton)

- Upstream: https://github.com/ebruneton/precomputed_atmospheric_scattering (2017).
- Copyright (c) 2017 Eric Bruneton.
- Licence: BSD-3-Clause, `licenses/bruneton-BSD-3-Clause.txt`.

What alwan takes: `atmosphere/functions.glsl` and the reference model's precomputation order
(`atmosphere/reference/model.cc`), ported to C (`src/alwan/api/alwan_sky.c` and
`src/alwan/core/alwan_sky_atmosphere_reader.inc`), and the demo's ozone absorption cross
sections (`src/alwan/data/sky/bruneton_ozone.csv`; Bremen IUP 233 K spectra averaged over 10 nm).

## ASTM G173-03 reference solar spectra

- Source: NREL (SMARTS 2.9.2), as pvlib-python 0.16.1 ships them (`pvlib/data/ASTMG173.csv`,
  pvlib BSD-3-Clause); the tabulated spectra are a US government work.
- What alwan takes: the table, `src/alwan/data/sky/astm_g173.csv`, read by
  `alwan_spd_astm_g173_*` and, averaged over 10 nm bins, as the solar spectrum of the Bruneton
  sky.

Preetham, Shirley and Smits 1999 ("A Practical Analytic Model for Daylight") contributes its
published equations and its Table 2 (`src/alwan/data/sky/preetham_table2.csv`); no code.

## DaltonLens-Python

- Upstream: https://github.com/DaltonLens/DaltonLens-Python.
- Copyright (c) 2021 DaltonLens.
- Licence: MIT, `licenses/DaltonLens-Python-MIT.txt`.

What alwan takes: the Brettel, Vienot and Mollon (1997) simulation matrices in
`src/alwan/data/matrices/cvd_*`, generated from it.

## ColorAide

- Upstream: https://github.com/facelessuser/coloraide.
- Copyright (c) 2020 onwards Isaac Muse.
- Licence: MIT, `licenses/ColorAide-MIT.md`.

What alwan takes: the XYB opsin matrix and bias (`src/alwan/data/xyb_bias.csv` and the
related matrices), and the ray-trace and chroma-reduction gamut fits written after its code
(`src/alwan/api/alwan_gamut.c`).

## spectral_film_lut

- Upstream: https://github.com/JanLohse/spectral_film_lut.
- Copyright (c) 2026 Jan Lohse.
- Licence: MIT, `src/alwan/data/film/LICENSE.txt`.

What alwan takes: the film stock profiles in `src/alwan/data/film/`.

## utility-dctls (Thatcher Freeman)

- Upstream: https://github.com/thatcherfreeman/utility-dctls.
- Copyright (c) 2023 Thatcher Freeman.
- Licence: MIT, `licenses/utility-dctls-MIT.txt`.

What alwan takes: the per-layer additive halation form of `Effects/Halation.dctl`, followed by
the experimental halation model.

## color.js (Lea Verou, Chris Lilley)

- Upstream: https://github.com/color-js/color.js (0.5.2).
- Copyright (c) 2021 Lea Verou, Chris Lilley.
- Licence: MIT, `licenses/colorjs-MIT.txt`.

What alwan takes: the CSS colour spaces' matrices and transfer functions, their tree of base
spaces, the handling of missing components, the interpolation of color-mix() and steps(),
and the L* and DeltaPhi* contrasts (`src/alwan/api/alwan_css_color.c`).

## material-color-utilities (Google)

- Upstream: https://github.com/material-foundation/material-color-utilities (0.3.0).
- Copyright 2021 Google LLC.
- Licence: Apache-2.0, `licenses/material-color-utilities-Apache-2.0.txt`.

What alwan takes: HCT (Cam16.fromInt in Material's viewing conditions, HctSolver) and the
tonal palette (`src/alwan/api/alwan_hct.c`), and the solver's critical planes
(`src/alwan/data/hct/critical_planes.csv`), translated from JavaScript to C.

## Tony McMapface

- Upstream: https://github.com/h3r2tic/tony-mc-mapface.
- Copyright (c) 2023 Tomasz Stachowiak.
- Licence: MIT OR Apache-2.0; alwan takes it under MIT, `licenses/tony-mc-mapface-MIT.txt`.

What alwan takes: the 48^3 display transform cube, `src/alwan/data/tony_mcmapface_lut3d.csv`.

## BakingLab (MJP), Stephen Hill's ACES fit

- Upstream: https://github.com/TheRealMJP/BakingLab, BakingLab/ACES.hlsl (the fit by Stephen Hill).
- Copyright (c) 2016 MJP.
- Licence: MIT, `licenses/BakingLab-MIT.txt`.

What alwan takes: the input and output matrices and the RRTAndODTFit coefficients of
`ALWAN_VIEW_ACES_HILL`, in `src/alwan/core/alwan_view_core.inc` and `.h`.

## tonemapper (Tizian Zeltner)

- Upstream: https://github.com/tizian/tonemapper, src/operators/DayFilmicOperator.cpp.
- Copyright (c) 2022 Tizian Zeltner.
- Licence: MIT, `licenses/tonemapper-MIT.txt`.

What alwan takes: the default parameters of `ALWAN_VIEW_DAY_FILMIC` (w 10, b 0.1, t 0.7,
s 0.8, c 2) and its written form of Day 2012's curve.

## rawtoaces-data

- Upstream: https://github.com/AcademySoftwareFoundation/rawtoaces-data.
- Copyright the Academy Software Foundation and contributors.
- Licence: Apache-2.0, `src/alwan/data/camera_sensitivities/rawtoaces/LICENSE.txt`.

What alwan takes: 52 camera spectral sensitivity sets, values unmodified, listed in that
directory's `SOURCE.txt`.

## ACES (Academy of Motion Picture Arts and Sciences)

- Upstream: https://github.com/aces-aswf/aces-core (formerly ampas/aces-dev).
- ACES 1.x (aces-dev v1.3): Copyright 2015 A.M.P.A.S., the Academy's licence terms,
  `licenses/aces-dev-1.3-LICENSE.md`.
- ACES 2.0 (aces-core): Apache-2.0, `licenses/aces-core-Apache-2.0.txt`.

What alwan takes: transform constants, splines and output transform logic read from the
published CTL (`src/alwan/data/splines/`, `src/alwan/data/aces2/`, the ACES 1.x and 2.0
output transforms in `src/alwan/api/`).

## Fogra

- FOGRA39L characterisation data, https://fogra.org.
- Fogra's own terms: free use and redistribution, including in software, provided the data
  is unmodified.

What alwan takes: `src/alwan/data/cmyk/fogra39.inc`, FOGRA39.txt byte for byte.

---

## AgX (EaryChow, Troy Sobotka, Jed Smith)

What alwan takes: `src/alwan/data/agx_blender_lut3d.csv` (EaryChow's AgX,
https://github.com/EaryChow/AgX), `agx_default_contrast_lut.csv` (Troy Sobotka,
https://github.com/sobotka/AgX) and `agx_sb2383_contrast_lut.csv`
(https://github.com/sobotka/SB2383-Configuration-Generation), and the AgX core's sigmoid
(`src/alwan/core/alwan_agx_core.inc`), which follows Jed Smith's `calculate_sigmoid` from
AgXLib in Sobotka's repository.

Licence: none of these repositories states a licence. The authors approved alwan's use of
this material; the owner checked the approval before the 3.0.0 release.

---

## JP2499 (Juan Pablo Zambrano)

What alwan takes: `src/alwan/core/alwan_agx_jp2499_core.inc` ports the picture formation of
Jp-DRT.dctl, https://github.com/jedypod/JP2499 (reference commit
e6fb981cc95af0544a3a24a659ccaad3fce73ed7).

Licence: the repository states no licence. The author's permission covers alwan's port (not
the DCTL file itself, which alwan does not ship); the owner checked it before the 3.0.0
release, and `alwan_dev/gendata/datasets.py` records it on the `jp2499` source.

---

## Freetone palette (Stuart Semple)

What alwan takes: `src/alwan/data/palettes/freetone.inc`.

Licence: the palette carries no licence. It is vendored on the owner's decision, as
`src/alwan/data/palettes/SOURCE.txt` records.

---

## Removed for licence reasons

APCA (apca-w3, https://github.com/Myndex/apca-w3) is "All Rights Reserved" and licensed to
the W3C for WCAG use only, with patents noted as pending. alwan's APCA contrast
(`alwan_apca_contrast`) was removed for 3.0.0 so that nothing under those terms ships.

The haemoglobin extinction table the skin model shipped (Scott Prahl's omlc.org compilation,
via the Virtual Tissue Simulator's MIT-licensed copy) was removed: Prahl's page carries a
copyright notice and no licence, so its MIT compatibility is not plain. The skin model now
takes haemoglobin spectra from the caller (`alwan_skin_params`).
