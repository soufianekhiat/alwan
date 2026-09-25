/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * A measured display, as a model, and the 3D LUT that calibrates it.
 *
 * Put a display on a meter, run a ramp up each channel, and you have what you
 * need to say what that display does with any signal:
 *
 *     XYZ = M [ t_r(d_r), t_g(d_g), t_b(d_b) ]^T + XYZ_black
 *
 * t_c is the channel's tone curve, normalised to run 0 to 1 over the drive
 * range, and M's columns are the three primaries at full drive with the black
 * taken off. The black is the reading at zero drive, carried once.
 *
 * HOW THE TONE CURVE IS FITTED decided whether this model works at all, and
 * the obvious order is the wrong one. Subtracting the black first and fitting
 * a GOG to what is left looks right and is not: a channel whose own curve has
 * a positive offset emits light at zero drive, that emission goes into the
 * shared black along with everything else, and the remainder is
 * (a d + b)^g - b^g, which is not a power law and which no GOG can be. The
 * offset, the O in GOG, becomes unfittable.
 *
 * So each channel is fitted as a GOGO on the RAW luminance ramp instead. A
 * measured ramp IS a GOGO exactly, since a scale on the outside of a power
 * law folds into its gain, so the offset survives and the flare comes back as
 * that channel's own reading of the display's black. The curve is then
 * normalised to 0 at zero drive and 1 at full drive, and the normalised curve
 * is still a GOGO, which is why it stores in the same struct: scaling by s
 * takes (a d + b)^g to (s^(1/g) a d + s^(1/g) b)^g, and the shift lands in the
 * flare, which goes negative. Suite 168 pins a display with an offset on one
 * channel precisely because the wrong order passes every other case.
 *
 * This is the standard model, and it assumes CHANNEL INDEPENDENCE and
 * ADDITIVITY: that each channel's output depends only on its own drive, and
 * that the three add. Real displays deviate, LCDs in particular, and the
 * deviation is not modelled here because it cannot be fitted from ramps. What
 * this code does instead is MEASURE it: alwan_display_model_fit_{T} reports
 * the residual, and a caller with measurements off the ramps can push them
 * through alwan_display_model_forward_{T} and see for themselves. A model that
 * reports how wrong it is beats one that does not say.
 *
 * NOT TESTABLE AGAINST A REFERENCE. colour-science has no display model and no
 * calibration bake. So suite 168 closes the loop instead: it builds a display
 * whose parameters it chose, generates the measurements that display would
 * give, fits the model back, and requires the parameters to return. Then it
 * bakes a LUT and pushes colour through LUT and model together, which has to
 * land where the source colour said. The strongest case needs no tolerance
 * argument at all: calibrating a display that ALREADY IS the source space must
 * produce the identity LUT, and any error in any part of the chain shows up as
 * a departure from it.
 *
 * Published in Berns (1996), "Methods for characterizing CRT displays",
 * Displays 16(4), and the same model underlies every display calibrator; the
 * LUT bake is the ordinary composition of it with a source space.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <string.h>
#include <math.h>

static int alwan_dm_finite(double v) {
    return (v == v) && (v > -1e308) && (v < 1e308);
}

/* ---------------------------------------------------------------- fit */

typedef struct {
    double x, y, z;
} alwan_dm_xyz;

