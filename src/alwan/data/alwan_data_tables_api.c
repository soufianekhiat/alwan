/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Embedded table definitions: the API-tier tables addressed by an INTEGER
 * row index (~1.2 MB of CSV, preprocessed twice).
 *
 * WHY A FOURTH .c AND NOT ONE OF THE OTHER THREE. The split across
 * alwan_data_tables{,_cubes,_spectral}.c is by MEASURED compile weight, not
 * taste (see the head comment of alwan_data_tables.c). The 68 SPD/CMF/camera
 * curves plus the CIE/CQS/TM-30 reflectance sets add ~2.4 MB of preprocessed
 * initializer; folding that into alwan_data_tables.c would roughly double the
 * "light curves" translation unit for no reason, and _cubes.c and _spectral.c
 * are already the heavy ones. So: a fourth file, still split by weight.
 *
 * WHAT LIVES HERE, AND WHY IT IS NOT A FLOAT-COORDINATE TABLE. Everything in
 * the other three data .c files is reached through a float coordinate. Nothing
 * here is. These tables are addressed by a loop counter or a validated enum,
 * and they are homed here for the registry's other three guarantees: the
 * extent is an enum constant instead of a sizeof at the call site, the CSV is
 * #included once instead of once per consumer, and each table has an enable
 * switch. Their reads still go through the shared gate -- alwan_table_row
 * rather than alwan_table_coord -- so ALWAN_READ_DATA_NO_BOUND_CHECK governs
 * them exactly as it governs the cubes.
 *
 * Blocks appear in the SAME ORDER as the declarations in alwan_data_tables.h.
 * That order equality is the invariant tools/check_table_registry.py enforces.
 *
 * The #if wrapper on each block is inert today: the switch defaults to 1. It
 * is here so enabling the feature later flips a default in
 * alwan_data_tables_config.h instead of editing forty blocks.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_data_tables.h"

/* The f32 pass of every dual-declared table narrows f64 CSV literals. */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

