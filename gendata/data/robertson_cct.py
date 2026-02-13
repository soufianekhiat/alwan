"""
Generate Robertson 1968 isotemperature line table.
Source: colour.temperature.robertson1968.DATA_ISOTEMPERATURE_LINES_ROBERTSON1968

Table format: 31 entries, 4 values each (reciprocal_mrd, u, v, slope).
Used by: alwan_quality.c (alwan_cct_robertson_xy)
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import save_vector

try:
    import colour
    from colour.temperature.robertson1968 import DATA_ISOTEMPERATURE_LINES_ROBERTSON1968
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


def generate_robertson_cct_table(output_dir):
    """Generate Robertson 1968 isotemperature line table."""

    print("\nGenerating Robertson CCT lookup table...")

    # Extract the standard 31-entry Robertson table from colour-science
    # Each entry is (r, u, v, t) where:
    #   r = reciprocal mega-reciprocal degrees (MRD^-1)
    #   u, v = CIE 1960 UCS coordinates on Planckian locus
    #   t = slope of isotemperature line in (u, v) space
    table = DATA_ISOTEMPERATURE_LINES_ROBERTSON1968
    assert len(table) == 31, f"Expected 31 entries, got {len(table)}"

    # Flatten to [r0, u0, v0, t0, r1, u1, v1, t1, ...]
    flat = []
    for entry in table:
        flat.extend([float(entry[0]), float(entry[1]), float(entry[2]), float(entry[3])])

    filepath = os.path.join(output_dir, 'fixtures', 'robertson_cct_locus.csv')
    save_vector(flat, filepath,
                f"{len(table)} Robertson 1968 isotemperature lines (r, u, v, t)")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python robertson_cct.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_robertson_cct_table(output_dir)