static alwan_status alwan_dm_fit_core(double *tone /* 12 */, double *matrix /* 9 */,
                                      double *black /* 3 */, double *rms_out,
                                      double const *drives,
                                      alwan_dm_xyz const *ramp_r,
                                      alwan_dm_xyz const *ramp_g,
                                      alwan_dm_xyz const *ramp_b,
                                      size_t count) {
    alwan_dm_xyz const *ramps[3];
    double *lum = NULL;
    double worst = 0.0;
    size_t i, c;
    alwan_status st = ALWAN_OK;

    ramps[0] = ramp_r;
    ramps[1] = ramp_g;
    ramps[2] = ramp_b;

    /* Five, not four: each channel's curve is fitted as a GOGO, and a GOGO
     * needs five points. */
    if (count < 5) return ALWAN_E_RANGE;
    /* The ramps have to start at black, because the black reading is what
     * everything else is measured against. */
    if (drives[0] != 0.0) return ALWAN_E_RANGE;
    for (i = 0; i < count; i++) {
        if (!alwan_dm_finite(drives[i])) return ALWAN_E_INVALID;
        for (c = 0; c < 3; c++) {
            if (!alwan_dm_finite(ramps[c][i].x) || !alwan_dm_finite(ramps[c][i].y)
                || !alwan_dm_finite(ramps[c][i].z)) {
                return ALWAN_E_INVALID;
            }
        }
    }
    /* The three ramps read the same display at drive zero, so their three
     * black readings are three measurements of one quantity. Averaging them is
     * the only use the duplication has. */
    black[0] = (ramps[0][0].x + ramps[1][0].x + ramps[2][0].x) / 3.0;
    black[1] = (ramps[0][0].y + ramps[1][0].y + ramps[2][0].y) / 3.0;
    black[2] = (ramps[0][0].z + ramps[1][0].z + ramps[2][0].z) / 3.0;

    lum = (double *)ALWAN_ALLOC(alwan_safe_array_size(count, sizeof(double)), sizeof(double));
    if (!lum) return ALWAN_E_NOMEM;

    for (c = 0; c < 3; c++) {
        alwan_display_gog_f64 fit;
        alwan_f64 f0 = 0.0, f1 = 0.0;
        double rms = 0.0, s, k;
        /* The primary at full drive, which is this column of M. */
        double const px = ramps[c][count - 1].x - black[0];
        double const py = ramps[c][count - 1].y - black[1];
        double const pz = ramps[c][count - 1].z - black[2];
        matrix[0 * 3 + c] = px;
        matrix[1 * 3 + c] = py;
        matrix[2 * 3 + c] = pz;
        if (!(py > 0.0)) { st = ALWAN_E_RANGE; break; }

        /* The tone curve is fitted to the RAW luminance ramp, with the flare
         * term, rather than to a black-subtracted one. That is not a detail.
         * Subtract the shared black first and a channel whose own curve has a
         * positive offset stops being a power law at all: its toe emission at
         * zero drive goes into the black, and what is left is
         * (a d + b)^g - b^g, which no GOG can be. The raw ramp, on the other
         * hand, IS a GOGO exactly, so the offset survives and the flare comes
         * back as this channel's reading of the display's black. */
        for (i = 0; i < count; i++) lum[i] = ramps[c][i].y;
        st = alwan_display_gog_fit_f64(&fit, &rms, drives, lum, count, 1);
        if (st != ALWAN_OK) break;

        /* Normalise the curve to run 0 to 1 over the drive range, so the scale
         * lives in M and not split between the two. The normalised curve is
         * itself a GOGO, which is why it stores in the same struct: scaling by
         * s takes (a d + b)^g to (s^(1/g) a d + s^(1/g) b)^g, and the shift
         * lands in the flare. */
        alwan_display_gog_eval_f64(&f0, &fit, 0.0);
        alwan_display_gog_eval_f64(&f1, &fit, 1.0);
        if (!((double)f1 > (double)f0)) { st = ALWAN_E_RANGE; break; }
        s = 1.0 / ((double)f1 - (double)f0);
        k = ALWAN_POW_F64(s, 1.0 / (double)fit.gamma);
        tone[c * 4 + 0] = (double)fit.gain * k;
        tone[c * 4 + 1] = (double)fit.offset * k;
        tone[c * 4 + 2] = (double)fit.gamma;
        tone[c * 4 + 3] = s * ((double)fit.flare - (double)f0);
        if (rms > worst) worst = rms;
    }
    ALWAN_FREE(lum);
    if (st != ALWAN_OK) return st;
    if (rms_out) *rms_out = worst;
    return ALWAN_OK;
}

/* ---------------------------------------------------------------- forward */

static void alwan_dm_forward(double *xyz, double const *tone, double const *matrix,
                             double const *black, double const *drive) {
    double lin[3];
    int c, r;
    for (c = 0; c < 3; c++) {
        alwan_display_gog_f64 g;
        alwan_f64 v = 0.0;
        g.gain = tone[c * 4 + 0];
        g.offset = tone[c * 4 + 1];
        g.gamma = tone[c * 4 + 2];
        g.flare = tone[c * 4 + 3];
        alwan_display_gog_eval_f64(&v, &g, (alwan_f64)drive[c]);
        lin[c] = (double)v;
    }
    for (r = 0; r < 3; r++) {
        xyz[r] = black[r];
        for (c = 0; c < 3; c++) xyz[r] += matrix[r * 3 + c] * lin[c];
    }
}

