"""
Generate Oklab and Oklch test fixtures.
Source: colour.XYZ_to_Oklab

Test XYZ colors are hardcoded (inputs).
Oklab and Oklch values are computed from colour-science.
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


# Test XYZ colors (inputs - hardcoded, D65 illuminant)
OKLAB_TEST_XYZ = [
    [0.95047, 1.00000, 1.08883],  # D65 white
    [0.00000, 0.00000, 0.00000],  # Black
    [0.41246, 0.21267, 0.01933],  # sRGB red
    [0.35758, 0.71515, 0.11919],  # sRGB green
    [0.18048, 0.07217, 0.95030],  # sRGB blue
    [0.76986, 0.92783, 0.13853],  # sRGB yellow
    [0.53806, 0.78732, 1.06950],  # sRGB cyan
    [0.59294, 0.28484, 0.96963],  # sRGB magenta
    [0.20517, 0.21586, 0.23306],  # Mid gray
    [0.09505, 0.10000, 0.10888],  # Dark gray
]


def generate_oklab_fixtures(output_dir):
    """Generate Oklab test fixtures."""

    print("\nGenerating Oklab fixtures...")

    # Compute Oklab values from colour-science
    oklab_data = []
    for xyz in OKLAB_TEST_XYZ:
        oklab = colour.XYZ_to_Oklab(np.array(xyz))

        # Store XYZ input and Oklab output
        oklab_data.extend(xyz)
        oklab_data.extend(oklab.tolist())

    filepath = os.path.join(output_dir, 'fixtures', 'oklab_values.csv')
    save_vector(oklab_data, filepath,
                f"{len(OKLAB_TEST_XYZ)} colors (XYZ->Oklab, 6 values/color)")


def generate_oklch_fixtures(output_dir):
    """Generate Oklch test fixtures."""

    print("\nGenerating Oklch fixtures...")

    # Compute Oklch from Oklab using colour-science
    oklch_data = []
    for xyz in OKLAB_TEST_XYZ:
        oklab = colour.XYZ_to_Oklab(np.array(xyz))

        # Convert Oklab to Oklch using colour-science
        oklch = colour.Oklab_to_Oklch(oklab)

        # Store Oklab input and Oklch output
        oklch_data.extend(oklab.tolist())
        oklch_data.extend(oklch.tolist())

    filepath = os.path.join(output_dir, 'fixtures', 'oklch_values.csv')
    save_vector(oklch_data, filepath,
                f"{len(OKLAB_TEST_XYZ)} colors (Oklab->Oklch, 6 values/color)")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python oklab_fixtures.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_oklab_fixtures(output_dir)
    generate_oklch_fixtures(output_dir)
