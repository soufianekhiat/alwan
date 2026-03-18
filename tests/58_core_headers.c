/*
 * Header-only core module tests
 * Verifies _v() value-returning variants match pointer-based originals.
 */

#include "test_common.h"
#include "core/alwan_math_core.h"
#include "core/alwan_oklab_core.h"
#include "core/alwan_jzazbz_core.h"
#include "core/alwan_colorspace_core.h"
#include "core/alwan_ipt_core.h"
#include "core/alwan_convenience_core.h"
#include "core/alwan_din99_core.h"
#include "core/alwan_hunter_lab_core.h"
#include "core/alwan_prolab_core.h"
#include "core/alwan_extended_core.h"
#include "core/alwan_ictcp_core.h"
#include "core/alwan_osa_ucs_core.h"
#include "core/alwan_rlab_core.h"
#include "core/alwan_atd95_core.h"
#include "core/alwan_llab_core.h"
#include "core/alwan_vision_core.h"
#include "core/alwan_color_correction_core.h"
#include "core/alwan_cat_core.h"
#include "core/alwan_quality_core.h"
#include "core/alwan_rayleigh_core.h"
#include "core/alwan_gamut_core.h"
#include "core/alwan_view_core.h"

/* ----------------------------------------------------------------
 * Math core tests
 * ---------------------------------------------------------------- */

static int test_mat3_identity(void) {
    TEST_START("mat3 identity");
    alwan_mat3x3 id = alwan_mat3_identity_f64_v();
    TEST_ASSERT_NEAR(id.m[0], ALWAN_ONE, ALWAN_EPSILON, "m[0]");
    TEST_ASSERT_NEAR(id.m[4], ALWAN_ONE, ALWAN_EPSILON, "m[4]");
    TEST_ASSERT_NEAR(id.m[8], ALWAN_ONE, ALWAN_EPSILON, "m[8]");
    TEST_ASSERT_NEAR(id.m[1], ALWAN_ZERO, ALWAN_EPSILON, "m[1]");
    TEST_ASSERT_NEAR(id.m[3], ALWAN_ZERO, ALWAN_EPSILON, "m[3]");
    TEST_PASS("mat3_identity");
}

static int test_mat3_det(void) {
    TEST_START("mat3 determinant");
    alwan_mat3x3 id = alwan_mat3_identity_f64_v();
    alwan_f64 det = alwan_mat3_det_f64_v(id);
    TEST_ASSERT_NEAR(det, ALWAN_ONE, ALWAN_EPSILON, "det(I)=1");
    TEST_PASS("mat3_det");
}

static int test_mat3_mulv(void) {
    TEST_START("mat3 * vec3");
    alwan_mat3x3 id = alwan_mat3_identity_f64_v();
    alwan_vec3 v;
    v.v[0] = ALWAN_LITERAL(1.0);
    v.v[1] = ALWAN_LITERAL(2.0);
    v.v[2] = ALWAN_LITERAL(3.0);
    alwan_vec3 r = alwan_mat3_mulv_f64_v(id, v);
    TEST_ASSERT_NEAR(r.v[0], v.v[0], ALWAN_EPSILON, "I*v[0]");
    TEST_ASSERT_NEAR(r.v[1], v.v[1], ALWAN_EPSILON, "I*v[1]");
    TEST_ASSERT_NEAR(r.v[2], v.v[2], ALWAN_EPSILON, "I*v[2]");
    TEST_PASS("mat3_mulv");
}

static int test_mat3_mul(void) {
    TEST_START("mat3 * mat3");
    alwan_mat3x3 id = alwan_mat3_identity_f64_v();
    alwan_mat3x3 r = alwan_mat3_mul_f64_v(id, id);
    TEST_ASSERT_NEAR(r.m[0], ALWAN_ONE, ALWAN_EPSILON, "I*I[0]");
    TEST_ASSERT_NEAR(r.m[4], ALWAN_ONE, ALWAN_EPSILON, "I*I[4]");
    TEST_ASSERT_NEAR(r.m[8], ALWAN_ONE, ALWAN_EPSILON, "I*I[8]");
    TEST_ASSERT_NEAR(r.m[1], ALWAN_ZERO, ALWAN_EPSILON, "I*I[1]");
    TEST_PASS("mat3_mul");
}

/* ----------------------------------------------------------------
 * Oklab core: compare _v() against pointer-based
 * ---------------------------------------------------------------- */

static int test_oklab_v_roundtrip(void) {
    TEST_START("oklab _v round-trip");

    alwan_xyz white;
    white.x = ALWAN_D65_X;
    white.y = ALWAN_D65_Y;
    white.z = ALWAN_D65_Z;

    /* _v variant */
    alwan_oklab ok_v = alwan_xyz_to_oklab_f64_v(white);
    alwan_xyz back_v = alwan_oklab_to_xyz_f64_v(ok_v);

    /* pointer variant */
    alwan_oklab ok_p;
    alwan_xyz_to_oklab(&ok_p, &white);
    alwan_xyz back_p;
    alwan_oklab_to_xyz(&back_p, &ok_p);

    /* Compare _v vs pointer results */
    TEST_ASSERT_NEAR(ok_v.L, ok_p.L, ALWAN_TEST_TOLERANCE, "oklab L");
    TEST_ASSERT_NEAR(ok_v.a, ok_p.a, ALWAN_TEST_TOLERANCE, "oklab a");
    TEST_ASSERT_NEAR(ok_v.b, ok_p.b, ALWAN_TEST_TOLERANCE, "oklab b");

    TEST_ASSERT_NEAR(back_v.x, back_p.x, ALWAN_TEST_TOLERANCE, "xyz round-trip x");
    TEST_ASSERT_NEAR(back_v.y, back_p.y, ALWAN_TEST_TOLERANCE, "xyz round-trip y");
    TEST_ASSERT_NEAR(back_v.z, back_p.z, ALWAN_TEST_TOLERANCE, "xyz round-trip z");

    TEST_PASS("oklab_v_roundtrip");
}

static int test_oklch_v_roundtrip(void) {
    TEST_START("oklch _v round-trip");

    alwan_xyz color;
    color.x = ALWAN_LITERAL(50.0);
    color.y = ALWAN_LITERAL(40.0);
    color.z = ALWAN_LITERAL(30.0);

    alwan_oklab ok_v = alwan_xyz_to_oklab_f64_v(color);
    alwan_oklch lch_v = alwan_oklab_to_oklch_f64_v(ok_v);
    alwan_oklab back_v = alwan_oklch_to_oklab_f64_v(lch_v);

    /* pointer variant */
    alwan_oklab ok_p;
    alwan_xyz_to_oklab(&ok_p, &color);
    alwan_oklch lch_p;
    alwan_oklab_to_oklch(&lch_p, &ok_p);
    alwan_oklab back_p;
    alwan_oklch_to_oklab(&back_p, &lch_p);

    TEST_ASSERT_NEAR(lch_v.L, lch_p.L, ALWAN_TEST_TOLERANCE, "oklch L");
    TEST_ASSERT_NEAR(lch_v.C, lch_p.C, ALWAN_TEST_TOLERANCE, "oklch C");
    TEST_ASSERT_NEAR(lch_v.h, lch_p.h, ALWAN_TEST_TOLERANCE, "oklch h");

    TEST_ASSERT_NEAR(back_v.L, back_p.L, ALWAN_TEST_TOLERANCE, "oklch->oklab L");
    TEST_ASSERT_NEAR(back_v.a, back_p.a, ALWAN_TEST_TOLERANCE, "oklch->oklab a");
    TEST_ASSERT_NEAR(back_v.b, back_p.b, ALWAN_TEST_TOLERANCE, "oklch->oklab b");

    TEST_PASS("oklch_v_roundtrip");
}

/* ----------------------------------------------------------------
 * Jzazbz core: compare _v() against pointer-based
 * ---------------------------------------------------------------- */

static int test_jzazbz_v_roundtrip(void) {
    TEST_START("jzazbz _v round-trip");

    alwan_xyz white;
    white.x = ALWAN_D65_X;
    white.y = ALWAN_D65_Y;
    white.z = ALWAN_D65_Z;

    /* _v variant */
    alwan_jzazbz jz_v = alwan_xyz_to_jzazbz_f64_v(white);
    alwan_xyz back_v = alwan_jzazbz_to_xyz_f64_v(jz_v);

    /* pointer variant */
    alwan_jzazbz jz_p;
    alwan_xyz_to_jzazbz(&jz_p, &white);
    alwan_xyz back_p;
    alwan_jzazbz_to_xyz(&back_p, &jz_p);

    TEST_ASSERT_NEAR(jz_v.Jz, jz_p.Jz, ALWAN_TEST_TOLERANCE, "Jz");
    TEST_ASSERT_NEAR(jz_v.az, jz_p.az, ALWAN_TEST_TOLERANCE, "az");
    TEST_ASSERT_NEAR(jz_v.bz, jz_p.bz, ALWAN_TEST_TOLERANCE, "bz");

    TEST_ASSERT_NEAR(back_v.x, back_p.x, ALWAN_TEST_TOLERANCE, "jz xyz x");
    TEST_ASSERT_NEAR(back_v.y, back_p.y, ALWAN_TEST_TOLERANCE, "jz xyz y");
    TEST_ASSERT_NEAR(back_v.z, back_p.z, ALWAN_TEST_TOLERANCE, "jz xyz z");

    TEST_PASS("jzazbz_v_roundtrip");
}

static int test_jzczhz_v_roundtrip(void) {
    TEST_START("jzczhz _v round-trip");

    alwan_xyz color;
    color.x = ALWAN_LITERAL(50.0);
    color.y = ALWAN_LITERAL(40.0);
    color.z = ALWAN_LITERAL(30.0);

    alwan_jzazbz jz_v = alwan_xyz_to_jzazbz_f64_v(color);
    alwan_jzczhz jzczhz_v = alwan_jzazbz_to_jzczhz_f64_v(jz_v);
    alwan_jzazbz back_v = alwan_jzczhz_to_jzazbz_f64_v(jzczhz_v);

    /* pointer variant */
    alwan_jzazbz jz_p;
    alwan_xyz_to_jzazbz(&jz_p, &color);
    alwan_jzczhz jzczhz_p;
    alwan_jzazbz_to_jzczhz(&jzczhz_p, &jz_p);
    alwan_jzazbz back_p;
    alwan_jzczhz_to_jzazbz(&back_p, &jzczhz_p);

    TEST_ASSERT_NEAR(jzczhz_v.Jz, jzczhz_p.Jz, ALWAN_TEST_TOLERANCE, "JzCzhz Jz");
    TEST_ASSERT_NEAR(jzczhz_v.Cz, jzczhz_p.Cz, ALWAN_TEST_TOLERANCE, "JzCzhz Cz");
    TEST_ASSERT_NEAR(jzczhz_v.hz, jzczhz_p.hz, ALWAN_TEST_TOLERANCE, "JzCzhz hz");

    TEST_ASSERT_NEAR(back_v.Jz, back_p.Jz, ALWAN_TEST_TOLERANCE, "jzczhz->jz Jz");
    TEST_ASSERT_NEAR(back_v.az, back_p.az, ALWAN_TEST_TOLERANCE, "jzczhz->jz az");
    TEST_ASSERT_NEAR(back_v.bz, back_p.bz, ALWAN_TEST_TOLERANCE, "jzczhz->jz bz");

    TEST_PASS("jzczhz_v_roundtrip");
}

/* ----------------------------------------------------------------
 * Colorspace core: compare _v() against pointer-based
 * ---------------------------------------------------------------- */

