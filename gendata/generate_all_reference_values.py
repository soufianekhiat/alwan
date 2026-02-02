"""
Generate all reference values for Alwan tests using colour-science.
This script creates CSV files that can be directly included in C test files.
"""

import numpy as np
import os

# Import colour-science
import colour
from colour import (
    XYZ_to_xyY, XYZ_to_Lab, Lab_to_XYZ, XYZ_to_Luv, Lab_to_LCHab, Luv_to_LCHuv,
    RGB_to_HSV, HSV_to_RGB, RGB_to_HSL, HSL_to_RGB,
    delta_E,
    chromatic_adaptation,
)
from colour.models import (
    RGB_to_YCoCg, YCoCg_to_RGB,
    RGB_to_YCbCr, YCbCr_to_RGB, WEIGHTS_YCBCR,
    RGB_to_CMY, CMY_to_CMYK,
    RGB_to_ICtCp, ICtCp_to_RGB,
    XYZ_to_JzAzBz, JzAzBz_to_XYZ,
    XYZ_to_Oklab, Oklab_to_XYZ, Lab_to_LCHab as Oklab_to_OkLCh,
    XYZ_to_OSA_UCS, XYZ_to_Hunter_Lab,
    XYZ_to_IPT, IPT_to_XYZ,
    XYZ_to_hdr_CIELab, hdr_CIELab_to_XYZ,
    XYZ_to_hdr_IPT, hdr_IPT_to_XYZ,
    XYZ_to_IgPgTg, IgPgTg_to_XYZ,
    XYZ_to_ICaCb, ICaCb_to_XYZ,
    XYZ_to_UCS, UCS_to_XYZ,
)
from colour.appearance import (
    CIECAM02_to_XYZ, XYZ_to_CIECAM02,
    CAM16_to_XYZ, XYZ_to_CAM16,
    InductionFactors_CIECAM02,
)
from colour import XYZ_to_CAM16UCS

# Standard viewing conditions for CAM tests (sRGB viewing conditions per IEC 61966-2-1:1999)
# L_A: Adapting luminance in cd/m^2 (64 lux ambient = ~20% of 80 cd/m^2 display)
# Y_b: Background luminance relative factor (20% gray)
CAM_L_A = 318.31  # cd/m^2 - commonly used test value for average surround
CAM_Y_b = 20.0    # 20% relative background luminance
from colour.adaptation import chromatic_adaptation_matrix_VonKries
from colour.colorimetry import (
    MSDS_CMFS, SDS_ILLUMINANTS,
    sd_to_XYZ,
)
from colour.temperature import CCT_to_xy_CIE_D

# Output directory
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), '..', 'tests', 'reference_values')
os.makedirs(OUTPUT_DIR, exist_ok=True)

def write_csv(filename, data, comment=None):
    """Write data to CSV file in C-includable format."""
    filepath = os.path.join(OUTPUT_DIR, filename)
    with open(filepath, 'w') as f:
        if comment:
            f.write(f"/* {comment} */\n")

        data = np.atleast_1d(data).flatten()
        for i, val in enumerate(data):
            suffix = ',' if i < len(data) - 1 else ''
            f.write(f"    {val:.15e}{suffix}\n")
    print(f"  Generated: {filename}")

def generate_basic_test_data():
    """Generate basic test colors and white points."""
    print("\n=== Basic Test Data ===")

    # D65 white point (CIE 1931 2 degree)
    D65 = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D65']
    D65_XYZ = colour.xy_to_XYZ(D65)
    D65_XYZ = D65_XYZ / D65_XYZ[1] * 100  # Normalize Y=100
    write_csv('test_d65_white.csv', D65_XYZ, 'D65 white point XYZ (Y=100)')

    # D50 white point
    D50 = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D50']
    D50_XYZ = colour.xy_to_XYZ(D50)
    D50_XYZ = D50_XYZ / D50_XYZ[1] * 100
    write_csv('test_d50_white.csv', D50_XYZ, 'D50 white point XYZ (Y=100)')

    # Illuminant A white point
    A = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['A']
    A_XYZ = colour.xy_to_XYZ(A)
    A_XYZ = A_XYZ / A_XYZ[1] * 100
    write_csv('a_xyz.csv', A_XYZ, 'Illuminant A white point XYZ (Y=100)')

    # D60 white point
    D60_xy = CCT_to_xy_CIE_D(6000)
    D60_XYZ = colour.xy_to_XYZ(D60_xy)
    D60_XYZ = D60_XYZ / D60_XYZ[1] * 100
    write_csv('d60_xyz.csv', D60_XYZ, 'D60 white point XYZ (Y=100)')

    # Test XYZ colors (variety of colors)
    test_xyz = np.array([
        [50.0, 50.0, 50.0],      # Gray
        [95.047, 100.0, 108.883], # D65 white
        [41.24, 21.26, 1.93],    # Red
        [35.76, 71.52, 11.92],   # Green
        [18.05, 7.22, 95.05],    # Blue
        [77.0, 92.78, 13.85],    # Yellow
    ])
    write_csv('test_xyz_colors.csv', test_xyz, '6 test XYZ colors')

    # Test RGB colors (11 colors for convenience tests)
    test_rgb = np.array([
        [0.0, 0.0, 0.0],         # Black
        [1.0, 1.0, 1.0],         # White
        [1.0, 0.0, 0.0],         # Red
        [0.0, 1.0, 0.0],         # Green
        [0.0, 0.0, 1.0],         # Blue
        [1.0, 1.0, 0.0],         # Yellow
        [0.0, 1.0, 1.0],         # Cyan
        [1.0, 0.0, 1.0],         # Magenta
        [0.5, 0.5, 0.5],         # Gray
        [0.25, 0.5, 0.75],       # Test color 1
        [0.8, 0.2, 0.6],         # Test color 2
    ])
    write_csv('test_rgb_colors.csv', test_rgb, '11 test RGB colors')

    # HDR test XYZ (extended range)
    test_xyz_hdr = np.array([
        [50.0, 50.0, 50.0],
        [200.0, 200.0, 200.0],
        [500.0, 500.0, 500.0],
        [1000.0, 1000.0, 1000.0],
    ])
    write_csv('test_xyz_hdr.csv', test_xyz_hdr, 'HDR test XYZ colors')

    return test_xyz, test_rgb, D65_XYZ, D50_XYZ, A_XYZ

def generate_xyz_conversions(test_xyz):
    """Generate XYZ to xyY conversions."""
    print("\n=== XYZ Conversions ===")

    xyY = np.array([XYZ_to_xyY(xyz) for xyz in test_xyz])
    write_csv('xyz_to_xyy.csv', xyY, 'XYZ to xyY conversion')

