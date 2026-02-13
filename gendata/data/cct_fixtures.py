"""
Generate CCT (Correlated Color Temperature) test fixtures.
Source: colour.xy_to_CCT()

Test xy coordinates are hardcoded (inputs).
CCT values are computed from colour-science.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import save_vector, format_scalar

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


def generate_cct_fixtures(output_dir):
    """Generate CCT test fixtures."""

    print("\nGenerating CCT and light quality test fixtures...")

    # CCT test cases: xy coordinates (INPUTS - hardcoded)
    # CCT values will be computed from colour-science
    cct_test_cases = [
        (0.31271, 0.32902),  # D65
        (0.34570, 0.35850),  # D50
        (0.33242, 0.34743),  # D55
        (0.32168, 0.33767),  # D60 (approximate)
        (0.44757, 0.40745),  # Illuminant A
        (0.25, 0.25),        # Bluish
        (0.40, 0.40),        # Warm white
    ]

    # Compute CCT using colour-science (NO HARDCODING)
    cct_results = []
    for x, y in cct_test_cases:
        xy = np.array([x, y])
        try:
            # Use colour-science CCT calculation with McCamy method
            cct_calc = colour.xy_to_CCT(xy, method='McCamy 1992')
            cct_results.extend([x, y, cct_calc])
        except Exception as e:
            print(f"  WARNING: Failed to compute CCT for ({x}, {y}): {e}")
            # Skip this test case if colour-science fails
            continue

    # Save CCT test cases
    filepath = os.path.join(output_dir, 'fixtures', 'cct_test_cases.csv')
    save_vector(cct_results, filepath, f"{len(cct_results)//3} CCT test cases (x, y, CCT)")

    # Robertson CCT test cases (for tests/17_quality_cct.c)
    # Uses Robertson 1968 method via colour.temperature.uv_to_CCT_Robertson1968
    robertson_cases = [
        (0.31271, 0.32902),  # D65 (~6504 K)
        (0.34570, 0.35850),  # D50 (~5003 K)
        (0.33242, 0.34743),  # D55 (~5503 K)
        (0.32168, 0.33767),  # D60 (~6000 K)
        (0.44757, 0.40745),  # Illuminant A (~2856 K)
        (0.37208, 0.37529),  # D40 (~4003 K)
        (0.29902, 0.31485),  # D75 (~7504 K)
    ]
    robertson_results = []
    for x, y in robertson_cases:
        # Convert xy to CIE 1960 UCS uv for Robertson's method
        u = 4 * x / (-2 * x + 12 * y + 3)
        v = 6 * y / (-2 * x + 12 * y + 3)
        uv = np.array([u, v])
        cct_rob = colour.temperature.uv_to_CCT_Robertson1968(uv)
        robertson_results.extend([x, y, float(cct_rob[0])])

    filepath = os.path.join(output_dir, 'fixtures', 'cct_robertson_test_cases.csv')
    save_vector(robertson_results, filepath,
                f"{len(robertson_results)//3} Robertson CCT test cases (x, y, CCT)")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python cct_fixtures.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_cct_fixtures(output_dir)