static int test_xyy_v_roundtrip(void) {
    TEST_START("xyY _v round-trip");

    alwan_xyz white;
    white.x = ALWAN_D65_X;
    white.y = ALWAN_D65_Y;
    white.z = ALWAN_D65_Z;

    alwan_xyy xyy_v = alwan_xyz_to_xyy_f64_v(white);
    alwan_xyz back_v = alwan_xyy_to_xyz_f64_v(xyy_v);

    alwan_xyy xyy_p;
    alwan_xyz_to_xyy(&xyy_p, &white);
    alwan_xyz back_p;
    alwan_xyy_to_xyz(&back_p, &xyy_p);

    TEST_ASSERT_NEAR(xyy_v.x, xyy_p.x, ALWAN_TEST_TOLERANCE, "xyy x");
    TEST_ASSERT_NEAR(xyy_v.y, xyy_p.y, ALWAN_TEST_TOLERANCE, "xyy y");
    TEST_ASSERT_NEAR(xyy_v.Y, xyy_p.Y, ALWAN_TEST_TOLERANCE, "xyy Y");

    TEST_ASSERT_NEAR(back_v.x, back_p.x, ALWAN_TEST_TOLERANCE, "xyy->xyz x");
    TEST_ASSERT_NEAR(back_v.y, back_p.y, ALWAN_TEST_TOLERANCE, "xyy->xyz y");
    TEST_ASSERT_NEAR(back_v.z, back_p.z, ALWAN_TEST_TOLERANCE, "xyy->xyz z");

    TEST_PASS("xyy_v_roundtrip");
}

static int test_lab_v_roundtrip(void) {
    TEST_START("Lab _v round-trip");

    alwan_xyz color;
    color.x = ALWAN_LITERAL(50.0);
    color.y = ALWAN_LITERAL(40.0);
    color.z = ALWAN_LITERAL(30.0);

    alwan_xyz wp;
    wp.x = ALWAN_D65_X;
    wp.y = ALWAN_D65_Y;
    wp.z = ALWAN_D65_Z;

    alwan_lab lab_v = alwan_xyz_to_lab_f64_v(color, wp);
    alwan_xyz back_v = alwan_lab_to_xyz_f64_v(lab_v, wp);

    alwan_lab lab_p;
    alwan_xyz_to_lab(&lab_p, &color, &wp);
    alwan_xyz back_p;
    alwan_lab_to_xyz(&back_p, &lab_p, &wp);

    TEST_ASSERT_NEAR(lab_v.L, lab_p.L, ALWAN_TEST_TOLERANCE, "lab L");
    TEST_ASSERT_NEAR(lab_v.a, lab_p.a, ALWAN_TEST_TOLERANCE, "lab a");
    TEST_ASSERT_NEAR(lab_v.b, lab_p.b, ALWAN_TEST_TOLERANCE, "lab b");

    TEST_ASSERT_NEAR(back_v.x, back_p.x, ALWAN_TEST_TOLERANCE, "lab->xyz x");
    TEST_ASSERT_NEAR(back_v.y, back_p.y, ALWAN_TEST_TOLERANCE, "lab->xyz y");
    TEST_ASSERT_NEAR(back_v.z, back_p.z, ALWAN_TEST_TOLERANCE, "lab->xyz z");

    TEST_PASS("lab_v_roundtrip");
}

static int test_lch_v_roundtrip(void) {
    TEST_START("LCh _v round-trip");

    alwan_lab lab;
    lab.L = ALWAN_LITERAL(50.0);
    lab.a = ALWAN_LITERAL(20.0);
    lab.b = ALWAN_LITERAL(-30.0);

    alwan_lch lch_v = alwan_lab_to_lch_f64_v(lab);
    alwan_lab back_v = alwan_lch_to_lab_f64_v(lch_v);

    alwan_lch lch_p;
    alwan_lab_to_lch(&lch_p, &lab);
    alwan_lab back_p;
    alwan_lch_to_lab(&back_p, &lch_p);

    TEST_ASSERT_NEAR(lch_v.L, lch_p.L, ALWAN_TEST_TOLERANCE, "lch L");
    TEST_ASSERT_NEAR(lch_v.C, lch_p.C, ALWAN_TEST_TOLERANCE, "lch C");
    TEST_ASSERT_NEAR(lch_v.h, lch_p.h, ALWAN_TEST_TOLERANCE, "lch h");

    TEST_ASSERT_NEAR(back_v.L, back_p.L, ALWAN_TEST_TOLERANCE, "lch->lab L");
    TEST_ASSERT_NEAR(back_v.a, back_p.a, ALWAN_TEST_TOLERANCE, "lch->lab a");
    TEST_ASSERT_NEAR(back_v.b, back_p.b, ALWAN_TEST_TOLERANCE, "lch->lab b");

    TEST_PASS("lch_v_roundtrip");
}

static int test_luv_v_roundtrip(void) {
    TEST_START("Luv _v round-trip");

    alwan_xyz color;
    color.x = ALWAN_LITERAL(50.0);
    color.y = ALWAN_LITERAL(40.0);
    color.z = ALWAN_LITERAL(30.0);

    alwan_xyz wp;
    wp.x = ALWAN_D65_X;
    wp.y = ALWAN_D65_Y;
    wp.z = ALWAN_D65_Z;

    alwan_luv luv_v = alwan_xyz_to_luv_f64_v(color, wp);
    alwan_xyz back_v = alwan_luv_to_xyz_f64_v(luv_v, wp);

    alwan_luv luv_p;
    alwan_xyz_to_luv(&luv_p, &color, &wp);
    alwan_xyz back_p;
    alwan_luv_to_xyz(&back_p, &luv_p, &wp);

    TEST_ASSERT_NEAR(luv_v.L, luv_p.L, ALWAN_TEST_TOLERANCE, "luv L");
    TEST_ASSERT_NEAR(luv_v.u, luv_p.u, ALWAN_TEST_TOLERANCE, "luv u");
    TEST_ASSERT_NEAR(luv_v.v, luv_p.v, ALWAN_TEST_TOLERANCE, "luv v");

    TEST_ASSERT_NEAR(back_v.x, back_p.x, ALWAN_TEST_TOLERANCE, "luv->xyz x");
    TEST_ASSERT_NEAR(back_v.y, back_p.y, ALWAN_TEST_TOLERANCE, "luv->xyz y");
    TEST_ASSERT_NEAR(back_v.z, back_p.z, ALWAN_TEST_TOLERANCE, "luv->xyz z");

    TEST_PASS("luv_v_roundtrip");
}

/* ----------------------------------------------------------------
 * IPT core: compare _v() against pointer-based
 * ---------------------------------------------------------------- */

static int test_ipt_v_roundtrip(void) {
    TEST_START("IPT _v round-trip");

    alwan_xyz white;
    white.x = ALWAN_D65_X;
    white.y = ALWAN_D65_Y;
    white.z = ALWAN_D65_Z;

    alwan_ipt ipt_v = alwan_xyz_to_ipt_f64_v(white);
    alwan_xyz back_v = alwan_ipt_to_xyz_f64_v(ipt_v);

    alwan_ipt ipt_p;
    alwan_xyz_to_ipt(&ipt_p, &white);
    alwan_xyz back_p;
    alwan_ipt_to_xyz(&back_p, &ipt_p);

    TEST_ASSERT_NEAR(ipt_v.I, ipt_p.I, ALWAN_TEST_TOLERANCE, "ipt I");
    TEST_ASSERT_NEAR(ipt_v.P, ipt_p.P, ALWAN_TEST_TOLERANCE, "ipt P");
    TEST_ASSERT_NEAR(ipt_v.T, ipt_p.T, ALWAN_TEST_TOLERANCE, "ipt T");

    TEST_ASSERT_NEAR(back_v.x, back_p.x, ALWAN_TEST_TOLERANCE, "ipt->xyz x");
    TEST_ASSERT_NEAR(back_v.y, back_p.y, ALWAN_TEST_TOLERANCE, "ipt->xyz y");
    TEST_ASSERT_NEAR(back_v.z, back_p.z, ALWAN_TEST_TOLERANCE, "ipt->xyz z");

    TEST_PASS("ipt_v_roundtrip");
}

static int test_iptch_v_roundtrip(void) {
    TEST_START("IPTch _v round-trip");

    alwan_xyz color;
    color.x = ALWAN_LITERAL(50.0);
    color.y = ALWAN_LITERAL(40.0);
    color.z = ALWAN_LITERAL(30.0);

    alwan_ipt ipt_v = alwan_xyz_to_ipt_f64_v(color);
    alwan_iptch iptch_v = alwan_ipt_to_iptch_f64_v(ipt_v);
    alwan_ipt back_v = alwan_iptch_to_ipt_f64_v(iptch_v);

    alwan_ipt ipt_p;
    alwan_xyz_to_ipt(&ipt_p, &color);
    alwan_iptch iptch_p;
    alwan_ipt_to_iptch(&iptch_p, &ipt_p);
    alwan_ipt back_p;
    alwan_iptch_to_ipt(&back_p, &iptch_p);

    TEST_ASSERT_NEAR(iptch_v.I, iptch_p.I, ALWAN_TEST_TOLERANCE, "iptch I");
    TEST_ASSERT_NEAR(iptch_v.C, iptch_p.C, ALWAN_TEST_TOLERANCE, "iptch C");
    TEST_ASSERT_NEAR(iptch_v.h, iptch_p.h, ALWAN_TEST_TOLERANCE, "iptch h");

    TEST_ASSERT_NEAR(back_v.I, back_p.I, ALWAN_TEST_TOLERANCE, "iptch->ipt I");
    TEST_ASSERT_NEAR(back_v.P, back_p.P, ALWAN_TEST_TOLERANCE, "iptch->ipt P");
    TEST_ASSERT_NEAR(back_v.T, back_p.T, ALWAN_TEST_TOLERANCE, "iptch->ipt T");

    TEST_PASS("iptch_v_roundtrip");
}

/* ----------------------------------------------------------------
 * Convenience core: compare _v() against pointer-based
 * ---------------------------------------------------------------- */

static int test_cmy_v_roundtrip(void) {
    TEST_START("CMY _v round-trip");

    alwan_rgb rgb;
    rgb.r = ALWAN_LITERAL(0.3);
    rgb.g = ALWAN_LITERAL(0.6);
    rgb.b = ALWAN_LITERAL(0.9);

    alwan_cmy cmy_v = alwan_rgb_to_cmy_f64_v(rgb);
    alwan_rgb back_v = alwan_cmy_to_rgb_f64_v(cmy_v);

    alwan_cmy cmy_p;
    alwan_rgb_to_cmy(&cmy_p, &rgb);
    alwan_rgb back_p;
    alwan_cmy_to_rgb(&back_p, &cmy_p);

    TEST_ASSERT_NEAR(cmy_v.c, cmy_p.c, ALWAN_EPSILON, "cmy c");
    TEST_ASSERT_NEAR(cmy_v.m, cmy_p.m, ALWAN_EPSILON, "cmy m");
    TEST_ASSERT_NEAR(cmy_v.y, cmy_p.y, ALWAN_EPSILON, "cmy y");

    TEST_ASSERT_NEAR(back_v.r, back_p.r, ALWAN_EPSILON, "cmy->rgb r");
    TEST_ASSERT_NEAR(back_v.g, back_p.g, ALWAN_EPSILON, "cmy->rgb g");
    TEST_ASSERT_NEAR(back_v.b, back_p.b, ALWAN_EPSILON, "cmy->rgb b");

    TEST_PASS("cmy_v_roundtrip");
}

