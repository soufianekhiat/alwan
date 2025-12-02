#!/usr/bin/env python3
"""
Generate test reference data for Rayleigh scattering functions.
All data generated using colour-science library.

Reference: Bodhaine et al. (1999)
"""

import numpy as np
import os
import sys

# Add parent directory to path for common utilities
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import colour
from colour.phenomena.rayleigh import (
    scattering_cross_section,
    rayleigh_optical_depth,
    CONSTANT_STANDARD_CO2_CONCENTRATION,
    CONSTANT_STANDARD_AIR_TEMPERATURE,
    CONSTANT_AVERAGE_PRESSURE_MEAN_SEA_LEVEL,
    CONSTANT_DEFAULT_LATITUDE,
    CONSTANT_DEFAULT_ALTITUDE,
)


def save_csv(filepath, data, flatten=True):
    """Save data to CSV file in the format expected by C tests."""
    if flatten:
        data = np.array(data).flatten()
    np.savetxt(filepath, data, delimiter=',', fmt='%.16e', newline=',')
    # Remove trailing comma and add newline
    with open(filepath, 'r') as f:
        content = f.read()
    with open(filepath, 'w') as f:
        f.write(content.rstrip(',') + '\n')


def generate_cross_section_tests(output_dir):
    """Generate test data for scattering cross section function."""

    # Test wavelengths in nm
    wavelengths_nm = np.array([360, 380, 400, 420, 450, 480, 500, 520,
                               550, 555, 580, 600, 620, 650, 680, 700,
                               720, 750, 780])

    # Save input wavelengths
    save_csv(os.path.join(output_dir, 'rayleigh_wavelengths_nm.csv'), wavelengths_nm)

    # Convert to centimeters for colour-science
    wavelengths_cm = wavelengths_nm * 1e-7

    # Compute cross sections with default parameters
    cross_sections = []
    for wl_cm in wavelengths_cm:
        sigma = scattering_cross_section(
            wl_cm,
            CO2_concentration=CONSTANT_STANDARD_CO2_CONCENTRATION,
            temperature=CONSTANT_STANDARD_AIR_TEMPERATURE
        )
        cross_sections.append(sigma)

    save_csv(os.path.join(output_dir, 'rayleigh_cross_section_default.csv'), cross_sections)

    # Test with different CO2 concentrations
    co2_levels = [280, 300, 350, 400, 450]  # ppm
    save_csv(os.path.join(output_dir, 'rayleigh_co2_levels.csv'), co2_levels)

    # At 555nm with different CO2 levels
    wl_555_cm = 555e-7
    cross_sections_co2 = []
    for co2 in co2_levels:
        sigma = scattering_cross_section(wl_555_cm, CO2_concentration=co2)
        cross_sections_co2.append(sigma)

    save_csv(os.path.join(output_dir, 'rayleigh_cross_section_co2_555nm.csv'), cross_sections_co2)

    print(f"Generated cross section test data")


