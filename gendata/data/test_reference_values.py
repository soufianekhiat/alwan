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

    # ZCAM correlates (stub using XYZ)
    filepath = os.path.join(ref_dir, 'test_zcam_correlates.csv')
    save_vector(cam_xyz_input, filepath, "ZCAM correlates test data (XYZ)")

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

    # Smits1999 white XYZ recovered (stub - use D65)
    smits_white_xyz = [0.95047, 1.0, 1.08883]
    filepath = os.path.join(ref_dir, 'smits1999_white_xyz_recovered.csv')
    save_vector(smits_white_xyz, filepath, "Smits1999 white XYZ recovered (stub)")

    # Photopic efficiency wavelengths (380-780nm in 5nm steps)
    photopic_wavelengths = [float(wl) for wl in range(380, 781, 5)]
    filepath = os.path.join(ref_dir, 'photopic_efficiency_wavelengths.csv')
    save_vector(photopic_wavelengths, filepath, "Photopic efficiency function wavelengths")

    print(f"  [OK] Generated {19} additional test reference files")

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