static int test_ycocg_v_roundtrip(void) {
    TEST_START("YCoCg _v round-trip");

    alwan_rgb rgb;
    rgb.r = ALWAN_LITERAL(0.3);
    rgb.g = ALWAN_LITERAL(0.6);
    rgb.b = ALWAN_LITERAL(0.9);

    alwan_ycocg ycocg_v = alwan_rgb_to_ycocg_f64_v(rgb);
    alwan_rgb back_v = alwan_ycocg_to_rgb_f64_v(ycocg_v);

    alwan_ycocg ycocg_p;
    alwan_rgb_to_ycocg(&ycocg_p, &rgb);
    alwan_rgb back_p;
    alwan_ycocg_to_rgb(&back_p, &ycocg_p);

    TEST_ASSERT_NEAR(ycocg_v.Y, ycocg_p.Y, ALWAN_EPSILON, "ycocg Y");
    TEST_ASSERT_NEAR(ycocg_v.Co, ycocg_p.Co, ALWAN_EPSILON, "ycocg Co");
    TEST_ASSERT_NEAR(ycocg_v.Cg, ycocg_p.Cg, ALWAN_EPSILON, "ycocg Cg");

    TEST_ASSERT_NEAR(back_v.r, back_p.r, ALWAN_EPSILON, "ycocg->rgb r");
    TEST_ASSERT_NEAR(back_v.g, back_p.g, ALWAN_EPSILON, "ycocg->rgb g");
    TEST_ASSERT_NEAR(back_v.b, back_p.b, ALWAN_EPSILON, "ycocg->rgb b");

    TEST_PASS("ycocg_v_roundtrip");
}

/* ----------------------------------------------------------------
 * Convenience core: HSV, HSL, YCbCr, YcCbcCrc
 * ---------------------------------------------------------------- */

static int test_hsv_v_roundtrip(void) {
    TEST_START("HSV _v round-trip");

    alwan_rgb rgb;
    rgb.r = ALWAN_LITERAL(0.3);
    rgb.g = ALWAN_LITERAL(0.6);
    rgb.b = ALWAN_LITERAL(0.9);

    /* _v variant */
    alwan_hsv hsv_v = alwan_rgb_to_hsv_f64_v(rgb);
    alwan_rgb back_v = alwan_hsv_to_rgb_f64_v(hsv_v);

    /* pointer variant */
    alwan_hsv hsv_p;
    alwan_rgb_to_hsv(&hsv_p, &rgb);
    alwan_rgb back_p;
    alwan_hsv_to_rgb(&back_p, &hsv_p);

    TEST_ASSERT_NEAR(hsv_v.h, hsv_p.h, ALWAN_TEST_TOLERANCE, "hsv h");
    TEST_ASSERT_NEAR(hsv_v.s, hsv_p.s, ALWAN_TEST_TOLERANCE, "hsv s");
    TEST_ASSERT_NEAR(hsv_v.v, hsv_p.v, ALWAN_TEST_TOLERANCE, "hsv v");

    TEST_ASSERT_NEAR(back_v.r, back_p.r, ALWAN_TEST_TOLERANCE, "hsv->rgb r");
    TEST_ASSERT_NEAR(back_v.g, back_p.g, ALWAN_TEST_TOLERANCE, "hsv->rgb g");
    TEST_ASSERT_NEAR(back_v.b, back_p.b, ALWAN_TEST_TOLERANCE, "hsv->rgb b");

    TEST_PASS("hsv_v_roundtrip");
}

static int test_hsl_v_roundtrip(void) {
    TEST_START("HSL _v round-trip");

    alwan_rgb rgb;
    rgb.r = ALWAN_LITERAL(0.3);
    rgb.g = ALWAN_LITERAL(0.6);
    rgb.b = ALWAN_LITERAL(0.9);

    /* _v variant */
    alwan_hsl hsl_v = alwan_rgb_to_hsl_f64_v(rgb);
    alwan_rgb back_v = alwan_hsl_to_rgb_f64_v(hsl_v);

    /* pointer variant */
    alwan_hsl hsl_p;
    alwan_rgb_to_hsl(&hsl_p, &rgb);
    alwan_rgb back_p;
    alwan_hsl_to_rgb(&back_p, &hsl_p);

    TEST_ASSERT_NEAR(hsl_v.h, hsl_p.h, ALWAN_TEST_TOLERANCE, "hsl h");
    TEST_ASSERT_NEAR(hsl_v.s, hsl_p.s, ALWAN_TEST_TOLERANCE, "hsl s");
    TEST_ASSERT_NEAR(hsl_v.l, hsl_p.l, ALWAN_TEST_TOLERANCE, "hsl l");

    TEST_ASSERT_NEAR(back_v.r, back_p.r, ALWAN_TEST_TOLERANCE, "hsl->rgb r");
    TEST_ASSERT_NEAR(back_v.g, back_p.g, ALWAN_TEST_TOLERANCE, "hsl->rgb g");
    TEST_ASSERT_NEAR(back_v.b, back_p.b, ALWAN_TEST_TOLERANCE, "hsl->rgb b");

    TEST_PASS("hsl_v_roundtrip");
}

static int test_ycbcr_v_roundtrip(void) {
    TEST_START("YCbCr _v round-trip");

    alwan_rgb rgb;
    rgb.r = ALWAN_LITERAL(0.3);
    rgb.g = ALWAN_LITERAL(0.6);
    rgb.b = ALWAN_LITERAL(0.9);

    /* BT.709 coefficients */
    alwan_f64 kr = ALWAN_LITERAL(0.2126);
    alwan_f64 kb = ALWAN_LITERAL(0.0722);

    /* _v variant */
    alwan_ycbcr ycbcr_v = alwan_rgb_to_ycbcr_kr_kb_f64_v(rgb, kr, kb);
    alwan_rgb back_v = alwan_ycbcr_to_rgb_kr_kb_f64_v(ycbcr_v, kr, kb);

    /* pointer variant */
    alwan_ycbcr ycbcr_p;
    alwan_rgb_to_ycbcr(&ycbcr_p, &rgb, ALWAN_YCBCR_BT709);
    alwan_rgb back_p;
    alwan_ycbcr_to_rgb(&back_p, &ycbcr_p, ALWAN_YCBCR_BT709);

    TEST_ASSERT_NEAR(ycbcr_v.Y, ycbcr_p.Y, ALWAN_TEST_TOLERANCE, "ycbcr Y");
    TEST_ASSERT_NEAR(ycbcr_v.Cb, ycbcr_p.Cb, ALWAN_TEST_TOLERANCE, "ycbcr Cb");
    TEST_ASSERT_NEAR(ycbcr_v.Cr, ycbcr_p.Cr, ALWAN_TEST_TOLERANCE, "ycbcr Cr");

    TEST_ASSERT_NEAR(back_v.r, back_p.r, ALWAN_TEST_TOLERANCE, "ycbcr->rgb r");
    TEST_ASSERT_NEAR(back_v.g, back_p.g, ALWAN_TEST_TOLERANCE, "ycbcr->rgb g");
    TEST_ASSERT_NEAR(back_v.b, back_p.b, ALWAN_TEST_TOLERANCE, "ycbcr->rgb b");

    TEST_PASS("ycbcr_v_roundtrip");
}

static int test_yccbccrc_v_roundtrip(void) {
    TEST_START("YcCbcCrc _v round-trip");

    alwan_rgb rgb;
    rgb.r = ALWAN_LITERAL(0.3);
    rgb.g = ALWAN_LITERAL(0.6);
    rgb.b = ALWAN_LITERAL(0.9);

    /* _v variant */
    alwan_yccbccrc yccbccrc_v = alwan_rgb_to_yccbccrc_f64_v(rgb, 10);
    alwan_rgb back_v = alwan_yccbccrc_to_rgb_f64_v(yccbccrc_v, 10);

    /* pointer variant */
    alwan_yccbccrc yccbccrc_p;
    alwan_rgb_to_yccbccrc(&yccbccrc_p, &rgb, 10);
    alwan_rgb back_p;
    alwan_yccbccrc_to_rgb(&back_p, &yccbccrc_p, 10);

    TEST_ASSERT_NEAR(yccbccrc_v.Yc, yccbccrc_p.Yc, ALWAN_TEST_TOLERANCE, "yccbccrc Yc");
    TEST_ASSERT_NEAR(yccbccrc_v.Cbc, yccbccrc_p.Cbc, ALWAN_TEST_TOLERANCE, "yccbccrc Cbc");
    TEST_ASSERT_NEAR(yccbccrc_v.Crc, yccbccrc_p.Crc, ALWAN_TEST_TOLERANCE, "yccbccrc Crc");

    TEST_ASSERT_NEAR(back_v.r, back_p.r, ALWAN_TEST_TOLERANCE, "yccbccrc->rgb r");
    TEST_ASSERT_NEAR(back_v.g, back_p.g, ALWAN_TEST_TOLERANCE, "yccbccrc->rgb g");
    TEST_ASSERT_NEAR(back_v.b, back_p.b, ALWAN_TEST_TOLERANCE, "yccbccrc->rgb b");

    TEST_PASS("yccbccrc_v_roundtrip");
}

/* ----------------------------------------------------------------
 * DIN99 core: compare _v() against pointer-based
 * ---------------------------------------------------------------- */

static int test_din99_v_roundtrip(void) {
    TEST_START("DIN99 _v round-trip");

    alwan_lab lab;
    lab.L = ALWAN_LITERAL(50.0);
    lab.a = ALWAN_LITERAL(20.0);
    lab.b = ALWAN_LITERAL(-30.0);

    /* _v variant (variant 0 = DIN99 / ASTM D2244-07) */
    alwan_din99 din99_v = alwan_lab_to_din99_f64_v(lab, 0);
    alwan_lab back_v = alwan_din99_to_lab_f64_v(din99_v, 0);

    /* pointer variant */
    alwan_din99 din99_p;
    alwan_lab_to_din99(&din99_p, &lab, 0);
    alwan_lab back_p;
    alwan_din99_to_lab(&back_p, &din99_p, 0);

    TEST_ASSERT_NEAR(din99_v.L99, din99_p.L99, ALWAN_TEST_TOLERANCE, "din99 L99");
    TEST_ASSERT_NEAR(din99_v.a99, din99_p.a99, ALWAN_TEST_TOLERANCE, "din99 a99");
    TEST_ASSERT_NEAR(din99_v.b99, din99_p.b99, ALWAN_TEST_TOLERANCE, "din99 b99");

    TEST_ASSERT_NEAR(back_v.L, back_p.L, ALWAN_TEST_TOLERANCE, "din99->lab L");
    TEST_ASSERT_NEAR(back_v.a, back_p.a, ALWAN_TEST_TOLERANCE, "din99->lab a");
    TEST_ASSERT_NEAR(back_v.b, back_p.b, ALWAN_TEST_TOLERANCE, "din99->lab b");

    TEST_PASS("din99_v_roundtrip");
}

/* ----------------------------------------------------------------
 * Hunter Lab core: compare _v() against pointer-based
 * ---------------------------------------------------------------- */

static int test_hunter_lab_v_roundtrip(void) {
    TEST_START("Hunter Lab _v round-trip");

    alwan_xyz white;
    white.x = ALWAN_D65_X;
    white.y = ALWAN_D65_Y;
    white.z = ALWAN_D65_Z;

    /* _v variant */
    alwan_hunter_lab hl_v = alwan_xyz_to_hunter_lab_f64_v(white);
    alwan_xyz back_v = alwan_hunter_lab_to_xyz_f64_v(hl_v);

    /* pointer variant */
    alwan_hunter_lab hl_p;
    alwan_xyz_to_hunter_lab(&hl_p, &white);
    alwan_xyz back_p;
    alwan_hunter_lab_to_xyz(&back_p, &hl_p);

    TEST_ASSERT_NEAR(hl_v.L, hl_p.L, ALWAN_TEST_TOLERANCE, "hunter_lab L");
    TEST_ASSERT_NEAR(hl_v.a, hl_p.a, ALWAN_TEST_TOLERANCE, "hunter_lab a");
    TEST_ASSERT_NEAR(hl_v.b, hl_p.b, ALWAN_TEST_TOLERANCE, "hunter_lab b");

    TEST_ASSERT_NEAR(back_v.x, back_p.x, ALWAN_TEST_TOLERANCE, "hunter_lab->xyz x");
    TEST_ASSERT_NEAR(back_v.y, back_p.y, ALWAN_TEST_TOLERANCE, "hunter_lab->xyz y");
    TEST_ASSERT_NEAR(back_v.z, back_p.z, ALWAN_TEST_TOLERANCE, "hunter_lab->xyz z");

    TEST_PASS("hunter_lab_v_roundtrip");
}

