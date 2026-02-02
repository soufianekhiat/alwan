"""
Debug script to trace through ACES 2.0 TonescaleCompress20 step by step.
Compare values with our C implementation.
"""

import numpy as np
import PyOpenColorIO as ocio

# Constants from OCIO ACES2 implementation
L_A = 100.0  # Adapting luminance (cd/m^2)
Y_b = 20.0   # Background luminance as percentage
REF_LUMINANCE = 100.0
J_SCALE = 100.0
CAM_NL_OFFSET = 27.13
SURROUND_C = 0.69  # Average surround

# AP1 primaries
AP1_RED_X, AP1_RED_Y = 0.713, 0.293
AP1_GREEN_X, AP1_GREEN_Y = 0.165, 0.830
AP1_BLUE_X, AP1_BLUE_Y = 0.128, 0.044
AP1_WHITE_X, AP1_WHITE_Y = 0.32168, 0.33767

def post_adaptation_compression_fwd(v):
    """Forward cone response compression."""
    if abs(v) < 1e-10:
        return 0.0
    sign = np.sign(v)
    F_L_Y = np.power(abs(v), 0.42)
    Ra = F_L_Y / (CAM_NL_OFFSET + F_L_Y)
    return sign * Ra

def post_adaptation_compression_inv(Ra):
    """Inverse cone response compression."""
    if abs(Ra) < 1e-10:
        return 0.0
    sign = np.sign(Ra)
    Ra_lim = min(abs(Ra), 0.99)
    F_L_Y = CAM_NL_OFFSET * Ra_lim / (1.0 - Ra_lim)
    Rc = np.power(F_L_Y, 1.0 / 0.42)
    return sign * Rc

def compute_F_L():
    """Compute luminance adaptation factor F_L."""
    k = 1.0 / (5.0 * L_A + 1.0)
    k4 = k ** 4
    F_L = 0.2 * k4 * (5.0 * L_A) + 0.1 * (1.0 - k4) ** 2 * np.power(5.0 * L_A, 1.0/3.0)
    return F_L

def compute_model_gamma():
    """Compute model gamma cz."""
    cz = SURROUND_C * (1.48 + np.sqrt(Y_b / REF_LUMINANCE))
    return cz

def J_to_Y(J, A_w_J, F_L_n, inv_cz):
    """Convert J (lightness) to Y (luminance) for tonescale."""
    if J <= 0:
        return 0.0
    # A = (J / J_scale)^(1/cz)
    A = np.power(J / J_SCALE, inv_cz)
    # Ra = A_w_J * A
    Ra = A_w_J * A
    # Y = inverse_compression(Ra) / F_L_n
    Y = post_adaptation_compression_inv(Ra) / F_L_n
    return Y

def Y_to_J(Y, A_w_J, F_L_n, cz):
    """Convert Y (luminance) to J (lightness) for tonescale."""
    if Y <= 0:
        return 0.0
    # Ra = compression(Y * F_L_n)
    Ra = post_adaptation_compression_fwd(Y * F_L_n)
    # J = J_scale * (Ra / A_w_J)^cz
    J = J_SCALE * np.power(Ra / A_w_J, cz)
    return J

def init_tonescale_params(peak_luminance):
    """Initialize tonescale parameters from peak luminance."""
    n_r = 100.0
    g = 1.15
    c = 0.18
    c_d = 10.013
    w_g = 0.14
    t_1 = 0.04
    r_hit_min = 128.0
    r_hit_max = 896.0

    r_hit = r_hit_min + (r_hit_max - r_hit_min) * (np.log(peak_luminance / n_r) / np.log(10000.0 / 100.0))

    m_0 = peak_luminance / n_r
    m_1 = 0.5 * (m_0 + np.sqrt(m_0 * (m_0 + 4.0 * t_1)))

    u = np.power((r_hit / m_1) / ((r_hit / m_1) + 1.0), g)
    m = m_1 / u

    w_i = np.log(peak_luminance / 100.0) / np.log(2.0)
    c_t = c_d / n_r * (1.0 + w_i * w_g)

    g_ip = 0.5 * (c_t + np.sqrt(c_t * (c_t + 4.0 * t_1)))
    g_ipp2 = -(m_1 * np.power(g_ip / m, 1.0 / g)) / (np.power(g_ip / m, 1.0 / g) - 1.0)

    w_2 = c / g_ipp2
    s_2 = w_2 * m_1 * n_r  # Fixed: multiply by reference_luminance
    u_2 = np.power((r_hit / m_1) / ((r_hit / m_1) + w_2), g)
    m_2 = m_1 / u_2

    return {'n_r': n_r, 'g': g, 't_1': t_1, 's_2': s_2, 'm_2': m_2}

def tonescale_fwd(Y_in, ts):
    """Apply tonescale compression."""
    x = max(0.0, Y_in)
    f = ts['m_2'] * np.power(x / (x + ts['s_2']), ts['g'])
    h = (f * f / (f + ts['t_1'])) if f > 0 else 0.0
    return h * ts['n_r']