#if ALWAN_TABLE_SPD_ILLUMINANT_A
/* ---- alwan_table_spd_illuminant_a ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/A_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_a_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/A_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_a_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/A_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_D50
/* ---- alwan_table_spd_illuminant_d50 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/D50_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_d50_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/D50_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_d50_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/D50_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_D55
/* ---- alwan_table_spd_illuminant_d55 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/D55_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_d55_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/D55_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_d55_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/D55_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_D65
/* ---- alwan_table_spd_illuminant_d65 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/D65_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_d65_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/D65_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_d65_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/D65_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_E
/* ---- alwan_table_spd_illuminant_e ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/E_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_e_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/E_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_e_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/E_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F1
/* ---- alwan_table_spd_illuminant_f1 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F1_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f1_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F1_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_f1_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F1_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F2
/* ---- alwan_table_spd_illuminant_f2 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F2_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f2_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F2_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_f2_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F2_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F3
/* ---- alwan_table_spd_illuminant_f3 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F3_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f3_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_f3_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F4
/* ---- alwan_table_spd_illuminant_f4 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F4_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f4_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F4_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_f4_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F4_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F5
/* ---- alwan_table_spd_illuminant_f5 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F5_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f5_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F5_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_f5_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F5_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F6
/* ---- alwan_table_spd_illuminant_f6 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F6_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f6_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F6_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_f6_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F6_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F7
/* ---- alwan_table_spd_illuminant_f7 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F7_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f7_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F7_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_f7_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F7_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F8
/* ---- alwan_table_spd_illuminant_f8 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F8_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f8_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F8_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_f8_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F8_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F9
/* ---- alwan_table_spd_illuminant_f9 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F9_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f9_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F9_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_f9_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F9_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F10
/* ---- alwan_table_spd_illuminant_f10 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F10_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f10_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F10_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_f10_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F10_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F11
/* ---- alwan_table_spd_illuminant_f11 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F11_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f11_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F11_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_f11_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F11_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F12
/* ---- alwan_table_spd_illuminant_f12 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F12_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f12_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F12_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_f12_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F12_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_B
/* ---- alwan_table_spd_illuminant_b ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/B_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_b_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/B_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_b_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/B_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_C
/* ---- alwan_table_spd_illuminant_c ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/C_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_c_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/C_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_c_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/C_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_D60
/* ---- alwan_table_spd_illuminant_d60 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/D60_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_d60_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/D60_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_d60_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/D60_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_D75
/* ---- alwan_table_spd_illuminant_d75 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/D75_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_d75_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/D75_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_d75_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/D75_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_D40
/* ---- alwan_table_spd_illuminant_d40 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/D40_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_d40_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/D40_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_d40_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/D40_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_D45
/* ---- alwan_table_spd_illuminant_d45 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/D45_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_d45_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/D45_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_d45_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/D45_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_D93
/* ---- alwan_table_spd_illuminant_d93 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/D93_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_d93_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/D93_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_d93_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/D93_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LED_B1
/* ---- alwan_table_spd_illuminant_led_b1 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LED-B1_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_led_b1_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LED-B1_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_led_b1_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LED-B1_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LED_B2
/* ---- alwan_table_spd_illuminant_led_b2 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LED-B2_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_led_b2_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LED-B2_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_led_b2_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LED-B2_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LED_B3
/* ---- alwan_table_spd_illuminant_led_b3 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LED-B3_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_led_b3_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LED-B3_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_led_b3_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LED-B3_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LED_B4
/* ---- alwan_table_spd_illuminant_led_b4 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LED-B4_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_led_b4_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LED-B4_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_led_b4_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LED-B4_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LED_B5
/* ---- alwan_table_spd_illuminant_led_b5 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LED-B5_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_led_b5_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LED-B5_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_led_b5_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LED-B5_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LED_BH1
/* ---- alwan_table_spd_illuminant_led_bh1 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LED-BH1_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_led_bh1_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LED-BH1_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_led_bh1_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LED-BH1_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LED_RGB1
/* ---- alwan_table_spd_illuminant_led_rgb1 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LED-RGB1_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_led_rgb1_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LED-RGB1_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_led_rgb1_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LED-RGB1_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LED_V1
/* ---- alwan_table_spd_illuminant_led_v1 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LED-V1_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_led_v1_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LED-V1_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_led_v1_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LED-V1_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LED_V2
/* ---- alwan_table_spd_illuminant_led_v2 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LED-V2_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_led_v2_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LED-V2_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_led_v2_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LED-V2_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_HP1
/* ---- alwan_table_spd_illuminant_hp1 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/HP1_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_hp1_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/HP1_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_hp1_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/HP1_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_HP2
/* ---- alwan_table_spd_illuminant_hp2 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/HP2_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_hp2_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/HP2_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_hp2_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/HP2_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_HP3
/* ---- alwan_table_spd_illuminant_hp3 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/HP3_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_hp3_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/HP3_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_hp3_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/HP3_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_HP4
/* ---- alwan_table_spd_illuminant_hp4 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/HP4_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_hp4_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/HP4_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_hp4_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/HP4_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_HP5
/* ---- alwan_table_spd_illuminant_hp5 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/HP5_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_hp5_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/HP5_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_spd_illuminant_hp5_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/HP5_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_CMF_CIE_1931_2DEG
/* ---- alwan_table_cmf_cie_1931_2deg_x ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/cie_1931_2deg_x_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_cie_1931_2deg_x_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_1931_2deg_x_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_cie_1931_2deg_x_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_1931_2deg_x_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_cie_1931_2deg_y ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/cie_1931_2deg_y_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_cie_1931_2deg_y_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_1931_2deg_y_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_cie_1931_2deg_y_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_1931_2deg_y_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_cie_1931_2deg_z ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/cie_1931_2deg_z_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_cie_1931_2deg_z_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_1931_2deg_z_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_cie_1931_2deg_z_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_1931_2deg_z_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_CMF_CIE_1964_10DEG
/* ---- alwan_table_cmf_cie_1964_10deg_x ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/cie_1964_10deg_x_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_cie_1964_10deg_x_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_1964_10deg_x_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_cie_1964_10deg_x_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_1964_10deg_x_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_cie_1964_10deg_y ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/cie_1964_10deg_y_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_cie_1964_10deg_y_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_1964_10deg_y_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_cie_1964_10deg_y_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_1964_10deg_y_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_cie_1964_10deg_z ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/cie_1964_10deg_z_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_cie_1964_10deg_z_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_1964_10deg_z_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_cie_1964_10deg_z_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_1964_10deg_z_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_CMF_CIE_2012_2DEG
/* ---- alwan_table_cmf_cie_2012_2deg_x ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/cie_2012_2deg_x_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_cie_2012_2deg_x_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2012_2deg_x_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_cie_2012_2deg_x_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2012_2deg_x_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_cie_2012_2deg_y ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/cie_2012_2deg_y_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_cie_2012_2deg_y_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2012_2deg_y_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_cie_2012_2deg_y_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2012_2deg_y_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_cie_2012_2deg_z ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/cie_2012_2deg_z_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_cie_2012_2deg_z_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2012_2deg_z_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_cie_2012_2deg_z_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2012_2deg_z_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_CMF_CIE_2012_10DEG
/* ---- alwan_table_cmf_cie_2012_10deg_x ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/cie_2012_10deg_x_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_cie_2012_10deg_x_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2012_10deg_x_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_cie_2012_10deg_x_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2012_10deg_x_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_cie_2012_10deg_y ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/cie_2012_10deg_y_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_cie_2012_10deg_y_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2012_10deg_y_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_cie_2012_10deg_y_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2012_10deg_y_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_cie_2012_10deg_z ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/cie_2012_10deg_z_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_cie_2012_10deg_z_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2012_10deg_z_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_cie_2012_10deg_z_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2012_10deg_z_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_CMF_STOCKMAN_SHARPE_2DEG
/* ---- alwan_table_cmf_stockman_sharpe_2deg_x ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/stockman_sharpe_2deg_x_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_stockman_sharpe_2deg_x_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stockman_sharpe_2deg_x_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_stockman_sharpe_2deg_x_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stockman_sharpe_2deg_x_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_stockman_sharpe_2deg_y ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/stockman_sharpe_2deg_y_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_stockman_sharpe_2deg_y_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stockman_sharpe_2deg_y_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_stockman_sharpe_2deg_y_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stockman_sharpe_2deg_y_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_stockman_sharpe_2deg_z ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/stockman_sharpe_2deg_z_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_stockman_sharpe_2deg_z_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stockman_sharpe_2deg_z_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_stockman_sharpe_2deg_z_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stockman_sharpe_2deg_z_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_CMF_CIE_2015_2DEG
/* ---- alwan_table_cmf_cie_2015_2deg_x ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/cie_2015_2deg_x_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_cie_2015_2deg_x_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2015_2deg_x_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_cie_2015_2deg_x_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2015_2deg_x_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_cie_2015_2deg_y ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/cie_2015_2deg_y_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_cie_2015_2deg_y_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2015_2deg_y_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_cie_2015_2deg_y_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2015_2deg_y_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_cie_2015_2deg_z ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/cie_2015_2deg_z_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_cie_2015_2deg_z_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2015_2deg_z_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_cie_2015_2deg_z_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2015_2deg_z_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_CMF_CIE_2015_10DEG
/* ---- alwan_table_cmf_cie_2015_10deg_x ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/cie_2015_10deg_x_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_cie_2015_10deg_x_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2015_10deg_x_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_cie_2015_10deg_x_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2015_10deg_x_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_cie_2015_10deg_y ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/cie_2015_10deg_y_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_cie_2015_10deg_y_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2015_10deg_y_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_cie_2015_10deg_y_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2015_10deg_y_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_cie_2015_10deg_z ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/cie_2015_10deg_z_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_cie_2015_10deg_z_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2015_10deg_z_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_cie_2015_10deg_z_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_2015_10deg_z_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_CMF_WRIGHT_GUILD_1931
/* ---- alwan_table_cmf_wright_guild_1931_r ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/wright_guild_1931_r_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_wright_guild_1931_r_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/wright_guild_1931_r_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_wright_guild_1931_r_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/wright_guild_1931_r_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_wright_guild_1931_g ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/wright_guild_1931_g_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_wright_guild_1931_g_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/wright_guild_1931_g_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_wright_guild_1931_g_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/wright_guild_1931_g_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_wright_guild_1931_b ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/wright_guild_1931_b_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_wright_guild_1931_b_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/wright_guild_1931_b_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_cmf_wright_guild_1931_b_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/wright_guild_1931_b_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_CAMERA_NIKON_5100
/* ---- alwan_table_camera_nikon_5100_r ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: camera_sensitivities/nikon_5100_r.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_camera_nikon_5100_r_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "camera_sensitivities/nikon_5100_r.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_camera_nikon_5100_r_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "camera_sensitivities/nikon_5100_r.csv"
};
#endif

/* ---- alwan_table_camera_nikon_5100_g ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: camera_sensitivities/nikon_5100_g.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_camera_nikon_5100_g_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "camera_sensitivities/nikon_5100_g.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_camera_nikon_5100_g_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "camera_sensitivities/nikon_5100_g.csv"
};
#endif

/* ---- alwan_table_camera_nikon_5100_b ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: camera_sensitivities/nikon_5100_b.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_camera_nikon_5100_b_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "camera_sensitivities/nikon_5100_b.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_camera_nikon_5100_b_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "camera_sensitivities/nikon_5100_b.csv"
};
#endif

#endif

#if ALWAN_TABLE_CAMERA_SIGMA_SDMERILL
/* ---- alwan_table_camera_sigma_sdmerill_r ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: camera_sensitivities/sigma_sdmerill_r.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_camera_sigma_sdmerill_r_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "camera_sensitivities/sigma_sdmerill_r.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_camera_sigma_sdmerill_r_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "camera_sensitivities/sigma_sdmerill_r.csv"
};
#endif

/* ---- alwan_table_camera_sigma_sdmerill_g ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: camera_sensitivities/sigma_sdmerill_g.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_camera_sigma_sdmerill_g_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "camera_sensitivities/sigma_sdmerill_g.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_camera_sigma_sdmerill_g_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "camera_sensitivities/sigma_sdmerill_g.csv"
};
#endif

/* ---- alwan_table_camera_sigma_sdmerill_b ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: camera_sensitivities/sigma_sdmerill_b.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_camera_sigma_sdmerill_b_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "camera_sensitivities/sigma_sdmerill_b.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_camera_sigma_sdmerill_b_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "camera_sensitivities/sigma_sdmerill_b.csv"
};
#endif

#endif

#if ALWAN_TABLE_SMITS1999
/* ---- alwan_table_smits1999_white ----
 * extent ALWAN_TABLE_SMITS1999_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: spectral_basis/smits1999/white.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_smits1999_white_f32[ALWAN_TABLE_SMITS1999_SIZE] = {
#include "spectral_basis/smits1999/white.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_smits1999_white_f64[ALWAN_TABLE_SMITS1999_SIZE] = {
#include "spectral_basis/smits1999/white.csv"
};
#endif

/* ---- alwan_table_smits1999_cyan ----
 * extent ALWAN_TABLE_SMITS1999_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: spectral_basis/smits1999/cyan.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_smits1999_cyan_f32[ALWAN_TABLE_SMITS1999_SIZE] = {
#include "spectral_basis/smits1999/cyan.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_smits1999_cyan_f64[ALWAN_TABLE_SMITS1999_SIZE] = {
#include "spectral_basis/smits1999/cyan.csv"
};
#endif

/* ---- alwan_table_smits1999_magenta ----
 * extent ALWAN_TABLE_SMITS1999_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: spectral_basis/smits1999/magenta.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_smits1999_magenta_f32[ALWAN_TABLE_SMITS1999_SIZE] = {
#include "spectral_basis/smits1999/magenta.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_smits1999_magenta_f64[ALWAN_TABLE_SMITS1999_SIZE] = {
#include "spectral_basis/smits1999/magenta.csv"
};
#endif

/* ---- alwan_table_smits1999_yellow ----
 * extent ALWAN_TABLE_SMITS1999_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: spectral_basis/smits1999/yellow.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_smits1999_yellow_f32[ALWAN_TABLE_SMITS1999_SIZE] = {
#include "spectral_basis/smits1999/yellow.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_smits1999_yellow_f64[ALWAN_TABLE_SMITS1999_SIZE] = {
#include "spectral_basis/smits1999/yellow.csv"
};
#endif

/* ---- alwan_table_smits1999_red ----
 * extent ALWAN_TABLE_SMITS1999_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: spectral_basis/smits1999/red.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_smits1999_red_f32[ALWAN_TABLE_SMITS1999_SIZE] = {
#include "spectral_basis/smits1999/red.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_smits1999_red_f64[ALWAN_TABLE_SMITS1999_SIZE] = {
#include "spectral_basis/smits1999/red.csv"
};
#endif

/* ---- alwan_table_smits1999_green ----
 * extent ALWAN_TABLE_SMITS1999_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: spectral_basis/smits1999/green.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_smits1999_green_f32[ALWAN_TABLE_SMITS1999_SIZE] = {
#include "spectral_basis/smits1999/green.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_smits1999_green_f64[ALWAN_TABLE_SMITS1999_SIZE] = {
#include "spectral_basis/smits1999/green.csv"
};
#endif

/* ---- alwan_table_smits1999_blue ----
 * extent ALWAN_TABLE_SMITS1999_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: spectral_basis/smits1999/blue.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_smits1999_blue_f32[ALWAN_TABLE_SMITS1999_SIZE] = {
#include "spectral_basis/smits1999/blue.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_smits1999_blue_f64[ALWAN_TABLE_SMITS1999_SIZE] = {
#include "spectral_basis/smits1999/blue.csv"
};
#endif

#endif

#if ALWAN_TABLE_MALLETT2019
/* ---- alwan_table_mallett2019_red ----
 * extent ALWAN_TABLE_MALLETT2019_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: spectral_basis/mallett2019/red.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_mallett2019_red_f32[ALWAN_TABLE_MALLETT2019_SIZE] = {
#include "spectral_basis/mallett2019/red.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_mallett2019_red_f64[ALWAN_TABLE_MALLETT2019_SIZE] = {
#include "spectral_basis/mallett2019/red.csv"
};
#endif

/* ---- alwan_table_mallett2019_green ----
 * extent ALWAN_TABLE_MALLETT2019_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: spectral_basis/mallett2019/green.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_mallett2019_green_f32[ALWAN_TABLE_MALLETT2019_SIZE] = {
#include "spectral_basis/mallett2019/green.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_mallett2019_green_f64[ALWAN_TABLE_MALLETT2019_SIZE] = {
#include "spectral_basis/mallett2019/green.csv"
};
#endif

/* ---- alwan_table_mallett2019_blue ----
 * extent ALWAN_TABLE_MALLETT2019_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: spectral_basis/mallett2019/blue.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_mallett2019_blue_f32[ALWAN_TABLE_MALLETT2019_SIZE] = {
#include "spectral_basis/mallett2019/blue.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_mallett2019_blue_f64[ALWAN_TABLE_MALLETT2019_SIZE] = {
#include "spectral_basis/mallett2019/blue.csv"
};
#endif

#endif

#if ALWAN_TABLE_AGX_SB2383_INSET
/* ---- alwan_table_agx_sb2383_inset ----
 * extent ALWAN_TABLE_AGX_SB2383_INSET_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: matrices/agx_sb2383_inset.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_agx_sb2383_inset_f32[ALWAN_TABLE_AGX_SB2383_INSET_SIZE] = {
#include "matrices/agx_sb2383_inset.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_agx_sb2383_inset_f64[ALWAN_TABLE_AGX_SB2383_INSET_SIZE] = {
#include "matrices/agx_sb2383_inset.csv"
};
#endif

#endif

#if ALWAN_TABLE_ROBERTSON_LOCUS
/* ---- alwan_table_robertson_locus ----
 * extent ALWAN_TABLE_ROBERTSON_SIZE. Reader: alwan_table2d_row_at_{f32,f64}
 * Source: fixtures/robertson_cct_locus.csv */