/* ----------------------------------------------------------------
 * ProLab core: compare _v() against pointer-based
 * ---------------------------------------------------------------- */

static int test_prolab_v_roundtrip(void) {
    TEST_START("ProLab _v round-trip");

    alwan_xyz white;
    white.x = ALWAN_D65_X;
    white.y = ALWAN_D65_Y;
    white.z = ALWAN_D65_Z;

    /* _v variant */
    alwan_prolab pl_v = alwan_xyz_to_prolab_f64_v(white);
    alwan_xyz back_v = alwan_prolab_to_xyz_f64_v(pl_v);

    /* pointer variant */
    alwan_prolab pl_p;
    alwan_xyz_to_prolab(&pl_p, &white);
    alwan_xyz back_p;
    alwan_prolab_to_xyz(&back_p, &pl_p);

    TEST_ASSERT_NEAR(pl_v.L, pl_p.L, ALWAN_TEST_TOLERANCE, "prolab L");
    TEST_ASSERT_NEAR(pl_v.a, pl_p.a, ALWAN_TEST_TOLERANCE, "prolab a");
    TEST_ASSERT_NEAR(pl_v.b, pl_p.b, ALWAN_TEST_TOLERANCE, "prolab b");

    TEST_ASSERT_NEAR(back_v.x, back_p.x, ALWAN_TEST_TOLERANCE, "prolab->xyz x");
    TEST_ASSERT_NEAR(back_v.y, back_p.y, ALWAN_TEST_TOLERANCE, "prolab->xyz y");
    TEST_ASSERT_NEAR(back_v.z, back_p.z, ALWAN_TEST_TOLERANCE, "prolab->xyz z");

    TEST_PASS("prolab_v_roundtrip");
}

/* ----------------------------------------------------------------
 * Extended core: Prismatic, HCL, IHLS
 * ---------------------------------------------------------------- */

static int test_prismatic_v_roundtrip(void) {
    TEST_START("Prismatic _v round-trip");

    alwan_rgb rgb;
    rgb.r = ALWAN_LITERAL(0.3);
    rgb.g = ALWAN_LITERAL(0.6);
    rgb.b = ALWAN_LITERAL(0.9);

    /* _v variant */
    alwan_prismatic pris_v = alwan_rgb_to_prismatic_f64_v(rgb);
    alwan_rgb back_v = alwan_prismatic_to_rgb_f64_v(pris_v);

    /* pointer variant */
    alwan_prismatic pris_p;
    alwan_rgb_to_prismatic(&pris_p, &rgb);
    alwan_rgb back_p;
    alwan_prismatic_to_rgb(&back_p, &pris_p);

    TEST_ASSERT_NEAR(pris_v.L, pris_p.L, ALWAN_TEST_TOLERANCE, "prismatic L");
    TEST_ASSERT_NEAR(pris_v.s, pris_p.s, ALWAN_TEST_TOLERANCE, "prismatic s");
    TEST_ASSERT_NEAR(pris_v.h, pris_p.h, ALWAN_TEST_TOLERANCE, "prismatic h");

    TEST_ASSERT_NEAR(back_v.r, back_p.r, ALWAN_TEST_TOLERANCE, "prismatic->rgb r");
    TEST_ASSERT_NEAR(back_v.g, back_p.g, ALWAN_TEST_TOLERANCE, "prismatic->rgb g");
    TEST_ASSERT_NEAR(back_v.b, back_p.b, ALWAN_TEST_TOLERANCE, "prismatic->rgb b");

    TEST_PASS("prismatic_v_roundtrip");
}

static int test_hcl_v_roundtrip(void) {
    TEST_START("HCL _v round-trip");

    alwan_rgb rgb;
    rgb.r = ALWAN_LITERAL(0.3);
    rgb.g = ALWAN_LITERAL(0.6);
    rgb.b = ALWAN_LITERAL(0.9);

    /* _v variant */
    alwan_hcl hcl_v = alwan_rgb_to_hcl_f64_v(rgb);
    alwan_rgb back_v = alwan_hcl_to_rgb_f64_v(hcl_v);

    /* pointer variant */
    alwan_hcl hcl_p;
    alwan_rgb_to_hcl(&hcl_p, &rgb);
    alwan_rgb back_p;
    alwan_hcl_to_rgb(&back_p, &hcl_p);

    TEST_ASSERT_NEAR(hcl_v.H, hcl_p.H, ALWAN_TEST_TOLERANCE, "hcl H");
    TEST_ASSERT_NEAR(hcl_v.C, hcl_p.C, ALWAN_TEST_TOLERANCE, "hcl C");
    TEST_ASSERT_NEAR(hcl_v.L, hcl_p.L, ALWAN_TEST_TOLERANCE, "hcl L");

    TEST_ASSERT_NEAR(back_v.r, back_p.r, ALWAN_TEST_TOLERANCE, "hcl->rgb r");
    TEST_ASSERT_NEAR(back_v.g, back_p.g, ALWAN_TEST_TOLERANCE, "hcl->rgb g");
    TEST_ASSERT_NEAR(back_v.b, back_p.b, ALWAN_TEST_TOLERANCE, "hcl->rgb b");

    TEST_PASS("hcl_v_roundtrip");
}

static int test_ihls_v_roundtrip(void) {
    TEST_START("IHLS _v round-trip");

    alwan_rgb rgb;
    rgb.r = ALWAN_LITERAL(0.3);
    rgb.g = ALWAN_LITERAL(0.6);
    rgb.b = ALWAN_LITERAL(0.9);

    /* _v variant */
    alwan_ihls ihls_v = alwan_rgb_to_ihls_f64_v(rgb);
    alwan_rgb back_v = alwan_ihls_to_rgb_f64_v(ihls_v);

    /* pointer variant */
    alwan_ihls ihls_p;
    alwan_rgb_to_ihls(&ihls_p, &rgb);
    alwan_rgb back_p;
    alwan_ihls_to_rgb(&back_p, &ihls_p);

    TEST_ASSERT_NEAR(ihls_v.H, ihls_p.H, ALWAN_TEST_TOLERANCE, "ihls H");
    TEST_ASSERT_NEAR(ihls_v.L, ihls_p.L, ALWAN_TEST_TOLERANCE, "ihls L");
    TEST_ASSERT_NEAR(ihls_v.S, ihls_p.S, ALWAN_TEST_TOLERANCE, "ihls S");

    TEST_ASSERT_NEAR(back_v.r, back_p.r, ALWAN_TEST_TOLERANCE, "ihls->rgb r");
    TEST_ASSERT_NEAR(back_v.g, back_p.g, ALWAN_TEST_TOLERANCE, "ihls->rgb g");
    TEST_ASSERT_NEAR(back_v.b, back_p.b, ALWAN_TEST_TOLERANCE, "ihls->rgb b");

    TEST_PASS("ihls_v_roundtrip");
}

/* ----------------------------------------------------------------
 * Extended core: hdr-CIELAB, hdr-IPT, IgPgTg, ICaCb
 * ---------------------------------------------------------------- */

static int test_hdr_cielab_v_roundtrip(void) {
    TEST_START("hdr-CIELAB _v round-trip");

    alwan_xyz white;
    white.x = ALWAN_D65_X;
    white.y = ALWAN_D65_Y;
    white.z = ALWAN_D65_Z;

    /* _v variant */
    alwan_lab hdr_v = alwan_xyz_to_hdr_cielab_f64_v(white);
    alwan_xyz back_v = alwan_hdr_cielab_to_xyz_f64_v(hdr_v);

    /* pointer variant */
    alwan_lab hdr_p;
    alwan_xyz_to_hdr_cielab(&hdr_p, &white);
    alwan_xyz back_p;
    alwan_hdr_cielab_to_xyz(&back_p, &hdr_p);

    TEST_ASSERT_NEAR(hdr_v.L, hdr_p.L, ALWAN_TEST_TOLERANCE, "hdr_cielab L");
    TEST_ASSERT_NEAR(hdr_v.a, hdr_p.a, ALWAN_TEST_TOLERANCE, "hdr_cielab a");
    TEST_ASSERT_NEAR(hdr_v.b, hdr_p.b, ALWAN_TEST_TOLERANCE, "hdr_cielab b");

    TEST_ASSERT_NEAR(back_v.x, back_p.x, ALWAN_TEST_TOLERANCE, "hdr_cielab->xyz x");
    TEST_ASSERT_NEAR(back_v.y, back_p.y, ALWAN_TEST_TOLERANCE, "hdr_cielab->xyz y");
    TEST_ASSERT_NEAR(back_v.z, back_p.z, ALWAN_TEST_TOLERANCE, "hdr_cielab->xyz z");

    TEST_PASS("hdr_cielab_v_roundtrip");
}

static int test_hdr_ipt_v_roundtrip(void) {
    TEST_START("hdr-IPT _v round-trip");

    alwan_xyz white;
    white.x = ALWAN_D65_X;
    white.y = ALWAN_D65_Y;
    white.z = ALWAN_D65_Z;

    /* _v variant */
    alwan_ipt hdr_v = alwan_xyz_to_hdr_ipt_f64_v(white);
    alwan_xyz back_v = alwan_hdr_ipt_to_xyz_f64_v(hdr_v);

    /* pointer variant */
    alwan_ipt hdr_p;
    alwan_xyz_to_hdr_ipt(&hdr_p, &white);
    alwan_xyz back_p;
    alwan_hdr_ipt_to_xyz(&back_p, &hdr_p);

    TEST_ASSERT_NEAR(hdr_v.I, hdr_p.I, ALWAN_TEST_TOLERANCE, "hdr_ipt I");
    TEST_ASSERT_NEAR(hdr_v.P, hdr_p.P, ALWAN_TEST_TOLERANCE, "hdr_ipt P");
    TEST_ASSERT_NEAR(hdr_v.T, hdr_p.T, ALWAN_TEST_TOLERANCE, "hdr_ipt T");

    TEST_ASSERT_NEAR(back_v.x, back_p.x, ALWAN_TEST_TOLERANCE, "hdr_ipt->xyz x");
    TEST_ASSERT_NEAR(back_v.y, back_p.y, ALWAN_TEST_TOLERANCE, "hdr_ipt->xyz y");
    TEST_ASSERT_NEAR(back_v.z, back_p.z, ALWAN_TEST_TOLERANCE, "hdr_ipt->xyz z");

    TEST_PASS("hdr_ipt_v_roundtrip");
}

static int test_igpgtg_v_roundtrip(void) {
    TEST_START("IgPgTg _v round-trip");

    alwan_xyz white;
    white.x = ALWAN_D65_X;
    white.y = ALWAN_D65_Y;
    white.z = ALWAN_D65_Z;

    /* _v variant */
    alwan_igpgtg ig_v = alwan_xyz_to_igpgtg_f64_v(white);
    alwan_xyz back_v = alwan_igpgtg_to_xyz_f64_v(ig_v);

    /* pointer variant */
    alwan_igpgtg ig_p;
    alwan_xyz_to_igpgtg(&ig_p, &white);
    alwan_xyz back_p;
    alwan_igpgtg_to_xyz(&back_p, &ig_p);

    TEST_ASSERT_NEAR(ig_v.Ig, ig_p.Ig, ALWAN_TEST_TOLERANCE, "igpgtg Ig");
    TEST_ASSERT_NEAR(ig_v.Pg, ig_p.Pg, ALWAN_TEST_TOLERANCE, "igpgtg Pg");
    TEST_ASSERT_NEAR(ig_v.Tg, ig_p.Tg, ALWAN_TEST_TOLERANCE, "igpgtg Tg");

    TEST_ASSERT_NEAR(back_v.x, back_p.x, ALWAN_TEST_TOLERANCE, "igpgtg->xyz x");
    TEST_ASSERT_NEAR(back_v.y, back_p.y, ALWAN_TEST_TOLERANCE, "igpgtg->xyz y");
    TEST_ASSERT_NEAR(back_v.z, back_p.z, ALWAN_TEST_TOLERANCE, "igpgtg->xyz z");

    TEST_PASS("igpgtg_v_roundtrip");
}

