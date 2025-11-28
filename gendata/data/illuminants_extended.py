"""
Generate additional illuminant data.
Source: colour-science

Generates:
- Illuminant B xy chromaticity coordinates
- Illuminant A SPD (360-830nm @ 1nm) - extended range
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import save_vector

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


def generate_illuminants_extended(output_dir):
    """Generate additional illuminant data."""

    print("\nGenerating additional illuminant data...")

    # 1. Illuminant B xy coordinates
    try:
        # Get Illuminant B from colour-science
        b_xy = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['B']

        filepath = os.path.join(output_dir, 'illuminants_xy', 'b_xy.csv')
        save_vector([b_xy[0], b_xy[1]], filepath, "Illuminant B xy chromaticity")
    except KeyError:
        print("  WARNING: Illuminant B not found in colour-science")

    # 2. Illuminant A SPD (extended range 360-830nm @ 1nm)
    try:
        # Get Illuminant A SPD and resample to extended range
        a_spd = colour.SDS_ILLUMINANTS['A']

        # Create extended wavelength range
        wavelengths = np.arange(360, 831, 1)  # 360-830nm @ 1nm

        # Interpolate SPD to extended range
        a_extended = []
        for wl in wavelengths:
            a_extended.append(a_spd[wl])

        filepath = os.path.join(output_dir, 'illuminants', 'A_360_830_1nm.csv')
        save_vector(a_extended, filepath, f"Illuminant A SPD (360-830nm @ 1nm, {len(wavelengths)} samples)")
    except Exception as e:
        print(f"  WARNING: Could not generate extended Illuminant A SPD: {e}")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python illuminants_extended.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_illuminants_extended(output_dir)
