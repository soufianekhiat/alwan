"""
Generate Color Matching Functions (CMFs).
Source: colour.MSDS_CMFS
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import ensure_dir, save_vector

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


def generate_cmfs(output_dir):
    """Generate Color Matching Functions from colour-science."""

    print("\nGenerating Color Matching Functions (CMFs)...")

    # CMF datasets to generate (filename_prefix, colour-science name, wavelength range)
    cmf_datasets = {
        'cie_1931_2deg': ('CIE 1931 2 Degree Standard Observer', 360, 830, 1),
        'cie_1964_10deg': ('CIE 1964 10 Degree Standard Observer', 360, 830, 1),
        'cie_2012_2deg': ('CIE 2015 2 Degree Standard Observer', 360, 830, 1),
        'cie_2012_10deg': ('CIE 2015 10 Degree Standard Observer', 360, 830, 1),
        'cie_2015_2deg': ('CIE 2015 2 Degree Standard Observer', 360, 830, 1),  # Alias for cie_2012
        'cie_2015_10deg': ('CIE 2015 10 Degree Standard Observer', 360, 830, 1),  # Alias for cie_2012
        'stockman_sharpe_2deg': ('Stockman & Sharpe 2 Degree Cone Fundamentals', 360, 830, 1),
        'stockman_sharpe_10deg': ('Stockman & Sharpe 10 Degree Cone Fundamentals', 360, 830, 1),
        'wright_guild_1931': ('Wright & Guild 1931 2 Degree RGB CMFs', 360, 830, 1)
    }

    for filename_prefix, (cmf_name, wl_start, wl_end, wl_step) in cmf_datasets.items():
        try:
            cmfs = colour.MSDS_CMFS[cmf_name]

            # Resample to desired wavelength range
            wavelengths = np.arange(wl_start, wl_end + 1, wl_step)

            # Sample the CMF at each wavelength
            x_bar = [float(cmfs[wl][0]) if wl in cmfs.wavelengths else 0.0 for wl in wavelengths]
            y_bar = [float(cmfs[wl][1]) if wl in cmfs.wavelengths else 0.0 for wl in wavelengths]
            z_bar = [float(cmfs[wl][2]) if wl in cmfs.wavelengths else 0.0 for wl in wavelengths]

            # Generate filename with wavelength range
            wl_range = f"{wl_start}_{wl_end}_{wl_step}nm"

            # Save separate component files
            cmf_dir = os.path.join(output_dir, 'cmf')

            x_file = os.path.join(cmf_dir, f'{filename_prefix}_x_{wl_range}.csv')
            y_file = os.path.join(cmf_dir, f'{filename_prefix}_y_{wl_range}.csv')
            z_file = os.path.join(cmf_dir, f'{filename_prefix}_z_{wl_range}.csv')

            save_vector(x_bar, x_file, f"{cmf_name} X-bar")
            save_vector(y_bar, y_file, f"{cmf_name} Y-bar")
            save_vector(z_bar, z_file, f"{cmf_name} Z-bar")

            print(f"  {filename_prefix}: {len(wavelengths)} samples ({wl_start}-{wl_end}nm @ {wl_step}nm)")

        except KeyError:
            print(f"  WARNING: CMF '{cmf_name}' not found")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python cmf.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_cmfs(output_dir)