static int test_icacb_v_roundtrip(void) {
    TEST_START("ICaCb _v round-trip");

    alwan_xyz white;
    white.x = ALWAN_D65_X;
    white.y = ALWAN_D65_Y;
    white.z = ALWAN_D65_Z;

    /* _v variant */
    alwan_icacb ic_v = alwan_xyz_to_icacb_f64_v(white);
    alwan_xyz back_v = alwan_icacb_to_xyz_f64_v(ic_v);

    /* pointer variant */
    alwan_icacb ic_p;
    alwan_xyz_to_icacb(&ic_p, &white);
    alwan_xyz back_p;
    alwan_icacb_to_xyz(&back_p, &ic_p);

    TEST_ASSERT_NEAR(ic_v.I, ic_p.I, ALWAN_TEST_TOLERANCE, "icacb I");
    TEST_ASSERT_NEAR(ic_v.Ca, ic_p.Ca, ALWAN_TEST_TOLERANCE, "icacb Ca");
    TEST_ASSERT_NEAR(ic_v.Cb, ic_p.Cb, ALWAN_TEST_TOLERANCE, "icacb Cb");

    TEST_ASSERT_NEAR(back_v.x, back_p.x, ALWAN_TEST_TOLERANCE, "icacb->xyz x");
    TEST_ASSERT_NEAR(back_v.y, back_p.y, ALWAN_TEST_TOLERANCE, "icacb->xyz y");
    TEST_ASSERT_NEAR(back_v.z, back_p.z, ALWAN_TEST_TOLERANCE, "icacb->xyz z");

    TEST_PASS("icacb_v_roundtrip");
}

/* ----------------------------------------------------------------
 * Colorspace core: UCS, UVW, direct cylindrical
 * ---------------------------------------------------------------- */

static int test_ucs_v_roundtrip(void) {
    TEST_START("UCS _v round-trip");

    alwan_xyz white;
    white.x = ALWAN_D65_X;
    white.y = ALWAN_D65_Y;
    white.z = ALWAN_D65_Z;

    /* _v variant */
    alwan_ucs ucs_v = alwan_xyz_to_ucs_f64_v(white);
    alwan_xyz back_v = alwan_ucs_to_xyz_f64_v(ucs_v);

    /* pointer variant */
    alwan_ucs ucs_p;
    alwan_xyz_to_ucs(&ucs_p, &white);
    alwan_xyz back_p;
    alwan_ucs_to_xyz(&back_p, &ucs_p);

    TEST_ASSERT_NEAR(ucs_v.U, ucs_p.U, ALWAN_TEST_TOLERANCE, "ucs U");
    TEST_ASSERT_NEAR(ucs_v.V, ucs_p.V, ALWAN_TEST_TOLERANCE, "ucs V");
    TEST_ASSERT_NEAR(ucs_v.W, ucs_p.W, ALWAN_TEST_TOLERANCE, "ucs W");

    TEST_ASSERT_NEAR(back_v.x, back_p.x, ALWAN_TEST_TOLERANCE, "ucs->xyz x");
    TEST_ASSERT_NEAR(back_v.y, back_p.y, ALWAN_TEST_TOLERANCE, "ucs->xyz y");
    TEST_ASSERT_NEAR(back_v.z, back_p.z, ALWAN_TEST_TOLERANCE, "ucs->xyz z");

    TEST_PASS("ucs_v_roundtrip");
}

static int test_uvw_v_roundtrip(void) {
    TEST_START("UVW _v round-trip");

    alwan_xyz color;
    color.x = ALWAN_LITERAL(50.0);
    color.y = ALWAN_LITERAL(40.0);
    color.z = ALWAN_LITERAL(30.0);

    alwan_xyz wp;
    wp.x = ALWAN_D65_X;
    wp.y = ALWAN_D65_Y;
    wp.z = ALWAN_D65_Z;

    /* _v variant */
    alwan_uvw uvw_v = alwan_xyz_to_uvw_f64_v(color, wp);
    alwan_xyz back_v = alwan_uvw_to_xyz_f64_v(uvw_v, wp);

    /* pointer variant */
    alwan_uvw uvw_p;
    alwan_xyz_to_uvw(&uvw_p, &color, &wp);
    alwan_xyz back_p;
    alwan_uvw_to_xyz(&back_p, &uvw_p, &wp);

    TEST_ASSERT_NEAR(uvw_v.U, uvw_p.U, ALWAN_TEST_TOLERANCE, "uvw U");
    TEST_ASSERT_NEAR(uvw_v.V, uvw_p.V, ALWAN_TEST_TOLERANCE, "uvw V");
    TEST_ASSERT_NEAR(uvw_v.W, uvw_p.W, ALWAN_TEST_TOLERANCE, "uvw W");

    TEST_ASSERT_NEAR(back_v.x, back_p.x, ALWAN_TEST_TOLERANCE, "uvw->xyz x");
    TEST_ASSERT_NEAR(back_v.y, back_p.y, ALWAN_TEST_TOLERANCE, "uvw->xyz y");
    TEST_ASSERT_NEAR(back_v.z, back_p.z, ALWAN_TEST_TOLERANCE, "uvw->xyz z");

    TEST_PASS("uvw_v_roundtrip");
}

static int test_xyz_lch_v_roundtrip(void) {
    TEST_START("XYZ->LCh _v round-trip");

    alwan_xyz color;
    color.x = ALWAN_LITERAL(50.0);
    color.y = ALWAN_LITERAL(40.0);
    color.z = ALWAN_LITERAL(30.0);

    alwan_xyz wp;
    wp.x = ALWAN_D65_X;
    wp.y = ALWAN_D65_Y;
    wp.z = ALWAN_D65_Z;

    /* _v variant */
    alwan_lch lch_v = alwan_xyz_to_lch_f64_v(color, wp);
    alwan_xyz back_v = alwan_lch_to_xyz_f64_v(lch_v, wp);

    /* pointer variant */
    alwan_lch lch_p;
    alwan_xyz_to_lch(&lch_p, &color, &wp);
    alwan_xyz back_p;
    alwan_lch_to_xyz(&back_p, &lch_p, &wp);

    TEST_ASSERT_NEAR(lch_v.L, lch_p.L, ALWAN_TEST_TOLERANCE, "xyz_lch L");
    TEST_ASSERT_NEAR(lch_v.C, lch_p.C, ALWAN_TEST_TOLERANCE, "xyz_lch C");
    TEST_ASSERT_NEAR(lch_v.h, lch_p.h, ALWAN_TEST_TOLERANCE, "xyz_lch h");

    TEST_ASSERT_NEAR(back_v.x, back_p.x, ALWAN_TEST_TOLERANCE, "xyz_lch->xyz x");
    TEST_ASSERT_NEAR(back_v.y, back_p.y, ALWAN_TEST_TOLERANCE, "xyz_lch->xyz y");
    TEST_ASSERT_NEAR(back_v.z, back_p.z, ALWAN_TEST_TOLERANCE, "xyz_lch->xyz z");

    TEST_PASS("xyz_lch_v_roundtrip");
}

static int test_xyz_lchuv_v_roundtrip(void) {
    TEST_START("XYZ->LChuv _v round-trip");

    alwan_xyz color;
    color.x = ALWAN_LITERAL(50.0);
    color.y = ALWAN_LITERAL(40.0);
    color.z = ALWAN_LITERAL(30.0);

    alwan_xyz wp;
    wp.x = ALWAN_D65_X;
    wp.y = ALWAN_D65_Y;
    wp.z = ALWAN_D65_Z;

    /* _v variant */
    alwan_lchuv lchuv_v = alwan_xyz_to_lchuv_f64_v(color, wp);
    alwan_xyz back_v = alwan_lchuv_to_xyz_f64_v(lchuv_v, wp);

    /* pointer variant */
    alwan_lchuv lchuv_p;
    alwan_xyz_to_lchuv(&lchuv_p, &color, &wp);
    alwan_xyz back_p;
    alwan_lchuv_to_xyz(&back_p, &lchuv_p, &wp);

    TEST_ASSERT_NEAR(lchuv_v.L, lchuv_p.L, ALWAN_TEST_TOLERANCE, "xyz_lchuv L");
    TEST_ASSERT_NEAR(lchuv_v.C, lchuv_p.C, ALWAN_TEST_TOLERANCE, "xyz_lchuv C");
    TEST_ASSERT_NEAR(lchuv_v.h, lchuv_p.h, ALWAN_TEST_TOLERANCE, "xyz_lchuv h");

    TEST_ASSERT_NEAR(back_v.x, back_p.x, ALWAN_TEST_TOLERANCE, "xyz_lchuv->xyz x");
    TEST_ASSERT_NEAR(back_v.y, back_p.y, ALWAN_TEST_TOLERANCE, "xyz_lchuv->xyz y");
    TEST_ASSERT_NEAR(back_v.z, back_p.z, ALWAN_TEST_TOLERANCE, "xyz_lchuv->xyz z");

    TEST_PASS("xyz_lchuv_v_roundtrip");
}

/* ----------------------------------------------------------------
 * Delta E metrics: compare _v() against pointer-based
 * ---------------------------------------------------------------- */

static int test_delta_e_76_v(void) {
    TEST_START("delta_e_76 _v");

    alwan_lab lab1;
    lab1.L = ALWAN_LITERAL(50.0);
    lab1.a = ALWAN_LITERAL(20.0);
    lab1.b = ALWAN_LITERAL(-30.0);

    alwan_lab lab2;
    lab2.L = ALWAN_LITERAL(60.0);
    lab2.a = ALWAN_LITERAL(10.0);
    lab2.b = ALWAN_LITERAL(-20.0);

    /* _v variant */
    alwan_f64 de_v = alwan_delta_e_76_f64_v(lab1, lab2);

    /* pointer variant */
    alwan_f64 de_p = alwan_delta_e_76(&lab1, &lab2);

    TEST_ASSERT_NEAR(de_v, de_p, ALWAN_TEST_TOLERANCE, "delta_e_76");

    TEST_PASS("delta_e_76_v");
}

static int test_delta_e_94_v(void) {
    TEST_START("delta_e_94 _v");

    alwan_lab lab1;
    lab1.L = ALWAN_LITERAL(50.0);
    lab1.a = ALWAN_LITERAL(20.0);
    lab1.b = ALWAN_LITERAL(-30.0);

    alwan_lab lab2;
    lab2.L = ALWAN_LITERAL(60.0);
    lab2.a = ALWAN_LITERAL(10.0);
    lab2.b = ALWAN_LITERAL(-20.0);

    /* _v variant */
    alwan_f64 de_v = alwan_delta_e_94_f64_v(lab1, lab2);

    /* pointer variant */
    alwan_f64 de_p = alwan_delta_e_94(&lab1, &lab2);

    TEST_ASSERT_NEAR(de_v, de_p, ALWAN_TEST_TOLERANCE, "delta_e_94");

    TEST_PASS("delta_e_94_v");
}

static int test_delta_e_2000_v(void) {
    TEST_START("delta_e_2000 _v");

    alwan_lab lab1;
    lab1.L = ALWAN_LITERAL(50.0);
    lab1.a = ALWAN_LITERAL(20.0);
    lab1.b = ALWAN_LITERAL(-30.0);

    alwan_lab lab2;
    lab2.L = ALWAN_LITERAL(60.0);
    lab2.a = ALWAN_LITERAL(10.0);
    lab2.b = ALWAN_LITERAL(-20.0);

    /* _v variant */
    alwan_f64 de_v = alwan_delta_e_2000_f64_v(lab1, lab2);

    /* pointer variant */
    alwan_f64 de_p = alwan_delta_e_2000(&lab1, &lab2);

    TEST_ASSERT_NEAR(de_v, de_p, ALWAN_TEST_TOLERANCE, "delta_e_2000");

    TEST_PASS("delta_e_2000_v");
}

