## [Unreleased]

### Fixed: output differs

- **ZCAM was wrong, forward and inverse, and had never been tested.** Suite 28 loaded a
  colour-science reference, printed each mismatch and returned success, so it printed nine of
  twelve on every run. Against the reference, lightness was off by up to 18 of 100. Four
  causes. The surround factor F_s was read from the F column, 1.0 / 0.9 / 0.8 where the model
  has 0.69 / 0.59 / 0.525, and F_s is an exponent, so this alone moved a dark grey's J_z from
  34.4 to 21.3. The achromatic response was Jzazbz's (L' + M') / 2 where ZCAM defines
  I_z = M' - epsilon, which put a saturated red out by 6.7. The model's first step,
  adaptation of the stimulus to D65 by the Zhai 2018 CAT over CAT02, was missing, and
  `discount_illuminant` was ignored with it. And the inverse read J_z through Jzazbz's curve
  and ignored every viewing condition, so it could not round-trip; a note in suite 93 put that
  down to f32 conditioning and omitted the test.

  ZCAM now follows Safdar, Hardeberg and Luo (2021) as colour-science implements it, on the
  Safdar 2021 Izazbz matrices alwan already vendors. Suite 28 asserts all nine correlates and
  the inverse over 66 cases, under three surrounds, illuminant A, a white at Y = 500 and a
  discounted illuminant: 5e-11 relative, 1e-13 of the white's luminance on the inverse, and
  4e-8 for the round trip through the f32 API. Every ZCAM correlate changes, and with it
  `alwan_zcam_to_ucs_{T}`. `alwan_delta_e_zcam_{T}` is a distance between two UCS points and
  is itself unchanged. The core signatures changed arity on purpose, from nine arguments to
  six, `(xyz, xyz_w, Fs, D, La, Y_b)`, so a shader calling the old form stops compiling rather
  than silently taking F_s where F used to go. `alwan_zcam_degree_of_adaptation_v` is new.
  Several documents described the inverse as iterative. It never was.

- **The OSA-UCS inverse was an approximation with invented coefficients.**
  `alwan_osa_ucs_to_xyz_{T}` recovered the cube roots of RGB from j and g with three
  numbers that appear in no paper, and dropped the 0.042 term of Lambda. It returned
  D65 white as (107, 111, 123), eleven per cent off. Its test ran that one colour,
  printed the miss, and passed, on every run. The inverse is now the reference's: a
  cubic for Y0 solved by Cardano, then Newton on the red cube root with the two linear
  forms of j and g, twenty fixed steps. It agrees with colour-science's `OSA_UCS_to_XYZ`
  to 3e-11 and round-trips XYZ to 2e-11, checked in suite 24 over the same pairs the
  forward transform is checked on. The header called it approximate; it is exact.

- **Zhai 2018 scaled with its destination white.** `alwan_cat_zhai2018_{T}` computed
  D * (RGB_o / RGB_w) + 1 - D, where Zhai and Luo, and colour-science, carry the luminance
  ratio: D * (Y_w / Y_o) * (RGB_o / RGB_w) + 1 - D. With every white at Y = 100, which is what
  the header asked for, the ratio is 1 and nothing changes. With a destination white written
  at Y = 1 beside a source at Y = 100, which is how ZCAM calls it, the result came out a
  hundred times too dim at D = 1. The scale of each white is now free. `zhai2018.csv` had
  been generated from colour-science since the function was written and no suite read it;
  suite 144 does now, with 27 more cases on two scales.

- **Dominant wavelength and excitation purity were wrong for purples.** The locus was
  not closed by the line of purples, so `alwan_dominant_wavelength_{T}` returned
  `ALWAN_E_INVALID` for every purple, and `alwan_excitation_purity_{T}` measured a
  purple against the opposite side of the locus: 0.333 where the CIE definition and
  colour-science give 0.640. A purple now has the negated complementary wavelength,
  `xy_wl_out` on the line of purples and `xy_cw_out` on the spectrum locus, as
  colour-science's `dominant_wavelength`, and `alwan_complementary_wavelength_{T}`
  returns the negated dominant wavelength when its own ray meets the line of purples.
  Excitation purity is no longer clamped to [0, 1]: a chromaticity outside the locus
  reads above 1. Segments were skipped as parallel below a 1e-10 determinant, which the
  locus's 1 nm steps near 830 nm fall under; only exact parallels are skipped now.
  Checked against colour-science on 216 chromaticities in suite 134.

- **V(lambda) and V'(lambda) were 10 nm tables.** gendata resampled colour-science's
  1 nm CIE data to 10 nm over 380-780 nm, and alwan interpolated linearly between the
  samples, so `alwan_luminous_efficiency_{T}` was off between every tenth nanometre
  and the photopic, scotopic and mesopic luminance integrals of a narrow band were off
  by up to 1.5e-2. The tables are colour-science's own now, 1 nm over 360-830 nm and
  380-780 nm, and suite 41 checks them to 1e-12 where it allowed 0.01.

- **The deterministic BT.709 and BT.2020 curves split at the wrong side of 0.018.**
  They shared sRGB's helper, which puts the break itself on the linear segment; the
  fast build and colour-science put it on the power segment. At exactly 0.018 the
  deterministic OETF returned 0.081 instead of 0.081248, and the EOTF likewise at
  0.081. Only those two inputs change, to the fast build's values.

- **An unknown integration method ran Simpson's rule.** `alwan_xyz_from_spd_{T}` and
  `alwan_xyz_from_spd_camera_{T}` treated every value other than
  `ALWAN_INTEGRATE_TRAPEZOID` as Simpson, so an out-of-range method integrated
  instead of failing. It is `ALWAN_E_INVALID` now.

- **The f32 Finlayson 2015 fit wrote past its buffers.** It solved into a stack
  array of 22 x 3 values, where the plain degree-4 basis has 34 terms, and then
  copied `matrix_size` x 3 values, although `matrix_size` is already the element
  count. Every basis of 8 or more terms wrote past the caller's matrix. It now
  copies exactly `matrix_size` values from a buffer of the widest basis. Suite 124
  guards the caller's buffer at every degree.

- **TM-30 Rf and the CQS colour differences were wrong at the shipped**
  **default.** Both computed appearance differences through the public API and
  then did arithmetic on the result in native units.

  `alwan_tm30_rf_{T}` calls `alwan_ciecam02_forward` and then forms CAM02-UCS
  by hand, with the literal `100.0` in `J'` and `PI / 180` on the hue. At
  `ALWAN_NORMALIZE_RANGES=1` the wrapper hands back `J` scaled by `0.01` and
  `h` by `1/360`, so both went into formulas written for the other scale. The
  measured residual against colour-science over 35 illuminants was a mean of
  0.82 where it should be 0.0010, and no test saw it because every build in
  both repos compiled the other branch.

  CQS has the same shape: it takes two `alwan_xyz_to_lab` results and computes
  `sqrt(dL^2 + da^2 + db^2)` itself. Only `L` is a bounded channel, so `dL`
  arrived a hundred times smaller than `da` and `db` and the root mixed two
  scales.

  Both denormalise before the arithmetic now. A default build is unchanged and
  the two configurations agree to every digit printed.

  The neighbouring CRI path was checked and is correct: it hands its Lab
  straight to `alwan_delta_e_76`, which denormalises its own inputs, so the
  two cancel.

- **The .cube reader accepted infinities and NaN.** `sscanf("%lf")` turns
  `1e999` into `HUGE_VAL` and `nan` into a quiet NaN without failing, so a row
  that parsed was not yet a row worth storing, and nothing checked. One
  infinity in a 3D LUT is worse than one bad entry: every sample that
  interpolates through that corner comes back non-finite, so a single
  character in a downloaded `.cube` poisoned a whole cell of the table.

  All six readers now reject a non-finite entry with `ALWAN_E_RANGE`: 3D and
  1D, path and buffer, `f32` and `f64`. The declared-size and row-count
  guards were already sound and are unchanged.

  Also worth knowing, and now covered by a test rather than left implicit: the
  size query (passing a `NULL` lut) returns as soon as it reads
  `LUT_3D_SIZE` and never looks at the body. A successful size query says the
  header is sane and nothing more, so it is not validation of the file.

- **The CCM fits solved through the normal equations and lost most of their
  precision doing it.** `alwan_colour_correction_matrix_cheung2004_{T}` and
  `..._finlayson2015_{T}` formed `AtA` and ran Gaussian elimination on it,
  which squares the condition number. They now factor `A` itself with a
  Householder QR. Same call, same arguments, same results on well-conditioned
  input; the difference is how much of the answer survives when the basis is
  not.

  Measured by recovering a known matrix from samples generated through it,
  which is exact in the linear case, so every digit lost belongs to the
  solver. Cheung at 35 terms goes from 2.9e-10 to 1.1e-13. The Finlayson
  root-polynomial at degree 4, 22 terms, goes from **1.3e-03 to 4.9e-11**.

  That last one was the case worth fixing. Its terms are `sqrt(RG)`,
  `cbrt(R2G)`, `(R3G)^(1/4)`: the same shape with slowly separating exponents,
  so the columns crowd together and the fit was losing about thirteen digits.
  It is the exposure-invariant model, which is what a caller reaches for when
  the lighting is uncertain, so it was least trustworthy exactly where it was
  most wanted.

  The fixed 35-term stack arrays went with the old body. The solver allocates
  to the problem now, so nothing structural caps the term count.

- **The bulk paths ignored `ALWAN_NORMALIZE_RANGES` for some spaces and
  honoured it for others.** At the shipped default,
  `alwan_xyz_to_hunter_lab_f64` and `alwan_xyz_to_hunter_lab_f64_map_interleave`
  are the same public conversion and disagreed in `L` by 99.0, the whole
  normalisation factor: the scalar wrapper applied `ALWAN_NORM_HUNTER_LAB` and
  the bulk path did not, while `alwan_xyz_to_lab_f64` and its bulk twin agreed
  to 3e-16 in the same build. A caller mixing the two got lightness a hundred
  times out on one of them, and hue three hundred and sixty times out for the
  cylindrical spaces.

  Every bulk entry point for a space with an `ALWAN_NORM_*` macro now applies
  it: the interleave wrappers for Hunter Lab, ProLab, DIN99 and UVW, the planar
  wrappers for those plus HCL and IHLS, the YCoCg planar pair, and both
  directions of the sRGB/Lab convenience pair. The planar generators take
  explicit channel hooks so each generated function states its scaling at the
  call site; they expand to nothing when the setting is off, so a default build
  is untouched.

  `alwan_srgb_to_lab_{T}` and `alwan_lab_to_srgb_{T}` were wrong on the scalar
  side as well. They call the `_v` core directly and so skipped the
  `ALWAN_NORM_LAB` that every other scalar wrapper applies.

- **The deterministic sRGB and BT.2020 OETF returned garbage above 1.0.**
  The high-side polynomial is fitted on `[split, 1]`, and the evaluator
  extrapolated it for any larger input instead of falling back: a
  degree-14 polynomial outside its domain diverges, so linear `4.0`
  encoded to `-9.2e+10` rather than `1.82`. Scene-linear values above 1
  are ordinary, so this was reachable by any HDR or ACES caller of a
  deterministic build. Both OETFs and both EOTFs now hand the
  out-of-domain case to `alwan_det_pow_pos`, which is unbounded. Values
  inside the fitted domain are untouched, so no already-correct output
  moves.

- **`ALWAN_FIT_LOCK_SCALE` and `ALWAN_FIT_LOCK_TF` did not hold their
  value.** The fit stores scale and exponent as logarithms, and the
  result was decoded back with `exp` even when locked. libm returns
  exactly `1.0` for `exp(log(1.0))`, which hid it; the deterministic
  polynomials do not, and a locked scale of 1.0 came back
  `1.0000000000026`. A locked parameter is now written back as the caller
  gave it rather than round-tripped.

- **YcCbcCrc double-offset its chroma in a default build.**
  `alwan_rgb_to_yccbccrc_{T}` already centres `Cbc` and `Crc` on the
  legal-range midpoint, `0.500489` at 10 bit, and `ALWAN_NORM_YCCBCCRC`
  then added a further `+0.5`. With `ALWAN_NORMALIZE_RANGES` at its
  shipped default of `1`, achromatic grey encoded to `1.000489` instead
  of `0.500489`, in-gamut chroma spanned roughly `[0.5626, 1.4384]`
  rather than `[0, 1]`, and a standards-conformant Y'cCbcCrc signal could
  not be decoded. The macro is a no-op now, as `ALWAN_NORM_YCBCR` has
  been since the identical defect was fixed there on 2026-08-27; this is
  the constant-luminance twin that was missed then.

  The scalar path had been disagreeing with the bulk kernels, which never
  added the offset, and with the committed reference CSV, which holds the
  un-offset value. No test caught it because every build in both repos
  compiles the library at `ALWAN_NORMALIZE_RANGES=0`, where the macro was
  already inert. YCoCg is not affected: its kernel emits `Co` and `Cg`
  centred on 0, so the `+0.5` there is the correct mapping.


- **The ACES 1.x HDR outputs are the ACES 1.1 to 1.3 transforms.**
  `ALWAN_ACES1_OUT_REC2020_{1000,2000,4000}NIT_PQ` evaluated the 1.0.3
  `ODT.Academy.Rec2020_ST2084_*nits` C9 spline, 0.18 at 10 cd/m2, under the
  default setting, and OCIO's fit of the 1.1 curve, 0.18 at 15 cd/m2, under
  `ALWAN_ACES_INTERP_OCIO`: one enum value, two transforms half a stop
  apart. The Single Stage Tone Scale of `ACESlib.SSTS.ctl` is implemented
  now, knots built from Y_MIN, Y_MID and Y_MAX as the CTL builds them,
  forward and inverse, with the RRTODT's clip to the Rec.2020 primaries
  before the adaptation to D65 and its stretched black. Those three values
  are the `RRTODT.Academy.Rec2020_*nits_15nits_ST2084` transforms in every
  setting, so mid-grey moves from 10 to 15 cd/m2 on the default path, 34 PQ
  codes of 1023. Against OCIO's ACES 1.1 view the default path now sits
  within 0.05 of a code away from black, and the evaluator matches the CTL
  transcription to 1.3e-13. The 1.0.3 ODTs remain under
  `ALWAN_ACES1_OUT_REC2020_*NIT_PQ_V103`; all fifteen outputs round-trip
  through the inverse at 1.3e-11.

- **RGB-space transfer functions audited against colour-science.** Neither of
  the two tables naming each space's curve had ever been compared with
  anything. 19 spaces were wrong: CIE RGB, Adobe Wide Gamut, Best, Beta,
  Don 4, Ekta Space PS5, Max, Russell and Xtreme were linear and are gamma
  2.2; SMPTE-C and NTSC 1987 were BT.709 and are gamma 2.2; NTSC 1953,
  PAL/SECAM and BT.470 were BT.709 and are gamma 2.8; P3-D65 was sRGB and
  is gamma 2.6; EBU Tech. 3213-E defines primaries only and is linear;
  GAMMA18_REC709 and DaVinci Intermediate were linear and now carry their
  curves. `gendata/gen_rgb_space_tf_reference.py` emits the reference for
  79 of the 104 spaces and suite 43 holds every one to 1e-6.

- **Eight transfer functions the library did not have**, appended to the
  enum so nothing renumbers: `ALWAN_TF_GAMMA18` (Apple RGB, ColorMatch),
  `ALWAN_TF_ROMM` (ProPhoto and ROMM, gamma 1.8 with a linear toe below
  1/512), `ALWAN_TF_RIMM`, `ALWAN_TF_ERIMM`, `ALWAN_TF_LSTAR` (ECI RGB v2),
  `ALWAN_TF_SMPTE240M`, `ALWAN_TF_ADOBE_RGB` (563/256 = 2.19921875, which
  Adobe RGB and Adobe Wide Gamut carried as 2.2, wrong by 4.2e-4) and
  `ALWAN_TF_DAVINCI_INTERMEDIATE`.

- **`alwan_rgb_space_get_tfs` answers for every space.** It named 20 of the
  104 spaces in a hand-written switch and refused the rest, ACEScct and every
  camera log space among them. It reads the descriptor table now.

- **TM-30 Rf took the CCT on the wrong observer.** The reference illuminant
  was chosen from a CCT computed with the 10 degree white against the 2
  degree Robertson locus, which put Illuminant A at 2789 K rather than
  2856 K and pulled every Planckian reference off. The CCT is read on the 2
  degree observer, as CRI and CQS already did. The sweep against
  colour-science over 35 illuminants goes from 0.530 mean and 1.990 max to
  0.0010 and 0.0058, which closes the item listed as known in 2.0.0.

### Breaking

- **Cameras are registry indices; the `alwan_camera_sensitivity` enum is gone.** The enum
  named two NPL cameras and lived beside the 52-camera registry, so a camera had two kinds
  of identity depending on which call reached it. The two NPL cameras now join the
  registry after the pack, at indices 52 and 53, as `"Nikon" "5100 (NPL)"` and
  `"Sigma" "SDMerill (NPL)"` under colour-science's names, and every camera call takes an
  index.

  Removed: `alwan_camera_sensitivity`, `alwan_spd_camera_sensitivity_{T}` and
  `alwan_xyz_from_spd_camera_{T}`, which returned camera RGB in an `alwan_xyz`.
  `alwan_camera_rgb_from_spd_{T}` now takes `size_t camera`, the registry index, and
  `alwan_camera_sensitivities_{T}` covers all 54 cameras, each on its own grid: 380-780 nm
  at 5 nm for the pack, 360-830 nm at 1 nm for the NPL pair. `alwan_camera_count` returns
  54, or 0 in a build with no camera table at all. Code that walks every index and
  stacks the curves on one grid will now meet two 471-sample cameras at the end; check
  each camera's `count` and range, or stop at 52 for the pack alone.

  Migration: `ALWAN_CAMERA_NIKON_5100` is 52, `ALWAN_CAMERA_SIGMA_SDMERILL` is 53, and
  `alwan_xyz_from_spd_camera` becomes `alwan_camera_rgb_from_spd` reading `.r .g .b`. The
  NPL cameras' numbers are unchanged to the bit: the det dump's camera sections, 4,370
  lines of sensitivities and camera RGB over a blackbody sweep, are byte-identical
  between the old calls and the new. An unknown camera is now `ALWAN_E_RANGE`, the
  registry's answer, where the enum gave `ALWAN_E_INVALID`.

- **`ALWAN_TABLE_CMF_CIE_2012_2DEG` and `_10DEG` are removed.** CIE 2012 and CIE 2015
  are one observer, the CMFs from the CIE 2006 cone fundamentals that CIE 170-2:2015
  standardised, and alwan stored it twice: six `cie_2012_*` tables byte-identical to
  the `cie_2015_*` ones. The copy is gone.

  `ALWAN_OBSERVER_CIE_2012_2DEG` and `_10DEG` keep their values and now read the 2015
  tables, so no result changes; suite 12 checks both names return the same CMFs bit
  for bit in both precisions. What breaks is configuration. A build that set
  `ALWAN_TABLE_CMF_CIE_2012_*` to compile the observer out now stops with an `#error`
  naming the replacement, `ALWAN_TABLE_CMF_CIE_2015_*`, which controls both names.
  A silent no-op would have left the tables in while the build believed them out.

### Fixed

- **Every CLF alwan exported with a view transform had its 3D LUT transposed.** CLF
  orders a LUT3D array with blue varying fastest and red slowest; alwan holds a cube the
  other way round, R-fastest, which is what `.cube` stores on disk, and
  `alwan_clf_export_view_{T}` treated the two as the same and said so in a comment. The
  effect was an exchange of the red and blue axes: greys sit on the diagonal and came
  back correct, which is why it survived, while a saturated colour came back as a
  different colour. Measured against OpenColorIO reading the file, ACEScg (0.6, 0.15,
  0.1) through an exported CLF came back (0.31, 0.27, 0.68) where alwan's own pipeline
  gives (0.67, 0.19, 0.24).

  Fixed, and pinned: suite 80 reads the array back out of the file alwan just wrote and
  compares it against the same cube baked directly, in both orders. The CLF order has to
  match to 0.0 and alwan's own order has to not, since a cube symmetric enough to satisfy
  both would prove nothing; it is off by 0.94. With the fix, five of six probe colours
  agree with OCIO to 2e-4. The sixth is a saturated ACEScg red that converts to linear
  sRGB with a channel at 1.315, which the CLF's gamut-clamp Range node clips and the
  baked cube does not, so that one differs by design rather than by error.

- **The experimental RGB fit could read past a buffer.** `alwan_rgb_fit_params.percentile`
  becomes an index into the sorted per-sample errors, and nothing validated it. At 1.5
  the block path read past the end of that buffer, since it clamped nothing, and a NaN or
  a negative value made the float-to-index cast undefined on both paths. Every entry
  point now refuses a percentile outside [0, 1], NaN included, with `ALWAN_E_INVALID`.
  The block path also clamps its index, because in f32 `(used - 1)` rounds up past 2^24
  samples even inside the range. And `alwan_rgb_fit_blocks_solve_{T}` read `space` and
  `params` before checking either, so a NULL crashed rather than returning
  `ALWAN_E_INVALID`. Suite 107 covers all three. Found by the table registry's
  float-to-index inventory, which had been reporting the site without anyone looking.

- **The hue quadrature was wrong, in CIECAM02 and in CAM16.** CIE 159:2004 interpolates
  H on `(h - h_i) / e_i`. alwan multiplied by `e_i` instead of dividing, and collapsed the
  standard's two end sectors into one sector reached by adding 360 to any hue below 20.14.
  Both are now as the standard has them: below the first tabulated hue H runs from 385.9 to
  400 on its own coefficients, and from 237.53 the last sector closes on 360 with `e =
  0.856` and a span of 85.9. The corrected function agrees with colour-science to 5.7e-14
  across a 40,001 point sweep of the hue circle, and the joins land where they should:
  H(360) = 385.9 = H(0), H(20.14) = 400.

  **This moves the H correlate that CIECAM02 and CAM16 report.** Nothing else in either
  model changes; J, C, h, Q, M and s are untouched.

  It survived because nothing ever looked at it. Suites 13 and 14 each load seven
  correlates per colour from a reference that has carried colour-science's correct H all
  along, and compared six of them. Both now compare the seventh.

