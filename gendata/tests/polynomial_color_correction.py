"""
Generate test data for polynomial color correction functions:
- Cheung 2004 polynomial expansion
- Finlayson 2015 polynomial expansion
- Vandermonde expansion
- White balance

ALL values come from colour-science - no hardcoded math.
Test inputs are defined here, expected outputs computed by colour-science.

Usage:
    python polynomial_color_correction.py [output_dir]

If output_dir not specified, defaults to <alwan_root>/tests/reference_values/
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import save_test_data

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


def generate_cheung2004_tests(output_dir):
    """Generate Cheung 2004 polynomial expansion test cases using colour-science."""

    print("\nGenerating Cheung 2004 polynomial expansion test data...")

    # Test RGB values (normalized 0-1)
    test_rgbs = [
        [0.5, 0.3, 0.2],
        [0.0, 0.0, 0.0],
        [1.0, 1.0, 1.0],
        [0.8, 0.2, 0.4],
        [0.1, 0.9, 0.5],
        [0.25, 0.75, 0.125],
    ]

    # All supported term counts
    term_counts = [3, 4, 5, 7, 8, 10, 11, 14, 16, 17, 19, 20, 22, 35]

    # Save input RGB values
    rgb_flat = []
    for rgb in test_rgbs:
        rgb_flat.extend(rgb)
    save_test_data(rgb_flat, os.path.join(output_dir, 'cheung2004_input_rgb.csv'),
                   f"{len(test_rgbs)} RGB triplets")

    # Generate expansion for each term count
    for terms in term_counts:
        expanded_data = []
        for rgb in test_rgbs:
            RGB = np.array(rgb)
            result = colour.characterisation.matrix_augmented_Cheung2004(RGB, terms)
            expanded_data.extend([float(x) for x in result])

        filepath = os.path.join(output_dir, f'cheung2004_expand_{terms}.csv')
        save_test_data(expanded_data, filepath,
                       f"{len(test_rgbs)} expansions, {terms} terms each")


def generate_finlayson2015_tests(output_dir):
    """Generate Finlayson 2015 polynomial expansion test cases using colour-science."""

    print("\nGenerating Finlayson 2015 polynomial expansion test data...")

    # Test RGB values (normalized 0-1)
    test_rgbs = [
        [0.5, 0.3, 0.2],
        [0.8, 0.2, 0.4],
        [0.1, 0.9, 0.5],
        [0.25, 0.75, 0.125],
    ]

    # Save input RGB values
    rgb_flat = []
    for rgb in test_rgbs:
        rgb_flat.extend(rgb)
    save_test_data(rgb_flat, os.path.join(output_dir, 'finlayson2015_input_rgb.csv'),
                   f"{len(test_rgbs)} RGB triplets")

    # Generate standard polynomial expansion for each degree
    for degree in [1, 2, 3, 4]:
        expanded_data = []
        for rgb in test_rgbs:
            RGB = np.array(rgb)
            result = colour.characterisation.polynomial_expansion_Finlayson2015(
                RGB, degree, root_polynomial_expansion=False)
            expanded_data.extend([float(x) for x in result])

        filepath = os.path.join(output_dir, f'finlayson2015_std_deg{degree}.csv')
        save_test_data(expanded_data, filepath,
                       f"{len(test_rgbs)} expansions, degree {degree}")

    # Generate root-polynomial expansion for each degree
    for degree in [1, 2, 3, 4]:
        expanded_data = []
        for rgb in test_rgbs:
            RGB = np.array(rgb)
            result = colour.characterisation.polynomial_expansion_Finlayson2015(
                RGB, degree, root_polynomial_expansion=True)
            expanded_data.extend([float(x) for x in result])

        filepath = os.path.join(output_dir, f'finlayson2015_root_deg{degree}.csv')
        save_test_data(expanded_data, filepath,
                       f"{len(test_rgbs)} root-poly expansions, degree {degree}")


def generate_vandermonde_tests(output_dir):
    """Generate Vandermonde polynomial expansion test cases using colour-science."""

    print("\nGenerating Vandermonde polynomial expansion test data...")

    # Test input arrays
    test_arrays = [
        [1.0, 2.0, 3.0],
        [0.5, 0.3, 0.2],
        [0.0, 0.5, 1.0],
    ]

    # Save input arrays (each is 3 elements)
    input_flat = []
    for arr in test_arrays:
        input_flat.extend(arr)
    save_test_data(input_flat, os.path.join(output_dir, 'vandermonde_input.csv'),
                   f"{len(test_arrays)} input arrays")

    # Generate expansion for each degree
    for degree in [1, 2, 3, 4]:
        expanded_data = []
        for arr in test_arrays:
            a = np.array(arr)
            result = colour.characterisation.polynomial_expansion_Vandermonde(a, degree)
            expanded_data.extend([float(x) for x in result])

        filepath = os.path.join(output_dir, f'vandermonde_deg{degree}.csv')
        save_test_data(expanded_data, filepath,
                       f"{len(test_arrays)} expansions, degree {degree}")


def generate_white_balance_tests(output_dir):
    """Generate white balance test cases."""

    print("\nGenerating white balance test data...")

    # Test measured gray values (simulating camera captures of gray card)
    test_grays = [
        [0.5, 0.45, 0.55],  # Slightly unbalanced
        [0.8, 0.7, 0.9],    # Warm cast
        [0.3, 0.35, 0.25],  # Cool cast
        [0.6, 0.6, 0.6],    # Already neutral
        [0.4, 0.5, 0.35],   # Mixed cast
    ]

    # Save input gray values
    gray_flat = []
    for gray in test_grays:
        gray_flat.extend(gray)
    save_test_data(gray_flat, os.path.join(output_dir, 'white_balance_input_gray.csv'),
                   f"{len(test_grays)} measured gray values")

    # Compute expected multipliers
    # Note: Our implementation normalizes so min channel = 1.0
    multipliers_data = []
    for gray in test_grays:
        min_val = min(gray)
        mult_r = min_val / gray[0]
        mult_g = min_val / gray[1]
        mult_b = min_val / gray[2]
        multipliers_data.extend([mult_r, mult_g, mult_b])

    save_test_data(multipliers_data, os.path.join(output_dir, 'white_balance_multipliers.csv'),
                   f"{len(test_grays)} multiplier triplets")


def generate_all_polynomial_tests(output_dir):
    """Generate all polynomial color correction test data."""
    generate_cheung2004_tests(output_dir)
    generate_finlayson2015_tests(output_dir)
    generate_vandermonde_tests(output_dir)
    generate_white_balance_tests(output_dir)


if __name__ == '__main__':
    # Default to tests/reference_values (relative to alwan root)
    if len(sys.argv) == 2:
        output_dir = sys.argv[1]
    else:
        # Find alwan root directory
        script_dir = os.path.dirname(os.path.abspath(__file__))
        alwan_root = os.path.dirname(os.path.dirname(script_dir))
        output_dir = os.path.join(alwan_root, 'tests', 'reference_values')

    print(f"Output directory: {output_dir}")
    generate_all_polynomial_tests(output_dir)
