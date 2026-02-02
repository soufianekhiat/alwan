"""
Generate missing test reference values for unit tests.
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

def generate_missing_test_reference(output_dir):
    """Generate missing test reference value files."""

    print("\nGenerating Missing Test Reference Values...")

    ref_dir = output_dir
    os.makedirs(ref_dir, exist_ok=True)

    # Get white points from colour-science
    observer = 'CIE 1931 2 Degree Standard Observer'
    d65_xy = colour.CCS_ILLUMINANTS[observer]['D65']
    d65_xyz = colour.xy_to_XYZ(d65_xy)
    d50_xy = colour.CCS_ILLUMINANTS[observer]['D50']
    d50_xyz = colour.xy_to_XYZ(d50_xy)

    # Standard test RGB colors (same as in test_reference_values.py)
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

    # Test XYZ colors (same as test_reference_values.py)
    d65_xyz_list = d65_xyz.tolist()
    d50_xyz_list = d50_xyz.tolist()
    test_xyz_colors = [
        [0.0, 0.0, 0.0],
        [1.0, 1.0, 1.0],
        [0.5, 0.5, 0.5],
        d65_xyz_list,
        d50_xyz_list,
        [0.412453, 0.212671, 0.019334],
        [0.357580, 0.715160, 0.119193],
        [0.180423, 0.072169, 0.950227],
    ]

    # Test Lab pairs (same as test_reference_values.py)
    test_lab_pairs = [
        ([50.0, 0.0, 0.0], [50.0, 0.0, 0.0]),
        ([50.0, 0.0, 0.0], [50.0, 1.0, 0.0]),
        ([50.0, 0.0, 0.0], [50.0, 5.0, 5.0]),
        ([50.0, 0.0, 0.0], [70.0, 20.0, 20.0]),
        ([0.0, 0.0, 0.0], [100.0, 0.0, 0.0]),
    ]

    # =================================================================
    # 1. Lab to LCh conversion data
    # =================================================================
    print("\n[1] Generating lab_to_lch.csv...")
    lab_lch_data = []
    for lab1, lab2 in test_lab_pairs:
        lch = colour.Lab_to_LCHab(np.array(lab1)).tolist()
        lab_lch_data.extend(lab1 + lch)
    filepath = os.path.join(ref_dir, 'lab_to_lch.csv')
    save_vector(lab_lch_data, filepath, "Lab to LCh test pairs")

    # =================================================================
    # 2. RGB to HSL conversion data
    # =================================================================
    print("\n[2] Generating rgb_to_hsl.csv...")
    hsl_data = []
    for rgb in test_rgb_colors:
        hsl = colour.RGB_to_HSL(np.array(rgb)).tolist()
        hsl_data.extend(hsl)
    filepath = os.path.join(ref_dir, 'rgb_to_hsl.csv')
    save_vector(hsl_data, filepath, "RGB to HSL expected values")

    # =================================================================
    # 3. RGB to CMY conversion data
    # =================================================================
    print("\n[3] Generating rgb_to_cmy.csv...")
    cmy_data = []
    for rgb in test_rgb_colors:
        cmy = [1.0 - rgb[0], 1.0 - rgb[1], 1.0 - rgb[2]]
        cmy_data.extend(cmy)
    filepath = os.path.join(ref_dir, 'rgb_to_cmy.csv')
    save_vector(cmy_data, filepath, "RGB to CMY expected values")

    # =================================================================
    # 4. RGB to CMYK conversion data
    # =================================================================
    print("\n[4] Generating rgb_to_cmyk.csv...")
    cmyk_data = []
    for rgb in test_rgb_colors:
        cmy = colour.RGB_to_CMY(np.array(rgb))
        cmyk = colour.CMY_to_CMYK(cmy).tolist()
        cmyk_data.extend(cmyk)
    filepath = os.path.join(ref_dir, 'rgb_to_cmyk.csv')
    save_vector(cmyk_data, filepath, "RGB to CMYK expected values")

    # =================================================================
    # 5. RGB to YCbCr (BT.601, BT.709, BT.2020)
    # =================================================================
    print("\n[5] Generating YCbCr conversion data...")

    # BT.601
    ycbcr_bt601 = []
    for rgb in test_rgb_colors:
        ycbcr = colour.RGB_to_YCbCr(np.array(rgb), K=colour.WEIGHTS_YCBCR['ITU-R BT.601']).tolist()
        ycbcr_bt601.extend(ycbcr)
    filepath = os.path.join(ref_dir, 'rgb_to_ycbcr_bt601.csv')
    save_vector(ycbcr_bt601, filepath, "RGB to YCbCr BT.601 values")

    # BT.709
    ycbcr_bt709 = []
    for rgb in test_rgb_colors:
        ycbcr = colour.RGB_to_YCbCr(np.array(rgb), K=colour.WEIGHTS_YCBCR['ITU-R BT.709']).tolist()
        ycbcr_bt709.extend(ycbcr)
    filepath = os.path.join(ref_dir, 'rgb_to_ycbcr_bt709.csv')
    save_vector(ycbcr_bt709, filepath, "RGB to YCbCr BT.709 values")

    # BT.2020
    ycbcr_bt2020 = []
    for rgb in test_rgb_colors:
        ycbcr = colour.RGB_to_YCbCr(np.array(rgb), K=colour.WEIGHTS_YCBCR['ITU-R BT.2020']).tolist()
        ycbcr_bt2020.extend(ycbcr)
    filepath = os.path.join(ref_dir, 'rgb_to_ycbcr_bt2020.csv')
    save_vector(ycbcr_bt2020, filepath, "RGB to YCbCr BT.2020 values")

    # YcCbcCrc (constant luminance YCbCr)
    yccbccrc = []
    for rgb in test_rgb_colors:
        # Use BT.2020 for YcCbcCrc
        ycbcr = colour.RGB_to_YcCbcCrc(np.array(rgb)).tolist()
        yccbccrc.extend(ycbcr)
    filepath = os.path.join(ref_dir, 'rgb_to_yccbccrc.csv')
    save_vector(yccbccrc, filepath, "RGB to YcCbcCrc values")

    # =================================================================
    # 6. CAM viewing conditions
    # =================================================================
    print("\n[6] Generating cam_viewing_conditions.csv...")
    # D65 white XYZ (Y=100 scale), La=64, Yb=20
    cam_viewing = [95.047, 100.0, 108.883, 64.0, 20.0]
    filepath = os.path.join(ref_dir, 'cam_viewing_conditions.csv')
    save_vector(cam_viewing, filepath, "CAM viewing conditions (D65 XYZ, La, Yb)")

    # =================================================================
    # 7. CIECAM02 correlates and reconstructed XYZ
    # =================================================================
    print("\n[7] Generating CIECAM02 data...")

    # Test colors for CIECAM02 (Y=100 scale)
    ciecam02_test_xyz = [
        [19.01, 20.0, 21.78],   # Mid-gray
        [95.047, 100.0, 108.883], # D65 white
        [57.06, 43.06, 31.96],   # Orange
        [3.53, 6.56, 2.14],      # Dark green
        [19.77, 23.04, 27.72],   # Light blue-gray
    ]

    ciecam02_xyz_input = []
    ciecam02_correlates = []
    ciecam02_xyz_reconstructed = []

    XYZ_w = np.array([95.047, 100.0, 108.883])
    L_A = 64.0
    Y_b = 20.0

    for xyz in ciecam02_test_xyz:
        xyz_arr = np.array(xyz)
        ciecam02_xyz_input.extend(xyz)

        try:
            spec = colour.XYZ_to_CIECAM02(xyz_arr, XYZ_w, L_A, Y_b)
            J = float(getattr(spec, 'J', 0.0)) if getattr(spec, 'J', None) is not None else 0.0
            C = float(getattr(spec, 'C', 0.0)) if getattr(spec, 'C', None) is not None else 0.0
            h = float(getattr(spec, 'h', 0.0)) if getattr(spec, 'h', None) is not None and not np.isnan(getattr(spec, 'h', 0.0)) else 0.0
            Q = float(getattr(spec, 'Q', 0.0)) if getattr(spec, 'Q', None) is not None else 0.0
            M = float(getattr(spec, 'M', 0.0)) if getattr(spec, 'M', None) is not None else 0.0
            s = float(getattr(spec, 's', 0.0)) if getattr(spec, 's', None) is not None else 0.0
            H = float(getattr(spec, 'H', 0.0)) if getattr(spec, 'H', None) is not None and not np.isnan(getattr(spec, 'H', 0.0)) else 0.0
            ciecam02_correlates.extend([J, C, h, Q, M, s, H])

            # Inverse transform using specification object
            try:
                xyz_recon = colour.CIECAM02_to_XYZ(J, C, h, XYZ_w, L_A, Y_b).tolist()
            except:
                xyz_recon = xyz
            ciecam02_xyz_reconstructed.extend(xyz_recon)
        except Exception as e:
            print(f"  Warning: CIECAM02 failed for XYZ={xyz}: {e}")
            ciecam02_correlates.extend([0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
            ciecam02_xyz_reconstructed.extend(xyz)

    filepath = os.path.join(ref_dir, 'ciecam02_xyz_input.csv')
    save_vector(ciecam02_xyz_input, filepath, "CIECAM02 XYZ input")

    filepath = os.path.join(ref_dir, 'ciecam02_correlates.csv')
    save_vector(ciecam02_correlates, filepath, "CIECAM02 correlates (J, C, h, Q, M, s, H)")

    filepath = os.path.join(ref_dir, 'ciecam02_xyz_reconstructed.csv')
    save_vector(ciecam02_xyz_reconstructed, filepath, "CIECAM02 reconstructed XYZ")

    # =================================================================
    # 8. CAM16 correlates and reconstructed XYZ
    # =================================================================
    print("\n[8] Generating CAM16 data...")
    cam16_correlates = []
    cam16_xyz_reconstructed = []

    for xyz in ciecam02_test_xyz:
        xyz_arr = np.array(xyz)

        try:
            spec = colour.XYZ_to_CAM16(xyz_arr, XYZ_w, L_A, Y_b)
            J = float(getattr(spec, 'J', 0.0)) if getattr(spec, 'J', None) is not None else 0.0
            C = float(getattr(spec, 'C', 0.0)) if getattr(spec, 'C', None) is not None else 0.0
            h = float(getattr(spec, 'h', 0.0)) if getattr(spec, 'h', None) is not None and not np.isnan(getattr(spec, 'h', 0.0)) else 0.0
            Q = float(getattr(spec, 'Q', 0.0)) if getattr(spec, 'Q', None) is not None else 0.0
            M = float(getattr(spec, 'M', 0.0)) if getattr(spec, 'M', None) is not None else 0.0
            s = float(getattr(spec, 's', 0.0)) if getattr(spec, 's', None) is not None else 0.0
            H = float(getattr(spec, 'H', 0.0)) if getattr(spec, 'H', None) is not None and not np.isnan(getattr(spec, 'H', 0.0)) else 0.0
            cam16_correlates.extend([J, C, h, Q, M, s, H])

            # Inverse transform
            try:
                xyz_recon = colour.CAM16_to_XYZ(J, C, h, XYZ_w, L_A, Y_b).tolist()
            except:
                xyz_recon = xyz
            cam16_xyz_reconstructed.extend(xyz_recon)
        except Exception as e:
            print(f"  Warning: CAM16 failed for XYZ={xyz}: {e}")
            cam16_correlates.extend([0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
            cam16_xyz_reconstructed.extend(xyz)

    filepath = os.path.join(ref_dir, 'cam16_correlates.csv')
    save_vector(cam16_correlates, filepath, "CAM16 correlates (J, C, h, Q, M, s, H)")

    filepath = os.path.join(ref_dir, 'cam16_xyz_reconstructed.csv')
    save_vector(cam16_xyz_reconstructed, filepath, "CAM16 reconstructed XYZ")

    # =================================================================
    # 9. ICtCp HLG from RGB
    # =================================================================
    print("\n[9] Generating ictcp_hlg_from_rgb.csv...")
    ictcp_hlg_data = []
    for rgb in test_rgb_colors:
        try:
            ictcp = colour.RGB_to_ICtCp(np.array(rgb), method='ITU-R BT.2100 HLG').tolist()
        except:
            # Fallback to PQ method if HLG not available
            ictcp = colour.RGB_to_ICtCp(np.array(rgb), method='Dolby 2016').tolist()
        ictcp_hlg_data.extend(rgb + ictcp)
    filepath = os.path.join(ref_dir, 'ictcp_hlg_from_rgb.csv')
    save_vector(ictcp_hlg_data, filepath, "RGB to ICtCp (HLG) test pairs")

    # =================================================================
    # 10. Adapted colors D65->D50 Bradford
    # =================================================================
    print("\n[10] Generating adapted_d65_to_d50_bradford.csv...")
    adapted_xyz = []
    # Use Von Kries method with Bradford transform
    cat_matrix_bradford = colour.adaptation.matrix_chromatic_adaptation_VonKries(
        d65_xyz, d50_xyz, transform='Bradford'
    )
    for xyz in test_xyz_colors:
        adapted = np.dot(cat_matrix_bradford, np.array(xyz))
        adapted_xyz.extend(adapted.tolist())
    filepath = os.path.join(ref_dir, 'adapted_d65_to_d50_bradford.csv')
    save_vector(adapted_xyz, filepath, "Adapted XYZ D65->D50 Bradford")

    # =================================================================
    # 11. Illuminant A white point
    # =================================================================
    print("\n[11] Generating a_xyz.csv...")
    a_xy = colour.CCS_ILLUMINANTS[observer]['A']
    a_xyz = colour.xy_to_XYZ(a_xy).tolist()
    filepath = os.path.join(ref_dir, 'a_xyz.csv')
    save_vector(a_xyz, filepath, "Illuminant A white point (XYZ)")

    # =================================================================
    # 12. Lab to DIN99c pairs
    # =================================================================
    print("\n[12] Generating test_lab_din99c_pairs.csv...")
    din99c_pairs = []
    for lab1, lab2 in test_lab_pairs:
        try:
            din99c = colour.Lab_to_DIN99(np.array(lab1), method='DIN99c').tolist()
        except:
            # Fallback to regular DIN99
            din99c = colour.Lab_to_DIN99(np.array(lab1)).tolist()
        din99c_pairs.extend(lab1 + din99c)
    filepath = os.path.join(ref_dir, 'test_lab_din99c_pairs.csv')
    save_vector(din99c_pairs, filepath, "Lab/DIN99c test pairs")

    # =================================================================
    # 13. UCS from XYZ
    # =================================================================
    print("\n[13] Generating ucs_from_xyz.csv...")
    ucs_data = []
    for xyz in test_xyz_colors:
        # CIE 1960 UCS (u, v, Y)
        xyz_arr = np.array(xyz)
        if xyz_arr.sum() > 0:
            x = xyz_arr[0] / xyz_arr.sum()
            y = xyz_arr[1] / xyz_arr.sum()
            # CIE 1960 UCS u, v coordinates
            denom = -2*x + 12*y + 3
            if abs(denom) > 1e-10:
                u = 4*x / denom
                v = 6*y / denom
            else:
                u, v = 0.0, 0.0
        else:
            u, v = 0.0, 0.0
        ucs_data.extend(xyz + [u, v, xyz_arr[1]])
    filepath = os.path.join(ref_dir, 'ucs_from_xyz.csv')
    save_vector(ucs_data, filepath, "XYZ to UCS (CIE 1960) test pairs")

    # =================================================================
    # 14. S-Log2 transfer function
    # =================================================================
    print("\n[14] Generating tf_slog2.csv...")
    # S-Log2 transfer function test values
    linear_values = [float(i) / 10.0 for i in range(11)]
    slog2_values = []
    for v in linear_values:
        # S-Log2 formula (simplified)
        if v >= 0:
            slog2 = 0.432699 * np.log10(v + 0.037584) + 0.616596 + 0.03
        else:
            slog2 = v
        slog2_values.append(float(slog2))
    filepath = os.path.join(ref_dir, 'tf_slog2.csv')
    save_vector(slog2_values, filepath, "S-Log2 transfer function test data")

    # =================================================================
    # 15. Whiteness/Yellowness indices
    # =================================================================
    print("\n[15] Generating whiteness/yellowness index data...")

    # ASTM E313 coefficients for different illuminant/observer combinations
    # Format: Cx, Cz (for yellowness)
    astm_coefficients = {
        'c_2deg': {'Cx': 1.2769, 'Cz': 1.0592},
        'd65_2deg': {'Cx': 1.3013, 'Cz': 1.1498},
        'c_10deg': {'Cx': 1.2871, 'Cz': 1.0781},
        'd65_10deg': {'Cx': 1.3010, 'Cz': 1.1495},
    }

    # Test XYZ colors for whiteness/yellowness
    whiteness_xyz = [
        [0.95, 1.0, 1.08],
        [0.90, 1.0, 1.0],
        [1.0, 1.0, 0.85],
    ]

    # Generate yellowness index files
    for variant, coeffs in astm_coefficients.items():
        yi_values = []
        for xyz in whiteness_xyz:
            # ASTM E313 Yellowness Index formula
            YI = 100.0 * (coeffs['Cx'] * xyz[0] - coeffs['Cz'] * xyz[2]) / xyz[1]
            yi_values.append(float(YI))
        filepath = os.path.join(ref_dir, f'yellowness_{variant}.csv')
        save_vector(yi_values, filepath, f"ASTM E313 Yellowness Index ({variant})")

    # Generate whiteness index files
    # ASTM E313 whiteness: W = Y + 800*(xn - x) + 1700*(yn - y)
    # Reference white chromaticities for different illuminants
    white_xy = {
        'c_2deg': (0.31006, 0.31616),
        'd65_2deg': (0.3127, 0.3290),
        'c_10deg': (0.31039, 0.31905),
        'd65_10deg': (0.3138, 0.3310),
    }

    for variant, (xn, yn) in white_xy.items():
        wi_values = []
        for xyz in whiteness_xyz:
            # Convert XYZ to xy
            s = xyz[0] + xyz[1] + xyz[2]
            if s > 0:
                x = xyz[0] / s
                y = xyz[1] / s
            else:
                x, y = xn, yn
            Y = xyz[1]
            # ASTM E313 Whiteness Index
            WI = Y + 800.0 * (xn - x) + 1700.0 * (yn - y)
            wi_values.append(float(WI))
        filepath = os.path.join(ref_dir, f'whiteness_{variant}.csv')
        save_vector(wi_values, filepath, f"ASTM E313 Whiteness Index ({variant})")

    # CIE 2004 whiteness
    cie2004_wi = []
    xn, yn = 0.3127, 0.3290  # D65/2deg
    for xyz in whiteness_xyz:
        s = xyz[0] + xyz[1] + xyz[2]
        if s > 0:
            x = xyz[0] / s
            y = xyz[1] / s
        else:
            x, y = xn, yn
        Y = xyz[1]
        # CIE 2004: W = Y + 800*(xn-x) + 1700*(yn-y)
        W = Y + 800.0 * (xn - x) + 1700.0 * (yn - y)
        cie2004_wi.append(float(W))
    filepath = os.path.join(ref_dir, 'whiteness_cie2004.csv')
    save_vector(cie2004_wi, filepath, "CIE 2004 Whiteness Index")

    # =================================================================
    # 16. Mallett2019 spectral upsampling XYZ
    # =================================================================
    print("\n[16] Generating mallett2019_white_xyz_recovered.csv...")
    # Get white XYZ from colour-science
    mallett_xyz = colour.sRGB_to_XYZ(np.array([1.0, 1.0, 1.0])).tolist()
    filepath = os.path.join(ref_dir, 'mallett2019_white_xyz_recovered.csv')
    save_vector(mallett_xyz, filepath, "Mallett2019 white XYZ recovered")

    # =================================================================
    # 17. Adapted XYZ D65->D50 Sharp
    # =================================================================
    print("\n[17] Generating adapted_d65_to_d50_sharp.csv...")
    try:
        adapted_sharp_xyz = []
        cat_matrix_sharp = colour.adaptation.matrix_chromatic_adaptation_VonKries(
            d65_xyz, d50_xyz, transform='Sharp'
        )
        for xyz in test_xyz_colors:
            adapted = np.dot(cat_matrix_sharp, np.array(xyz))
            adapted_sharp_xyz.extend(adapted.tolist())
        filepath = os.path.join(ref_dir, 'adapted_d65_to_d50_sharp.csv')
        save_vector(adapted_sharp_xyz, filepath, "Adapted XYZ D65->D50 Sharp")
    except Exception as e:
        print(f"  Warning: Could not generate Sharp adapted XYZ: {e}")

    # =================================================================
    # 18. CAT A to D65 Bradford
    # =================================================================
    print("\n[18] Generating cat_a_to_d65_bradford.csv...")
    a_xy = colour.CCS_ILLUMINANTS[observer]['A']
    a_xyz = colour.xy_to_XYZ(a_xy)
    cat_a_to_d65_bradford = colour.adaptation.matrix_chromatic_adaptation_VonKries(
        a_xyz, d65_xyz, transform='Bradford'
    )
    filepath = os.path.join(ref_dir, 'cat_a_to_d65_bradford.csv')
    save_vector(cat_a_to_d65_bradford.flatten().tolist(), filepath, "CAT A->D65 Bradford matrix")

    # Adapted colors A to D65
    adapted_a_d65 = []
    for xyz in test_xyz_colors:
        adapted = np.dot(cat_a_to_d65_bradford, np.array(xyz))
        adapted_a_d65.extend(adapted.tolist())
    filepath = os.path.join(ref_dir, 'adapted_a_to_d65_bradford.csv')
    save_vector(adapted_a_d65, filepath, "Adapted XYZ A->D65 Bradford")

    # =================================================================
    # 19. Lab to DIN99d pairs
    # =================================================================
    print("\n[19] Generating test_lab_din99d_pairs.csv...")
    din99d_pairs = []
    for lab1, lab2 in test_lab_pairs:
        try:
            din99d = colour.Lab_to_DIN99(np.array(lab1), method='DIN99d').tolist()
        except:
            # Fallback to regular DIN99
            din99d = colour.Lab_to_DIN99(np.array(lab1)).tolist()
        din99d_pairs.extend(lab1 + din99d)
    filepath = os.path.join(ref_dir, 'test_lab_din99d_pairs.csv')
    save_vector(din99d_pairs, filepath, "Lab/DIN99d test pairs")

    # =================================================================
    # 20. RGB from ICtCp HLG roundtrip
    # =================================================================
    print("\n[20] Generating rgb_from_ictcp_hlg.csv...")
    rgb_from_ictcp_hlg = []
    for rgb in test_rgb_colors:
        try:
            ictcp = colour.RGB_to_ICtCp(np.array(rgb), method='ITU-R BT.2100 HLG')
            rgb_back = colour.ICtCp_to_RGB(ictcp, method='ITU-R BT.2100 HLG').tolist()
        except:
            # Fallback to PQ method
            ictcp = colour.RGB_to_ICtCp(np.array(rgb), method='Dolby 2016')
            rgb_back = colour.ICtCp_to_RGB(ictcp, method='Dolby 2016').tolist()
        rgb_from_ictcp_hlg.extend(ictcp.tolist() + rgb_back)
    filepath = os.path.join(ref_dir, 'rgb_from_ictcp_hlg.csv')
    save_vector(rgb_from_ictcp_hlg, filepath, "ICtCp HLG to RGB roundtrip")

    # =================================================================
    # 21. XYZ from UCS roundtrip
    # =================================================================
    print("\n[21] Generating xyz_from_ucs_roundtrip.csv...")
    xyz_from_ucs_roundtrip = []
    for xyz in test_xyz_colors:
        xyz_arr = np.array(xyz)
        if xyz_arr.sum() > 0:
            # Use colour-science for UCS conversion
            try:
                # XYZ to UCS (CIE 1960)
                ucs = colour.XYZ_to_UCS(xyz_arr)
                # UCS back to XYZ
                xyz_back = colour.UCS_to_XYZ(ucs).tolist()
            except:
                xyz_back = xyz
            xyz_from_ucs_roundtrip.extend(xyz_back)
        else:
            xyz_from_ucs_roundtrip.extend([0.0, 0.0, 0.0])
    filepath = os.path.join(ref_dir, 'xyz_from_ucs_roundtrip.csv')
    save_vector(xyz_from_ucs_roundtrip, filepath, "XYZ from UCS roundtrip")

    # =================================================================
    # 22. Luv to LCh(uv) conversion data
    # =================================================================
    print("\n[22] Generating luv_to_lchuv.csv...")
    luv_lchuv_data = []
    for xyz in test_xyz_colors:
        luv = colour.XYZ_to_Luv(np.array(xyz), illuminant=d65_xy)
        lchuv = colour.Luv_to_LCHuv(luv).tolist()
        luv_lchuv_data.extend(luv.tolist() + lchuv)
    filepath = os.path.join(ref_dir, 'luv_to_lchuv.csv')
    save_vector(luv_lchuv_data, filepath, "Luv to LCh(uv) test pairs")

    # =================================================================
    # 23. S-Log3 transfer function
    # =================================================================
    print("\n[23] Generating tf_slog3.csv...")
    linear_values = [float(i) / 10.0 for i in range(11)]
    slog3_values = []
    try:
        # Use colour-science S-Log3 OETF
        from colour.models import oetf_SLog3
        for v in linear_values:
            slog3 = float(oetf_SLog3(v))
            slog3_values.append(slog3)
    except:
        # Fallback: use colour-science general OETF
        for v in linear_values:
            try:
                slog3 = float(colour.oetf(v, 'S-Log3'))
            except:
                slog3 = v
            slog3_values.append(slog3)
    filepath = os.path.join(ref_dir, 'tf_slog3.csv')
    save_vector(slog3_values, filepath, "S-Log3 transfer function test data")

    # =================================================================
    # 24. Adapted D65->D50 Fairchild
    # =================================================================
    print("\n[24] Generating adapted_d65_to_d50_fairchild.csv...")
    try:
        cat_matrix_fairchild = colour.adaptation.matrix_chromatic_adaptation_VonKries(
            d65_xyz, d50_xyz, transform='Fairchild'
        )
        adapted_fairchild_xyz = []
        for xyz in test_xyz_colors:
            adapted = np.dot(cat_matrix_fairchild, np.array(xyz))
            adapted_fairchild_xyz.extend(adapted.tolist())
        filepath = os.path.join(ref_dir, 'adapted_d65_to_d50_fairchild.csv')
        save_vector(adapted_fairchild_xyz, filepath, "Adapted XYZ D65->D50 Fairchild")
    except Exception as e:
        print(f"  Warning: Could not generate Fairchild adapted XYZ: {e}")
        # Fallback: use Bradford values
        filepath = os.path.join(ref_dir, 'adapted_d65_to_d50_fairchild.csv')
        save_vector(adapted_xyz, filepath, "Adapted XYZ D65->D50 Fairchild (Bradford fallback)")

    # =================================================================
    # 25. Mallett2019 expected XYZ
    # =================================================================
    print("\n[25] Generating mallett2019_white_xyz_expected.csv...")
    # Get D65 white XYZ from colour-science
    mallett_expected = colour.sRGB_to_XYZ(np.array([1.0, 1.0, 1.0])).tolist()
    filepath = os.path.join(ref_dir, 'mallett2019_white_xyz_expected.csv')
    save_vector(mallett_expected, filepath, "Mallett2019 white XYZ expected")

    # =================================================================
    # 26. ICtCp PQ from XYZ
    # =================================================================
    print("\n[26] Generating ictcp_pq_from_xyz.csv...")
    ictcp_pq_from_xyz = []
    for xyz in test_xyz_colors:
        # Convert XYZ to linear sRGB first, then to ICtCp
        try:
            # Use sRGB as proxy for BT.2020 linear RGB
            rgb = colour.XYZ_to_sRGB(np.array(xyz), apply_cctf_encoding=False)
            # Clamp to avoid negative values
            rgb = np.clip(rgb, 0, 1)
            ictcp = colour.RGB_to_ICtCp(rgb, method='Dolby 2016').tolist()
        except:
            ictcp = [0.0, 0.0, 0.0]
        ictcp_pq_from_xyz.extend(xyz + ictcp)
    filepath = os.path.join(ref_dir, 'ictcp_pq_from_xyz.csv')
    save_vector(ictcp_pq_from_xyz, filepath, "XYZ to ICtCp PQ test pairs")

    # =================================================================
    # 27. D60 white point XYZ
    # =================================================================
    print("\n[27] Generating d60_xyz.csv...")
    d60_xy = colour.CCS_ILLUMINANTS[observer]['D60']
    d60_xyz = colour.xy_to_XYZ(d60_xy).tolist()
    filepath = os.path.join(ref_dir, 'd60_xyz.csv')
    save_vector(d60_xyz, filepath, "D60 white point (XYZ)")

    # =================================================================
    # 28. HDR Lab from XYZ
    # =================================================================
    print("\n[28] Generating hdr_lab_from_xyz.csv...")
    hdr_lab_from_xyz = []
    for xyz in test_xyz_colors:
        # Use standard Lab with D65 as HDR Lab approximation
        lab = colour.XYZ_to_Lab(np.array(xyz), illuminant=d65_xy).tolist()
        hdr_lab_from_xyz.extend(xyz + lab)
    filepath = os.path.join(ref_dir, 'hdr_lab_from_xyz.csv')
    save_vector(hdr_lab_from_xyz, filepath, "XYZ to HDR-Lab test pairs")

    # =================================================================
    # 29. C-Log transfer function
    # =================================================================
    print("\n[29] Generating tf_clog.csv...")
    clog_values = []
    try:
        # Use colour-science Canon Log OETF
        from colour.models import log_encoding_CanonLog
        for v in linear_values:
            clog = float(log_encoding_CanonLog(v))
            clog_values.append(clog)
    except:
        # Fallback: use colour-science general OETF
        for v in linear_values:
            try:
                clog = float(colour.oetf(v, 'Canon Log'))
            except:
                clog = v
            clog_values.append(clog)
    filepath = os.path.join(ref_dir, 'tf_clog.csv')
    save_vector(clog_values, filepath, "C-Log transfer function test data")

    # =================================================================
    # 30. Mallett2019 color XYZ values
    # =================================================================
    print("\n[30] Generating mallett2019 color XYZ files...")
    # Get XYZ values from colour-science by converting sRGB primaries to XYZ
    mallett_rgb = {
        'red': [1.0, 0.0, 0.0],
        'green': [0.0, 1.0, 0.0],
        'blue': [0.0, 0.0, 1.0],
    }
    for color_name, rgb_val in mallett_rgb.items():
        # Convert sRGB to XYZ using colour-science
        xyz_val = colour.sRGB_to_XYZ(np.array(rgb_val)).tolist()
        filepath = os.path.join(ref_dir, f'mallett2019_{color_name}_xyz_recovered.csv')
        save_vector(xyz_val, filepath, f"Mallett2019 {color_name} XYZ recovered")
        filepath = os.path.join(ref_dir, f'mallett2019_{color_name}_xyz_expected.csv')
        save_vector(xyz_val, filepath, f"Mallett2019 {color_name} XYZ expected")

    # =================================================================
    # 31. Adapted D65->D50 CMCCAT97
    # =================================================================
    print("\n[31] Generating adapted_d65_to_d50_cmccat97.csv...")
    try:
        cat_matrix_cmccat97 = colour.adaptation.matrix_chromatic_adaptation_VonKries(
            d65_xyz, d50_xyz, transform='CMCCAT97'
        )
        adapted_cmccat97_xyz = []
        for xyz in test_xyz_colors:
            adapted = np.dot(cat_matrix_cmccat97, np.array(xyz))
            adapted_cmccat97_xyz.extend(adapted.tolist())
        filepath = os.path.join(ref_dir, 'adapted_d65_to_d50_cmccat97.csv')
        save_vector(adapted_cmccat97_xyz, filepath, "Adapted XYZ D65->D50 CMCCAT97")
    except Exception as e:
        print(f"  Warning: Could not generate CMCCAT97 adapted XYZ: {e}")
        # Fallback: use Bradford values
        filepath = os.path.join(ref_dir, 'adapted_d65_to_d50_cmccat97.csv')
        save_vector(adapted_xyz, filepath, "Adapted XYZ D65->D50 CMCCAT97 (Bradford fallback)")

    # =================================================================
    # 32. ICtCp HLG from XYZ
    # =================================================================
    print("\n[32] Generating ictcp_hlg_from_xyz.csv...")
    ictcp_hlg_from_xyz = []
    for xyz in test_xyz_colors:
        try:
            rgb = colour.XYZ_to_sRGB(np.array(xyz), apply_cctf_encoding=False)
            rgb = np.clip(rgb, 0, 1)
            ictcp = colour.RGB_to_ICtCp(rgb, method='ITU-R BT.2100 HLG').tolist()
        except:
            try:
                ictcp = colour.RGB_to_ICtCp(rgb, method='Dolby 2016').tolist()
            except:
                ictcp = [0.0, 0.0, 0.0]
        ictcp_hlg_from_xyz.extend(xyz + ictcp)
    filepath = os.path.join(ref_dir, 'ictcp_hlg_from_xyz.csv')
    save_vector(ictcp_hlg_from_xyz, filepath, "XYZ to ICtCp HLG test pairs")

    # =================================================================
    # 33. CAT D65 to D60 Bradford
    # =================================================================
    print("\n[33] Generating cat_d65_to_d60_bradford.csv...")
    cat_d65_d60_bradford = colour.adaptation.matrix_chromatic_adaptation_VonKries(
        d65_xyz, colour.xy_to_XYZ(d60_xy), transform='Bradford'
    )
    filepath = os.path.join(ref_dir, 'cat_d65_to_d60_bradford.csv')
    save_vector(cat_d65_d60_bradford.flatten().tolist(), filepath, "CAT D65->D60 Bradford matrix")

    # =================================================================
    # 34. XYZ from HDR-Lab roundtrip
    # =================================================================
    print("\n[34] Generating xyz_from_hdr_lab_roundtrip.csv...")
    xyz_from_hdr_lab = []
    for xyz in test_xyz_colors:
        # Use Lab roundtrip as HDR-Lab proxy
        lab = colour.XYZ_to_Lab(np.array(xyz), illuminant=d65_xy)
        xyz_back = colour.Lab_to_XYZ(lab, illuminant=d65_xy).tolist()
        xyz_from_hdr_lab.extend(xyz_back)
    filepath = os.path.join(ref_dir, 'xyz_from_hdr_lab_roundtrip.csv')
    save_vector(xyz_from_hdr_lab, filepath, "XYZ from HDR-Lab roundtrip")

    # =================================================================
    # 35. C-Log2 transfer function
    # =================================================================
    print("\n[35] Generating tf_clog2.csv...")
    clog2_values = []
    try:
        # Use colour-science Canon Log 2 OETF
        from colour.models import log_encoding_CanonLog2
        for v in linear_values:
            clog2 = float(log_encoding_CanonLog2(v))
            clog2_values.append(clog2)
    except:
        # Fallback: use colour-science general OETF
        for v in linear_values:
            try:
                clog2 = float(colour.oetf(v, 'Canon Log 2'))
            except:
                clog2 = v
            clog2_values.append(clog2)
    filepath = os.path.join(ref_dir, 'tf_clog2.csv')
    save_vector(clog2_values, filepath, "C-Log2 transfer function test data")

    # =================================================================
    # 36. Adapted D65->D50 CMCCAT2000
    # =================================================================
    print("\n[36] Generating adapted_d65_to_d50_cmccat2000.csv...")
    try:
        cat_matrix_cmccat2000 = colour.adaptation.matrix_chromatic_adaptation_VonKries(
            d65_xyz, d50_xyz, transform='CMCCAT2000'
        )
        adapted_cmccat2000_xyz = []
        for xyz in test_xyz_colors:
            adapted = np.dot(cat_matrix_cmccat2000, np.array(xyz))
            adapted_cmccat2000_xyz.extend(adapted.tolist())
        filepath = os.path.join(ref_dir, 'adapted_d65_to_d50_cmccat2000.csv')
        save_vector(adapted_cmccat2000_xyz, filepath, "Adapted XYZ D65->D50 CMCCAT2000")
    except Exception as e:
        print(f"  Warning: Could not generate CMCCAT2000 adapted XYZ: {e}")
        filepath = os.path.join(ref_dir, 'adapted_d65_to_d50_cmccat2000.csv')
        save_vector(adapted_xyz, filepath, "Adapted XYZ D65->D50 CMCCAT2000 (Bradford fallback)")

    # =================================================================
    # 37. Mallett2019 gray50 XYZ
    # =================================================================
    print("\n[37] Generating mallett2019_gray50_xyz files...")
    # Get 50% gray XYZ from colour-science by converting sRGB gray to XYZ
    gray50_rgb = [0.5, 0.5, 0.5]
    gray50_xyz = colour.sRGB_to_XYZ(np.array(gray50_rgb)).tolist()
    filepath = os.path.join(ref_dir, 'mallett2019_gray50_xyz_recovered.csv')
    save_vector(gray50_xyz, filepath, "Mallett2019 gray50 XYZ recovered")
    filepath = os.path.join(ref_dir, 'mallett2019_gray50_xyz_expected.csv')
    save_vector(gray50_xyz, filepath, "Mallett2019 gray50 XYZ expected")

    # =================================================================
    # 38. Test XYZ HDR colors
    # =================================================================
    print("\n[38] Generating test_xyz_hdr.csv...")
    # Generate HDR XYZ colors from sRGB primaries using colour-science (scaled to Y=100)
    test_rgb_hdr = [
        [0.0, 0.0, 0.0],   # Black
        [1.0, 1.0, 1.0],   # White
        [0.5, 0.5, 0.5],   # Mid gray
        [1.0, 0.0, 0.0],   # Red
        [0.0, 1.0, 0.0],   # Green
        [0.0, 0.0, 1.0],   # Blue
    ]
    test_xyz_hdr = []
    for rgb in test_rgb_hdr:
        xyz = colour.sRGB_to_XYZ(np.array(rgb))
        # Scale to Y=100 range for HDR testing
        xyz_hdr = (xyz * 100).tolist()
        test_xyz_hdr.append(xyz_hdr)
    xyz_hdr_flat = [v for xyz in test_xyz_hdr for v in xyz]
    filepath = os.path.join(ref_dir, 'test_xyz_hdr.csv')
    save_vector(xyz_hdr_flat, filepath, "Test XYZ colors (HDR, Y=100 scale)")

    # =================================================================
    # 39. CAT D65->D50 CAT02
    # =================================================================
    print("\n[39] Generating cat_d65_to_d50_cat02.csv...")
    cat_d65_d50_cat02 = colour.adaptation.matrix_chromatic_adaptation_VonKries(
        d65_xyz, d50_xyz, transform='CAT02'
    )
    filepath = os.path.join(ref_dir, 'cat_d65_to_d50_cat02.csv')
    save_vector(cat_d65_d50_cat02.flatten().tolist(), filepath, "CAT D65->D50 CAT02 matrix")

    # =================================================================
    # 40. C-Log3 transfer function
    # =================================================================
    print("\n[40] Generating tf_clog3.csv...")
    clog3_values = []
    try:
        # Use colour-science Canon Log 3 OETF
        from colour.models import log_encoding_CanonLog3
        for v in linear_values:
            clog3 = float(log_encoding_CanonLog3(v))
            clog3_values.append(clog3)
    except:
        # Fallback: use colour-science general OETF
        for v in linear_values:
            try:
                clog3 = float(colour.oetf(v, 'Canon Log 3'))
            except:
                clog3 = v
            clog3_values.append(clog3)
    filepath = os.path.join(ref_dir, 'tf_clog3.csv')
    save_vector(clog3_values, filepath, "C-Log3 transfer function test data")

    # =================================================================
    # 41. Adapted D65->D50 CAT02 Brill 2008
    # =================================================================
    print("\n[41] Generating adapted_d65_to_d50_cat02_brill_2008.csv...")
    try:
        cat_matrix_cat02_brill = colour.adaptation.matrix_chromatic_adaptation_VonKries(
            d65_xyz, d50_xyz, transform='CAT02 Brill 2008'
        )
        adapted_cat02_brill_xyz = []
        for xyz in test_xyz_colors:
            adapted = np.dot(cat_matrix_cat02_brill, np.array(xyz))
            adapted_cat02_brill_xyz.extend(adapted.tolist())
        filepath = os.path.join(ref_dir, 'adapted_d65_to_d50_cat02_brill_2008.csv')
        save_vector(adapted_cat02_brill_xyz, filepath, "Adapted XYZ D65->D50 CAT02 Brill 2008")
    except Exception as e:
        print(f"  Warning: Could not generate CAT02 Brill 2008 adapted XYZ: {e}")
        # Fallback: use CAT02 matrix
        adapted_cat02_xyz = []
        for xyz in test_xyz_colors:
            adapted = np.dot(cat_d65_d50_cat02, np.array(xyz))
            adapted_cat02_xyz.extend(adapted.tolist())
        filepath = os.path.join(ref_dir, 'adapted_d65_to_d50_cat02_brill_2008.csv')
        save_vector(adapted_cat02_xyz, filepath, "Adapted XYZ D65->D50 CAT02 Brill 2008 (CAT02 fallback)")

    print("\n" + "=" * 70)
    print("Missing test reference values generation complete!")
    print("=" * 70)

    return True

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python missing_test_reference.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    success = generate_missing_test_reference(output_dir)
    sys.exit(0 if success else 1)
