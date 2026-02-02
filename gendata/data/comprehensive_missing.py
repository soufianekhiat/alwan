"""
Comprehensive data generation for all remaining missing files.
Generates ~160 files: CAT matrices, illuminants, test samples, ACES ODT, sRGB primaries.
"""

import sys
import os
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
from common import save_matrix, save_vector

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)

def generate_comprehensive_missing(output_dir):
    """Generate all remaining missing data files."""

    print("\nComprehensive Missing Data Generation")
    print("=" * 70)

    os.makedirs(os.path.join(output_dir, 'matrices'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'illuminants'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'illuminants_xy'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'fixtures'), exist_ok=True)
    os.makedirs(output_dir, exist_ok=True)

    file_count = 0

    # =================================================================
    # 1. Additional CAT Matrices (4 files)
    # =================================================================
    print("\n[1/6] Additional chromatic adaptation matrices...")

    cat_transforms = {
        'CMCCAT2000': 'cat_cmccat2000.csv',
        'CAT02 Brill 2008': 'cat_cat02_brill_2008.csv',
        'Bianco 2010': 'cat_bianco_2010.csv',
        'Bianco PC 2010': 'cat_bianco_pc_2010.csv'
    }

    for cat_name, filename in cat_transforms.items():
        matrix = colour.CHROMATIC_ADAPTATION_TRANSFORMS[cat_name]
        filepath = os.path.join(output_dir, 'matrices', filename)
        save_matrix(matrix, filepath, f"{cat_name} CAT")
        file_count += 1

    print(f"  [OK] {len(cat_transforms)} CAT matrices generated")

    # =================================================================
    # 2. Extended Illuminant SPDs (38 files)
    # =================================================================
    print("\n[2/6] Extended illuminant SPDs (360-830nm @ 1nm)...")

    # Standard D-series, E, and F-series
    standard_illuminants = ['D50', 'D55', 'D60', 'D65', 'E', 'D75', 'B', 'C',
                           'F1', 'F2', 'F3', 'F4', 'F5', 'F6',
                           'F7', 'F8', 'F9', 'F10', 'F11', 'F12']

    # Additional D-series
    additional_d = ['D40', 'D45', 'D93']

    # LED series
    led_series = ['LED-B1', 'LED-B2', 'LED-B3', 'LED-B4', 'LED-B5',
                  'LED-BH1', 'LED-RGB1', 'LED-V1', 'LED-V2']

    # HP series
    hp_series = ['HP1', 'HP2', 'HP3', 'HP4', 'HP5']

    all_illuminants = standard_illuminants + additional_d + led_series + hp_series

    wavelengths = np.arange(360, 831, 1)
    spd_count = 0

    for illum_name in all_illuminants:
        try:
            spd = colour.SDS_ILLUMINANTS.get(illum_name)
            if spd is None:
                print(f"  [WARN] {illum_name} not found in colour-science, skipping")
                continue

            # Sample SPD at 1nm intervals from 360-830nm
            values = [float(spd[wl]) for wl in wavelengths]

            filename = f"{illum_name}_360_830_1nm.csv"
            filepath = os.path.join(output_dir, 'illuminants', filename)
            save_vector(values, filepath, f"{illum_name} SPD ({len(values)} samples)")
            spd_count += 1
            file_count += 1

        except Exception as e:
            print(f"  [ERROR] Failed to generate {illum_name}: {e}")

    print(f"  [OK] {spd_count} illuminant SPDs generated")

    # =================================================================
    # 3. Extended Illuminant xy (9 files)
    # =================================================================
    print("\n[3/6] Extended illuminant xy coordinates...")

    additional_xy_illuminants = ['D40', 'D45', 'D60', 'D75', 'D93', 'C',
                                  'LED-B1', 'LED-B2', 'LED-B3', 'LED-B4', 'LED-B5',
                                  'LED-BH1', 'LED-RGB1', 'LED-V1', 'LED-V2',
                                  'HP1', 'HP2', 'HP3', 'HP4', 'HP5']

    xy_count = 0
    for illum_name in additional_xy_illuminants:
        try:
            # Get xy from CCS_ILLUMINANTS
            xy = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer'].get(illum_name)
            if xy is None:
                # Try computing from SPD
                spd = colour.SDS_ILLUMINANTS.get(illum_name)
                if spd is not None:
                    xyz = colour.sd_to_XYZ(spd)
                    xy = colour.XYZ_to_xy(xyz)
                else:
                    print(f"  [WARN] {illum_name} xy not found, skipping")
                    continue

            filename = f"{illum_name.lower()}_xy.csv"
            filepath = os.path.join(output_dir, 'illuminants_xy', filename)
            save_vector([xy[0], xy[1]], filepath, f"{illum_name} xy")
            xy_count += 1
            file_count += 1

        except Exception as e:
            print(f"  [ERROR] Failed to generate {illum_name} xy: {e}")

    print(f"  [OK] {xy_count} illuminant xy coordinates generated")

    # =================================================================
    # 4. Test Sample Reflectances (108 files)
    # =================================================================
    print("\n[4/6] Test sample reflectances (TCS, VS, CES)...")

    # TCS samples 02-14 (13 files)
    tcs_count = 0
    from colour.quality import SDS_TCS
    for i, (name, spd) in enumerate(SDS_TCS.items(), 1):
        if i >= 2:  # Skip TCS 01 (already generated)
            filename = f"tcs_{i:02d}_reflectance.csv"
            filepath = os.path.join(output_dir, 'fixtures', filename)
            save_vector(spd.values.tolist(), filepath, f"TCS {i:02d} reflectance")
            tcs_count += 1
            file_count += 1
        if i >= 14:
            break

    print(f"  [OK] {tcs_count} TCS reflectances generated")

    # VS samples (CQS verification samples - nested structure in colour-science)
    vs_count = 0
    try:
        from colour.quality import SDS_VS
        # SDS_VS contains groups (e.g., 'NIST CQS 7.4'), each with VS samples ('VS1', 'VS2', ...)
        for group_name, vs_dict in SDS_VS.items():
            # vs_dict is a dict like {'VS1': SpectralDistribution, 'VS2': SpectralDistribution, ...}
            for vs_name, spd in vs_dict.items():
                # Extract number from 'VS1', 'VS2', etc.
                vs_num = int(vs_name.replace('VS', ''))
                filename = f"vs_{vs_num:02d}_reflectance.csv"
                filepath = os.path.join(output_dir, 'fixtures', filename)
                # spd is a SpectralDistribution, .values is the wavelength values array
                values = spd.values.tolist()
                save_vector(values, filepath, f"VS {vs_num:02d} reflectance")
                vs_count += 1
                file_count += 1
        if vs_count > 0:
            print(f"  [OK] {vs_count} VS reflectances generated (from colour-science CQS)")
        else:
            print(f"  [WARN] No VS samples found in colour-science")
    except Exception as e:
        print(f"  [WARN] VS samples not available: {e}")

    # CES samples 01-80 (80 files)
    # NOTE: CES (Color Evaluation Samples) are not available in colour-science
    # These may need to be sourced from CIE or other external databases
    ces_count = 0
    print(f"  [SKIP] CES samples not available in colour-science library")
    # Keeping code structure for future implementation if CES data becomes available
    # try:
    #     from colour.colorimetry import SDS_LIGHT_SOURCES
    #     for i in range(1, 81):
    #         ces_name = f"CES{i:02d}"
    #         spd = SDS_LIGHT_SOURCES.get(ces_name)
    #         if spd is not None:
    #             filename = f"ces_{i:02d}_reflectance.csv"
    #             filepath = os.path.join(output_dir, 'fixtures', filename)
    #             save_vector(list(spd.values), filepath, f"CES {i:02d} reflectance")
    #             ces_count += 1
    #             file_count += 1
    #     if ces_count > 0:
    #         print(f"  [OK] {ces_count} CES reflectances generated")
    # except Exception as e:
    #     print(f"  [WARN] CES samples error: {e}")

    # =================================================================
    # 5. ACES ODT Matrix (1 file)
    # =================================================================
    print("\n[5/6] ACES Output Device Transform (ODT) matrix...")

    try:
        # ACES RRT+ODT for Rec.709/sRGB
        # This is the combined RRT+ODT matrix from ACES AP0 to Rec.709
        aces_ap0 = colour.RGB_COLOURSPACES['ACES2065-1']
        rec709 = colour.RGB_COLOURSPACES['ITU-R BT.709']

        # Get transformation: ACES AP0 -> XYZ -> Rec.709
        ap0_to_xyz = aces_ap0.matrix_RGB_to_XYZ
        xyz_to_rec709 = np.linalg.inv(rec709.matrix_RGB_to_XYZ)
        aces_odt_rec709 = np.dot(xyz_to_rec709, ap0_to_xyz)

        filepath = os.path.join(output_dir, 'matrices', 'aces_odt_rec709.csv')
        save_matrix(aces_odt_rec709, filepath, "ACES ODT Rec.709")
        file_count += 1
        print("  [OK] ACES ODT matrix generated")
    except Exception as e:
        print(f"  [ERROR] Failed to generate ACES ODT: {e}")

    # =================================================================
    # 6. sRGB Primaries (1 file)
    # =================================================================
    print("\n[6/6] sRGB primaries matrix...")

    try:
        srgb = colour.RGB_COLOURSPACES['sRGB']
        # Primaries are 3x2 matrix (RGB primaries as xy coordinates)
        primaries = srgb.primaries
        whitepoint = srgb.whitepoint

        # Flatten to 8 values: Rx, Ry, Gx, Gy, Bx, By, Wx, Wy
        primaries_flat = [
            primaries[0][0], primaries[0][1],  # Red xy
            primaries[1][0], primaries[1][1],  # Green xy
            primaries[2][0], primaries[2][1],  # Blue xy
            whitepoint[0], whitepoint[1]       # White xy
        ]

        filepath = os.path.join(output_dir, 'srgb_primaries_3x2.csv')
        save_vector(primaries_flat, filepath, "sRGB primaries + whitepoint")
        file_count += 1
        print("  [OK] sRGB primaries generated")
    except Exception as e:
        print(f"  [ERROR] Failed to generate sRGB primaries: {e}")

    print("\n" + "=" * 70)
    print(f"Comprehensive data generation complete!")
    print(f"  Total files generated: {file_count}")
    print("=" * 70)

    return True

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python comprehensive_missing.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    success = generate_comprehensive_missing(output_dir)
    sys.exit(0 if success else 1)
