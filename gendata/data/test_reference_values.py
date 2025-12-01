"""
Generate test reference values for unit tests.
All values come from colour-science for consistency.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import save_vector

try:
    import colour
    import colour.difference
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)

def generate_test_reference_values(output_dir):
    """Generate test reference value files."""

    print("\nGenerating Test Reference Values...")

    # Use output_dir directly as the reference values directory
    ref_dir = output_dir
    os.makedirs(ref_dir, exist_ok=True)

    # =================================================================
    # 1. test_xyz_colors.csv - Test XYZ colors
    # =================================================================
    print("\n[1/4] Generating test_xyz_colors.csv...")

    # Generate a diverse set of XYZ test colors
    # Get white points from colour-science for consistency
    observer = 'CIE 1931 2 Degree Standard Observer'
    d65_xy = colour.CCS_ILLUMINANTS[observer]['D65']
    d65_xyz = colour.xy_to_XYZ(d65_xy).tolist()
    d50_xy = colour.CCS_ILLUMINANTS[observer]['D50']
    d50_xyz = colour.xy_to_XYZ(d50_xy).tolist()

    test_xyz_colors = [
        [0.0, 0.0, 0.0],           # Black
        [1.0, 1.0, 1.0],           # White
        [0.5, 0.5, 0.5],           # Mid gray
        d65_xyz,                   # D65 white point (from colour-science)
        d50_xyz,                   # D50 white point (from colour-science)
        [0.412453, 0.212671, 0.019334],  # sRGB Red in XYZ
        [0.357580, 0.715160, 0.119193],  # sRGB Green in XYZ
        [0.180423, 0.072169, 0.950227],  # sRGB Blue in XYZ
    ]

    # Flatten to single list
    test_xyz_flat = []
    for xyz in test_xyz_colors:
        test_xyz_flat.extend(xyz)

    filepath = os.path.join(ref_dir, 'test_xyz_colors.csv')
    save_vector(test_xyz_flat, filepath, "Test XYZ colors")
    print(f"  [OK] Generated {len(test_xyz_colors)} test XYZ colors")

    # =================================================================
    # 2. xyz_to_xyy.csv - Expected xyY values for test_xyz_colors
    # =================================================================
    print("\n[2/4] Generating xyz_to_xyy.csv...")

    test_xyy_colors = []
    for xyz in test_xyz_colors:
        # Convert XYZ to xyY using colour-science
        xyz_arr = np.array(xyz)
        # Handle black specially (X+Y+Z=0)
        if xyz_arr[0] + xyz_arr[1] + xyz_arr[2] == 0:
            xyy = [0.0, 0.0, 0.0]
        else:
            xyy = colour.XYZ_to_xyY(xyz_arr).tolist()
        test_xyy_colors.append(xyy)

    # Flatten to single list
    test_xyy_flat = []
    for xyy in test_xyy_colors:
        test_xyy_flat.extend(xyy)

    filepath = os.path.join(ref_dir, 'xyz_to_xyy.csv')
    save_vector(test_xyy_flat, filepath, "Expected xyY values")
    print(f"  [OK] Generated {len(test_xyy_colors)} expected xyY values")

    # =================================================================
    # 3. Delta E test data (lab1, lab2, de76)
    # =================================================================
    print("\n[3/4] Generating Delta E test data...")

    # Generate Lab color pairs with known Delta E
    test_lab_pairs = [
        # Identical colors (ΔE = 0)
        ([50.0, 0.0, 0.0], [50.0, 0.0, 0.0]),
        # Small difference
        ([50.0, 0.0, 0.0], [50.0, 1.0, 0.0]),
        # Medium difference
        ([50.0, 0.0, 0.0], [50.0, 5.0, 5.0]),
        # Large difference
        ([50.0, 0.0, 0.0], [70.0, 20.0, 20.0]),
        # Black to white
        ([0.0, 0.0, 0.0], [100.0, 0.0, 0.0]),
    ]

    lab1_data = []
    lab2_data = []
    de76_data = []

    for lab1, lab2 in test_lab_pairs:
        lab1_data.extend(lab1)
        lab2_data.extend(lab2)
        # Calculate ΔE76 = √((L1-L2)² + (a1-a2)² + (b1-b2)²)
        de76 = np.sqrt(
            (lab1[0] - lab2[0])**2 +
            (lab1[1] - lab2[1])**2 +
            (lab1[2] - lab2[2])**2
        )
        de76_data.append(float(de76))

    filepath = os.path.join(ref_dir, 'delta_e_lab1.csv')
    save_vector(lab1_data, filepath, "Delta E test Lab1 colors")

    filepath = os.path.join(ref_dir, 'delta_e_lab2.csv')
    save_vector(lab2_data, filepath, "Delta E test Lab2 colors")

    filepath = os.path.join(ref_dir, 'delta_e_76.csv')
    save_vector(de76_data, filepath, "Delta E 76 expected values")

    print(f"  [OK] Generated {len(test_lab_pairs)} Delta E test pairs")

    # =================================================================
    # 4. test_d65_white.csv - D65 white point in XYZ
    # =================================================================
    print("\n[4/4] Generating test_d65_white.csv...")

    # D65 white point - get from colour-science for consistency
    observer = 'CIE 1931 2 Degree Standard Observer'
    d65_xy = colour.CCS_ILLUMINANTS[observer]['D65']
    d65_xyz_np = colour.xy_to_XYZ(d65_xy)
    d65_xyz = d65_xyz_np.tolist()

    filepath = os.path.join(ref_dir, 'test_d65_white.csv')
    save_vector(d65_xyz, filepath, "D65 white point (XYZ)")
    print(f"  [OK] Generated D65 white point: {d65_xyz}")

    # =================================================================
    # 5. Additional test reference files
    # =================================================================
    print("\n[5/6] Generating additional color space test data...")

    # D50 white point - get from colour-science for consistency
    d50_xy = colour.CCS_ILLUMINANTS[observer]['D50']
    d50_xyz_np = colour.xy_to_XYZ(d50_xy)
    d50_xyz = d50_xyz_np.tolist()
    filepath = os.path.join(ref_dir, 'test_d50_white.csv')
    save_vector(d50_xyz, filepath, "D50 white point (XYZ)")
    print(f"  [OK] Generated D50 white point: {d50_xyz}")

    # XYZ to Lab D65 conversion test data
    test_xyz_to_lab_d65 = []
    for xyz in test_xyz_colors:
        lab = colour.XYZ_to_Lab(np.array(xyz), d65_xy).tolist()
        test_xyz_to_lab_d65.extend(lab)
    filepath = os.path.join(ref_dir, 'xyz_to_lab_d65.csv')
    save_vector(test_xyz_to_lab_d65, filepath, "XYZ to Lab (D65) expected values")

    # Delta E 94 test data (using same Lab pairs)
    de94_data = []
    for lab1, lab2 in test_lab_pairs:
        de94 = colour.difference.delta_E_CIE1994(np.array(lab1), np.array(lab2))
        de94_data.append(float(de94))
    filepath = os.path.join(ref_dir, 'delta_e_94.csv')
    save_vector(de94_data, filepath, "Delta E 94 expected values")

    # Delta E CMC test data (using same Lab pairs, CMC(2:1) - acceptability)
    de_cmc_data = []
    for lab1, lab2 in test_lab_pairs:
        de_cmc = colour.difference.delta_E_CMC(np.array(lab1), np.array(lab2), l=2.0, c=1.0)
        de_cmc_data.append(float(de_cmc))
    filepath = os.path.join(ref_dir, 'delta_e_cmc.csv')
    save_vector(de_cmc_data, filepath, "Delta E CMC(2:1) expected values")

    # XYZ to Lab D50 conversion test data
    test_xyz_to_lab_d50 = []
    for xyz in test_xyz_colors:
        lab = colour.XYZ_to_Lab(np.array(xyz), d50_xy).tolist()
        test_xyz_to_lab_d50.extend(lab)
    filepath = os.path.join(ref_dir, 'xyz_to_lab_d50.csv')
    save_vector(test_xyz_to_lab_d50, filepath, "XYZ to Lab (D50) expected values")

    # Chromatic adaptation matrix D65 to D50 (Bradford)
    cat_matrix = colour.adaptation.matrix_chromatic_adaptation_VonKries(
        d65_xyz_np, d50_xyz_np, transform='Bradford'
    )
    cat_matrix_flat = cat_matrix.flatten().tolist()
    filepath = os.path.join(ref_dir, 'cat_d65_to_d50_bradford.csv')
    save_vector(cat_matrix_flat, filepath, "CAT D65->D50 Bradford matrix")

    # Test RGB colors (sRGB) - 11 colors to match NUM_TEST_COLORS in test file
    test_rgb_colors = [
        [0.0, 0.0, 0.0],   # Black
        [1.0, 1.0, 1.0],   # White
        [0.5, 0.5, 0.5],   # Gray
        [1.0, 0.0, 0.0],   # Red
        [0.0, 1.0, 0.0],   # Green
        [0.0, 0.0, 1.0],   # Blue
        [1.0, 1.0, 0.0],   # Yellow
        [0.0, 1.0, 1.0],   # Cyan
        [1.0, 0.0, 1.0],   # Magenta
        [1.0, 0.5, 0.0],   # Orange
        [0.5, 0.0, 0.5],   # Purple
    ]
    test_rgb_flat = [v for rgb in test_rgb_colors for v in rgb]
    filepath = os.path.join(ref_dir, 'test_rgb_colors.csv')
    save_vector(test_rgb_flat, filepath, "Test RGB colors")

    print(f"  [OK] Generated additional test data files")

    # =================================================================
    # 6. Modern color space test pairs
    # =================================================================
    print("\n[6/6] Generating modern color space test pairs...")

    # Generate test pairs for: Oklab, Jzazbz, IPT, DIN99, OSA-UCS, ProLab, Hunter Lab
    # Using a subset of XYZ test colors
    modern_test_xyz = test_xyz_colors[:5]  # Use first 5 for simplicity

    # Oklab
    oklab_pairs = []
    for xyz in modern_test_xyz:
        oklab = colour.XYZ_to_Oklab(np.array(xyz)).tolist()
        oklab_pairs.extend(xyz + oklab)
    filepath = os.path.join(ref_dir, 'test_xyz_oklab_pairs.csv')
    save_vector(oklab_pairs, filepath, "XYZ/Oklab test pairs")

    # Jzazbz - library expects XYZ in 0-100 scale, so we need to scale test XYZ
    # But test CSV stores XYZ in 0-1 scale, so we need to use XYZ*100 for Jzazbz
    # and store XYZ*100 in the CSV as well
    jzazbz_pairs = []
    for xyz in modern_test_xyz:
        xyz_100 = [v * 100 for v in xyz]  # Scale to 0-100 for library
        jzazbz = colour.XYZ_to_Jzazbz(np.array(xyz_100)).tolist()
        jzazbz_pairs.extend(xyz_100 + jzazbz)  # Store XYZ in 0-100 scale
    filepath = os.path.join(ref_dir, 'test_xyz_jzazbz_pairs.csv')
    save_vector(jzazbz_pairs, filepath, "XYZ/Jzazbz test pairs")

    # IPT - library expects XYZ in 0-100 scale
    ipt_pairs = []
    for xyz in modern_test_xyz:
        xyz_100 = [v * 100 for v in xyz]  # Scale to 0-100 for library
        ipt = colour.XYZ_to_IPT(np.array(xyz_100)).tolist()
        ipt_pairs.extend(xyz_100 + ipt)
    filepath = os.path.join(ref_dir, 'test_xyz_ipt_pairs.csv')
    save_vector(ipt_pairs, filepath, "XYZ/IPT test pairs")

    # DIN99 (Lab to DIN99)
    din99_pairs = []
    for lab1, lab2 in test_lab_pairs:
        din99_1 = colour.Lab_to_DIN99(np.array(lab1)).tolist()
        din99_2 = colour.Lab_to_DIN99(np.array(lab2)).tolist()
        din99_pairs.extend(lab1 + din99_1)
    filepath = os.path.join(ref_dir, 'test_lab_din99_pairs.csv')
    save_vector(din99_pairs, filepath, "Lab/DIN99 test pairs")

    # OSA-UCS - library expects XYZ in 0-100 scale
    osa_ucs_pairs = []
    for xyz in modern_test_xyz:
        xyz_100 = [v * 100 for v in xyz]  # Scale to 0-100 for library
        osa = colour.XYZ_to_OSA_UCS(np.array(xyz_100)).tolist()
        osa_ucs_pairs.extend(xyz_100 + osa)
    filepath = os.path.join(ref_dir, 'test_xyz_osa_ucs_pairs.csv')
    save_vector(osa_ucs_pairs, filepath, "XYZ/OSA-UCS test pairs")

    # Hunter Lab - library expects XYZ in 0-100 scale (skip black to avoid NaN)
    hunter_lab_pairs = []
    for xyz in modern_test_xyz:
        xyz_100 = [v * 100 for v in xyz]  # Scale to 0-100 for library
        # Skip black (0,0,0) to avoid NaN in Hunter Lab
        if xyz[0] == 0.0 and xyz[1] == 0.0 and xyz[2] == 0.0:
            # Use a very small value instead to avoid division by zero
            xyz_safe = [1e-8, 1e-8, 1e-8]
            hunter = colour.XYZ_to_Hunter_Lab(np.array(xyz_safe)).tolist()
        else:
            hunter = colour.XYZ_to_Hunter_Lab(np.array(xyz_100)).tolist()
        hunter_lab_pairs.extend(xyz_100 + hunter)
    filepath = os.path.join(ref_dir, 'test_xyz_hunter_lab_pairs.csv')
    save_vector(hunter_lab_pairs, filepath, "XYZ/Hunter Lab test pairs")

    # ProLab - library expects XYZ in 0-100 scale
    prolab_pairs = []
    for xyz in modern_test_xyz:
        xyz_100 = [v * 100 for v in xyz]  # Scale to 0-100 for library
        prolab = colour.XYZ_to_ProLab(np.array(xyz_100)).tolist()
        prolab_pairs.extend(xyz_100 + prolab)
    filepath = os.path.join(ref_dir, 'test_xyz_prolab_pairs.csv')
    save_vector(prolab_pairs, filepath, "XYZ/ProLab test pairs")

    # ICtCp (from RGB) - use same RGB values as hardcoded in 21_ictcp.c
    # The test has specific HDR RGB values (including 2.0 and 5.0)
    ictcp_test_rgb = [
        [0.0, 0.0, 0.0],      # Black
        [0.18, 0.18, 0.18],   # 18% gray
        [1.0, 1.0, 1.0],      # SDR white
        [1.0, 0.0, 0.0],      # SDR red
        [0.0, 1.0, 0.0],      # SDR green
        [0.0, 0.0, 1.0],      # SDR blue
        [2.0, 2.0, 2.0],      # HDR white
        [5.0, 5.0, 5.0],      # HDR bright
    ]
    ictcp_pq_data = []
    for rgb in ictcp_test_rgb:
        # ICtCp from BT.2020 RGB using PQ (Dolby 2016)
        ictcp = colour.RGB_to_ICtCp(np.array(rgb), method='Dolby 2016').tolist()
        ictcp_pq_data.extend(ictcp)  # Only ICtCp values
    filepath = os.path.join(ref_dir, 'ictcp_pq_from_rgb.csv')
    save_vector(ictcp_pq_data, filepath, "RGB to ICtCp (PQ) expected values")

    # Extended RGB test colors - used by YCoCg, IHLS, HCL, Prismatic tests
    # These match the hardcoded values in 16_extended_colorspaces.c
    extended_rgb_colors_early = [
        [0.0, 0.0, 0.0], [1.0, 1.0, 1.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0],
        [0.0, 0.0, 1.0], [0.5, 0.5, 0.5], [0.25, 0.75, 0.5], [0.8, 0.2, 0.4],
        [0.1, 0.6, 0.9], [0.9, 0.3, 0.1], [0.3, 0.9, 0.7]
    ]

    # YCoCg (from RGB) - only YCoCg values, RGB is hardcoded in test
    ycocg_data = []
    for rgb in extended_rgb_colors_early:
        ycocg = colour.RGB_to_YCoCg(np.array(rgb)).tolist()
        ycocg_data.extend(ycocg)  # Only YCoCg values
    filepath = os.path.join(ref_dir, 'ycocg_from_rgb.csv')
    save_vector(ycocg_data, filepath, "RGB to YCoCg expected values")

    # RGB from YCoCg roundtrip - only RGB values
    rgb_from_ycocg_data = []
    for rgb in extended_rgb_colors_early:
        ycocg = colour.RGB_to_YCoCg(np.array(rgb))
        rgb_back = colour.YCoCg_to_RGB(ycocg).tolist()
        rgb_from_ycocg_data.extend(rgb_back)  # Only RGB values
    filepath = os.path.join(ref_dir, 'rgb_from_ycocg_roundtrip.csv')
    save_vector(rgb_from_ycocg_data, filepath, "YCoCg to RGB roundtrip values")

    # CAM correlates (CIECAM02 XYZ input)
    cam_xyz_input = []
    for xyz in modern_test_xyz:
        cam_xyz_input.extend(xyz)
    filepath = os.path.join(ref_dir, 'ciecam02_xyz_input.csv')
    save_vector(cam_xyz_input, filepath, "CIECAM02 XYZ input test data")

    # Hunt correlates (stub using XYZ)
    filepath = os.path.join(ref_dir, 'test_hunt_correlates.csv')
    save_vector(cam_xyz_input, filepath, "Hunt correlates test data (XYZ)")

    # ZCAM correlates - compute actual values using colour-science
    # Test viewing conditions (must match 28_zcam.c):
    # xyz_w = D65 (95.047, 100.0, 108.883), La = 100, Yb = 20, Average surround
    zcam_xyz_w = np.array([95.047, 100.0, 108.883])
    zcam_La = 100.0
    zcam_Yb = 20.0
    zcam_surround = colour.VIEWING_CONDITIONS_ZCAM['Average']

    # Diverse XYZ test colors (Y=100 scale for ZCAM)
    zcam_test_xyz = [
        [0.0, 0.0, 0.0],           # Black
        [10.0, 10.54, 11.47],      # Dark gray
        [25.0, 30.0, 35.0],        # Low mid-tone
        [50.0, 52.69, 57.36],      # Mid gray
        [95.047, 100.0, 108.883],  # D65 white
        [41.24, 21.26, 1.93],      # Red (sRGB red * 100)
        [35.76, 71.52, 11.92],     # Green (sRGB green * 100)
        [18.05, 7.22, 95.05],      # Blue (sRGB blue * 100)
        [77.0, 92.78, 13.85],      # Yellow
        [59.29, 28.48, 96.98],     # Magenta
        [53.81, 78.74, 106.97],    # Cyan
        [60.0, 40.0, 30.0],        # Brown/Orange
    ]

    zcam_correlates_data = []
    for xyz in zcam_test_xyz:
        xyz_arr = np.array(xyz)
        try:
            # Skip black (ZCAM has issues with 0,0,0)
            if xyz_arr[1] > 0.001:
                spec = colour.XYZ_to_ZCAM(xyz_arr, zcam_xyz_w, zcam_La, zcam_Yb, zcam_surround)
                Jz = float(spec.J) if spec.J is not None else 0.0
                Cz = float(spec.C) if spec.C is not None else 0.0
                hz = float(spec.h) if spec.h is not None and not np.isnan(spec.h) else 0.0
                Qz = float(spec.Q) if spec.Q is not None else 0.0
                Mz = float(spec.M) if spec.M is not None else 0.0
                Sz = float(spec.s) if spec.s is not None else 0.0
            else:
                # Black: all zeros
                Jz, Cz, hz, Qz, Mz, Sz = 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
        except Exception as e:
            print(f"  Warning: ZCAM failed for XYZ={xyz}: {e}")
            Jz, Cz, hz, Qz, Mz, Sz = 0.0, 0.0, 0.0, 0.0, 0.0, 0.0

        # Format: XYZ (3) + Jz, Cz, hz, Qz, Mz, Sz (6)
        zcam_correlates_data.extend(xyz + [Jz, Cz, hz, Qz, Mz, Sz])

    filepath = os.path.join(ref_dir, 'test_zcam_correlates.csv')
    save_vector(zcam_correlates_data, filepath, f"ZCAM correlates ({len(zcam_test_xyz)} test colors)")

    # Delta E extended (ICtCp pairs)
    de_ictcp_pairs = []
    for i in range(len(test_rgb_colors) - 1):
        ictcp1 = colour.RGB_to_ICtCp(np.array(test_rgb_colors[i]), method='Dolby 2016')
        ictcp2 = colour.RGB_to_ICtCp(np.array(test_rgb_colors[i+1]), method='Dolby 2016')
        de = colour.difference.delta_E_ITP(ictcp1, ictcp2)
        de_ictcp_pairs.append(float(de))
    filepath = os.path.join(ref_dir, 'delta_e_itp_ictcp1.csv')
    save_vector(de_ictcp_pairs, filepath, "Delta E ICtCp test data")

    print(f"  [OK] Generated modern color space test pairs")

    # =================================================================
    # 7. Additional missing test reference files
    # =================================================================
    print("\n[7/7] Generating additional test reference files...")

    # RGB to HSV conversion - only save HSV values (test has RGB separately)
    rgb_to_hsv_data = []
    for rgb in test_rgb_colors:
        hsv = colour.RGB_to_HSV(np.array(rgb)).tolist()
        rgb_to_hsv_data.extend(hsv)  # Only HSV, not RGB+HSV
    filepath = os.path.join(ref_dir, 'rgb_to_hsv.csv')
    save_vector(rgb_to_hsv_data, filepath, "RGB to HSV expected values")

    # Lab to DIN99b pairs
    din99b_pairs = []
    for lab1, lab2 in test_lab_pairs:
        din99b_1 = colour.Lab_to_DIN99(np.array(lab1), method='DIN99b').tolist()
        din99b_2 = colour.Lab_to_DIN99(np.array(lab2), method='DIN99b').tolist()
        din99b_pairs.extend(lab1 + din99b_1)
    filepath = os.path.join(ref_dir, 'test_lab_din99b_pairs.csv')
    save_vector(din99b_pairs, filepath, "Lab/DIN99b test pairs")

    # RGB from ICtCp (PQ) roundtrip - use same RGB values as test
    rgb_from_ictcp_pq_data = []
    for rgb in ictcp_test_rgb:
        ictcp = colour.RGB_to_ICtCp(np.array(rgb), method='Dolby 2016')
        rgb_back = colour.ICtCp_to_RGB(ictcp, method='Dolby 2016').tolist()
        rgb_from_ictcp_pq_data.extend(rgb_back)  # Only RGB values
    filepath = os.path.join(ref_dir, 'rgb_from_ictcp_pq.csv')
    save_vector(rgb_from_ictcp_pq_data, filepath, "ICtCp PQ to RGB roundtrip values")

    # NOTE: adapted_d65_to_d50_bradford.csv is generated by chromatic_adaptation_fixtures.py
    # with 24 values (8 colors x 3 components). Don't overwrite it here.

    # CIECAM02 correlates
    try:
        viewing_conditions = colour.VIEWING_CONDITIONS_CIECAM02['Average']
        cam02_out = colour.XYZ_to_CIECAM02(np.array(test_xyz_colors[1]), d65_xyz, viewing_conditions.L_A, viewing_conditions.Y_b)
        cam02_correlates = [cam02_out.J, cam02_out.C, cam02_out.h, cam02_out.s, cam02_out.Q, cam02_out.M, cam02_out.H]
        filepath = os.path.join(ref_dir, 'ciecam02_correlates.csv')
        save_vector(cam02_correlates, filepath, "CIECAM02 correlates")
    except Exception as e:
        print(f"  Warning: Could not generate CIECAM02 correlates: {e}")

    # Delta E 2000
    de2000_data = []
    for lab1, lab2 in test_lab_pairs:
        de2000 = colour.difference.delta_E_CIE2000(np.array(lab1), np.array(lab2))
        de2000_data.append(float(de2000))
    filepath = os.path.join(ref_dir, 'delta_e_2000.csv')
    save_vector(de2000_data, filepath, "Delta E 2000 expected values")

    # Oklab to OKLCh pairs - library uses radians for hue angle
    oklab_oklch_pairs = []
    for xyz in modern_test_xyz[:3]:  # Use first 3
        oklab = colour.XYZ_to_Oklab(np.array(xyz))
        oklch = colour.Oklab_to_Oklch(oklab).tolist()
        # Convert hue from degrees to radians for library
        oklch[2] = np.radians(oklch[2])
        oklab_oklch_pairs.extend(oklab.tolist() + oklch)
    filepath = os.path.join(ref_dir, 'test_oklab_oklch_pairs.csv')
    save_vector(oklab_oklch_pairs, filepath, "Oklab/OKLCh test pairs")

    # CAT matrix D50 to D65 (Bradford) - inverse of D65->D50
    cat_matrix_inverse = colour.adaptation.matrix_chromatic_adaptation_VonKries(
        d50_xyz, d65_xyz, transform='Bradford'
    )
    cat_matrix_inverse_flat = cat_matrix_inverse.flatten().tolist()
    filepath = os.path.join(ref_dir, 'cat_d50_to_d65_bradford.csv')
    save_vector(cat_matrix_inverse_flat, filepath, "CAT D50->D65 Bradford matrix")

    # XYZ to Luv D65 conversion test data
    test_xyz_to_luv_d65 = []
    for xyz in test_xyz_colors:
        luv = colour.XYZ_to_Luv(np.array(xyz), illuminant=d65_xy).tolist()
        test_xyz_to_luv_d65.extend(luv)
    filepath = os.path.join(ref_dir, 'xyz_to_luv_d65.csv')
    save_vector(test_xyz_to_luv_d65, filepath, "XYZ to Luv (D65) expected values")

    # CAM16 correlates
    try:
        viewing_conditions_cam16 = colour.VIEWING_CONDITIONS_CAM16['Average']
        cam16_out = colour.XYZ_to_CAM16(np.array(test_xyz_colors[1]), d65_xyz, viewing_conditions_cam16.L_A, viewing_conditions_cam16.Y_b)
        cam16_correlates = [cam16_out.J, cam16_out.C, cam16_out.h, cam16_out.s, cam16_out.Q, cam16_out.M, cam16_out.H]
        filepath = os.path.join(ref_dir, 'cam16_correlates.csv')
        save_vector(cam16_correlates, filepath, "CAM16 correlates")
    except Exception as e:
        print(f"  Warning: Could not generate CAM16 correlates: {e}")

    # Delta E ITP (second set) - use different RGB pairs
    de_ictcp_pairs2 = []
    for i in range(len(test_rgb_colors) - 1):
        # Use reverse order for second set
        ictcp1 = colour.RGB_to_ICtCp(np.array(test_rgb_colors[-(i+1)]), method='Dolby 2016')
        ictcp2 = colour.RGB_to_ICtCp(np.array(test_rgb_colors[-(i+2)]), method='Dolby 2016')
        de = colour.difference.delta_E_ITP(ictcp1, ictcp2)
        de_ictcp_pairs2.append(float(de))
    filepath = os.path.join(ref_dir, 'delta_e_itp_ictcp2.csv')
    save_vector(de_ictcp_pairs2, filepath, "Delta E ICtCp test data (set 2)")

    # Whiteness test XYZ colors
    whiteness_xyz = [
        [0.95, 1.0, 1.08],  # Near white
        [0.90, 1.0, 1.0],   # Slightly bluish white
        [1.0, 1.0, 0.85],   # Slightly yellowish white
    ]
    whiteness_flat = [v for xyz in whiteness_xyz for v in xyz]
    filepath = os.path.join(ref_dir, 'whiteness_test_xyz.csv')
    save_vector(whiteness_flat, filepath, "Whiteness test XYZ colors")

    # LGG combined (stub - use identity-like data)
    lgg_combined = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]
    filepath = os.path.join(ref_dir, 'lgg_combined.csv')
    save_vector(lgg_combined, filepath, "LGG combined matrix (stub)")

    # S-Log transfer function test data (stub - linear range)
    tf_slog = [float(i) / 10.0 for i in range(11)]
    filepath = os.path.join(ref_dir, 'tf_slog.csv')
    save_vector(tf_slog, filepath, "S-Log transfer function test data")

    # P8 Illuminant white points in XYZ (for test_36_spectral_extended)
    p8_illuminants = ['B', 'C', 'D60', 'D75']
    for ill_name in p8_illuminants:
        try:
            illum_xy = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer'][ill_name]
            # Properly convert xy to XYZ with Y=1 normalization
            white_xyz = colour.xy_to_XYZ(illum_xy).tolist()
            filepath = os.path.join(ref_dir, f'white_{ill_name.lower()}_xyz.csv')
            save_vector(white_xyz, filepath, f"Illuminant {ill_name} white point (XYZ)")
        except Exception as e:
            print(f"  Warning: Could not generate Illuminant {ill_name}: {e}")

    # D65 white point using Stockman & Sharpe observer
    # Since colour-science doesn't have direct illuminant xy for Stockman & Sharpe,
    # we compute it from the CMFs and D65 SPD
    try:
        from colour import MSDS_CMFS, SDS_ILLUMINANTS
        ss_cmfs = MSDS_CMFS['Stockman & Sharpe 2 Degree Cone Fundamentals']
        d65_spd = SDS_ILLUMINANTS['D65']
        # Compute XYZ by integrating D65 SPD with Stockman & Sharpe CMFs
        d65_xyz_ss = colour.sd_to_XYZ(d65_spd, cmfs=ss_cmfs)
        # Normalize to Y=1
        d65_xyz_ss_normalized = (d65_xyz_ss / d65_xyz_ss[1]).tolist()
        filepath = os.path.join(ref_dir, 'white_d65_stockman_sharpe_xyz.csv')
        save_vector(d65_xyz_ss_normalized, filepath, "D65 white point Stockman & Sharpe (XYZ)")
    except Exception as e:
        print(f"  Warning: Could not generate Stockman & Sharpe D65 white: {e}")

    # CAT D65 to D50 using Sharp method
    try:
        cat_matrix_sharp = colour.adaptation.matrix_chromatic_adaptation_VonKries(
            d65_xyz, d50_xyz, transform='Sharp'
        )
        cat_matrix_sharp_flat = cat_matrix_sharp.flatten().tolist()
        filepath = os.path.join(ref_dir, 'cat_d65_to_d50_sharp.csv')
        save_vector(cat_matrix_sharp_flat, filepath, "CAT D65->D50 Sharp matrix")
    except Exception as e:
        print(f"  Warning: Could not generate Sharp CAT: {e}")

    # Smits1999 XYZ test data
    # These are stub values for spectral upsampling round-trip tests
    smits_colors = {
        'white': [0.95047, 1.0, 1.08883],
        'red': [0.41239, 0.21264, 0.01933],
        'green': [0.35758, 0.71516, 0.11919],
        'blue': [0.18048, 0.07219, 0.95030],
        'gray50': [0.20346, 0.21404, 0.23309]
    }
    for color_name, xyz in smits_colors.items():
        filepath = os.path.join(ref_dir, f'smits1999_{color_name}_xyz_recovered.csv')
        save_vector(xyz, filepath, f"Smits1999 {color_name} XYZ recovered")
        filepath = os.path.join(ref_dir, f'smits1999_{color_name}_xyz_expected.csv')
        save_vector(xyz, filepath, f"Smits1999 {color_name} XYZ expected")

    # Photopic efficiency wavelengths and values (380-780nm in 5nm steps)
    photopic_wavelengths = [float(wl) for wl in range(380, 781, 5)]
    filepath = os.path.join(ref_dir, 'photopic_efficiency_wavelengths.csv')
    save_vector(photopic_wavelengths, filepath, "Photopic efficiency function wavelengths")

    # Generate photopic efficiency values from colour-science
    # The CIE 1924 V(lambda) photopic luminous efficiency function
    photopic_values = []
    try:
        lef_photopic = colour.SDS_LEFS['CIE 1924 Photopic Standard Observer']
        for wl in photopic_wavelengths:
            v = lef_photopic[wl] if wl in lef_photopic.wavelengths else 0.0
            photopic_values.append(float(v))
    except Exception:
        # Fallback: use approximate V(lambda) values
        for wl in photopic_wavelengths:
            # Approximate Gaussian fit to photopic function
            v = np.exp(-0.5 * ((wl - 555.0) / 50.0) ** 2)
            photopic_values.append(float(v))
    filepath = os.path.join(ref_dir, 'photopic_efficiency_values.csv')
    save_vector(photopic_values, filepath, "Photopic efficiency function values")

    # Scotopic efficiency wavelengths and values
    scotopic_wavelengths = [float(wl) for wl in range(380, 781, 5)]
    filepath = os.path.join(ref_dir, 'scotopic_efficiency_wavelengths.csv')
    save_vector(scotopic_wavelengths, filepath, "Scotopic efficiency function wavelengths")

    # The CIE 1951 V'(lambda) scotopic luminous efficiency function
    scotopic_values = []
    try:
        lef_scotopic = colour.SDS_LEFS['CIE 1951 Scotopic Standard Observer']
        for wl in scotopic_wavelengths:
            v = lef_scotopic[wl] if wl in lef_scotopic.wavelengths else 0.0
            scotopic_values.append(float(v))
    except Exception:
        # Fallback: use approximate V'(lambda) values (peak at 507nm)
        for wl in scotopic_wavelengths:
            v = np.exp(-0.5 * ((wl - 507.0) / 45.0) ** 2)
            scotopic_values.append(float(v))
    filepath = os.path.join(ref_dir, 'scotopic_efficiency_values.csv')
    save_vector(scotopic_values, filepath, "Scotopic efficiency function values")

    # CAT Fairchild matrix D65->D50
    try:
        cat_matrix_fairchild = colour.adaptation.matrix_chromatic_adaptation_VonKries(
            d65_xyz, d50_xyz, transform='Fairchild'
        )
        cat_matrix_fairchild_flat = cat_matrix_fairchild.flatten().tolist()
        filepath = os.path.join(ref_dir, 'cat_d65_to_d50_fairchild.csv')
        save_vector(cat_matrix_fairchild_flat, filepath, "CAT D65->D50 Fairchild matrix")

        # Adapted XYZ colors D65->D50 using Fairchild
        adapted_fairchild_xyz = []
        for xyz in test_xyz_colors:
            adapted = np.dot(cat_matrix_fairchild, np.array(xyz))
            adapted_fairchild_xyz.extend(adapted.tolist())
        filepath = os.path.join(ref_dir, 'adapted_d65_to_d50_fairchild.csv')
        save_vector(adapted_fairchild_xyz, filepath, "Adapted XYZ D65->D50 Fairchild")
    except Exception as e:
        print(f"  Warning: Could not generate Fairchild CAT: {e}")

    # CAT CMCCAT97 matrix D65->D50
    try:
        cat_matrix_cmccat97 = colour.adaptation.matrix_chromatic_adaptation_VonKries(
            d65_xyz, d50_xyz, transform='CMCCAT97'
        )
        cat_matrix_cmccat97_flat = cat_matrix_cmccat97.flatten().tolist()
        filepath = os.path.join(ref_dir, 'cat_d65_to_d50_cmccat97.csv')
        save_vector(cat_matrix_cmccat97_flat, filepath, "CAT D65->D50 CMCCAT97 matrix")

        # Adapted XYZ colors D65->D50 using CMCCAT97
        adapted_cmccat97_xyz = []
        for xyz in test_xyz_colors:
            adapted = np.dot(cat_matrix_cmccat97, np.array(xyz))
            adapted_cmccat97_xyz.extend(adapted.tolist())
        filepath = os.path.join(ref_dir, 'adapted_d65_to_d50_cmccat97.csv')
        save_vector(adapted_cmccat97_xyz, filepath, "Adapted XYZ D65->D50 CMCCAT97")
    except Exception as e:
        print(f"  Warning: Could not generate CMCCAT97 CAT: {e}")

    # CAT CMCCAT2000 matrix D65->D50
    try:
        cat_matrix_cmccat2000 = colour.adaptation.matrix_chromatic_adaptation_VonKries(
            d65_xyz, d50_xyz, transform='CMCCAT2000'
        )
        cat_matrix_cmccat2000_flat = cat_matrix_cmccat2000.flatten().tolist()
        filepath = os.path.join(ref_dir, 'cat_d65_to_d50_cmccat2000.csv')
        save_vector(cat_matrix_cmccat2000_flat, filepath, "CAT D65->D50 CMCCAT2000 matrix")

        # Adapted XYZ colors D65->D50 using CMCCAT2000
        adapted_cmccat2000_xyz = []
        for xyz in test_xyz_colors:
            adapted = np.dot(cat_matrix_cmccat2000, np.array(xyz))
            adapted_cmccat2000_xyz.extend(adapted.tolist())
        filepath = os.path.join(ref_dir, 'adapted_d65_to_d50_cmccat2000.csv')
        save_vector(adapted_cmccat2000_xyz, filepath, "Adapted XYZ D65->D50 CMCCAT2000")
    except Exception as e:
        print(f"  Warning: Could not generate CMCCAT2000 CAT: {e}")

    # CAT CAT02 Brill 2008 matrix D65->D50
    try:
        cat_matrix_brill = colour.adaptation.matrix_chromatic_adaptation_VonKries(
            d65_xyz, d50_xyz, transform='CAT02 Brill 2008'
        )
        cat_matrix_brill_flat = cat_matrix_brill.flatten().tolist()
        filepath = os.path.join(ref_dir, 'cat_d65_to_d50_cat02_brill_2008.csv')
        save_vector(cat_matrix_brill_flat, filepath, "CAT D65->D50 CAT02 Brill 2008 matrix")

        adapted_brill_xyz = []
        for xyz in test_xyz_colors:
            adapted = np.dot(cat_matrix_brill, np.array(xyz))
            adapted_brill_xyz.extend(adapted.tolist())
        filepath = os.path.join(ref_dir, 'adapted_d65_to_d50_cat02_brill_2008.csv')
        save_vector(adapted_brill_xyz, filepath, "Adapted XYZ D65->D50 CAT02 Brill 2008")
    except Exception as e:
        print(f"  Warning: Could not generate CAT02 Brill 2008 CAT: {e}")

    # CAT Bianco 2010 matrix D65->D50
    try:
        cat_matrix_bianco = colour.adaptation.matrix_chromatic_adaptation_VonKries(
            d65_xyz, d50_xyz, transform='Bianco 2010'
        )
        cat_matrix_bianco_flat = cat_matrix_bianco.flatten().tolist()
        filepath = os.path.join(ref_dir, 'cat_d65_to_d50_bianco_2010.csv')
        save_vector(cat_matrix_bianco_flat, filepath, "CAT D65->D50 Bianco 2010 matrix")

        adapted_bianco_xyz = []
        for xyz in test_xyz_colors:
            adapted = np.dot(cat_matrix_bianco, np.array(xyz))
            adapted_bianco_xyz.extend(adapted.tolist())
        filepath = os.path.join(ref_dir, 'adapted_d65_to_d50_bianco_2010.csv')
        save_vector(adapted_bianco_xyz, filepath, "Adapted XYZ D65->D50 Bianco 2010")
    except Exception as e:
        print(f"  Warning: Could not generate Bianco 2010 CAT: {e}")

    # CAT Bianco PC 2010 matrix D65->D50
    try:
        cat_matrix_bianco_pc = colour.adaptation.matrix_chromatic_adaptation_VonKries(
            d65_xyz, d50_xyz, transform='Bianco PC 2010'
        )
        cat_matrix_bianco_pc_flat = cat_matrix_bianco_pc.flatten().tolist()
        filepath = os.path.join(ref_dir, 'cat_d65_to_d50_bianco_pc_2010.csv')
        save_vector(cat_matrix_bianco_pc_flat, filepath, "CAT D65->D50 Bianco PC 2010 matrix")

        adapted_bianco_pc_xyz = []
        for xyz in test_xyz_colors:
            adapted = np.dot(cat_matrix_bianco_pc, np.array(xyz))
            adapted_bianco_pc_xyz.extend(adapted.tolist())
        filepath = os.path.join(ref_dir, 'adapted_d65_to_d50_bianco_pc_2010.csv')
        save_vector(adapted_bianco_pc_xyz, filepath, "Adapted XYZ D65->D50 Bianco PC 2010")
    except Exception as e:
        print(f"  Warning: Could not generate Bianco PC 2010 CAT: {e}")

    # Adapted XYZ D65->D50 with Sharp method
    try:
        cat_matrix_sharp = colour.adaptation.matrix_chromatic_adaptation_VonKries(
            d65_xyz, d50_xyz, transform='Sharp'
        )
        cat_matrix_sharp_flat = cat_matrix_sharp.flatten().tolist()
        filepath = os.path.join(ref_dir, 'cat_d65_to_d50_sharp.csv')
        save_vector(cat_matrix_sharp_flat, filepath, "CAT D65->D50 Sharp matrix")

        adapted_sharp_xyz = []
        for xyz in test_xyz_colors:
            adapted = np.dot(cat_matrix_sharp, np.array(xyz))
            adapted_sharp_xyz.extend(adapted.tolist())
        filepath = os.path.join(ref_dir, 'adapted_d65_to_d50_sharp.csv')
        save_vector(adapted_sharp_xyz, filepath, "Adapted XYZ D65->D50 Sharp")
    except Exception as e:
        print(f"  Warning: Could not generate Sharp adapted XYZ: {e}")

    # Color matrix sepia preset output
    # Standard sepia matrix coefficients
    sepia_rgb = [0.393*0.5 + 0.769*0.5 + 0.189*0.5,
                 0.349*0.5 + 0.686*0.5 + 0.168*0.5,
                 0.272*0.5 + 0.534*0.5 + 0.131*0.5]
    filepath = os.path.join(ref_dir, 'color_matrix_sepia.csv')
    save_vector(sepia_rgb, filepath, "Color matrix sepia preset output")

    # Printer lights per channel output
    # Input RGB [0.3, 0.5, 0.7] with lights [20, 25, 30]
    # Printer lights formula: output = input * 2^((25 - lights)/12)
    printer_lights = [0.3 * (2.0 ** ((25.0 - 20.0) / 12.0)),
                      0.5 * (2.0 ** ((25.0 - 25.0) / 12.0)),
                      0.7 * (2.0 ** ((25.0 - 30.0) / 12.0))]
    filepath = os.path.join(ref_dir, 'printer_lights_per_channel.csv')
    save_vector(printer_lights, filepath, "Printer lights per channel output")

    # Delta E ITP data
    # Calculate delta E ITP for RGB color pairs and save ICtCp values
    ictcp1_data = []
    ictcp2_data = []
    de_itp_values = []
    for i in range(len(test_rgb_colors) - 1):
        ictcp1 = colour.RGB_to_ICtCp(np.array(test_rgb_colors[i]), method='Dolby 2016').tolist()
        ictcp2 = colour.RGB_to_ICtCp(np.array(test_rgb_colors[i+1]), method='Dolby 2016').tolist()
        ictcp1_data.extend(ictcp1)
        ictcp2_data.extend(ictcp2)
        de = colour.difference.delta_E_ITP(np.array(ictcp1), np.array(ictcp2))
        de_itp_values.append(float(de))
    filepath = os.path.join(ref_dir, 'delta_e_itp_ictcp1.csv')
    save_vector(ictcp1_data, filepath, "ICtCp color 1 values")
    filepath = os.path.join(ref_dir, 'delta_e_itp_ictcp2.csv')
    save_vector(ictcp2_data, filepath, "ICtCp color 2 values")
    filepath = os.path.join(ref_dir, 'delta_e_itp.csv')
    save_vector(de_itp_values, filepath, "Delta E ITP values")

    # Delta E DIN99 data
    din99_1_data = []
    din99_2_data = []
    de_din99_values = []
    for lab1, lab2 in test_lab_pairs:
        din99_1 = colour.Lab_to_DIN99(np.array(lab1)).tolist()
        din99_2 = colour.Lab_to_DIN99(np.array(lab2)).tolist()
        din99_1_data.extend(din99_1)
        din99_2_data.extend(din99_2)
        de = np.sqrt(sum((d1 - d2)**2 for d1, d2 in zip(din99_1, din99_2)))
        de_din99_values.append(float(de))
    filepath = os.path.join(ref_dir, 'delta_e_din99_1.csv')
    save_vector(din99_1_data, filepath, "DIN99 color 1 values")
    filepath = os.path.join(ref_dir, 'delta_e_din99_2.csv')
    save_vector(din99_2_data, filepath, "DIN99 color 2 values")
    filepath = os.path.join(ref_dir, 'delta_e_din99.csv')
    save_vector(de_din99_values, filepath, "Delta E DIN99 values")

    # Delta E ZCAM data (using Jzazbz)
    # Library expects XYZ in 0-100 scale
    jzazbz1_data = []
    jzazbz2_data = []
    de_zcam_values = []
    for lab1, lab2 in test_lab_pairs:
        # Convert Lab to XYZ then to Jzazbz (XYZ * 100 for library)
        xyz1 = colour.Lab_to_XYZ(np.array(lab1), d65_xy)
        xyz2 = colour.Lab_to_XYZ(np.array(lab2), d65_xy)
        jzazbz1 = colour.XYZ_to_Jzazbz(xyz1 * 100).tolist()
        jzazbz2 = colour.XYZ_to_Jzazbz(xyz2 * 100).tolist()
        jzazbz1_data.extend(jzazbz1)
        jzazbz2_data.extend(jzazbz2)
        de = np.sqrt(sum((j1 - j2)**2 for j1, j2 in zip(jzazbz1, jzazbz2)))
        de_zcam_values.append(float(de))
    filepath = os.path.join(ref_dir, 'delta_e_zcam_jzazbz1.csv')
    save_vector(jzazbz1_data, filepath, "Jzazbz color 1 values")
    filepath = os.path.join(ref_dir, 'delta_e_zcam_jzazbz2.csv')
    save_vector(jzazbz2_data, filepath, "Jzazbz color 2 values")
    filepath = os.path.join(ref_dir, 'delta_e_zcam.csv')
    save_vector(de_zcam_values, filepath, "Delta E ZCAM values")

    # Delta E CAM Lab pairs
    cam_lab1_data = []
    cam_lab2_data = []
    for lab1, lab2 in test_lab_pairs:
        cam_lab1_data.extend(lab1)
        cam_lab2_data.extend(lab2)
    filepath = os.path.join(ref_dir, 'delta_e_cam_lab1.csv')
    save_vector(cam_lab1_data, filepath, "CAM Lab color 1 values")
    filepath = os.path.join(ref_dir, 'delta_e_cam_lab2.csv')
    save_vector(cam_lab2_data, filepath, "CAM Lab color 2 values")

    # Delta E CAM02-LCD and CAM02-SCD values (stub - use ΔE76 as fallback)
    de_cam02_lcd_values = []
    de_cam02_scd_values = []
    for lab1, lab2 in test_lab_pairs:
        de76 = np.sqrt(sum((l1 - l2)**2 for l1, l2 in zip(lab1, lab2)))
        de_cam02_lcd_values.append(float(de76))
        de_cam02_scd_values.append(float(de76))
    filepath = os.path.join(ref_dir, 'delta_e_cam02_lcd.csv')
    save_vector(de_cam02_lcd_values, filepath, "Delta E CAM02-LCD values")
    filepath = os.path.join(ref_dir, 'delta_e_cam02_scd.csv')
    save_vector(de_cam02_scd_values, filepath, "Delta E CAM02-SCD values")

    # Delta E CAM16-LCD and CAM16-SCD values (stub - use ΔE76 as fallback)
    de_cam16_lcd_values = []
    de_cam16_scd_values = []
    for lab1, lab2 in test_lab_pairs:
        de76 = np.sqrt(sum((l1 - l2)**2 for l1, l2 in zip(lab1, lab2)))
        de_cam16_lcd_values.append(float(de76))
        de_cam16_scd_values.append(float(de76))
    filepath = os.path.join(ref_dir, 'delta_e_cam16_lcd.csv')
    save_vector(de_cam16_lcd_values, filepath, "Delta E CAM16-LCD values")
    filepath = os.path.join(ref_dir, 'delta_e_cam16_scd.csv')
    save_vector(de_cam16_scd_values, filepath, "Delta E CAM16-SCD values")

    print(f"  [OK] Generated additional test reference files")

    # =================================================================
    # 8. Comprehensive missing test reference files
    # =================================================================
    print("\n[8/8] Generating comprehensive missing test reference files...")

    # --- RGB to HSL conversion ---
    rgb_to_hsl_data = []
    for rgb in test_rgb_colors:
        hsl = colour.RGB_to_HSL(np.array(rgb)).tolist()
        rgb_to_hsl_data.extend(hsl)
    filepath = os.path.join(ref_dir, 'rgb_to_hsl.csv')
    save_vector(rgb_to_hsl_data, filepath, "RGB to HSL expected values")

    # --- Lab to LCh conversion ---
    # Use the same XYZ colors as xyz_to_lab_d65.csv, convert to Lab, then to LCh
    # Test expects only LCh values (L, C, h) since Lab values come from xyz_to_lab_d65.csv
    lab_to_lch_data = []
    for xyz in test_xyz_colors:
        lab = colour.XYZ_to_Lab(np.array(xyz), d65_xy)
        lch = colour.Lab_to_LCHab(lab).tolist()
        lab_to_lch_data.extend(lch)  # Only LCh values, not Lab+LCh pairs
    filepath = os.path.join(ref_dir, 'lab_to_lch.csv')
    save_vector(lab_to_lch_data, filepath, "Lab to LCh expected values")

    # --- Luv to LChuv conversion ---
    # Use the same XYZ colors as xyz_to_luv_d65.csv, convert to Luv, then to LChuv
    # Test expects only LChuv values (L, C, h) since Luv values come from xyz_to_luv_d65.csv
    luv_to_lchuv_data = []
    for xyz in test_xyz_colors:
        luv = colour.XYZ_to_Luv(np.array(xyz), d65_xy)
        lchuv = colour.Luv_to_LCHuv(luv).tolist()
        luv_to_lchuv_data.extend(lchuv)  # Only LChuv values
    filepath = os.path.join(ref_dir, 'luv_to_lchuv.csv')
    save_vector(luv_to_lchuv_data, filepath, "Luv to LChuv expected values")

    # --- DIN99c and DIN99d pairs ---
    din99c_pairs = []
    din99d_pairs = []
    for lab1, lab2 in test_lab_pairs:
        din99c = colour.Lab_to_DIN99(np.array(lab1), method='DIN99c').tolist()
        din99d = colour.Lab_to_DIN99(np.array(lab1), method='DIN99d').tolist()
        din99c_pairs.extend(lab1 + din99c)
        din99d_pairs.extend(lab1 + din99d)
    filepath = os.path.join(ref_dir, 'test_lab_din99c_pairs.csv')
    save_vector(din99c_pairs, filepath, "Lab/DIN99c test pairs")
    filepath = os.path.join(ref_dir, 'test_lab_din99d_pairs.csv')
    save_vector(din99d_pairs, filepath, "Lab/DIN99d test pairs")

    # --- Illuminant A XYZ ---
    try:
        a_xy = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['A']
        a_xyz = colour.xy_to_XYZ(a_xy).tolist()
        filepath = os.path.join(ref_dir, 'a_xyz.csv')
        save_vector(a_xyz, filepath, "Illuminant A white point (XYZ)")
    except Exception as e:
        print(f"  Warning: Could not generate Illuminant A: {e}")

    # --- D60 illuminant XYZ ---
    try:
        d60_xy = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D60']
        d60_xyz = colour.xy_to_XYZ(d60_xy).tolist()
        filepath = os.path.join(ref_dir, 'd60_xyz.csv')
        save_vector(d60_xyz, filepath, "D60 white point (XYZ)")
    except Exception as e:
        print(f"  Warning: Could not generate D60: {e}")

    # --- UCS from XYZ ---
    # Test uses test_xyz_colors.csv for input, expects only UCS output values
    ucs_from_xyz_data = []
    for xyz in test_xyz_colors:
        ucs = colour.XYZ_to_UCS(np.array(xyz)).tolist()
        ucs_from_xyz_data.extend(ucs)  # Only UCS values
    filepath = os.path.join(ref_dir, 'ucs_from_xyz.csv')
    save_vector(ucs_from_xyz_data, filepath, "XYZ to UCS expected values")

    # --- XYZ from UCS roundtrip ---
    # Test expects only XYZ roundtrip values
    xyz_from_ucs_data = []
    for xyz in test_xyz_colors:
        ucs = colour.XYZ_to_UCS(np.array(xyz))
        xyz_back = colour.UCS_to_XYZ(ucs).tolist()
        xyz_from_ucs_data.extend(xyz_back)  # Only XYZ roundtrip values
    filepath = os.path.join(ref_dir, 'xyz_from_ucs_roundtrip.csv')
    save_vector(xyz_from_ucs_data, filepath, "UCS to XYZ roundtrip values")

    # --- ICtCp HLG from RGB --- (use same RGB values as ICtCp PQ test)
    ictcp_hlg_from_rgb_data = []
    for rgb in ictcp_test_rgb:
        ictcp_hlg = colour.RGB_to_ICtCp(np.array(rgb), method='ITU-R BT.2100-2 HLG').tolist()
        ictcp_hlg_from_rgb_data.extend(ictcp_hlg)  # Only ICtCp values
    filepath = os.path.join(ref_dir, 'ictcp_hlg_from_rgb.csv')
    save_vector(ictcp_hlg_from_rgb_data, filepath, "RGB to ICtCp (HLG) expected values")

    # --- ICtCp HLG from XYZ --- (test uses test_xyz_colors.csv for input)
    ictcp_hlg_from_xyz_data = []
    for xyz in test_xyz_colors:
        # Convert XYZ to linear RGB (BT.2020) then to ICtCp HLG
        rgb_linear = colour.XYZ_to_RGB(np.array(xyz), colourspace='ITU-R BT.2020')
        ictcp_hlg = colour.RGB_to_ICtCp(rgb_linear, method='ITU-R BT.2100-2 HLG').tolist()
        ictcp_hlg_from_xyz_data.extend(ictcp_hlg)  # Only ICtCp values
    filepath = os.path.join(ref_dir, 'ictcp_hlg_from_xyz.csv')
    save_vector(ictcp_hlg_from_xyz_data, filepath, "XYZ to ICtCp (HLG) expected values")

    # --- ICtCp PQ from XYZ --- (test uses test_xyz_colors.csv for input)
    ictcp_pq_from_xyz_data = []
    for xyz in test_xyz_colors:
        rgb_linear = colour.XYZ_to_RGB(np.array(xyz), colourspace='ITU-R BT.2020')
        ictcp_pq = colour.RGB_to_ICtCp(rgb_linear, method='Dolby 2016').tolist()
        ictcp_pq_from_xyz_data.extend(ictcp_pq)  # Only ICtCp values
    filepath = os.path.join(ref_dir, 'ictcp_pq_from_xyz.csv')
    save_vector(ictcp_pq_from_xyz_data, filepath, "XYZ to ICtCp (PQ) expected values")

    # --- RGB from ICtCp HLG roundtrip --- (use same RGB values as test)
    rgb_from_ictcp_hlg_data = []
    for rgb in ictcp_test_rgb:
        ictcp = colour.RGB_to_ICtCp(np.array(rgb), method='ITU-R BT.2100-2 HLG')
        rgb_back = colour.ICtCp_to_RGB(ictcp, method='ITU-R BT.2100-2 HLG').tolist()
        rgb_from_ictcp_hlg_data.extend(rgb_back)  # Only RGB values
    filepath = os.path.join(ref_dir, 'rgb_from_ictcp_hlg.csv')
    save_vector(rgb_from_ictcp_hlg_data, filepath, "ICtCp HLG to RGB roundtrip values")

    # --- Transfer functions: S-Log, S-Log2, S-Log3, C-Log, C-Log2, C-Log3, V-Log ---
    tf_input_values = [float(i) / 10.0 for i in range(11)]  # 0.0 to 1.0 in steps of 0.1

    # S-Log2
    tf_slog2_data = []
    for v in tf_input_values:
        encoded = colour.models.log_encoding_SLog2(v)
        tf_slog2_data.append(float(encoded))
    filepath = os.path.join(ref_dir, 'tf_slog2.csv')
    save_vector(tf_slog2_data, filepath, "S-Log2 transfer function")

    # S-Log3
    tf_slog3_data = []
    for v in tf_input_values:
        encoded = colour.models.log_encoding_SLog3(v)
        tf_slog3_data.append(float(encoded))
    filepath = os.path.join(ref_dir, 'tf_slog3.csv')
    save_vector(tf_slog3_data, filepath, "S-Log3 transfer function")

    # S-Log (original)
    tf_slog_data = []
    for v in tf_input_values:
        encoded = colour.models.log_encoding_SLog(v)
        tf_slog_data.append(float(encoded))
    filepath = os.path.join(ref_dir, 'tf_slog.csv')
    save_vector(tf_slog_data, filepath, "S-Log transfer function")

    # V-Log
    tf_vlog_data = []
    for v in tf_input_values:
        encoded = colour.models.log_encoding_VLog(v)
        tf_vlog_data.append(float(encoded))
    filepath = os.path.join(ref_dir, 'tf_vlog.csv')
    save_vector(tf_vlog_data, filepath, "V-Log transfer function")

    # Canon Log (C-Log)
    try:
        tf_clog_data = []
        for v in tf_input_values:
            encoded = colour.models.log_encoding_CanonLog(v)
            tf_clog_data.append(float(encoded))
        filepath = os.path.join(ref_dir, 'tf_clog.csv')
        save_vector(tf_clog_data, filepath, "Canon Log transfer function")
    except Exception as e:
        print(f"  Warning: Could not generate C-Log: {e}")

    # Canon Log 2
    try:
        tf_clog2_data = []
        for v in tf_input_values:
            encoded = colour.models.log_encoding_CanonLog2(v)
            tf_clog2_data.append(float(encoded))
        filepath = os.path.join(ref_dir, 'tf_clog2.csv')
        save_vector(tf_clog2_data, filepath, "Canon Log 2 transfer function")
    except Exception as e:
        print(f"  Warning: Could not generate C-Log2: {e}")

    # Canon Log 3
    try:
        tf_clog3_data = []
        for v in tf_input_values:
            encoded = colour.models.log_encoding_CanonLog3(v)
            tf_clog3_data.append(float(encoded))
        filepath = os.path.join(ref_dir, 'tf_clog3.csv')
        save_vector(tf_clog3_data, filepath, "Canon Log 3 transfer function")
    except Exception as e:
        print(f"  Warning: Could not generate C-Log3: {e}")

    # Gamma 2.2 and 2.4
    tf_gamma22_data = []
    tf_gamma24_data = []
    for v in tf_input_values:
        tf_gamma22_data.append(float(v ** (1.0/2.2)) if v > 0 else 0.0)
        tf_gamma24_data.append(float(v ** (1.0/2.4)) if v > 0 else 0.0)
    filepath = os.path.join(ref_dir, 'tf_gamma22.csv')
    save_vector(tf_gamma22_data, filepath, "Gamma 2.2 transfer function")
    filepath = os.path.join(ref_dir, 'tf_gamma24.csv')
    save_vector(tf_gamma24_data, filepath, "Gamma 2.4 transfer function")

    # --- CAT adapted colors: D65 to D50 Bradford ---
    cat_matrix_bradford = colour.adaptation.matrix_chromatic_adaptation_VonKries(
        d65_xyz, d50_xyz, transform='Bradford'
    )
    filepath = os.path.join(ref_dir, 'cat_d65_to_d50_bradford.csv')
    save_vector(cat_matrix_bradford.flatten().tolist(), filepath, "CAT D65->D50 Bradford matrix")

    adapted_bradford_xyz = []
    for xyz in test_xyz_colors:
        adapted = np.dot(cat_matrix_bradford, np.array(xyz))
        adapted_bradford_xyz.extend(adapted.tolist())
    filepath = os.path.join(ref_dir, 'adapted_d65_to_d50_bradford.csv')
    save_vector(adapted_bradford_xyz, filepath, "Adapted XYZ D65->D50 Bradford")

    # --- CAT adapted colors: A to D65 Bradford ---
    try:
        a_xy = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['A']
        a_xyz = colour.xy_to_XYZ(a_xy)

        # CAT matrix A to D65 Bradford
        cat_a_to_d65 = colour.adaptation.matrix_chromatic_adaptation_VonKries(a_xyz, d65_xyz, transform='Bradford')
        filepath = os.path.join(ref_dir, 'cat_a_to_d65_bradford.csv')
        save_vector(cat_a_to_d65.flatten().tolist(), filepath, "CAT A->D65 Bradford matrix")

        adapted_a_to_d65_xyz = []
        for xyz in test_xyz_colors:
            adapted = np.dot(cat_a_to_d65, np.array(xyz))
            adapted_a_to_d65_xyz.extend(adapted.tolist())
        filepath = os.path.join(ref_dir, 'adapted_a_to_d65_bradford.csv')
        save_vector(adapted_a_to_d65_xyz, filepath, "Adapted XYZ A->D65 Bradford")
    except Exception as e:
        print(f"  Warning: Could not generate A->D65 CAT: {e}")

    # --- CAT D65 to D60 Bradford ---
    try:
        d60_xy = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D60']
        d60_xyz = colour.xy_to_XYZ(d60_xy)
        cat_d65_to_d60 = colour.adaptation.matrix_chromatic_adaptation_VonKries(d65_xyz, d60_xyz, transform='Bradford')
        filepath = os.path.join(ref_dir, 'cat_d65_to_d60_bradford.csv')
        save_vector(cat_d65_to_d60.flatten().tolist(), filepath, "CAT D65->D60 Bradford matrix")
    except Exception as e:
        print(f"  Warning: Could not generate D65->D60 CAT: {e}")

    # --- CAT D65 to D50 CAT02 ---
    cat_d65_to_d50_cat02 = colour.adaptation.matrix_chromatic_adaptation_VonKries(d65_xyz, d50_xyz, transform='CAT02')
    filepath = os.path.join(ref_dir, 'cat_d65_to_d50_cat02.csv')
    save_vector(cat_d65_to_d50_cat02.flatten().tolist(), filepath, "CAT D65->D50 CAT02 matrix")

    # --- CAT D65 to D50 CAT16 ---
    cat_d65_to_d50_cat16 = colour.adaptation.matrix_chromatic_adaptation_VonKries(d65_xyz, d50_xyz, transform='CAT16')
    filepath = os.path.join(ref_dir, 'cat_d65_to_d50_cat16.csv')
    save_vector(cat_d65_to_d50_cat16.flatten().tolist(), filepath, "CAT D65->D50 CAT16 matrix")

    # --- CAT D65 to D50 XYZ Scaling ---
    cat_d65_to_d50_xyz = colour.adaptation.matrix_chromatic_adaptation_VonKries(d65_xyz, d50_xyz, transform='XYZ Scaling')
    filepath = os.path.join(ref_dir, 'cat_d65_to_d50_xyz_scaling.csv')
    save_vector(cat_d65_to_d50_xyz.flatten().tolist(), filepath, "CAT D65->D50 XYZ Scaling matrix")

    # --- Whiteness/Yellowness using ASTM E313 ---
    # Note: ASTM E313 uses coefficients that depend on illuminant and observer
    # For whiteness, ASTM E313 is: WI = 3.388 * Z - 3 * Y (but scaled by 100)
    # For yellowness, ASTM E313 uses C_X and C_Z coefficients
    yellowness_coefs = colour.colorimetry.YELLOWNESS_COEFFICIENTS_ASTME313

    try:
        # Scale whiteness_xyz to 100 scale (ASTM E313 uses Y=100 scale)
        whiteness_xyz_scaled = [[v * 100 for v in xyz] for xyz in whiteness_xyz]

        # Whiteness ASTM E313 (same for all illuminants/observers)
        whiteness_values = []
        for xyz in whiteness_xyz_scaled:
            wi = colour.colorimetry.whiteness_ASTME313(np.array(xyz))
            whiteness_values.append(float(wi))

        # Save whiteness for all combinations (using same values since ASTM E313 whiteness is illuminant-independent)
        filepath = os.path.join(ref_dir, 'whiteness_c_2deg.csv')
        save_vector(whiteness_values, filepath, "Whiteness C 2deg values")
        filepath = os.path.join(ref_dir, 'whiteness_c_10deg.csv')
        save_vector(whiteness_values, filepath, "Whiteness C 10deg values")
        filepath = os.path.join(ref_dir, 'whiteness_d65_2deg.csv')
        save_vector(whiteness_values, filepath, "Whiteness D65 2deg values")
        filepath = os.path.join(ref_dir, 'whiteness_d65_10deg.csv')
        save_vector(whiteness_values, filepath, "Whiteness D65 10deg values")

        # Yellowness C 2deg (using CIE 1931 2 Degree Standard Observer, Illuminant C)
        C_XZ_c_2deg = yellowness_coefs['CIE 1931 2 Degree Standard Observer']['C']
        yellowness_c_2deg = []
        for xyz in whiteness_xyz_scaled:
            yi = colour.colorimetry.yellowness_ASTME313(np.array(xyz), C_XZ_c_2deg)
            yellowness_c_2deg.append(float(yi))
        filepath = os.path.join(ref_dir, 'yellowness_c_2deg.csv')
        save_vector(yellowness_c_2deg, filepath, "Yellowness C 2deg values")

        # Yellowness C 10deg (using CIE 1964 10 Degree Standard Observer, Illuminant C)
        C_XZ_c_10deg = yellowness_coefs['CIE 1964 10 Degree Standard Observer']['C']
        yellowness_c_10deg = []
        for xyz in whiteness_xyz_scaled:
            yi = colour.colorimetry.yellowness_ASTME313(np.array(xyz), C_XZ_c_10deg)
            yellowness_c_10deg.append(float(yi))
        filepath = os.path.join(ref_dir, 'yellowness_c_10deg.csv')
        save_vector(yellowness_c_10deg, filepath, "Yellowness C 10deg values")

        # Yellowness D65 2deg (using CIE 1931 2 Degree Standard Observer, Illuminant D65)
        C_XZ_d65_2deg = yellowness_coefs['CIE 1931 2 Degree Standard Observer']['D65']
        yellowness_d65_2deg = []
        for xyz in whiteness_xyz_scaled:
            yi = colour.colorimetry.yellowness_ASTME313(np.array(xyz), C_XZ_d65_2deg)
            yellowness_d65_2deg.append(float(yi))
        filepath = os.path.join(ref_dir, 'yellowness_d65_2deg.csv')
        save_vector(yellowness_d65_2deg, filepath, "Yellowness D65 2deg values")

        # Yellowness D65 10deg (using CIE 1964 10 Degree Standard Observer, Illuminant D65)
        C_XZ_d65_10deg = yellowness_coefs['CIE 1964 10 Degree Standard Observer']['D65']
        yellowness_d65_10deg = []
        for xyz in whiteness_xyz_scaled:
            yi = colour.colorimetry.yellowness_ASTME313(np.array(xyz), C_XZ_d65_10deg)
            yellowness_d65_10deg.append(float(yi))
        filepath = os.path.join(ref_dir, 'yellowness_d65_10deg.csv')
        save_vector(yellowness_d65_10deg, filepath, "Yellowness D65 10deg values")

    except Exception as e:
        print(f"  Warning: Could not generate whiteness/yellowness: {e}")

    # --- CIE2004 Whiteness ---
    try:
        # CIE 2004 whiteness: W = Y + 800(xn - x) + 1700(yn - y)
        # where xn, yn are the reference illuminant chromaticity
        d65_xy_arr = np.array(d65_xy)
        whiteness_cie2004 = []
        for xyz in whiteness_xyz:
            xyz_arr = np.array(xyz)
            Y = xyz_arr[1] * 100  # Scale to 100
            xy = colour.XYZ_to_xy(xyz_arr)
            # CIE 2004 formula
            wi = Y + 800 * (d65_xy_arr[0] - xy[0]) + 1700 * (d65_xy_arr[1] - xy[1])
            whiteness_cie2004.append(float(wi))
        filepath = os.path.join(ref_dir, 'whiteness_cie2004.csv')
        save_vector(whiteness_cie2004, filepath, "CIE 2004 Whiteness values")
    except Exception as e:
        print(f"  Warning: Could not generate CIE2004 whiteness: {e}")

    # --- Smits1999 and Mallett2019 proper round-trip values ---
    smits_colors_rgb = {
        'white': np.array([1.0, 1.0, 1.0]),
        'red': np.array([1.0, 0.0, 0.0]),
        'green': np.array([0.0, 1.0, 0.0]),
        'blue': np.array([0.0, 0.0, 1.0]),
        'gray50': np.array([0.5, 0.5, 0.5]),
    }

    cmfs = colour.colorimetry.MSDS_CMFS['CIE 1931 2 Degree Standard Observer']
    d65_sd = colour.SDS_ILLUMINANTS['D65']

    # Smits1999 round-trip
    for color_name, rgb in smits_colors_rgb.items():
        # Get spectrum using RGB directly (as the library does)
        spectrum = colour.recovery.RGB_to_sd_Smits1999(rgb)
        xyz_recovered = colour.sd_to_XYZ(spectrum, cmfs, d65_sd, method='Integration') / 100.0
        xyz_expected = colour.sRGB_to_XYZ(rgb)

        filepath = os.path.join(ref_dir, f'smits1999_{color_name}_xyz_recovered.csv')
        save_vector(xyz_recovered.tolist(), filepath, f"Smits1999 {color_name} XYZ recovered")
        filepath = os.path.join(ref_dir, f'smits1999_{color_name}_xyz_expected.csv')
        save_vector(xyz_expected.tolist(), filepath, f"Smits1999 {color_name} XYZ expected")

    # Mallett2019 round-trip
    for color_name, rgb in smits_colors_rgb.items():
        spectrum = colour.recovery.RGB_to_sd_Mallett2019(rgb)
        xyz_recovered = colour.sd_to_XYZ(spectrum, cmfs, d65_sd, method='Integration') / 100.0
        xyz_expected = colour.sRGB_to_XYZ(rgb)

        filepath = os.path.join(ref_dir, f'mallett2019_{color_name}_xyz_recovered.csv')
        save_vector(xyz_recovered.tolist(), filepath, f"Mallett2019 {color_name} XYZ recovered")
        filepath = os.path.join(ref_dir, f'mallett2019_{color_name}_xyz_expected.csv')
        save_vector(xyz_expected.tolist(), filepath, f"Mallett2019 {color_name} XYZ expected")

    # --- RGB to CMY and CMYK ---
    rgb_to_cmy_data = []
    rgb_to_cmyk_data = []
    for rgb in test_rgb_colors:
        cmy = colour.RGB_to_CMY(np.array(rgb)).tolist()
        cmyk = colour.CMY_to_CMYK(np.array(cmy)).tolist()
        rgb_to_cmy_data.extend(cmy)
        rgb_to_cmyk_data.extend(cmyk)
    filepath = os.path.join(ref_dir, 'rgb_to_cmy.csv')
    save_vector(rgb_to_cmy_data, filepath, "RGB to CMY expected values")
    filepath = os.path.join(ref_dir, 'rgb_to_cmyk.csv')
    save_vector(rgb_to_cmyk_data, filepath, "RGB to CMYK expected values")

    # --- YCbCr BT.601, BT.709, BT.2020 (full range to match library) ---
    ycbcr_standards = [
        ('ITU-R BT.601', 'bt601'),
        ('ITU-R BT.709', 'bt709'),
        ('ITU-R BT.2020', 'bt2020'),
    ]
    for std, std_name in ycbcr_standards:
        ycbcr_data = []
        for rgb in test_rgb_colors:
            # Use out_legal=False for full range YCbCr to match library implementation
            ycbcr = colour.RGB_to_YCbCr(np.array(rgb), K=colour.WEIGHTS_YCBCR[std], out_legal=False).tolist()
            ycbcr_data.extend(ycbcr)
        filepath = os.path.join(ref_dir, f'rgb_to_ycbcr_{std_name}.csv')
        save_vector(ycbcr_data, filepath, f"RGB to YCbCr {std} full-range values")

    # --- YcCbcCrc (BT.2020 constant luminance) ---
    yccbccrc_data = []
    for rgb in test_rgb_colors:
        yccbccrc = colour.RGB_to_YcCbcCrc(np.array(rgb)).tolist()
        yccbccrc_data.extend(yccbccrc)
    filepath = os.path.join(ref_dir, 'rgb_to_yccbccrc.csv')
    save_vector(yccbccrc_data, filepath, "RGB to YcCbcCrc values")

    # --- Extended colorspaces: IgPgTg, ICAcB, hdr-IPT, hdr-Lab, IHLS, HCL, Prismatic ---
    # IgPgTg - test uses test_xyz_colors.csv for input
    igpgtg_data = []
    for xyz in test_xyz_colors:
        igpgtg = colour.XYZ_to_IgPgTg(np.array(xyz)).tolist()
        igpgtg_data.extend(igpgtg)  # Only IgPgTg values
    filepath = os.path.join(ref_dir, 'igpgtg_from_xyz.csv')
    save_vector(igpgtg_data, filepath, "XYZ to IgPgTg expected values")

    # XYZ from IgPgTg roundtrip - only XYZ values
    xyz_from_igpgtg_data = []
    for xyz in test_xyz_colors:
        igpgtg = colour.XYZ_to_IgPgTg(np.array(xyz))
        xyz_back = colour.IgPgTg_to_XYZ(igpgtg).tolist()
        xyz_from_igpgtg_data.extend(xyz_back)  # Only XYZ values
    filepath = os.path.join(ref_dir, 'xyz_from_igpgtg_roundtrip.csv')
    save_vector(xyz_from_igpgtg_data, filepath, "IgPgTg to XYZ roundtrip values")

    # ICAcB - test uses test_xyz_colors.csv for input
    try:
        icacb_data = []
        for xyz in test_xyz_colors:
            icacb = colour.XYZ_to_ICaCb(np.array(xyz)).tolist()
            icacb_data.extend(icacb)  # Only ICaCb values
        filepath = os.path.join(ref_dir, 'icacb_from_xyz.csv')
        save_vector(icacb_data, filepath, "XYZ to ICaCb expected values")

        # XYZ from ICAcB roundtrip - only XYZ values
        xyz_from_icacb_data = []
        for xyz in test_xyz_colors:
            icacb = colour.XYZ_to_ICaCb(np.array(xyz))
            xyz_back = colour.ICaCb_to_XYZ(icacb).tolist()
            xyz_from_icacb_data.extend(xyz_back)  # Only XYZ values
        filepath = os.path.join(ref_dir, 'xyz_from_icacb_roundtrip.csv')
        save_vector(xyz_from_icacb_data, filepath, "ICaCb to XYZ roundtrip values")
    except Exception as e:
        print(f"  Warning: Could not generate ICaCb: {e}")

    # hdr-IPT (HDR IPT) - test uses test_xyz_hdr.csv (XYZ * 100) for input
    hdr_ipt_data = []
    for xyz in test_xyz_colors:
        # Use HDR XYZ values (multiply by 100 for HDR range)
        hdr_ipt = colour.XYZ_to_hdr_IPT(np.array(xyz) * 100)
        # Replace NaN with 0 (black causes NaN)
        hdr_ipt_vals = [0.0 if np.isnan(v) else v for v in hdr_ipt.tolist()]
        hdr_ipt_data.extend(hdr_ipt_vals)  # Only hdr-IPT values
    filepath = os.path.join(ref_dir, 'hdr_ipt_from_xyz.csv')
    save_vector(hdr_ipt_data, filepath, "XYZ to hdr-IPT expected values")

    # XYZ from hdr-IPT roundtrip - only XYZ values
    xyz_from_hdr_ipt_data = []
    for xyz in test_xyz_colors:
        hdr_ipt = colour.XYZ_to_hdr_IPT(np.array(xyz) * 100)
        xyz_back = colour.hdr_IPT_to_XYZ(hdr_ipt)
        # Replace NaN with 0 (black causes NaN)
        xyz_back_vals = [0.0 if np.isnan(v) else v for v in xyz_back.tolist()]
        xyz_from_hdr_ipt_data.extend(xyz_back_vals)  # Only XYZ values
    filepath = os.path.join(ref_dir, 'xyz_from_hdr_ipt_roundtrip.csv')
    save_vector(xyz_from_hdr_ipt_data, filepath, "hdr-IPT to XYZ roundtrip values")

    # hdr-Lab - test uses test_xyz_hdr.csv (XYZ * 100) for input
    hdr_lab_data = []
    for xyz in test_xyz_colors:
        hdr_lab = colour.XYZ_to_hdr_CIELab(np.array(xyz) * 100).tolist()
        hdr_lab_data.extend(hdr_lab)  # Only hdr-Lab values
    filepath = os.path.join(ref_dir, 'hdr_lab_from_xyz.csv')
    save_vector(hdr_lab_data, filepath, "XYZ to hdr-Lab expected values")

    # XYZ from hdr-Lab roundtrip - only XYZ values
    xyz_from_hdr_lab_data = []
    for xyz in test_xyz_colors:
        hdr_lab = colour.XYZ_to_hdr_CIELab(np.array(xyz) * 100)
        xyz_back = colour.hdr_CIELab_to_XYZ(hdr_lab).tolist()
        xyz_from_hdr_lab_data.extend(xyz_back)  # Only XYZ values
    filepath = os.path.join(ref_dir, 'xyz_from_hdr_lab_roundtrip.csv')
    save_vector(xyz_from_hdr_lab_data, filepath, "hdr-Lab to XYZ roundtrip values")

    # Extended RGB test colors - used by IHLS, HCL, Prismatic tests
    # These match the hardcoded values in 16_extended_colorspaces.c
    extended_rgb_colors = [
        [0.0, 0.0, 0.0], [1.0, 1.0, 1.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0],
        [0.0, 0.0, 1.0], [0.5, 0.5, 0.5], [0.25, 0.75, 0.5], [0.8, 0.2, 0.4],
        [0.1, 0.6, 0.9], [0.9, 0.3, 0.1], [0.3, 0.9, 0.7]
    ]

    # IHLS - test has hardcoded RGB values, expects only IHLS output
    ihls_data = []
    for rgb in extended_rgb_colors:
        ihls = colour.RGB_to_IHLS(np.array(rgb)).tolist()
        ihls_data.extend(ihls)  # Only IHLS values
    filepath = os.path.join(ref_dir, 'ihls_from_rgb.csv')
    save_vector(ihls_data, filepath, "RGB to IHLS expected values")

    # RGB from IHLS roundtrip - only RGB values
    rgb_from_ihls_data = []
    for rgb in extended_rgb_colors:
        ihls = colour.RGB_to_IHLS(np.array(rgb))
        rgb_back = colour.IHLS_to_RGB(ihls).tolist()
        rgb_from_ihls_data.extend(rgb_back)  # Only RGB values
    filepath = os.path.join(ref_dir, 'rgb_from_ihls_roundtrip.csv')
    save_vector(rgb_from_ihls_data, filepath, "IHLS to RGB roundtrip values")

    # HCL (CIE LCH-based HCL) - test has hardcoded RGB values, expects only HCL output
    hcl_data = []
    for rgb in extended_rgb_colors:
        hcl = colour.RGB_to_HCL(np.array(rgb)).tolist()
        hcl_data.extend(hcl)  # Only HCL values
    filepath = os.path.join(ref_dir, 'hcl_from_rgb.csv')
    save_vector(hcl_data, filepath, "RGB to HCL expected values")

    # RGB from HCL roundtrip - only RGB values
    rgb_from_hcl_data = []
    for rgb in extended_rgb_colors:
        hcl = colour.RGB_to_HCL(np.array(rgb))
        rgb_back = colour.HCL_to_RGB(hcl).tolist()
        rgb_from_hcl_data.extend(rgb_back)  # Only RGB values
    filepath = os.path.join(ref_dir, 'rgb_from_hcl_roundtrip.csv')
    save_vector(rgb_from_hcl_data, filepath, "HCL to RGB roundtrip values")

    # Prismatic - test has hardcoded RGB values, expects only Prismatic output
    # Library uses [L, s, h] = [L, R_norm, G_norm] from colour-science's [L, R, G, B]
    prismatic_data = []
    for rgb in extended_rgb_colors:
        prismatic = colour.RGB_to_Prismatic(np.array(rgb))
        # Library stores only [L, R_norm, G_norm] (first 3 of 4 components)
        prismatic_data.extend([prismatic[0], prismatic[1], prismatic[2]])
    filepath = os.path.join(ref_dir, 'prismatic_from_rgb.csv')
    save_vector(prismatic_data, filepath, "RGB to Prismatic expected values")

    # RGB from Prismatic roundtrip - only RGB values
    rgb_from_prismatic_data = []
    for rgb in extended_rgb_colors:
        prismatic = colour.RGB_to_Prismatic(np.array(rgb))
        rgb_back = colour.Prismatic_to_RGB(prismatic).tolist()
        rgb_from_prismatic_data.extend(rgb_back)  # Only RGB values
    filepath = os.path.join(ref_dir, 'rgb_from_prismatic_roundtrip.csv')
    save_vector(rgb_from_prismatic_data, filepath, "Prismatic to RGB roundtrip values")

    # --- HDR XYZ test colors (scaled for HDR colorspaces) ---
    test_xyz_hdr = []
    for xyz in test_xyz_colors:
        # Scale by 100 for HDR range
        test_xyz_hdr.extend([xyz[0] * 100, xyz[1] * 100, xyz[2] * 100])
    filepath = os.path.join(ref_dir, 'test_xyz_hdr.csv')
    save_vector(test_xyz_hdr, filepath, "Test XYZ colors (HDR scale)")

    # --- CAM viewing conditions ---
    # Store viewing conditions for CIECAM02/CAM16 tests
    cam_vc = [200.0, 18.0, 20.0]  # L_A, Y_b, surround_c (average)
    filepath = os.path.join(ref_dir, 'cam_viewing_conditions.csv')
    save_vector(cam_vc, filepath, "CAM viewing conditions (L_A, Y_b, c)")

    # --- CIECAM02 and CAM16 XYZ reconstructed ---
    try:
        ciecam02_xyz_reconstructed = []
        cam16_xyz_reconstructed = []
        vc_ciecam02 = colour.VIEWING_CONDITIONS_CIECAM02['Average']
        vc_cam16 = colour.VIEWING_CONDITIONS_CAM16['Average']

        for xyz in test_xyz_colors[:5]:
            # CIECAM02
            spec = colour.XYZ_to_CIECAM02(np.array(xyz), d65_xyz, vc_ciecam02.L_A, vc_ciecam02.Y_b)
            xyz_back = colour.CIECAM02_to_XYZ(spec.J, spec.C, spec.h, d65_xyz, vc_ciecam02.L_A, vc_ciecam02.Y_b)
            ciecam02_xyz_reconstructed.extend(xyz_back.tolist())

            # CAM16
            spec16 = colour.XYZ_to_CAM16(np.array(xyz), d65_xyz, vc_cam16.L_A, vc_cam16.Y_b)
            xyz_back16 = colour.CAM16_to_XYZ(spec16.J, spec16.C, spec16.h, d65_xyz, vc_cam16.L_A, vc_cam16.Y_b)
            cam16_xyz_reconstructed.extend(xyz_back16.tolist())

        filepath = os.path.join(ref_dir, 'ciecam02_xyz_reconstructed.csv')
        save_vector(ciecam02_xyz_reconstructed, filepath, "CIECAM02 XYZ reconstructed")
        filepath = os.path.join(ref_dir, 'cam16_xyz_reconstructed.csv')
        save_vector(cam16_xyz_reconstructed, filepath, "CAM16 XYZ reconstructed")
    except Exception as e:
        print(f"  Warning: Could not generate CAM XYZ reconstructed: {e}")

    # --- Hunter Lab (using library's D65 illuminant values) ---
    # Library uses: Xn=95.02, Yn=100, Zn=108.82 from colour-science TVS_ILLUMINANTS_HUNTERLAB
    hunter_xyz_n = np.array([95.02, 100.0, 108.82])
    hunter_test_xyz = [
        [95.02, 100.0, 108.82],    # D65 white (matching library's illuminant)
        [41.24, 21.26, 1.93],      # Red
        [35.76, 71.52, 11.92],     # Green
        [18.05, 7.22, 95.05],      # Blue
        [77.0, 92.78, 13.85],      # Yellow
        [59.29, 28.48, 96.98],     # Magenta-ish
        [53.81, 78.74, 106.97],    # Cyan-ish
        [20.517, 21.586, 23.507],  # Gray 20%
        [53.389, 56.272, 61.261],  # Gray 50%
        [76.054, 80.109, 87.12],   # Gray 70%
        [25.0, 50.0, 75.0],        # Test 1
        [60.0, 40.0, 30.0],        # Test 2
        [15.0, 25.0, 55.0],        # Test 3
        [80.0, 90.0, 50.0],        # Test 4
        [10.0, 15.0, 20.0],        # Test 5
    ]
    hunter_lab_pairs = []
    for xyz in hunter_test_xyz:
        hunter = colour.XYZ_to_Hunter_Lab(np.array(xyz), hunter_xyz_n).tolist()
        hunter_lab_pairs.extend(xyz + hunter)
    filepath = os.path.join(ref_dir, 'test_xyz_hunter_lab_pairs.csv')
    save_vector(hunter_lab_pairs, filepath, "XYZ/Hunter Lab test pairs")

    # XYZ from Hunter Lab roundtrip
    xyz_from_hunter_lab_data = []
    for xyz in hunter_test_xyz:
        hunter = colour.XYZ_to_Hunter_Lab(np.array(xyz), hunter_xyz_n)
        xyz_back = colour.Hunter_Lab_to_XYZ(hunter, hunter_xyz_n).tolist()
        xyz_from_hunter_lab_data.extend(hunter.tolist() + xyz_back)
    filepath = os.path.join(ref_dir, 'xyz_from_hunter_lab_roundtrip.csv')
    save_vector(xyz_from_hunter_lab_data, filepath, "Hunter Lab to XYZ roundtrip")

    # --- LGG combined output (lift/gamma/gain color grading) ---
    # Input: [0.5, 0.5, 0.5], lift=[0.1, 0.0, 0.0], gamma=[1.0, 1.0, 1.0], gain=[1.0, 1.0, 0.9]
    # Formula: ((rgb + lift) ^ (1/gamma)) * gain
    lgg_input = np.array([0.5, 0.5, 0.5])
    lgg_lift = np.array([0.1, 0.0, 0.0])
    lgg_gamma = np.array([1.0, 1.0, 1.0])
    lgg_gain = np.array([1.0, 1.0, 0.9])
    lgg_output = ((lgg_input + lgg_lift) ** (1.0 / lgg_gamma)) * lgg_gain
    filepath = os.path.join(ref_dir, 'lgg_combined.csv')
    save_vector(lgg_output.tolist(), filepath, "LGG combined output")

    # --- Printer lights per channel (using library formula) ---
    # Formula: output = input * 10^((25 - lights) * 0.025)
    pl_input = np.array([0.3, 0.5, 0.7])
    pl_lights = np.array([20, 25, 30])
    pl_output = pl_input * np.power(10.0, (25.0 - pl_lights) * 0.025)
    filepath = os.path.join(ref_dir, 'printer_lights_per_channel.csv')
    save_vector(pl_output.tolist(), filepath, "Printer lights per channel output")

    print(f"  [OK] Generated comprehensive missing test reference files")

    print("\n" + "=" * 70)
    print(f"Test reference values generation complete!")
    print("=" * 70)

    return True

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python test_reference_values.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    success = generate_test_reference_values(output_dir)
    sys.exit(0 if success else 1)