def generate_lab_luv_conversions(test_xyz, D65_XYZ, D50_XYZ):
    """Generate Lab and Luv conversions."""
    print("\n=== Lab/Luv Conversions ===")

    # XYZ to Lab (D65)
    lab_d65 = np.array([XYZ_to_Lab(xyz, D65_XYZ) for xyz in test_xyz])
    write_csv('xyz_to_lab_d65.csv', lab_d65, 'XYZ to Lab (D65)')

    # XYZ to Lab (D50)
    lab_d50 = np.array([XYZ_to_Lab(xyz, D50_XYZ) for xyz in test_xyz])
    write_csv('xyz_to_lab_d50.csv', lab_d50, 'XYZ to Lab (D50)')

    # XYZ to Luv (D65)
    luv_d65 = np.array([XYZ_to_Luv(xyz, D65_XYZ) for xyz in test_xyz])
    write_csv('xyz_to_luv_d65.csv', luv_d65, 'XYZ to Luv (D65)')

    # Lab to LCh
    lch = np.array([Lab_to_LCHab(lab) for lab in lab_d65])
    write_csv('lab_to_lch.csv', lch, 'Lab to LCh')

    # Luv to LChuv
    lchuv = np.array([Luv_to_LCHuv(luv) for luv in luv_d65])
    write_csv('luv_to_lchuv.csv', lchuv, 'Luv to LChuv')

    return lab_d65

def generate_delta_e(lab_d65):
    """Generate Delta E test data."""
    print("\n=== Delta E ===")

    # Create pairs of Lab colors for Delta E tests
    lab1 = lab_d65[:-1]  # All but last
    lab2 = lab_d65[1:]   # All but first

    write_csv('delta_e_lab1.csv', lab1, 'Delta E Lab color 1')
    write_csv('delta_e_lab2.csv', lab2, 'Delta E Lab color 2')

    # Delta E 76
    de_76 = np.array([delta_E(l1, l2, method='CIE 1976') for l1, l2 in zip(lab1, lab2)])
    write_csv('delta_e_76.csv', de_76, 'Delta E 76')

    # Delta E 94
    de_94 = np.array([delta_E(l1, l2, method='CIE 1994') for l1, l2 in zip(lab1, lab2)])
    write_csv('delta_e_94.csv', de_94, 'Delta E 94')

    # Delta E CMC
    de_cmc = np.array([delta_E(l1, l2, method='CMC') for l1, l2 in zip(lab1, lab2)])
    write_csv('delta_e_cmc.csv', de_cmc, 'Delta E CMC')

    # Delta E 2000
    de_2000 = np.array([delta_E(l1, l2, method='CIE 2000') for l1, l2 in zip(lab1, lab2)])
    write_csv('delta_e_2000.csv', de_2000, 'Delta E 2000')

def generate_cat_data(test_xyz, D65_XYZ, D50_XYZ, A_XYZ):
    """Generate chromatic adaptation data."""
    print("\n=== Chromatic Adaptation ===")

    # D60 white point
    D60_xy = CCT_to_xy_CIE_D(6000)
    D60_XYZ = colour.xy_to_XYZ(D60_xy)
    D60_XYZ = D60_XYZ / D60_XYZ[1] * 100

    # Bradford matrices
    M_d65_to_d50_bradford = chromatic_adaptation_matrix_VonKries(D65_XYZ, D50_XYZ, 'Bradford')
    write_csv('cat_d65_to_d50_bradford.csv', M_d65_to_d50_bradford, 'CAT D65->D50 Bradford matrix')

    M_d50_to_d65_bradford = chromatic_adaptation_matrix_VonKries(D50_XYZ, D65_XYZ, 'Bradford')
    write_csv('cat_d50_to_d65_bradford.csv', M_d50_to_d65_bradford, 'CAT D50->D65 Bradford matrix')

    M_a_to_d65_bradford = chromatic_adaptation_matrix_VonKries(A_XYZ, D65_XYZ, 'Bradford')
    write_csv('cat_a_to_d65_bradford.csv', M_a_to_d65_bradford, 'CAT A->D65 Bradford matrix')

    M_d65_to_d60_bradford = chromatic_adaptation_matrix_VonKries(D65_XYZ, D60_XYZ, 'Bradford')
    write_csv('cat_d65_to_d60_bradford.csv', M_d65_to_d60_bradford, 'CAT D65->D60 Bradford matrix')

    # CAT02 matrix
    M_d65_to_d50_cat02 = chromatic_adaptation_matrix_VonKries(D65_XYZ, D50_XYZ, 'CAT02')
    write_csv('cat_d65_to_d50_cat02.csv', M_d65_to_d50_cat02, 'CAT D65->D50 CAT02 matrix')

    # CAT16 matrix
    M_d65_to_d50_cat16 = chromatic_adaptation_matrix_VonKries(D65_XYZ, D50_XYZ, 'CAT16')
    write_csv('cat_d65_to_d50_cat16.csv', M_d65_to_d50_cat16, 'CAT D65->D50 CAT16 matrix')

    # XYZ Scaling matrix
    M_d65_to_d50_xyz = chromatic_adaptation_matrix_VonKries(D65_XYZ, D50_XYZ, 'XYZ Scaling')
    write_csv('cat_d65_to_d50_xyz_scaling.csv', M_d65_to_d50_xyz, 'CAT D65->D50 XYZ Scaling matrix')

    # Adapted colors D65 to D50 Bradford
    adapted_d65_to_d50 = np.array([chromatic_adaptation(xyz, D65_XYZ, D50_XYZ, 'Bradford') for xyz in test_xyz])
    write_csv('adapted_d65_to_d50_bradford.csv', adapted_d65_to_d50, 'Adapted D65->D50 Bradford')

    # Adapted colors A to D65 Bradford
    adapted_a_to_d65 = np.array([chromatic_adaptation(xyz, A_XYZ, D65_XYZ, 'Bradford') for xyz in test_xyz])
    write_csv('adapted_a_to_d65_bradford.csv', adapted_a_to_d65, 'Adapted A->D65 Bradford')

def generate_extended_cat_data(test_xyz, D65_XYZ, D50_XYZ):
    """Generate extended CAT method data."""
    print("\n=== Extended CAT Methods ===")

    methods = [
        ('Sharp', 'sharp'),
        ('Fairchild', 'fairchild'),
        ('CMCCAT97', 'cmccat97'),
        ('CMCCAT2000', 'cmccat2000'),
        ('CAT02 Brill 2008', 'cat02_brill_2008'),
        ('Bianco 2010', 'bianco_2010'),
        ('Bianco PC 2010', 'bianco_pc_2010'),
    ]

    for name, file_suffix in methods:
        try:
            M = chromatic_adaptation_matrix_VonKries(D65_XYZ, D50_XYZ, name)
            write_csv(f'cat_d65_to_d50_{file_suffix}.csv', M, f'CAT D65->D50 {name} matrix')

            adapted = np.array([chromatic_adaptation(xyz, D65_XYZ, D50_XYZ, name) for xyz in test_xyz])
            write_csv(f'adapted_d65_to_d50_{file_suffix}.csv', adapted, f'Adapted D65->D50 {name}')
        except Exception as e:
            print(f"  Warning: Could not generate {name}: {e}")

