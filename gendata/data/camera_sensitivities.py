"""
Generate Camera Sensitivity Functions.
Source: colour.MSDS_CAMERA_SENSITIVITIES
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


def generate_camera_sensitivities(output_dir):
    """Generate Camera Sensitivity Functions from colour-science."""

    print("\nGenerating Camera Sensitivities...")

    # Camera datasets to generate (filename_prefix, colour-science name, expected wavelength range)
    camera_datasets = {
        'nikon_5100': ('Nikon 5100 (NPL)', 380, 780, 5),
        'sigma_sdmerill': ('Sigma SDMerill (NPL)', 380, 780, 5),
    }

    cameras_dir = os.path.join(output_dir, 'camera_sensitivities')
    ensure_dir(cameras_dir)

    for filename_prefix, (camera_name, wl_start, wl_end, wl_step) in camera_datasets.items():
        try:
            sensitivities = colour.MSDS_CAMERA_SENSITIVITIES[camera_name]

            # Expected wavelength range
            wavelengths = np.arange(wl_start, wl_end + 1, wl_step)

            # Sample the camera sensitivities at each wavelength
            # sensitivities.values has shape (n_wavelengths, 3) for [red, green, blue]
            red = []
            green = []
            blue = []

            for wl in wavelengths:
                if wl in sensitivities.wavelengths:
                    idx = list(sensitivities.wavelengths).index(wl)
                    red.append(float(sensitivities.values[idx, 0]))
                    green.append(float(sensitivities.values[idx, 1]))
                    blue.append(float(sensitivities.values[idx, 2]))
                else:
                    # If wavelength not in data, use 0
                    red.append(0.0)
                    green.append(0.0)
                    blue.append(0.0)

            # Save separate component files
            r_file = os.path.join(cameras_dir, f'{filename_prefix}_r.csv')
            g_file = os.path.join(cameras_dir, f'{filename_prefix}_g.csv')
            b_file = os.path.join(cameras_dir, f'{filename_prefix}_b.csv')

            save_vector(red, r_file, f"{camera_name} Red sensitivity")
            save_vector(green, g_file, f"{camera_name} Green sensitivity")
            save_vector(blue, b_file, f"{camera_name} Blue sensitivity")

            print(f"  {filename_prefix}: {len(wavelengths)} samples ({wl_start}-{wl_end}nm @ {wl_step}nm)")

        except KeyError:
            print(f"  WARNING: Camera '{camera_name}' not found in colour-science")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python camera_sensitivities.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_camera_sensitivities(output_dir)
