# Code Quality Audit Notes

This file is a historical audit summary, not a live authoritative count of
every current violation in the repository.

Older versions of this document tracked large fixed/TODO tables. Those totals
were useful during the audit pass, but they should no longer be read as an
exhaustive current-state metric unless someone recomputes them from the repo.

---

## What Was Verified As Landed

The following broad outcomes are still directly supported by the current source
layout and comments referenced in the earlier audit:

- tolerance handling was consolidated instead of being scattered per test
- several inline matrices were moved to CSV-backed includes
- photopic and scotopic LUTs were moved out of hardcoded source arrays
- many transfer-function and model constants gained source comments

Those changes are visible in the current `src/alwan/` tree and are the lasting
value of the original audit.

---

## Remaining Follow-Ups Worth Keeping Visible

The earlier audit still points to a few categories that remain useful as
follow-up work items:

- document or regenerate the AgX curve polynomial provenance
- add missing literature/spec citations for:
  - CIE Luv constants
  - DIN99 constants
  - Hunter Lab constants
  - ProLab constants
  - OSA-UCS constants
- document the source/provenance of the ACES 2.0 Fourier chroma normalization
  arrays
- clean up remaining hardcoded-reference TODOs in Python `gendata` scripts if
  those scripts return to active use

These are better treated as documentation/provenance tasks than as blockers for
the shipped C API.

---

## How To Read This File

- Use it as a reminder of provenance and audit debt.
- Do not assume old fixed/TODO counts are current.
- Re-run a fresh audit if you need exact present-day statistics.

---

## Suggested Rule For Future Updates

When adding to this file:

1. describe the category of issue
2. link it to a current source location or current follow-up task
3. avoid stale aggregate counts unless they are recomputed in the same change