/* f64 in every build; see ALWAN_TABLE_EXTERN_F64_ONLY in alwan_data_tables.h.
 * No f32 twin: nothing reads one, and both precisions read this table so the
 * f32 entry points return the same numbers as the f64 ones. */
alwan_f64 const alwan_table_robertson_locus_f64[ALWAN_TABLE_ROBERTSON_SIZE] = {
#include "fixtures/robertson_cct_locus.csv"
};

#endif

#if ALWAN_TABLE_TCS_REFLECTANCE
/* ---- alwan_table_tcs_reflectance ----
 * extent ALWAN_TABLE_TCS_SIZE. Reader: alwan_table2d_row_at_{f32,f64}
 * Source: fixtures/tcs_01_reflectance.csv .. fixtures/tcs_14_reflectance.csv, concatenated in order.
 * Each CSV ends with a trailing comma, so this is the same tokens
 * in the same order as the 14 separate arrays it replaces. */
/* f64 in every build; see ALWAN_TABLE_EXTERN_F64_ONLY in alwan_data_tables.h.
 * No f32 twin: nothing reads one, and both precisions read this table so the
 * f32 entry points return the same numbers as the f64 ones. */
alwan_f64 const alwan_table_tcs_reflectance_f64[ALWAN_TABLE_TCS_SIZE] = {
#include "fixtures/tcs_01_reflectance.csv"
#include "fixtures/tcs_02_reflectance.csv"
#include "fixtures/tcs_03_reflectance.csv"
#include "fixtures/tcs_04_reflectance.csv"
#include "fixtures/tcs_05_reflectance.csv"
#include "fixtures/tcs_06_reflectance.csv"
#include "fixtures/tcs_07_reflectance.csv"
#include "fixtures/tcs_08_reflectance.csv"
#include "fixtures/tcs_09_reflectance.csv"
#include "fixtures/tcs_10_reflectance.csv"
#include "fixtures/tcs_11_reflectance.csv"
#include "fixtures/tcs_12_reflectance.csv"
#include "fixtures/tcs_13_reflectance.csv"
#include "fixtures/tcs_14_reflectance.csv"
};

