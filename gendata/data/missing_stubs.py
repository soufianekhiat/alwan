"""
Generate stub data for files not available in colour-science.

Creates placeholder data to allow compilation:
- F-series illuminants (F1-F12): Use D65 as fallback
- CES samples (01-80): Use neutral gray reflectance
- Extended D-series (D40, D45, D93): Calculate xy using CIE formula, SPD using D65 fallback
- ARRI LogC4: Use ARRI Wide Gamut 4 as fallback
- REDLog: Use REDWideGamutRGB as fallback
- S-Log: Use S-Gamut3.Cine as fallback
- S-Log2: Use S-Gamut as fallback
- S-Log3: Use S-Gamut3 as fallback
"""

import sys
import os
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
from common import save_vector

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)

def calculate_d_series_xy(cct):
    """Calculate CIE D-series xy chromaticity from CCT."""
    # CIE D-series formula from CIE 15:2004
    if 4000 <= cct <= 7000:
        xd = -4.6070e9/(cct**3) + 2.9678e6/(cct**2) + 0.09911e3/cct + 0.244063
    elif 7000 < cct <= 25000:
        xd = -2.0064e9/(cct**3) + 1.9018e6/(cct**2) + 0.24748e3/cct + 0.23704
    else:
        raise ValueError(f"CCT {cct}K outside valid range (4000-25000K)")

    yd = -3.000 * xd**2 + 2.870 * xd - 0.275
    return [xd, yd]

