"""
Diagnostic script to identify colour-science import hang.
"""
import sys
import time

print("Step 1: Testing basic imports...")
import numpy as np
print("  - numpy: OK")

print("\nStep 2: Testing colour-science import (this may hang)...")
print("  If this hangs, the issue is with colour-science initialization")
sys.stdout.flush()

start = time.time()
import colour
elapsed = time.time() - start
print(f"  - colour-science: OK ({elapsed:.2f}s)")
print(f"  - version: {colour.__version__}")

print("\nStep 3: Testing colour-science submodules...")
from colour.appearance import llab
print("  - colour.appearance.llab: OK")

print("\nAll imports successful!")