### Added

- **Neural layer kernels (roadmap 3.10, step two).** `alwan_nn_dense_{T}`,
  `alwan_nn_conv2d_{T}` (stride, zero padding, groups and depthwise),
  `alwan_nn_activation_{T}` (ReLU, leaky, sigmoid, tanh, GELU in the tanh form),
  `alwan_nn_pool2d_{T}` (max and average), `alwan_nn_global_avg_{T}`,
  `alwan_nn_upsample2d_{T}` (nearest and PyTorch's align_corners=False bilinear),
  `alwan_nn_softmax_{T}`, `alwan_nn_add_{T}` and `alwan_nn_concat_channels_{T}`, each
  the per-element kernel of `core/alwan_nn_core.h` in a loop over the output. The
  kernels are written against `ALWAN_NN_READ_*` accessors, the seam the table readers
  proved, so a shader binds its own buffers and there is no GPU twin to drift; every
  accumulate runs in a fixed order into an `ALWAN_DET_PRECISE` local with no fused
  multiply-add, which is what makes a deterministic build bit-exact across backends and
  the reason to run a network here at all. Tensors are channels-last with HWIO weights;
  a converted model carries them in that order. Suite 176 holds the f64 path to PyTorch
  in float64 on seeded random tensors (worst 9e-16), the f32 path to the f64 one (worst
  2e-7), and the whole-tensor forms to the per-element kernels bit for bit.
  `docs/api/nn.md`. On the GPU, `alwan_dev/hlsl_regression/run_nn_parity.py` binds the
  kernels to StructuredBuffers and runs them on D3D12 WARP under dxc and fxc against the
  pointer-bound C kernels: every tensor kernel is bit-exact in an ordinary build and in a
  deterministic one, and the exp-based activations and softmax are bit-exact in the
  deterministic build, which is the setting the claim is made in.

- **A PyTorch model becomes a generated C function (roadmap 3.10, step three).**
  `alwan_dev/gendata/nn_convert.py` walks a `torch.nn.Sequential` and emits an `.inc`:
  the weights as static `float` arrays and a `<name>_forward(out, in, arena)` that calls
  the `alwan_nn_*_f32` layers in order with every shape a compile-time constant, so
  inference allocates nothing and the arena's size is a `#define`. Batch normalisation
  (2d and 1d) is folded into the convolution or dense layer before it; Dropout and
  Identity emit nothing; every weight is permuted once into channels-last / HWIO; and
  the one permutation that is easy to get wrong, the columns of a Linear that follows a
  Flatten (torch flattens c-major, alwan's activation is an H x W x C run), is applied
  exactly once. There is no model file reader and there will not be one. Suite 177 runs
  two models defined in `gendata/tests/nn_demo_reference.py` end to end, which between
  them use every layer the converter accepts, against torch in float64 on the same
  float32 weights with batch norm unfolded: worst 1.9e-8 and 4.6e-8 on outputs of order
  one, where a permutation or fold mistake is order one. The first target network's
  weights are non-commercial and cannot ship, which is why the converter is proven on
  models the repository defines. `docs/api/nn.md`.

- **Every public operation has a reference entry, and the count is a gate.**
  `alwan_dev/tools/check_doc_coverage.py` folds the precision and buffer-form suffixes
  and asks which base operations appear nowhere under `docs/`: 54 of 687 on the morning
  of 2026-09-22, 0 of 675 by the end of it (the `_batch` forms fold into their metric).
  The 54 went on the pages their families live on, written from their header comments:
  the camera and DNG helpers in `api/spectral.md`, the ISO 12232 exposure functions and
  Mantiuk 2006 in `api/hdr.md`, the illuminant chromaticity accessors, the CGATS.17
  writers and the CMYK inverse cache in `api/reference-data.md`, the batch colour
  differences and Michaelis-Menten in `api/color-difference.md`,
  `alwan_rgb_space_limits` in `api/color-spaces.md`, and a new `api/display.md` for the
  GOG fits, the display model and the calibration LUT.

- **The twenty `alwan_data_get_*` accessors refuse a NULL argument.** None checked its
  `data` or `count` pointer, and no suite called any of them; writing their entry found
  it. `ALWAN_E_INVALID` now, and suite 113 calls the eight illuminant chromaticity
  accessors, checks each returns its two embedded values and the same pointer every
  time, and that E is the equal-energy point.

- **`alwan_image_convert_data_{T}`: the data semantic, decided once for the image.**
  `alwan_data_semantic` has been declared since 2.0.0 and nothing in the library read
  it. This does what its three comments say and no more: `ALWAN_DATA_COLOR` is
  `alwan_image_convert_{T}` to the byte; `ALWAN_DATA_NON_COLOR` keeps the numbers and
  changes only the pixel format, so a normal map, a mask or a displacement is not colour
  managed (U8 0..255 becomes F32 0..1 and comes back exact; the same format in and out is
  a row copy, every bit pattern kept, F16 NaN included); `ALWAN_DATA_UNKNOWN` is
  `ALWAN_E_INVALID`, because the enum says the application decides. Suite 75.

- **PQ's EOTF gave a different answer for the same value depending on where it sat in
  the buffer.** In a fast build, `alwan_eotf_apply_{T}` with `ALWAN_TF_PQ` runs four
  lanes through a SIMD kernel and the remainder through the scalar. For an encoded value
  above PQ's domain (4.0, say), the scalar returns NaN, since the ratio under the final
  power goes negative and no luminance is right for it; the SIMD kernel clamped that
  ratio to zero and returned 0. So a 9-element buffer with 4.0 at positions 0 and 8
  came back 0 at one end and NaN at the other. Both copies of the kernel (the one the
  generic apply uses and the one the ICtCp maps use) now do what the scalar does, and
  suite 34 pins the lanes and the tail agreeing. In-domain values were never affected;
  a determinism build was never affected either, since its lanes already ran the
  scalar. OUTPUT CHANGED for PQ-encoded input above 1 on the SIMD lanes only: NaN
  where it was 0.

- **What every transfer curve does outside [0, 1] is now stated and pinned.** sRGB
  continues its linear toe below zero (12.92 x, so -0.5 encodes to -6.46) and its power
  curve above one, and a round trip returns the value either side; that is CLF's
  `monCurveFwd` and OpenColorIO's default, and NOT the mirrored `monCurveMirrorFwd`
  (which -0.5 would encode to -0.74), which alwan does not offer through the enum. The
  pure gammas, PQ and HLG encode a negative input as they encode zero (for PQ that is
  c1^m2 = 7.3e-7, not 0) and decode a negative encoded value to 0. Suite 34. Nothing
  changed here but the header and the test; the first draft of the test assumed the
  mirrored form, failed, and that is how the contract got written down.

- **`alwan_interop_query` and `alwan_interop_query_id`: what the interop tables know
  about a space, as fields.** A UI or a file writer that has to decide, rather than look
  up one string, gets the formatted ID, the Forum's display-referred ID where one is
  published beside a scene-referred one (`srgb_rec709_display` for sRGB), whether the ID
  is a Forum ID or the `alwan:` namespace, scene or display referred by the ID's own
  suffix, HDR by the descriptor's transfer function (PQ or HLG), and that transfer
  function. Every field is derived from the tables and the descriptor; nothing is a
  judgement alwan makes on its own, which is why "basic" in the Forum's sense is NOT a
  field: the recommendation text is not vendored, so the subset cannot be transcribed,
  and a guess would be worse than the absence. Suite 59 pins named cases and the counts
  over the whole table: 52 entries, 19 published, 15 scene-referred, 12 display-referred,
  3 HDR.

- **`alwan_gamut_volume_perceptual_{T}`: the RGB cube's volume in Lab, Oklab or XYZ,
  deterministically.** The header used to say a perceptual gamut volume "would require
  Monte Carlo sampling". It does not: the image of the cube under a smooth injective map
  has the volume of the integral of |det J|, and a tetrahedral mesh evaluates it. An n^3
  lattice of RGB points is mapped into the target, each cell is cut into six tetrahedra
  along its diagonal, and their volumes are summed. No hull, so a concave boundary is
  measured as concave, and no random numbers, so two calls agree to the bit. For a linear
  target it is exact at n = 1, reproducing `alwan_gamut_volume`'s |det M| to every digit;
  in Lab and Oklab it converges as a mesh does, moving 3e-3 from n = 32 to 64 and 5e-4
  from 64 to 96 on sRGB. Lab is taken relative to the space's own white. Memory is two
  (n + 1)^2 slabs from the context's allocator or the default one.

  Suite 18 pins it against colour-science's `RGB_colourspace_volume_MonteCarlo` at 1e7
  samples on four D65 spaces, within four of that sampler's own sigmas: sRGB in Lab is
  about 8.2e5 by both. Two things learned about that sampler are in the gendata script:
  run in parallel it hands one `random_state` to every worker and came out 2% high, so it
  is run single-process, which is also faster; and it is Lab only, so Oklab is pinned
  against colour's conversion through an independent quadrature, and the test says so.

- **Planar buffer forms for the twenty-one operations whose interleave form has no
  scalar behind it.** The ACES 1 and 2 output transforms, camera RGB to ACES2065-1, the
  Zhai 2018 adaptation, CLF apply, the DNG highlight blend, the four BT.2408 conversions,
  PU21 encode and decode, the four film look stages and `alwan_film_render_rgb`, AgX,
  JP2499, the view transform and the matrix transform each gain a `_map_planar` twin in
  both precisions (`api/alwan_planar_maps.c`, 42 exports). Their interleave bodies are
  either inline in the loop or a SIMD kernel, so a planar loop over a scalar could not be
  bit-identical to them. Each twin instead gathers a 256-pixel tile out of the three
  planes into packed triples, runs the interleave form on the tile and scatters back,
  which is identical by construction, kernels included. Suite 175 asserts it per pixel
  over 300 pixels, so a tile boundary is crossed, with a plane stride that is not the
  interleave stride, and that the verdict on an empty buffer is the interleave form's own
  (CLF says `ALWAN_E_INVALID`, the rest `ALWAN_OK`). With the eleven below, every
  operation that fits the planar convention now has one. `alwan_film_render` does not:
  it takes a spectrum per pixel, not three channels.

- **Planar buffer forms for the eleven operations that had only an interleaved one.**
  `alwan_ipt_to_iptch_{T}`, `alwan_iptch_to_ipt_{T}`, `alwan_hdr_gamut_map_jzczhz_{T}`,
  `alwan_colour_correct_cheung2004_{T}`, `alwan_colour_correct_finlayson2015_{T}`,
  `alwan_gamut_map_advanced_{T}`, `alwan_gamut_map_xyz_to_rgb_{T}`,
  `alwan_simulate_cvd_gamut_safe_{T}`, `alwan_simulate_cvd_machado_gamut_safe_{T}`,
  `alwan_ycbcr_to_rgb_gamut_safe_{T}` and `alwan_yccbccrc_to_rgb_gamut_safe_{T}` all gained
  a `_map_planar` twin in both precisions: three channel planes in, three out, one stride
  shared by the planes, the trailing arguments in the interleave order. That is the shape a
  video pipeline and a plane-per-channel image already hold, so it saves the caller an
  interleave pass on each side.

  The four gamut-safe conversions fetch the space descriptor once for the buffer rather
  than once per pixel, as their interleave forms do, so the two agree to the bit. Suite 174
  asserts exactly that: planar against interleave, per pixel, both precisions, over a plane
  stride that is deliberately not the interleave stride, so a twin that assumed packed
  planes fails. It also checks that a NULL plane, a NULL space and a NULL matrix are
  refused.

- **`alwan_optimize_spectrum_for_xyz_{T}` did not optimise anything.** It ignored the
  observer, ignored the target's X and Z, and gave every one of its seven Gaussians the
  weight `target_xyz->y / 7`, so two targets with the same luminance produced the same
  "match" and no target was met. It also rejected a NULL context it never used, which is
  why the determinism dump's five `optimize_spectrum` sections had never been written, and
  it forced an 81-sample 380-780 nm grid over whatever the caller had allocated, so a
  caller that pre-allocated a smaller SPD, as the header tells it to, was written past the
  end of its buffer.

  It now solves for the weights. XYZ is a linear functional of the SPD, so with A the 3x7
  matrix whose column is each basis function's XYZ under the requested observer on the
  caller's own grid, the minimum-norm solution of `A w = xyz` meets the target exactly:
  measured 3.6e-14 of Y = 50 in suite 42. A negative weight means the target is not a
  non-negative mixture of this basis, which is the case for every saturated colour, and
  that is `ALWAN_E_RANGE` with the values zeroed rather than an SPD that misses the target;
  the header points those callers at `alwan_xyz_to_spectrum_otsu2018`. The caller's grid is
  honoured, and `count = 0` still asks for the 81-sample default every caller got before.

- **`alwan_gamut_map_advanced_{T}` rejected one of its own methods, and only sometimes.**
  `ALWAN_GAMUT_MAP_HUE_PRESERVING` returned `ALWAN_E_INVALID`, but the in-gamut early-out
  returns before the method is examined, so the same call succeeded on a colour inside the
  cube and failed on one outside it. `alwan_simulate_cvd_gamut_safe_{T}` passes this enum
  straight through and its header offers the whole of it: with that method it failed on
  every one of the 2,426 random sRGB colours (of 4,000) whose CVD simulation leaves the
  gamut, which is to say on the pixels it exists for. The early-out was its only other way
  out and it fires for none of those, nor for any of the 24,526 out of 40,000. The method is now the projection the plain
  `alwan_gamut_{T}` maps run, applied in the working space so `space` is honoured, and it
  is bit-identical to those maps for sRGB. Suite 41 sweeps all eight methods over colours
  whose simulation is known to leave the gamut.

- **Buffer forms of the last per-pixel operations that had none.** IPT <-> IPTch, the
  Jzczhz HDR gamut map, `gamut_map_advanced`, `gamut_map_xyz_to_rgb`, and the Cheung 2004
  and Finlayson 2015 colour corrections, in `api/alwan_apply_maps.c`, bit-identical to their
  scalars (suite 174).

- **Buffer forms of every colour-difference metric.** Sixteen `alwan_delta_e_<name>_{T}_map_interleave`
  in `api/alwan_delta_e_maps.c`, one shape for all: two strided buffers of the metric's
  colour type in, a strided buffer of distances out, params or `textiles` after `count`
  where the scalar takes them. Each is its scalar in a loop; suite 173 holds every one to
  bit equality with it over a padded stride. The four `_batch` forms are the older spelling
  and stay.

- **Buffer forms of nine appearance models.** Hellwig 2022, Kim 2009, Hunt, LLAB, ATD95,
  Nayatani 95, RLAB, CAM18sl and CAM20u gain `_forward_{T}_map_interleave`, and the five
  with an inverse gain `_inverse_{T}_map_interleave`, in `api/alwan_cam_maps.c`. Each is
  the scalar in a loop, the arguments validated once; suite 172 holds every one to bit
  equality with its scalar over a padded stride, both precisions.

- **`alwan_aces1_output_transform_{T}_map_interleave`.** The ACES 1.x output transform
  over a buffer, both precisions. It has no per-call state to hoist, so this is the scalar
  in a loop with the preset validated once; suite 56 asserts bit equality with the scalar
  over every preset.

- **ZCAM and Zhai 2018 maps, with the white's terms hoisted.**
  `alwan_zcam_forward_{T}_map_interleave`, `alwan_zcam_inverse_{T}_map_interleave` and
  `alwan_cat_zhai2018_{T}_map_interleave`. The ZCAM core now has a params form: everything
  that depends on the white and the viewing conditions, the two-step CAT gains in both
  directions, I_z,w, and seven powers, is computed once by `alwan_zcam_params_v` and used by
  `alwan_zcam_forward_params_v` / `alwan_zcam_inverse_params_v`; the six-argument scalars
  call the pair. The Zhai CAT splits the same way into `alwan_cat_zhai2018_gains_v` and
  `alwan_cat_zhai2018_apply_v`. In every case the per-pixel arithmetic is the one the
  scalar ran before, in the same order, so a map is bit-identical to its scalar twin
  (suites 28 and 144 assert equality) and the determinism dump did not move by a byte.
  `ALWAN_CORE_FNLIT_MAP(base)` spells `base_f64_map_interleave` inside an `_impl.inc`,
  which `##` could not.

- **Table readers compile as shaders, and all 43 cores now do.** `alwan_table_core.h`
  and the two cores that include it, `alwan_lut_core.h` and `alwan_vision_core.h`, were
  the three that did not build as HLSL, for one reason: the readers took the table as a
  pointer. They are now in `core/alwan_table_reader.inc`, written against an accessor.
  A shader defines `ALWAN_TABLE_NAME` and `ALWAN_TABLE_READ(i)`, to a `StructuredBuffer`,
  a static array, a texture load or an offset into a shared buffer, includes the file,
  and gets `alwan_table3d_sample_tetrahedral_<name>(size, rgb)` and the rest.
  `ALWAN_TABLE_READ_MAT3` and `ALWAN_TABLE_READ_P0` / `_P1` / `_P2` enable the matrix
  ramp and the three-plane cube. C binds the same bodies to a pointer, so every existing
  reader keeps its name and signature and there is no GPU twin of a reader left to drift.

  The arithmetic is exposed on its own for a caller that fetches its own texels:
  `alwan_table_blend_v`, `_delta_v`, `_catmull_rom_v`, `_mat3_v`, `_trilinear_v`,
  `_tetrahedral_v`, `alwan_table_cell_nearest_v`, and the flat node address of each
  layout, `alwan_table3d_index_v`, `alwan_table2d_strip_index_v`,
  `alwan_table3d_planar_index_v`. New platform macros: `ALWAN_HAS_POINTERS`,
  `ALWAN_PARAM_INT_OUT`, `ALWAN_PARAM_ARRAY_IN`. `alwan_lut2d_dimensions_v`,
  `alwan_lut3d_to_2d_v` and `alwan_lut2d_to_3d_v` take their integer outputs through
  `ALWAN_PARAM_INT_OUT`, which is `int *` in C as before. The three
  `alwan_lut*_sample_v` pointer wrappers are compiled out on HLSL and GLSL.

  A table read is bit-exact between the GPU and C in an ordinary build. That was
  measured on D3D12 WARP under dxc and fxc, and it did not hold at first: dxc
  reassociated the Catmull-Rom and tetrahedral sums, and every blend through
  `alwan_lerp` became the `lerp` intrinsic, each one ULP off. The blends are `precise`
  now and none uses `alwan_lerp`. No C output changes: the determinism dump is
  byte-identical, 413,041 lines.

- **`srgbe_p3d65_display` parses.** The Color Interop Forum's "Display P3 HDR" ID now
  resolves to `ALWAN_RGB_SPACE_DISPLAY_P3`, where it was `ALWAN_E_NODATA`. It is the same
  primaries, white point and piecewise sRGB curve as `srgb_p3d65_display`; what differs
  is that values above 1.0 are allowed, so a UI's SDR white can sit at 1.0 while
  highlights extend past it. `alwan_rgb_space` describes primaries, a white point and a
  transfer function and carries no notion of the range a value may occupy, so both IDs
  land on one space. A second space identical in every field would convert identically
  and only make the enum longer.

  The Forum's other two unparsed IDs, `g26_xyzd65_display` and `pq_xyzd65_display`, stay
  `ALWAN_E_NODATA`. Neither needs a new transfer function, since `alwan_dcdm_oetf`
  already carries the 48 / 52.37 headroom they call for, but both need an XYZ-primaries
  space at D65 and that turned out to rest on an unsettled question about the space alwan
  already has. Suite 59 now measures it: `ALWAN_RGB_SPACE_LINEAR_CIE_XYZ_D65` stores the
  identity matrix and is documented as D65-relative, but declares illuminant E as its
  white, so D65's own tristimulus taken through it to linear Rec.709 comes out
  (0.801, 1.053, 1.099) when a ctx is passed and exactly (1, 1, 1) when it is not. The
  same conversion has two answers depending on the context argument, and the correct one
  is the one where no adaptation runs. Nothing is changed here; roadmap 3.6 carries the
  decision with both numbers attached.

- **A CGATS.17 writer beside the OQM one.** `alwan_chart_write_cgats17_{T}` and
  `alwan_chart_write_cgats17_buffer_{T}`, with the same shape as the existing writers:
  the buffer form reports the length it needs when given a NULL buffer and refuses a
  buffer one byte short rather than truncating.

  The two dialects differ in exactly two places, which is worth stating because a format
  change sounds bigger than this one is: the identifier on the first line, and how a
  reflectance column names its wavelength, `SPECTRAL_NM560` against `SPEC_560`. The
  header keys, `NUMBER_OF_FIELDS`, the `DATA_FORMAT` block, `NUMBER_OF_SETS` and the data
  are the same text in both, because CGATS.17's structure is what the OQM writer was
  already emitting. Suite 112 asserts that by transforming one output into the other and
  requiring the bytes to match, so a third difference appearing would fail the test
  whether or not anyone thought to look for it.

  Neither writer invents a header. A CGATS file conventionally carries ORIGINATOR and
  CREATED, and these do not add them: a key the source chart never had would come back
  from a reload as though it were the chart's own, and the round trip would stop being an
  identity. What the chart carries is what gets written.

  Both dialects read back through `alwan_chart_load_{T}`, which is the test of either.
  The reflectances survive a CGATS.17 round trip exactly, 0.0 over 36 bands, since the
  reader already accepted all three spellings the standard allows.

- **Mantiuk 2006: the first LOCAL tone mapper.** `alwan_tonemap_mantiuk2006_{T}`, with
  `alwan_tonemap_local_params_{T}`. Every tone mapper alwan had until now is a curve: a
  pixel's result depends on that pixel and on statistics of the whole image. This one is
  not. It takes the log luminance apart into contrasts at every scale, shrinks each
  through a response curve, and then solves for the image whose contrasts those are. A
  pixel's result therefore depends on its neighbours, which is what lets it hold local
  texture while losing global range, and it is why the call takes a width and a height
  rather than a count.

  Suite 170 shows the difference rather than asserting it. Two identical grey patches,
  one on a dark surround and one on a bright one, come out at 0.2088 and 0.2092: no
  operator in `alwan_tonemap_global_{T}` can do that, because none of them can see the
  surround. Changing a single pixel in one corner moves the opposite corner by 3.7e-05,
  since the reconstruction couples the whole image and there are no tiles in it.

  The reconstruction is a least-squares problem solved by conjugate gradients, and two
  things about that are worth stating. The operator it inverts has constants in its null
  space, so the answer is fixed only up to overall level; the iteration starts at the
  image's own log luminance and never leaves that level, which is what gives the output
  a meaningful anchor and why it is NOT normalised on the way out. And the solve stops
  on a relative residual of 1e-3 rather than an iteration count, reaching it in five to
  thirteen steps against a cap of 100. That is what makes the result reproducible at
  all: a solver that stopped on a count would make the answer a property of its own loop
  rather than of the equations, and no second implementation could agree with it.
  `iterations_out` reports the count so a caller can see a result that capped.

  Reproduces OpenCV's `TonemapMantiuk` to 1.1e-06 in float32 across five cases, two
  image sizes, three scales and two saturations, once OpenCV's framing is taken off as
  it was for Drago. Reproducing it meant reproducing two choices that are not the
  obvious ones: the pyramid halves with a BILINEAR RESIZE rather than a Gaussian
  pyrDown, so the Mertens pyramid already in alwan is the wrong tool and this operator
  carries its own; and the level count is `(int)(logf(min(w, h)) / logf(2))` computed in
  FLOAT, kept exactly rather than replaced with an integer log, because a float landing
  a hair under an integer would drop a level and change the picture.

  One case in the fixture is 35 x 27 on purpose. A power-of-two image gives the pyramid
  exact halves and the resize degenerates to averaging pairs; an odd one makes it
  resample, and it also takes 13 conjugate gradient steps where the even case takes 7.
  A single well-behaved size would have hidden both.

  `luminance_weights` follows the rest of alwan: zero is the sRGB primaries' Y row.
  OpenCV uses Rec.601 luma, 0.299 / 0.587 / 0.114, which is a different quantity and the
  wrong one for linear light, so the suite passes it explicitly to compare like with
  like. Published in Mantiuk, Myszkowski and Seidel, ACM TAP 3(3), 2006.

- **Drago 2003, adaptive logarithmic mapping.** `ALWAN_TONEMAP_DRAGO2003`, the twelfth
  operator of `alwan_tonemap_global_{T}`, with `drago_bias` added to
  `alwan_tonemap_params_{T}`. Luminance is divided by its own log average, and

      L_d = (L_dmax / 100) / log10(L_wmax + 1)
            * log(L_w + 1) / log(2 + 8 (L_w / L_wmax)^(log b / log 0.5))

  so the base of the logarithm slides with the luminance: 2 at black, 10 at the peak,
  and `drago_bias` sets how it moves between them. The paper's recommended range is 0.7
  to 0.9 and the default is 0.85. `L_dmax` is `display_peak`, in cd/m2, which Tumblin
  1999 already read. A `drago_bias` outside (0, 1] is `ALWAN_E_INVALID`; 1 is legal and
  makes the base 10 everywhere.

  Because the log average divides out, the operator is invariant to a uniform scale of
  its input: the same scene six stops brighter gives the same picture, to 5.4e-15. That
  is the published operator, not a normalisation applied on top, and it is worth saying
  because the obvious way to check it would not distinguish the two.

  Pinned against OpenCV's `TonemapDrago` to 3.2e-07 in float32, which is where a
  float32 fixture lands. OpenCV wraps the same formula in two steps of its own, an
  affine rescale of the input to [0, 1] and a min-max rescale of the output, and the
  second makes the paper's `1 / log10(L_wmax + 1)` term redundant so it drops it. Suite
  169 feeds both sides the already-rescaled image and applies the output rescale to
  alwan's result, so what is compared is the operator and what is removed is the
  framing. alwan ships the paper.

  One thing the suite measures that is worth knowing before reaching for this operator:
  the slide is GENTLE at the default bias. The exponent is log 0.85 / log 0.5 = 0.234,
  so a pixel 3.6 decades below the peak still sits at base 3.15, not 2; the base only
  reaches 2 in the limit. A smaller bias steepens it, and b = 0.5 makes the exponent
  exactly 1.

  Which leads to the one thing a caller really has to know, and it is a property of the
  published formula rather than of this implementation. THE OPERATOR IS NOT MONOTONE AT
  EVERY BIAS. The denominator grows like `L^(log b / log 0.5)` and the numerator like
  `log L`, so for a small enough bias on a wide enough scene the denominator wins and a
  brighter pixel comes out darker. Measured on a luminance ramp, the first inversion is
  at about

      bias 0.50   3 decades of scene range
      bias 0.70   5 decades
      bias 0.80   8 decades
      bias 0.85   none up to 8 decades

  so the default is safe on anything a camera produces, while 0.70, which is inside the
  paper's own recommended range of 0.7 to 0.9, inverts tones on a five-decade scene.
  alwan does not clamp it: the operator is what it is, and a caller choosing a low bias
  for a very high range scene should be told rather than quietly repaired. Suite 169
  pins nine points of that boundary, from bias 0.50 at two decades, still monotone, to
  bias 0.50 at three, where 62 of 399 steps go backwards.

  Only the gamma 1, saturation 1 path is compared, and nothing is lost by that: alwan
  has neither parameter on this operator, `ALWAN_TONEMAP_GAMMA` being its own. It is
  also the only path OpenCV can be asked for, since it raises an intermediate to a
  fractional power without guarding a negative one and so returns NaN on some images
  and not others.

- **A measured display, as a model, and the LUT that calibrates it.**
  `alwan_display_model_fit_{T}` turns a meter run into a model of what a display does with
  any signal. Give it one drive axis and three ramps of measured XYZ, one per channel, and
  it returns `alwan_display_model_{T}`:

      XYZ = M [ t_r(d_r), t_g(d_g), t_b(d_b) ]^T + XYZ_black

  `t_c` is that channel's tone curve normalised to run 0 to 1, `M`'s columns are the
  primaries at full drive with the black taken off, and the black is the reading at zero
  drive. `alwan_display_model_forward_{T}` applies it and `alwan_display_model_invert_{T}`
  runs it backwards. `alwan_display_calibration_lut_{T}` bakes the whole chain, source
  EOTF, source primaries, a chromatic adaptation onto the display's measured white, and
  the model inverse, into a 3D LUT that takes source signal to drive, in the R-fastest
  order `alwan_cube_export_3d_{T}` and `alwan_lut3d_sample_{T}` already use.

  Each channel is fitted as a GOGO on the RAW luminance ramp, not on a black-subtracted
  one, and the order is the whole problem. Subtracting the shared black first looks right
  and destroys the offset: a channel whose curve has one is still emitting at zero drive,
  that emission goes into the black along with the room, and the remainder is
  `(a d + b)^g - b^g`, which is not a power law and which no GOG can be. Fitted raw, a
  measured ramp IS a GOGO exactly, so the offset survives and the flare comes back as that
  channel's own reading of the black. The stored curve is then normalised, and stays a
  GOGO with a negative flare, so `alwan_display_gog_eval_{T}` evaluates it directly.

  Two consequences worth knowing before reading the numbers back. A column of `M` is the
  channel's SWING, full drive minus zero drive, not its absolute peak, because the light a
  channel emits at zero drive cannot be told apart from the room by any meter. And the
  black the model reports is the zero-drive READING, which is the ambient plus whatever the
  channels leak, for the same reason.

  `excursion_out` on the inverse, and `worst_excursion` on the bake, are how far outside
  the display's gamut a request fell, 0 when inside. The drive comes back clamped to what
  the display can send, and the excursion says what was asked for beyond it: a reported
  clamp rather than a silent one. On a display with a real black it is the number a
  calibrator reports as the black level, since a source space asking for absolute zero at
  signal zero is asking for something no panel does.

  THE MODEL ASSUMES CHANNEL INDEPENDENCE AND ADDITIVITY, and real displays deviate, LCDs
  most of all. Ramps cannot fit that deviation, so alwan does not pretend to: the fit
  reports its residual, and a caller with measurements off the ramps can push them through
  the forward model and see for themselves.

  NOT TESTABLE AGAINST A REFERENCE, and the header says so: colour-science has no display
  model and no calibration bake. Suite 168 closes the loop instead. It builds a display it
  chose, generates the measurements that display would give, and requires the fit to
  return it, to 2.8e-17 on the primaries, 5.8e-15 on the gammas and 3.6e-16 on the tone
  curves; six colours off the ramps, which the fit never saw, come back to 3.1e-16. Then
  Rec.709 baked onto a P3 display lands eight source colours to 6.7e-16 of XYZ.

  The case that needs no tolerance argument is the one worth having: calibrating a display
  that ALREADY IS the source space must give the IDENTITY LUT, and it does, to 7.3e-15.
  That check fails on a transposed matrix, a wrong adaptation, an off-by-one in the cube
  order, a mismatched transfer function or a tone curve fitted to the wrong thing, none of
  which have to be anticipated for it to catch them.

  One number in that suite is 5.6e-08 rather than 1e-15, and it is arithmetic rather than
  a defect. Inverting to DRIVE is ill-conditioned at black: a power law has zero slope at
  zero, so its inverse has infinite slope there, and recovering a channel sitting at black
  beside two bright ones costs about `eps^(1/gamma)`, which is 5e-08 for gamma 2.2. A
  10-bit display steps drive by 9.8e-04, five orders of magnitude coarser. The statement
  that is exact is the one in luminance, where the conditioning runs the other way, and
  the suite pins that at 6.7e-16.

- **A display's tone response, fitted from measured patches.**
  `alwan_display_gog_fit_{T}` takes pairs of normalised drive and normalised luminance off a
  meter and returns the curve that joins them, as `alwan_display_gog_{T}`:

      GOG    L = (gain d + offset)^gamma
      GOGO   L = (gain d + offset)^gamma + flare

  The flare is what the room, the screen surface and the meter add to black, and it is why a
  measured display almost never reads zero at zero. Pass `with_flare` non-zero to fit it.
  `alwan_display_gog_eval_{T}` applies the model and `alwan_display_gog_invert_{T}` inverts it
  in closed form, so a caller can go either way once the fit is in hand.

  The fit gets there in two stages, because the obvious way does not work. Gain and gamma trade
  off against each other, so the luminance residual has a long curved valley and a general
  optimiser started cold wanders along it. But the model straightens: for a fixed gamma and
  flare, `(L - c)^(1/gamma) = gain d + offset` is an ordinary line whose best gain and offset
  follow in closed form, so the search runs over one parameter, or two with the flare, with the
  other two solved exactly inside it. A short simplex on the true luminance residual then
  finishes. `rms_out`, which may be NULL, is that residual, the quantity the fit minimises
  rather than the straightened one it started from.

  Deterministic by construction: a fixed initial simplex, a fixed iteration count, nothing
  random and no convergence test that could stop somewhere else on another machine. The same
  patches give the same answer bit for bit.

  A GOG fit needs at least four patches and a GOGO five, since a fit to exactly as many points
  as it has parameters says nothing about the display; fewer is `ALWAN_E_RANGE`. A luminance
  below the flare has no drive and is `ALWAN_E_RANGE` too, because a display cannot go darker
  than its own black. A non-finite measurement is `ALWAN_E_INVALID` rather than something the
  fit works around.

  NOT TESTABLE AGAINST A REFERENCE, and the header says so: nothing in colour-science fits
  either model. Suite 167 uses the geometry instead, generating patches from a known model and
  requiring the fit to return its parameters, which is the stronger check since agreeing with a
  second implementation would not catch both being wrong the same way. Seven cases come back to
  about 1e-15, so the suite's bounds are 1e-11. With 1e-03 of noise on the patches the fitted
  curve stays within 3.1e-04 of the true one everywhere. And fitting a display that has flare
  without the flare term costs six orders of magnitude of residual, 1.17e-03 against 6.65e-17,
  which is the term earning its place as an assertion rather than a claim. Published in Berns
  (1996), "Methods for characterizing CRT displays", Displays 16(4).

  One case is there to keep the second stage honest. A negative offset is legal and drives the
  first patch below zero, where the model floors at the flare, so the straightened fit sees a
  point that is not on its line and the closed-form start comes out biased. The refinement
  works where a floored point costs nothing and pulls the offset back to -0.02 exactly. Without
  it the suite would still pass six cases of seven.

- **The Michaelis-Menten relation, made callable.** `alwan_michaelis_menten_rate_{T}` and
  `..._substrate_{T}`, plus the `_abebe2017_` pair that carries the extra `b_m` term. The
  saturating two-parameter curve was already inside the library twice, as
  `ALWAN_LIGHTNESS_ABEBE2017_MICHAELIS_MENTEN` and as the JP2499 tonescale, each reaching it
  through its own code. A caller fitting a receptor response or building a tonescale can now
  reach the relation itself.

  A denominator that vanishes is `ALWAN_E_DIVZERO`, which for the inverse means the rate the
  curve approaches and never reaches: a saturating curve has no answer for an input above its
  own maximum, and saying so beats returning an infinity. A non-finite argument is
  `ALWAN_E_INVALID`.

  Suite 166 pins all four forms against colour-science, exactly, and checks three things that
  need no reference because they are properties of the relation rather than of anyone's code:
  `b_m` of one reproduces Michaelis's form bit for bit, the rate at `S = K_m` is half of
  `V_max`, and the inverse undoes the forward direction over four decades. The suite also
  found a null dereference in the first draft of the implementation, where the guard was on
  the internal scratch pointer rather than on the caller's.

- **Machado 2009 as a continuous model, so CVD can be simulated for the actual display.**
  `alwan_cvd_matrix_machado2009_shift_{T}` derives the colour vision deficiency matrix from the
  Stockman and Sharpe cone fundamentals rather than interpolating between the eleven matrices
  the paper tabulates. Those eleven are right for eleven severities on the display the authors
  used, a 1997 CRT, and cannot answer for anything else.

  The display is not a detail. Full protanopia on that CRT and on an Apple Studio Display
  differ by 0.40 in a matrix coefficient. `alwan_display_primaries_spd_{T}` reaches both
  embedded sets, and a caller can pass measured spectra for their own panel. Passing NULL for
  all three primaries gives the CRT the published tables were made with, which reproduces
  colour-science to 1e-14 over twenty-four matrices spanning both displays and twelve shifts.

  The shifts are in nanometres rather than a severity in [0, 1]: the paper's protanomaly and
  deuteranomaly run 0 to 20, its tritanomaly tables use 5 to 59, and the authors say the shift
  paradigm is an approximation there rather than a model of tritanopia.

  Two things about the spectra that cost a cycle each and are now written down. The published
  primaries are at 5 nm and the reference resamples them with Sprague; doing it linearly moves
  the derived matrix by up to 1.8e-02, so gendata calls the reference and alwan embeds the 1 nm
  result rather than reimplementing Sprague to produce a constant. And extrapolating those
  spectra by holding the last measured value instead of by zero leaves a primary emitting
  0.0066 from 781 nm to 830 nm, a tail that alone moves the matrix by 7e-06. That was a bug in
  alwan's own generator, found by the disagreement it caused.

- **Pointer's gamut as a volume, and it is not a convex hull.**
  `alwan_pointer_gamut_max_chroma_{T}` gives the greatest chroma a real surface colour reaches
  at a CIELAB lightness and hue, from Pointer's (1980) published grid of sixteen lightnesses
  by thirty-six hues, and `alwan_is_within_pointer_gamut_lab_{T}` and `_xyz_{T}` answer
  membership from it. alwan already had the chromaticity outline; this is the volume under it.

  colour-science's `is_within_pointer_gamut` tests against a Delaunay mesh over the table,
  which is its convex hull. Pointer's gamut is not convex, so that admits colours the
  measurement says are not there. Push each of the 575 non-zero grid directions past its own
  tabulated maximum chroma and colour's hull still calls 329 of 575 inside at 2 per cent past,
  and 80 of 575 inside at 25 per cent past. alwan calls none of them inside, because reading
  the table for what it says answers them by construction. Suite 40 asserts both halves of
  that, so a drift towards a hull would fail rather than pass quietly.

  `alwan_pointer_gamut_white_{T}` is the white the table is referenced to and it is not
  `ALWAN_ILLUMINANT_C`. That rounds to (0.31006, 0.31616) where Pointer's data is against
  (0.31005673430392799, 0.31614570478920401), a difference that moves a Lab by about 0.01 and
  changes answers at the boundary. Using the rounded one failed the suite's own white check,
  which is how the eight-digit constant was caught.

  Also fixed while regenerating the table: `gendata/data/reference_data.py` had stopped
  running at all, because colour-science made `ColourChecker` a dataclass rather than a
  mapping and `len()` on one now raises. The Pointer boundary and ColorChecker CSVs it also
  writes regenerate byte-identically, which is the check that nothing else had drifted.

- **The Lab extent of an RGB space.** `alwan_rgb_space_limits_{T}` returns the smallest and
  largest L*, a* and b* a space can reach, taken over the eight corners of its cube and under
  its own white. L* is monotonic in Y and Y is linear in RGB, so its extremes are at corners by
  construction; a* and b* are differences of non-linear functions and theirs need not be, which
  makes this a bound over the corners rather than a proof about the whole cube. Measured against
  400,000 points drawn uniformly from the sRGB cube, nothing exceeded it.

  A disagreement with colour-science worth recording. Its `RGB_colourspace_limits` uses whatever
  matrix the colourspace object carries, and for sRGB, Adobe RGB and ProPhoto that is the
  rounded matrix the standard publishes rather than the one the primaries derive. alwan derives.
  The two differ by up to 3.9e-02 in a* and b*, and the giveaway is Adobe RGB, whose stored
  matrix puts white at L* 99.99961 instead of 100. Suite 164 holds alwan to the limits computed
  from the derived matrix, where the two agree to 1.1e-13, and asserts the gap to the stored one
  rather than ignoring it.

- **Ellipse fitting, and the normalisation that makes it usable.**
  `alwan_ellipse_fit_halir1998_{T}` fits an ellipse to scattered points by the direct method
  of Halir and Flusser (1998), so the answer cannot come back a hyperbola however the points
  are spread. `alwan_ellipse_canonical_{T}` and `alwan_ellipse_general_{T}` move between the
  six general coefficients and the centre, semi-axes and rotation.

  The six coefficients come back with unit 2-norm and `a > 0`. They have no natural scale,
  since any non-zero multiple is the same ellipse, so without a stated convention the answer
  would be reproducible only against whichever eigenvector solver produced it.

  **The points are centred and scaled before the fit and the conic is mapped back after**, and
  that is what makes the routine work on the data it exists for. A MacAdam 1942 ellipse is
  about 1e-3 across at a chromaticity near 0.19, so `x^2`, `xy` and `y^2` barely vary across
  the point set and the scatter matrix goes nearly rank deficient. Measured on all
  twenty-five published ellipses: alwan recovers them to 2e-13. colour-science, which does
  not normalise, is off by up to 2.2e-4 in the semi-axes, a fifth of the shape for one whose
  semi-minor axis is 5e-4, with rotations wrong by up to 24 degrees; on one of the twenty-five
  its eigenvalues come out complex and it returns twelve coefficients that its own
  canonical-form conversion cannot unpack.

  Suite 165 therefore uses two oracles. Every case is generated from a known ellipse, so the
  geometry is the oracle and would catch both libraries being wrong the same way.
  colour-science is a second opinion on the six well-conditioned cases, where the two agree to
  2.5e-10. On the twenty-five MacAdam ellipses the test instead requires alwan to be a
  thousand times nearer the published parameters than colour is.

  Two smaller things the suite settles. A circle has no rotation, and the canonical form
  returns 0 for it rather than the arc tangent of two quantities that are both rounding noise:
  points generated on a circle leave `b` at about 1e-16, enough to miss a test for zero, and
  that returned -9.43 degrees before the guard. And the test measures position and angle
  separately rather than taking a maximum over both, because a maximum would be comparing a
  chromaticity with a degree: the noisy fits land within 2.5e-05 in xy and 0.03 degrees, and
  one number would have reported 0.03 and read it as the larger error.

- **The optimal colour solid, and no convex hull.** `alwan_colour_solid_create_{T}` builds
  the Rosch-MacAdam solid for an observer and an illuminant, and
  `alwan_colour_solid_contains_{T}` says whether a tristimulus is a colour a surface could
  have at all. Pointer's gamut, which alwan already had, is a chromaticity boundary measured
  from real samples; this is the harder limit underneath it, and it is computed rather than
  tabulated.

  The implementation is short because the set has a name. A reflectance lies in [0, 1] at each
  wavelength, so the achievable tristimulus form `{ sum_i R_i w_i : 0 <= R_i <= 1 }`, which is
  a zonotope: the image of a cube under a linear map. A zonotope's support in a direction is
  `sum_i max(0, n . w_i)`, because the best reflectance for that direction is 1 wherever the
  dot product is positive, and its facet normals are the cross products of pairs of generators.
  So membership is exact with two dot products per facet, and needs no hull library, no
  Delaunay triangulation and none of the degeneracy handling either would bring. The vertices
  are the classical optimal colour stimuli, which fall out of the same fact and are
  Schrodinger's two-transition reflectances.

  `alwan_colour_solid_vertices_{T}`, `_white_{T}`, `_num_vertices`, `_num_facets` and `_bins`
  report what was built. `bins` is 3 to 256; the build is cubic in it.

  Suite 164 pins two different claims against colour-science: the vertices term by term, worst
  2.4e-15 over four observer and illuminant combinations, and membership over 480 probes.
  colour answers membership with a Delaunay mesh and alwan answers from the facets, so
  agreement between two different algorithms for one set is evidence the identity holds rather
  than a restatement of the code. The probes include vertices nudged a thousandth in and out
  along the direction they are extreme in, which is where a hull and a facet test would part
  company if they were going to. All 480 agree.

  One detail worth recording: the quadrature is rectangles, not the trapezoid rule
  `alwan_xyz_from_spd_{T}` applies. A pulse wave is a band that is wholly on or wholly off, so
  weighting the two end bands at half would contradict what the construction means. The
  generators come from `alwan_spectral_weights_observer_{T}` with the end columns doubled and
  the rows renormalised, which reproduces colour-science's own generator matrix to 1.7e-16.

- **CxF3 measurement files.** `alwan_chart_load_{T}` now reads CxF3 (ISO 17972-1:2015)
  beside CGATS.17, which is the format the spectrophotometer vendors write. There is no new
  entry point and no format argument: the loader looks at the first byte that is not
  whitespace, sends an XML buffer to the CxF reader and everything else to the CGATS one, and
  fills the same `alwan_chart_{T}`. `alwan_chart_xyz_{T}`, `alwan_chart_reflectance_{T}`,
  `alwan_chart_lab_{T}`, `alwan_chart_device_values_{T}`, `alwan_chart_header_{T}` and
  `alwan_cmyk_model_from_chart_{T}` all answer for a CxF unchanged, which is the reason to read
  a second format rather than to grow a parallel API. Bytes that open with `<` but are not a
  CxF fall through to the CGATS reader instead of being claimed, so the error describes what is
  actually wrong with them. The export count is unchanged.

  An `Object` becomes a patch; `ReflectanceSpectrum`, `ColorCIELab` and `ColorCIEXYZ` carry the
  colorimetry in the order the columns have in CGATS; `ColorCMYK` and `ColorRGB` the device
  values; the `ColorSpecification` the illuminant, the observer and the wavelength grid when
  the spectra do not state their own; `FileInformation` the header keys. Element names are
  matched on the local name, so `cc:`, `cxf:` and no prefix all read. Two spectral grids in one
  file is `ALWAN_E_INVALID` rather than a resample, because resampling would be a guess about
  which grid the caller meant.

  This one is NOT testable against a reference, and the header says so. colour-science has no
  CxF reader and neither does anything else alwan depends on, so unlike CLF there is no second
  implementation to hand the same file to; the element shapes were written from ISO 17972-1,
  the CxF3 core schema and X-Rite's developer documentation. What suite 163 does pin is the two
  halves that can be: the colorimetry is colour-science's answer for the same spectra, and every
  case that can be is written twice from one set of numbers, once as CGATS and once as CxF3,
  with the two required to load to the same chart. A reader that dropped a patch or attached a
  spectrum to the wrong object fails that with no CxF oracle in sight.

  Writing the fixture turned up a quadrature difference worth recording. alwan closes an odd
  last interval of Simpson's rule with a trapezoid and scipy corrects the whole integral, so on
  an even number of sample points the two land 9.9e-06 apart for reasons that have nothing to do
  with colour. The fixture uses 37 points, where both run plain Simpson, and the agreement is
  then 5.6e-16.

- **One XML scanner, not two.** The structural scanning the CLF reader had grown is now
  `api/alwan_xml_common.h`, shared with the CxF reader: tags, attributes, self-closing forms,
  comments, the declaration, a DOCTYPE, skipping an element whole, and namespace-prefix-blind
  name comparison. Numbers stay with each caller on purpose, because CLF parses with strtod
  under a saved `LC_NUMERIC` and the chart reader hand-parses so a comma-decimal locale cannot
  change what a file means, and putting a number reader in the shared header would force one of
  those choices on the other. No behaviour change; suite 162's twenty-five cases cover the port.

- **CLF import.** `alwan_clf_import` reads a Common LUT Format ProcessList and
  `alwan_clf_apply_{T}_map_interleave` evaluates it, so an ACES LMT or an OCIO transform
  comes into alwan rather than only out of it. Every ProcessNode type CLF defines is
  understood: Matrix (3x3 or 3x4), Range, Exponent in all four of its styles, LUT1D,
  LUT3D, ASC_CDL in all four of its styles, and Log in all eight of its styles.
  `alwan_clf_node_count` and `alwan_clf_node_type_at` say what was read, and
  `alwan_clf_destroy` frees it. The XML is scanned by a reader that understands exactly
  CLF's shape rather than a general parser, and the whole file is read into memory first,
  so the sizes a header declares and the data that follows cannot disagree.

  A file carrying anything else is REFUSED with `ALWAN_E_NODATA` rather than partly
  applied: a ProcessList missing one of its stages is not the transform, and a wrong
  answer is worse than none. That covers the nodes OCIO writes into a CTF but CLF does
  not define, such as `ExposureContrast`, and any style outside the seven nodes' lists.

  What each node means was measured against OpenColorIO reading the same file rather
  than transcribed, and suite 162 pins it over twenty-five cases: each node and style
  alone, and two chains, because a reader can get every node right and still apply them
  in the wrong order. Each case is held to 3e-05 of its own peak answer rather than a
  fixed distance, since OCIO evaluates in float32 and its `pow` is a fast approximation:
  `antiLog10(1.4)` is 25.12 and OCIO puts it 6.7e-05 away, while `log2` of the smallest
  normal float is -126 and matches to 1.5e-05. The Matrix, Range and LUT cases sit at
  1e-8.

  Two details of the Log node are worth recording because they are not in the text of
  the specification. OCIO floors a logarithm's input at the smallest normal float32, so
  `log10(0)` is -37.9298 rather than an infinity, and alwan matches that. The camera
  styles' linear segment below `linSideBreak` takes the curve's own slope at the break
  unless the file states `linearSlope`.

- **3D LUT inversion.** `alwan_lut3d_invert_{T}` builds the cube that undoes a cube:
  Newton's method per node on the 3x3 system, the Jacobian by central differences over
  half a forward cell, a fixed step count so a deterministic build takes one path, and
  every step clamped into the unit cube. The inverse is addressed over [0, 1] in the
  forward cube's output space.

  A cube is not invertible everywhere and the entry point does not pretend otherwise.
  Where the forward table flattens, one preimage is as good as another; where a node
  lies outside the forward table's image there is no preimage at all. Neither is
  reported as an error, because neither is one: `out_worst_residual` is the answer, and
  the header says how to read it.

  Suite 161 asserts the thing that identifies the cause rather than a bare tolerance.
  Refining the forward grid has to bring the inverse closer to the analytically baked
  inverse, and it does: 4.13e-02, 1.64e-02, 6.36e-03 at grids of 9, 17 and 33, while the
  node residual falls 7.9e-06, 1.0e-07, 4.8e-09. The residual column is the solver and it
  converges to nothing; the other is the grid. The identity inverts to the identity
  exactly, and a cube clipping above 0.5 reports a residual of exactly 0.50.

- **Five more LUT interchange formats.** `.spi1d`, `.spi3d` and `.spimtx` (Sony
  Pictures Imageworks), `.3dl` (Autodesk, both the Flame and the Lustre flavour) and
  `.csp` (Cinespace), read and written, beside the `.cube` pair that was already here. `alwan_spi1d_import_{T}`
  reports the file's component count and input domain rather than assuming either; the
  domain is reported and never applied, since alwan's 1-D samplers address [0, 1].
  `alwan_3dl_import_{T}` takes the output bit depth from the Lustre header where there
  is one and infers it from the largest value in the file otherwise, which is what every
  other reader does and what a table that never reaches its own maximum has to live
  with. A 3D LUT stays R-fastest in memory, as `.cube` stores it; both of these formats
  are B-fastest on disk and both directions transpose.

  Suite 161 holds each reader to OpenColorIO's own evaluation of the same file at the
  cube's grid nodes, where interpolation is the identity, so a difference is the parse.
  The fixture transform has channel crosstalk on purpose: a symmetric table reads the
  same whichever way round the axes go. The writers are read back through the reader
  that pins, rather than compared byte for byte against OCIO, because two writers can
  differ in spacing and digits and mean the same table; `.spi3d` and `.spi1d` round trip
  bit for bit and `.3dl` to within half a step of the depth asked for.

  `.csp` carries a prelut: a per-channel piecewise-linear remap applied before the cube
  is addressed, which is how a shaper for log material is stored, and each channel may
  have its own point count. It is never dropped silently. `alwan_csp_import_3d_{T}`
  takes buffers for it, and passing NULL for them against a file whose prelut is not
  the identity is `ALWAN_E_INVALID`, not a quiet loss; the ordinary file, whose prelut
  is the two-point identity, reads with NULL. `.spimtx` is a 3x3 matrix and three
  offsets, and the file holds those offsets in 16-bit code units, so 65535 on disk adds
  exactly 1.0. That was measured against OCIO rather than read off a specification, and
  the entry points take and return the offset in the data's own units.

  The Resolve `.cube` joins them: that variant may carry a `LUT_1D_SIZE` shaper and a
  `LUT_3D_SIZE` cube in one file, with the shaper's rows above the cube's and no marker
  of their own. `alwan_cube_import_3d_shaper_{T}` reads both, and a file with no shaper
  reads through it too, so one call takes any `.cube`. The plain readers no longer skip
  the keyword and read the shaper's rows as cube samples: `alwan_cube_import_3d_{T}`
  refuses a file with a `LUT_1D_SIZE` and `alwan_cube_import_1d_{T}` one with a
  `LUT_3D_SIZE`, both with `ALWAN_E_INVALID`, because neither table is the transform on
  its own. Before this they read on and failed with `ALWAN_E_RANGE` after writing the
  wrong values into the caller's buffer.

  One difference from OCIO 2.5, and the suite pins it. A `.3dl` mesh line at size 3
  holds three numbers and looks exactly like a data line, so OCIO cannot read back the
  size-3 file it writes, in either flavour. It is still decidable, because with L
  three-number lines only one of L and L - 1 can be a perfect cube, and alwan reads it.

- **Film.** Eighty-seven photographic stocks as their datasheets describe them, the
  whole of spectral_film_lut's exported catalogue, and the pipeline from a scene
  spectrum to the projected print. Negatives from Vision3 50D to 500T, the Verita, EXR,
  Vision 320T and 5247 to 5250 era, Portra 160 to 800 with its pushes, Ektar, Gold,
  Ultramax, Vericolor, Aerocolor, Agfa Vista and the Fuji C200, Pro, Superia, Natura
  and Eterna sheets; 5222 and Tri-X at their development times; prints from 2383, 2393
  and 3513DI to the Endura, Crystal Archive, Fujiflex and Duraflex papers, 2302 at its
  development times and Polymax at its grades; Ektachrome 100D, Provia, Velvia,
  Kodachrome 64, Aerochrome III, FP-100C and Instax as reversals; Ilfochrome and
  Ektachrome Radiance as reversal prints. Every one is its own switch under
  `ALWAN_TABLES_FILM`. `alwan_film_get_profile_{T}` hands out a stock's tables on
  380-780 nm at 10 nm: spectral sensitivity per layer, the characteristic curve on a
  uniform 1024-point log exposure grid, the dye densities, the base, the reference grey
  and the interlayer masking matrix. `alwan_film_expose`, `_calibrate`, `_develop`,
  `_transmittance`, `_printer_light`, `_print` and `_project` are the stages,
  `alwan_film_render` and its `_map_interleave` form the whole trip, and
  `alwan_film_status_density` is ISO 5-3 Status A, Status M and ACES printing density
  of a transmittance.

  `alwan_film_look` and `alwan_film_render_rgb_{T}_map_interleave` are the chain from
  scene-linear RGB footage as shot, so a caller never touches a spectrum: Jakob 2019
  upsampling in the footage's gamut with highlights above white kept, the grey card at
  0.18 as the calibration point, the negative balanced on it or left with its cast, the
  neutral printer light with offsets in stops, a push or pull in stops, and XYZ out
  relative to the print's clear base. The header says what the exposure lesson was:
  anchoring a frame's median at grey pushes a low-key scene by stops and makes any stock
  look overexposed; take the scale from the camera, not from the picture.

  The look also comes in two halves, `alwan_film_look_expose` and
  `alwan_film_look_finish`, which are `alwan_film_render_rgb` to the bit back to back,
  so a spatial operator can sit between them on the negative's linear exposures. The
  first such operator is **halation**, in the experimental tier: the light that comes
  back from the base, with a kernel derived from a Lambertian entry, the unpolarised
  Fresnel reflectance at the base-to-air interface and total internal reflection past
  the critical angle, so it has the Fresnel-suppressed disc, the sharp rim at
  `2d / sqrt(n^2 - 1)` and the `(2d / r)^4` tail, and the rim radius in pixels is the
  one size parameter. Per-layer strengths, red first. No reference implements it, so
  suite 159 pins the derivation's own statements and the header names what it was
  written from.

  The second operator is **grain**, also experimental, in two forms. `alwan_film_grain`
  is Newson, Faraj, Galerne and Delon's Boolean disc model on a picture, rendered by
  Monte Carlo with the process intensity that makes it mean-preserving and a per-cell
  seeded generator so the field is the same on every run. `alwan_film_grain_density`
  is that model's many-thin-layers limit on a negative's activations, which is Selwyn's
  law: Gaussian on density with a spread of `(rms / 1000) sqrt(D) sqrt(A_48 / pitch^2)`,
  so the stock's own RMS granularity and the pixel pitch on the film set the size and
  there is no radius to choose. One opaque disc field per layer, the obvious first
  attempt, is a hundred times grainier than any sheet. `finish` splits once more into
  `alwan_film_look_develop` and `alwan_film_look_print`, to the bit, so the grain can
  sit on the developed negative. Suite 160 pins the models' statements; the papers, and
  Selwyn and Dainty and Shaw for the limit, are in the header.

  The profiles are spectral_film_lut's (Jan Lohse, MIT), digitised from the sheets and
  profiled by its `FilmSpectral`; alwan ships the profiling's output, not the
  digitisation, with the licence beside the tables, and gendata refuses to write a new
  profile unless a shipped one regenerates bit for bit. Suite 158 holds every stage to
  that model over its own spectra, nine cases: a colour negative printed, a black and
  white one printed, a reversal projected, a still negative on the other cine print, a
  pushed still negative on a colour paper, Tri-X on a graded paper, a second reversal
  projected, a reversal on a reversal print, and Verita 5206 on 2383. The model is
  float32 throughout, so nothing is to the bit: alwan's f64 lands within 1.1e-06
  relative of it at every stage, and the suite prints the worst it saw. Three profiles
  carry the model's own overshoot and ship as they are: Verita 5206's adjusted base
  spectrum dips to -0.66 at a band, Vericolor III's dye density to -0.49, and 5247's
  colour-masking matrix reaches 55 in its third row, which amplifies that layer's noise
  by as much; `masking = 0` on `alwan_film_develop`, or a zeroed `color_masking` in the
  profile, prints 5247 without it. The interlayer diffusion is spatial and is not in
  this release.

- **Typed image buffers for the spectral entry points.** The four upsamplers, Smits
  1999, Mallett 2019, Otsu 2018 and Jakob 2019, and `alwan_spectral_to_tristimulus` gain
  a `_map_interleave_ex` form: `void` pointers and an `alwan_pixel_format` per side, so
  u8, u16, f16, f32 or f64 colours go in and spectra come back in whichever of those the
  renderer keeps, or the reverse, without a conversion pass over either buffer. They
  dispatch as the other `_ex` forms do: both sides f32 is the f32 bulk form, both f64
  the f64 one, exactly; a mixed pair tiles through f64 when either side is f64, f32
  otherwise, with the edges converted by the same loaders every `_ex` uses.

  A spectrum is up to 85 channels per pixel where the existing tiles carry three, so
  rather than grow the scratch, a tile holds fewer pixels the more bands there are, 72
  at 85 bands. Nothing moves to the heap or the context, and the count per tile cannot
  change a per-pixel result. The tristimulus form takes f64 weights and always sums in
  f64, storing to the requested format; the header says so, because an f32/f32 call gets
  the f64 sum narrowed and not the f32 bulk form's sum. Suite 157 holds every pair to
  the bulk form it is documented to reproduce, across five tile boundaries.

- **PCHIP interpolation.** `ALWAN_INTERP_PCHIP` joins `alwan_interp_method`: Fritsch and
  Carlson's monotone cubic, which reads the same four points `ALWAN_INTERP_CUBIC` does and
  chooses the node derivatives so the curve cannot overshoot between samples. Where the
  data turns, it flattens.

  That is the difference worth knowing. `ALWAN_INTERP_CUBIC` is Catmull-Rom, not a spline:
  smooth, and it rings past the samples. On a reflectance, a density curve or a transfer
  function an overshoot puts values outside a range the data never left. On the suite's
  step fixture PCHIP holds exactly [0, 1] where Catmull-Rom swings 7.4e-02 beyond it.

  Matches scipy's `PchipInterpolator`, which is what colour-science wraps, to 3.3e-16
  including the endpoints (suite 42).

  Note that `alwan_interpolate_{T}` clamps to the end values outside the range of `x_in`
  where scipy continues its polynomial, so the two agree inside the data and diverge
  outside it by design.

- **A robust loss on the colour correction fits.** `alwan_ccm_fit_params` grows
  `robust_scale`, `robust_k`, `robust_iterations` and `robust_tol`. With a scale above
  zero the fit minimises `sum_i w_i rho(|r_i| / scale) + ridge |X|_F^2` with Huber's
  `rho`, so a bad patch pulls on the fit in proportion to its error rather than its
  square. Zero is off and is the default; every existing fit is unchanged, bit for bit.

  The residual is the **norm over the three channels**, so a patch is an outlier as a
  whole. That is what a glared or scratched patch is, and it keeps a chart's colour from
  being pulled apart channel by channel.

  Solved by iteratively reweighted least squares, which is why it composes: each round is
  the same solve, so the weights, the ridge, the solver choice, `rcond` and the
  neutral-preserving constraint all keep working. A robust fit still reproduces its
  neutral exactly, to 4.4e-16. It converges in eleven to thirteen rounds.

  `ridge` keeps meaning the penalty on the objective above, and the library scales the
  inner solve by `2 scale^2` to make that true. Huber's quadratic region carries a
  `1/(2 scale^2)` that a squared residual does not; without the scaling, turning the
  robust loss on would quietly multiply a caller's regularisation by 200 at a scale of
  0.05, and the fit lands at a measurably worse point.

  Held to a general optimiser minimising the same objective: the objective agrees to
  1.5e-12 and the coefficients to 1.1e-07. The gap is not slack. The minimum is flat, so
  two different searches agree on its value far more closely than on where they stopped,
  and the tests assert the objective and keep the coefficients as a sanity rail (suite
  124).

- **PSNR and CPSNR.** `alwan_psnr_{T}` reports peak signal to noise ratio per channel and
  pooled, over `width` x `height` pixels of 1 to 4 channels, with `border` pixels dropped
  from every edge before anything is accumulated. Either output may be `NULL`.

  The crop is not a convenience. A demosaicked image is least reliable at its edges, every
  paper crops before comparing, and a figure taken over the whole frame is not comparable
  with the ones they publish.

  CPSNR is one MSE pooled across every channel, which is what the demosaicing literature
  means by it and what scikit-image returns for a multi-channel image. It is **not** the
  mean of the per-channel decibels: on the suite's own fixtures the two sit between 0.01
  and 0.16 dB apart, close enough to pass for rounding, so both sides are asserted.

  A channel that matches exactly reports `+inf`, as `alwan_pu21_psnr_{T}` does. In a
  deterministic build the logarithm routes through `alwan_det_log10`, so the figures are
  the same bits on every platform.

  Held to scikit-image's `peak_signal_noise_ratio` at 7.1e-15 dB per channel and 3.6e-15
  pooled, across channel counts, borders and data ranges (suite 150), which also compares
  the four demosaicing methods on it: bilinear 30.4 dB, Malvar 2004 33.6, Menon 2007
  33.9 unrefined and 34.2 with its refining step.

- **A neutral-preserving constraint on the colour correction fits.** `alwan_ccm_fit_params`
  grows `neutral_in` and `neutral_out`, three values each, set together or both `NULL`. The
  fit then reproduces that one pair **exactly**, to round-off rather than to a tolerance,
  and is the best fit to everything else subject to it. Give the neutral in the units of
  `M_T` and its target in those of `M_R`; each fit expands it with its own expansion, so
  the same pair serves Cheung and Finlayson and a caller never builds the expanded row.

  This is what a target-based IDT wants: a profile that leaves greys grey. An
  unconstrained fit lands near the neutral and not on it, and the error it leaves there
  reads differently from the same error on a saturated patch.

  It is not free, and the suite asserts as much: exactness at one point is paid for in
  residual everywhere else. The constraint also determines a direction instead of fitting
  it, so a fit that needed as many samples as terms now needs one fewer, and `rank_out`
  counts that direction as found so a healthy fit of *n* terms still reports *n*.

  It reaches `alwan_ccm_fit_*`, `alwan_ccm_loo_*` and `alwan_ccm_select_*` alike, in both
  precisions, because all of them pass the same params through one solve.

  Held to 1.3e-13 against an independent solve over six constrained fits, with the neutral
  itself exact to 4.4e-16 (suite 124).

- **Seventeen more standard illuminants.** `alwan_illuminant` goes from 38 values to 55:
  the CIE FL3.1 to FL3.15 fluorescents, and the two indoor daylights ID50 and ID65.

  colour-science spells the fluorescents FL3.1 to FL3.15; alwan spells its fluorescents
  F1 to F12, so these are `ALWAN_ILLUMINANT_F3_1` to `_F3_15`, a dot not being legal in an
  identifier. `F3_1` is not a variant spelling of `F3`, which is colour's FL3: they are
  different lamps.

  Each carries a full 471-sample SPD at 360-830nm and 1nm, on the same grid and by the same
  linear interpolation and CIE 15 constant hold as the 38 before it, and each has its own
  `ALWAN_TABLE_SPD_ILLUMINANT_*` switch. None of the seventeen has xy chromaticity, so
  `alwan_data_get_illuminant_xy_{T}` returns `ALWAN_E_INVALID` for them, as it already does
  for F1, F3 to F6, F8 to F10 and F12.

  Suite 104 pins all 55 against colour-science at eight probe wavelengths, two of which sit
  deliberately off the source's 5nm grid, and its structural check that no two illuminants
  are bit-identical now spans all 55. That check is what would catch a fluorescent variant
  silently duplicating another, which is the way fifteen illuminant tables once shipped as
  copies of D65.

- **The seven luminous efficiency functions as datasets.** `alwan_lef` names them: CIE
  1924 V(lambda), Judd 1951 and Judd-Vos 1978, which correct it below 460nm, CIE 1964
  for the 10 degree field, the CIE 2008 2 and 10 degree physiologically relevant
  functions on the Stockman and Sharpe cone fundamentals, and CIE 1951 V'(lambda).
  `alwan_vision_type` keeps naming the regime, and `alwan_luminous_efficiency` keeps
  reading the two canonical functions, so nothing the deterministic dump pins moves; the
  five competing photopic functions were reachable by no name before.

  `alwan_spd_lef_{T}` hands any of the seven out as an SPD on its own grid, 1nm for six
  of them and 10nm over 370-770nm for Judd 1951, as published. CIE 1924 and CIE 1951
  come from the statics `alwan_luminous_efficiency` reads; the other five are registry
  tables with a switch each. `alwan_spd_luminous_flux_lef_{T}` is the flux integral
  under any of them, the same trapezoid as `alwan_spd_luminous_flux`, and matches
  `colour.luminous_flux(sd, lef=...)` to 8e-16 in f64 over A, D65 and a sodium lamp
  (suite 156); for the two canonical functions the two calls give one number.

- **The seven ISO 7589 sensitometric sources, as tabulated, and a Planckian tail.**
  Photographic and Sensitometric Daylight, Studio Tungsten and its sensitometric variant,
  Photoflood and its variant, and the Sensitometric Printer, from colour-science's
  `SDS_ILLUMINANTS`. They are an `alwan_iso7589_source` rather than seven more
  `alwan_illuminant` values, because they do not fit the 360-830nm table the others
  share: tabulated at 10nm from 350 to 690nm (the Printer to 560), five of them still
  climbing where the tabulation stops. Holding the last value flat, as every
  `alwan_illuminant` does, would put numbers of alwan's own under a standard's name.

  So `alwan_spd_iso7589_native_{T}` returns the tabulation to the bit, and
  `alwan_spd_iso7589_{T}` resamples it onto the caller's grid with the caller's
  `alwan_extrapolate_mode`: the hold, zero, the end slope, or the slope clamped at zero.
  On 360-830nm at 1nm, CONSTANT and LINEAR match colour's own Constant and Linear
  extrapolators to 6e-16 (suite 155).

  `alwan_spd_extend_planckian_{T}` is the continuation the physics of a tungsten or
  photoflood source calls for. It fits a temperature to the last `fit_count` samples
  (0 for all) with the scale free, so only the shape of the window sets T, by golden
  section over 1000-25000K, then continues each end along the Planckian through the end
  sample. The fitted T matches scipy's to 7e-8 relative. Measured on studio tungsten
  against the Academy's tabulation of the same source, which runs to 780nm where colour's
  stops at 690: over 695-780nm the Planckian tail is within 1.1% of the measurement, the
  flat hold 16.5% low, the straight line 6.0% high, and over 380-690nm the two
  tabulations are one curve to 1.4e-16. The fitted temperature is 3068K.

- **Spectral OpenEXR layout, documented.** `alwan.h` now describes the Fichet, Pacanowski
  and Wilkie layout (JCGT 2021) beside `alwan_spectral_to_tristimulus_{T}_map_interleave`:
  the S0 and T layers, the decimal-comma channel names, the mandatory header attributes,
  how to build the interleaved buffer, and which weights reproduce the paper's preview.
  Two things a reader would otherwise trip on are spelled out: OpenEXR returns channels
  sorted by name, so `S0.1000nm` comes before `S0.380nm` and bands must be ordered by
  value; and the paper's default band filter is a gate, which differs from alwan's point
  sample integration at the end bands. alwan still reads and writes no files.

- **Cubic spline interpolation.** `alwan_interpolate_cubic_spline_{T}` is the C2 spline
  through every sample, with `ALWAN_SPLINE_NOT_A_KNOT` and `ALWAN_SPLINE_NATURAL`
  boundaries. Not-a-knot is what scipy's `CubicSpline` and colour-science's
  `CubicSplineInterpolator` mean by the name; the textbook natural spline misses both by
  5e-2 to 0.12 near the ends, so the boundary is an argument rather than a guess.

  It is its own entry point rather than an `alwan_interp_method` because its node slopes
  come from a solve over every sample, which needs scratch; it takes a context for that and
  uses its allocator. `ALWAN_INTERP_CUBIC` is unchanged, and its header comment now says it
  is Catmull-Rom and points here. Outside the samples it holds the end value, as
  `alwan_interpolate_{T}` does.

  Suite 154 holds it to scipy at 1.2e-14 relative on well-conditioned grids in f64 and 2e-5
  in f32, over both boundaries and the two- and three-sample cases scipy special-cases. On
  a grid built to be ill-conditioned the two differ by 6.4e-11, and against the same system
  solved at 50 digits alwan is the closer, 4.8e-11 to scipy's 1.1e-10.

- **Fifty-six light sources from colour-science.** `alwan_illuminant` goes from 55 values
  to 111, taking in `SDS_LIGHT_SOURCES` under colour's own names: eight from RIT's
  PointerData spreadsheet, then the traditional, LED and Philips sheets of NIST's CQS
  simulation 7.4, and a Kinoton 75P xenon projector lamp.

  Every identifier carries an `LS_` prefix, which keeps all 56 apart from the standards.
  That matters most for two of them. `LS_SA` and `LS_SC` are not lamps: they are CIE
  illuminants A and C as the RIT spreadsheet tabulates them, matching
  `ALWAN_ILLUMINANT_A` to 5e-4 and `ALWAN_ILLUMINANT_C` exactly over 380-780nm. Below 380nm
  they are the flat hold where A and C carry real data, so prefer A and C. Unprefixed,
  `ALWAN_ILLUMINANT_SA` would sit beside `ALWAN_ILLUMINANT_A` as an apparent peer.

  `LS_INCANDESCENT` and `LS_60_AW_SOFT_WHITE` are one measurement. colour ships them
  bit-identical, from two sheets of the same NIST spreadsheet. Both keep a value, so every
  entry stays reachable by the name colour gives it.

  Each carries a full 471-sample SPD at 360-830nm and 1nm, on the same grid and by the
  same rule as the 55 before it. The sources are measured at 380-780nm, so each table is a
  flat hold over the 20nm below and the 50nm above, which is the convention the FL3.x and
  ID entries use and less extrapolation than the CIE F series already carries.

  They share one group switch, `ALWAN_TABLE_SPD_ILLUMINANT_LS_*` defaulting to
  `ALWAN_TABLES_LIGHT_SOURCES` rather than `ALWAN_TABLES_SPD`. Fifty-six 471-sample tables
  in two precisions is more embedded data than every CIE illuminant put together, so a
  build that wants the standard series and not the lamp catalogue drops them with one
  define and still has D65.

  Suite 153 holds their white points and CCTs to colour-science, checks each SPD is
  non-negative and flat outside the measured range, checks `LS_SA` and `LS_SC` against A
  and C in both directions, and checks no two tables are bit-identical bar the known pair.

- **`alwan_illuminant_white_point_{T}` answers for every illuminant.** It took a shortcut
  for `ALWAN_OBSERVER_CIE_1931_2DEG`, reading a pinned xy chromaticity pair rather than
  integrating, and returned `ALWAN_E_INVALID` when the illuminant had no such pair. Only 29
  do, so 26 shipped illuminants, every FL3.x and both indoor daylights among them, failed
  under the most common observer while succeeding under every other one.

  It now falls through to the SPD integration it already used for other observers. The xy
  shortcut is kept where the data exists, because two divisions beat integrating 471
  samples. Suite 153 walks all 111 values in both precisions.

  `alwan_data_get_illuminant_xy_{T}` is unchanged and still answers `ALWAN_E_INVALID` for
  an illuminant with no pinned pair. That is the accessor's own contract: no chromaticity
  pair is pinned, not no such illuminant.

- **Otsu et al. 2018 spectral recovery.** `alwan_xyz_to_spectrum_otsu2018_{T}` recovers a
  reflectance from a tristimulus value: a decision tree over CIE xy picks one of eight
  clusters, and the answer is that cluster's mean plus a weighted sum of its three basis
  functions. 36 samples, 380-730nm at 10nm, clamped to [0, 1] as the reference does.

  It takes XYZ where `alwan_rgb_to_spectrum_smits1999` and `..._mallett2019` take RGB. The
  selector is defined over chromaticity, so there is no RGB space to assume and none is
  named.

  The cluster's basis-to-XYZ matrix is embedded already inverted, and the XYZ of its mean
  with it. colour-science rebuilds both by integrating the basis functions against the CMFs
  and the illuminant on every call, but neither depends on the stimulus, so they are
  constants rather than per-call work. Precomputing them in gendata leaves no spectral
  integration in the runtime at all, which is what keeps this method usable where a CMF
  table is not, the GPU backends included.

  Held to colour-science at 3.9e-16 over 160 colours chosen to reach all eight clusters;
  the reference generator refuses to write a file whose cases miss one, because the sparse
  branches are only entered well away from the neutral axis (suite 149).

- **The Helmholtz-Kohlrausch effect (Nayatani 1997).**
  `alwan_hke_object_nayatani1997_{T}` and `alwan_hke_luminous_nayatani1997_{T}` predict how
  much brighter a chromatic stimulus looks than an achromatic one of the same luminance.
  Both take CIE 1960 UCS chromaticities, the stimulus against the adapting field, plus the
  adapting luminance. They live in the vision layer beside APCA and the Barten family,
  because they predict a perceptual response rather than convert a colour.

  The two methods differ in exactly one coefficient, the weight on the hue term: -0.866 for
  VCC, which matches for equal brightness, and -0.134 for VAC, which matches for equal
  lightness. The object variant returns a multiplier that is **exactly** 1 when the stimulus
  sits on the adapting field, and the luminous one is `0.4462 (object + 0.3086)^3`.

  Both are native-range with no normalization macro, which `docs/ranges.md` rule 2 already
  covers and which that document now names explicitly: they are viewing-condition-dependent
  multipliers with no fixed bound, and normalising them would destroy the 1.0 that is the
  only value in them anyone recognises.

  Held to colour-science at 2.2e-16 over 960 rows sweeping the full hue period at four
  saturations and five adapting luminances, both variants on every row (suite 147,
  `docs/api/vision.md`).

- **sCAM (Li and Luo 2024).** `alwan_scam_forward_{T}` and `alwan_scam_inverse_{T}` add the
  last appearance model alwan was missing. Ten correlates, four of which nothing else here
  reports: vividness, blackness, whiteness and depth. Blackness is `100 - V` and `V` passes
  100 for a saturated stimulus, so it goes negative, reaching -33 in the reference. Nothing
  clamps it and the suite asserts that it happens.

  The model is assembled rather than reimplemented: the stimulus is adapted to a D65 white
  by Li 2025, divided through by the white's luminance, converted by sUCS, and the
  correlates follow. Both of those pieces were already in alwan and already held to
  colour-science on their own, so what is new is the assembly and the correlate arithmetic.
  It also carries its own hue quadrature, on its own table, which is not CIECAM02's.

  Held to colour-science over 30 conditions spanning three surrounds, four adapting
  luminances and a discounted illuminant: worst 2.0e-13 forward and 5.7e-14 inverse
  (suite 146, `docs/api/color-appearance.md`).

  One property of the model is worth knowing before relying on it. **The inverse is not the
  identity of the forward.** Li 2025's adaptation is reversed by swapping its two whites,
  and that swap is not its own inverse. Where the two whites are close the gap sits near
  1e-9; for a D65 white viewed under illuminant A it reaches 7.42 in XYZ. That is the
  model, not this implementation: colour-science does the same thing to the same digit.

- **CIECAM16 (CIE 248:2022).** `alwan_ciecam16_forward_{T}` and
  `alwan_ciecam16_inverse_{T}` add the CIE's revision of CAM16. It reports the same seven
  correlates and changes two things: the post-adaptation compression becomes linear below
  0.26 and linear above 150, tangent to the old curve at both joins, and the adaptation
  term divides by a fixed 100 rather than by the white's own Y.

  Worth knowing before choosing between the two models: inside that window, with a white on
  the Y = 100 scale, CIECAM16 and CAM16 produce identical numbers, bit for bit. The
  difference appears at the extremes and when the white is not Y = 100. Suite 145 asserts
  both halves of that, 19 rows apart and 16 exactly equal, because a reference that only
  sampled ordinary colours would be satisfied by a CAM16 clone.

  Held to colour-science over 35 conditions: 1.7e-13 in J, 2.9e-13 in C
  (`docs/api/color-appearance.md`).

- **Three chromatic adaptation models, and the von Kries cone space.**
  `alwan_cat_cie1994_{T}` (CIE 109-1994), `alwan_cat_vk20_{T}` (Fairchild 2020) and
  `alwan_cat_li2025_{T}` join Zhai 2018 as adaptation *models*, where the amount of
  adaptation depends on the viewing conditions and there is no single matrix to hand back.
  CIE 1994 takes the two adapting fields' chromaticities, their illuminances and the
  background luminance factor; vK20 adapts towards a weighted mixture of a previous, a
  current and a reference white; Li 2025 is a CAT16 von Kries step whose degree of
  adaptation follows the CIECAM form and which carries both whites' luminances through.
  `ALWAN_CAT_VON_KRIES` also becomes selectable: the matrix had been generated all along
  and nothing read it. Held to colour-science over 20 conditions each, worst 5.7e-14,
  2.2e-16 and 2.8e-14 (suite 144, `docs/api/chromatic-adaptation.md`).

  Fairchild 1990 is not among them, and the documentation says why. colour-science's
  implementation computes the degrees of adaptation from the stimulus rather than the
  illuminant and reuses that one `p` for both the forward and inverse gains, where it
  cancels along with the `c` scaling: its output is identical for adapting luminances of
  20, 200 and 2000, and identical with the illuminant discounted or not, which leaves a
  plain von Kries step. There is nothing there to implement against.

- **CRI sample by sample, not just Ra.** `alwan_cri_specification_{T}` returns the
  fourteen special indices of CIE 13.3-1995, R1 to R14, alongside Ra. Ra itself is bit for
  bit what `alwan_cri_ra` returned before, from the same pipeline; the eight samples it
  averages are computed once. R9, the saturated red, is the index an average cannot show,
  and it is reported unclamped: high pressure sodium reaches -261 on it while its Ra reads
  8.5. The new entry point also reports failure as a status, where `alwan_cri_ra` reports
  -1, which a genuinely poor source can also score. Held to colour-science over all 35
  illuminants both libraries carry: worst 0.43 in Ra and 1.70 in a special index (suite 32,
  `docs/api/cct-light-quality.md`).

- **The rest of TM-30-18, not just Rf.** `alwan_tm30_specification_{T}` fills an
  `alwan_tm30_{T}` with the gamut index Rg, the sixteen hue bins' local fidelity, chroma
  shift and hue shift, the bin each of the 99 samples fell in, and the two sets of (a', b')
  averages a colour vector graphic is drawn from. The pipeline is the one
  `alwan_tm30_rf_{T}` already ran, which had been throwing all of this away; Rf itself is
  unchanged. Held to colour-science across daylight, incandescent, a broad and a narrow
  fluorescent, a phosphor LED and a sodium lamp, so Rg is exercised from 54 to 101 (suite
  32, `docs/api/cct-light-quality.md`).

- **An exact CCT and Duv solve: the Planckian table and Ohno 2013.**
  `alwan_planckian_table_create_{T}` samples the Planck 1900 locus once, against an
  observer's own CMFs, and keeps it; `alwan_uv_to_cct_ohno2013_{T}` then reads that table
  to return CCT and signed Duv by Ohno's triangular solution near the locus and his
  parabolic one beyond |Duv| = 0.002. Unlike `alwan_cct_duv_optimize`, which searches a
  closed-form approximation of the locus and says so, this one is held to
  colour-science's own Ohno 2013 (suite 133, `docs/api/reference-data.md`).

- **HyCH, the Huang 2015 power function, and STRESS.** `alwan_delta_e_hych_{T}` adds the
  hybrid difference of Huang et al. 2015, CIEDE2000's terms taken city block in lightness
  and Euclidean across chroma and hue, with the textile weighting available.
  `alwan_power_function_huang2015_{T}` applies the power the same authors fitted to each
  of twelve formulas, and `alwan_index_stress_{T}` reports the STRESS index of García et
  al. 2007, how far a formula's numbers sit from the visual judgements they should track.
  All three match colour-science (suite 143, `docs/api/color-difference.md`).

- **Lab to CMYK through a printing characterisation.** `alwan_lab_to_cmyk_{T}` searches a
  characterisation for the colorants that print closest to a Lab value at the black the
  caller fixes, and reports the CIEDE2000 it could not close. Inside the gamut that
  difference is nothing a spectrophotometer would read; outside it, the CMYK is the
  nearest the press can reach and the difference says how far short it fell, rather than
  being clamped away. Held to scipy's own inversion of the same model (suite 142,
  `docs/api/reference-data.md`).

- **ITU-R BT.1729 sweeps and staircase.** `ALWAN_PATTERN_BT1729_SWEEP_H`,
  `ALWAN_PATTERN_BT1729_SWEEP_V` and `ALWAN_PATTERN_BT1729_STAIRCASE` render the zones the
  recommendation specifies numerically. The sweeps run 60 to 960 cycles across the picture
  and 32 to 540 down it, which is the recommendation's megahertz written once instead of
  per system, and they put all sixteen Nyquist markers of Tables 2 and 3 within a tenth of
  a point of where it prints them (suite 141, `docs/api/patterns.md`).

- **ITU-R BT.814-4 PLUGE.** `ALWAN_PATTERN_PLUGE_BT814` renders the black level signal of
  Annex 2 at any size: twenty narrow stripes above and below black, the higher level patch
  for the gain control, and the two coarse stripes, with `pluge_range` choosing the SDR
  patch (code 940) or the HDR one (399, for PQ and HLG alike). Every edge is the
  recommendation's own sample and line number as a fraction of the picture, and at 1920 x
  1080 and 3840 x 2160 every sample matches the tables and the EBU's PLUGE files (suite
  140, `docs/api/patterns.md`).

