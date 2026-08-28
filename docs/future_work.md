# Future work

Things alwan does not do yet, and knows it. Kept separate from
[alwan_decisions.md](alwan_decisions.md), which is for choices that are settled:
nothing here is a decision, and nothing there is a plan.

An entry earns a place here by being a real gap with a known shape. "Could be
faster" and "might be nice" do not qualify.

---

## Hunt inverse -- planned for 3.0.9

`alwan_hunt_forward_*` is implemented and matches colour-science to 4.6e-08. The
inverse is not implemented, and is deliberately not in the 2.0.0 scope.

The model is invertible in principle, but the forward runs a chromatic adaptation
whose parameters depend on the adapted signal, so the inverse needs an iterative
solve rather than a closed form. The same shape as the ACES 1.x RedMod10 inverse,
which is a bracketed scalar root find; Hunt's is three-dimensional.

Nothing else in the appearance-model set is missing its inverse.

## TM-30 Rf residual, 0.53 mean against colour-science

Measured over 33 illuminants. CRI and CQS, on the same grid and the same
integration, sit an order of magnitude lower:

| metric | mean | max |
|---|---|---|
| CRI Ra | 0.057 | 0.428 |
| CQS Qa | 0.065 | 0.235 |
| TM-30 Rf | **0.530** | **1.990** |

### Ruled out, with measurements

- **The integration convention**, which `alwan_decisions.md` used to blame. The
  white point of a blackbody over 360-830 nm against 380-780 nm differs by 1e-6
  in xy from 1959 K to 6504 K; colour-science's own Rf at 1 nm against 5 nm
  differs by 0.0000; and CRI and CQS share the grid at 0.06.
- **The blackbody SPD.** alwan's and colour-science's agree to 5.3e-15 in shape
  at 2856 K over the whole 360-830 nm grid.
- **The CCT method.** alwan uses Robertson, colour-science uses Ohno 2013. Mean
  difference 1.9 K, max 7.1 K, and **uncorrelated** with the Rf error (r = -0.07).
  Illuminant A differs by 0.2 K and is still off by 1.24.
- **The Rf formula.** `10 ln(exp((100 - 6.73 dE)/10) + 1)` and the sample-count
  divisor are both correct.

### Where it is

Entirely in the Planckian branch:

| reference branch | n | mean | max |
|---|---|---|---|
| Planckian, CCT < 4000 K | 12 | **0.980** | 1.990 |
| blend, 4000-5000 K | 10 | 0.305 | 0.887 |
| daylight, CCT > 5000 K | 11 | 0.244 | 0.408 |

### The sharpest lead

Self-referential cases do not return 100. Illuminant A **is** a Planckian at
2856 K, so its reference is its own spectrum and every sample's dE should be
zero. colour-science returns exactly 100.000; alwan returns 98.762, which back-
solves to a mean dE of 0.183 in CAM02-UCS units. D65 against its own daylight
reference returns 99.739 rather than 100.

So something adds a small, roughly uniform offset between a spectrum and a
reference built from that same spectrum, and it is about four times larger on the
Planckian branch than the daylight one. That points at the reference construction
or the CIECAM02 / CAM02-UCS path rather than at any of the items ruled out above.

Next step is to dump alwan's per-sample dE for illuminant A and compare against
colour-science's, which will separate "uniform offset" (adaptation or white
point) from "a few samples" (CES data).

## Corpus files carry no chromaticities

Raised by the_flow2 in `alwan_dev/to_alwan.txt`, and it is a corpus decision
rather than a library one, so it sits here until someone makes it.

151 of the 166 SRIC EXR files declare no chromaticities attribute. alwan reads
them as linear AP0 from provenance; a general reader must treat absent
chromaticities as Rec.709, because that is what the OpenEXR specification says.
Both readings are correct in their own scope, and the report established that the
pixel data does not settle it either way.

Stamping the chromaticities attribute on those files would make the corpus
self-describing and remove the disagreement for every downstream reader. That is
a change to the corpus, not to alwan.

## EXR loader, from `alwan_dev/to_alwan.txt`

`image_gen/src/exr_loader.cpp` is a dev tool, not shipped library code. UINT
channels are now rejected rather than converted into a plausible-looking image
of nonsense. What the report raised and remains:

- **FLOAT channels are converted, and values above 65504 saturate to inf.**
  OpenEXR does the conversion, so this is correct up to that ceiling, but the
  ceiling is silent. Worth fixing before the loader is pointed at the corpus for
  the chromaticities question above, since an inf would corrupt exactly the
  out-of-gamut statistics that question turns on.
- **Non-zero data window origin is untested.** The code handles it through
  `(y - dw.min.y)`, but no file in the corpus has overscan, so that path has
  never run. Nice to have. The reporter notes their own loader has the same gap
  for the same reason.
- **Alpha is not read.** 165 of 247 files carry an A channel; only R, G and B
  are routed into the buffer. This is by design for what image_gen does, and is
  recorded here only so the next reader does not take it for an oversight.
- **The reporter offered their loader and their header-survey script.** Worth
  taking up; not yet done.
