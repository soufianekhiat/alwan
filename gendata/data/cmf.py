"""
Generate Color Matching Functions (CMFs).
Source: colour.MSDS_CMFS
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import ensure_dir, format_scalar

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


def generate_cmfs(output_dir):
    """Generate Color Matching Functions from colour-science."""

    print("\nGenerating Color Matching Functions (CMFs)...")

    # CMF datasets to generate
    cmf_datasets = {
        'cie_1931_2deg': 'CIE 1931 2 Degree Standard Observer',
        'cie_1964_10deg': 'CIE 1964 10 Degree Standard Observer',
        'cie_2015_2deg': 'CIE 2012 2 Degree Standard Observer',
        'cie_2015_10deg': 'CIE 2012 10 Degree Standard Observer'
    }

    for filename, cmf_name in cmf_datasets.items():
        try:
            cmfs = colour.MSDS_CMFS[cmf_name]

            # Extract wavelengths and tristimulus values
            wavelengths = cmfs.wavelengths
            x_bar = cmfs.values[:, 0]
            y_bar = cmfs.values[:, 1]
            z_bar = cmfs.values[:, 2]

            # Save CMF data
            filepath = os.path.join(output_dir, 'cmf', f'{filename}.csv')
            ensure_dir(filepath)

            with open(filepath, 'w', newline='') as f:
                # Format: wavelength,x_bar,y_bar,z_bar (one row per wavelength)
                for wl, x, y, z in zip(wavelengths, x_bar, y_bar, z_bar):
                    values = [format_scalar(wl), format_scalar(x), format_scalar(y), format_scalar(z)]
                    f.write(','.join(values) + '\n')

            print(f"  {filepath} ({len(wavelengths)} samples, {wavelengths[0]}-{wavelengths[-1]}nm)")

        except KeyError:
            print(f"  WARNING: CMF '{cmf_name}' not found")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python cmf.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_cmfs(output_dir)