- **EBU Tech 3325 test patterns.** `ALWAN_PATTERN_EBU_1` to `ALWAN_PATTERN_EBU_12_GREY`
  render the monitor measurement patterns of EBU Tech 3325: EBU_1 and EBU_2, the 13
  measurement-point patches and four windows of EBU_3, the 20 grey steps of EBU_4, and
  the primaries and 15 EBU test colours of EBU_5, chosen by the standard's own numbers in
  `alwan_pattern_params`. At 1920 x 1080 every sample matches the 59 official BT.709 SDR
  files code for code (suite 139, `docs/api/patterns.md`).

- **Colour bar test patterns.** `alwan_pattern_render_{T}` and its planar twin render
  the four ITU-R BT.471-1 colour bar signals, the EBU bars among them, and the ARIB
  STD-B28 multiformat colour bar SMPTE RP 219 is based on, at any size, as the native
  R'G'B' signal with below-black levels kept. Every ARIB level matches the standard's
  mV tables within their rounding and its 10-bit codes exactly (suite 138,
  `docs/api/patterns.md`).

- **CMYK printing characterisations, FOGRA39 embedded.** `alwan_cmyk_model_*` turns
  CMYK into CIELAB through an ISO 12642-2 (IT8.7/4) data set: Lab interpolated
  multilinearly inside the target's CMY cubes on six K planes, then linearly in K,
  which matches scipy's interpolation to 1e-10 and puts the 321 FOGRA39 patches it does
  not use at dE2000 mean 0.13, maximum 1.21. FOGRA39 (Fogra's FOGRA39L data,
  ISO 12647-2:2004/Amd 1) is embedded byte for byte, unmodified and with Fogra named
  as the source, as Fogra's terms require, and parsed at run time by alwan's own CGATS
  reader; any other IT8.7/4 file loads through `alwan_chart_*`. The chart reader and
  writer now keep CMYK and RGB device columns (`alwan_chart_device_values_{T}`) and the
  file's own Lab (`alwan_chart_lab_{T}`). `alwan_palette_nearest_{T}` finds the
  palette colour nearest a Lab value by CIEDE2000, Freetone through a CMYK
  characterisation (suite 137).

