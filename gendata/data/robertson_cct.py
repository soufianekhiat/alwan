"""
Generate Robertson CCT lookup table (Planckian locus in CIE 1960 UCS).
Source: colour.CCT_to_xy()

This table is used by the Robertson method for accurate CCT estimation.
CCT values (inputs) are hardcoded configuration.
Planckian locus coordinates are computed from colour-science.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import format_scalar, ensure_dir

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


def generate_robertson_cct_table(output_dir):
    """Generate Robertson CCT lookup table."""

    print("\nGenerating Robertson CCT lookup table...")

    # CCT values for lookup table (Kelvin) - CONFIGURATION (inputs)
    robertson_ccts = [
        1000, 1500, 2000, 2500, 3000, 3500, 4000, 4500, 5000, 5500,
        6000, 6500, 7000, 7500, 8000, 8500, 9000, 9500, 10000,
        12000, 15000, 20000
    ]

    # Compute Planckian locus points in CIE 1960 UCS with slopes
    robertson_table = []
    for i, cct in enumerate(robertson_ccts):
        # Get xy for blackbody at this CCT from colour-science (NO HARDCODING)
        xy = colour.CCT_to_xy(cct)

        # Convert CIE 1931 xy to CIE 1960 UCS uv
        u = 4 * xy[0] / (12 * xy[1] - 2 * xy[0] + 3)
        v = 6 * xy[1] / (12 * xy[1] - 2 * xy[0] + 3)

        # Compute slopes (du/dT and dv/dT) for interpolation
        # Use finite differences with neighboring points
        if i == 0:
            # Forward difference for first point
            xy_next = colour.CCT_to_xy(robertson_ccts[i+1])
            u_next = 4 * xy_next[0] / (12 * xy_next[1] - 2 * xy_next[0] + 3)
            v_next = 6 * xy_next[1] / (12 * xy_next[1] - 2 * xy_next[0] + 3)
            du = (u_next - u) / (robertson_ccts[i+1] - cct)
            dv = (v_next - v) / (robertson_ccts[i+1] - cct)
        elif i == len(robertson_ccts) - 1:
            # Backward difference for last point
            xy_prev = colour.CCT_to_xy(robertson_ccts[i-1])
            u_prev = 4 * xy_prev[0] / (12 * xy_prev[1] - 2 * xy_prev[0] + 3)
            v_prev = 6 * xy_prev[1] / (12 * xy_prev[1] - 2 * xy_prev[0] + 3)
            du = (u - u_prev) / (cct - robertson_ccts[i-1])
            dv = (v - v_prev) / (cct - robertson_ccts[i-1])
        else:
            # Central difference for middle points
            xy_prev = colour.CCT_to_xy(robertson_ccts[i-1])
            xy_next = colour.CCT_to_xy(robertson_ccts[i+1])
            u_prev = 4 * xy_prev[0] / (12 * xy_prev[1] - 2 * xy_prev[0] + 3)
            v_prev = 6 * xy_prev[1] / (12 * xy_prev[1] - 2 * xy_prev[0] + 3)
            u_next = 4 * xy_next[0] / (12 * xy_next[1] - 2 * xy_next[0] + 3)
            v_next = 6 * xy_next[1] / (12 * xy_next[1] - 2 * xy_next[0] + 3)
            du = (u_next - u_prev) / (robertson_ccts[i+1] - robertson_ccts[i-1])
            dv = (v_next - v_prev) / (robertson_ccts[i+1] - robertson_ccts[i-1])

        robertson_table.append([cct, u, v, du, dv])

    # Write Robertson table as single-line CSV for C embedding
    # Format: cct1,u1,v1,du1,dv1,cct2,u2,v2,du2,dv2,...
    # Use fixed-point notation to avoid scientific notation issues in C compilation
    filepath = os.path.join(output_dir, 'fixtures', 'robertson_cct_locus.csv')
    ensure_dir(filepath)

    with open(filepath, 'w', newline='') as f:
        all_values = []
        for row in robertson_table:
            # Format with enough precision, avoiding scientific notation
            formatted_values = [f'{v:.17f}' if abs(v) < 1.0 else format_scalar(v) for v in row]
            all_values.extend(formatted_values)
        f.write(','.join(all_values) + '\n')

    print(f"  {filepath} ({len(robertson_table)} CCT points, {len(all_values)} values)")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python robertson_cct.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_robertson_cct_table(output_dir)