static int test_delta_e_cmc_v(void) {
    TEST_START("delta_e_cmc _v");

    alwan_lab lab1;
    lab1.L = ALWAN_LITERAL(50.0);
    lab1.a = ALWAN_LITERAL(20.0);
    lab1.b = ALWAN_LITERAL(-30.0);

    alwan_lab lab2;
    lab2.L = ALWAN_LITERAL(60.0);
    lab2.a = ALWAN_LITERAL(10.0);
    lab2.b = ALWAN_LITERAL(-20.0);

    alwan_f64 l = ALWAN_LITERAL(1.0);
    alwan_f64 c = ALWAN_LITERAL(1.0);

    /* _v variant */
    alwan_f64 de_v = alwan_delta_e_cmc_f64_v(lab1, lab2, l, c);

    /* pointer variant */
    alwan_f64 de_p = alwan_delta_e_cmc(&lab1, &lab2, l, c);

    TEST_ASSERT_NEAR(de_v, de_p, ALWAN_TEST_TOLERANCE, "delta_e_cmc");

    TEST_PASS("delta_e_cmc_v");
}

/* ----------------------------------------------------------------
 * CAM delta E core: compare _v() against pointer-based
 * ---------------------------------------------------------------- */

static int test_delta_e_cam02_lcd_v(void) {
    TEST_START("delta_e_cam02_lcd _v");
    alwan_cam_jab jab1; jab1.J = ALWAN_LITERAL(50.0); jab1.a = ALWAN_LITERAL(10.0); jab1.b = ALWAN_LITERAL(-5.0);
    alwan_cam_jab jab2; jab2.J = ALWAN_LITERAL(60.0); jab2.a = ALWAN_LITERAL(15.0); jab2.b = ALWAN_LITERAL(5.0);
    alwan_f64 de_v = alwan_delta_e_cam02_lcd_f64_v(jab1, jab2);
    alwan_f64 de_p = alwan_delta_e_cam02_lcd(&jab1, &jab2);
    TEST_ASSERT_NEAR(de_v, de_p, ALWAN_TEST_TOLERANCE, "cam02_lcd");
    TEST_PASS("delta_e_cam02_lcd_v");
}

static int test_delta_e_cam16_ucs_v(void) {
    TEST_START("delta_e_cam16_ucs _v");
    alwan_cam_jab jab1; jab1.J = ALWAN_LITERAL(50.0); jab1.a = ALWAN_LITERAL(10.0); jab1.b = ALWAN_LITERAL(-5.0);
    alwan_cam_jab jab2; jab2.J = ALWAN_LITERAL(60.0); jab2.a = ALWAN_LITERAL(15.0); jab2.b = ALWAN_LITERAL(5.0);
    alwan_f64 de_v = alwan_delta_e_cam16_ucs_f64_v(jab1, jab2);
    alwan_f64 de_p = alwan_delta_e_cam16_ucs(&jab1, &jab2);
    TEST_ASSERT_NEAR(de_v, de_p, ALWAN_TEST_TOLERANCE, "cam16_ucs");
    TEST_PASS("delta_e_cam16_ucs_v");
}

/* ----------------------------------------------------------------
 * CMYK core: compare _v() against pointer-based
 * ---------------------------------------------------------------- */

static int test_cmyk_v_roundtrip(void) {
    TEST_START("CMYK _v round-trip");
    alwan_cmy cmy;
    cmy.c = ALWAN_LITERAL(0.7);
    cmy.m = ALWAN_LITERAL(0.4);
    cmy.y = ALWAN_LITERAL(0.9);

    alwan_cmyk cmyk_v = alwan_cmy_to_cmyk_f64_v(cmy);
    alwan_cmy back_v = alwan_cmyk_to_cmy_f64_v(cmyk_v);

    /* pointer variant */
    alwan_f64 c_p, m_p, y_p, k_p;
    alwan_cmy_to_cmyk(&c_p, &m_p, &y_p, &k_p, &cmy);
    alwan_cmy back_p;
    alwan_cmyk_to_cmy(&back_p, c_p, m_p, y_p, k_p);

    TEST_ASSERT_NEAR(cmyk_v.c, c_p, ALWAN_EPSILON, "cmyk c");
    TEST_ASSERT_NEAR(cmyk_v.m, m_p, ALWAN_EPSILON, "cmyk m");
    TEST_ASSERT_NEAR(cmyk_v.y, y_p, ALWAN_EPSILON, "cmyk y");
    TEST_ASSERT_NEAR(cmyk_v.k, k_p, ALWAN_EPSILON, "cmyk k");

    TEST_ASSERT_NEAR(back_v.c, back_p.c, ALWAN_EPSILON, "cmyk->cmy c");
    TEST_ASSERT_NEAR(back_v.m, back_p.m, ALWAN_EPSILON, "cmyk->cmy m");
    TEST_ASSERT_NEAR(back_v.y, back_p.y, ALWAN_EPSILON, "cmyk->cmy y");

    TEST_PASS("cmyk_v_roundtrip");
}

/* ----------------------------------------------------------------
 * mat3_inv core: compare _v() against pointer-based
 * ---------------------------------------------------------------- */

static int test_mat3_inv_v(void) {
    TEST_START("mat3_inv _v");

    /* A non-trivial matrix */
    alwan_mat3x3 m;
    m.m[0] = ALWAN_LITERAL(1.0); m.m[1] = ALWAN_LITERAL(2.0); m.m[2] = ALWAN_LITERAL(3.0);
    m.m[3] = ALWAN_LITERAL(0.0); m.m[4] = ALWAN_LITERAL(1.0); m.m[5] = ALWAN_LITERAL(4.0);
    m.m[6] = ALWAN_LITERAL(5.0); m.m[7] = ALWAN_LITERAL(6.0); m.m[8] = ALWAN_LITERAL(0.0);

    /* _v variant */
    alwan_mat3x3 inv_v = alwan_mat3_inv_f64_v(m);

    /* pointer variant */
    alwan_mat3x3 inv_p;
    alwan_mat3_inv(&inv_p, &m);

    /* Compare all 9 elements */
    for (int i = 0; i < 9; i++) {
        char label[32];
        sprintf(label, "inv[%d]", i);
        TEST_ASSERT_NEAR(inv_v.m[i], inv_p.m[i], ALWAN_TEST_TOLERANCE, label);
    }

    /* Verify M * M^-1 ~= I */
    alwan_mat3x3 product = alwan_mat3_mul_f64_v(m, inv_v);
    TEST_ASSERT_NEAR(product.m[0], ALWAN_LITERAL(1.0), ALWAN_TEST_TOLERANCE, "M*Minv[0,0]");
    TEST_ASSERT_NEAR(product.m[4], ALWAN_LITERAL(1.0), ALWAN_TEST_TOLERANCE, "M*Minv[1,1]");
    TEST_ASSERT_NEAR(product.m[8], ALWAN_LITERAL(1.0), ALWAN_TEST_TOLERANCE, "M*Minv[2,2]");
    TEST_ASSERT_NEAR(product.m[1], ALWAN_LITERAL(0.0), ALWAN_TEST_TOLERANCE, "M*Minv[0,1]");
    TEST_ASSERT_NEAR(product.m[3], ALWAN_LITERAL(0.0), ALWAN_TEST_TOLERANCE, "M*Minv[1,0]");

    TEST_PASS("mat3_inv_v");
}

/* ----------------------------------------------------------------
 * ICtCp PQ core: RGB -> ICtCp(PQ) -> RGB roundtrip
 * ---------------------------------------------------------------- */

static int test_ictcp_pq_v_roundtrip(void) {
    TEST_START("ICtCp PQ _v round-trip");
    /* Use small HDR-range BT.2020 linear values */
    alwan_rgb rgb_in;
    rgb_in.r = ALWAN_LITERAL(0.18);
    rgb_in.g = ALWAN_LITERAL(0.10);
    rgb_in.b = ALWAN_LITERAL(0.05);

    alwan_ictcp ictcp = alwan_rgb_to_ictcp_pq_f64_v(rgb_in);
    alwan_rgb rgb_out = alwan_ictcp_pq_to_rgb_f64_v(ictcp);

    TEST_ASSERT_NEAR(rgb_out.r, rgb_in.r, ALWAN_TEST_TOLERANCE, "r");
    TEST_ASSERT_NEAR(rgb_out.g, rgb_in.g, ALWAN_TEST_TOLERANCE, "g");
    TEST_ASSERT_NEAR(rgb_out.b, rgb_in.b, ALWAN_TEST_TOLERANCE, "b");
    TEST_PASS("ictcp_pq_v_roundtrip");
}

/* ----------------------------------------------------------------
 * ICtCp HLG core: RGB -> ICtCp(HLG) -> RGB roundtrip
 * ---------------------------------------------------------------- */

static int test_ictcp_hlg_v_roundtrip(void) {
    TEST_START("ICtCp HLG _v round-trip");
    alwan_rgb rgb_in;
    rgb_in.r = ALWAN_LITERAL(0.18);
    rgb_in.g = ALWAN_LITERAL(0.10);
    rgb_in.b = ALWAN_LITERAL(0.05);

    alwan_ictcp ictcp = alwan_rgb_to_ictcp_hlg_f64_v(rgb_in);
    alwan_rgb rgb_out = alwan_ictcp_hlg_to_rgb_f64_v(ictcp);

    TEST_ASSERT_NEAR(rgb_out.r, rgb_in.r, ALWAN_TEST_TOLERANCE, "r");
    TEST_ASSERT_NEAR(rgb_out.g, rgb_in.g, ALWAN_TEST_TOLERANCE, "g");
    TEST_ASSERT_NEAR(rgb_out.b, rgb_in.b, ALWAN_TEST_TOLERANCE, "b");
    TEST_PASS("ictcp_hlg_v_roundtrip");
}

/* ----------------------------------------------------------------
 * OSA-UCS core: XYZ -> OSA-UCS forward (no exact inverse)
 * Verify output matches known range
 * ---------------------------------------------------------------- */

static int test_osa_ucs_v_forward(void) {
    TEST_START("OSA-UCS _v forward");
    /* D65 white at Y=100 scale */
    alwan_xyz xyz_in;
    xyz_in.x = ALWAN_LITERAL(95.047);
    xyz_in.y = ALWAN_LITERAL(100.0);
    xyz_in.z = ALWAN_LITERAL(108.883);

    alwan_osa_ucs osa = alwan_xyz_to_osa_ucs_f64_v(xyz_in);

    /* For a D65 white, j and g should be near 0 (achromatic) */
    TEST_ASSERT_NEAR(osa.j, ALWAN_ZERO, ALWAN_LITERAL(1.0), "j near 0");
    TEST_ASSERT_NEAR(osa.g, ALWAN_ZERO, ALWAN_LITERAL(1.0), "g near 0");
    /* L should be a reasonable positive value */
    int l_ok = (osa.L > ALWAN_LITERAL(-20.0) && osa.L < ALWAN_LITERAL(20.0));
    TEST_ASSERT(l_ok, "L in reasonable range");
    TEST_PASS("osa_ucs_v_forward");
}

/* ----------------------------------------------------------------
 * RLAB core: XYZ -> RLAB -> XYZ roundtrip
 * ---------------------------------------------------------------- */