#endif

#if ALWAN_TABLE_VS_REFLECTANCE
/* ---- alwan_table_vs_reflectance ----
 * extent ALWAN_TABLE_VS_SIZE. Reader: alwan_table2d_row_at_{f32,f64}
 * Source: fixtures/vs_01_reflectance.csv .. fixtures/vs_15_reflectance.csv, concatenated in order.
 * Each CSV ends with a trailing comma, so this is the same tokens
 * in the same order as the 15 separate arrays it replaces. */
/* f64 in every build; see ALWAN_TABLE_EXTERN_F64_ONLY in alwan_data_tables.h.
 * No f32 twin: nothing reads one, and both precisions read this table so the
 * f32 entry points return the same numbers as the f64 ones. */
alwan_f64 const alwan_table_vs_reflectance_f64[ALWAN_TABLE_VS_SIZE] = {
#include "fixtures/vs_01_reflectance.csv"
#include "fixtures/vs_02_reflectance.csv"
#include "fixtures/vs_03_reflectance.csv"
#include "fixtures/vs_04_reflectance.csv"
#include "fixtures/vs_05_reflectance.csv"
#include "fixtures/vs_06_reflectance.csv"
#include "fixtures/vs_07_reflectance.csv"
#include "fixtures/vs_08_reflectance.csv"
#include "fixtures/vs_09_reflectance.csv"
#include "fixtures/vs_10_reflectance.csv"
#include "fixtures/vs_11_reflectance.csv"
#include "fixtures/vs_12_reflectance.csv"
#include "fixtures/vs_13_reflectance.csv"
#include "fixtures/vs_14_reflectance.csv"
#include "fixtures/vs_15_reflectance.csv"
};

