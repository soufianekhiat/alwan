"""
Common utilities for Alwan data generation.
No hardcoded values - all data comes from colour-science.
"""

import numpy as np
import os


def format_scalar(value):
    """Format a scalar value with high precision in scientific notation."""
    return f"{value:.20e}"


def ensure_dir(filepath):
    """Ensure the directory for a file exists."""
    os.makedirs(os.path.dirname(filepath), exist_ok=True)


def save_matrix(matrix, filepath, description=""):
    """
    Save a matrix to CSV file.
    Matrix is flattened to a single row.
    Trailing comma added for C array inclusion via #include.
    """
    ensure_dir(filepath)
    with open(filepath, 'w', newline='') as f:
        flat_matrix = matrix.flatten()
        formatted_values = [format_scalar(v) for v in flat_matrix]
        # Add trailing comma for C array inclusion
        f.write(','.join(formatted_values) + ',\n')

    shape_str = f"{matrix.shape[0]}x{matrix.shape[1]}" if len(matrix.shape) == 2 else str(len(matrix))
    print(f"  {filepath} ({shape_str}, {len(flat_matrix)} values){' - ' + description if description else ''}")


def save_vector(vector, filepath, description=""):
    """Save a 1D array/vector to CSV file with trailing comma for C inclusion."""
    ensure_dir(filepath)
    with open(filepath, 'w', newline='') as f:
        formatted_values = [format_scalar(v) for v in vector]
        # Add trailing comma for C array inclusion
        f.write(','.join(formatted_values) + ',\n')

    print(f"  {filepath} ({len(vector)} values){' - ' + description if description else ''}")


def save_test_data(test_data, filepath, description=""):
    """
    Save test data to CSV file.
    test_data should be a flat list of values.
    Trailing comma added for C array inclusion via #include.
    """
    ensure_dir(filepath)
    with open(filepath, 'w', newline='') as f:
        formatted_values = [format_scalar(v) for v in test_data]
        # Add trailing comma for C array inclusion
        f.write(','.join(formatted_values) + ',\n')

    print(f"  {filepath} ({len(test_data)} values){' - ' + description if description else ''}")