/* ---------------------------------------------------------------- inverse */

/* The drive that produces an XYZ, and how far outside the display's gamut the
 * request was. The excursion is reported rather than swallowed: the drive
 * comes back clamped, because a display cannot emit what it cannot emit, and
 * the caller is told how much was asked for beyond that. */
static alwan_status alwan_dm_invert(double *drive, double *excursion,
                                    double const *tone, double const *matrix_inv,
                                    double const *black, double const *xyz) {
    double rel[3], lin[3];
    double worst = 0.0;
    int c, r;

    for (r = 0; r < 3; r++) {
        if (!alwan_dm_finite(xyz[r])) return ALWAN_E_INVALID;
        rel[r] = xyz[r] - black[r];
    }
    for (r = 0; r < 3; r++) {
        lin[r] = 0.0;
        for (c = 0; c < 3; c++) lin[r] += matrix_inv[r * 3 + c] * rel[c];
    }
    for (c = 0; c < 3; c++) {
        alwan_display_gog_f64 g;
        alwan_f64 v = 0.0;
        double over = 0.0;
        if (lin[c] < 0.0) over = -lin[c];
        else if (lin[c] > 1.0) over = lin[c] - 1.0;
        if (over > worst) worst = over;
        if (lin[c] < 0.0) lin[c] = 0.0;
        if (lin[c] > 1.0) lin[c] = 1.0;

        g.gain = tone[c * 4 + 0];
        g.offset = tone[c * 4 + 1];
        g.gamma = tone[c * 4 + 2];
        g.flare = tone[c * 4 + 3];
        if (alwan_display_gog_invert_f64(&v, &g, (alwan_f64)lin[c]) != ALWAN_OK) {
            /* Below the curve's own floor: the darkest this channel reaches. */
            v = 0.0;
        }
        drive[c] = (double)v;
        /* The curve is normalised to 0 at zero drive, so a request of zero or
         * less is zero drive, and saying so is not a rounding convenience. The
         * fit leaves the normalised offset a hair either side of zero, because
         * the residual barely notices it there, and a hair NEGATIVE makes the
         * curve flat over a sliver of drive above zero. Over a flat stretch one
         * preimage is as good as another, so the inverse returns the smallest,
         * which keeps it an exact inverse of the forward at zero rather than
         * off by offset/gain. The sliver is about 1e-07 of drive wide, well
         * under any quantisation a display can send. */
        if (lin[c] <= 0.0) drive[c] = 0.0;
        if (drive[c] < 0.0) drive[c] = 0.0;
        if (drive[c] > 1.0) drive[c] = 1.0;
    }
    if (excursion) *excursion = worst;
    return ALWAN_OK;
}

/* ---------------------------------------------------------------- entry */

/* The parameter names carry a trailing underscore on purpose: a macro
 * parameter called m would be substituted into rgb_to_xyz.m as well. */
#define ALWAN_DM_PACK(mdl_, tone_, mat_, blk_)                                 \
    do {                                                                       \
        int _c;                                                                \
        for (_c = 0; _c < 3; _c++) {                                           \
            (tone_)[_c * 4 + 0] = (double)(mdl_)->tone[_c].gain;               \
            (tone_)[_c * 4 + 1] = (double)(mdl_)->tone[_c].offset;             \
            (tone_)[_c * 4 + 2] = (double)(mdl_)->tone[_c].gamma;              \
            (tone_)[_c * 4 + 3] = (double)(mdl_)->tone[_c].flare;              \
        }                                                                      \
        for (_c = 0; _c < 9; _c++) (mat_)[_c] = (double)(mdl_)->rgb_to_xyz.m[_c]; \
        (blk_)[0] = (double)(mdl_)->black.x;                                   \
        (blk_)[1] = (double)(mdl_)->black.y;                                   \
        (blk_)[2] = (double)(mdl_)->black.z;                                   \
    } while (0)