#endif

#if ALWAN_TABLE_CES_REFLECTANCE
/* ---- alwan_table_ces_reflectance ----
 * extent ALWAN_TABLE_CES_SIZE. Reader: alwan_table2d_row_at_{f32,f64}
 * Source: fixtures/ces_01_reflectance.csv .. fixtures/ces_99_reflectance.csv, concatenated in order.
 * Each CSV ends with a trailing comma, so this is the same tokens
 * in the same order as the 99 separate arrays it replaces. */
/* f64 in every build; see ALWAN_TABLE_EXTERN_F64_ONLY in alwan_data_tables.h.
 * No f32 twin: nothing reads one, and both precisions read this table so the
 * f32 entry points return the same numbers as the f64 ones. */
alwan_f64 const alwan_table_ces_reflectance_f64[ALWAN_TABLE_CES_SIZE] = {
#include "fixtures/ces_01_reflectance.csv"
#include "fixtures/ces_02_reflectance.csv"
#include "fixtures/ces_03_reflectance.csv"
#include "fixtures/ces_04_reflectance.csv"
#include "fixtures/ces_05_reflectance.csv"
#include "fixtures/ces_06_reflectance.csv"
#include "fixtures/ces_07_reflectance.csv"
#include "fixtures/ces_08_reflectance.csv"
#include "fixtures/ces_09_reflectance.csv"
#include "fixtures/ces_10_reflectance.csv"
#include "fixtures/ces_11_reflectance.csv"
#include "fixtures/ces_12_reflectance.csv"
#include "fixtures/ces_13_reflectance.csv"
#include "fixtures/ces_14_reflectance.csv"
#include "fixtures/ces_15_reflectance.csv"
#include "fixtures/ces_16_reflectance.csv"
#include "fixtures/ces_17_reflectance.csv"
#include "fixtures/ces_18_reflectance.csv"
#include "fixtures/ces_19_reflectance.csv"
#include "fixtures/ces_20_reflectance.csv"
#include "fixtures/ces_21_reflectance.csv"
#include "fixtures/ces_22_reflectance.csv"
#include "fixtures/ces_23_reflectance.csv"
#include "fixtures/ces_24_reflectance.csv"
#include "fixtures/ces_25_reflectance.csv"
#include "fixtures/ces_26_reflectance.csv"
#include "fixtures/ces_27_reflectance.csv"
#include "fixtures/ces_28_reflectance.csv"
#include "fixtures/ces_29_reflectance.csv"
#include "fixtures/ces_30_reflectance.csv"
#include "fixtures/ces_31_reflectance.csv"
#include "fixtures/ces_32_reflectance.csv"
#include "fixtures/ces_33_reflectance.csv"
#include "fixtures/ces_34_reflectance.csv"
#include "fixtures/ces_35_reflectance.csv"
#include "fixtures/ces_36_reflectance.csv"
#include "fixtures/ces_37_reflectance.csv"
#include "fixtures/ces_38_reflectance.csv"
#include "fixtures/ces_39_reflectance.csv"
#include "fixtures/ces_40_reflectance.csv"
#include "fixtures/ces_41_reflectance.csv"
#include "fixtures/ces_42_reflectance.csv"
#include "fixtures/ces_43_reflectance.csv"
#include "fixtures/ces_44_reflectance.csv"
#include "fixtures/ces_45_reflectance.csv"
#include "fixtures/ces_46_reflectance.csv"
#include "fixtures/ces_47_reflectance.csv"
#include "fixtures/ces_48_reflectance.csv"
#include "fixtures/ces_49_reflectance.csv"
#include "fixtures/ces_50_reflectance.csv"
#include "fixtures/ces_51_reflectance.csv"
#include "fixtures/ces_52_reflectance.csv"
#include "fixtures/ces_53_reflectance.csv"
#include "fixtures/ces_54_reflectance.csv"
#include "fixtures/ces_55_reflectance.csv"
#include "fixtures/ces_56_reflectance.csv"
#include "fixtures/ces_57_reflectance.csv"
#include "fixtures/ces_58_reflectance.csv"
#include "fixtures/ces_59_reflectance.csv"
#include "fixtures/ces_60_reflectance.csv"
#include "fixtures/ces_61_reflectance.csv"
#include "fixtures/ces_62_reflectance.csv"
#include "fixtures/ces_63_reflectance.csv"
#include "fixtures/ces_64_reflectance.csv"
#include "fixtures/ces_65_reflectance.csv"
#include "fixtures/ces_66_reflectance.csv"
#include "fixtures/ces_67_reflectance.csv"
#include "fixtures/ces_68_reflectance.csv"
#include "fixtures/ces_69_reflectance.csv"
#include "fixtures/ces_70_reflectance.csv"
#include "fixtures/ces_71_reflectance.csv"
#include "fixtures/ces_72_reflectance.csv"
#include "fixtures/ces_73_reflectance.csv"
#include "fixtures/ces_74_reflectance.csv"
#include "fixtures/ces_75_reflectance.csv"
#include "fixtures/ces_76_reflectance.csv"
#include "fixtures/ces_77_reflectance.csv"
#include "fixtures/ces_78_reflectance.csv"
#include "fixtures/ces_79_reflectance.csv"
#include "fixtures/ces_80_reflectance.csv"
#include "fixtures/ces_81_reflectance.csv"
#include "fixtures/ces_82_reflectance.csv"
#include "fixtures/ces_83_reflectance.csv"
#include "fixtures/ces_84_reflectance.csv"
#include "fixtures/ces_85_reflectance.csv"
#include "fixtures/ces_86_reflectance.csv"
#include "fixtures/ces_87_reflectance.csv"
#include "fixtures/ces_88_reflectance.csv"
#include "fixtures/ces_89_reflectance.csv"
#include "fixtures/ces_90_reflectance.csv"
#include "fixtures/ces_91_reflectance.csv"
#include "fixtures/ces_92_reflectance.csv"
#include "fixtures/ces_93_reflectance.csv"
#include "fixtures/ces_94_reflectance.csv"
#include "fixtures/ces_95_reflectance.csv"
#include "fixtures/ces_96_reflectance.csv"
#include "fixtures/ces_97_reflectance.csv"
#include "fixtures/ces_98_reflectance.csv"
#include "fixtures/ces_99_reflectance.csv"
};

