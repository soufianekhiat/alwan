/* Debug trace for CIECAM02 Color 0 intermediate values */
#include <stdio.h>
#include <math.h>
#include "../src/alwan/alwan.h"
#include "../src/alwan/alwan_internal.h"
#include "../src/alwan/core/alwan_cam_core.h"

int main(void) {
    /* Color 0 = D65 white */
    alwan_f64 viewing_params[] = {
#include "reference_values/cam_viewing_conditions.csv"
    };
    alwan_xyz XYZ_w = {viewing_params[0], viewing_params[1], viewing_params[2]};
    alwan_f64 L_A = viewing_params[3];
    alwan_f64 Y_b = viewing_params[4];
    alwan_xyz XYZ = XYZ_w;

    printf("XYZ_w: [%.20e, %.20e, %.20e]\n", XYZ_w.x, XYZ_w.y, XYZ_w.z);

    /* Step 1: CAT02 cone responses */
    alwan_vec3 xyz_v = {{XYZ.x, XYZ.y, XYZ.z}};
    alwan_vec3 rgb_cat = alwan_mat3_mulv_v(CAM_M_CAT02, xyz_v);
    printf("RGB_cat: [%.20e, %.20e, %.20e]\n", rgb_cat.v[0], rgb_cat.v[1], rgb_cat.v[2]);

    /* Step 2: White point cone responses */
    alwan_vec3 white_v = {{XYZ_w.x, XYZ_w.y, XYZ_w.z}};
    alwan_vec3 rgb_cat_w = alwan_mat3_mulv_v(CAM_M_CAT02, white_v);

    /* Step 3: Degree of adaptation */
    alwan_f64 F = 1.0;
    alwan_f64 D = F * (1.0 - 1.0/3.6 * exp((-L_A - 42.0) / 92.0));
    printf("D: %.20e\n", D);

    /* Step 4: Chromatic adaptation */
    alwan_f64 D_R = D * (XYZ_w.y / rgb_cat_w.v[0]) + 1.0 - D;
    alwan_f64 D_G = D * (XYZ_w.y / rgb_cat_w.v[1]) + 1.0 - D;
    alwan_f64 D_B = D * (XYZ_w.y / rgb_cat_w.v[2]) + 1.0 - D;
    printf("D_RGB: [%.20e, %.20e, %.20e]\n", D_R, D_G, D_B);

    alwan_vec3 rgb_c = {{rgb_cat.v[0] * D_R, rgb_cat.v[1] * D_G, rgb_cat.v[2] * D_B}};
    printf("RGB_c: [%.20e, %.20e, %.20e]\n", rgb_c.v[0], rgb_c.v[1], rgb_c.v[2]);

    /* Step 5: HPE conversion using precomputed matrix */
    alwan_vec3 hpe = alwan_mat3_mulv_v(CAM_M_HPE_CAT02_INV, rgb_c);
    printf("HPE: [%.20e, %.20e, %.20e]\n", hpe.v[0], hpe.v[1], hpe.v[2]);

    /* Also try 2-step HPE conversion */
    alwan_vec3 xyz_c = alwan_mat3_mulv_v(CAM_M_CAT02_INV, rgb_c);
    alwan_vec3 hpe2 = alwan_mat3_mulv_v(CAM_M_HPE, xyz_c);
    printf("HPE (2-step): [%.20e, %.20e, %.20e]\n", hpe2.v[0], hpe2.v[1], hpe2.v[2]);
    printf("HPE diff: [%.20e, %.20e, %.20e]\n", hpe.v[0]-hpe2.v[0], hpe.v[1]-hpe2.v[1], hpe.v[2]-hpe2.v[2]);

    /* Step 6: FL */
    alwan_f64 n = Y_b / XYZ_w.y;
    (void)n;  /* Used in full CIECAM02 for N_bb/N_cb, not needed in this trace */
    alwan_f64 k = 1.0 / (5.0 * L_A + 1.0);
    alwan_f64 k4 = k*k*k*k;
    alwan_f64 FL = 0.2 * k4 * (5.0 * L_A) + 0.1 * (1.0 - k4) * (1.0 - k4) * pow(5.0 * L_A, 1.0/3.0);
    printf("FL: %.20e\n", FL);

    /* Step 7: Post-adaptation nonlinear compression */
    alwan_f64 scaled_R = FL * hpe.v[0] / 100.0;
    alwan_f64 scaled_G = FL * hpe.v[1] / 100.0;
    alwan_f64 scaled_B = FL * hpe.v[2] / 100.0;
    printf("scaled: [%.20e, %.20e, %.20e]\n", scaled_R, scaled_G, scaled_B);

    alwan_f64 pow_R = pow(fabs(scaled_R), 0.42);
    alwan_f64 pow_G = pow(fabs(scaled_G), 0.42);
    alwan_f64 pow_B = pow(fabs(scaled_B), 0.42);
    printf("pow: [%.20e, %.20e, %.20e]\n", pow_R, pow_G, pow_B);

    alwan_f64 R_a = 400.0 * pow_R / (27.13 + pow_R) + 0.1;
    alwan_f64 G_a = 400.0 * pow_G / (27.13 + pow_G) + 0.1;
    alwan_f64 B_a = 400.0 * pow_B / (27.13 + pow_B) + 0.1;
    printf("RGB_a: [%.20e, %.20e, %.20e]\n", R_a, G_a, B_a);

    /* Step 8: Opponent signals */
    alwan_f64 a = R_a - 12.0 * G_a / 11.0 + B_a / 11.0;
    alwan_f64 b = (R_a + G_a - 2.0 * B_a) / 9.0;
    printf("a: %.20e\n", a);
    printf("b: %.20e\n", b);

    /* Step 9: Hue angle */
    alwan_f64 h_rad = atan2(b, a);
    alwan_f64 h = h_rad * 180.0 / 3.14159265358979323846;
    if (h < 0) h += 360.0;
    printf("h_rad: %.20e\n", h_rad);
    printf("h: %.20e\n", h);

    return 0;
}
