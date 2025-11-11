# Alwan Data Files

This directory contains reference colour science data used by the Alwan library.

## Data Sources

All data files are generated from the authoritative [Colour](https://www.colour-science.org/) Python package using the `generate_data.ps1` script in the root directory.

## File Format

All CSV files use C-parsable format with maximum precision numeric literals:
- No headers (for easy C parsing)
- Comma-separated values
- Maximum available precision from colour-science package
- No quotes or whitespace

## Files

### Illuminants

- `d65_xy.csv`: CIE D65 standard illuminant (x, y chromaticity coordinates)
  - Used by: sRGB, BT.709, Display P3, BT.2020

- `d60_xy.csv`: CIE D60 standard illuminant (x, y chromaticity coordinates)
  - Used by: ACES color spaces

### RGB Primaries (Legacy Format)

- `srgb_primaries_3x2.csv`: sRGB primaries in 3×2 format (rx,ry,gx,gy,bx,by)
  - Maintained for backward compatibility with M0 tests

### RGB Color Spaces

The `rgb_spaces/` directory contains complete RGB color space definitions.
Each file contains a single line with 8 numeric values in the format:
```
rx,ry,gx,gy,bx,by,wx,wy
```

Where:
- `rx, ry`: Red primary chromaticity coordinates
- `gx, gy`: Green primary chromaticity coordinates
- `bx, by`: Blue primary chromaticity coordinates
- `wx, wy`: White point chromaticity coordinates

Available color spaces:
- `srgb.csv`: sRGB (IEC 61966-2-1)
- `bt709.csv`: ITU-R BT.709 (identical primaries to sRGB)
- `display_p3.csv`: DCI-P3 with D65 white point
- `bt2020.csv`: ITU-R BT.2020 (Rec. 2020)
- `aces2065_1.csv`: ACES2065-1 (AP0 primaries, Academy Color Encoding System)
- `acescg.csv`: ACEScg (AP1 primaries, ACES Computer Graphics)
- `acesproxy.csv`: ACESproxy (AP1 primaries)

Example (sRGB):
```
0.64000000000000001,0.33000000000000002,0.29999999999999999,0.59999999999999998,0.14999999999999999,0.059999999999999998,0.31269999999999998,0.32900000000000001
```

## Regenerating Data

To regenerate all data files from the latest colour-science package:

```powershell
.\generate_data.ps1
```

Requirements:
- Python 3.7+
- colour-science package (auto-installed if missing)

## Data Validation

All data can be validated against the source by running:

```python
import colour
# Example: Check D65
print(colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D65'])
# Example: Check sRGB
srgb = colour.RGB_COLOURSPACES['sRGB']
print(srgb.primaries, srgb.whitepoint)
```

## Notes

- ACES2065-1 (AP0) primaries include imaginary colors (negative y-coordinate for blue primary)
- These imaginary primaries can cause numerical challenges in matrix operations
- All BT.709 and sRGB use identical primaries and D65 white point
- Display P3 uses DCI-P3 primaries with D65 white point (not DCI white)