#endif

#if ALWAN_TABLE_DAYLIGHT_BASIS
/* ---- alwan_table_daylight_basis ----
 * extent ALWAN_TABLE_DAYLIGHT_BASIS_SIZE. Reader: alwan_table2d_row_at_f64
 * Source: fixtures/daylight_basis_s012.csv (S0, S1, S2 concatenated) */
/* f64 in every build; see ALWAN_TABLE_EXTERN_F64_ONLY in alwan_data_tables.h. */
alwan_f64 const alwan_table_daylight_basis_f64[ALWAN_TABLE_DAYLIGHT_BASIS_SIZE] = {
#include "fixtures/daylight_basis_s012.csv"
};
#endif

#if ALWAN_TABLE_SSI_BIN_WEIGHTS
/* ---- alwan_table_ssi_bin_weights ----
 * extent ALWAN_TABLE_SSI_BIN_TAPS. Reader: alwan_table1d_row_{f32,f64}
 * Source: ssi_bin_weights.csv */
/* f64 in every build; see ALWAN_TABLE_EXTERN_F64_ONLY in alwan_data_tables.h.
 * No f32 twin: nothing reads one, and both precisions read this table so the
 * f32 entry points return the same numbers as the f64 ones. */
alwan_f64 const alwan_table_ssi_bin_weights_f64[ALWAN_TABLE_SSI_BIN_TAPS] = {
#include "ssi_bin_weights.csv"
};

#endif

#if ALWAN_TABLE_SSI_SPECTRAL_WEIGHTS
/* ---- alwan_table_ssi_spectral_weights ----
 * extent ALWAN_TABLE_SSI_BIN_COUNT. Reader: alwan_table1d_row_{f32,f64}
 * Source: ssi_spectral_weights.csv */
/* f64 in every build; see ALWAN_TABLE_EXTERN_F64_ONLY in alwan_data_tables.h.
 * No f32 twin: nothing reads one, and both precisions read this table so the
 * f32 entry points return the same numbers as the f64 ones. */
alwan_f64 const alwan_table_ssi_spectral_weights_f64[ALWAN_TABLE_SSI_BIN_COUNT] = {
#include "ssi_spectral_weights.csv"
};

#endif


ALWAN_DIAG_POP
