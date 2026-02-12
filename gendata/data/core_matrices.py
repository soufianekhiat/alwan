"""
Generate core color space matrices for Alwan.
Source: colour-science

Generates:
- data/matrices/oklab_m1.csv          - Oklab XYZ to LMS (M1)
- data/matrices/oklab_m2.csv          - Oklab LMS' to Lab (M2)
- data/matrices/oklab_m1_inv.csv      - Oklab LMS to XYZ (M1 inverse)
- data/matrices/oklab_m2_inv.csv      - Oklab Lab to LMS' (M2 inverse)
- data/matrices/jzazbz_xyz_to_lms.csv          - Jzazbz XYZ to LMS
- data/matrices/jzazbz_lms_to_xyz.csv          - Jzazbz LMS to XYZ
- data/matrices/jzazbz_lms_p_to_izazbz.csv     - Jzazbz LMS' to Izazbz
- data/matrices/jzazbz_izazbz_to_lms_p.csv     - Jzazbz Izazbz to LMS'
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import save_matrix

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


def generate_core_matrices(output_dir):
    """Generate Oklab and Jzazbz matrices."""

    print("\nGenerating Oklab matrices...")

    # Oklab M1: XYZ (D65) to LMS cone response
    from colour.models.oklab import MATRIX_1_XYZ_TO_LMS
    oklab_m1 = MATRIX_1_XYZ_TO_LMS
    filepath = os.path.join(output_dir, 'matrices', 'oklab_m1.csv')
    save_matrix(oklab_m1, filepath, "Oklab M1: XYZ (D65) to LMS")

    # Oklab M2: LMS' to Lab
    from colour.models.oklab import MATRIX_2_LMS_TO_LAB
    oklab_m2 = MATRIX_2_LMS_TO_LAB
    filepath = os.path.join(output_dir, 'matrices', 'oklab_m2.csv')
    save_matrix(oklab_m2, filepath, "Oklab M2: LMS' to Lab")

    # Oklab M1 inverse: LMS to XYZ
    oklab_m1_inv = np.linalg.inv(oklab_m1)
    filepath = os.path.join(output_dir, 'matrices', 'oklab_m1_inv.csv')
    save_matrix(oklab_m1_inv, filepath, "Oklab M1 inverse: LMS to XYZ")

    # Oklab M2 inverse: Lab to LMS'
    oklab_m2_inv = np.linalg.inv(oklab_m2)
    filepath = os.path.join(output_dir, 'matrices', 'oklab_m2_inv.csv')
    save_matrix(oklab_m2_inv, filepath, "Oklab M2 inverse: Lab to LMS'")

    print("\nGenerating Jzazbz matrices...")

    # Jzazbz XYZ to LMS
    from colour.models.jzazbz import MATRIX_JZAZBZ_XYZ_TO_LMS
    jzazbz_xyz_to_lms = MATRIX_JZAZBZ_XYZ_TO_LMS
    filepath = os.path.join(output_dir, 'matrices', 'jzazbz_xyz_to_lms.csv')
    save_matrix(jzazbz_xyz_to_lms, filepath, "Jzazbz XYZ to LMS (Safdar 2017)")

    # Jzazbz LMS to XYZ (inverse)
    jzazbz_lms_to_xyz = np.linalg.inv(jzazbz_xyz_to_lms)
    filepath = os.path.join(output_dir, 'matrices', 'jzazbz_lms_to_xyz.csv')
    save_matrix(jzazbz_lms_to_xyz, filepath, "Jzazbz LMS to XYZ (inverse)")

    # Jzazbz LMS' to Izazbz
    from colour.models.jzazbz import MATRIX_JZAZBZ_LMS_P_TO_IZAZBZ_SAFDAR2017
    jzazbz_lms_p_to_izazbz = MATRIX_JZAZBZ_LMS_P_TO_IZAZBZ_SAFDAR2017
    filepath = os.path.join(output_dir, 'matrices', 'jzazbz_lms_p_to_izazbz.csv')
    save_matrix(jzazbz_lms_p_to_izazbz, filepath, "Jzazbz LMS' to Izazbz (Safdar 2017)")

    # Jzazbz Izazbz to LMS' (inverse)
    jzazbz_izazbz_to_lms_p = np.linalg.inv(jzazbz_lms_p_to_izazbz)
    filepath = os.path.join(output_dir, 'matrices', 'jzazbz_izazbz_to_lms_p.csv')
    save_matrix(jzazbz_izazbz_to_lms_p, filepath, "Jzazbz Izazbz to LMS' (inverse)")

    print("\nDone generating core matrices.")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python core_matrices.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_core_matrices(output_dir)
