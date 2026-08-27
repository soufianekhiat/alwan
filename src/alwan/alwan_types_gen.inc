/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Parameterized type definitions - included twice (f32 and f64)
 * No include guard.
 *
 * Expects before inclusion:
 *   ALWAN_T       -- float or double
 *   ALWAN_SUFFIX  -- _f32 or _f64
 */

/* Token-pasting helpers (safe to redefine identically on second pass) */
#define ALWAN_TN2_(base, sfx) base##sfx
#define ALWAN_TN_(base, sfx)  ALWAN_TN2_(base, sfx)
#define ALWAN_TN(base)        ALWAN_TN_(base, ALWAN_SUFFIX)

/* ----------------------------------------------------------------
 * Math Types
 * ---------------------------------------------------------------- */

typedef struct { ALWAN_T v[2]; }  ALWAN_TN(alwan_vec2);
typedef struct { ALWAN_T v[3]; }  ALWAN_TN(alwan_vec3);
typedef struct { ALWAN_T m[9]; }  ALWAN_TN(alwan_mat3x3);
typedef struct { ALWAN_T m[16]; } ALWAN_TN(alwan_mat4x4);

/* ----------------------------------------------------------------
 * Semantic Color Types (layout-compatible with suffixed alwan_vec3)
 * ---------------------------------------------------------------- */

typedef struct { ALWAN_T r, g, b; }     ALWAN_TN(alwan_rgb);
typedef struct { ALWAN_T c, m, y, k; }  ALWAN_TN(alwan_cmyk);
typedef struct { ALWAN_T c, m, y; }     ALWAN_TN(alwan_cmy);
typedef struct { ALWAN_T h, s, v; }     ALWAN_TN(alwan_hsv);
typedef struct { ALWAN_T h, s, l; }     ALWAN_TN(alwan_hsl);
typedef struct { ALWAN_T h, s, p; }     ALWAN_TN(alwan_hsp);
typedef struct { ALWAN_T h, s, p; }     ALWAN_TN(alwan_hsplog);
typedef struct { ALWAN_T h, s, y; }     ALWAN_TN(alwan_hsy);
typedef struct { ALWAN_T h, w, b; }     ALWAN_TN(alwan_hwb);
typedef struct { ALWAN_T x, y, z; }     ALWAN_TN(alwan_xyz);
typedef struct { ALWAN_T x, y, Y; }     ALWAN_TN(alwan_xyy);
typedef struct { ALWAN_T L, a, b; }     ALWAN_TN(alwan_lab);
typedef struct { ALWAN_T L, u, v; }     ALWAN_TN(alwan_luv);
typedef struct { ALWAN_T L, C, h; }     ALWAN_TN(alwan_lch);
typedef struct { ALWAN_T L, C, h; }     ALWAN_TN(alwan_lchuv);
typedef struct { ALWAN_T L, a, b; }     ALWAN_TN(alwan_oklab);
typedef struct { ALWAN_T L, C, h; }     ALWAN_TN(alwan_oklch);
typedef struct { ALWAN_T Jz, az, bz; }  ALWAN_TN(alwan_jzazbz);
typedef struct { ALWAN_T Jz, Cz, hz; }  ALWAN_TN(alwan_jzczhz);
typedef struct { ALWAN_T I, Ct, Cp; }   ALWAN_TN(alwan_ictcp);
typedef struct { ALWAN_T I, P, T; }     ALWAN_TN(alwan_ipt);
typedef struct { ALWAN_T Ig, Pg, Tg; }  ALWAN_TN(alwan_igpgtg);
typedef struct { ALWAN_T I, Ca, Cb; }   ALWAN_TN(alwan_icacb);
typedef struct { ALWAN_T Y, Cb, Cr; }   ALWAN_TN(alwan_ycbcr);
typedef struct { ALWAN_T Y, Co, Cg; }   ALWAN_TN(alwan_ycocg);
typedef struct { ALWAN_T Yc, Cbc, Crc; } ALWAN_TN(alwan_yccbccrc);
typedef struct { ALWAN_T U, V, W; }     ALWAN_TN(alwan_uvw);
typedef struct { ALWAN_T L99, a99, b99; } ALWAN_TN(alwan_din99);
typedef struct { ALWAN_T L, a, b; }     ALWAN_TN(alwan_hunter_lab);
typedef struct { ALWAN_T I, C, h; }     ALWAN_TN(alwan_iptch);
typedef struct { ALWAN_T L, a, b; }     ALWAN_TN(alwan_prolab);
typedef struct { ALWAN_T L, j, g; }     ALWAN_TN(alwan_osa_ucs);
typedef struct { ALWAN_T U, V, W; }     ALWAN_TN(alwan_ucs);
typedef struct { ALWAN_T L, s, h; }     ALWAN_TN(alwan_prismatic);
typedef struct { ALWAN_T H, C, L; }     ALWAN_TN(alwan_hcl);
typedef struct { ALWAN_T H, L, S; }     ALWAN_TN(alwan_ihls);
typedef struct { ALWAN_T J, a, b; }     ALWAN_TN(alwan_cam_jab);
typedef struct { ALWAN_T h, s, l; }     ALWAN_TN(alwan_hsluv);
typedef struct { ALWAN_T h, s, l; }     ALWAN_TN(alwan_hpluv);
typedef struct { ALWAN_T h, s, l; }     ALWAN_TN(alwan_okhsl);
typedef struct { ALWAN_T h, s, v; }     ALWAN_TN(alwan_okhsv);
typedef struct { ALWAN_T h, s, l; }     ALWAN_TN(alwan_cubehelix);
typedef struct { ALWAN_T H, L, C; }     ALWAN_TN(alwan_hlc);