static int test_rlab_v_forward(void) {
    TEST_START("RLAB _v forward");

    alwan_xyz xyz_in;
    xyz_in.x = ALWAN_LITERAL(19.01);
    xyz_in.y = ALWAN_LITERAL(20.0);
    xyz_in.z = ALWAN_LITERAL(21.78);

    /* D65 white point */
    alwan_xyz xyz_w;
    xyz_w.x = ALWAN_LITERAL(95.05);
    xyz_w.y = ALWAN_LITERAL(100.0);
    xyz_w.z = ALWAN_LITERAL(108.88);

    /* Reference white = D65 */
    alwan_xyz xyz_n = xyz_w;

    /* Average surround: sigma = 1/2.3, D = 1.0 (hard copy) */
    alwan_f64 sigma = ALWAN_ONE / ALWAN_LITERAL(2.3);
    alwan_f64 D = ALWAN_ONE;

    alwan_rlab_v_correlates_f64 corr = alwan_rlab_forward_f64_v(xyz_in, xyz_w, xyz_n, sigma, D);

    /* L should be positive (RLAB L can exceed 100 on Y=100 scale) */
    int l_ok = (corr.L > ALWAN_ZERO);
    TEST_ASSERT(l_ok, "L > 0");
    /* Hue should be in [0, 360) */
    int h_ok = (corr.h >= ALWAN_ZERO && corr.h < ALWAN_LITERAL(360.0));
    TEST_ASSERT(h_ok, "h in [0, 360)");
    /* Chroma should be non-negative */
    int c_ok = (corr.C >= ALWAN_ZERO);
    TEST_ASSERT(c_ok, "C >= 0");
    TEST_PASS("rlab_v_forward");
}

/* ----------------------------------------------------------------
 * ATD95 core: forward (no inverse). Verify it runs and produces
 * sensible output for a known input.
 * ---------------------------------------------------------------- */

static int test_atd95_v_forward(void) {
    TEST_START("ATD95 _v forward");

    alwan_xyz xyz_in;
    xyz_in.x = ALWAN_LITERAL(19.01);
    xyz_in.y = ALWAN_LITERAL(20.0);
    xyz_in.z = ALWAN_LITERAL(21.78);

    alwan_xyz white;
    white.x = ALWAN_LITERAL(95.05);
    white.y = ALWAN_LITERAL(100.0);
    white.z = ALWAN_LITERAL(108.88);

    alwan_f64 Y_0 = ALWAN_LITERAL(318.0);
    alwan_f64 k1 = ALWAN_LITERAL(1.0);
    alwan_f64 k2 = ALWAN_LITERAL(0.0);
    alwan_f64 sigma = ALWAN_LITERAL(300.0);

    alwan_atd95_v_correlates_f64 corr = alwan_atd95_forward_f64_v(xyz_in, white, Y_0, k1, k2, sigma);

    /* Brightness should be positive */
    int br_ok = (corr.Br > ALWAN_ZERO);
    TEST_ASSERT(br_ok, "Br > 0");
    /* Hue should be in [0, 360) */
    int h_ok = (corr.H >= ALWAN_ZERO && corr.H < ALWAN_LITERAL(360.0));
    TEST_ASSERT(h_ok, "H in [0, 360)");
    TEST_PASS("atd95_v_forward");
}

/* ----------------------------------------------------------------
 * LLAB core: forward (no inverse). Verify it runs and produces
 * sensible output for a known input.
 * ---------------------------------------------------------------- */

static int test_llab_v_forward(void) {
    TEST_START("LLAB _v forward");

    alwan_xyz xyz_in;
    xyz_in.x = ALWAN_LITERAL(19.01);
    xyz_in.y = ALWAN_LITERAL(20.0);
    xyz_in.z = ALWAN_LITERAL(21.78);

    /* D65 as both test and reference illuminant */
    alwan_xyz xyz_0;
    xyz_0.x = ALWAN_LITERAL(95.05);
    xyz_0.y = ALWAN_LITERAL(100.0);
    xyz_0.z = ALWAN_LITERAL(108.88);

    alwan_xyz xyz_r = xyz_0;
    alwan_f64 Y_b = ALWAN_LITERAL(20.0);
    /* Average surround factors */
    alwan_f64 D = ALWAN_LITERAL(1.0);
    alwan_f64 F_S = ALWAN_LITERAL(3.0);
    alwan_f64 F_L = ALWAN_LITERAL(0.0);

    alwan_llab_v_correlates_f64 corr = alwan_llab_forward_f64_v(xyz_in, xyz_0, xyz_r, Y_b, D, F_S, F_L);

    /* L should be positive (for Y=20) */
    int l_ok = (corr.L > ALWAN_ZERO && corr.L < ALWAN_LITERAL(100.0));
    TEST_ASSERT(l_ok, "L in (0, 100)");
    /* Hue should be in [0, 360) */
    int h_ok = (corr.h >= ALWAN_ZERO && corr.h < ALWAN_LITERAL(360.0));
    TEST_ASSERT(h_ok, "h in [0, 360)");
    TEST_PASS("llab_v_forward");
}

/* ----------------------------------------------------------------
 * Vision (CVD) core: severity=0 should give identity
 * ---------------------------------------------------------------- */

static int test_cvd_v_identity(void) {
    TEST_START("CVD _v identity at severity=0");

    alwan_rgb rgb_in;
    rgb_in.r = ALWAN_LITERAL(0.5);
    rgb_in.g = ALWAN_LITERAL(0.3);
    rgb_in.b = ALWAN_LITERAL(0.8);

    /* severity = 0 -> output should equal input */
    alwan_rgb out_p = alwan_simulate_protanopia_f64_v(rgb_in, ALWAN_ZERO);
    alwan_rgb out_d = alwan_simulate_deuteranopia_f64_v(rgb_in, ALWAN_ZERO);
    alwan_rgb out_t = alwan_simulate_tritanopia_f64_v(rgb_in, ALWAN_ZERO);

    TEST_ASSERT_NEAR(out_p.r, rgb_in.r, ALWAN_TEST_TOLERANCE, "protan r");
    TEST_ASSERT_NEAR(out_p.g, rgb_in.g, ALWAN_TEST_TOLERANCE, "protan g");
    TEST_ASSERT_NEAR(out_p.b, rgb_in.b, ALWAN_TEST_TOLERANCE, "protan b");
    TEST_ASSERT_NEAR(out_d.r, rgb_in.r, ALWAN_TEST_TOLERANCE, "deutan r");
    TEST_ASSERT_NEAR(out_d.g, rgb_in.g, ALWAN_TEST_TOLERANCE, "deutan g");
    TEST_ASSERT_NEAR(out_d.b, rgb_in.b, ALWAN_TEST_TOLERANCE, "deutan b");
    TEST_ASSERT_NEAR(out_t.r, rgb_in.r, ALWAN_TEST_TOLERANCE, "tritan r");
    TEST_ASSERT_NEAR(out_t.g, rgb_in.g, ALWAN_TEST_TOLERANCE, "tritan g");
    TEST_ASSERT_NEAR(out_t.b, rgb_in.b, ALWAN_TEST_TOLERANCE, "tritan b");
    TEST_PASS("cvd_v_identity");
}

/* ----------------------------------------------------------------
 * LGG core: identity parameters should give identity output
 * ---------------------------------------------------------------- */

static int test_lgg_v_identity(void) {
    TEST_START("LGG _v identity");

    alwan_rgb rgb_in;
    rgb_in.r = ALWAN_LITERAL(0.5);
    rgb_in.g = ALWAN_LITERAL(0.3);
    rgb_in.b = ALWAN_LITERAL(0.8);

    /* Identity: lift=0, gamma=1, gain=1 */
    alwan_rgb lift  = { ALWAN_ZERO, ALWAN_ZERO, ALWAN_ZERO };
    alwan_rgb gamma = { ALWAN_ONE,  ALWAN_ONE,  ALWAN_ONE  };
    alwan_rgb gain  = { ALWAN_ONE,  ALWAN_ONE,  ALWAN_ONE  };

    alwan_rgb out = alwan_lgg_apply_f64_v(rgb_in, lift, gamma, gain);

    TEST_ASSERT_NEAR(out.r, rgb_in.r, ALWAN_TEST_TOLERANCE, "r");
    TEST_ASSERT_NEAR(out.g, rgb_in.g, ALWAN_TEST_TOLERANCE, "g");
    TEST_ASSERT_NEAR(out.b, rgb_in.b, ALWAN_TEST_TOLERANCE, "b");
    TEST_PASS("lgg_v_identity");
}

/* ----------------------------------------------------------------
 * Color matrix core: identity matrix should give identity output
 * ---------------------------------------------------------------- */

static int test_color_matrix_v_identity(void) {
    TEST_START("color_matrix _v identity");

    alwan_rgb rgb_in;
    rgb_in.r = ALWAN_LITERAL(0.5);
    rgb_in.g = ALWAN_LITERAL(0.3);
    rgb_in.b = ALWAN_LITERAL(0.8);

    alwan_mat3x3 id = alwan_mat3_identity_f64_v();
    alwan_rgb out = alwan_color_matrix_apply_f64_v(rgb_in, id);

    TEST_ASSERT_NEAR(out.r, rgb_in.r, ALWAN_TEST_TOLERANCE, "r");
    TEST_ASSERT_NEAR(out.g, rgb_in.g, ALWAN_TEST_TOLERANCE, "g");
    TEST_ASSERT_NEAR(out.b, rgb_in.b, ALWAN_TEST_TOLERANCE, "b");
    TEST_PASS("color_matrix_v_identity");
}

/* ----------------------------------------------------------------
 * CAT core: D65->D65 adaptation should be identity
 * ---------------------------------------------------------------- */

static int test_cat_v_identity(void) {
    TEST_START("CAT _v D65->D65 identity");

    alwan_xyz d65;
    d65.x = ALWAN_LITERAL(0.95047);
    d65.y = ALWAN_ONE;
    d65.z = ALWAN_LITERAL(1.08883);

    /* XYZ scaling: same white -> identity matrix */
    alwan_mat3x3 cat_id = alwan_cat_xyz_scaling_f64_v(d65, d65);
    TEST_ASSERT_NEAR(cat_id.m[0], ALWAN_ONE, ALWAN_TEST_TOLERANCE, "xyz_scaling[0]");
    TEST_ASSERT_NEAR(cat_id.m[4], ALWAN_ONE, ALWAN_TEST_TOLERANCE, "xyz_scaling[4]");
    TEST_ASSERT_NEAR(cat_id.m[8], ALWAN_ONE, ALWAN_TEST_TOLERANCE, "xyz_scaling[8]");

    /* Apply identity adaptation to a test color */
    alwan_xyz test_in;
    test_in.x = ALWAN_LITERAL(0.5);
    test_in.y = ALWAN_LITERAL(0.3);
    test_in.z = ALWAN_LITERAL(0.8);

    alwan_xyz test_out = alwan_cat_adapt_f64_v(cat_id, test_in);
    TEST_ASSERT_NEAR(test_out.x, test_in.x, ALWAN_TEST_TOLERANCE, "adapt x");
    TEST_ASSERT_NEAR(test_out.y, test_in.y, ALWAN_TEST_TOLERANCE, "adapt y");
    TEST_ASSERT_NEAR(test_out.z, test_in.z, ALWAN_TEST_TOLERANCE, "adapt z");
    TEST_PASS("cat_v_identity");
}

/* ----------------------------------------------------------------
 * Quality core: CCT McCamy vs pointer-based
 * ---------------------------------------------------------------- */

static int test_cct_mccamy_v(void) {
    TEST_START("CCT McCamy _v");
    alwan_f64 x = ALWAN_LITERAL(0.3127);
    alwan_f64 y = ALWAN_LITERAL(0.3290);
    alwan_f64 cct = alwan_cct_mccamy_f64_v(x, y);
    /* D65 should be ~6504K */
    int ok = (cct > ALWAN_LITERAL(6000.0) && cct < ALWAN_LITERAL(7000.0));
    TEST_ASSERT(ok, "D65 CCT in [6000, 7000]");
    TEST_PASS("cct_mccamy_v");
}

static int test_cct_hernandez_v(void) {
    TEST_START("CCT Hernandez _v");
    alwan_f64 x = ALWAN_LITERAL(0.3127);
    alwan_f64 y = ALWAN_LITERAL(0.3290);
    alwan_f64 cct = alwan_cct_hernandez_f64_v(x, y);
    /* D65 should be ~6500K */
    int ok = (cct > ALWAN_LITERAL(6000.0) && cct < ALWAN_LITERAL(7000.0));
    TEST_ASSERT(ok, "D65 CCT in [6000, 7000]");
    TEST_PASS("cct_hernandez_v");
}