alwan_status alwan_display_model_fit_f64(alwan_display_model_f64 *out, alwan_f64 *rms_out,
                                         alwan_f64 const *drives,
                                         alwan_xyz_f64 const *ramp_red,
                                         alwan_xyz_f64 const *ramp_green,
                                         alwan_xyz_f64 const *ramp_blue,
                                         size_t count) {
    double tone[12], matrix[9], black[3], rms = 0.0;
    alwan_status st;
    int i;
    if (!out || !drives || !ramp_red || !ramp_green || !ramp_blue) return ALWAN_E_INVALID;
    st = alwan_dm_fit_core(tone, matrix, black, &rms, drives,
                           (alwan_dm_xyz const *)ramp_red,
                           (alwan_dm_xyz const *)ramp_green,
                           (alwan_dm_xyz const *)ramp_blue, count);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < 3; i++) {
        out->tone[i].gain = (alwan_f64)tone[i * 4 + 0];
        out->tone[i].offset = (alwan_f64)tone[i * 4 + 1];
        out->tone[i].gamma = (alwan_f64)tone[i * 4 + 2];
        out->tone[i].flare = (alwan_f64)tone[i * 4 + 3];
    }
    for (i = 0; i < 9; i++) out->rgb_to_xyz.m[i] = (alwan_f64)matrix[i];
    out->black.x = (alwan_f64)black[0];
    out->black.y = (alwan_f64)black[1];
    out->black.z = (alwan_f64)black[2];
    if (rms_out) *rms_out = (alwan_f64)rms;
    return ALWAN_OK;
}

alwan_status alwan_display_model_fit_f32(alwan_display_model_f32 *out, alwan_f32 *rms_out,
                                         alwan_f32 const *drives,
                                         alwan_xyz_f32 const *ramp_red,
                                         alwan_xyz_f32 const *ramp_green,
                                         alwan_xyz_f32 const *ramp_blue,
                                         size_t count) {
    double tone[12], matrix[9], black[3], rms = 0.0;
    double *d = NULL;
    alwan_dm_xyz *r = NULL, *g = NULL, *b = NULL;
    alwan_status st;
    size_t i;
    int k;
    if (!out || !drives || !ramp_red || !ramp_green || !ramp_blue) return ALWAN_E_INVALID;
    if (count < 5) return ALWAN_E_RANGE;
    d = (double *)ALWAN_ALLOC(alwan_safe_array_size(count, sizeof(double)), sizeof(double));
    r = (alwan_dm_xyz *)ALWAN_ALLOC(alwan_safe_array_size(count, sizeof(alwan_dm_xyz)),
                                    sizeof(alwan_dm_xyz));
    g = (alwan_dm_xyz *)ALWAN_ALLOC(alwan_safe_array_size(count, sizeof(alwan_dm_xyz)),
                                    sizeof(alwan_dm_xyz));
    b = (alwan_dm_xyz *)ALWAN_ALLOC(alwan_safe_array_size(count, sizeof(alwan_dm_xyz)),
                                    sizeof(alwan_dm_xyz));
    if (!d || !r || !g || !b) {
        ALWAN_FREE(d); ALWAN_FREE(r); ALWAN_FREE(g); ALWAN_FREE(b);
        return ALWAN_E_NOMEM;
    }
    for (i = 0; i < count; i++) {
        d[i] = (double)drives[i];
        r[i].x = (double)ramp_red[i].x;   r[i].y = (double)ramp_red[i].y;   r[i].z = (double)ramp_red[i].z;
        g[i].x = (double)ramp_green[i].x; g[i].y = (double)ramp_green[i].y; g[i].z = (double)ramp_green[i].z;
        b[i].x = (double)ramp_blue[i].x;  b[i].y = (double)ramp_blue[i].y;  b[i].z = (double)ramp_blue[i].z;
    }
    st = alwan_dm_fit_core(tone, matrix, black, &rms, d, r, g, b, count);
    ALWAN_FREE(d); ALWAN_FREE(r); ALWAN_FREE(g); ALWAN_FREE(b);
    if (st != ALWAN_OK) return st;
    for (k = 0; k < 3; k++) {
        out->tone[k].gain = (alwan_f32)tone[k * 4 + 0];
        out->tone[k].offset = (alwan_f32)tone[k * 4 + 1];
        out->tone[k].gamma = (alwan_f32)tone[k * 4 + 2];
        out->tone[k].flare = (alwan_f32)tone[k * 4 + 3];
    }
    for (k = 0; k < 9; k++) out->rgb_to_xyz.m[k] = (alwan_f32)matrix[k];
    out->black.x = (alwan_f32)black[0];
    out->black.y = (alwan_f32)black[1];
    out->black.z = (alwan_f32)black[2];
    if (rms_out) *rms_out = (alwan_f32)rms;
    return ALWAN_OK;
}