/* ST.2086 Mastering Display Color Volume Metadata */
typedef struct {
    ALWAN_T display_primaries_xy[6]; /* rx, ry, gx, gy, bx, by */
    ALWAN_T white_point_xy[2];       /* wx, wy */
    ALWAN_T max_luminance;           /* cd/m2 (nits) */
    ALWAN_T min_luminance;           /* cd/m2 (nits) */
} ALWAN_TN(alwan_st2086_metadata);

/* Content Light Level Info */
typedef struct {
    ALWAN_T max_cll;
    ALWAN_T max_fall;
} ALWAN_TN(alwan_content_light_level);

/* ----------------------------------------------------------------
 * Parameter Structs
 * ---------------------------------------------------------------- */

/* Delta E CMC(l:c) parameters */
typedef struct { ALWAN_T l; ALWAN_T c; } ALWAN_TN(alwan_delta_e_cmc_params);

/* Delta E ITP parameters */
typedef struct { ALWAN_T scalar_factor; } ALWAN_TN(alwan_delta_e_itp_params);

/* Full Barten (1999) CSF model parameters */
typedef struct {
    ALWAN_T sigma;    /* Line-spread function std dev (degrees) */
    ALWAN_T k;        /* Signal-to-noise ratio (default: 3.0) */
    ALWAN_T T;        /* Integration time in seconds (default: 0.1) */
    ALWAN_T X_0;      /* Angular size x in degrees (default: 60) */
    ALWAN_T Y_0;      /* Angular size y in degrees (-1 = use X_0) */
    ALWAN_T X_max;    /* Max integration area x in degrees (default: 12) */
    ALWAN_T Y_max;    /* Max integration area y in degrees (-1 = use X_max) */
    ALWAN_T N_max;    /* Max integration cycles (default: 15) */
    ALWAN_T n;        /* Quantum efficiency (default: 0.03) */
    ALWAN_T p;        /* Photon conversion factor (default: 1.2274e6) */
    ALWAN_T E;        /* Retinal illuminance in Trolands */
    ALWAN_T phi_0;    /* Neural noise spectral density (default: 3e-8) */
    ALWAN_T u_0;      /* Lateral inhibition cutoff frequency (default: 7) */
} ALWAN_TN(alwan_csf_barten1999_params);

/* Atmospheric parameters for Rayleigh calculations */
typedef struct {
    ALWAN_T CO2_concentration;  /* CO2 concentration in ppm (default: 300) */
    ALWAN_T temperature;        /* Temperature in Kelvin (default: 288.15) */
    ALWAN_T pressure;           /* Pressure in Pascals (default: 101325) */
    ALWAN_T latitude;           /* Latitude in degrees (default: 0) */
    ALWAN_T altitude;           /* Altitude in meters (default: 0) */
} ALWAN_TN(alwan_atmosphere_params);

/* ----------------------------------------------------------------
 * SPD Structs
 * ---------------------------------------------------------------- */