static int test_cct_to_xy_kang_v(void) {
    TEST_START("CCT to xy Kang _v");
    alwan_vec2 xy = alwan_cct_to_xy_kang_f64_v(ALWAN_LITERAL(6504.0));
    /* D65: x~0.3127, y~0.3290 */
    TEST_ASSERT_NEAR(xy.v[0], ALWAN_LITERAL(0.3127), ALWAN_LITERAL(0.01), "x near 0.3127");
    TEST_ASSERT_NEAR(xy.v[1], ALWAN_LITERAL(0.3290), ALWAN_LITERAL(0.01), "y near 0.3290");
    TEST_PASS("cct_to_xy_kang_v");
}

/* ----------------------------------------------------------------
 * Rayleigh core: cross section produces positive result
 * ---------------------------------------------------------------- */

static int test_rayleigh_cross_section_v(void) {
    TEST_START("Rayleigh cross section _v");
    alwan_f64 sigma = rayleigh_cross_section_f64_v(
        ALWAN_LITERAL(550.0), RAYLEIGH_V_STD_CO2, RAYLEIGH_V_STD_TEMPERATURE);
    int ok = (sigma > ALWAN_ZERO);
    TEST_ASSERT(ok, "sigma > 0");
    TEST_PASS("rayleigh_cross_section_v");
}

static int test_rayleigh_optical_depth_v(void) {
    TEST_START("Rayleigh optical depth _v");
    alwan_f64 tau = rayleigh_optical_depth_f64_v(
        ALWAN_LITERAL(550.0), RAYLEIGH_V_STD_CO2, RAYLEIGH_V_STD_TEMPERATURE,
        RAYLEIGH_V_STD_PRESSURE, ALWAN_ZERO, ALWAN_ZERO);
    int ok = (tau > ALWAN_ZERO);
    TEST_ASSERT(ok, "tau > 0");
    TEST_PASS("rayleigh_optical_depth_v");
}

/* ----------------------------------------------------------------
 * Gamut core: clip, Oklab round-trip, cusp
 * ---------------------------------------------------------------- */

static int test_gamut_clip_v(void) {
    TEST_START("gamut clip _v");
    alwan_vec3 oob;
    oob.v[0] = ALWAN_LITERAL(-0.5);
    oob.v[1] = ALWAN_LITERAL(0.5);
    oob.v[2] = ALWAN_LITERAL(1.5);
    alwan_vec3 clipped = gamut_clip_f64_v(oob);
    TEST_ASSERT_NEAR(clipped.v[0], ALWAN_ZERO, ALWAN_EPSILON, "clip r");
    TEST_ASSERT_NEAR(clipped.v[1], ALWAN_LITERAL(0.5), ALWAN_EPSILON, "clip g");
    TEST_ASSERT_NEAR(clipped.v[2], ALWAN_ONE, ALWAN_EPSILON, "clip b");
    TEST_PASS("gamut_clip_v");
}

static int test_gamut_oklab_roundtrip_v(void) {
    TEST_START("gamut Oklab _v round-trip");
    alwan_vec3 rgb;
    rgb.v[0] = ALWAN_LITERAL(0.5);
    rgb.v[1] = ALWAN_LITERAL(0.3);
    rgb.v[2] = ALWAN_LITERAL(0.8);
    alwan_vec3 oklab = gamut_linear_srgb_to_oklab_f64_v(rgb);
    alwan_vec3 back = gamut_oklab_to_linear_srgb_f64_v(oklab);
    TEST_ASSERT_NEAR(back.v[0], rgb.v[0], ALWAN_LITERAL(1e-6), "r");
    TEST_ASSERT_NEAR(back.v[1], rgb.v[1], ALWAN_LITERAL(1e-6), "g");
    TEST_ASSERT_NEAR(back.v[2], rgb.v[2], ALWAN_LITERAL(1e-6), "b");
    TEST_PASS("gamut_oklab_roundtrip_v");
}

static int test_gamut_find_cusp_v(void) {
    TEST_START("gamut find cusp _v");
    /* Red hue direction */
    alwan_vec2 cusp = gamut_find_cusp_f64_v(ALWAN_ONE, ALWAN_ZERO);
    int ok = (cusp.v[0] > ALWAN_ZERO && cusp.v[0] < ALWAN_ONE &&
              cusp.v[1] > ALWAN_ZERO);
    TEST_ASSERT(ok, "cusp L in (0,1), C > 0");
    TEST_PASS("gamut_find_cusp_v");
}

/* ----------------------------------------------------------------
 * View core: tonemap, AgX curve
 * ---------------------------------------------------------------- */

static int test_aces_tonemap_v(void) {
    TEST_START("ACES tonemap _v");
    /* Black maps to ~0, mid-gray stays mid, white asymptotes */
    alwan_f64 t0 = alwan_aces_tonemap_f64_v(ALWAN_ZERO);
    alwan_f64 t_mid = alwan_aces_tonemap_f64_v(ALWAN_LITERAL(0.18));
    alwan_f64 t_hi = alwan_aces_tonemap_f64_v(ALWAN_LITERAL(10.0));
    TEST_ASSERT_NEAR(t0, ALWAN_ZERO, ALWAN_LITERAL(0.01), "f(0)~0");
    int mid_ok = (t_mid > ALWAN_LITERAL(0.05) && t_mid < ALWAN_LITERAL(0.5));
    TEST_ASSERT(mid_ok, "f(0.18) in reasonable range");
    int hi_ok = (t_hi > ALWAN_LITERAL(0.9) && t_hi < ALWAN_LITERAL(1.1));
    TEST_ASSERT(hi_ok, "f(10) near 1.0");
    TEST_PASS("aces_tonemap_v");
}

static int test_agx_curve_v(void) {
    TEST_START("AgX curve _v");
    alwan_f64 t0 = alwan_agx_curve_f64_v(ALWAN_ZERO);
    alwan_f64 t1 = alwan_agx_curve_f64_v(ALWAN_ONE);
    TEST_ASSERT_NEAR(t0, ALWAN_ZERO, ALWAN_LITERAL(0.01), "curve(0)~0");
    int ok = (t1 > ALWAN_LITERAL(0.5) && t1 <= ALWAN_ONE);
    TEST_ASSERT(ok, "curve(1) in (0.5, 1]");
    TEST_PASS("agx_curve_v");
}

/* ----------------------------------------------------------------
 * Test runner
 * ---------------------------------------------------------------- */

typedef struct {
    char const *name;
    int (*fn)(void);
} test_entry;

int test_58_core_headers_main(void) {
    static test_entry const tests[] = {
        {"mat3_identity",           test_mat3_identity},
        {"mat3_det",                test_mat3_det},
        {"mat3_mulv",               test_mat3_mulv},
        {"mat3_mul",                test_mat3_mul},
        {"oklab_v_roundtrip",       test_oklab_v_roundtrip},
        {"oklch_v_roundtrip",       test_oklch_v_roundtrip},
        {"jzazbz_v_roundtrip",      test_jzazbz_v_roundtrip},
        {"jzczhz_v_roundtrip",      test_jzczhz_v_roundtrip},
        {"xyy_v_roundtrip",         test_xyy_v_roundtrip},
        {"lab_v_roundtrip",         test_lab_v_roundtrip},
        {"lch_v_roundtrip",         test_lch_v_roundtrip},
        {"luv_v_roundtrip",         test_luv_v_roundtrip},
        {"ipt_v_roundtrip",         test_ipt_v_roundtrip},
        {"iptch_v_roundtrip",       test_iptch_v_roundtrip},
        {"cmy_v_roundtrip",         test_cmy_v_roundtrip},
        {"ycocg_v_roundtrip",       test_ycocg_v_roundtrip},
        {"hsv_v_roundtrip",         test_hsv_v_roundtrip},
        {"hsl_v_roundtrip",         test_hsl_v_roundtrip},
        {"ycbcr_v_roundtrip",       test_ycbcr_v_roundtrip},
        {"yccbccrc_v_roundtrip",    test_yccbccrc_v_roundtrip},
        {"din99_v_roundtrip",       test_din99_v_roundtrip},
        {"hunter_lab_v_roundtrip",  test_hunter_lab_v_roundtrip},
        {"prolab_v_roundtrip",      test_prolab_v_roundtrip},
        {"prismatic_v_roundtrip",   test_prismatic_v_roundtrip},
        {"hcl_v_roundtrip",         test_hcl_v_roundtrip},
        {"ihls_v_roundtrip",        test_ihls_v_roundtrip},
        {"hdr_cielab_v_roundtrip",  test_hdr_cielab_v_roundtrip},
        {"hdr_ipt_v_roundtrip",     test_hdr_ipt_v_roundtrip},
        {"igpgtg_v_roundtrip",      test_igpgtg_v_roundtrip},
        {"icacb_v_roundtrip",       test_icacb_v_roundtrip},
        {"ucs_v_roundtrip",         test_ucs_v_roundtrip},
        {"uvw_v_roundtrip",         test_uvw_v_roundtrip},
        {"xyz_lch_v_roundtrip",     test_xyz_lch_v_roundtrip},
        {"xyz_lchuv_v_roundtrip",   test_xyz_lchuv_v_roundtrip},
        {"delta_e_76_v",            test_delta_e_76_v},
        {"delta_e_94_v",            test_delta_e_94_v},
        {"delta_e_2000_v",          test_delta_e_2000_v},
        {"delta_e_cmc_v",           test_delta_e_cmc_v},
        {"delta_e_cam02_lcd_v",     test_delta_e_cam02_lcd_v},
        {"delta_e_cam16_ucs_v",     test_delta_e_cam16_ucs_v},
        {"cmyk_v_roundtrip",        test_cmyk_v_roundtrip},
        {"mat3_inv_v",              test_mat3_inv_v},
        {"ictcp_pq_v_roundtrip",    test_ictcp_pq_v_roundtrip},
        {"ictcp_hlg_v_roundtrip",   test_ictcp_hlg_v_roundtrip},
        {"osa_ucs_v_forward",       test_osa_ucs_v_forward},
        {"rlab_v_forward",          test_rlab_v_forward},
        {"atd95_v_forward",         test_atd95_v_forward},
        {"llab_v_forward",          test_llab_v_forward},
        {"cvd_v_identity",          test_cvd_v_identity},
        {"lgg_v_identity",          test_lgg_v_identity},
        {"color_matrix_v_identity", test_color_matrix_v_identity},
        {"cat_v_identity",          test_cat_v_identity},
        {"cct_mccamy_v",            test_cct_mccamy_v},
        {"cct_hernandez_v",         test_cct_hernandez_v},
        {"cct_to_xy_kang_v",        test_cct_to_xy_kang_v},
        {"rayleigh_cross_section_v", test_rayleigh_cross_section_v},
        {"rayleigh_optical_depth_v", test_rayleigh_optical_depth_v},
        {"gamut_clip_v",            test_gamut_clip_v},
        {"gamut_oklab_roundtrip_v", test_gamut_oklab_roundtrip_v},
        {"gamut_find_cusp_v",       test_gamut_find_cusp_v},
        {"aces_tonemap_v",          test_aces_tonemap_v},
        {"agx_curve_v",             test_agx_curve_v},
    };

    test_count = 0;
    test_passed = 0;
    test_failed = 0;

    size_t const n = sizeof(tests) / sizeof(tests[0]);
    int failures = 0;

    for (size_t i = 0; i < n; i++) {
        int result = tests[i].fn();
        if (result != 0) {
            printf("[FAIL] Test '%s' failed\n", tests[i].name);
            failures++;
        } else {
            printf("[PASS] %s\n", tests[i].name);
        }
    }

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed (out of %zu)\n",
           (int)(n - (size_t)failures), failures, n);
    printf("========================================\n");

    return failures > 0 ? 1 : 0;
}