def generate_optical_depth_tests(output_dir):
    """Generate test data for Rayleigh optical depth function."""

    # Load wavelengths from previous function
    wavelengths_nm = np.array([360, 380, 400, 420, 450, 480, 500, 520,
                               550, 555, 580, 600, 620, 650, 680, 700,
                               720, 750, 780])
    wavelengths_cm = wavelengths_nm * 1e-7

    # Compute optical depth with default parameters
    optical_depths = []
    for wl_cm in wavelengths_cm:
        depth = rayleigh_optical_depth(
            wl_cm,
            CO2_concentration=CONSTANT_STANDARD_CO2_CONCENTRATION,
            temperature=CONSTANT_STANDARD_AIR_TEMPERATURE,
            pressure=CONSTANT_AVERAGE_PRESSURE_MEAN_SEA_LEVEL,
            latitude=CONSTANT_DEFAULT_LATITUDE,
            altitude=CONSTANT_DEFAULT_ALTITUDE
        )
        optical_depths.append(depth)

    save_csv(os.path.join(output_dir, 'rayleigh_optical_depth_default.csv'), optical_depths)

    # Test with different pressures at 555nm
    pressures = [50000, 75000, 101325, 110000]  # Pa
    save_csv(os.path.join(output_dir, 'rayleigh_pressures.csv'), pressures)

    wl_555_cm = 555e-7
    optical_depths_pressure = []
    for p in pressures:
        depth = rayleigh_optical_depth(wl_555_cm, pressure=p)
        optical_depths_pressure.append(depth)

    save_csv(os.path.join(output_dir, 'rayleigh_optical_depth_pressure_555nm.csv'), optical_depths_pressure)

    # Test with different latitudes at 555nm
    latitudes = [0, 15, 30, 45, 60, 75, 90]  # degrees
    save_csv(os.path.join(output_dir, 'rayleigh_latitudes.csv'), latitudes)

    optical_depths_latitude = []
    for lat in latitudes:
        depth = rayleigh_optical_depth(wl_555_cm, latitude=lat)
        optical_depths_latitude.append(depth)

    save_csv(os.path.join(output_dir, 'rayleigh_optical_depth_latitude_555nm.csv'), optical_depths_latitude)

    # Test with different altitudes at 555nm
    altitudes = [0, 500, 1000, 1500, 2000, 3000, 5000]  # meters
    save_csv(os.path.join(output_dir, 'rayleigh_altitudes.csv'), altitudes)

    optical_depths_altitude = []
    for alt in altitudes:
        depth = rayleigh_optical_depth(wl_555_cm, altitude=alt)
        optical_depths_altitude.append(depth)

    save_csv(os.path.join(output_dir, 'rayleigh_optical_depth_altitude_555nm.csv'), optical_depths_altitude)

    # Test with combined parameters
    # Paris: lat=48.8567, alt=35m
    depth_paris = rayleigh_optical_depth(wl_555_cm, latitude=48.8567, altitude=35.0)

    # Denver: lat=39.7392, alt=1609m
    depth_denver = rayleigh_optical_depth(wl_555_cm, latitude=39.7392, altitude=1609.0)

    # High altitude site: lat=0, alt=5000m
    depth_high = rayleigh_optical_depth(wl_555_cm, latitude=0, altitude=5000.0)

    combined_results = [depth_paris, depth_denver, depth_high]
    save_csv(os.path.join(output_dir, 'rayleigh_optical_depth_combined.csv'), combined_results)

    # Save combined params for C test (lat, alt pairs)
    combined_params = [48.8567, 35.0, 39.7392, 1609.0, 0, 5000.0]
    save_csv(os.path.join(output_dir, 'rayleigh_combined_params.csv'), combined_params)

    print(f"Generated optical depth test data")


def generate_spd_tests(output_dir):
    """Generate test data for SPD (spectral distribution) function."""

    # Standard visible range
    wl_start = 360
    wl_end = 780
    wl_step = 10

    wavelengths_nm = np.arange(wl_start, wl_end + 1, wl_step)
    wavelengths_cm = wavelengths_nm * 1e-7

    # Save SPD parameters
    spd_params = [wl_start, wl_end, wl_step]
    save_csv(os.path.join(output_dir, 'rayleigh_spd_params.csv'), spd_params)

    # Compute SPD with default parameters
    spd_values = []
    for wl_cm in wavelengths_cm:
        depth = rayleigh_optical_depth(wl_cm)
        spd_values.append(depth)

    save_csv(os.path.join(output_dir, 'rayleigh_spd_default.csv'), spd_values)
    save_csv(os.path.join(output_dir, 'rayleigh_spd_wavelengths.csv'), wavelengths_nm)

    print(f"Generated SPD test data ({len(spd_values)} values)")


def main():
    if len(sys.argv) < 2:
        output_dir = os.path.join(os.path.dirname(__file__), '..', '..',
                                   'tests', 'reference_values')
    else:
        output_dir = sys.argv[1]

    os.makedirs(output_dir, exist_ok=True)

    print(f"Generating Rayleigh scattering test data to: {output_dir}")
    print(f"Using colour-science version: {colour.__version__}")

    # Print constants for verification
    print(f"\nConstants from colour-science:")
    print(f"  STANDARD_CO2_CONCENTRATION = {CONSTANT_STANDARD_CO2_CONCENTRATION}")
    print(f"  STANDARD_AIR_TEMPERATURE = {CONSTANT_STANDARD_AIR_TEMPERATURE}")
    print(f"  AVERAGE_PRESSURE_MEAN_SEA_LEVEL = {CONSTANT_AVERAGE_PRESSURE_MEAN_SEA_LEVEL}")
    print(f"  DEFAULT_LATITUDE = {CONSTANT_DEFAULT_LATITUDE}")
    print(f"  DEFAULT_ALTITUDE = {CONSTANT_DEFAULT_ALTITUDE}")
    print()

    generate_cross_section_tests(output_dir)
    generate_optical_depth_tests(output_dir)
    generate_spd_tests(output_dir)

    print("\nAll Rayleigh scattering test data generated successfully!")


if __name__ == '__main__':
    main()
