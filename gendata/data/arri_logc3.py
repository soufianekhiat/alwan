"""
Generate ARRI Wide Gamut / LogC3 color space data.
Source: colour-science RGB color spaces

ARRI LogC3 is a logarithmic encoding used with ARRI Wide Gamut color space.
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


def generate_arri_logc3(output_dir):
    """Generate ARRI LogC3 color space data."""

    print("\nGenerating ARRI LogC3 color space data...")

    try:
        # Get ARRI Wide Gamut 3 color space (LogC3 uses this gamut)
        arri_space = colour.RGB_COLOURSPACES['ARRI Wide Gamut 3']

        # Extract color space parameters
        primaries = arri_space.primaries
        whitepoint = arri_space.whitepoint

        # Flatten data: R_xy, G_xy, B_xy, W_xy (8 values total)
        space_data = [
            primaries[0][0], primaries[0][1],  # Red xy
            primaries[1][0], primaries[1][1],  # Green xy
            primaries[2][0], primaries[2][1],  # Blue xy
            whitepoint[0], whitepoint[1]       # White xy
        ]

        filepath = os.path.join(output_dir, 'rgb_spaces', 'arri_logc3.csv')
        save_vector(space_data, filepath, "ARRI LogC3 (Wide Gamut 3) color space")

        print(f"    Primaries: R({primaries[0][0]:.4f}, {primaries[0][1]:.4f}), "
              f"G({primaries[1][0]:.4f}, {primaries[1][1]:.4f}), "
              f"B({primaries[2][0]:.4f}, {primaries[2][1]:.4f})")
        print(f"    Whitepoint: ({whitepoint[0]:.4f}, {whitepoint[1]:.4f})")

    except KeyError:
        print("  WARNING: ARRI Wide Gamut 3 not found in colour-science")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python arri_logc3.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_arri_logc3(output_dir)