def main():
    print("=" * 60)
    print("ACES 2.0 TonescaleCompress20 Debug Values")
    print("=" * 60)

    # Compute JMh parameters
    F_L = compute_F_L()
    F_L_n = F_L / REF_LUMINANCE
    cz = compute_model_gamma()
    inv_cz = 1.0 / cz
    A_w_J = post_adaptation_compression_fwd(F_L)

    print(f"\nJMh Parameters:")
    print(f"  F_L = {F_L:.10f}")
    print(f"  F_L_n = {F_L_n:.10f}")
    print(f"  cz (model gamma) = {cz:.10f}")
    print(f"  inv_cz = {inv_cz:.10f}")
    print(f"  A_w_J = {A_w_J:.10f}")

    # Test peak luminance
    PEAK = 1000.0
    ts = init_tonescale_params(PEAK)

    print(f"\nTonescale Parameters at {PEAK} nits:")
    print(f"  n_r = {ts['n_r']}")
    print(f"  g = {ts['g']}")
    print(f"  t_1 = {ts['t_1']}")
    print(f"  s_2 = {ts['s_2']:.10f}")
    print(f"  m_2 = {ts['m_2']:.10f}")

    # Test J values
    test_J_values = [0.0, 10.0, 25.0, 45.0, 75.0, 100.0, 150.0]

    print(f"\nJ to Y to tonescale to J conversion:")
    print("-" * 60)
    for J in test_J_values:
        Y_in = J_to_Y(J, A_w_J, F_L_n, inv_cz)
        Y_out = tonescale_fwd(Y_in, ts)
        J_out = Y_to_J(Y_out, A_w_J, F_L_n, cz)
        print(f"  J={J:6.1f} -> Y_in={Y_in:12.6f} -> Y_out={Y_out:12.6f} -> J_out={J_out:12.6f}")

    # Compare with OCIO
    print("\n" + "=" * 60)
    print("OCIO Reference Values")
    print("=" * 60)

    # Create OCIO processor
    config = ocio.Config.CreateRaw()
    group = ocio.GroupTransform()

    ap1_params = [AP1_RED_X, AP1_RED_Y, AP1_GREEN_X, AP1_GREEN_Y,
                  AP1_BLUE_X, AP1_BLUE_Y, AP1_WHITE_X, AP1_WHITE_Y]

    # RGB to JMh
    rgb_to_jmh = ocio.FixedFunctionTransform(
        style=ocio.FIXED_FUNCTION_ACES_RGB_TO_JMH_20,
        params=ap1_params
    )
    group.appendTransform(rgb_to_jmh)

    # Tonescale compress
    tonescale = ocio.FixedFunctionTransform(
        style=ocio.FIXED_FUNCTION_ACES_TONESCALE_COMPRESS_20,
        params=[PEAK]
    )
    group.appendTransform(tonescale)

    # JMh to RGB
    jmh_to_rgb = ocio.FixedFunctionTransform(
        style=ocio.FIXED_FUNCTION_ACES_RGB_TO_JMH_20,
        params=ap1_params,
        direction=ocio.TRANSFORM_DIR_INVERSE
    )
    group.appendTransform(jmh_to_rgb)

    processor = config.getProcessor(group)
    cpu = processor.getDefaultCPUProcessor()

    # Also test just RGB_to_JMh to see J values
    jmh_processor = config.getProcessor(rgb_to_jmh)
    jmh_cpu = jmh_processor.getDefaultCPUProcessor()

    # Test cases
    test_rgb = [
        [0.18, 0.18, 0.18],   # 18% gray
        [0.5, 0.5, 0.5],      # mid gray
        [1.0, 1.0, 1.0],      # white
        [1.0, 0.0, 0.0],      # red
        [0.8, 0.4, 0.2],      # brown
    ]

    print(f"\nOCIO RGB -> JMh:")
    print("-" * 60)
    for rgb in test_rgb:
        rgb_arr = np.array(rgb, dtype=np.float32).copy()
        jmh_cpu.applyRGB(rgb_arr)
        print(f"  RGB=({rgb[0]:.2f}, {rgb[1]:.2f}, {rgb[2]:.2f}) -> JMh=({rgb_arr[0]:.4f}, {rgb_arr[1]:.4f}, {rgb_arr[2]:.4f})")

    print(f"\nOCIO Full pipeline RGB -> tonescale -> RGB:")
    print("-" * 60)
    for rgb in test_rgb:
        rgb_in = np.array(rgb, dtype=np.float32).copy()
        rgb_out = rgb_in.copy()
        cpu.applyRGB(rgb_out)
        print(f"  RGB in =({rgb[0]:.4f}, {rgb[1]:.4f}, {rgb[2]:.4f})")
        print(f"  RGB out=({rgb_out[0]:.4f}, {rgb_out[1]:.4f}, {rgb_out[2]:.4f})")
        print()

if __name__ == "__main__":
    main()