- **Named palettes and HEX.** `alwan_palette_*` looks colours up by name and searches
  names by substring in two palettes: Stuart Semple's Freetone, 1,310 colours in device
  CMYK from the Freetone bundle, which states no licence; and the 147 CSS Color 3
  keywords. `alwan_rgb_to_hex_{T}`, `alwan_hex_to_rgb_{T}` and
  `alwan_css_color_3_keyword_to_rgb_{T}` follow colour-science, except that three HEX
  digits read as CSS reads them (suite 136, `docs/api/palettes.md`).

- **Yrg, IPT Ragoo 2021, sUCS, Izazbz and Hunter Rdab.** The colour models
  colour-science has and alwan lacked, each with its inverse: `alwan_xyz_to_yrg_{T}`
  (Kirk 2019); `alwan_xyz_to_ipt_ragoo2021_{T}`, whose four matrices Ragoo refitted
  along with the exponent; `alwan_xyz_to_sucs_{T}` (Li and Luo 2024);
  `alwan_xyz_to_izazbz_{T}` for Safdar 2017 and ZCAM's Safdar 2021; and
  `alwan_xyz_to_hunter_rdab_{T}` with an optional white and K_ab. Against colour-science
  to 1.1e-13 in double precision (suite 135).

- **Colorimetric purity.** `alwan_colorimetric_purity_{T}`, excitation purity times
  y_wl / y, as colour-science's `colorimetric_purity` (suite 134).

