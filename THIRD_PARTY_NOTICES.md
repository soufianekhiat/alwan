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

## OpenCV and opencv_contrib

- Upstream: https://github.com/opencv/opencv and https://github.com/opencv/opencv_contrib,
  tag 5.0.0.
- Copyright: see `licenses/OpenCV-COPYRIGHT.txt` (Intel Corporation, Willow Garage, NVIDIA,
  AMD, OpenCV Foundation, Itseez, Xperience AI and others).
- Licence: Apache License 2.0 (`src/alwan/data/opencv/LICENSE-opencv.txt`,
  `src/alwan/data/opencv/LICENSE-opencv_contrib.txt`). Some ported files carry older
  BSD-style headers instead, kept as their conditions ask:
  - Intel License Agreement, `licenses/OpenCV-Intel-License-Agreement.txt`:
    `modules/photo/src/inpaint.cpp`, `modules/core/src/dxt.cpp`.
  - 3-clause BSD (Intel, Willow Garage), `licenses/OpenCV-BSD-3-Clause-Intel-WillowGarage.txt`:
    `modules/xphoto/src/dct_image_denoising.cpp`.
  - 3-clause BSD, `licenses/OpenCV-ximgproc-BSD-3-Clause.txt`:
    `modules/ximgproc/src/weighted_median_filter.cpp`, `adaptive_manifold_filter_n.cpp`,
    `edgeaware_filters_common.cpp`.
  - 3-clause BSD (Intel, Willow Garage, Itseez), `licenses/OpenCV-imgproc-resize-BSD-3-Clause.txt`:
    `modules/imgproc/src/resize.cpp`.
  - 3-clause BSD (Copyright (C) 2014, Beat Kueng, Lukas Vogel, Morten Lysgaard),
    `licenses/OpenCV-ximgproc-niblack-BSD-3-Clause.txt`: `modules/ximgproc/src/niblack_thresholding.cpp`.
- OpenCV ships no NOTICE file at that tag.

What alwan takes:

| alwan file | From OpenCV |
|---|---|
| `src/alwan/api/alwan_decolor.c` | `photo/src/contrast_preserve.cpp` (cv::decolor), the float BGR to Lab conversion |
| `src/alwan/api/alwan_gradient_edit.c` | `photo/src/seamless_cloning*.cpp` (Poisson cloning and its edits) |
| `src/alwan/api/alwan_stylize.c` | `photo/src/npr.cpp`, `npr.hpp`; `xphoto/src/oilpainting.cpp` |
| `src/alwan/api/alwan_white_balance.c` | `xphoto/src/simple_color_balance.cpp`, `grayworld_white_balance.cpp`, `learning_based_color_balance.cpp` |
| `src/alwan/api/alwan_inpaint_opencv.c` | `photo/src/inpaint.cpp` (Telea, Navier-Stokes) |
| `src/alwan/api/alwan_dct_denoise_opencv.c` | `xphoto/src/dct_image_denoising.cpp` |
| `src/alwan/api/alwan_cv_dxt.c` | `core/src/dxt.cpp` (DFT and DCT) |
| `src/alwan/api/alwan_am_filter.c` | `ximgproc` adaptive manifold filter |
| `src/alwan/api/alwan_wmf.c` | `ximgproc` weighted median filter |
| `src/alwan/api/alwan_resize_opencv.c`, `alwan_resize_opencv_impl.inc` | `imgproc/src/resize.cpp` (INTER_CUBIC and INTER_AREA: `resizeGeneric_`, `resizeAreaFast_`, `resizeArea_`, `computeResizeAreaTab`) |
| `src/alwan/api/alwan_threshold_local.c` | `ximgproc/src/niblack_thresholding.cpp` (Wolf and NICK binarisation), with `imgproc` boxFilter and sqrBoxFilter's double sums |
| `src/alwan/api/alwan_match_template.c` | `imgproc/src/templmatch.cpp` (`common_matchTemplate`, the six `TM_*` scores) |
| `src/alwan/api/alwan_checker_detect.c` | the cv2 operations the colour checker detector calls (resize, bilateral filter, threshold, contours, minAreaRect, approxPolyDP, warpPerspective) |
| `src/alwan/api/alwan_clahe.c`, `src/alwan/api/alwan_denoise.c` | CLAHE, fast non-local means and anisotropic diffusion, reproduced to OpenCV's arithmetic |
| `src/alwan/data/opencv/rgb2lab_lut_s16.csv`, `lab2srgb_float.csv` | colour conversion tables, read back from cv2 |
| `src/alwan/data/opencv/lbwb_*.csv` | the LearningBasedWB trained model, verbatim |

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

## colour-science (colour, colour-hdri, colour-checker-detection)

- Upstream: https://github.com/colour-science.
- colour: Copyright 2013 Colour Developers, BSD-3-Clause,
  `src/alwan/data/LICENSE-colour-science.txt`.
- colour-hdri: Copyright 2015 Colour Developers, BSD-3-Clause,
  `licenses/colour-hdri-BSD-3-Clause.txt`.
- colour-checker-detection: Copyright 2018 Colour Developers, BSD-3-Clause,
  `licenses/colour-checker-detection-BSD-3-Clause.txt`.

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
- `src/alwan/api/alwan_dng.c`: `highlights_recovery_LCHab` from colour-hdri 0.2.6.
- `src/alwan/api/alwan_checker_detect.c`: the segmentation detector of
  colour-checker-detection 0.2.3.