def generate_missing_stubs(output_dir):
    """Generate all missing stub data files."""

    print("\nGenerating Missing Stub Data")
    print("=" * 70)

    os.makedirs(os.path.join(output_dir, 'illuminants'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'illuminants_xy'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'fixtures'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'rgb_spaces'), exist_ok=True)

    file_count = 0

    # =================================================================
    # 1. F-series Illuminants (F1-F12): Use D65 as fallback
    # =================================================================
    print("\n[1/8] F-series illuminants (using D65 fallback)...")

    # Get D65 SPD as fallback
    d65_spd = colour.SDS_ILLUMINANTS['D65']
    wavelengths = np.arange(360, 831, 1)
    d65_values = [float(d65_spd[wl]) for wl in wavelengths]

    for i in range(1, 13):
        filename = f"F{i}_360_830_1nm.csv"
        filepath = os.path.join(output_dir, 'illuminants', filename)
        save_vector(d65_values, filepath, f"F{i} SPD (D65 fallback)")
        file_count += 1

    print(f"  [OK] 12 F-series illuminant SPDs generated (D65 fallback)")

    # =================================================================
    # 2. CES Samples (01-80): Neutral gray reflectance
    # =================================================================
    print("\n[2/8] CES samples (using neutral gray reflectance)...")

    # CES samples: 360-830nm @ 5nm = 95 values
    # Use flat 50% gray reflectance as placeholder
    ces_values = [0.5] * 95

    for i in range(1, 81):
        filename = f"ces_{i:02d}_reflectance.csv"
        filepath = os.path.join(output_dir, 'fixtures', filename)
        save_vector(ces_values, filepath, f"CES {i:02d} reflectance (neutral gray stub)")
        file_count += 1

    print(f"  [OK] 80 CES reflectance samples generated (neutral gray stubs)")

    # =================================================================
    # 3. Extended D-series xy (D40, D45, D93): Calculate using CIE formula
    # =================================================================
    print("\n[3/8] Extended D-series xy coordinates...")

    # CCT mapping for D-series
    d_series_cct = {
        'D40': 4000,
        'D45': 4500,
        'D93': 9300
    }

    for name, cct in d_series_cct.items():
        xy = calculate_d_series_xy(cct)
        filename = f"{name.lower()}_xy.csv"
        filepath = os.path.join(output_dir, 'illuminants_xy', filename)
        save_vector(xy, filepath, f"{name} xy (calculated from CCT={cct}K)")
        file_count += 1

    print(f"  [OK] 3 extended D-series xy coordinates generated")

    # =================================================================
    # 4. Extended D-series SPDs (D40, D45, D93): Use D65 as fallback
    # =================================================================
    print("\n[4/8] Extended D-series SPDs (using D65 fallback)...")

    # Use D65 SPD as fallback (already loaded above)
    for name in d_series_cct.keys():
        filename = f"{name}_360_830_1nm.csv"  # Capital D for SPD files
        filepath = os.path.join(output_dir, 'illuminants', filename)
        save_vector(d65_values, filepath, f"{name} SPD (D65 fallback)")
        file_count += 1

    print(f"  [OK] 3 extended D-series SPDs generated (D65 fallback)")

    # =================================================================
    # 5. ARRI LogC4: Use ARRI Wide Gamut 4
    # =================================================================
    print("\n[5/8] ARRI LogC4 RGB space (using ARRI Wide Gamut 4)...")

    try:
        # Use ARRI Wide Gamut 4 as fallback
        arri_wg4 = colour.RGB_COLOURSPACES['ARRI Wide Gamut 4']
        primaries = arri_wg4.primaries
        whitepoint = arri_wg4.whitepoint

        # Flatten to 8 values: Rx, Ry, Gx, Gy, Bx, By, Wx, Wy
        arri_flat = [
            primaries[0][0], primaries[0][1],  # Red xy
            primaries[1][0], primaries[1][1],  # Green xy
            primaries[2][0], primaries[2][1],  # Blue xy
            whitepoint[0], whitepoint[1]       # White xy
        ]

        filepath = os.path.join(output_dir, 'rgb_spaces', 'arri_logc4.csv')
        save_vector(arri_flat, filepath, "ARRI LogC4 (ARRI Wide Gamut 4 fallback)")
        file_count += 1
        print(f"  [OK] ARRI LogC4 RGB space generated (ARRI Wide Gamut 4 fallback)")
    except Exception as e:
        print(f"  [ERROR] Failed to generate ARRI LogC4: {e}")

    # =================================================================
    # 6. REDLog: Use REDWideGamutRGB
    # =================================================================
    print("\n[6/8] REDLog RGB space (using REDWideGamutRGB)...")

    try:
        # Use REDWideGamutRGB as fallback
        red_wg = colour.RGB_COLOURSPACES['REDWideGamutRGB']
        primaries = red_wg.primaries
        whitepoint = red_wg.whitepoint

        # Flatten to 8 values: Rx, Ry, Gx, Gy, Bx, By, Wx, Wy
        red_flat = [
            primaries[0][0], primaries[0][1],  # Red xy
            primaries[1][0], primaries[1][1],  # Green xy
            primaries[2][0], primaries[2][1],  # Blue xy
            whitepoint[0], whitepoint[1]       # White xy
        ]

        filepath = os.path.join(output_dir, 'rgb_spaces', 'redlog.csv')
        save_vector(red_flat, filepath, "REDLog (REDWideGamutRGB fallback)")
        file_count += 1
        print(f"  [OK] REDLog RGB space generated (REDWideGamutRGB fallback)")
    except Exception as e:
        print(f"  [ERROR] Failed to generate REDLog: {e}")

    # =================================================================
    # 7. S-Log: Use S-Gamut3.Cine
    # =================================================================
    print("\n[7/8] S-Log RGB space (using S-Gamut3.Cine)...")

    try:
        # Use S-Gamut3.Cine as fallback
        sgamut3_cine = colour.RGB_COLOURSPACES['S-Gamut3.Cine']
        primaries = sgamut3_cine.primaries
        whitepoint = sgamut3_cine.whitepoint

        # Flatten to 8 values: Rx, Ry, Gx, Gy, Bx, By, Wx, Wy
        slog_flat = [
            primaries[0][0], primaries[0][1],  # Red xy
            primaries[1][0], primaries[1][1],  # Green xy
            primaries[2][0], primaries[2][1],  # Blue xy
            whitepoint[0], whitepoint[1]       # White xy
        ]

        filepath = os.path.join(output_dir, 'rgb_spaces', 's-log.csv')
        save_vector(slog_flat, filepath, "S-Log (S-Gamut3.Cine fallback)")
        file_count += 1
        print(f"  [OK] S-Log RGB space generated (S-Gamut3.Cine fallback)")
    except Exception as e:
        print(f"  [ERROR] Failed to generate S-Log: {e}")

    # =================================================================
    # 8. S-Log2: Use S-Gamut
    # =================================================================
    print("\n[8/9] S-Log2 RGB space (using S-Gamut)...")

    try:
        # Use S-Gamut as fallback
        sgamut = colour.RGB_COLOURSPACES['S-Gamut']
        primaries = sgamut.primaries
        whitepoint = sgamut.whitepoint

        # Flatten to 8 values: Rx, Ry, Gx, Gy, Bx, By, Wx, Wy
        slog2_flat = [
            primaries[0][0], primaries[0][1],  # Red xy
            primaries[1][0], primaries[1][1],  # Green xy
            primaries[2][0], primaries[2][1],  # Blue xy
            whitepoint[0], whitepoint[1]       # White xy
        ]

        filepath = os.path.join(output_dir, 'rgb_spaces', 's-log2.csv')
        save_vector(slog2_flat, filepath, "S-Log2 (S-Gamut fallback)")
        file_count += 1
        print(f"  [OK] S-Log2 RGB space generated (S-Gamut fallback)")
    except Exception as e:
        print(f"  [ERROR] Failed to generate S-Log2: {e}")

    # =================================================================
    # 9. S-Log3: Use S-Gamut3
    # =================================================================
    print("\n[9/10] S-Log3 RGB space (using S-Gamut3)...")

    try:
        # Use S-Gamut3 as fallback
        sgamut3 = colour.RGB_COLOURSPACES['S-Gamut3']
        primaries = sgamut3.primaries
        whitepoint = sgamut3.whitepoint

        # Flatten to 8 values: Rx, Ry, Gx, Gy, Bx, By, Wx, Wy
        slog3_flat = [
            primaries[0][0], primaries[0][1],  # Red xy
            primaries[1][0], primaries[1][1],  # Green xy
            primaries[2][0], primaries[2][1],  # Blue xy
            whitepoint[0], whitepoint[1]       # White xy
        ]

        filepath = os.path.join(output_dir, 'rgb_spaces', 's-log3.csv')
        save_vector(slog3_flat, filepath, "S-Log3 (S-Gamut3 fallback)")
        file_count += 1
        print(f"  [OK] S-Log3 RGB space generated (S-Gamut3 fallback)")
    except Exception as e:
        print(f"  [ERROR] Failed to generate S-Log3: {e}")

    # =================================================================
    # 10. FilmLight T-Log: Use FilmLight E-Gamut
    # =================================================================
    print("\n[10/10] FilmLight T-Log RGB space (using FilmLight E-Gamut)...")

    try:
        # Use FilmLight E-Gamut as fallback
        filmlight_egamut = colour.RGB_COLOURSPACES['FilmLight E-Gamut']
        primaries = filmlight_egamut.primaries
        whitepoint = filmlight_egamut.whitepoint

        # Flatten to 8 values: Rx, Ry, Gx, Gy, Bx, By, Wx, Wy
        tlog_flat = [
            primaries[0][0], primaries[0][1],  # Red xy
            primaries[1][0], primaries[1][1],  # Green xy
            primaries[2][0], primaries[2][1],  # Blue xy
            whitepoint[0], whitepoint[1]       # White xy
        ]

        filepath = os.path.join(output_dir, 'rgb_spaces', 'filmlight_t-log.csv')
        save_vector(tlog_flat, filepath, "FilmLight T-Log (FilmLight E-Gamut fallback)")
        file_count += 1
        print(f"  [OK] FilmLight T-Log RGB space generated (FilmLight E-Gamut fallback)")
    except Exception as e:
        print(f"  [ERROR] Failed to generate FilmLight T-Log: {e}")

    print("\n" + "=" * 70)
    print(f"Missing stub data generation complete!")
    print(f"  Total files generated: {file_count}")
    print(f"")
    print(f"NOTE: These are placeholder/fallback values for compilation.")
    print(f"      F-series uses D65, CES uses neutral gray, D-series calculated,")
    print(f"      ARRI LogC4 uses ARRI Wide Gamut 4, REDLog uses REDWideGamutRGB,")
    print(f"      S-Log uses S-Gamut3.Cine, S-Log2 uses S-Gamut, S-Log3 uses S-Gamut3,")
    print(f"      FilmLight T-Log uses FilmLight E-Gamut.")
    print("=" * 70)

    return True

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python missing_stubs.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    success = generate_missing_stubs(output_dir)
    sys.exit(0 if success else 1)