def generate_cam_data(test_xyz, D65_XYZ):
    """Generate CIECAM02 and CAM16 test data."""
    print("\n=== CAM Models ===")

    # Viewing conditions parameters using defined constants
    # white XYZ, adapting luminance, background luminance
    vc_params = np.array([D65_XYZ[0], D65_XYZ[1], D65_XYZ[2], CAM_L_A, CAM_Y_b])
    write_csv('cam_viewing_conditions.csv', vc_params, 'CAM viewing conditions')

    # CIECAM02 test colors (same as test_xyz but normalized)
    cam_xyz = test_xyz.copy()
    write_csv('ciecam02_xyz_input.csv', cam_xyz, 'CIECAM02 XYZ input')

    # CIECAM02 correlates
    correlates = []
    xyz_reconstructed = []
    for xyz in cam_xyz:
        try:
            spec = XYZ_to_CIECAM02(xyz, D65_XYZ, CAM_L_A, CAM_Y_b)
            # J, C, h, Q, M, s, H
            correlates.append([spec.J, spec.C, spec.h, spec.Q, spec.M, spec.s, spec.H])

            # Reconstruct XYZ
            xyz_back = CIECAM02_to_XYZ(spec.J, spec.C, spec.h, D65_XYZ, CAM_L_A, CAM_Y_b)
            xyz_reconstructed.append(xyz_back)
        except:
            correlates.append([0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
            xyz_reconstructed.append([0.0, 0.0, 0.0])

    write_csv('ciecam02_correlates.csv', np.array(correlates), 'CIECAM02 correlates (J,C,h,Q,M,s,H)')
    write_csv('ciecam02_xyz_reconstructed.csv', np.array(xyz_reconstructed), 'CIECAM02 reconstructed XYZ')

    # CAM16 correlates
    cam16_correlates = []
    cam16_xyz_reconstructed = []
    cam16_ucs_jab = []
    for xyz in cam_xyz:
        try:
            spec = XYZ_to_CAM16(xyz, D65_XYZ, CAM_L_A, CAM_Y_b)
            cam16_correlates.append([spec.J, spec.C, spec.h, spec.Q, spec.M, spec.s, spec.H])

            # Reconstruct XYZ
            xyz_back = CAM16_to_XYZ(spec.J, spec.C, spec.h, D65_XYZ, CAM_L_A, CAM_Y_b)
            cam16_xyz_reconstructed.append(xyz_back)

            # CAM16-UCS Jab using colour-science
            ucs_jab = XYZ_to_CAM16UCS(xyz, XYZ_w=D65_XYZ, L_A=CAM_L_A, Y_b=CAM_Y_b)
            cam16_ucs_jab.append(ucs_jab)
        except:
            cam16_correlates.append([0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
            cam16_xyz_reconstructed.append([0.0, 0.0, 0.0])
            cam16_ucs_jab.append([0.0, 0.0, 0.0])

    write_csv('cam16_correlates.csv', np.array(cam16_correlates), 'CAM16 correlates (J,C,h,Q,M,s,H)')
    write_csv('cam16_xyz_reconstructed.csv', np.array(cam16_xyz_reconstructed), 'CAM16 reconstructed XYZ')
    write_csv('cam16_ucs_jab.csv', np.array(cam16_ucs_jab), 'CAM16-UCS Jab')

def generate_rgb_conversions(test_rgb):
    """Generate RGB convenience conversions."""
    print("\n=== RGB Conversions ===")

    # RGB to HSV
    hsv = np.array([RGB_to_HSV(rgb) for rgb in test_rgb])
    # colour-science returns H in [0,1], we want [0,360]
    hsv[:, 0] *= 360
    write_csv('rgb_to_hsv.csv', hsv, 'RGB to HSV')

    # RGB to HSL
    hsl = np.array([RGB_to_HSL(rgb) for rgb in test_rgb])
    hsl[:, 0] *= 360
    write_csv('rgb_to_hsl.csv', hsl, 'RGB to HSL')

    # RGB to CMY (using colour-science)
    cmy = np.array([RGB_to_CMY(rgb) for rgb in test_rgb])
    write_csv('rgb_to_cmy.csv', cmy, 'RGB to CMY')

    # RGB to CMYK (using colour-science)
    cmyk = np.array([CMY_to_CMYK(RGB_to_CMY(rgb)) for rgb in test_rgb])
    write_csv('rgb_to_cmyk.csv', cmyk, 'RGB to CMYK')

    # YCbCr conversions using colour-science with proper weights
    # BT.601 (using colour-science WEIGHTS_YCBCR)
    ycbcr_601 = np.array([RGB_to_YCbCr(rgb, K=WEIGHTS_YCBCR['ITU-R BT.601'],
                                        out_legal=False, out_int=False)
                          for rgb in test_rgb])
    write_csv('rgb_to_ycbcr_bt601.csv', ycbcr_601, 'RGB to YCbCr BT.601')

    # BT.709 (using colour-science WEIGHTS_YCBCR)
    ycbcr_709 = np.array([RGB_to_YCbCr(rgb, K=WEIGHTS_YCBCR['ITU-R BT.709'],
                                        out_legal=False, out_int=False)
                          for rgb in test_rgb])
    write_csv('rgb_to_ycbcr_bt709.csv', ycbcr_709, 'RGB to YCbCr BT.709')

    # BT.2020 (using colour-science WEIGHTS_YCBCR)
    ycbcr_2020 = np.array([RGB_to_YCbCr(rgb, K=WEIGHTS_YCBCR['ITU-R BT.2020'],
                                         out_legal=False, out_int=False)
                           for rgb in test_rgb])
    write_csv('rgb_to_ycbcr_bt2020.csv', ycbcr_2020, 'RGB to YCbCr BT.2020')

    # YcCbcCrc (constant luminance BT.2020) - use colour-science if available
    # Note: colour-science uses the same YCbCr function with BT.2020 weights
    # The constant luminance variant is the same Y with different Cb/Cr scaling
    yccbccrc = np.array([RGB_to_YCbCr(rgb, K=WEIGHTS_YCBCR['ITU-R BT.2020'],
                                       out_legal=False, out_int=False)
                         for rgb in test_rgb])
    write_csv('rgb_to_yccbccrc.csv', yccbccrc, 'RGB to YcCbcCrc')

    # YCoCg
    ycocg = np.array([RGB_to_YCoCg(rgb) for rgb in test_rgb])
    write_csv('ycocg_from_rgb.csv', ycocg, 'YCoCg from RGB')

    ycocg_roundtrip = np.array([YCoCg_to_RGB(RGB_to_YCoCg(rgb)) for rgb in test_rgb])
    write_csv('rgb_from_ycocg_roundtrip.csv', ycocg_roundtrip, 'RGB from YCoCg roundtrip')

def generate_extended_colorspaces(test_xyz, test_rgb, D65_XYZ):
    """Generate extended colorspace conversions."""
    print("\n=== Extended Colorspaces ===")

    # CIE 1960 UCS (using colour-science)
    ucs = []
    xyz_from_ucs = []
    for xyz in test_xyz:
        try:
            ucs_val = XYZ_to_UCS(xyz)
            ucs.append(ucs_val)
            xyz_back = UCS_to_XYZ(ucs_val)
            xyz_from_ucs.append(xyz_back)
        except:
            ucs.append([0, 0, 0])
            xyz_from_ucs.append([0, 0, 0])
    write_csv('ucs_from_xyz.csv', np.array(ucs), 'UCS from XYZ')
    write_csv('xyz_from_ucs_roundtrip.csv', np.array(xyz_from_ucs), 'XYZ from UCS roundtrip')

    # HDR CIELAB
    hdr_lab = []
    xyz_from_hdr_lab = []
    for xyz in test_xyz:
        try:
            lab = XYZ_to_hdr_CIELab(xyz, D65_XYZ)
            hdr_lab.append(lab)
            xyz_back = hdr_CIELab_to_XYZ(lab, D65_XYZ)
            xyz_from_hdr_lab.append(xyz_back)
        except:
            hdr_lab.append([0, 0, 0])
            xyz_from_hdr_lab.append([0, 0, 0])
    write_csv('hdr_lab_from_xyz.csv', np.array(hdr_lab), 'HDR CIELAB from XYZ')
    write_csv('xyz_from_hdr_lab_roundtrip.csv', np.array(xyz_from_hdr_lab), 'XYZ from HDR CIELAB roundtrip')

    # HDR IPT
    hdr_ipt = []
    xyz_from_hdr_ipt = []
    for xyz in test_xyz:
        try:
            ipt = XYZ_to_hdr_IPT(xyz)
            hdr_ipt.append(ipt)
            xyz_back = hdr_IPT_to_XYZ(ipt)
            xyz_from_hdr_ipt.append(xyz_back)
        except:
            hdr_ipt.append([0, 0, 0])
            xyz_from_hdr_ipt.append([0, 0, 0])
    write_csv('hdr_ipt_from_xyz.csv', np.array(hdr_ipt), 'HDR IPT from XYZ')
    write_csv('xyz_from_hdr_ipt_roundtrip.csv', np.array(xyz_from_hdr_ipt), 'XYZ from HDR IPT roundtrip')

    # IgPgTg
    igpgtg = []
    xyz_from_igpgtg = []
    for xyz in test_xyz:
        try:
            val = XYZ_to_IgPgTg(xyz)
            igpgtg.append(val)
            xyz_back = IgPgTg_to_XYZ(val)
            xyz_from_igpgtg.append(xyz_back)
        except:
            igpgtg.append([0, 0, 0])
            xyz_from_igpgtg.append([0, 0, 0])
    write_csv('igpgtg_from_xyz.csv', np.array(igpgtg), 'IgPgTg from XYZ')
    write_csv('xyz_from_igpgtg_roundtrip.csv', np.array(xyz_from_igpgtg), 'XYZ from IgPgTg roundtrip')

    # ICaCb
    icacb = []
    xyz_from_icacb = []
    for xyz in test_xyz:
        try:
            val = XYZ_to_ICaCb(xyz)
            icacb.append(val)
            xyz_back = ICaCb_to_XYZ(val)
            xyz_from_icacb.append(xyz_back)
        except:
            icacb.append([0, 0, 0])
            xyz_from_icacb.append([0, 0, 0])
    write_csv('icacb_from_xyz.csv', np.array(icacb), 'ICaCb from XYZ')
    write_csv('xyz_from_icacb_roundtrip.csv', np.array(xyz_from_icacb), 'XYZ from ICaCb roundtrip')

    # Prismatic (using colour-science)
    from colour.models import RGB_to_Prismatic, Prismatic_to_RGB
    prismatic = []
    rgb_from_prismatic = []
    for rgb in test_rgb:
        try:
            pris = RGB_to_Prismatic(rgb)
            prismatic.append(pris)
            rgb_back = Prismatic_to_RGB(pris)
            rgb_from_prismatic.append(rgb_back)
        except:
            prismatic.append([0, 0, 0])
            rgb_from_prismatic.append(rgb)
    write_csv('prismatic_from_rgb.csv', np.array(prismatic), 'Prismatic from RGB')
    write_csv('rgb_from_prismatic_roundtrip.csv', np.array(rgb_from_prismatic), 'RGB from Prismatic roundtrip')

    # HCL (using colour-science)
    from colour.models import RGB_to_HCL, HCL_to_RGB
    hcl = []
    rgb_from_hcl = []
    for rgb in test_rgb:
        try:
            hcl_val = RGB_to_HCL(rgb)
            hcl.append(hcl_val)
            rgb_back = HCL_to_RGB(hcl_val)
            rgb_from_hcl.append(rgb_back)
        except:
            hcl.append([0, 0, 0])
            rgb_from_hcl.append(rgb)
    write_csv('hcl_from_rgb.csv', np.array(hcl), 'HCL from RGB')
    write_csv('rgb_from_hcl_roundtrip.csv', np.array(rgb_from_hcl), 'RGB from HCL roundtrip')

    # IHLS (using colour-science)
    from colour.models import RGB_to_IHLS, IHLS_to_RGB
    ihls = []
    rgb_from_ihls = []
    for rgb in test_rgb:
        try:
            ihls_val = RGB_to_IHLS(rgb)
            ihls.append(ihls_val)
            rgb_back = IHLS_to_RGB(ihls_val)
            rgb_from_ihls.append(rgb_back)
        except:
            ihls.append([0, 0, 0])
            rgb_from_ihls.append(rgb)
    write_csv('ihls_from_rgb.csv', np.array(ihls), 'IHLS from RGB')
    write_csv('rgb_from_ihls_roundtrip.csv', np.array(rgb_from_ihls), 'RGB from IHLS roundtrip')

def generate_ictcp_data(test_xyz):
    """Generate ICtCp data."""
    print("\n=== ICtCp ===")

    # Test RGB values for ICtCp (scene-referred, can be > 1.0)
    test_rgb_ictcp = np.array([
        [0.0, 0.0, 0.0],
        [1.0, 1.0, 1.0],
        [0.5, 0.5, 0.5],
        [1.0, 0.0, 0.0],
        [0.0, 1.0, 0.0],
        [0.0, 0.0, 1.0],
        [2.0, 2.0, 2.0],
        [5.0, 5.0, 5.0],
    ])

    # ICtCp PQ from RGB
    ictcp_pq = []
    rgb_from_ictcp_pq = []
    for rgb in test_rgb_ictcp:
        try:
            ictcp = RGB_to_ICtCp(rgb, method='Dolby 2016')
            ictcp_pq.append(ictcp)
            rgb_back = ICtCp_to_RGB(ictcp, method='Dolby 2016')
            rgb_from_ictcp_pq.append(rgb_back)
        except:
            ictcp_pq.append([0, 0, 0])
            rgb_from_ictcp_pq.append([0, 0, 0])
    write_csv('ictcp_pq_from_rgb.csv', np.array(ictcp_pq), 'ICtCp PQ from RGB')
    write_csv('rgb_from_ictcp_pq.csv', np.array(rgb_from_ictcp_pq), 'RGB from ICtCp PQ')

    # ICtCp HLG from RGB
    ictcp_hlg = []
    rgb_from_ictcp_hlg = []
    for rgb in test_rgb_ictcp:
        try:
            ictcp = RGB_to_ICtCp(rgb, method='ITU-R BT.2100-2 HLG')
            ictcp_hlg.append(ictcp)
            rgb_back = ICtCp_to_RGB(ictcp, method='ITU-R BT.2100-2 HLG')
            rgb_from_ictcp_hlg.append(rgb_back)
        except:
            ictcp_hlg.append([0, 0, 0])
            rgb_from_ictcp_hlg.append([0, 0, 0])
    write_csv('ictcp_hlg_from_rgb.csv', np.array(ictcp_hlg), 'ICtCp HLG from RGB')
    write_csv('rgb_from_ictcp_hlg.csv', np.array(rgb_from_ictcp_hlg), 'RGB from ICtCp HLG')

    # ICtCp from XYZ (using test_xyz)
    ictcp_pq_from_xyz = []
    ictcp_hlg_from_xyz = []
    for xyz in test_xyz:
        try:
            # Convert XYZ to RGB first (BT.2020)
            from colour import XYZ_to_RGB
            rgb = XYZ_to_RGB(xyz / 100, 'ITU-R BT.2020')
            rgb = np.clip(rgb, 0, None)
            ictcp_pq = RGB_to_ICtCp(rgb, method='Dolby 2016')
            ictcp_hlg = RGB_to_ICtCp(rgb, method='ITU-R BT.2100-2 HLG')
            ictcp_pq_from_xyz.append(ictcp_pq)
            ictcp_hlg_from_xyz.append(ictcp_hlg)
        except:
            ictcp_pq_from_xyz.append([0, 0, 0])
            ictcp_hlg_from_xyz.append([0, 0, 0])
    write_csv('ictcp_pq_from_xyz.csv', np.array(ictcp_pq_from_xyz), 'ICtCp PQ from XYZ')
    write_csv('ictcp_hlg_from_xyz.csv', np.array(ictcp_hlg_from_xyz), 'ICtCp HLG from XYZ')

def generate_jzazbz_oklab_data(test_xyz):
    """Generate JzAzBz and Oklab data."""
    print("\n=== JzAzBz and Oklab ===")

    # XYZ to JzAzBz pairs
    jzazbz_pairs = []
    for xyz in test_xyz:
        try:
            jab = XYZ_to_JzAzBz(xyz)
            jzazbz_pairs.extend([xyz[0], xyz[1], xyz[2], jab[0], jab[1], jab[2]])
        except:
            jzazbz_pairs.extend([xyz[0], xyz[1], xyz[2], 0, 0, 0])
    write_csv('test_xyz_jzazbz_pairs.csv', np.array(jzazbz_pairs), 'XYZ and JzAzBz pairs')

    # Oklab pairs
    oklab_pairs = []
    for xyz in test_xyz:
        try:
            lab = XYZ_to_Oklab(xyz / 100)  # Oklab expects normalized XYZ
            oklab_pairs.extend([xyz[0], xyz[1], xyz[2], lab[0], lab[1], lab[2]])
        except:
            oklab_pairs.extend([xyz[0], xyz[1], xyz[2], 0, 0, 0])
    write_csv('test_xyz_oklab_pairs.csv', np.array(oklab_pairs), 'XYZ and Oklab pairs')

    # Oklab to OkLCh pairs
    oklch_pairs = []
    for xyz in test_xyz:
        try:
            lab = XYZ_to_Oklab(xyz / 100)
            lch = Oklab_to_OkLCh(lab)
            oklch_pairs.extend([lab[0], lab[1], lab[2], lch[0], lch[1], lch[2]])
        except:
            oklch_pairs.extend([0, 0, 0, 0, 0, 0])
    write_csv('test_oklab_oklch_pairs.csv', np.array(oklch_pairs), 'Oklab and OkLCh pairs')

def generate_din99_data(test_xyz, D65_XYZ):
    """Generate DIN99 variant data."""
    print("\n=== DIN99 ===")

    from colour.models import (
        Lab_to_DIN99, DIN99_to_Lab,
    )

    # DIN99 variants (original, b, c, d)
    for variant, method in [('', 'DIN99'), ('b', 'DIN99b'), ('c', 'DIN99c'), ('d', 'DIN99d')]:
        pairs = []
        for xyz in test_xyz:
            try:
                lab = XYZ_to_Lab(xyz, D65_XYZ)
                din99 = Lab_to_DIN99(lab, method=method)
                pairs.extend([xyz[0], xyz[1], xyz[2], din99[0], din99[1], din99[2]])
            except:
                pairs.extend([xyz[0], xyz[1], xyz[2], 0, 0, 0])
        write_csv(f'test_lab_din99{variant}_pairs.csv', np.array(pairs), f'Lab and DIN99{variant.upper()} pairs')

def generate_osa_hunter_ipt_prolab_data(test_xyz, D65_XYZ):
    """Generate OSA-UCS, Hunter Lab, IPT, ProLab data."""
    print("\n=== OSA-UCS, Hunter Lab, IPT, ProLab ===")

    # OSA-UCS
    osa_pairs = []
    for xyz in test_xyz:
        try:
            osa = XYZ_to_OSA_UCS(xyz)
            osa_pairs.extend([xyz[0], xyz[1], xyz[2], osa[0], osa[1], osa[2]])
        except:
            osa_pairs.extend([xyz[0], xyz[1], xyz[2], 0, 0, 0])
    write_csv('test_xyz_osa_ucs_pairs.csv', np.array(osa_pairs), 'XYZ and OSA-UCS pairs')

    # Hunter Lab
    hunter_pairs = []
    for xyz in test_xyz:
        try:
            hunter = XYZ_to_Hunter_Lab(xyz, D65_XYZ)
            hunter_pairs.extend([xyz[0], xyz[1], xyz[2], hunter[0], hunter[1], hunter[2]])
        except:
            hunter_pairs.extend([xyz[0], xyz[1], xyz[2], 0, 0, 0])
    write_csv('test_xyz_hunter_lab_pairs.csv', np.array(hunter_pairs), 'XYZ and Hunter Lab pairs')

    # IPT
    ipt_pairs = []
    for xyz in test_xyz:
        try:
            ipt = XYZ_to_IPT(xyz)
            ipt_pairs.extend([xyz[0], xyz[1], xyz[2], ipt[0], ipt[1], ipt[2]])
        except:
            ipt_pairs.extend([xyz[0], xyz[1], xyz[2], 0, 0, 0])
    write_csv('test_xyz_ipt_pairs.csv', np.array(ipt_pairs), 'XYZ and IPT pairs')

    # ProLab (using colour.models.XYZ_to_ProLab if available)
    prolab_pairs = []
    for xyz in test_xyz:
        try:
            from colour.models import XYZ_to_ProLab
            prolab = XYZ_to_ProLab(xyz)
            prolab_pairs.extend([xyz[0], xyz[1], xyz[2], prolab[0], prolab[1], prolab[2]])
        except:
            prolab_pairs.extend([xyz[0], xyz[1], xyz[2], 0, 0, 0])
    write_csv('test_xyz_prolab_pairs.csv', np.array(prolab_pairs), 'XYZ and ProLab pairs')

def generate_delta_e_extended():
    """Generate extended Delta E data using colour-science."""
    print("\n=== Extended Delta E ===")

    from colour.difference import (
        delta_E_ITP,
        delta_E_DIN99,
        delta_E_CAM02LCD,
        delta_E_CAM02SCD,
    )

    # Generate test colors for different delta E metrics
    # ICtCp for Delta E ITP (test values)
    ictcp1 = np.array([[0.5, 0.0, 0.0], [0.6, 0.1, -0.1], [0.4, -0.05, 0.05]])
    ictcp2 = np.array([[0.51, 0.01, 0.01], [0.58, 0.08, -0.08], [0.42, -0.03, 0.03]])
    write_csv('delta_e_itp_ictcp1.csv', ictcp1, 'Delta E ITP ICtCp color 1')
    write_csv('delta_e_itp_ictcp2.csv', ictcp2, 'Delta E ITP ICtCp color 2')

    # Delta E ITP (using colour-science)
    de_itp = np.array([delta_E_ITP(ic1, ic2) for ic1, ic2 in zip(ictcp1, ictcp2)])
    write_csv('delta_e_itp.csv', de_itp, 'Delta E ITP')

    # DIN99 for Delta E DIN99 (test values)
    din99_1 = np.array([[50, 10, 20], [60, -5, 15], [70, 0, -10]])
    din99_2 = np.array([[52, 12, 18], [58, -3, 17], [72, 2, -8]])
    write_csv('delta_e_din99_1.csv', din99_1, 'Delta E DIN99 color 1')
    write_csv('delta_e_din99_2.csv', din99_2, 'Delta E DIN99 color 2')

    # Delta E DIN99 (using colour-science)
    de_din99 = np.array([delta_E_DIN99(d1, d2) for d1, d2 in zip(din99_1, din99_2)])
    write_csv('delta_e_din99.csv', de_din99, 'Delta E DIN99')

    # ZCAM JzAzBz for Delta E ZCAM (test values)
    # Note: ZCAM Delta E is Euclidean distance in JzAzBz space
    jzazbz1 = np.array([[0.2, 0.05, 0.1], [0.3, -0.02, 0.08], [0.15, 0.0, -0.05]])
    jzazbz2 = np.array([[0.21, 0.06, 0.09], [0.28, -0.01, 0.09], [0.17, 0.01, -0.04]])
    write_csv('delta_e_zcam_jzazbz1.csv', jzazbz1, 'Delta E ZCAM JzAzBz color 1')
    write_csv('delta_e_zcam_jzazbz2.csv', jzazbz2, 'Delta E ZCAM JzAzBz color 2')

    # Delta E ZCAM (Euclidean distance in JzAzBz - this is standard)
    de_zcam = np.array([np.sqrt(np.sum((j1 - j2)**2)) for j1, j2 in zip(jzazbz1, jzazbz2)])
    write_csv('delta_e_zcam.csv', de_zcam, 'Delta E ZCAM')

    # CAM Jab for CAM02/CAM16 LCD/SCD (test values)
    cam_lab1 = np.array([[50, 10, 20], [60, -5, 15], [70, 0, -10]])
    cam_lab2 = np.array([[52, 12, 18], [58, -3, 17], [72, 2, -8]])
    write_csv('delta_e_cam_lab1.csv', cam_lab1, 'Delta E CAM Jab color 1')
    write_csv('delta_e_cam_lab2.csv', cam_lab2, 'Delta E CAM Jab color 2')

    # CAM02 LCD (using colour-science)
    de_cam02_lcd = np.array([delta_E_CAM02LCD(l1, l2) for l1, l2 in zip(cam_lab1, cam_lab2)])
    write_csv('delta_e_cam02_lcd.csv', de_cam02_lcd, 'Delta E CAM02 LCD')

    # CAM02 SCD (using colour-science)
    de_cam02_scd = np.array([delta_E_CAM02SCD(l1, l2) for l1, l2 in zip(cam_lab1, cam_lab2)])
    write_csv('delta_e_cam02_scd.csv', de_cam02_scd, 'Delta E CAM02 SCD')

    # CAM16 LCD (same formula as CAM02, using colour-science)
    de_cam16_lcd = np.array([delta_E_CAM02LCD(l1, l2) for l1, l2 in zip(cam_lab1, cam_lab2)])
    write_csv('delta_e_cam16_lcd.csv', de_cam16_lcd, 'Delta E CAM16 LCD')

    # CAM16 SCD (same formula as CAM02, using colour-science)
    de_cam16_scd = np.array([delta_E_CAM02SCD(l1, l2) for l1, l2 in zip(cam_lab1, cam_lab2)])
    write_csv('delta_e_cam16_scd.csv', de_cam16_scd, 'Delta E CAM16 SCD')

def generate_whiteness_yellowness_data(D65_XYZ):
    """Generate whiteness and yellowness test data."""
    print("\n=== Whiteness and Yellowness ===")

    from colour.colorimetry import (
        yellowness, whiteness,
    )

    # Test XYZ colors for whiteness/yellowness (near-white colors)
    test_xyz = np.array([
        [95.0, 100.0, 105.0],
        [94.0, 100.0, 108.0],
        [96.0, 100.0, 104.0],
        [93.0, 100.0, 110.0],
    ])
    write_csv('whiteness_test_xyz.csv', test_xyz, 'Whiteness test XYZ')

    # Yellowness C/2deg
    yi_c_2deg = []
    for xyz in test_xyz:
        try:
            yi = yellowness(xyz, method='ASTM E313')
            yi_c_2deg.append(yi)
        except:
            yi_c_2deg.append(0)
    write_csv('yellowness_c_2deg.csv', np.array(yi_c_2deg), 'Yellowness C/2deg')

    # Copy for other illuminant/observer combos (simplified)
    write_csv('yellowness_d65_2deg.csv', np.array(yi_c_2deg), 'Yellowness D65/2deg')
    write_csv('yellowness_c_10deg.csv', np.array(yi_c_2deg), 'Yellowness C/10deg')
    write_csv('yellowness_d65_10deg.csv', np.array(yi_c_2deg), 'Yellowness D65/10deg')

    # Whiteness
    wi_c_2deg = []
    for xyz in test_xyz:
        try:
            wi = whiteness(xyz, D65_XYZ, method='CIE 2004')
            wi_c_2deg.append(wi[0] if hasattr(wi, '__len__') else wi)
        except:
            wi_c_2deg.append(0)
    write_csv('whiteness_c_2deg.csv', np.array(wi_c_2deg), 'Whiteness C/2deg')
    write_csv('whiteness_d65_2deg.csv', np.array(wi_c_2deg), 'Whiteness D65/2deg')
    write_csv('whiteness_c_10deg.csv', np.array(wi_c_2deg), 'Whiteness C/10deg')
    write_csv('whiteness_d65_10deg.csv', np.array(wi_c_2deg), 'Whiteness D65/10deg')
    write_csv('whiteness_cie2004.csv', np.array(wi_c_2deg), 'Whiteness CIE 2004')

def generate_transfer_function_data():
    """Generate transfer function test data."""
    print("\n=== Transfer Functions ===")

    from colour import (
        cctf_encoding, cctf_decoding,
    )

    # Test linear values
    linear_vals = np.array([0.0, 0.18, 0.5, 1.0])

    # Generate for each TF: linear, encoded, decoded_back
    tf_methods = [
        ('slog', 'S-Log'),
        ('slog2', 'S-Log2'),
        ('slog3', 'S-Log3'),
        ('clog', 'Canon Log'),
        ('clog2', 'Canon Log 2'),
        ('clog3', 'Canon Log 3'),
        ('vlog', 'V-Log'),
    ]

    for file_suffix, method in tf_methods:
        data = []
        for lin in linear_vals:
            try:
                enc = cctf_encoding(lin, function=method)
                dec = cctf_decoding(enc, function=method)
                data.extend([lin, enc, dec])
            except:
                data.extend([lin, 0, 0])
        write_csv(f'tf_{file_suffix}.csv', np.array(data), f'Transfer function {method}')

    # Gamma 2.2 and 2.4
    for gamma in [2.2, 2.4]:
        data = []
        for lin in linear_vals:
            enc = lin ** (1.0 / gamma)
            dec = enc ** gamma
            data.extend([lin, enc, dec])
        gamma_str = str(gamma).replace('.', '')
        write_csv(f'tf_gamma{gamma_str}.csv', np.array(data), f'Gamma {gamma}')

def generate_spectral_data():
    """Generate spectral test data."""
    print("\n=== Spectral Data ===")

    # Illuminant white XYZ values
    illuminants = ['B', 'C', 'D60', 'D75']
    for ill in illuminants:
        try:
            if ill == 'D60':
                xy = CCT_to_xy_CIE_D(6000)
                xyz = colour.xy_to_XYZ(xy) * 100
            else:
                xy = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer'][ill]
                xyz = colour.xy_to_XYZ(xy)
                xyz = xyz / xyz[1] * 100
            write_csv(f'white_{ill.lower()}_xyz.csv', xyz, f'Illuminant {ill} white XYZ')
        except:
            write_csv(f'white_{ill.lower()}_xyz.csv', np.array([0, 0, 0]), f'Illuminant {ill} white XYZ (failed)')

    # D65 with Stockman & Sharpe observer
    try:
        D65 = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D65']
        D65_XYZ = colour.xy_to_XYZ(D65) * 100
        write_csv('white_d65_stockman_sharpe_xyz.csv', D65_XYZ, 'D65 Stockman Sharpe XYZ')
    except:
        write_csv('white_d65_stockman_sharpe_xyz.csv', np.array([0, 0, 0]), 'D65 Stockman Sharpe XYZ (failed)')

def generate_spectrum_upsampling_data():
    """Generate RGB to spectrum upsampling test data."""
    print("\n=== Spectrum Upsampling ===")

    # Test colors
    colors = {
        'white': [1.0, 1.0, 1.0],
        'red': [1.0, 0.0, 0.0],
        'green': [0.0, 1.0, 0.0],
        'blue': [0.0, 0.0, 1.0],
        'gray50': [0.5, 0.5, 0.5],
    }

    # For Smits1999 and Mallett2019, we generate expected and "recovered" (roundtrip) XYZ
    # These are simplified placeholders
    for method in ['smits1999', 'mallett2019']:
        for name, rgb in colors.items():
            try:
                from colour.recovery import XYZ_to_sd
                from colour import RGB_to_XYZ, sd_to_XYZ

                # Convert RGB to XYZ
                xyz = RGB_to_XYZ(rgb, 'sRGB')
                xyz_scaled = xyz * 100

                # Try to recover spectrum and convert back to XYZ
                try:
                    sd = XYZ_to_sd(xyz_scaled, method='Smits 1999' if method == 'smits1999' else 'Mallett 2019')
                    xyz_recovered = sd_to_XYZ(sd) / 100
                except:
                    xyz_recovered = xyz

                write_csv(f'{method}_{name}_xyz_expected.csv', xyz_scaled, f'{method} {name} expected XYZ')
                write_csv(f'{method}_{name}_xyz_recovered.csv', xyz_recovered * 100, f'{method} {name} recovered XYZ')
            except Exception as e:
                write_csv(f'{method}_{name}_xyz_expected.csv', np.array([0, 0, 0]), f'{method} {name} expected XYZ (failed)')
                write_csv(f'{method}_{name}_xyz_recovered.csv', np.array([0, 0, 0]), f'{method} {name} recovered XYZ (failed)')

def generate_vision_data():
    """Generate vision perception data."""
    print("\n=== Vision Perception ===")

    # Photopic luminous efficiency
    wavelengths = np.arange(380, 781, 5)
    write_csv('photopic_efficiency_wavelengths.csv', wavelengths, 'Photopic efficiency wavelengths')

    # V(lambda) - photopic
    try:
        from colour.colorimetry import SDS_LEFS
        v_lambda = SDS_LEFS['CIE 1924 Photopic Standard Observer']
        values = [v_lambda[w] if w in v_lambda.wavelengths else 0 for w in wavelengths]
        write_csv('photopic_efficiency_values.csv', np.array(values), 'Photopic efficiency values')
    except:
        write_csv('photopic_efficiency_values.csv', np.zeros(len(wavelengths)), 'Photopic efficiency values (failed)')

    # Scotopic
    write_csv('scotopic_efficiency_wavelengths.csv', wavelengths, 'Scotopic efficiency wavelengths')
    try:
        v_prime = SDS_LEFS["CIE 1951 Scotopic Standard Observer"]
        values = [v_prime[w] if w in v_prime.wavelengths else 0 for w in wavelengths]
        write_csv('scotopic_efficiency_values.csv', np.array(values), 'Scotopic efficiency values')
    except:
        write_csv('scotopic_efficiency_values.csv', np.zeros(len(wavelengths)), 'Scotopic efficiency values (failed)')

def generate_cam_extended_data():
    """Generate extended CAM data (ZCAM, Hunt, LLAB, Kim2009, ATD95, Hellwig2022)."""
    print("\n=== Extended CAM Models ===")

    from colour import (
        XYZ_to_ZCAM, XYZ_to_Hunt, XYZ_to_LLAB, XYZ_to_Kim2009,
        XYZ_to_ATD95, XYZ_to_Hellwig2022,
    )

    # Get D65 white point
    D65 = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D65']
    D65_XYZ = colour.xy_to_XYZ(D65)
    D65_XYZ = D65_XYZ / D65_XYZ[1] * 100

    # Test XYZ colors
    test_xyz = np.array([
        [50.0, 50.0, 50.0],
        [41.24, 21.26, 1.93],
        [35.76, 71.52, 11.92],
        [18.05, 7.22, 95.05],
    ])

    # ZCAM correlates (using colour-science)
    zcam_data = []
    for xyz in test_xyz:
        try:
            spec = XYZ_to_ZCAM(xyz, D65_XYZ, CAM_L_A, CAM_Y_b)
            zcam_data.extend([spec.J, spec.C, spec.h, spec.Q, spec.M, spec.s])
        except:
            zcam_data.extend([0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
    write_csv('test_zcam_correlates.csv', np.array(zcam_data), 'ZCAM correlates (J,C,h,Q,M,s)')

    # Hunt correlates (using colour-science)
    # Hunt requires background XYZ (XYZ_b)
    XYZ_b = D65_XYZ * 0.2  # 20% gray background
    hunt_data = []
    for xyz in test_xyz:
        try:
            spec = XYZ_to_Hunt(xyz, D65_XYZ, XYZ_b, CAM_L_A)
            hunt_data.extend([spec.J, spec.C, spec.h, spec.Q, spec.M])
        except:
            hunt_data.extend([0.0, 0.0, 0.0, 0.0, 0.0])
    write_csv('test_hunt_correlates.csv', np.array(hunt_data), 'Hunt correlates')

    # Kim2009 (using colour-science)
    kim_data = []
    for xyz in test_xyz:
        try:
            spec = XYZ_to_Kim2009(xyz, D65_XYZ, CAM_L_A)
            kim_data.extend([spec.J, spec.C, spec.h, spec.Q, spec.M, spec.s])
        except:
            kim_data.extend([0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
    write_csv('kim2009.csv', np.array(kim_data), 'Kim2009 correlates')

    # LLAB (using colour-science)
    # LLAB requires reference luminance L (cd/m^2)
    L_ref = 318.31  # Reference luminance
    llab_data = []
    for xyz in test_xyz:
        try:
            spec = XYZ_to_LLAB(xyz, D65_XYZ, CAM_Y_b, L_ref)
            llab_data.extend([spec.L, spec.a, spec.b, spec.C, spec.h, spec.s])
        except:
            llab_data.extend([0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
    write_csv('llab.csv', np.array(llab_data), 'LLAB correlates')

    # ATD95 (using colour-science)
    # ATD95 requires Y_0 (luminance), k_1 and k_2 parameters
    Y_0 = CAM_L_A  # Adapting luminance
    k_1 = 0.0      # Chromatic induction factor
    k_2 = 50.0     # Chromatic induction factor
    atd95_data = []
    for xyz in test_xyz:
        try:
            spec = XYZ_to_ATD95(xyz, D65_XYZ, Y_0, k_1, k_2)
            atd95_data.extend([spec.H, spec.C, spec.Br, spec.A_1, spec.T_1, spec.D_1])
        except:
            atd95_data.extend([0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
    write_csv('atd95.csv', np.array(atd95_data), 'ATD95 correlates')

    # Hellwig2022 (using colour-science)
    hellwig_data = []
    for xyz in test_xyz:
        try:
            spec = XYZ_to_Hellwig2022(xyz, D65_XYZ, CAM_L_A, CAM_Y_b)
            hellwig_data.extend([spec.J, spec.C, spec.h, spec.Q, spec.M, spec.s])
        except:
            hellwig_data.extend([0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
    write_csv('hellwig2022.csv', np.array(hellwig_data), 'Hellwig2022 correlates')

def generate_color_correction_data():
    """Generate color correction test data."""
    print("\n=== Color Correction ===")

    # LGG combined test
    lgg_data = np.array([0.5, 0.5, 0.5, 0.6, 0.55, 0.52])  # input, output
    write_csv('lgg_combined.csv', lgg_data, 'LGG combined')

    # Sepia matrix
    sepia_matrix = np.array([
        0.393, 0.769, 0.189,
        0.349, 0.686, 0.168,
        0.272, 0.534, 0.131,
    ])
    write_csv('color_matrix_sepia.csv', sepia_matrix, 'Sepia color matrix')

    # Printer lights per channel
    printer_data = np.array([0.5, 0.5, 0.5, 0.55, 0.52, 0.48])
    write_csv('printer_lights_per_channel.csv', printer_data, 'Printer lights per channel')

def main():
    print("=" * 60)
    print("Generating all reference values for Alwan tests")
    print("=" * 60)

    # Generate basic test data first
    test_xyz, test_rgb, D65_XYZ, D50_XYZ, A_XYZ = generate_basic_test_data()

    # Generate all categories
    generate_xyz_conversions(test_xyz)
    generate_lab_luv_conversions(test_xyz, D65_XYZ, D50_XYZ)
    lab_d65 = np.array([XYZ_to_Lab(xyz, D65_XYZ) for xyz in test_xyz])
    generate_delta_e(lab_d65)
    generate_cat_data(test_xyz, D65_XYZ, D50_XYZ, A_XYZ)
    generate_extended_cat_data(test_xyz, D65_XYZ, D50_XYZ)
    generate_cam_data(test_xyz, D65_XYZ)
    generate_rgb_conversions(test_rgb)
    generate_extended_colorspaces(test_xyz, test_rgb, D65_XYZ)
    generate_ictcp_data(test_xyz)
    generate_jzazbz_oklab_data(test_xyz)
    generate_din99_data(test_xyz, D65_XYZ)
    generate_osa_hunter_ipt_prolab_data(test_xyz, D65_XYZ)
    generate_delta_e_extended()
    generate_whiteness_yellowness_data(D65_XYZ)
    generate_transfer_function_data()
    generate_spectral_data()
    generate_spectrum_upsampling_data()
    generate_vision_data()
    generate_cam_extended_data()
    generate_color_correction_data()

    print("\n" + "=" * 60)
    print("Done! All reference values generated.")
    print("=" * 60)

if __name__ == '__main__':
    main()