/* Spectral Power Distribution (SPD) - uniformly sampled spectrum */
typedef struct {
    ALWAN_T *values;        /* SPD values (power/reflectance/transmittance) */
    ALWAN_T wavelength_min; /* Starting wavelength (nm) */
    ALWAN_T wavelength_max; /* Ending wavelength (nm) */
    size_t count;           /* Number of samples */
} ALWAN_TN(alwan_spd);

/* Spectral shape descriptor */
typedef struct {
    ALWAN_T peak_wavelength;  /* Peak wavelength (nm) */
    ALWAN_T peak_value;       /* Peak power/reflectance value */
    ALWAN_T fwhm;             /* Full width at half maximum (nm) */
    ALWAN_T centroid;         /* Weighted mean wavelength (nm) */
    ALWAN_T bandwidth;        /* Total wavelength range (nm) */
} ALWAN_TN(alwan_spd_shape);

/* ----------------------------------------------------------------
 * CAM Viewing Conditions
 * ---------------------------------------------------------------- */

/* CIECAM02 viewing conditions */
typedef struct {
    ALWAN_TN(alwan_xyz) white_xyz;             /* Reference white in XYZ (Y typically 100) */
    ALWAN_T adapting_luminance;                /* Luminance of adapting field (La) in cd/m^2 */
    ALWAN_T background_luminance;              /* Relative luminance of background (Yb/Yw) */
    alwan_ciecam02_surround surround;          /* Viewing surround condition */
    int discount_illuminant;                   /* 1 to discount illuminant, 0 otherwise */
} ALWAN_TN(alwan_ciecam02_viewing_conditions);

/* CAM16 viewing conditions */
typedef struct {
    ALWAN_TN(alwan_xyz) white_xyz;             /* Reference white in XYZ (Y typically 100) */
    ALWAN_T adapting_luminance;                /* Luminance of adapting field (La) in cd/m^2 */
    ALWAN_T background_luminance;              /* Relative luminance of background (Yb/Yw) */
    alwan_cam16_surround surround;             /* Viewing surround condition */
    int discount_illuminant;                   /* 1 to discount illuminant, 0 otherwise */
} ALWAN_TN(alwan_cam16_viewing_conditions);

/* ZCAM viewing conditions */
typedef struct {
    ALWAN_TN(alwan_xyz) xyz_w;                 /* White point (absolute XYZ in cd/m^2) */
    ALWAN_T La;                                /* Adapting luminance (cd/m^2) */
    ALWAN_T Yb;                                /* Background luminance factor */
    alwan_zcam_surround surround;              /* Viewing surround condition */
    int discount_illuminant;                   /* 1 to discount illuminant, 0 otherwise */
} ALWAN_TN(alwan_zcam_viewing_conditions);

/* RLAB viewing conditions */
typedef struct {
    ALWAN_TN(alwan_xyz) xyz_w;                 /* White point (XYZ, Y=100) */
    ALWAN_TN(alwan_xyz) xyz_n;                 /* Reference white (usually D65) */
    ALWAN_T Y_n;                               /* Absolute adapting luminance, cd/m^2.
                                                * RLAB's incomplete-adaptation term depends on
                                                * this whenever D < 1. Pass 318.31 for the
                                                * reference viewing condition; 0 is treated as
                                                * 318.31 so a zero-initialised struct behaves. */
    alwan_rlab_surround surround;              /* Viewing surround */
    int D_factor;                              /* Discounting factor: 0=auto, 1=hard, 2=soft, 3=trans */
} ALWAN_TN(alwan_rlab_viewing_conditions);

/* Hunt viewing conditions */
typedef struct {
    ALWAN_TN(alwan_xyz) xyz_w;                 /* White point (XYZ, Y=100) */
    ALWAN_T La;                                /* Adapting luminance (cd/m^2) */
    ALWAN_T Yb;                                /* Background luminance factor */
    alwan_hunt_surround surround;              /* Viewing surround */
    int discount_illuminant;                   /* 1 to discount illuminant, 0 otherwise */
} ALWAN_TN(alwan_hunt_viewing_conditions);

/* Hellwig2022 viewing conditions */
typedef struct {
    ALWAN_TN(alwan_xyz) white_xyz;             /* Reference white in XYZ (Y typically 100) */
    ALWAN_T adapting_luminance;                /* Luminance of adapting field (La) in cd/m^2 */
    ALWAN_T background_luminance;              /* Relative luminance of background (Yb/Yw) */
    alwan_hellwig2022_surround surround;       /* Viewing surround condition */
    int discount_illuminant;                   /* 1 to discount illuminant, 0 otherwise */
} ALWAN_TN(alwan_hellwig2022_viewing_conditions);