alwan_status alwan_display_model_forward_f64(alwan_xyz_f64 *xyz_out,
                                             alwan_display_model_f64 const *model,
                                             alwan_rgb_f64 const *drive) {
    double tone[12], matrix[9], black[3], xyz[3], dv[3];
    if (!xyz_out || !model || !drive) return ALWAN_E_INVALID;
    ALWAN_DM_PACK(model, tone, matrix, black);
    dv[0] = (double)drive->r; dv[1] = (double)drive->g; dv[2] = (double)drive->b;
    alwan_dm_forward(xyz, tone, matrix, black, dv);
    xyz_out->x = (alwan_f64)xyz[0];
    xyz_out->y = (alwan_f64)xyz[1];
    xyz_out->z = (alwan_f64)xyz[2];
    return ALWAN_OK;
}

alwan_status alwan_display_model_forward_f32(alwan_xyz_f32 *xyz_out,
                                             alwan_display_model_f32 const *model,
                                             alwan_rgb_f32 const *drive) {
    double tone[12], matrix[9], black[3], xyz[3], dv[3];
    if (!xyz_out || !model || !drive) return ALWAN_E_INVALID;
    ALWAN_DM_PACK(model, tone, matrix, black);
    dv[0] = (double)drive->r; dv[1] = (double)drive->g; dv[2] = (double)drive->b;
    alwan_dm_forward(xyz, tone, matrix, black, dv);
    xyz_out->x = (alwan_f32)xyz[0];
    xyz_out->y = (alwan_f32)xyz[1];
    xyz_out->z = (alwan_f32)xyz[2];
    return ALWAN_OK;
}

alwan_status alwan_display_model_invert_f64(alwan_rgb_f64 *drive_out, alwan_f64 *excursion_out,
                                            alwan_display_model_f64 const *model,
                                            alwan_xyz_f64 const *xyz) {
    double tone[12], matrix[9], black[3], inv[9], drive[3], xv[3], ex = 0.0;
    alwan_mat3x3_f64 m, mi;
    alwan_status st;
    int i;
    if (!drive_out || !model || !xyz) return ALWAN_E_INVALID;
    ALWAN_DM_PACK(model, tone, matrix, black);
    for (i = 0; i < 9; i++) m.m[i] = (alwan_f64)matrix[i];
    st = alwan_mat3_inv_f64(&mi, &m);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < 9; i++) inv[i] = (double)mi.m[i];
    xv[0] = (double)xyz->x; xv[1] = (double)xyz->y; xv[2] = (double)xyz->z;
    st = alwan_dm_invert(drive, &ex, tone, inv, black, xv);
    if (st != ALWAN_OK) return st;
    drive_out->r = (alwan_f64)drive[0];
    drive_out->g = (alwan_f64)drive[1];
    drive_out->b = (alwan_f64)drive[2];
    if (excursion_out) *excursion_out = (alwan_f64)ex;
    return ALWAN_OK;
}

alwan_status alwan_display_model_invert_f32(alwan_rgb_f32 *drive_out, alwan_f32 *excursion_out,
                                            alwan_display_model_f32 const *model,
                                            alwan_xyz_f32 const *xyz) {
    double tone[12], matrix[9], black[3], inv[9], drive[3], xv[3], ex = 0.0;
    alwan_mat3x3_f64 m, mi;
    alwan_status st;
    int i;
    if (!drive_out || !model || !xyz) return ALWAN_E_INVALID;
    ALWAN_DM_PACK(model, tone, matrix, black);
    for (i = 0; i < 9; i++) m.m[i] = (alwan_f64)matrix[i];
    st = alwan_mat3_inv_f64(&mi, &m);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < 9; i++) inv[i] = (double)mi.m[i];
    xv[0] = (double)xyz->x; xv[1] = (double)xyz->y; xv[2] = (double)xyz->z;
    st = alwan_dm_invert(drive, &ex, tone, inv, black, xv);
    if (st != ALWAN_OK) return st;
    drive_out->r = (alwan_f32)drive[0];
    drive_out->g = (alwan_f32)drive[1];
    drive_out->b = (alwan_f32)drive[2];
    if (excursion_out) *excursion_out = (alwan_f32)ex;
    return ALWAN_OK;
}

