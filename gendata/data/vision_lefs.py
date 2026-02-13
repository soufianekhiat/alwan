"""
Generate Luminous Efficiency Function (LEF) data for vision module.
Source: colour.SDS_LEFS

Generates:
  - photopic_v_lambda.csv   (CIE 1924 V(lambda) -wavelength,value pairs)
  - scotopic_vp_lambda.csv  (CIE 1951 V'(lambda) -wavelength,value pairs)

Both sampled at 10 nm from 380–780 nm, with the peak wavelength
inserted (555 nm photopic, 507 nm scotopic) to preserve the exact
maximum.
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


def _regular_wavelengths_with_peak(start, end, step, peak_nm):
    """Return sorted wavelength array: regular grid + peak wavelength."""
    wl = np.arange(start, end + step, step, dtype=np.float64)
    if peak_nm not in wl:
        wl = np.sort(np.append(wl, peak_nm))
    return wl


def _save_lef(spd, wavelengths, filepath, description):
    """Interpolate LEF at given wavelengths and save as interleaved CSV."""
    ensure_dir(filepath)

    values = spd[wavelengths]

    with open(filepath, 'w', newline='') as f:
        pairs = []
        for wl, val in zip(wavelengths, values):
            pairs.append(format_scalar(wl))
            pairs.append(format_scalar(val))
        f.write(','.join(pairs) + ',\n')

    print(f"  {filepath} ({len(wavelengths)} samples, "
          f"{wavelengths[0]:.0f}-{wavelengths[-1]:.0f} nm) -{description}")


def generate_vision_lefs(output_dir):
    """Generate luminous efficiency function CSVs from colour-science."""

    print("\nGenerating Luminous Efficiency Functions...")

    vision_dir = os.path.join(output_dir, 'vision')

    # -- Photopic V(lambda) -CIE 1924 --
    photopic = colour.SDS_LEFS['CIE 1924 Photopic Standard Observer']
    wl_photopic = _regular_wavelengths_with_peak(380, 780, 10, 555)
    _save_lef(photopic, wl_photopic,
              os.path.join(vision_dir, 'photopic_v_lambda.csv'),
              "CIE 1924 V(lambda)")

    # -- Scotopic V'(lambda) -CIE 1951 --
    scotopic = colour.SDS_LEFS['CIE 1951 Scotopic Standard Observer']
    wl_scotopic = _regular_wavelengths_with_peak(380, 780, 10, 507)
    _save_lef(scotopic, wl_scotopic,
              os.path.join(vision_dir, 'scotopic_vp_lambda.csv'),
              "CIE 1951 V'(lambda)")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python vision_lefs.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_vision_lefs(output_dir)
