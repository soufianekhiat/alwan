# Color Difference API

Functions for calculating perceptual color differences using various ΔE (delta E) metrics.

---

## Overview

Color difference metrics quantify how different two colors appear to human vision:

- **ΔE76 (CIE 1976)** — Simple Euclidean distance in Lab
- **ΔE94 (CIE 1994)** — Improved with weighting factors
- **ΔE00 (CIEDE2000)** — Most accurate, industry standard
- **ΔE CMC** — Textile industry standard
- **ΔE ITP** — For HDR content (ICtCp-based)

---

## Perceptual Thresholds

| ΔE Value | Perception |
|----------|-----------|
| < 1.0 | Not perceptible |
| 1.0 - 2.0 | Perceptible through close observation |
| 2.0 - 10.0 | Perceptible at a glance |
| > 10.0 | Colors are clearly different |

---

## Single-Element Functions

### alwan_delta_e_76

```c
alwan_scalar alwan_delta_e_76(alwan_lab const *lab1, alwan_lab const *lab2);
```

Euclidean distance in Lab space. Simplest but least accurate.

---

### alwan_delta_e_94

```c
alwan_scalar alwan_delta_e_94(alwan_lab const *lab1, alwan_lab const *lab2);
```

CIE 1994 color difference with default weighting (graphic arts).

---

### alwan_delta_e_2000

```c
alwan_scalar alwan_delta_e_2000(alwan_lab const *lab1, alwan_lab const *lab2);
```

CIEDE2000 - most accurate for perceptual color differences.

---

### alwan_delta_e_cmc

```c
alwan_scalar alwan_delta_e_cmc(alwan_lab const *lab1, alwan_lab const *lab2,
                               alwan_scalar l, alwan_scalar c);
```

CMC l:c color difference. Use l=2, c=1 for acceptability; l=1, c=1 for perceptibility.

---

### alwan_delta_e_itp

```c
alwan_scalar alwan_delta_e_itp(alwan_ictcp const *itp1, alwan_ictcp const *itp2,
                               alwan_scalar scalar_factor);
```

ICtCp-based metric for HDR content. Use scalar_factor=720 for standard usage.

---

## Bulk Functions

### alwan_delta_e_76_bulk

```c
int alwan_delta_e_76_bulk(
    alwan_scalar *delta_e_out,    // Output ΔE values
    alwan_lab const *lab1_in,     // First color array
    alwan_lab const *lab2_in,     // Second color array
    size_t count,
    size_t in1_stride,            // Stride in bytes for lab1
    size_t in2_stride             // Stride in bytes for lab2
);
```

Batch ΔE*76 calculation. Returns `ALWAN_OK` on success.

---

### alwan_delta_e_2000_bulk

```c
int alwan_delta_e_2000_bulk(
    alwan_scalar *delta_e_out,
    alwan_lab const *lab1_in,
    alwan_lab const *lab2_in,
    size_t count,
    size_t in1_stride,
    size_t in2_stride
);
```

Batch CIEDE2000 calculation.

---

## Usage Example

```c
alwan_lab lab1 = {50.0, 10.0, 20.0};
alwan_lab lab2 = {52.0, 12.0, 18.0};

alwan_scalar de76 = alwan_delta_e_76(&lab1, &lab2);
alwan_scalar de2000 = alwan_delta_e_2000(&lab1, &lab2);

printf("ΔE76: %.2f, ΔE00: %.2f\n", de76, de2000);
```

---

## Error Codes

- `ALWAN_OK` (0) — Success
- `ALWAN_E_INVALID` (-1) — NULL pointer

---

## See Also

- [Color Spaces](color-spaces.md) — Lab conversions
- [Color Appearance](color-appearance.md) — CAM-based metrics