/* ---------------------------------------------------------------- the bake */

/* The white the display reaches at full drive, which is what a source white
 * has to be adapted to. */
static void alwan_dm_white(double *white, double const *tone, double const *matrix,
                           double const *black) {
    double const one[3] = { 1.0, 1.0, 1.0 };
    alwan_dm_forward(white, tone, matrix, black, one);
}

static alwan_status alwan_dm_bake(double *lut, int size,
                                  double const *tone, double const *matrix,
                                  double const *black,
                                  alwan_rgb_space_desc_f64 const *source,
                                  alwan_transfer_function source_eotf,
                                  alwan_cat_method cat,
                                  double *worst_excursion) {
    double inv[9], adapt[9], src_to_xyz[9];
    alwan_mat3x3_f64 m, mi;
    alwan_status st;
    double worst = 0.0;
    int i, j, k, n;

    if (size < 2 || size > 256) return ALWAN_E_RANGE;

    for (i = 0; i < 9; i++) m.m[i] = (alwan_f64)matrix[i];
    st = alwan_mat3_inv_f64(&mi, &m);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < 9; i++) inv[i] = (double)mi.m[i];

    {
        alwan_mat3x3_f64 s2x, x2s;
        st = alwan_rgb_derive_matrices_f64(&s2x, &x2s, source);
        if (st != ALWAN_OK) return st;
        for (i = 0; i < 9; i++) src_to_xyz[i] = (double)s2x.m[i];
    }

    /* The source white and the display's measured white are almost never the
     * same, so the source has to be adapted onto the display rather than
     * silently landing somewhere between. */
    {
        double white[3];
        alwan_xyz_f64 sw, dw;
        alwan_mat3x3_f64 cm;
        double const denom = (double)source->white_xy[1];
        if (denom == 0.0) return ALWAN_E_RANGE;
        sw.x = (alwan_f64)((double)source->white_xy[0] / denom);
        sw.y = (alwan_f64)1.0;
        sw.z = (alwan_f64)((1.0 - (double)source->white_xy[0] - denom) / denom);
        alwan_dm_white(white, tone, matrix, black);
        if (!(white[1] > 0.0)) return ALWAN_E_RANGE;
        dw.x = (alwan_f64)(white[0] / white[1]);
        dw.y = (alwan_f64)1.0;
        dw.z = (alwan_f64)(white[2] / white[1]);
        st = alwan_cat_matrix_f64(&cm, &sw, &dw, cat);
        if (st != ALWAN_OK) return st;
        for (i = 0; i < 9; i++) adapt[i] = (double)cm.m[i];
    }

    n = 0;
    /* R fastest, which is alwan's cube order throughout. */
    for (k = 0; k < size; k++) {
        for (j = 0; j < size; j++) {
            for (i = 0; i < size; i++) {
                double enc[3], lin[3], xyz[3], scaled[3], drive[3], ex = 0.0;
                int r, c;
                enc[0] = (double)i / (double)(size - 1);
                enc[1] = (double)j / (double)(size - 1);
                enc[2] = (double)k / (double)(size - 1);
                for (c = 0; c < 3; c++) {
                    alwan_f64 v = (alwan_f64)enc[c];
                    alwan_f64 o = 0.0;
                    st = alwan_eotf_apply_f64(&o, sizeof(alwan_f64), &v, sizeof(alwan_f64),
                                              1, source_eotf);
                    if (st != ALWAN_OK) return st;
                    lin[c] = (double)o;
                }
                for (r = 0; r < 3; r++) {
                    xyz[r] = 0.0;
                    for (c = 0; c < 3; c++) xyz[r] += src_to_xyz[r * 3 + c] * lin[c];
                }
                for (r = 0; r < 3; r++) {
                    scaled[r] = 0.0;
                    for (c = 0; c < 3; c++) scaled[r] += adapt[r * 3 + c] * xyz[c];
                }
                /* The source is normalised to its own white at Y = 1; the
                 * display reaches whatever it reaches. Putting the source's
                 * white on the display's white is what makes the two
                 * comparable, and it is why the adapted XYZ is scaled by the
                 * display's peak before inversion. */
                {
                    double white[3];
                    alwan_dm_white(white, tone, matrix, black);
                    for (r = 0; r < 3; r++) scaled[r] *= white[1];
                }
                st = alwan_dm_invert(drive, &ex, tone, inv, black, scaled);
                if (st != ALWAN_OK) return st;
                if (ex > worst) worst = ex;
                lut[n * 3 + 0] = drive[0];
                lut[n * 3 + 1] = drive[1];
                lut[n * 3 + 2] = drive[2];
                n++;
            }
        }
    }
    if (worst_excursion) *worst_excursion = worst;
    return ALWAN_OK;
}

