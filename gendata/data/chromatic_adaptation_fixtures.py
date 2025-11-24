"""
Generate chromatic adaptation test fixtures.
Source: colour.adaptation functions

Test colors are hardcoded (inputs).
Adapted colors and CAT matrices are computed from colour-science.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import save_vector, save_matrix

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


# Test XYZ colors (inputs - hardcoded)
TEST_XYZ_COLORS = [
    [0.95047, 1.0, 1.08883],             # D65 white
    [0.5, 0.5, 0.5],                     # Mid-gray
    [0.412456, 0.212673, 0.019334],      # sRGB red (D65)
    [0.357576, 0.715152, 0.119192],      # sRGB green (D65)
    [0.180437, 0.072175, 0.950304],      # sRGB blue (D65)
]

# CAT test cases: (src_white, dst_white, method)
CAT_TEST_CASES = [
    ('d65', 'd50', 'Bradford'),
    ('d50', 'd65', 'Bradford'),
    ('a', 'd65', 'Bradford'),
    ('d65', 'd60', 'Bradford'),
    ('d65', 'd50', 'CAT02'),
    ('d65', 'd50', 'CAT16'),
    ('d65', 'd50', 'XYZ Scaling'),
]


def generate_white_points(output_dir):
    """Generate XYZ values for standard illuminants."""

    print("\nGenerating white point XYZ values...")

    observer = 'CIE 1931 2 Degree Standard Observer'

    illuminants = {
        'd65': 'D65',
        'd60': 'D60',
        'd55': 'D55',
        'd50': 'D50',
        'a': 'A',
        'e': 'E',
    }

    white_points = {}
    for name, illuminant in illuminants.items():
        xy = colour.CCS_ILLUMINANTS[observer][illuminant]
        xyz = colour.xy_to_XYZ(xy)
        white_points[name] = xyz

        filepath = os.path.join(output_dir, 'fixtures', f'{name}_xyz.csv')
        save_vector(xyz, filepath, f"{illuminant} white point (XYZ)")

    return white_points


def generate_cat_matrices(output_dir, white_points):
    """Generate chromatic adaptation matrices."""

    print("\nGenerating chromatic adaptation matrices...")

    for src_name, dst_name, method in CAT_TEST_CASES:
        src_wp = white_points[src_name]
        dst_wp = white_points[dst_name]

        # Compute CAT matrix from colour-science
        cat_matrix = colour.adaptation.matrix_chromatic_adaptation_VonKries(
            src_wp, dst_wp, transform=method
        )

        # Save matrix
        filename = f'cat_{src_name}_to_{dst_name}_{method.lower().replace(" ", "_")}.csv'
        filepath = os.path.join(output_dir, 'fixtures', filename)
        save_matrix(cat_matrix, filepath, f"CAT {src_name}→{dst_name} ({method})")


def generate_adapted_colors(output_dir, white_points):
    """Generate adapted color test fixtures."""

    print("\nGenerating adapted color fixtures...")

    # Save test colors
    filepath = os.path.join(output_dir, 'fixtures', 'test_xyz_colors.csv')
    flat_colors = []
    for xyz in TEST_XYZ_COLORS:
        flat_colors.extend(xyz)
    save_vector(flat_colors, filepath, f"{len(TEST_XYZ_COLORS)} test XYZ colors")

    # Adaptation: D65 → D50 (Bradford)
    adapted_d65_d50 = []
    for xyz in TEST_XYZ_COLORS:
        adapted = colour.adaptation.chromatic_adaptation_VonKries(
            xyz, white_points['d65'], white_points['d50'], transform='Bradford'
        )
        adapted_d65_d50.extend(adapted)

    filepath = os.path.join(output_dir, 'fixtures', 'adapted_d65_to_d50_bradford.csv')
    save_vector(adapted_d65_d50, filepath, "Colors adapted D65→D50 (Bradford)")

    # Adaptation: A → D65 (Bradford)
    adapted_a_d65 = []
    for xyz in TEST_XYZ_COLORS:
        adapted = colour.adaptation.chromatic_adaptation_VonKries(
            xyz, white_points['a'], white_points['d65'], transform='Bradford'
        )
        adapted_a_d65.extend(adapted)

    filepath = os.path.join(output_dir, 'fixtures', 'adapted_a_to_d65_bradford.csv')
    save_vector(adapted_a_d65, filepath, "Colors adapted A→D65 (Bradford)")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python chromatic_adaptation_fixtures.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    white_points = generate_white_points(output_dir)
    generate_cat_matrices(output_dir, white_points)
    generate_adapted_colors(output_dir, white_points)