- **Luminous flux, mired and the Planckian and daylight loci.**
  `alwan_spd_luminous_flux_{T}`, `_efficiency_{T}` and `_efficacy_{T}`, photopic and
  scotopic, as colour-science integrates them, to 2e-15; `alwan_cct_to_mired_{T}` and
  back; Krystek 1985's locus, `alwan_cct_to_uv_krystek1985_{T}`; Planck's law against
  an observer's CMFs, `alwan_cct_to_uv_planck1900_{T}`, to 7e-16; and exact inverses
  of both loci, `alwan_uv_to_cct_krystek1985_{T}` and `alwan_xy_to_cct_cie_d_{T}`,
  where colour-science runs a search that stops within 5e-5 K of them (suite 133).

- **Lightness, Munsell value, whiteness and yellowness methods.** The rest of
  colour-science's registries: `alwan_lightness_{T}` and its inverse for CIE 1976,
  Glasser 1958, Wyszecki 1963, Fairchild 2010 and 2011 and Abebe 2017;
  `alwan_munsell_value_{T}` for seven value functions, ASTM D1535 by exact inversion
  of its quintic where colour-science interpolates; the D1535 and Newhall 1943
  quintics back to luminance; whiteness Berger 1959, Taube 1960, Stensby 1968 and
  Ganz 1979, yellowness ASTM D1925 and the ASTM E313 alternative. Every one matches
  colour-science exactly in double precision, the inverses to 5e-16 (suite 132).

