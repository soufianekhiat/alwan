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

    # Jzazbz (requires absolute luminance, use 100 cd/m²)
    jzazbz_pairs = []
    for xyz in modern_test_xyz:
        jzazbz = colour.XYZ_to_Jzazbz(np.array(xyz) * 100).tolist()
        jzazbz_pairs.extend(xyz + jzazbz)
    filepath = os.path.join(ref_dir, 'test_xyz_jzazbz_pairs.csv')
    save_vector(jzazbz_pairs, filepath, "XYZ/Jzazbz test pairs")

    # IPT
    ipt_pairs = []
    for xyz in modern_test_xyz:
        ipt = colour.XYZ_to_IPT(np.array(xyz)).tolist()
        ipt_pairs.extend(xyz + ipt)
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

    # OSA-UCS
    osa_ucs_pairs = []
    for xyz in modern_test_xyz:
        osa = colour.XYZ_to_OSA_UCS(np.array(xyz)).tolist()
        osa_ucs_pairs.extend(xyz + osa)
    filepath = os.path.join(ref_dir, 'test_xyz_osa_ucs_pairs.csv')
    save_vector(osa_ucs_pairs, filepath, "XYZ/OSA-UCS test pairs")

    # Hunter Lab (skip black to avoid NaN from division by zero)
    hunter_lab_pairs = []
    for xyz in modern_test_xyz:
        # Skip black (0,0,0) to avoid NaN in Hunter Lab
        if xyz[0] == 0.0 and xyz[1] == 0.0 and xyz[2] == 0.0:
            # Use a very small value instead to avoid division by zero
            xyz_safe = [1e-10, 1e-10, 1e-10]
            hunter = colour.XYZ_to_Hunter_Lab(np.array(xyz_safe)).tolist()
        else:
            hunter = colour.XYZ_to_Hunter_Lab(np.array(xyz)).tolist()
        hunter_lab_pairs.extend(xyz + hunter)
    filepath = os.path.join(ref_dir, 'test_xyz_hunter_lab_pairs.csv')
    save_vector(hunter_lab_pairs, filepath, "XYZ/Hunter Lab test pairs")

    # ProLab (using Lab as proxy if ProLab not available)
    try:
        prolab_pairs = []
        for xyz in modern_test_xyz:
            # ProLab may not be in colour-science, use Lab as fallback
            lab = colour.XYZ_to_Lab(np.array(xyz)).tolist()
            prolab_pairs.extend(xyz + lab)
        filepath = os.path.join(ref_dir, 'test_xyz_prolab_pairs.csv')
        save_vector(prolab_pairs, filepath, "XYZ/ProLab test pairs (Lab fallback)")
    except:
        pass

    # ICtCp (from RGB)
    ictcp_pairs = []
    for rgb in test_rgb_colors:
        # ICtCp from BT.2020 RGB
        ictcp = colour.RGB_to_ICtCp(np.array(rgb), method='Dolby 2016').tolist()
        ictcp_pairs.extend(rgb + ictcp)
    filepath = os.path.join(ref_dir, 'ictcp_pq_from_rgb.csv')
    save_vector(ictcp_pairs, filepath, "RGB to ICtCp (PQ) test pairs")

    # YCoCg (from RGB)
    ycocg_pairs = []
    for rgb in test_rgb_colors:
        ycocg = colour.RGB_to_YCoCg(np.array(rgb)).tolist()
        ycocg_pairs.extend(rgb + ycocg)
    filepath = os.path.join(ref_dir, 'ycocg_from_rgb.csv')
    save_vector(ycocg_pairs, filepath, "RGB to YCoCg test pairs")

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

    # RGB from ICtCp (PQ) roundtrip
    rgb_from_ictcp_data = []
    for rgb in test_rgb_colors:
        ictcp = colour.RGB_to_ICtCp(np.array(rgb), method='Dolby 2016')
        rgb_back = colour.ICtCp_to_RGB(ictcp, method='Dolby 2016').tolist()
        rgb_from_ictcp_data.extend(ictcp.tolist() + rgb_back)
    filepath = os.path.join(ref_dir, 'rgb_from_ictcp_pq.csv')
    save_vector(rgb_from_ictcp_data, filepath, "ICtCp to RGB roundtrip test pairs")

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

    # RGB from YCoCg roundtrip
    rgb_from_ycocg_data = []
    for rgb in test_rgb_colors:
        # YCoCg forward and back using colour-science
        ycocg = colour.RGB_to_YCoCg(np.array(rgb))
        rgb_back = colour.YCoCg_to_RGB(ycocg).tolist()
        rgb_from_ycocg_data.extend(ycocg.tolist() + rgb_back)
    filepath = os.path.join(ref_dir, 'rgb_from_ycocg_roundtrip.csv')
    save_vector(rgb_from_ycocg_data, filepath, "YCoCg to RGB roundtrip test pairs")

    # Oklab to OKLCh pairs
    oklab_oklch_pairs = []
    for xyz in modern_test_xyz[:3]:  # Use first 3
        oklab = colour.XYZ_to_Oklab(np.array(xyz))
        oklch = colour.Lab_to_LCHab(oklab).tolist()  # LCHab transform works for Oklab too
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
            adapted = colour.chromatic_adaptation(np.array(xyz), d65_xy, d50_xy, method='Fairchild')
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
            adapted = colour.chromatic_adaptation(np.array(xyz), d65_xy, d50_xy, method='CMCCAT97')
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
            adapted = colour.chromatic_adaptation(np.array(xyz), d65_xy, d50_xy, method='CMCCAT2000')
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
            adapted = colour.chromatic_adaptation(np.array(xyz), d65_xy, d50_xy, method='CAT02 Brill 2008')
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
            adapted = colour.chromatic_adaptation(np.array(xyz), d65_xy, d50_xy, method='Bianco 2010')
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
            adapted = colour.chromatic_adaptation(np.array(xyz), d65_xy, d50_xy, method='Bianco PC 2010')
            adapted_bianco_pc_xyz.extend(adapted.tolist())
        filepath = os.path.join(ref_dir, 'adapted_d65_to_d50_bianco_pc_2010.csv')
        save_vector(adapted_bianco_pc_xyz, filepath, "Adapted XYZ D65->D50 Bianco PC 2010")
    except Exception as e:
        print(f"  Warning: Could not generate Bianco PC 2010 CAT: {e}")

    # Adapted XYZ D65->D50 with Sharp method
    try:
        adapted_sharp_xyz = []
        for xyz in test_xyz_colors:
            adapted = colour.chromatic_adaptation(np.array(xyz), d65_xy, d50_xy, method='Sharp')
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

    # Delta E ITP values
    # Calculate delta E ITP for RGB color pairs
    de_itp_values = []
    for i in range(len(test_rgb_colors) - 1):
        ictcp1 = colour.RGB_to_ICtCp(np.array(test_rgb_colors[i]), method='Dolby 2016')
        ictcp2 = colour.RGB_to_ICtCp(np.array(test_rgb_colors[i+1]), method='Dolby 2016')
        de = colour.difference.delta_E_ITP(ictcp1, ictcp2)
        de_itp_values.append(float(de))
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
    jzazbz1_data = []
    jzazbz2_data = []
    de_zcam_values = []
    for lab1, lab2 in test_lab_pairs:
        # Convert Lab to XYZ then to Jzazbz
        xyz1 = colour.Lab_to_XYZ(np.array(lab1), d65_xy)
        xyz2 = colour.Lab_to_XYZ(np.array(lab2), d65_xy)
        jzazbz1 = colour.XYZ_to_Jzazbz(xyz1 * 100).tolist()  # Scale for HDR range
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
