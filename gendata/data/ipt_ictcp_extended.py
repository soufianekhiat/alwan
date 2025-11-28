"""
Generate extended IPT and ICtCp transformation matrices.
Includes LMS transformations, PQ/HLG encoding matrices, and BT.2020 conversions.
"""

import sys
import os
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
from common import save_matrix, save_vector

try:
    import colour
    from colour.models.ipt import (
        MATRIX_IPT_XYZ_TO_LMS, MATRIX_IPT_LMS_TO_XYZ,
        MATRIX_IPT_LMS_P_TO_IPT, MATRIX_IPT_IPT_TO_LMS_P
    )
    from colour.models.rgb.ictcp import (
        MATRIX_ICTCP_RGB_TO_LMS, MATRIX_ICTCP_LMS_TO_RGB,
        MATRIX_ICTCP_LMS_P_TO_ICTCP, MATRIX_ICTCP_ICTCP_TO_LMS_P,
        MATRIX_ICTCP_LMS_P_TO_ICTCP_BT2100_HLG_2, MATRIX_ICTCP_ICTCP_TO_LMS_P_BT2100_HLG_2
    )
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)

def generate_ipt_ictcp_extended(output_dir):
    """Generate extended IPT and ICtCp matrices."""

    print("\nGenerating Extended IPT/ICtCp Matrices")
    print("=" * 50)

    os.makedirs(os.path.join(output_dir, 'matrices'), exist_ok=True)
    os.makedirs(output_dir, exist_ok=True)

    # IPT Matrices
    print("\n[1/10] IPT XYZ <-> LMS transformations...")
    xyz_to_lms_ipt = MATRIX_IPT_XYZ_TO_LMS
    lms_to_xyz_ipt = MATRIX_IPT_LMS_TO_XYZ

    save_matrix(lms_to_xyz_ipt, os.path.join(output_dir, 'ipt_lms_to_xyz.csv'), "IPT LMS to XYZ")
    save_matrix(xyz_to_lms_ipt, os.path.join(output_dir, 'matrices', 'xyz_to_lms_ipt.csv'), "XYZ to LMS for IPT")
    save_matrix(lms_to_xyz_ipt, os.path.join(output_dir, 'matrices', 'lms_to_xyz_ipt.csv'), "LMS to XYZ for IPT")
    print("  [OK] IPT XYZ<->LMS matrices generated")

    # IPT LMS' to IPT transformation (after power function)
    print("\n[2/10] IPT LMS' <-> IPT transformations...")
    lms_p_to_ipt = MATRIX_IPT_LMS_P_TO_IPT
    ipt_to_lms_p = MATRIX_IPT_IPT_TO_LMS_P

    save_matrix(lms_p_to_ipt, os.path.join(output_dir, 'ipt_lms_p_to_ipt.csv'), "IPT LMS' to IPT")
    save_matrix(ipt_to_lms_p, os.path.join(output_dir, 'ipt_ipt_to_lms_p.csv'), "IPT to LMS'")
    print("  [OK] IPT LMS'<->IPT matrices generated")

    # ICtCp PQ encoding matrices
    print("\n[3/10] ICtCp PQ encoding matrices...")
    lms_p_to_ictcp_pq = MATRIX_ICTCP_LMS_P_TO_ICTCP
    ictcp_to_lms_p_pq = MATRIX_ICTCP_ICTCP_TO_LMS_P

    save_matrix(lms_p_to_ictcp_pq, os.path.join(output_dir, 'ictcp_lms_p_to_ictcp_pq.csv'), "ICtCp LMS' to ICtCp (PQ)")
    save_matrix(ictcp_to_lms_p_pq, os.path.join(output_dir, 'ictcp_ictcp_to_lms_p_pq.csv'), "ICtCp to LMS' (PQ)")
    print("  [OK] ICtCp PQ matrices generated")

    # ICtCp HLG encoding matrices
    print("\n[4/10] ICtCp HLG encoding matrices...")
    lms_p_to_ictcp_hlg = MATRIX_ICTCP_LMS_P_TO_ICTCP_BT2100_HLG_2
    ictcp_to_lms_p_hlg = MATRIX_ICTCP_ICTCP_TO_LMS_P_BT2100_HLG_2

    save_matrix(lms_p_to_ictcp_hlg, os.path.join(output_dir, 'ictcp_lms_p_to_ictcp_hlg.csv'), "ICtCp LMS' to ICtCp (HLG)")
    save_matrix(ictcp_to_lms_p_hlg, os.path.join(output_dir, 'ictcp_ictcp_to_lms_p_hlg.csv'), "ICtCp to LMS' (HLG)")
    print("  [OK] ICtCp HLG matrices generated")

    # ICtCp BT.2020 conversions
    print("\n[5/10] ICtCp BT.2020 <-> XYZ conversions...")
    bt2020_space = colour.RGB_COLOURSPACES['ITU-R BT.2020']
    xyz_to_bt2020 = bt2020_space.matrix_XYZ_to_RGB
    bt2020_to_xyz = bt2020_space.matrix_RGB_to_XYZ

    save_matrix(xyz_to_bt2020, os.path.join(output_dir, 'ictcp_xyz_to_bt2020.csv'), "XYZ to BT.2020")
    save_matrix(bt2020_to_xyz, os.path.join(output_dir, 'ictcp_bt2020_to_xyz.csv'), "BT.2020 to XYZ")
    print("  [OK] BT.2020 conversion matrices generated")

    print("\n" + "=" * 50)
    print("Extended IPT/ICtCp matrices generated successfully!")
    print("  - 10 CSV files created")
    print("=" * 50)

    return True

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python ipt_ictcp_extended.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    success = generate_ipt_ictcp_extended(output_dir)
    sys.exit(0 if success else 1)
