"""
Generate test data for:
- Zhai 2018 two-step chromatic adaptation (alwan_cat_zhai2018)
- CAM02-UCS Delta E (alwan_delta_e_cam02_ucs)
- CAM16-UCS Delta E (alwan_delta_e_cam16_ucs)

ALL values come from colour-science - no hardcoded math.
Test inputs are defined here, expected outputs computed by colour-science.

Usage:
    python zhai2018_cam_ucs_delta_e.py [output_dir]

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


def generate_zhai2018_tests(output_dir):
    """Generate Zhai 2018 two-step CAT test cases using colour-science."""

    print("\nGenerating Zhai 2018 chromatic adaptation test data...")

    # Test cases: XYZ_b (sample), XYZ_wb (source white), XYZ_wd (dest white), D_b, D_d
    # Using Y=100 scale as per colour-science convention
    test_cases = [
        # Standard case from colour-science docs
        {
            'XYZ_b': np.array([48.900, 43.620, 6.250]),
            'XYZ_wb': np.array([109.850, 100.0, 35.585]),  # Illuminant A
            'XYZ_wd': np.array([95.047, 100.0, 108.883]),  # D65
            'D_b': 0.9407,
            'D_d': 0.9800,
            'XYZ_wo': np.array([100.0, 100.0, 100.0]),
            'transform': 'CAT02'
        },
        # Full adaptation both ways
        {
            'XYZ_b': np.array([50.0, 50.0, 50.0]),
            'XYZ_wb': np.array([95.047, 100.0, 108.883]),  # D65
            'XYZ_wd': np.array([96.422, 100.0, 82.521]),   # D50
            'D_b': 1.0,
            'D_d': 1.0,
            'XYZ_wo': np.array([100.0, 100.0, 100.0]),
            'transform': 'CAT02'
        },
        # Partial adaptation
        {
            'XYZ_b': np.array([41.24, 21.26, 1.93]),  # Red
            'XYZ_wb': np.array([95.047, 100.0, 108.883]),
            'XYZ_wd': np.array([109.850, 100.0, 35.585]),
            'D_b': 0.7,
            'D_d': 0.85,
            'XYZ_wo': np.array([100.0, 100.0, 100.0]),
            'transform': 'CAT02'
        },
        # Using CAT16 transform
        {
            'XYZ_b': np.array([35.76, 71.52, 11.92]),  # Green
            'XYZ_wb': np.array([95.047, 100.0, 108.883]),
            'XYZ_wd': np.array([96.422, 100.0, 82.521]),
            'D_b': 1.0,
            'D_d': 1.0,
            'XYZ_wo': np.array([100.0, 100.0, 100.0]),
            'transform': 'CAT16'
        },
    ]

    zhai2018_data = []

    for case in test_cases:
        # Compute adapted XYZ using colour-science
        XYZ_d = colour.adaptation.chromatic_adaptation_Zhai2018(
            case['XYZ_b'],
            case['XYZ_wb'],
            case['XYZ_wd'],
            D_b=case['D_b'],
            D_d=case['D_d'],
            XYZ_wo=case['XYZ_wo'],
            transform=case['transform']
        )

        # Transform code: 0=CAT02, 1=CAT16
        transform_code = 0 if case['transform'] == 'CAT02' else 1

        # Format: XYZ_b(3), XYZ_wb(3), XYZ_wd(3), D_b, D_d, XYZ_wo(3), transform, XYZ_d(3) = 17 values
        zhai2018_data.extend(case['XYZ_b'].tolist())
        zhai2018_data.extend(case['XYZ_wb'].tolist())
        zhai2018_data.extend(case['XYZ_wd'].tolist())
        zhai2018_data.append(float(case['D_b']))
        zhai2018_data.append(float(case['D_d']))
        zhai2018_data.extend(case['XYZ_wo'].tolist())
        zhai2018_data.append(float(transform_code))
        zhai2018_data.extend(XYZ_d.tolist())

    filepath = os.path.join(output_dir, 'zhai2018.csv')
    save_test_data(zhai2018_data, filepath, f"{len(test_cases)} test cases, 17 values each")


def generate_cam_ucs_delta_e_tests(output_dir):
    """Generate CAM02-UCS and CAM16-UCS Delta E test cases using colour-science."""

    print("\nGenerating CAM02-UCS and CAM16-UCS Delta E test data...")

    # Test J'a'b' pairs (already in UCS space, scaled 0-100)
    # These are typical CAM UCS coordinates
    test_pairs = [
        # Near-identical colors
        (np.array([54.90433134, -0.08450395, -0.06854831]),
         np.array([54.80352754, -3.96940084, -13.57591013])),
        # Complementary colors
        (np.array([50.0, 20.0, 10.0]),
         np.array([50.0, -20.0, -10.0])),
        # Same lightness, different chroma
        (np.array([70.0, 30.0, 0.0]),
         np.array([70.0, 0.0, 30.0])),
        # Different lightness, same chroma
        (np.array([30.0, 15.0, 15.0]),
         np.array([80.0, 15.0, 15.0])),
        # Near-neutral
        (np.array([50.0, 0.5, 0.5]),
         np.array([50.0, -0.5, -0.5])),
        # High chroma
        (np.array([60.0, 50.0, 30.0]),
         np.array([60.0, 30.0, 50.0])),
    ]

    # Extract Jab1 and Jab2 arrays separately (matching existing test file pattern)
    jab1_data = []
    jab2_data = []
    cam02_ucs_de = []
    cam16_ucs_de = []

    for jab1, jab2 in test_pairs:
        jab1_data.extend(jab1.tolist())
        jab2_data.extend(jab2.tolist())
        cam02_ucs_de.append(float(colour.difference.delta_E_CAM02UCS(jab1, jab2)))
        cam16_ucs_de.append(float(colour.difference.delta_E_CAM16UCS(jab1, jab2)))

    # Save separate files for inputs (matching existing pattern in 30_delta_e_extended.c)
    save_test_data(jab1_data, os.path.join(output_dir, 'delta_e_cam_ucs_jab1.csv'),
                   f"{len(test_pairs)} Jab1 values (3 per color)")
    save_test_data(jab2_data, os.path.join(output_dir, 'delta_e_cam_ucs_jab2.csv'),
                   f"{len(test_pairs)} Jab2 values (3 per color)")
    save_test_data(cam02_ucs_de, os.path.join(output_dir, 'delta_e_cam02_ucs.csv'),
                   f"{len(test_pairs)} CAM02-UCS Delta E values")
    save_test_data(cam16_ucs_de, os.path.join(output_dir, 'delta_e_cam16_ucs.csv'),
                   f"{len(test_pairs)} CAM16-UCS Delta E values")


def generate_all_new_features(output_dir):
    """Generate all new feature test data."""
    generate_zhai2018_tests(output_dir)
    generate_cam_ucs_delta_e_tests(output_dir)
    # NOTE: HyCH skipped - not available in colour-science 0.4.6


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
    generate_all_new_features(output_dir)