alwan_status alwan_display_calibration_lut_f64(alwan_f64 *lut_out, int size,
                                               alwan_display_model_f64 const *model,
                                               alwan_rgb_space_desc_f64 const *source,
                                               alwan_transfer_function source_eotf,
                                               alwan_cat_method cat,
                                               alwan_f64 *worst_excursion) {
    double tone[12], matrix[9], black[3], worst = 0.0;
    double *lut;
    alwan_status st;
    size_t total, i;
    if (!lut_out || !model || !source) return ALWAN_E_INVALID;
    if (size < 2 || size > 256) return ALWAN_E_RANGE;
    ALWAN_DM_PACK(model, tone, matrix, black);
    total = (size_t)size * (size_t)size * (size_t)size * 3u;
    lut = (double *)ALWAN_ALLOC(alwan_safe_array_size(total, sizeof(double)), sizeof(double));
    if (!lut) return ALWAN_E_NOMEM;
    st = alwan_dm_bake(lut, size, tone, matrix, black, source, source_eotf, cat, &worst);
    if (st == ALWAN_OK) {
        for (i = 0; i < total; i++) lut_out[i] = (alwan_f64)lut[i];
        if (worst_excursion) *worst_excursion = (alwan_f64)worst;
    }
    ALWAN_FREE(lut);
    return st;
}

alwan_status alwan_display_calibration_lut_f32(alwan_f32 *lut_out, int size,
                                               alwan_display_model_f32 const *model,
                                               alwan_rgb_space_desc_f32 const *source,
                                               alwan_transfer_function source_eotf,
                                               alwan_cat_method cat,
                                               alwan_f32 *worst_excursion) {
    double tone[12], matrix[9], black[3], worst = 0.0;
    double *lut;
    alwan_rgb_space_desc_f64 src;
    alwan_status st;
    size_t total, i;
    if (!lut_out || !model || !source) return ALWAN_E_INVALID;
    if (size < 2 || size > 256) return ALWAN_E_RANGE;
    ALWAN_DM_PACK(model, tone, matrix, black);

    memset(&src, 0, sizeof(src));
    for (i = 0; i < 6; i++) src.primaries_xy[i] = (alwan_f64)source->primaries_xy[i];
    src.white_xy[0] = (alwan_f64)source->white_xy[0];
    src.white_xy[1] = (alwan_f64)source->white_xy[1];
    src.oetf = source->oetf;
    src.eotf = source->eotf;
    src.has_matrices = 0;

    total = (size_t)size * (size_t)size * (size_t)size * 3u;
    lut = (double *)ALWAN_ALLOC(alwan_safe_array_size(total, sizeof(double)), sizeof(double));
    if (!lut) return ALWAN_E_NOMEM;
    st = alwan_dm_bake(lut, size, tone, matrix, black, &src, source_eotf, cat, &worst);
    if (st == ALWAN_OK) {
        for (i = 0; i < total; i++) lut_out[i] = (alwan_f32)lut[i];
        if (worst_excursion) *worst_excursion = (alwan_f32)worst;
    }
    ALWAN_FREE(lut);
    return st;
}