- **Five RGB spaces.** `ALWAN_RGB_SPACE_LINEAR_CIE_XYZ_D65`, `ALWAN_RGB_SPACE_SRGB_AP1`
  and `ALWAN_RGB_SPACE_GAMMA24_REC709`, the Color Interop Forum texture spaces alwan
  lacked (`REC1886_REC709` encodes with BT.709's camera curve, not a 2.4 power), and
  the camera gamuts `ALWAN_RGB_SPACE_FILMLIGHT_E_GAMUT_2` and
  `ALWAN_RGB_SPACE_F_GAMUT_C`. Their curves are checked against colour-science in
  suite 43, now 82 spaces.

- **The rest of colour-science's log curves.** `ALWAN_TF_LOG3G12`, `ALWAN_TF_PANALOG`,
  `ALWAN_TF_VIPERLOG`, `ALWAN_TF_PLOG`, `ALWAN_TF_FILMIC_PRO6`, `ALWAN_TF_MILOG` and
  `ALWAN_TF_LOG2`, pinned against colour-science in suite 103, now 36 curves.
  FiLMiC Pro 6's EOTF is exact, by Newton's method, where colour-science interpolates.
  `ALWAN_TF_COUNT` counts the curves.

- **ITU-T H.273 code points.** `alwan_h273_*` maps the three numbers a video stream
  signals its colour with, colour_primaries, transfer_characteristics and
  matrix_coefficients, to alwan's RGB spaces, transfer functions and Kr, Kb, and
  back. Against colour-science's H.273 tables: the chromaticities exactly, Kr and Kb
  to 2e-16, every transfer code through its curve to 6e-17, except transfer 11 (see
  `docs/alwan_decisions.md`).

- **Six H.273 transfer functions.** `ALWAN_TF_H273_LOG`, `ALWAN_TF_H273_LOG_SQRT`,
  `ALWAN_TF_XVYCC`, `ALWAN_TF_BT1361`, `ALWAN_TF_SYCC` and `ALWAN_TF_BT2020_12BIT`,
  appended to the enum and pinned against colour-science in suite 103, which now
  covers 29 curves.

- **Global tone mapping operators.** `alwan_tonemap_global_{T}` maps an image by one
  of colour-hdri's eleven global operators: simple, normalisation, gamma,
  logarithmic, exponential, Schlick's two mappings, Schlick 1994, Tumblin 1999,
  Reinhard and Devlin 2005, and Hable's filmic curve. One parameter struct serves
  them all, and its zero value is every published default with luminance by the
  sRGB primaries. They agree with colour-hdri to 3e-15. Reinhard 2004 follows the
  paper where colour-hdri does not, for chromatic adaptation and the automatic
  contrast; see `docs/alwan_decisions.md`.

- **ASTM E308 and E2022.** `alwan_xyz_from_spd_astm_e308_{T}` computes XYZ the way
  industrial colorimetry reports it, scaled so a perfect reflector has Y = 100. It
  takes data at 1, 5, 10 or 20 nm. At 10 nm, and at 5 or 20 nm on request, it uses
  E2022 weighting tables built from the 1 nm observer and illuminant, with the E308
  adjustment for a shorter spectrum. By default, 20 nm data is interpolated to 10 nm
  first. `alwan_astm_e2022_weights_{T}` returns the tables. Against colour-science's
  `sd_to_XYZ_ASTME308`, XYZ agrees to 7e-14 and the tables to 9e-16.

- **CCM fit: leave-one-out and the term count.** `alwan_ccm_loo_cheung2004_{T}` and
  `alwan_ccm_loo_finlayson2015_{T}` score a fit by its held-out error: each patch
  predicted by a fit to the others, with the caller's weights, ridge and solver.
  `alwan_ccm_select_cheung2004_{T}` picks the Cheung term count with the lowest.
  Matches scikit-learn's `LeaveOneOut` to 2e-13.

- **SSIM and PU-SSIM.** `alwan_ssim_{T}` is Wang, Bovik, Sheikh and Simoncelli 2004
  with the paper's settings, and matches scikit-image's `structural_similarity` to
  2e-14. `alwan_pu21_ssim_{T}` is `pu21_metric.m`'s PU-SSIM: luminance with its
  weights, limited to the PU21 range, PU21 encoded, then SSIM over 256. At the image
  borders it follows scikit-image, where MATLAB's `ssim` differs.

- **PU21 and PU-PSNR.** `alwan_pu21_encode_{T}` and `alwan_pu21_decode_{T}`, scalar
  and interleaved, map absolute HDR luminance to Mantiuk and Azimi's 2021
  perceptually uniform scale in all four published variants, so PSNR on HDR says
  something. `alwan_pu21_psnr_{T}` is `pu21_metric.m`'s PU-PSNR. The core builds on
  the GPU backends. Against the authors' `pu21_encoder.m`, evaluated at 50 digits, the
  encode agrees to a few 1e-13 and PU-PSNR to 1e-9 dB.

- **CCM fit: an SVD solver and the rank.** `alwan_ccm_fit_params` gains `solver`,
  `rcond` and `rank_out`. `ALWAN_CCM_SOLVER_SVD`, a one-sided Jacobi SVD, answers the
  rank-deficient fits QR refuses with the minimum-norm solution, and fits with fewer
  samples than terms. `rank_out` says how many terms the data supports, and QR
  reports -1 when it finds the system deficient instead of a bare `ALWAN_E_DIVZERO`.
  Rank and coefficients match `numpy.linalg.lstsq`.

- **CCM fits with weights and a ridge.** `alwan_ccm_fit_cheung2004_{T}` and
  `alwan_ccm_fit_finlayson2015_{T}` take `alwan_ccm_fit_params`: a weight per
  sample, 0 dropping one, and Tikhonov regularisation on every coefficient. Both
  are rows of the same QR, and a ridge lets a fit have more terms than samples.
  The result matches scikit-learn's Ridge with sample weights on colour-science's
  expansions to 1.3e-13. The existing fits are these with no params, bit for bit.

- **Robertson response recovery.** `alwan_crf_robertson2003_{T}` recovers a camera's
  response from every pixel of a bracket by alternating merge and re-estimation,
  Robertson, Borman and Stevenson 2003, and matches OpenCV's CalibrateRobertson to
  5e-6. Values no pixel holds are filled from their neighbours where OpenCV leaves
  NaN. The response goes to `alwan_hdr_merge` like Debevec's. It needs closely
  spaced exposures: 1/2 stop apart it merges to 0.009 stops, 3 stops apart it
  leaves a half-stop sawtooth that Debevec's smoothness term avoids.

- **Exposure fusion.** `alwan_exposure_fusion_mertens2007_{T}` fuses a bracket of
  display-encoded pictures into one, Mertens, Kautz and Van Reeth 2007: contrast,
  saturation and well-exposedness weights, blended through Laplacian pyramids.
  Native in both precisions. With OpenCV's borders and weighting, it matches
  MergeMertens to 4e-6, except in regions flat in every exposure, where OpenCV's
  float32 weights meet its 1e-12 floor and its rounding decides the blend.

- **Camera sensitivities from a chart.** `alwan_camera_sensitivities_from_chart_{T}`
  recovers the spectral sensitivities of a camera nobody has measured from a
  photographed chart, Jiang, Liu, Gu and Suesstrunk 2013. The basis is alwan's own,
  colour-science's `PCA_Jiang2013` over the 52 rawtoaces-data cameras, shipped as a
  table (`ALWAN_TABLE_CAMERA_BASIS`). colour-science's Dyer 2017 basis is not in the
  pinned rawtoaces-data, so its licence could not be checked. Given the same basis
  the result matches `RGB_to_msds_camera_sensitivities_Jiang2013` to 2e-14, and the
  curves go straight to `alwan_idt_matrix`.

- **Bayer demosaicing.** `alwan_cfa_bayer_demosaic_{T}` with bilinear, Malvar,
  He and Cutler 2004, and Menon, Andriani and Calvagno 2007 (DDFAPD) with or
  without its refining step, for the four Bayer layouts, and
  `alwan_cfa_bayer_mosaic_{T}`. Borders and summation order follow
  colour-demosaicing, and the results match it bit for bit. With the DNG model and
  the spectral IDT this completes a path from a Bayer buffer to ACES2065-1.

- **Camera response recovery.** `alwan_crf_debevec1997_{T}` recovers a camera's
  response from an exposure bracket, Debevec and Malik 1997 over Grossberg and
  Nayar 2003 samples (`alwan_crf_samples_grossberg2003_{T}`), with the polynomial
  extrapolation and normalisation colour-hdri applies. The result is the response
  `alwan_hdr_merge` takes. Each sample's log exposure is eliminated before the
  least-squares solve, so it runs on 250 unknowns instead of 1250.

- **Exposure and bracket merging.** The ISO 2720 meter equations
  (`alwan_average_luminance_{T}`, `alwan_average_illuminance_{T}`, EV from either,
  EV100), ISO 12232 focal plane exposure, saturation-based speed and exposure
  index, and Lagarde's 2014 photometric scale, with EXIF numbers as plain inputs.
  `alwan_hdr_merge_{T}_map_interleave` merges an exposure bracket into radiance
  with colour-hdri's four weighting functions and an optional response curve.
  Against colour-hdri: 1e-13 on the exposure model, 1e-12 on the merge, and an
  unclipped pixel merges back to the scene value.

- **The DNG colour model.** `alwan_dng_profile_{T}` holds a DNG camera profile's
  colour tags, and `alwan_dng_xyz_to_camera_matrix_{T}`,
  `alwan_dng_xy_to_camera_neutral_{T}`, `alwan_dng_camera_neutral_to_xy_{T}` and
  `alwan_dng_camera_to_xyz_matrix_{T}` compute what a raw converter does with them:
  ColorMatrix, CameraCalibration and ForwardMatrix interpolated in inverse CCT
  between the calibration illuminants, AsShotNeutral to a white and back, camera
  space to XYZ D50. Against colour-hdri on five profiles and six whites: 2.2e-15.
  `alwan_highlights_recovery_blend_{T}_map_interleave` is dcraw's highlight blend,
  to 2.2e-16.

- **Spectral camera characterisation.** 52 measured cameras from the Academy's
  rawtoaces-data (Apache-2.0, pinned to commit e9b8503, licence and source kept
  beside the tables), looked up by make and model through rawtoaces' own aliases:
  `alwan_camera_find`, `alwan_camera_info`, `alwan_camera_sensitivities_{T}`. The
  190-patch IDT training set and ISO 7589 studio tungsten come with them.

  `alwan_idt_matrix_{T}` is ACES P-2013-001 Method A as rawtoaces v1 solves it:
  white balance multipliers and a white-preserving 3x3 to ACES2065-1, fitted over
  the training reflectances on the CIE Lab objective or, optionally, Jzazbz.
  `alwan_camera_rgb_to_aces2065_1_{T}_map_interleave` applies the pair. Against
  `colour.matrix_idt` on the same bytes the matrices agree to 5.3e-9 (Lab) and
  2.1e-8 (Jzazbz). That is inside the precision the reference itself reaches:
  scipy's BFGS and a tight Nelder-Mead disagree by 3e-9 and 4e-8 on these inputs.

  `alwan_spd_to_aces2065_1_{T}` takes a spectrum to relative exposure values
  through the Reference Input Capture Device, with the ACES flare and CAT02 to the
  ACES white. It matches `colour.sd_to_aces_relative_exposure_values` to 1.6e-15
  without adaptation and 3.3e-7 with it; the difference is the illuminant white,
  which alwan takes over 360-830 nm (docs/alwan_decisions.md).

- **Spectral foundation.** CIE daylight at any chromaticity
  (`alwan_spd_cie_daylight_{T}`), Gaussian sources and Ohno's 2005 LED model,
  single or summed, all matching colour-science to 2e-15. `alwan_spd_observer_{T}`
  hands out an observer's functions as SPDs. For multispectral images,
  `alwan_spectral_weights_{T}` folds the illuminant, three responses and the
  integration rule into one 3 x N matrix, computed once, and
  `alwan_spectral_to_tristimulus_{T}_map_interleave` applies it per pixel; the
  result equals `alwan_xyz_from_spd` to 7.5e-16. `alwan_camera_rgb_from_spd_{T}` is
  `alwan_xyz_from_spd_camera` under a name that says it returns RGB.

- **BT.2408 conversions.** HLG to PQ and back through display light on the
  reference display, and SDR placed at the 203 cd/m2 HDR reference white:
  `alwan_bt2408_{hlg_to_pq,pq_to_hlg,sdr_to_pq,sdr_to_hlg}_{T}_map_interleave`. The
  HLG system gamma follows the display peak (BT.2100-2), and PQ above that peak is
  clipped only when asked. Against colour-science compositions of the BT.2100
  functions: 6.6e-15.

- **ISO 21496-1 gain maps.** `alwan_gain_map_params_{T}` holds the standard's
  metadata in its own log2 units. `alwan_gain_map_weight_{T}` gives the fraction a
  display of a given headroom applies, `alwan_gain_map_measure_{T}` fits the gain
  range to a pair of renditions, and the encode and apply maps go between them, for
  three-channel and single-channel maps. The core functions are GPU-portable, so a
  shader can apply a map. A round trip returns the alternate to 4e-16, and the f32
  path agrees with libultrahdr's arithmetic to 1.6e-7.

- **`alwan_get_build_info`.** The switches that change results are fixed when the
  library is built: `ALWAN_NORMALIZE_RANGES`, `ALWAN_DETERMINISTIC`, the
  precisions and the table configuration. The linked binary now reports them, so
  an application can check it got the library it was written against.

- **Tables can be compiled out.** `ALWAN_DATA_TABLES_MINIMAL=1` drops every
  switchable table; group and per-table switches in
  `data/alwan_data_tables_config.h` choose more finely. The public surface does not
  change: a reader of a missing table returns `ALWAN_E_NODATA` (a count returns 0,
  a metric its documented error value), never a link failure. The minimal static
  library is 7.4 MB against 71.5 MB (MSVC x64 Release).

- **`alwan_chart_*`: read a target's own measurement file.** OpenQualia's
  Measurement File Standard, which is CGATS.17-2009 with a fixed set of header
  keys, plus the plain CGATS batch reference files that share its structure.
  `XYZ_*` columns are used as written, then `LAB_*` under the file's
  illuminant, then reflectance columns (`SPEC_560`, `SPECTRAL_NM560`, `nm560`)
  integrated against its `ILLUMINANT` and `OBSERVER` and normalised so a
  perfect diffuser reads Y = 1. Loads from a path or from bytes you already
  hold, and writes back out.

  This is the other half of a distinction the library already made. The
  `alwan_color_checker_*` functions answer for a target as a product, from
  embedded values; these answer for it as an object. A ColorChecker has
  published values because every one is meant to be the same chart, and a
  professional target does not: an IT8 or a DT NGT2 is measured per sheet or
  per batch, two off the same press differ, and a chart fades. ISO 12641 fixes
  an IT8's layout and leaves its colorimetry to the manufacturer, which is why
  `ALWAN_IT8_7_2` carries 288 patch names and no numbers. They were always
  meant to come from the file that ships with the target.

  No network access, and none needed: the measurement is a file the user
  downloads once. `SERIAL` is a header key like any other, so an application
  reads the QR code on the target, scans a directory, compares
  `alwan_chart_header_{T}(chart, "SERIAL")`, and on a miss points the user at
  the vendor's measurement page. Thanks to Doug Peterson for pointing out that
  the network API this appeared to need was not needed at all.

- **`alwan_hunt_inverse_f32` / `_f64`: Hunt appearance correlates back to
  XYZ.** Reads `J`, `C` and `h`; `Q`, `s` and `M` are functions of those under
  the given viewing conditions, so they are recomputed rather than trusted.
  Pass the same conditions the forward was given.

  It is closed form. The roadmap had this deferred to 3.0.0 as a
  three-dimensional iterative solve, because the chromatic adaptation appeared
  to depend on the adapted signal. It does not: its four per-channel terms read
  the white, the background, the proximal field and the adapting luminance
  only, so the adaptation undoes exactly. What remains is algebra. The hue
  fixes the direction of the opponent pair; the three colour-difference signals
  sum to zero so they fix only the differences, leaving one achromatic level;
  saturation ties the two together linearly; and brightness then gives the
  level from one linear equation. The single iteration left is a scalar fixed
  point on the scotopic response, and it runs only when `vc.S` is left at 0,
  since that is the field whose default is the stimulus `Y`.

  Round-trips the sRGB gamut to `6.5e-11` in f64 across ten viewing-condition
  variants, covering the Helson-Judd, proximal-field, discounting,
  coloured-background and explicit-scotopic paths. Two limits are inherited
  from clamps in the forward rather than introduced here: a stimulus with a
  negative cone response is not recoverable, and a negative saturation is
  reported as `C = 0`. Neither is reachable from inside a real display gamut.

- **OpenCL backend (`ALWAN_BACKEND` 4).** Bootstrap with `alwan_opencl.h`, then
  the core headers, and call the core from your own `__kernel`. Single
  precision by default; `ALWAN_OPENCL_FP64=1` switches to `double` on a device
  that reports `cl_khr_fp64`. A backend id, unlike CUDA, because OpenCL C is not
  C: program scope needs address spaces, there is no C library, and `double` is
  an extension. It takes the single-pass GPU branch HLSL and GLSL take.

  Verified on an RTX 3060 through the NVIDIA runtime: all 43 core headers build,
  and six conversions run on the device and agree with the compiled C library to
  the f32 rounding scale of each. The runtime is the compiler, so
  `clBuildProgram` is the compile check.

  `ALWAN_CONSTEXPR` is `__global const`, not `__constant`, and the reflex choice
  fails structurally: `__constant` is not part of the generic address space, in
  1.2 or 2.0, so a `__constant T *` cannot reach a function that also takes
  runtime data. `alwan_table_core`'s samplers are called both with a
  compile-time table and with a caller's own LUT, so under `__constant` one of
  the two cannot compile. Hence `-cl-std=CL2.0`, which is what program-scope
  globals need. `ALWAN_OPENCL_CONSTANT_TABLES=1` restores `__constant` for a
  1.2-only device.

  Two things carry over better than to the shading languages: OpenCL C has a
  signed `cbrt` and a truncating `fmod`, both matching libm, where HLSL and GLSL
  each need compensation. `ALWAN_ABS` maps to `fabs`, since OpenCL C `abs` is
  the integer one.

  **A deterministic OpenCL build is bit-exact against a deterministic CPU**, all
  six kernels measured, every sample. That is determinism across an architecture
  boundary and across two different languages, which is more than the CUDA
  result shows, since CUDA compiles the same source on both sides.

  It needs two things beyond the polynomials. Contraction is handled by
  `#pragma OPENCL FP_CONTRACT OFF` in the deterministic header. Division is not:
  **OpenCL does not require single-precision divide to be correctly rounded**,
  it allows 2.5 ULP, so `-cl-fp32-correctly-rounded-divide-sqrt` is required.
  Without it four of the six differ. It hides behind the primitives, because
  `log2`, `exp2`, `pow_pos` and `cbrt` divide only by `0.5` and are exact
  either way, while the sRGB EOTF divides by `1.055` and Lab by the white
  point. The flag is an optional device capability, so
  `CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT` has to be checked before the claim.

- **CUDA backend: the per-pixel core is callable from your own kernel.** The
  same shape as HLSL, GLSL and Halide. Alwan does not dispatch, allocate device
  memory or own the image; you write the `__global__` function, index the pixel
  and call the core on it. `ALWAN_CUDA` is 1 under nvcc.

  The difference from the shading languages is what it costs to get there. They
  need a substitute vocabulary and give up double precision, pointers and the C
  library. nvcc needs none of that, so a CUDA build takes the C emission path
  unchanged and **a kernel can call both the `_f32` and the `_f64` core**, the
  whole surface rather than a single-precision subset. That is also why there is
  no `ALWAN_BACKEND_CUDA` id: the emission really is the C one, and a separate
  id would make every `ALWAN_BACKEND == ALWAN_BACKEND_C` guard wrong about a
  CUDA build.

  What it needed was `__host__ __device__` on the header-only functions and a
  storage class for the constant tables that works from both passes, both keyed
  off `__CUDACC__` in `alwan_platform.h`.

  The tables were the interesting part. Plain `__device__` makes them
  unreadable from the host, and `constexpr` covers the matrices, which pass by
  value, but not the deterministic coefficient arrays, which reach Horner by
  address. The answer is the pass split, since `__CUDA_ARCH__` is defined only
  in the device pass. Getting there also routed 121 file-scope `static const`
  declarations across 19 core headers through `ALWAN_CONSTEXPR`, which is the
  macro that exists for exactly this and which they had been bypassing.

  Verified on an RTX 3060 with CUDA 12.6: all 43 core headers compile, fast and
  deterministic, and kernels run on the device and are compared against the same
  source on the host. **A deterministic build is bit-exact between the CPU and
  the GPU**, every kernel measured, both precisions, every sample. See
  `docs/backends_limits.md`.

  `--fmad=false` is required. The `#pragma STDC FP_CONTRACT OFF` in
  `alwan_deterministic.h` is a C compiler pragma and nvcc's device compiler does
  not honour it.

- **ColorChecker SG and BabelColor Average reference data.** The SG's 140
  patches (A1..N10, the formulation after November 2014) and BabelColor's
  average of 30 Classic charts, both xyY under D50 for the 1931 2 degree
  observer, from colour-science through `gendata`. The Digital SG resolves
  to the SG: it is the same physical target under its product name.

- **Five more chart tables, each under its own illuminant.** The Classic
  through its production runs (1976, before and after the November 2014
  pigment change), the SG before that change, and Image Engineering's
  TE226 V2, 45 patches. `alwan_color_checker_native_illuminant` reports
  what a table's values are published under, which is Illuminant C for the
  1976 Classic and D65 for the TE226; the lookup adapts from there rather
  than assuming D50, so reading one of those as D50 no longer silently
  chromatic-adapts the reference data itself.

- **Temporal picture formation** (`alwan_picture_form_local_exp_resume`).
  One frame of a moving picture, resuming the previous frame's solve: the
  exposure field and the smoothed adaptation base are carried by the
  caller, and `iterations` and `base_sweeps` are the budget spent catching
  up this frame. At full budget it reproduces the still-picture solve
  exactly; below it the field lags the scene, and that lag is the
  adaptation, for a few sweeps of work per frame instead of a full solve.

  Measured: a static scene settles (0.222 stops of movement on the first
  frame, 0.0078 by the twentieth); a four-stop step produces a monotone
  walk with no overshoot, reaching 63% of its travel after 17 frames at 2
  iterations and 5 at 8, so the budget steers the lag rather than the lag
  being whatever the solver happens to give. Note that with the
  scene-adaptive pivot the operator is *invariant* to a uniform change in
  the light, so a step response needs a fixed pivot to exist at all.

- **A block-aware RGB space fit** (`alwan_rgb_fit_blocks_solve`,
  `alwan_rgb_fit_blocks_evaluate`). A block format stores two endpoints per
  tile and an index per texel, so a tile is forced onto a line segment;
  BC1 is `bits_channel = {5,6,5}`, `block_size = 4`, `index_bits = 2`.
  The objective is the codec itself on a subsample of tiles, with no
  smooth surrogate, so the number reported is the number minimised.

  Measured against the ordinary fit through a real BC1 codec on five
  textures, it does not win: 1, 1, 2, 2 and 15 percent against the cloud
  fit's 1, 2, 6, 0 and 18. That is a ceiling rather than an unconverged
  search, since four times the budget converges to the identical answer.
  It is shipped because it is the correct objective for the format, it
  converges in about 200 iterations, and its answer is qualitatively
  different: it holds sRGB's triangle and moves only the scale, which is
  what a format with per-tile endpoints should want.

- **Patch names and layouts, including for a target with no colorimetry.**
  `alwan_color_checker_patch_name` says which patch an index is ("dark
  skin", "A1", "GS0"), and `alwan_color_checker_grid` gives the
  rectangular colour field: 6 by 4 for a Classic, 14 by 10 for an SG,
  22 by 12 for an IT8.7/2 whose 24 greys follow it. A name belongs to the
  target rather than to a measurement, so `ALWAN_IT8_7_2` is carried for
  its layout alone: ISO 12641 fixes 288 patches and leaves the values to
  the manufacturer and the production run, so there is no canonical IT8
  colorimetry to carry. `alwan_color_checker_num_patch_names` therefore
  answers a different question from `alwan_color_checker_num_patches`,
  which stays the count of patches alwan has values for, 0 for an IT8.
  Its numbers live in its batch reference file, which
  `gendata/openqualia.py` reads.

- **Spectral chart data, integrated rather than adapted.** Reflectance
  spectra for the ColorChecker (Ohta 1997, 24 patches, 380-780 nm at 5 nm,
  the same numbers ISO 17321-1 carries), BabelColor Average (380-730 nm at
  10 nm) and PMC (30 patches of skin tones and memory colours, 400-700 nm
  at 10 nm). `alwan_color_checker_reflectance` hands back one patch's
  spectrum on the grid it was measured on, and
  `alwan_color_checker_data` integrates it under whichever illuminant is
  asked for, normalised by a perfect diffuser so white lands at Y = 1 like
  the tristimulus tables. That is a different answer from adapting a
  tristimulus table, and the correct one: a spectrum is what a patch does
  to light, so a different light is a different integral, not a matrix.

- **Reading an OpenQualia measurement file** (`alwan_dev`,
  `gendata/openqualia.py`). OpenQualia standardises the measurement that
  ships with an individual target as CGATS.17-2009, reached from a QR label
  on the chart itself. The reader takes XYZ, Lab or spectral data out of one
  and writes the same layout alwan embeds, so a specific sheet's own values,
  including targets alwan carries no averages for such as the DT NGT2, can
  be used instead of a published average.

- **A time constant in seconds for the exposure adaptation**
  (`alwan_picture_form_local_exp_lag`, `alwan_picture_form_local_exp_apply`).
  The temporal solve converges every frame, so the field's structure is
  always the frame in hand; what carries from one frame to the next is a
  level, which is also what an eye carries. `lag` filters the solved field's
  mean toward a level the caller keeps, by `1 - exp(-dt / tau)` of the gap
  per frame, with `tau_light` when the light has gone up and `tau_dark` when
  it has gone down, and puts the difference back as a uniform offset; the
  structure does not move. `apply` forms the picture from whatever field the
  caller ends up with, and is the solve's own last step bit for bit. Before
  this the adaptation rate was whatever the per-frame budget left
  unconverged, which also left a moving edge trailing a halo of where it
  had been.

- **`alwan_zcam_from_ucs`**, the inverse of `alwan_zcam_to_ucs`. Jz, Mz
  and hz come back exactly and the other correlates are 0, as CAM16's
  `from_ucs` does. ZCAM was the one appearance model with a UCS and no way
  back from it.

### Changed

- **Interop IDs are the Color Interop Forum's published ones.** `alwan_interop_format`
  wrote `lin_ap1`, `srgb_texture`, `rec2100_pq` and the like, none of which the Forum
  publishes. It now writes the IDs of the Forum's texture (v1.1.0) and display
  (v1.0.0) recommendations, `lin_ap1_scene`, `srgb_rec709_scene`,
  `pq_rec2020_display`, the scene-referred one where both exist, and `alwan:` IDs,
  such as `alwan:logc3_awg3`, for spaces the Forum has not published, as its ID rules
  require. `alwan_interop_parse` still reads every ID alwan wrote before, and the
  Forum's display IDs. `ALWAN_RGB_SPACE_DISPLAY_P3_HDR`, P3 with PQ, is the Forum's
  `pq_p3d65_display`, not its "Display P3 HDR".

- **`ALWAN_NORMALIZE_RANGES` is documented as a library build switch.** The note in
  `alwan_platform.h` said to define it before including `alwan.h`. For a linked
  library that changes the header's NORM/DENORM helpers and nothing else. It now
  says to build alwan and the application with the same value, and
  `alwan_get_build_info` reports the library's.

- **The single-pass GPU backends get the whole deterministic layer, including
  the angle family.** HLSL, GLSL and OpenCL share one implementation of it, and
  it used to stop at the four sRGB / BT.2020 transfer functions. It now covers
  `pow`, `cbrt`, `exp`, `ln`, `log2`, `log10` and `sin`, `cos`, `tan`, `atan`,
  `atan2`, `acos`, `tanh`, from the same committed coefficient tables and the
  same Cody-Waite reduction the C path uses. The angle family matters most:
  every cylindrical space, every CAM hue correlate, dE2000 and dE CMC reach it,
  and until now all of them fell through to a hardware intrinsic under
  `ALWAN_DETERMINISTIC`.

  Verified primitive by primitive on OpenCL against a deterministic host: all
  twelve bit-exact over 65536 samples each.

  That branch is also no longer HLSL-shaped. It was written with `precise`,
  `[unroll]` and HLSL's `frexp` signature, so those spellings plus the exponent
  type moved into `alwan_platform.h`. The quadrant that `sin`, `cos` and `tan`
  share is returned by value rather than through an out-parameter, since HLSL
  spells that `out int` and OpenCL spells it `int *`, and `tan` needs it as an
  integer to test its low bit.

- **`ALWAN_DETERMINISTIC` now reaches the header-only core, which it did not.**
  `ALWAN_CORE_POW` and its siblings forwarded to `ALWAN_POW_F64`, and
  `alwan_math.h` is what redefines those to the deterministic polynomials. A
  macro expands at its use site, so the compiled library was always correct: its
  API `.c` files include `alwan.h`, and so `alwan_math.h`, before any core
  header expands anything. **A core-only translation unit never includes
  `alwan_math.h`.** A CUDA kernel, an HLSL shader, or any header-only consumer
  got libm for `pow`, `cbrt`, `exp`, `log` and the whole angle family under
  `ALWAN_DETERMINISTIC=1`, silently. The four transfer functions were the
  exception only because `alwan_core_f*_setup.h` routed those four by name.

  Every transcendental is routed there now, so the core is deterministic on its
  own terms rather than on include order. `ABS`, `SQRT`, `FLOOR`, `CEIL`,
  `ROUND`, `FMOD` and `TRUNC` are deliberately left alone: exact IEEE-754
  operations cannot differ between vendors.

  The consequence is measurable. A deterministic CUDA build is now **bit-exact
  against the CPU** on every kernel in the parity harness, both precisions,
  every sample. It was previously indistinguishable from a fast build, which is
  the symptom that led here. This class of hole is invisible to a CPU-only test
  matrix, because every runner has the same libm behind the macro.

- **Planck's law: the zero-denominator guard was dead in single precision.**
  `alwan_spd_core` compared against `1e-100`, and this template is instantiated
  at both precisions. `1e-100f` is not a representable float, so the f32 pass
  had an out-of-range constant, which is undefined behaviour and in practice
  folds to zero; the guard could never fire there and a zero denominator reached
  the division. It is `1e-37` now, representable in both and unreachable by any
  real temperature: the denominator falls below it only above 2.5e9 K. Found by
  compiling the core as CUDA, where nvcc rejects the literal outright.

- **`alwan_half_core.h` includes the header defining `ALWAN_MEMCPY`.** It used
  the macro without including `alwan_config.h` and compiled anyway, because
  every translation unit that reached it had already pulled that in through
  something else. Compiling the header on its own found it.

- **The Hunt forward agrees with colour-science to `2.8e-14`, not `4.6e-08`.**
  Nothing in the forward moved. Its reference file was written five values per
  colour, `J` through `M` with no `XYZ`, while the test read nine and indexed at
  stride nine, so it compared three colours against numbers describing none of
  them. That never failed, because the comparison printed a mismatch without
  asserting on it. The reference is regenerated with `XYZ` first over the sRGB
  cube, 45 colours under conditions that match what the test builds, and the
  test now asserts per correlate. The generator's bare `except` that turned a
  failed reference call into a row of zeros is gone as well.

- **Deterministic `log2` is a minimax fit of `log2(m)/(m-1)`, not of
  `log2(m)`.** `log2` has a zero at the `m = 1` end of its fitted domain,
  which rules out a relative-error Remez, and the absolute fit that forced
  read `2.6e-12`. The quotient is analytic and bounded away from zero across
  `[0.5, 1]`, so it fits cleanly; `alwan_det_log2` multiplies `(m - 1)` back
  in. Degree 18, and the reading is `3.3e-16`, the f64 rounding floor.
  `exp2` is refit in the same pass at degree 12 and reads `4.4e-16`. Every
  primitive defined over the pair inherits it: `ln`, `exp`, `pow_pos`,
  `cbrt`, `log10`, `tanh`. Fits are computed with Wolfram at 60 digits
  (`alwan_dev/gendata/gen_math_minimax.wls`); the header regenerates from the
  committed coefficients with Python alone, which re-derives the achieved
  error rather than trusting the recorded one.

- **The sRGB and BT.2020 transfer functions drop their own polynomials for
  `alwan_det_pow_pos`.** `alwan_det_srgb_*` and `alwan_det_bt2020_*` carried
  per-TF Chebyshev tables, a lo and a hi segment each, reading between
  `4.8e-08` and `2.5e-04`. Degree was not the problem: `x^(1/2.4)` has an
  algebraic branch point at 0 and the OETF domain starts at `0.0031308`, so
  convergence is slow whatever is spent on it, and a true minimax fit at the
  same degrees is about twice as good. `pow_pos` reduces the argument instead
  of approximating through the singularity and reads `3.3e-16`. The tables,
  the split points and the two-branch dispatch are gone, on the C and the GPU
  path alike, and with them a class of failure: a fitted polynomial evaluated
  outside its domain is not merely inaccurate, and the sRGB OETF of linear
  `4.0` returned `-9.2e10`. The C path guarded that case; the GPU path never
  did. Nothing extrapolates now. The cost is a `log2` and an `exp2` per call
  in place of one Horner, in a build that exists for reproducibility.

- **`alwan_create` refuses a configuration that cannot be meant.** A
  non-zero `flags`, which the header has always documented as reserved and
  zero, or an `alloc_cb` without its `free_cb` and the reverse, now return
  NULL. Before, the flags were stored and ignored, and a lone allocator was
  paired with the default free, so custom memory was released by the wrong
  function some time later. NULL is also what an allocation failure returns,
  so a caller that already checks the result needs nothing new.

- **`alwan_color_checker_num_patches` reports what alwan can hand you.** It
  returned 140 for the SG and 24 for BabelColor HCT while every lookup for
  those types returned an error. It now returns the length of the embedded
  table, so a type with no data reports 0, and its lookups return
  `ALWAN_E_NODATA` rather than `ALWAN_E_INVALID`.

- **Experimental RGB space fit** (`src/alwan/experimental/alwan_rgb_fit.c`).
  From a dataset of colours in any RGB space, find primaries, a white point,
  a transfer function, a free power or the sRGB curve, and a scale (the
  linear value that encodes to code 1.0) that minimise the perceptual error
  of the quantised round trip at a given bit depth. The
  solver is a deterministic Nelder-Mead on a smooth surrogate of the
  quantisation error; the report carries the true quantised numbers. Three
  ways to run again: `step` resumes, `restart` tightens the search around the
  current best, and a filled descriptor warm-starts `begin` or `solve`. The
  result is an ordinary `alwan_rgb_space_desc` plus `alwan_fit_tf`.

- **The deterministic angle family** (`ALWAN_DETERMINISTIC`). `sin`, `cos`,
  `tan`, `tanh`, `atan`, `atan2`, `acos` and `log10` are polynomial
  implementations now rather than platform libm, joining the `pow` / `exp`
  / `log` / `cbrt` set. Three minimax polynomials carry all eight, fitted
  by Remez at 60 digits; parity is factored out, so `sin(0)` and `atan(0)`
  are exactly zero and the fits sit three or more orders below f64
  epsilon. What reaches the caller is Horner rounding: 0.5 ULP for sin and
  cos, 1 ULP for atan, measured against libm.

  This is what the byte-identity contract was waiting on. Thirty families
  were outside it -- every CAM hue correlate, every LCh-style conversion,
  dE2000 and dE CMC, ACES JMh, the log10 camera curves, the Barten CSF and
  the Lanczos kernel -- for the single reason that they reached libm
  through an angle. The only math macros still reaching libm are ABS,
  CEIL, FLOOR, FMOD, ROUND, SQRT and TRUNC, all exact IEEE-754 operations.
  See [determinism.md](docs/determinism.md) for what the resulting claim
  does and does not rest on.


- **Const placement is a rule now, and gated in CI.** The project is east
  const. The GPU-portable tier is the exception and stays west, because it has
  to parse as HLSL: legacy fxc requires the qualifier before the type and
  rejects `alwan_scalar const a` outright.

  Half of this was already enforced. `check_east_const.py` banned the east form
  in the shader tier; outside it the docs said east const "is legal C and is
  left alone", so the rest of the tree was a mix. It is uniform now: 210 sites
  in the library and 100 in alwan_dev moved to east.

  Neither direction changes a type. `const T x` and `T const x` are the same,
  as are `const T *p` and `T const *p`; `T *const p` is a const pointer, a
  different type, and is untouched under either style.

  `check_const_style.py` replaces `check_east_const.py` and gates both
  directions. It also closes a gap in the old scope: the shader tier is the
  include closure of a bootstrap, not `src/alwan/core` alone, and
  `alwan_types_gen.inc` and `alwan_build_config.h` reach a shader through
  `alwan_types.h` without having been scanned. Both were clean, so the gap was
  latent rather than live.

  Vendored code is excluded and stays byte-identical to upstream: tinyexr,
  miniz, kb_text_shape and rgb2spec account for 881 of the west-const sites in
  alwan_dev, and reformatting them would cost the ability to diff against the
  version they came from.

### Datasets

Every dataset ingested for this release, with the licence it came under. The rule
is in the plan and it is the reason some of these are absent: a set is vendored
only under Apache-2.0, BSD-3-Clause, CC-BY-4.0 or an explicit grant, and anything
else is fetched at generation time into an untracked cache and used to validate
against, never shipped.

| Ingested | Source | Licence |
|---|---|---|
| 87 film stock profiles | spectral_film_lut | MIT |
| 52 camera spectral sensitivities, the 190-patch IDT training set, the ISO 7589 studio tungsten SPD | rawtoaces-data (ASWF) | Apache-2.0 |
| A 6-component PCA sensitivity basis for Jiang 2013 recovery | derived here from the rawtoaces-data cameras above | Apache-2.0, inherited |
| 59 illuminant SPDs and 56 measured light sources | colour-science `SDS_ILLUMINANTS`, `SDS_LIGHT_SOURCES` | BSD-3-Clause |
| Smith-Pokorny 1975 and Stiles-Burch 1955/1959 CMFs, the extended LEFs, CRT and Apple Studio display primaries | colour-science `MSDS_CMFS`, `SDS_LEFS`, `MSDS_DISPLAY_PRIMARIES` | BSD-3-Clause |
| Mallett 2019 and Otsu 2018 spectral bases | colour-science `MSDS_BASIS_FUNCTIONS_sRGB_MALLETT2019`, `BASIS_FUNCTIONS_OTSU2018` | BSD-3-Clause |
| ColorChecker Classic pre/post-2014, SG, BabelColor average, PMC and TE226 references | colour-science, after X-Rite's and Image Engineering's tables | BSD-3-Clause |
| ACES RICD, and the ACES 1.1-1.3 SSTS knots | colour-science `MSDS_ACES_RICD`; ACES-dev | BSD-3-Clause; AMPAS |
| Pointer's gamut volume chroma | colour-science | BSD-3-Clause |
| CSS Color Module Level 3 keywords | colour-science, after the W3C recommendation | BSD-3-Clause |
| FOGRA39L characterisation (ISO 12647-2:2004/Amd 1 coated) | Fogra | Fogra's terms: free use and redistribution, including in software, unmodified |
| Freetone, 1,310 colours in device CMYK | Stuart Semple | **none stated.** Vendored on the owner's decision, and recorded here rather than left implicit |

Matrices added this release are published constants from their papers rather
than datasets, so they carry no licence of their own: Huang 2015, Jzazbz 2021,
Kirk 2019, Ragoo 2021, SUCS, and the F-Gamut and E-Gamut primaries.

Fetched and used for validation only, never vendored, because the licence does
not allow it or does not exist: Jiang 2013 (CC-BY-NC-SA-4.0, non-commercial),
Zhao 2009, Solomatov and Akkaynak 2023, the Munsell, forest, paper and lumber
reflectance sets, Karge 2015 and Brendel 2020 light sources, Asano 2015
observers, Luo and Rhodes 1999, LUTCHI, and Hung and Berns 1995.

## [2.0.0]

First public release (tag `v2.0.0`).

The tag sits after a correctness pass over the areas that had no external
ground truth, a pass over the GPU-reachable core,
a build-matrix pass over every combination of build system, precision,
determinism, linkage and toolchain, and a pass making the GPU-portable core
compile under both shader compilers. Several entries below change
published output relative to the pre-release build; they are listed
first because a caller who pinned values against that build will see them.

### Changed: output differs

- **ACESproxy is now quantised.** It is defined as an *integer* log encoding, so
  the rounding is part of the transfer function. Linear 0.18 now encodes as
  `426/1023 = 0.4164223`, matching colour-science exactly, where it previously
  returned the continuous `426.30344/1023`. `EOTF(OETF(x))` is now a staircase and
  returns the centre of the code value the input landed in, within half a step
  (`2^(1/100) - 1 = 6.96e-03`).
- **CQS Qa** ran NIST 7.4's scaling factor (3.104) with 9.0's absent CCT factor,
  which is neither method. 9.0 uses 3.2. Deviation from colour-science over 33
  illuminants: mean 0.447 -> 0.065, max 4.490 -> 0.235, D65 99.037 -> 100.0001.
- **Y'CbCr and YcCbcCrc chroma** was centred twice in the default build
  (`ALWAN_NORMALIZE_RANGES=1`), once by the kernel and once by the normalisation
  layer, so it came out offset by 1.0 instead of 0.5. The legal/full range pair
  assumed chroma centred on 0 while every producer emits it centred on 0.5.
- **The ACES 1.x inverse output transform** now inverts its own forward. Worst
  relative round-trip over non-clipping chromatic input, all twelve outputs:
  1.31e-01 -> 1.337e-11. Three defects: the inverse RedMod10 and Glow10 were
  omitted entirely, `DCDM_48NIT` returned early through an unrelated AP1-space
  path, and the inverse dimSurround used exponent `g` where it needed `g/(1-g)`.
  alwan's RedMod10 inverse is now more accurate than OCIO's, which stops at the
  closed form and round-trips to 3.7e-03.
- **Blackbody SPDs** are documented as spectral radiance but returned exitance,
  so values were pi times too large. Now matches colour-science's `planck_law`
  to 3.7e-10. Chromaticity was unaffected either way.
- **HSLuv** returned a literal `1.0` from `l2y` for every `L > 8`, so every
  non-dark lightness decoded to white. L now round-trips to 1.4e-14.
- **HSY** decoded hue 1.0, documented as in-domain, to magenta instead of red.
- **HCL** produced `H = 5pi/3`, outside the model's `[-pi, pi]`, when `R - G` was
  small and negative.
- **`alwan_xyz_from_spd`'s `bandpass_nm`** was consumed by a `(void)` cast while
  four documents said the Stearns & Stearns 1988 correction was applied. It is
  now implemented, using the original neighbours as the paper specifies.
  colour-science's version updates its array in place and so uses already
  corrected neighbours; the two differ by 4.3e-3 and alwan follows the paper.
- **CRI, TM-30 and CQS** now use a CIE daylight reference at or above 5000 K
  rather than D65 at every CCT, and adapt test-onto-reference rather than both
  onto a common third white. CES data extended from 80 samples to the full 99.
- **Colour appearance models**: Hunt implemented in full (4.6e-08), ATD95
  `spow_response` corrected and `H` returned as the model's ratio, RLAB
  adaptation fixed in both directions (5e-07), Kim2009 `media` exposed (4e-11).
- **Unsupported transfer functions** in the ACES 1.x path returned `ALWAN_OK`
  with scene-linear values. They now return the status they got.
- **Illuminants F2, F7 and F11 had xy data but no way to reach it.**
  `illuminants_xy/f2_xy.csv`, `f7_xy.csv` and `f11_xy.csv` were generated and
  shipped, but never embedded and never given a case in
  `alwan_data_get_illuminant_xy_*`, so `alwan_illuminant_white_point_*`
  returned `ALWAN_E_INVALID` for the whole F series. The three CIE 15 names
  for practical use now resolve; the other nine fluorescents still have no xy
  table. Illuminants resolvable through the enum: 26 -> 29.

### Added

- **`ALWAN_GAMUT_FORM_COMPLETE_PEAK`.** `COMPLETE`'s wider window turned out to
  be a flat 0.945 gain on the carrier with the display peak 9.85 stops over
  mid-grey, so ordinary footage never reached white (scene 16 formed to
  0.945), and its 0.861 purity cap desaturates every saturated pixel by
  13.9% from black up, which reads as a veil inside the display range. The
  new variant keeps `COMPLETE`'s white rail, purity shelf (held at its scene
  positions, +4.3 to +13.8 stops, rather than moving with the window) and
  exact 18% anchor on the family's 16.5-stop window, with the tone exponent
  derived from the pivot's actual position (2.559) instead of assuming a
  symmetric window, and the 0.997 cap. Same operator, same 15-of-15 row.
  `COMPLETE` and `COMPLETE_HEMI_LOOK` are unchanged.

- **Sharpmake reaches the precision axis.** `ALWAN_BUILD_PRECISION` (`both`,
  `f32`, `f64`) is read at generation time and adds the matching define, so the
  same axis is drivable from both build systems instead of requiring an edit to
  `AlwanLib.cs`.

- `alwan_dev/tools/check_gpu_identifiers.py`: reserved-word lint over the
  GPU-reachable lines of every core (101 words probed against dxc, plus the
  GLSL 4.60 and Metal lists); gates the Tooling CI job and runs as a post-build
  step of the library. Reports C types and C headers in the same code without
  gating (`--strict` to gate).
- `alwan_dev/tools/check_gpu_compile.py`: every core compiled as a shader, one
  translation unit each, fast and deterministic, gated on a per-compiler
  expected-clean list; a `gpu-compile` CI job on Windows runs it.
- `docs/backends_limits.md` verification status now carries the measured
  40/43 under each compiler, and why the remaining three cannot compile.

- `alwan_table2d_grid_sample_f32` / `_f64`: bilinear and nearest sampling of a
  genuine `rows x stride` 2-d grid, which is the rank `ALWAN_SAMPLE_BILINEAR`
  fits. The 2-d strip is a flattened cube and still rejects BILINEAR.
- `ALWAN_SAMPLE_CATMULL_ROM` implemented at rank 1: the four-tap interpolating
  cubic with clamped outer taps. It can overshoot by about 1/8 of a step across
  a hard edge, so it is not any rank's default.
- `ALWAN_ROUND` on the HLSL, GLSL and Halide backends, and `ALWAN_CORE_ROUND`
  through the core aliases and both precision setups.
- `docs/alwan_decisions.md`: where alwan knowingly differs from a reference and
  why, including the house defaults, the no-gamut-clamp rule, and the precision
  model of the test suite.
- `docs/api/tables.md`: the table and LUT sampling family, which had no
  reference page. `alwan_gamut_map_spatial` and the bulk gamut forms added to
  `docs/api/gamut.md`. The remaining documentation gap is now measured rather
  than estimated: 118 of 411 base operations have no entry in `docs/api/`,
  clustered by area in `docs/alwan_future.md` theme 10.
- Both shader compilers gate it, via `--compiler dxc|fxc|both`, and both run in
  the Tooling CI job. dxc is Clang-based and accepts C-shaped syntax the legacy
  HLSL grammar never allowed; fxc (Shader Model 5, D3D11) rejects it, so
  checking only dxc hides a whole class of breakage.
- `alwan_dev/tools/check_east_const.py` (with `--fix`): gates qualifiers
  written after the type over `src/alwan/core` and the platform layer, without
  needing a shader compiler. Outside that tier east const is legal C and is not
  flagged. Preprocessor directives are skipped, since reordering
  `# define ALWAN_CONSTEXPR const` would redefine the `const` keyword itself.
- `check_core_parity.py` now compares declarations as well as function bodies.
  Named constants, embedded CSVs and struct definitions in the GPU branch were
  unchecked. A missing declaration is caught by the compilers; what only this
  catches is a constant whose *value* differs between the two copies, which
  compiles cleanly on both sides and silently gives the GPU a different answer
  from the C reference.

### Fixed: no output change

- **A single-precision build could not be linked as a shared library.**
  `ALWAN_BUILD_ONLY_F32` left 219 unresolved symbols and `ALWAN_BUILD_ONLY_F64`
  left 203, because entry points written by hand rather than emitted from the
  dual-precision `.inc` carried no precision gate, the typed `_ex` delegates
  named both precisions' workers unconditionally, and the f64-internal facades
  reached past the extent of `ALWAN_WITH_F64_FACADE`. A static archive never
  resolves a symbol nothing references and an ELF shared object permits
  undefined symbols by default, so only an MSVC DLL failed. Every pixel format
  keeps working in a single-precision build: the typed tile loaders read and
  write all of them through whichever worker was compiled. Output in the
  default dual-precision build is unchanged, and the suite executes the same
  75,034 checks it did before.
- **An f32-only build is no longer meaningfully smaller.** Measured on a static
  MSVC Release build: dual 69.3 MB, f32-only 67.1 MB, f64-only 45.8 MB. Keeping
  the f64-internal facades means an f32 entry point defined to compute in
  double pulls in the f64 spectral, colorspace, CAT, CAM, LUT and Oklab code
  and the tables it reads. `ALWAN_BUILD_ONLY_F32` selects a float-only API
  surface; `ALWAN_BUILD_ONLY_F64` is the one that selects a smaller library.
  See [docs/configuration.md](docs/configuration.md).

- **Core headers compiled as HLSL failed on reserved words.** The cores are
  one source for every backend and the_flow compiles them as HLSL through
  `alwan_hlsl.h`; a local named `out` in `alwan_math_core.h`,
  `alwan_cat_core.h` and `alwan_vision_core.h` was a hard syntax error there and
  took the whole preview shader down. Every reserved word used as a name in
  GPU-reachable core code is now renamed, in the `.h` and its `.inc` twin:
  `out` (also `alwan_hunt_core`, `alwan_extended_core.inc`), `linear` (the
  camera-log and gamma OETF parameters in `alwan_rgb_core`, `alwan_atd95_core`,
  `alwan_jzazbz_core`, `alwan_convenience_core`, `alwan_extended_core.inc`),
  `in` (`alwan_hdr_core`), `matrix` and `input` (`alwan_color_correction_core`,
  `alwan_prolab_core`). 436 identifier sites, C semantics and output unchanged
  (107/107 suites). Two more cores compile under dxc as a result: 32 of 43,
  listed in `alwan_dev/hlsl_regression/cores_dxc_clean.txt`.

- Table indices are tied to the enum that bounds them.
  `alwan_rgb_get_space_descriptor_*` bounds-checked `space` against one table's
  count and then subscripted three others; all four are now
  `_Static_assert`ed against `ALWAN_RGB_SPACE_COUNT`. `spd_copy_table` took its
  extent as a parameter unrelated to the table it was given.
- `alwan_spectral_locus_xy_*` accepted NaN: `x < MIN || x > MAX` is both-false
  for NaN, so it reached the cast and resolved to the high edge with
  `ALWAN_OK`. Range tests on float-indexed entry points are now negated.
- Two test suites could not fail. `47_llab` discarded two of its three test
  functions with `(void)` casts, and `33_rgb_spaces_p5` had twelve failure paths
  returning the runner's success code. Both were latent; alwan was right in
  every case, and the tests were wrong.
- The EXR loader (dev tool) asked OpenEXR for HALF regardless of storage, so a
  FLOAT channel saturated to inf above 65504. It now decodes as FLOAT, and
  rejects UINT rather than converting it into a plausible-looking image.
- **The HLSL headers did not compile under fxc** (Shader Model 5, D3D11), which
  a dxc-only matrix never sees. Type qualifiers were written after the type
  (`alwan_scalar const a = ...`); the legacy grammar requires them before it and
  rejects the east form with `error X3000: syntax error: unexpected token
  'const'`, then a spurious `X3080: function must return a value` from the
  enclosing function failing to parse. Rewritten to the west form at 814 sites
  across 50 files, in both the `.h` GPU branch and the `.inc` template so the
  two stay in lockstep. Value declarations only: pointer forms are untouched,
  since `T *const p` is not `const T *p`.
- `(void)x;` casts in `core/` are not valid HLSL, and were why
  `alwan_hero_wavelength_core.h` failed under fxc while passing under dxc. Now
  routed through `ALWAN_UNUSED(x)`, which is empty on the GPU branches.
- **Eight more cores compile as shaders, 32 of 43 to 40 of 43** under both
  compilers. `atd95` (correlate struct), `hunt` (surround enum and
  viewing-conditions struct), `quality` (Krystek coefficient block) and `view`
  (`alwan_cdl_apply_v`) were each missing a declaration from their GPU branch.
  `extended`, `hellwig2022` and `hdr` used raw pointer out-parameters where the
  house macros exist for exactly that; they now use `ALWAN_PARAM_*` and
  `ALWAN_CORE_PARAM_*`, so C still gets pointers and a shader gets `out`.
  `half` punned float bits through `memcpy` and now goes through
  `alwan_half_asuint` / `_asfloat`, which map to `asuint` / `asfloat` on HLSL
  and `floatBitsToUint` / `uintBitsToFloat` on GLSL.
- `alwan_config.h` included `<stddef.h>`, `<stdint.h>` and `<string.h>`
  unconditionally, so every core reaching it failed on the include line before
  any of its own code was parsed. Those, and the allocator hooks that take
  addresses, are now guarded to the CPU backends; the size and fixed-width
  spellings come from `alwan_types.h` on HLSL and GLSL.
- `alwan_content_light_level_v` is CPU-only: it walks a strided host buffer,
  which a shader has no equivalent for.
- Documentation is plain ASCII, except the Arabic spelling of the project name.
  540 typographic characters (arrows, dashes, Greek, box drawing, check marks)
  replaced with ASCII equivalents across 25 files.

### Known

- TM-30 Rf still deviates from colour-science by 0.53 mean, concentrated in the
  Planckian branch. The integration convention, the blackbody SPD, the CCT
  method and the Rf formula are all ruled out with measurements. Tracked in
  `docs/alwan_future.md`.
- `table`, `lut` and `vision` do not compile as shaders under either compiler.
  Their readers take the table by pointer and a shading language has no pointer
  type; the addressing gate itself is pure integer and float maths and would
  compile, the fetch is what does not. Serving them on GPU needs a resource
  abstraction (`Buffer` / `StructuredBuffer`, or a texture) rather than a syntax
  fix. See `docs/backends_limits.md`.

---

### Foundation

The pre-release build the hardening pass started from. Everything below plus
the dated foundation block that follows is also in the tag.

### Added
- **ACES constants on the public macro surface**: `ALWAN_AP0_RED_x` .. 
  `ALWAN_AP0_BLUE_y` (SMPTE ST 2065-1 primaries) and `ALWAN_ACES_WHITE_x/y`,
  so ACES matrices can be derived from `alwan_platform.h` without hardcoding.
  Previously only the AP1 primaries and a D65 white were exported, and pairing
  those is a colour space that does not exist: the derived RGB-to-XYZ matrix is
  off by 8e-2 per element against ACEScg, and it compiled without complaint.
  `ALWAN_D60_x/y` (CIE D60) is also exported, and is deliberately NOT the same
  number as `ALWAN_ACES_WHITE_x/y`: ACES pins its white to a rounded value that
  differs from CIE D60 in the fifth decimal. Reported by the_flow2 after hitting
  the AP1-plus-D65 combination in their EXR loader.
- **Table reader layer**: every embedded table is now read through a
  declared reader (`alwan_table1d_sample`, `alwan_table2d_sample`,
  `alwan_table3d_sample`, `alwan_table1d_mat3_sample`, plus named readers
  for the AgX contrast curves, the AgX Blender cube and the Jakob2019
  coefficients). Readers take an `alwan_sample_mode`
  (`LINEAR`/`NEAREST`/`TRILINEAR`/`TETRAHEDRAL`, optionally
  `| ALWAN_SAMPLE_STRICT`), and a single addressing gate converts the
  coordinate to a validated cell, so no reader indexes a table unchecked.
  Definitions live in `src/alwan/data/`, declarations in `alwan.h`.
- **`alwan_version_string()`**: reports the version compiled into the
  binary, for checking a dynamically loaded library against the headers.
- **`.cube` import size query**: passing `NULL` for the LUT buffer parses
  the header and returns the cube size, so callers can allocate exactly.
- **`ALWAN_READ_DATA_NO_BOUND_CHECK`**: opt-in switch that compiles the
  addressing clamp out when every coordinate is known finite and in range.

### Fixed
- **Five camera log curves computed the wrong transfer function**: T-Log,
  REDLog, REDLogFilm, L-Log and ACESproxy each shipped a plausible-looking but
  incorrect curve, and V-Log picked the wrong branch exactly at its cut point.
  All six now match colour-science (and therefore OCIO / aces-dev) to f64
  round-off:
  - `ALWAN_TF_TLOG` was `0.33*log10(lin/0.01)+0.02`, a generic Cineon-shaped
    log that is neither FilmLight T-Log (0.21 away) nor Panasonic V-Log (0.16
    away), while its comment claimed V-Log and the README claimed T-Log. It is
    now the real FilmLight T-Log, including the `T-Log(0) = 0.075` black offset
    the old curve lacked. Pairs with E-Gamut, whose primaries were already
    correct.
  - `ALWAN_TF_REDLOG` and `ALWAN_TF_REDLOGFILM` used an invented
    `(log10(0.9x + 0.1) + 3)/3` form. They now use the Sony Imageworks
    reference encodings; REDLogFilm is Cineon, as the reference defines it.
  - `ALWAN_TF_LLOG` used Panasonic V-Log's constants. It now uses Leica's
    published L-Log parameters (`cut1 = 0.006`, `a = 8`, `b = 0.09`,
    `c = 0.27`, `d = 1.3`, `e = 0.0115`, `f = 0.6`).
  - `ALWAN_TF_ACESPROXY` omitted the `2^-9.72` floor and the
    `[CV_min, CV_max]` clamp entirely and used `log2(lin/0.18)` where
    S-2013-001 specifies `mid_log_offset = 2.5`; it was off by up to 113 code
    values. The spec's integer rounding is still left to the caller so the
    EOTF stays an exact inverse.
  - `ALWAN_TF_VLOG` split on `linear <= cut1`; Panasonic's spec splits on
    `linear < cut1`, so the cut point itself belongs to the log segment.
- **E-Log attributed to the wrong manufacturer**: `ALWAN_TF_ELOG` is
  Olympus/OM System OM-Log400, not a FilmLight curve (FilmLight's companion to
  T-Log is E-Gamut, a gamut, not a transfer function). The implementation was
  always correct; the enum comment, README and `docs/determinism.md` were not.
  The enum name is unchanged for compatibility.

- **Out-of-bounds table reads on non-finite coordinates**: a NaN
  coordinate defeated every `SATURATE`/range guard (all comparisons against
  NaN are false) and reached an `(int)`/`(size_t)` cast, which is undefined
  behaviour and yields `INT_MIN` on x86-64. That indexed the table far out
  of bounds. Affected the AgX contrast LUT, the AgX Blender cube, the public
  `alwan_lut{1,2,3}d_sample` family, `alwan_table_interp_1d`,
  `alwan_table_interp_3d_{trilinear,tetrahedral}` and SPD resampling.
  A NaN coordinate now resolves to the low edge everywhere; results for
  finite in-range input are unchanged, bit for bit.
- **Picture formation**: `alwan_gamut_map_spatial` with 18 spatial
  formation methods (`alwan_gamut_formation_method`), developed in
  correspondence with Troy Sobotka; the `COMPLETE` /
  `COMPLETE_HEMI_LOOK` operators are the first to satisfy all 15
  numerically-testable formation constraints at once, and the constraint
  matrix ships as a regression test (`docs/picture_formation.md`).
- **Experimental formation tier**: `src/alwan/experimental/`:
  evidence-driven research operators (`alwan_picture_form_hybrid_exp`,
  `_global_exp`, `_local_exp`, `_evidence`, `_pure_exp`), clearly fenced
  and structurally tested.
- **AgX family expansion**: `ALWAN_VIEW_AGX_{ORIGINAL,PUNCHY,GOLDEN,
  SB2383,BLENDER}` presets plus the parameterized analytic AgX engine and
  the JP2499 display transform; HLSL-compilable analytic core.
- **Cross-platform byte-identical file I/O**: CLF (SMPTE ST 2136-1:2024)
  and `.cube` export are hardened to produce byte-for-byte identical files
  on every supported platform/precision, including native-f32 cube export.
- **File-I/O byte content on the determinism contract**: the determinism
  guarantee explicitly covers the exact bytes written to `.cube`/`.clf`
  files as well as in-memory numeric output; the harness MD5s the on-disk
  files and asserts hash equality across runners.

### Improved
- **Native f32/f64 dualization**: the allocator, cube, view, CAM, RLAB,
  SPD, and spectrum-upsampling subsystems are native dual-precision
  (cores instantiated once per precision, computing in float throughout);
  the iterative-inverse / least-squares cores (ZCAM/ACES 1.x inverses,
  CCM fits, gamut volume/coverage) remain f64-internal facades gated by
  `ALWAN_WITH_F64_FACADE` (always `1`). Single-precision-only builds via
  `ALWAN_BUILD_ONLY_F32` / `ALWAN_BUILD_ONLY_F64`.
- **NEON gamut kernel completion**: the aarch64 NEON gamut-mapping kernel
  reaches parity with the SSE2/AVX/AVX2 kernels (select-vs-mask path).
- **Determinism CI on six runners**: `determinism.yml` runs on all six
  Linux/Windows/macOS x x86_64/aarch64 targets and asserts byte/hash
  equality; the `det_run_regression` dump pins the full public API surface
  (~407k lines), with NEON f64 exercised so FMA-contraction or SIMD
  reduction-order drift fails the build.
- **Test coverage**: 102 test suites (up from 84).

---

## [2.0.0-foundation]

### Added
- **F16 pixel format**: half-float as a first-class pixel format (U8/U16/F16/F32/F64 parity)
- **F32/F64 runtime dispatch**: single binary emits both precision variants; no recompilation required
- **LUT system**: 1D/2D/3D LUT baking, import/export (.cube, CLF/Color Interop Forum XML)
- **CVD simulation**: Machado 2009 for protanopia, deutanopia, tritanopia at 11 severity levels
- **HDR tone mapping**: BT.2446 Method A, BT.2390 EETF, Reinhard 2002, Stachowiak 2023, MaxCLL/MaxFALL
- **Video signal encoding**: full and narrow range YCbCr per SMPTE BT.601/709/2020
- **High-level image API**: `alwan_image_convert`, `alwan_image_convert_rgba` with stride and premultiplied alpha
- **New color spaces**: LCH(ab), HLC, HSLuv, HPLuv, OkHSL, OkHSV
- **Color Interop Forum**: CIF interoperability helpers and CLF export
- **GPU shader backends**: GLSL, HLSL, Halide header-only backends

### Improved
- **SIMD vectorization**: AVX2 kernels for sRGB<->XYZ, sRGB<->CIELab, sRGB<->Oklab; force-inlined eotf/oetf under SVML
- **U16 SIMD**: AVX2 paths for U16 planar (8 px/batch) and AoS (4 px/batch); previously scalar
- **SIMD scalar arrays**: `alwan_eotf_apply` / `alwan_oetf_apply` gain contiguous SIMD fast paths
- **Test coverage**: 84 test suites (up from 75)

---

## [1.0.0]

Initial release.
