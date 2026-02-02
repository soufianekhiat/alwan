#!/usr/bin/env python3
"""
Generate test reference data for Barten 1999 CSF functions.
All data generated using colour-science library.

Reference: Barten (1999)
"""

import numpy as np
import os
import sys

import colour
from colour.contrast import (
    pupil_diameter_Barten1999,
    retinal_illuminance_Barten1999,
    optical_MTF_Barten1999,
    sigma_Barten1999,
    maximum_angular_size_Barten1999,
    contrast_sensitivity_function_Barten1999,
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


def generate_pupil_diameter_tests(output_dir):
    """Generate test data for pupil_diameter_Barten1999."""
    print("Generating pupil diameter tests...")

    # Test luminances
    luminances = np.array([0.01, 0.1, 1.0, 10.0, 100.0, 1000.0, 10000.0])
    save_csv(os.path.join(output_dir, 'barten_luminances.csv'), luminances)

    # Test with default X_0=60, Y_0=60
    pupil_default = []
    for L in luminances:
        d = pupil_diameter_Barten1999(L, 60, 60)
        pupil_default.append(d)
    save_csv(os.path.join(output_dir, 'barten_pupil_diameter_default.csv'), pupil_default)

    # Test with different angular sizes
    angular_sizes = np.array([10, 30, 60, 90, 120])
    save_csv(os.path.join(output_dir, 'barten_angular_sizes.csv'), angular_sizes)

    pupil_angular = []
    for X_0 in angular_sizes:
        d = pupil_diameter_Barten1999(100, X_0, X_0)  # L=100
        pupil_angular.append(d)
    save_csv(os.path.join(output_dir, 'barten_pupil_diameter_angular.csv'), pupil_angular)


def generate_retinal_illuminance_tests(output_dir):
    """Generate test data for retinal_illuminance_Barten1999."""
    print("Generating retinal illuminance tests...")

    luminances = np.array([0.01, 0.1, 1.0, 10.0, 100.0, 1000.0])
    pupil_diameters = np.array([1.0, 2.0, 2.1, 3.0, 4.0, 5.0])

    save_csv(os.path.join(output_dir, 'barten_pupil_diameters.csv'), pupil_diameters)

    # With Stiles-Crawford correction
    retinal_sc = []
    for L, d in zip(luminances, pupil_diameters):
        E = retinal_illuminance_Barten1999(L, d, True)
        retinal_sc.append(E)
    save_csv(os.path.join(output_dir, 'barten_retinal_illuminance_sc.csv'), retinal_sc)

    # Without Stiles-Crawford correction
    retinal_no_sc = []
    for L, d in zip(luminances, pupil_diameters):
        E = retinal_illuminance_Barten1999(L, d, False)
        retinal_no_sc.append(E)
    save_csv(os.path.join(output_dir, 'barten_retinal_illuminance_no_sc.csv'), retinal_no_sc)

    # Fixed L=100, varying d
    retinal_fixed_L = []
    for d in pupil_diameters:
        E = retinal_illuminance_Barten1999(100, d, True)
        retinal_fixed_L.append(E)
    save_csv(os.path.join(output_dir, 'barten_retinal_illuminance_fixed_L.csv'), retinal_fixed_L)


def generate_optical_mtf_tests(output_dir):
    """Generate test data for optical_MTF_Barten1999."""
    print("Generating optical MTF tests...")

    frequencies = np.array([0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0])
    sigmas = np.array([0.005, 0.008, 0.01, 0.012, 0.015, 0.02])

    save_csv(os.path.join(output_dir, 'barten_frequencies.csv'), frequencies)
    save_csv(os.path.join(output_dir, 'barten_sigmas.csv'), sigmas)

    # MTF with default sigma=0.01
    mtf_default = []
    for u in frequencies:
        M = optical_MTF_Barten1999(u, 0.01)
        mtf_default.append(M)
    save_csv(os.path.join(output_dir, 'barten_mtf_default.csv'), mtf_default)

    # MTF at u=4 with different sigmas
    mtf_u4 = []
    for sigma in sigmas:
        M = optical_MTF_Barten1999(4, sigma)
        mtf_u4.append(M)
    save_csv(os.path.join(output_dir, 'barten_mtf_u4.csv'), mtf_u4)


def generate_sigma_tests(output_dir):
    """Generate test data for sigma_Barten1999."""
    print("Generating sigma tests...")

    pupil_diameters = np.array([1.0, 1.5, 2.0, 2.1, 2.5, 3.0, 4.0, 5.0])

    # Default sigma_0=0.5/60, C_ab=0.08/60
    sigma_0 = 0.5 / 60
    C_ab = 0.08 / 60

    sigma_results = []
    for d in pupil_diameters:
        s = sigma_Barten1999(sigma_0, C_ab, d)
        sigma_results.append(s)
    save_csv(os.path.join(output_dir, 'barten_sigma_default.csv'), sigma_results)

    # Different sigma_0 values at d=2.1
    sigma_0_values = np.array([0.3, 0.4, 0.5, 0.6, 0.7]) / 60
    save_csv(os.path.join(output_dir, 'barten_sigma_0_values.csv'), sigma_0_values)

    sigma_vary_s0 = []
    for s0 in sigma_0_values:
        s = sigma_Barten1999(s0, C_ab, 2.1)
        sigma_vary_s0.append(s)
    save_csv(os.path.join(output_dir, 'barten_sigma_vary_s0.csv'), sigma_vary_s0)


def generate_max_angular_size_tests(output_dir):
    """Generate test data for maximum_angular_size_Barten1999."""
    print("Generating maximum angular size tests...")

    frequencies = np.array([0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0])

    # Default X_0=60, X_max=12, N_max=15
    max_size_default = []
    for u in frequencies:
        X = maximum_angular_size_Barten1999(u, 60, 12, 15)
        max_size_default.append(X)
    save_csv(os.path.join(output_dir, 'barten_max_angular_size_default.csv'), max_size_default)

    # Different X_0 values at u=4
    X_0_values = np.array([10, 20, 30, 60, 90, 120])
    save_csv(os.path.join(output_dir, 'barten_X_0_values.csv'), X_0_values)

    max_size_vary_X0 = []
    for X_0 in X_0_values:
        X = maximum_angular_size_Barten1999(4, X_0, 12, 15)
        max_size_vary_X0.append(X)
    save_csv(os.path.join(output_dir, 'barten_max_angular_size_vary_X0.csv'), max_size_vary_X0)


def generate_csf_tests(output_dir):
    """Generate test data for contrast_sensitivity_function_Barten1999."""
    print("Generating CSF tests...")

    frequencies = np.array([0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0])
    save_csv(os.path.join(output_dir, 'barten_csf_frequencies.csv'), frequencies)

    # CSF with all defaults
    csf_default = []
    for u in frequencies:
        S = contrast_sensitivity_function_Barten1999(u)
        csf_default.append(S)
    save_csv(os.path.join(output_dir, 'barten_csf_default.csv'), csf_default)

    # CSF with different retinal illuminance values
    E_values = np.array([10, 50, 100, 500, 1000])
    save_csv(os.path.join(output_dir, 'barten_E_values.csv'), E_values)

    csf_vary_E = []
    for E in E_values:
        S = contrast_sensitivity_function_Barten1999(4, E=E)  # u=4
        csf_vary_E.append(S)
    save_csv(os.path.join(output_dir, 'barten_csf_vary_E.csv'), csf_vary_E)

    # CSF with different sigma values
    sigma_values = np.array([0.005, 0.008, 0.01, 0.012, 0.015])
    save_csv(os.path.join(output_dir, 'barten_csf_sigma_values.csv'), sigma_values)

    csf_vary_sigma = []
    for sigma in sigma_values:
        S = contrast_sensitivity_function_Barten1999(4, sigma=sigma)  # u=4
        csf_vary_sigma.append(S)
    save_csv(os.path.join(output_dir, 'barten_csf_vary_sigma.csv'), csf_vary_sigma)

    # Full frequency sweep at different luminance levels
    # Using proper workflow: L -> d -> sigma, E -> CSF
    luminances_for_csf = np.array([1.0, 10.0, 100.0])
    save_csv(os.path.join(output_dir, 'barten_csf_luminances.csv'), luminances_for_csf)

    for idx, L in enumerate(luminances_for_csf):
        d = pupil_diameter_Barten1999(L, 60, 60)
        sigma = sigma_Barten1999(0.5/60, 0.08/60, d)
        E = retinal_illuminance_Barten1999(L, d, True)

        csf_sweep = []
        for u in frequencies:
            S = contrast_sensitivity_function_Barten1999(u, sigma=sigma, E=E)
            csf_sweep.append(S)
        save_csv(os.path.join(output_dir, f'barten_csf_L{int(L)}.csv'), csf_sweep)


def main():
    if len(sys.argv) < 2:
        output_dir = os.path.join(os.path.dirname(__file__), '..', '..',
                                   'tests', 'reference_values')
    else:
        output_dir = sys.argv[1]

    os.makedirs(output_dir, exist_ok=True)

    print(f"Generating Barten 1999 CSF test data to: {output_dir}")
    print(f"Using colour-science version: {colour.__version__}")

    generate_pupil_diameter_tests(output_dir)
    generate_retinal_illuminance_tests(output_dir)
    generate_optical_mtf_tests(output_dir)
    generate_sigma_tests(output_dir)
    generate_max_angular_size_tests(output_dir)
    generate_csf_tests(output_dir)

    print("\nAll Barten 1999 CSF test data generated successfully!")


if __name__ == '__main__':
    main()