## scikit-image

- Upstream: https://github.com/scikit-image/scikit-image, v0.26.0.
- Copyright 2009-2022 the scikit-image team, with the per-file holders listed in its licence
  (Massachusetts Institute of Technology, Broad Institute, Lee Kamentsky, Peter J. Verveer,
  University of Wisconsin-Madison and others).
- Licence: BSD-3-Clause, `licenses/scikit-image-BSD-3-Clause.txt`.

What alwan takes: routines reproduced from scikit-image's code, among them the watershed
heap, `find_contours` and `approximate_polygon` (`alwan_contour.c`), the skeletonisation
passes transcribed from `_skeletonize_various_cy.pyx` and the medial axis from
`_skeletonize.py` (`alwan_morphology.c`), the convex hull's `possible_hull` and
`point_in_polygon` (`_convex_hull.pyx`, `_shared/geometry.pyx`; `alwan_morphology.c`), the
Moravec and FAST corner loops (`feature/corner_cy.pyx`, `alwan_corner.c`), the multi-Otsu
search (`filters/_multiotsu.pyx`, `alwan_threshold.c`), Felzenszwalb's graph segmentation and
quick shift (`segmentation/_felzenszwalb_cy.pyx`, `segmentation/_quickshift_cy.pyx`,
`alwan_segment_ext.c`, which also follows `_chan_vese.py` and
`random_walker_segmentation.py`), the co-occurrence loop and properties
(`feature/_texture.pyx`, `feature/texture.py`), the HOG gradients, cell histograms, block
norms and visualisation lines (`feature/_hog.py`, `feature/_hoghistogram.pyx`,
`draw/_draw.pyx`; `alwan_texture_descriptors.c`), the Gabor kernel and filter
(`filters/_gabor.py`, `alwan_texture.c`), total
variation denoising (`alwan_denoise.c`), the thresholds, and the stain and video matrices in
`src/alwan/data/stain/` and `src/alwan/data/video/`, read from `skimage/color/colorconv.py`.

## SciPy

- Upstream: https://github.com/scipy/scipy, v1.16.3.
- Copyright (c) 2001-2002 Enthought, Inc. 2003, SciPy Developers.
- Licence: BSD-3-Clause, `licenses/scipy-BSD-3-Clause.txt`.

What alwan takes: the B-spline interpolation of `scipy.ndimage.map_coordinates` for
`ALWAN_WARP_BSPLINE3` and `BSPLINE5` (`src/alwan/api/alwan_warp.c`), from
`scipy/ndimage/src/ni_splines.c` (`get_spline_interpolation_weights`, the filter poles,
`apply_filter` with its reflect initialisations) and the 12-pixel edge padding of
`_interpolation.py`.

## Pillow

- Upstream: https://github.com/python-pillow/Pillow.
- Copyright 1997-2011 Secret Labs AB, 1995-2011 Fredrik Lundh and contributors, 2010
  Jeffrey A. Clark and contributors.
- Licence: MIT-CMU (HPND), `licenses/Pillow-MIT-CMU.txt`.

What alwan takes: routines reproduced from Pillow's libImaging to its arithmetic: the median
cut, octree and maximum coverage quantisers (`alwan_palette_median_cut.c`,
`alwan_quantize_octree.c`, `alwan_quantize_max_coverage.c`), resampling (`alwan_resize.c`),
`Image.transform` (`alwan_warp.c`) and the box-blur unsharp mask (`alwan_sharpen.c`), all in
`src/alwan/api/`.

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

## Virtual Tissue Simulator (haemoglobin extinction spectra)

- Upstream: https://github.com/VirtualPhotonics/VTS, file
  `src/Vts/Modeling/Spectroscopy/Resources/SpectralDictionary.txt` at commit 3b6a8830
  (pinned in alwan_dev's gendata/datasets.py).
- Copyright (c) 2026 Virtual Photonics Technology Initiative.
- Licence: MIT, `licenses/VTS-MIT.txt`.
- Provenance: the VTS's Hb and HbO2 values are Scott Prahl's compilation of the molar
  extinction of haemoglobin (omlc.org, "Tabulated Molar Extinction Coefficient for
  Hemoglobin in Water", from W. B. Gratzer and N. Kollias), which carries a copyright notice
  ("(c) SAP 4 March 1998") and no licence. alwan relies on the VTS's MIT grant for these
  values and credits Prahl, Gratzer and Kollias as their source.

What alwan takes: the two spectra from 350 to 850 nm in `src/alwan/data/skin/haemoglobin.csv`
(converted to cm^-1/M by alwan_dev's `gendata/data/skin_haemoglobin.py`), read by the skin
model (`src/alwan/api/alwan_skin.c`). The model's other coefficients are published formulas
(Donner and Jensen 2006, Jacques 2013, Star et al. 1988) implemented by alwan.

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

## PyWavelets

- Upstream: https://github.com/PyWavelets/pywt.
- Copyright (c) 2006-2012 Filip Wasilewski, (c) 2012 onwards the PyWavelets Developers.
- Licence: MIT, `licenses/PyWavelets-MIT.txt`.

What alwan takes: the wavelet filter banks in `src/alwan/data/wavelets/`, exported from
PyWavelets.

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