/* Kim2009 viewing conditions */
typedef struct {
    ALWAN_TN(alwan_xyz) white_xyz;             /* Reference white in XYZ (Y typically 100) */
    ALWAN_T La;                                /* Adapting luminance (cd/m^2) */
    ALWAN_T Yb;                                /* Background luminance factor */
    int discount_illuminant;                   /* 1 to discount illuminant, 0 otherwise */
} ALWAN_TN(alwan_kim2009_viewing_conditions);

/* LLAB viewing conditions */
typedef struct {
    ALWAN_TN(alwan_xyz) white_xyz;             /* Reference white in XYZ (Y typically 100) */
    ALWAN_TN(alwan_xyz) xyz_0;                 /* Illuminant of test condition */
    ALWAN_TN(alwan_xyz) xyz_r;                 /* Illuminant of reference condition */
    ALWAN_T Y_b;                               /* Background luminance factor */
    alwan_llab_surround surround;              /* Viewing surround */
    int D_factor;                              /* Degree of adaptation (0-1 or auto) */
} ALWAN_TN(alwan_llab_viewing_conditions);

/* ATD95 viewing conditions */
typedef struct {
    ALWAN_TN(alwan_xyz) white_xyz;             /* Reference white in XYZ */
    ALWAN_T Y_0;                               /* Absolute adapting field luminance in cd/m^2 */
    ALWAN_T sigma;                             /* Saturation adjustment parameter */
    ALWAN_T k1;                                /* Adaptation parameter 1 */
    ALWAN_T k2;                                /* Adaptation parameter 2 */
} ALWAN_TN(alwan_atd95_viewing_conditions);

/* Nayatani95 viewing conditions */
typedef struct {
    ALWAN_TN(alwan_xyz) white_xyz;             /* Reference white in XYZ (Y typically 100) */
    ALWAN_T L_0;                               /* Absolute luminance of reference white (cd/m^2) */
    ALWAN_T Y_0;                               /* Relative luminance of background */
    ALWAN_T E_0;                               /* Illuminance of reference field (lux) */
    ALWAN_T E_0r;                              /* Normalizing factor */
} ALWAN_TN(alwan_nayatani95_viewing_conditions);

/* ----------------------------------------------------------------
 * CAM Correlates
 * ---------------------------------------------------------------- */

/* CIECAM02 appearance correlates */
typedef struct {
    ALWAN_T J;  /* Lightness (0-100) */
    ALWAN_T C;  /* Chroma (0+) */
    ALWAN_T h;  /* Hue angle (0-360 degrees) */
    ALWAN_T s;  /* Saturation (0+) */
    ALWAN_T Q;  /* Brightness (0+) */
    ALWAN_T M;  /* Colorfulness (0+) */
    ALWAN_T H;  /* Hue quadrature (0-400) */
} ALWAN_TN(alwan_ciecam02_correlates);

/* CAM16 appearance correlates */
typedef struct {
    ALWAN_T J;  /* Lightness (0-100) */
    ALWAN_T C;  /* Chroma (0+) */
    ALWAN_T h;  /* Hue angle (0-360 degrees) */
    ALWAN_T s;  /* Saturation (0+) */
    ALWAN_T Q;  /* Brightness (0+) */
    ALWAN_T M;  /* Colorfulness (0+) */
    ALWAN_T H;  /* Hue quadrature (0-400) */
} ALWAN_TN(alwan_cam16_correlates);

/* ZCAM appearance correlates */
typedef struct {
    ALWAN_T Jz;     /* Lightness */
    ALWAN_T Cz;     /* Chroma */
    ALWAN_T hz;     /* Hue angle (degrees) */
    ALWAN_T Qz;     /* Brightness */
    ALWAN_T Mz;     /* Colorfulness */
    ALWAN_T Sz;     /* Saturation */
    ALWAN_T Vz;     /* Vividness */
    ALWAN_T Kz;     /* Blackness */
    ALWAN_T Wz;     /* Whiteness */
} ALWAN_TN(alwan_zcam_correlates);

