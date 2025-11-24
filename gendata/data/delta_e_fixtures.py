"""
Generate ΔE (Delta-E) metric test fixtures.
Source: colour.delta_E functions

Test Lab pairs are hardcoded (inputs).
Expected ΔE values are computed from colour-science.
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


# ΔE test pairs (inputs - hardcoded as per requirements)
DELTA_E_TEST_PAIRS = [
    # Pair 1: Small difference
    ([50.0, 2.5, -1.0], [50.0, 0.0, -1.0]),
    # Pair 2: Moderate difference
    ([50.0, 2.5, -1.0], [60.0, 5.0, 3.0]),
    # Pair 3: Large difference
    ([50.0, 2.5, -1.0], [80.0, 30.0, 20.0]),
    # Pair 4: Hue shift
    ([50.0, 10.0, 0.0], [50.0, 0.0, 10.0]),
]


def generate_delta_e_fixtures(output_dir):
    """Generate ΔE metric test fixtures."""

    print("\nGenerating ΔE metric test fixtures...")

    # Extract Lab1 and Lab2 pairs
    lab1_values = []
    lab2_values = []

    for lab1, lab2 in DELTA_E_TEST_PAIRS:
        lab1_values.extend(lab1)
        lab2_values.extend(lab2)

    # Save Lab1 values
    filepath = os.path.join(output_dir, 'fixtures', 'delta_e_lab1.csv')
    save_vector(lab1_values, filepath, f"{len(DELTA_E_TEST_PAIRS)} Lab1 values")

    # Save Lab2 values
    filepath = os.path.join(output_dir, 'fixtures', 'delta_e_lab2.csv')
    save_vector(lab2_values, filepath, f"{len(DELTA_E_TEST_PAIRS)} Lab2 values")

    # Compute and save ΔE values for each metric
    metrics = {
        'delta_e_76': lambda l1, l2: colour.delta_E_CIE1976(l1, l2),
        'delta_e_94': lambda l1, l2: colour.delta_E_CIE1994(l1, l2),
        'delta_e_cmc_2_1': lambda l1, l2: colour.delta_E_CMC(l1, l2, l=2, c=1),
        'delta_e_00': lambda l1, l2: colour.delta_E_CIE2000(l1, l2),
    }

    for filename, func in metrics.items():
        delta_e_values = []
        for lab1, lab2 in DELTA_E_TEST_PAIRS:
            de = func(np.array(lab1), np.array(lab2))
            delta_e_values.append(de)

        filepath = os.path.join(output_dir, 'fixtures', f'{filename}.csv')
        save_vector(delta_e_values, filepath, f"ΔE values for {filename}")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python delta_e_fixtures.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_delta_e_fixtures(output_dir)
