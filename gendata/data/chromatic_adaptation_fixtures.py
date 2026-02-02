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


# Test XYZ colors - get from colour-science for consistency
def get_test_xyz_colors():
    """Generate test colors matching test_reference_values.py."""
    observer = 'CIE 1931 2 Degree Standard Observer'
    d65_xy = colour.CCS_ILLUMINANTS[observer]['D65']
    d65_xyz = colour.xy_to_XYZ(d65_xy).tolist()
    d50_xy = colour.CCS_ILLUMINANTS[observer]['D50']
    d50_xyz = colour.xy_to_XYZ(d50_xy).tolist()

    return [
        [0.0, 0.0, 0.0],                      # Black
        [1.0, 1.0, 1.0],                      # White
        [0.5, 0.5, 0.5],                      # Mid-gray
        d65_xyz,                              # D65 white point (from colour-science)
        d50_xyz,                              # D50 white point (from colour-science)
        [0.412453, 0.212671, 0.019334],       # sRGB Red in XYZ
        [0.357580, 0.715160, 0.119193],       # sRGB Green in XYZ
        [0.180423, 0.072169, 0.950227],       # sRGB Blue in XYZ
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

# P7 Extended CAT test cases: (src_white, dst_white, method, filename_suffix)
# These are tested in test_35_cat_extended.c
CAT_EXTENDED_CASES = [
    ('d65', 'd50', 'Sharp', 'sharp'),
    ('d65', 'd50', 'Fairchild', 'fairchild'),
    ('d65', 'd50', 'CMCCAT97', 'cmccat97'),
    ('d65', 'd50', 'CMCCAT2000', 'cmccat2000'),
    ('d65', 'd50', 'CAT02 Brill 2008', 'cat02_brill_2008'),
    ('d65', 'd50', 'Bianco 2010', 'bianco_2010'),
    ('d65', 'd50', 'Bianco PC 2010', 'bianco_pc_2010'),
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
        save_matrix(cat_matrix, filepath, f"CAT {src_name}->{dst_name} ({method})")


def generate_adapted_colors(output_dir, white_points, test_output_dir=None):
    """Generate adapted color test fixtures."""

    print("\nGenerating adapted color fixtures...")

    # Get test colors from colour-science
    test_xyz_colors = get_test_xyz_colors()

    # Adaptation: D65 -> D50 (Bradford)
    adapted_d65_d50 = []
    for xyz in test_xyz_colors:
        adapted = colour.adaptation.chromatic_adaptation_VonKries(
            xyz, white_points['d65'], white_points['d50'], transform='Bradford'
        )
        adapted_d65_d50.extend(adapted)

    filepath = os.path.join(output_dir, 'fixtures', 'adapted_d65_to_d50_bradford.csv')
    save_vector(adapted_d65_d50, filepath, f"Colors adapted D65->D50 (Bradford) - {len(test_xyz_colors)} colors")

    # Also save to test directory if specified
    if test_output_dir:
        test_filepath = os.path.join(test_output_dir, 'adapted_d65_to_d50_bradford.csv')
        save_vector(adapted_d65_d50, test_filepath, f"Test: Colors adapted D65->D50 (Bradford) - {len(test_xyz_colors)} colors")

    # Adaptation: A -> D65 (Bradford)
    adapted_a_d65 = []
    for xyz in test_xyz_colors:
        adapted = colour.adaptation.chromatic_adaptation_VonKries(
            xyz, white_points['a'], white_points['d65'], transform='Bradford'
        )
        adapted_a_d65.extend(adapted)

    filepath = os.path.join(output_dir, 'fixtures', 'adapted_a_to_d65_bradford.csv')
    save_vector(adapted_a_d65, filepath, f"Colors adapted A->D65 (Bradford) - {len(test_xyz_colors)} colors")

    # Also save to test directory if specified
    if test_output_dir:
        test_filepath = os.path.join(test_output_dir, 'adapted_a_to_d65_bradford.csv')
        save_vector(adapted_a_d65, test_filepath, f"Test: Colors adapted A->D65 (Bradford) - {len(test_xyz_colors)} colors")


def generate_extended_cat_fixtures(output_dir, white_points, test_output_dir=None):
    """Generate P7 extended CAT matrices and adapted colors for test_35.

    Args:
        output_dir: Base output directory for library data (src/alwan/data)
        white_points: Dictionary of white point XYZ values
        test_output_dir: Optional separate directory for test reference values
    """

    print("\nGenerating P7 extended CAT fixtures...")

    # Get test colors
    test_xyz_colors = get_test_xyz_colors()

    for src_name, dst_name, method, file_suffix in CAT_EXTENDED_CASES:
        src_wp = white_points[src_name]
        dst_wp = white_points[dst_name]

        # Generate CAT matrix D65->D50
        cat_matrix = colour.adaptation.matrix_chromatic_adaptation_VonKries(
            src_wp, dst_wp, transform=method
        )

        # Save CAT matrix to fixtures (library data)
        matrix_filename = f'cat_{src_name}_to_{dst_name}_{file_suffix}.csv'
        matrix_filepath = os.path.join(output_dir, 'fixtures', matrix_filename)
        save_matrix(cat_matrix, matrix_filepath, f"CAT {src_name}->{dst_name} ({method})")

        # Generate adapted colors
        adapted_colors = []
        for xyz in test_xyz_colors:
            adapted = colour.adaptation.chromatic_adaptation_VonKries(
                xyz, src_wp, dst_wp, transform=method
            )
            adapted_colors.extend(adapted)

        # Save adapted colors to fixtures (library data)
        adapted_filename = f'adapted_{src_name}_to_{dst_name}_{file_suffix}.csv'
        adapted_filepath = os.path.join(output_dir, 'fixtures', adapted_filename)
        save_vector(adapted_colors, adapted_filepath,
                    f"Colors adapted {src_name}->{dst_name} ({method}) - {len(test_xyz_colors)} colors")

        # Also save to test reference values directory if specified
        if test_output_dir:
            test_matrix_filepath = os.path.join(test_output_dir, matrix_filename)
            save_matrix(cat_matrix, test_matrix_filepath, f"Test: CAT {src_name}->{dst_name} ({method})")

            test_adapted_filepath = os.path.join(test_output_dir, adapted_filename)
            save_vector(adapted_colors, test_adapted_filepath,
                        f"Test: Colors adapted {src_name}->{dst_name} ({method}) - {len(test_xyz_colors)} colors")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python chromatic_adaptation_fixtures.py <output_dir> [test_output_dir]")
        sys.exit(1)

    output_dir = sys.argv[1]
    test_output_dir = sys.argv[2] if len(sys.argv) > 2 else None

    white_points = generate_white_points(output_dir)
    generate_cat_matrices(output_dir, white_points)
    generate_adapted_colors(output_dir, white_points, test_output_dir)
    generate_extended_cat_fixtures(output_dir, white_points, test_output_dir)