/* RLAB appearance correlates */
typedef struct {
    ALWAN_T L;    /* Lightness */
    ALWAN_T C;    /* Chroma */
    ALWAN_T h;    /* Hue angle (degrees) */
    ALWAN_T s;    /* Saturation */
    ALWAN_T a;    /* Red-green opponent */
    ALWAN_T b;    /* Yellow-blue opponent */
} ALWAN_TN(alwan_rlab_correlates);

/* Hunt appearance correlates */
typedef struct {
    ALWAN_T J;    /* Lightness */
    ALWAN_T C;    /* Chroma */
    ALWAN_T h;    /* Hue angle (degrees) */
    ALWAN_T s;    /* Saturation */
    ALWAN_T Q;    /* Brightness */
    ALWAN_T M;    /* Colourfulness */
} ALWAN_TN(alwan_hunt_correlates);

/* Hellwig2022 appearance correlates */
typedef struct {
    ALWAN_T J;  /* Lightness (0-100) */
    ALWAN_T C;  /* Chroma (0+) */
    ALWAN_T h;  /* Hue angle (0-360 degrees) */
    ALWAN_T s;  /* Saturation (0+) */
    ALWAN_T Q;  /* Brightness (0+) */
    ALWAN_T M;  /* Colorfulness (0+) */
    ALWAN_T H;  /* Hue quadrature (0-400) */
} ALWAN_TN(alwan_hellwig2022_correlates);

/* Kim2009 appearance correlates */
typedef struct {
    ALWAN_T J;  /* Lightness */
    ALWAN_T C;  /* Chroma */
    ALWAN_T h;  /* Hue angle (degrees) */
} ALWAN_TN(alwan_kim2009_correlates);

/* LLAB appearance correlates */
typedef struct {
    ALWAN_T L;  /* Lightness */
    ALWAN_T Ch; /* Chroma */
    ALWAN_T h;  /* Hue angle (degrees) */
    ALWAN_T s;  /* Saturation */
} ALWAN_TN(alwan_llab_correlates);

/* ATD95 correlates */
typedef struct {
    ALWAN_T H;    /* Hue */
    ALWAN_T C;    /* Chroma */
    ALWAN_T Br;   /* Brightness */
    ALWAN_T A_1;  /* First achromatic response */
    ALWAN_T T_1;  /* First tritanopic response */
    ALWAN_T D_1;  /* First deuteranopic response */
    ALWAN_T A_2;  /* Second achromatic response */
    ALWAN_T T_2;  /* Second tritanopic response */
    ALWAN_T D_2;  /* Second deuteranopic response */
} ALWAN_TN(alwan_atd95_correlates);

/* Nayatani95 appearance correlates */
typedef struct {
    ALWAN_T L_star_N;  /* Perceived lightness */
    ALWAN_T C;         /* Chroma */
    ALWAN_T theta;     /* Hue angle (radians) */
    ALWAN_T S;         /* Saturation */
    ALWAN_T B_r;       /* Brightness */
    ALWAN_T L_star_P;  /* Brightness-to-lightness ratio */
} ALWAN_TN(alwan_nayatani95_correlates);

/* CAM18sl appearance correlates */
typedef struct {
    ALWAN_T Q;   /* Brightness */
    ALWAN_T C;   /* Colorfulness */
    ALWAN_T h;   /* Hue angle (degrees) */
    ALWAN_T M;   /* Colorfulness */
    ALWAN_T a;   /* Red-green */
    ALWAN_T b;   /* Yellow-blue */
} ALWAN_TN(alwan_cam18sl_correlates);

/* CAM20u appearance correlates */
typedef struct {
    ALWAN_T Q;   /* Brightness */
    ALWAN_T M;   /* Colorfulness */
    ALWAN_T h;   /* Hue angle (degrees) */
    ALWAN_T C;   /* Chroma */
    ALWAN_T s;   /* Saturation */
    ALWAN_T a;   /* Red-green */
    ALWAN_T b;   /* Yellow-blue */
} ALWAN_TN(alwan_cam20u_correlates);

/* Resolved table cell: the output of the addressing gate.
 * i0/i1 are provably in [0, size-1] and frac is never NaN, which is what lets
 * every table reader index its corners with no per-read bounds check. */
typedef struct {
    int   i0;
    int   i1;
    ALWAN_T frac;
} ALWAN_TN(alwan_table_cell);

#undef ALWAN_TN
#undef ALWAN_TN_
#undef ALWAN_TN2_
