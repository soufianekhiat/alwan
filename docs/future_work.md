# Future work

Things alwan does not do yet, and knows it. Kept separate from
[alwan_decisions.md](alwan_decisions.md), which is for choices that are settled:
nothing here is a decision, and nothing there is a plan.

An entry earns a place here by being a real gap with a known shape. "Could be
faster" and "might be nice" do not qualify.

---

## Hunt inverse

`alwan_hunt_forward_*` is implemented and matches colour-science to 4.6e-08. The
inverse is not implemented.

The model is invertible in principle, but the forward runs a chromatic adaptation
whose parameters depend on the adapted signal, so the inverse needs an iterative
solve rather than a closed form. The same shape as the ACES 1.x RedMod10 inverse,
which is a bracketed scalar root find; Hunt's is three-dimensional.

Nothing else in the appearance-model set is missing its inverse.

## TM-30 Rf sits an order of magnitude above CRI and CQS

Measured against colour-science over 33 illuminants, on the same 360-830 nm
trapezoid grid:

| metric | mean | max |
|---|---|---|
| CRI Ra | 0.057 | 0.428 |
| CQS Qa | 0.065 | 0.235 |
| TM-30 Rf | **0.530** | **1.990** |

`alwan_decisions.md` used to attribute TM-30's residual to the integration
convention. That attribution was never measured, and it has since been disproved
for CQS, whose residual was a crossed scaling factor and fell to 0.065 with the
integration untouched. Since all three share the grid, the convention accounts
for roughly 0.06, not 0.53.

Worst offenders are HP1 (1.99), FL4 (1.57) and HP3 (1.43): high-pressure sodium
and a narrow-band fluorescent, which is the shape of a reference-illuminant or
sample-set difference rather than an arithmetic one. The CES sample set was
extended from 80 to the full 99 during the pre-release work, so the count is no
longer a candidate. Unresolved.

## Sample modes pinned but not implemented

`ALWAN_SAMPLE_BILINEAR` and `ALWAN_SAMPLE_CATMULL_ROM` exist in the enum so their
values are stable, and return `ALWAN_E_INVALID`. Rejecting rather than silently
downgrading is deliberate and documented in alwan_decisions.md; the gap is that
they are not implemented at all.

Catmull-Rom in particular is worth having for LUT sampling, where the current
choice is linear or nearest.

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
