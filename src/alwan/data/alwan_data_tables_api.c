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
/* Compiled in every build: the documented f64-internal facades read these f64
 * tables from their f32 entry points, so the data has to exist even when the
 * f64 public surface is excluded. See ALWAN_WITH_F64_FACADE. */
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_hp5_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/HP5_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F3_1
/* ---- alwan_table_spd_illuminant_f3_1 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F3_1_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f3_1_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_1_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_f3_1_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_1_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F3_2
/* ---- alwan_table_spd_illuminant_f3_2 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F3_2_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f3_2_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_2_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_f3_2_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_2_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F3_3
/* ---- alwan_table_spd_illuminant_f3_3 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F3_3_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f3_3_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_3_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_f3_3_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_3_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F3_4
/* ---- alwan_table_spd_illuminant_f3_4 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F3_4_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f3_4_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_4_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_f3_4_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_4_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F3_5
/* ---- alwan_table_spd_illuminant_f3_5 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F3_5_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f3_5_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_5_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_f3_5_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_5_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F3_6
/* ---- alwan_table_spd_illuminant_f3_6 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F3_6_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f3_6_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_6_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_f3_6_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_6_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F3_7
/* ---- alwan_table_spd_illuminant_f3_7 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F3_7_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f3_7_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_7_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_f3_7_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_7_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F3_8
/* ---- alwan_table_spd_illuminant_f3_8 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F3_8_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f3_8_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_8_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_f3_8_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_8_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F3_9
/* ---- alwan_table_spd_illuminant_f3_9 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F3_9_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f3_9_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_9_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_f3_9_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_9_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F3_10
/* ---- alwan_table_spd_illuminant_f3_10 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F3_10_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f3_10_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_10_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_f3_10_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_10_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F3_11
/* ---- alwan_table_spd_illuminant_f3_11 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F3_11_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f3_11_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_11_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_f3_11_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_11_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F3_12
/* ---- alwan_table_spd_illuminant_f3_12 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F3_12_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f3_12_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_12_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_f3_12_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_12_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F3_13
/* ---- alwan_table_spd_illuminant_f3_13 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F3_13_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f3_13_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_13_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_f3_13_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_13_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F3_14
/* ---- alwan_table_spd_illuminant_f3_14 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F3_14_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f3_14_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_14_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_f3_14_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_14_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_F3_15
/* ---- alwan_table_spd_illuminant_f3_15 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/F3_15_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_f3_15_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_15_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_f3_15_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/F3_15_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_ID50
/* ---- alwan_table_spd_illuminant_id50 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/ID50_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_id50_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/ID50_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_id50_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/ID50_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_ID65
/* ---- alwan_table_spd_illuminant_id65 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/ID65_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_id65_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/ID65_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_id65_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/ID65_360_830_1nm.csv"
};
#endif

#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_LS_NATURAL
/* ---- alwan_table_spd_illuminant_ls_natural ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_NATURAL_360_830_1nm.csv (Natural) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_natural_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_NATURAL_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_natural_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_NATURAL_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_PHILIPS_TL84
/* ---- alwan_table_spd_illuminant_ls_philips_tl84 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_PHILIPS_TL84_360_830_1nm.csv (Philips TL-84) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_philips_tl84_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_PHILIPS_TL84_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_philips_tl84_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_PHILIPS_TL84_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_SA
/* ---- alwan_table_spd_illuminant_ls_sa ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_SA_360_830_1nm.csv (SA) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_sa_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_SA_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_sa_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_SA_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_SC
/* ---- alwan_table_spd_illuminant_ls_sc ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_SC_360_830_1nm.csv (SC) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_sc_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_SC_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_sc_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_SC_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_T8_LUXLINE_PLUS_WHITE
/* ---- alwan_table_spd_illuminant_ls_t8_luxline_plus_white ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_T8_LUXLINE_PLUS_WHITE_360_830_1nm.csv (T8 Luxline Plus White) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_t8_luxline_plus_white_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_T8_LUXLINE_PLUS_WHITE_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_t8_luxline_plus_white_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_T8_LUXLINE_PLUS_WHITE_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_T8_POLYLUX_3000
/* ---- alwan_table_spd_illuminant_ls_t8_polylux_3000 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_T8_POLYLUX_3000_360_830_1nm.csv (T8 Polylux 3000) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_t8_polylux_3000_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_T8_POLYLUX_3000_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_t8_polylux_3000_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_T8_POLYLUX_3000_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_T8_POLYLUX_4000
/* ---- alwan_table_spd_illuminant_ls_t8_polylux_4000 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_T8_POLYLUX_4000_360_830_1nm.csv (T8 Polylux 4000) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_t8_polylux_4000_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_T8_POLYLUX_4000_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_t8_polylux_4000_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_T8_POLYLUX_4000_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_THORN_KOLOR_RITE
/* ---- alwan_table_spd_illuminant_ls_thorn_kolor_rite ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_THORN_KOLOR_RITE_360_830_1nm.csv (Thorn Kolor-rite) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_thorn_kolor_rite_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_THORN_KOLOR_RITE_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_thorn_kolor_rite_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_THORN_KOLOR_RITE_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_COOL_WHITE_FL
/* ---- alwan_table_spd_illuminant_ls_cool_white_fl ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_COOL_WHITE_FL_360_830_1nm.csv (Cool White FL) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_cool_white_fl_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_COOL_WHITE_FL_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_cool_white_fl_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_COOL_WHITE_FL_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_DAYLIGHT_FL
/* ---- alwan_table_spd_illuminant_ls_daylight_fl ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_DAYLIGHT_FL_360_830_1nm.csv (Daylight FL) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_daylight_fl_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_DAYLIGHT_FL_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_daylight_fl_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_DAYLIGHT_FL_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_HPS
/* ---- alwan_table_spd_illuminant_ls_hps ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_HPS_360_830_1nm.csv (HPS) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_hps_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_HPS_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_hps_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_HPS_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_INCANDESCENT
/* ---- alwan_table_spd_illuminant_ls_incandescent ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_INCANDESCENT_360_830_1nm.csv (Incandescent) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_incandescent_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_INCANDESCENT_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_incandescent_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_INCANDESCENT_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_LPS
/* ---- alwan_table_spd_illuminant_ls_lps ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_LPS_360_830_1nm.csv (LPS) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_lps_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_LPS_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_lps_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_LPS_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_MERCURY
/* ---- alwan_table_spd_illuminant_ls_mercury ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_MERCURY_360_830_1nm.csv (Mercury) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_mercury_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_MERCURY_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_mercury_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_MERCURY_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_METAL_HALIDE
/* ---- alwan_table_spd_illuminant_ls_metal_halide ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_METAL_HALIDE_360_830_1nm.csv (Metal Halide) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_metal_halide_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_METAL_HALIDE_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_metal_halide_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_METAL_HALIDE_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_NEODIMIUM_INCANDESCENT
/* ---- alwan_table_spd_illuminant_ls_neodimium_incandescent ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_NEODIMIUM_INCANDESCENT_360_830_1nm.csv (Neodimium Incandescent) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_neodimium_incandescent_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_NEODIMIUM_INCANDESCENT_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_neodimium_incandescent_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_NEODIMIUM_INCANDESCENT_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_SUPER_HPS
/* ---- alwan_table_spd_illuminant_ls_super_hps ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_SUPER_HPS_360_830_1nm.csv (Super HPS) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_super_hps_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_SUPER_HPS_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_super_hps_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_SUPER_HPS_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_TRIPHOSPHOR_FL
/* ---- alwan_table_spd_illuminant_ls_triphosphor_fl ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_TRIPHOSPHOR_FL_360_830_1nm.csv (Triphosphor FL) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_triphosphor_fl_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_TRIPHOSPHOR_FL_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_triphosphor_fl_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_TRIPHOSPHOR_FL_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_3LED_1
/* ---- alwan_table_spd_illuminant_ls_3led_1 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_3LED_1_360_830_1nm.csv (3-LED-1 (457/540/605)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_3led_1_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_3LED_1_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_3led_1_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_3LED_1_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_3LED_2
/* ---- alwan_table_spd_illuminant_ls_3led_2 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_3LED_2_360_830_1nm.csv (3-LED-2 (473/545/616)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_3led_2_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_3LED_2_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_3led_2_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_3LED_2_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_3LED_2_YELLOW
/* ---- alwan_table_spd_illuminant_ls_3led_2_yellow ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_3LED_2_YELLOW_360_830_1nm.csv (3-LED-2 Yellow) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_3led_2_yellow_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_3LED_2_YELLOW_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_3led_2_yellow_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_3LED_2_YELLOW_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_3LED_3
/* ---- alwan_table_spd_illuminant_ls_3led_3 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_3LED_3_360_830_1nm.csv (3-LED-3 (465/546/614)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_3led_3_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_3LED_3_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_3led_3_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_3LED_3_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_3LED_4
/* ---- alwan_table_spd_illuminant_ls_3led_4 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_3LED_4_360_830_1nm.csv (3-LED-4 (455/547/623)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_3led_4_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_3LED_4_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_3led_4_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_3LED_4_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_4LED_NO_YELLOW
/* ---- alwan_table_spd_illuminant_ls_4led_no_yellow ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_4LED_NO_YELLOW_360_830_1nm.csv (4-LED No Yellow) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_4led_no_yellow_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_4LED_NO_YELLOW_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_4led_no_yellow_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_4LED_NO_YELLOW_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_4LED_YELLOW
/* ---- alwan_table_spd_illuminant_ls_4led_yellow ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_4LED_YELLOW_360_830_1nm.csv (4-LED Yellow) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_4led_yellow_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_4LED_YELLOW_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_4led_yellow_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_4LED_YELLOW_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_4LED_1
/* ---- alwan_table_spd_illuminant_ls_4led_1 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_4LED_1_360_830_1nm.csv (4-LED-1 (461/526/576/624)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_4led_1_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_4LED_1_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_4led_1_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_4LED_1_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_4LED_2
/* ---- alwan_table_spd_illuminant_ls_4led_2 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_4LED_2_360_830_1nm.csv (4-LED-2 (447/512/573/627)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_4led_2_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_4LED_2_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_4led_2_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_4LED_2_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_LUXEON_WW_2880
/* ---- alwan_table_spd_illuminant_ls_luxeon_ww_2880 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_LUXEON_WW_2880_360_830_1nm.csv (Luxeon WW 2880) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_luxeon_ww_2880_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_LUXEON_WW_2880_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_luxeon_ww_2880_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_LUXEON_WW_2880_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_PHOS_1
/* ---- alwan_table_spd_illuminant_ls_phos_1 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_PHOS_1_360_830_1nm.csv (PHOS-1) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_phos_1_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_PHOS_1_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_phos_1_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_PHOS_1_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_PHOS_2
/* ---- alwan_table_spd_illuminant_ls_phos_2 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_PHOS_2_360_830_1nm.csv (PHOS-2) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_phos_2_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_PHOS_2_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_phos_2_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_PHOS_2_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_PHOS_3
/* ---- alwan_table_spd_illuminant_ls_phos_3 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_PHOS_3_360_830_1nm.csv (PHOS-3) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_phos_3_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_PHOS_3_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_phos_3_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_PHOS_3_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_PHOS_4
/* ---- alwan_table_spd_illuminant_ls_phos_4 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_PHOS_4_360_830_1nm.csv (PHOS-4) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_phos_4_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_PHOS_4_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_phos_4_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_PHOS_4_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_PHOSPHOR_LED_YAG
/* ---- alwan_table_spd_illuminant_ls_phosphor_led_yag ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_PHOSPHOR_LED_YAG_360_830_1nm.csv (Phosphor LED YAG) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_phosphor_led_yag_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_PHOSPHOR_LED_YAG_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_phosphor_led_yag_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_PHOSPHOR_LED_YAG_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_60_AW_SOFT_WHITE
/* ---- alwan_table_spd_illuminant_ls_60_aw_soft_white ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_60_AW_SOFT_WHITE_360_830_1nm.csv (60 A/W (Soft White)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_60_aw_soft_white_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_60_AW_SOFT_WHITE_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_60_aw_soft_white_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_60_AW_SOFT_WHITE_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_C100S54
/* ---- alwan_table_spd_illuminant_ls_c100s54 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_C100S54_360_830_1nm.csv (C100S54 (HPS)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_c100s54_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_C100S54_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_c100s54_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_C100S54_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_C100S54C
/* ---- alwan_table_spd_illuminant_ls_c100s54c ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_C100S54C_360_830_1nm.csv (C100S54C (HPS)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_c100s54c_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_C100S54C_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_c100s54c_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_C100S54C_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_F32T8_TL830
/* ---- alwan_table_spd_illuminant_ls_f32t8_tl830 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_F32T8_TL830_360_830_1nm.csv (F32T8/TL830 (Triphosphor)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_f32t8_tl830_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F32T8_TL830_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_f32t8_tl830_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F32T8_TL830_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_F32T8_TL835
/* ---- alwan_table_spd_illuminant_ls_f32t8_tl835 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_F32T8_TL835_360_830_1nm.csv (F32T8/TL835 (Triphosphor)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_f32t8_tl835_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F32T8_TL835_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_f32t8_tl835_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F32T8_TL835_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_F32T8_TL841
/* ---- alwan_table_spd_illuminant_ls_f32t8_tl841 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_F32T8_TL841_360_830_1nm.csv (F32T8/TL841 (Triphosphor)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_f32t8_tl841_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F32T8_TL841_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_f32t8_tl841_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F32T8_TL841_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_F32T8_TL850
/* ---- alwan_table_spd_illuminant_ls_f32t8_tl850 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_F32T8_TL850_360_830_1nm.csv (F32T8/TL850 (Triphosphor)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_f32t8_tl850_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F32T8_TL850_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_f32t8_tl850_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F32T8_TL850_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_F32T8_TL865_PLUS
/* ---- alwan_table_spd_illuminant_ls_f32t8_tl865_plus ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_F32T8_TL865_PLUS_360_830_1nm.csv (F32T8/TL865/PLUS (Triphosphor)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_f32t8_tl865_plus_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F32T8_TL865_PLUS_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_f32t8_tl865_plus_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F32T8_TL865_PLUS_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_F34_CW_RS_EW
/* ---- alwan_table_spd_illuminant_ls_f34_cw_rs_ew ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_F34_CW_RS_EW_360_830_1nm.csv (F34/CW/RS/EW (Cool White FL)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_f34_cw_rs_ew_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F34_CW_RS_EW_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_f34_cw_rs_ew_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F34_CW_RS_EW_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_F34T12_LW_RS_EW
/* ---- alwan_table_spd_illuminant_ls_f34t12_lw_rs_ew ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_F34T12_LW_RS_EW_360_830_1nm.csv (F34T12/LW/RS/EW) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_f34t12_lw_rs_ew_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F34T12_LW_RS_EW_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_f34t12_lw_rs_ew_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F34T12_LW_RS_EW_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_F34T12WW_RS_EW
/* ---- alwan_table_spd_illuminant_ls_f34t12ww_rs_ew ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_F34T12WW_RS_EW_360_830_1nm.csv (F34T12WW/RS/EW (Warm White FL)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_f34t12ww_rs_ew_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F34T12WW_RS_EW_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_f34t12ww_rs_ew_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F34T12WW_RS_EW_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_F40_C50
/* ---- alwan_table_spd_illuminant_ls_f40_c50 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_F40_C50_360_830_1nm.csv (F40/C50 (Broadband FL)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_f40_c50_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F40_C50_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_f40_c50_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F40_C50_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_F40_C75
/* ---- alwan_table_spd_illuminant_ls_f40_c75 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_F40_C75_360_830_1nm.csv (F40/C75 (Broadband FL)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_f40_c75_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F40_C75_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_f40_c75_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F40_C75_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_F40_CWX
/* ---- alwan_table_spd_illuminant_ls_f40_cwx ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_F40_CWX_360_830_1nm.csv (F40/CWX (Broadband FL)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_f40_cwx_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F40_CWX_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_f40_cwx_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F40_CWX_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_F40_DX
/* ---- alwan_table_spd_illuminant_ls_f40_dx ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_F40_DX_360_830_1nm.csv (F40/DX (Broadband FL)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_f40_dx_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F40_DX_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_f40_dx_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F40_DX_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_F40_DXTP
/* ---- alwan_table_spd_illuminant_ls_f40_dxtp ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_F40_DXTP_360_830_1nm.csv (F40/DXTP (Delux FL)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_f40_dxtp_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F40_DXTP_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_f40_dxtp_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F40_DXTP_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_F40_N
/* ---- alwan_table_spd_illuminant_ls_f40_n ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_F40_N_360_830_1nm.csv (F40/N (Natural FL)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_f40_n_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F40_N_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_f40_n_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_F40_N_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_H38HT_100
/* ---- alwan_table_spd_illuminant_ls_h38ht_100 ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_H38HT_100_360_830_1nm.csv (H38HT-100 (Mercury)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_h38ht_100_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_H38HT_100_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_h38ht_100_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_H38HT_100_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_H38JA_100_DX
/* ---- alwan_table_spd_illuminant_ls_h38ja_100_dx ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_H38JA_100_DX_360_830_1nm.csv (H38JA-100/DX (Mercury DX)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_h38ja_100_dx_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_H38JA_100_DX_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_h38ja_100_dx_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_H38JA_100_DX_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_MHC100_U_MP_3K
/* ---- alwan_table_spd_illuminant_ls_mhc100_u_mp_3k ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_MHC100_U_MP_3K_360_830_1nm.csv (MHC100/U/MP/3K) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_mhc100_u_mp_3k_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_MHC100_U_MP_3K_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_mhc100_u_mp_3k_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_MHC100_U_MP_3K_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_MHC100_U_MP_4K
/* ---- alwan_table_spd_illuminant_ls_mhc100_u_mp_4k ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_MHC100_U_MP_4K_360_830_1nm.csv (MHC100/U/MP/4K) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_mhc100_u_mp_4k_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_MHC100_U_MP_4K_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_mhc100_u_mp_4k_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_MHC100_U_MP_4K_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_SDW_T_100W_LV
/* ---- alwan_table_spd_illuminant_ls_sdw_t_100w_lv ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_SDW_T_100W_LV_360_830_1nm.csv (SDW-T 100W/LV (Super HPS)) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_sdw_t_100w_lv_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_SDW_T_100W_LV_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_sdw_t_100w_lv_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_SDW_T_100W_LV_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ILLUMINANT_LS_KINOTON_75P
/* ---- alwan_table_spd_illuminant_ls_kinoton_75p ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/LS_KINOTON_75P_360_830_1nm.csv (Kinoton 75P) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_illuminant_ls_kinoton_75p_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_KINOTON_75P_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_illuminant_ls_kinoton_75p_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "illuminants/LS_KINOTON_75P_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ISO7589_PHOTOGRAPHIC_DAYLIGHT
/* ---- alwan_table_spd_iso7589_photographic_daylight ----
 * extent ALWAN_TABLE_ISO7589_350_690_10NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/iso7589/photographic_daylight_350_690_10nm.csv, as tabulated (BSD-3, colour-science) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_iso7589_photographic_daylight_f32[ALWAN_TABLE_ISO7589_350_690_10NM_SIZE] = {
#include "illuminants/iso7589/photographic_daylight_350_690_10nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_iso7589_photographic_daylight_f64[ALWAN_TABLE_ISO7589_350_690_10NM_SIZE] = {
#include "illuminants/iso7589/photographic_daylight_350_690_10nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ISO7589_SENSITOMETRIC_DAYLIGHT
/* ---- alwan_table_spd_iso7589_sensitometric_daylight ----
 * extent ALWAN_TABLE_ISO7589_350_690_10NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/iso7589/sensitometric_daylight_350_690_10nm.csv, as tabulated (BSD-3, colour-science) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_iso7589_sensitometric_daylight_f32[ALWAN_TABLE_ISO7589_350_690_10NM_SIZE] = {
#include "illuminants/iso7589/sensitometric_daylight_350_690_10nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_iso7589_sensitometric_daylight_f64[ALWAN_TABLE_ISO7589_350_690_10NM_SIZE] = {
#include "illuminants/iso7589/sensitometric_daylight_350_690_10nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ISO7589_STUDIO_TUNGSTEN
/* ---- alwan_table_spd_iso7589_studio_tungsten ----
 * extent ALWAN_TABLE_ISO7589_350_690_10NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/iso7589/studio_tungsten_350_690_10nm.csv, as tabulated (BSD-3, colour-science) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_iso7589_studio_tungsten_f32[ALWAN_TABLE_ISO7589_350_690_10NM_SIZE] = {
#include "illuminants/iso7589/studio_tungsten_350_690_10nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_iso7589_studio_tungsten_f64[ALWAN_TABLE_ISO7589_350_690_10NM_SIZE] = {
#include "illuminants/iso7589/studio_tungsten_350_690_10nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ISO7589_SENSITOMETRIC_STUDIO_TUNGSTEN
/* ---- alwan_table_spd_iso7589_sensitometric_studio_tungsten ----
 * extent ALWAN_TABLE_ISO7589_350_690_10NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/iso7589/sensitometric_studio_tungsten_350_690_10nm.csv, as tabulated (BSD-3, colour-science) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_iso7589_sensitometric_studio_tungsten_f32[ALWAN_TABLE_ISO7589_350_690_10NM_SIZE] = {
#include "illuminants/iso7589/sensitometric_studio_tungsten_350_690_10nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_iso7589_sensitometric_studio_tungsten_f64[ALWAN_TABLE_ISO7589_350_690_10NM_SIZE] = {
#include "illuminants/iso7589/sensitometric_studio_tungsten_350_690_10nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ISO7589_PHOTOFLOOD
/* ---- alwan_table_spd_iso7589_photoflood ----
 * extent ALWAN_TABLE_ISO7589_350_690_10NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/iso7589/photoflood_350_690_10nm.csv, as tabulated (BSD-3, colour-science) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_iso7589_photoflood_f32[ALWAN_TABLE_ISO7589_350_690_10NM_SIZE] = {
#include "illuminants/iso7589/photoflood_350_690_10nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_iso7589_photoflood_f64[ALWAN_TABLE_ISO7589_350_690_10NM_SIZE] = {
#include "illuminants/iso7589/photoflood_350_690_10nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ISO7589_SENSITOMETRIC_PHOTOFLOOD
/* ---- alwan_table_spd_iso7589_sensitometric_photoflood ----
 * extent ALWAN_TABLE_ISO7589_350_690_10NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/iso7589/sensitometric_photoflood_350_690_10nm.csv, as tabulated (BSD-3, colour-science) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_iso7589_sensitometric_photoflood_f32[ALWAN_TABLE_ISO7589_350_690_10NM_SIZE] = {
#include "illuminants/iso7589/sensitometric_photoflood_350_690_10nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_iso7589_sensitometric_photoflood_f64[ALWAN_TABLE_ISO7589_350_690_10NM_SIZE] = {
#include "illuminants/iso7589/sensitometric_photoflood_350_690_10nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_SPD_ISO7589_SENSITOMETRIC_PRINTER
/* ---- alwan_table_spd_iso7589_sensitometric_printer ----
 * extent ALWAN_TABLE_ISO7589_350_560_10NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: illuminants/iso7589/sensitometric_printer_350_560_10nm.csv, as tabulated (BSD-3, colour-science) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_spd_iso7589_sensitometric_printer_f32[ALWAN_TABLE_ISO7589_350_560_10NM_SIZE] = {
#include "illuminants/iso7589/sensitometric_printer_350_560_10nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_spd_iso7589_sensitometric_printer_f64[ALWAN_TABLE_ISO7589_350_560_10NM_SIZE] = {
#include "illuminants/iso7589/sensitometric_printer_350_560_10nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_LEF_JUDD_1951_PHOTOPIC
/* ---- alwan_table_lef_judd_1951_photopic ----
 * extent ALWAN_TABLE_LEF_370_770_10NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: vision/lef/judd_1951_photopic_370_770_10nm.csv (BSD-3, colour-science SDS_LEFS) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_lef_judd_1951_photopic_f32[ALWAN_TABLE_LEF_370_770_10NM_SIZE] = {
#include "vision/lef/judd_1951_photopic_370_770_10nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_lef_judd_1951_photopic_f64[ALWAN_TABLE_LEF_370_770_10NM_SIZE] = {
#include "vision/lef/judd_1951_photopic_370_770_10nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_LEF_JUDD_VOS_1978_PHOTOPIC
/* ---- alwan_table_lef_judd_vos_1978_photopic ----
 * extent ALWAN_TABLE_LEF_380_780_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: vision/lef/judd_vos_1978_photopic_380_780_1nm.csv (BSD-3, colour-science SDS_LEFS) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_lef_judd_vos_1978_photopic_f32[ALWAN_TABLE_LEF_380_780_1NM_SIZE] = {
#include "vision/lef/judd_vos_1978_photopic_380_780_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_lef_judd_vos_1978_photopic_f64[ALWAN_TABLE_LEF_380_780_1NM_SIZE] = {
#include "vision/lef/judd_vos_1978_photopic_380_780_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_LEF_CIE_1964_PHOTOPIC_10DEG
/* ---- alwan_table_lef_cie_1964_photopic_10deg ----
 * extent ALWAN_TABLE_LEF_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: vision/lef/cie_1964_photopic_10deg_360_830_1nm.csv (BSD-3, colour-science SDS_LEFS) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_lef_cie_1964_photopic_10deg_f32[ALWAN_TABLE_LEF_360_830_1NM_SIZE] = {
#include "vision/lef/cie_1964_photopic_10deg_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_lef_cie_1964_photopic_10deg_f64[ALWAN_TABLE_LEF_360_830_1NM_SIZE] = {
#include "vision/lef/cie_1964_photopic_10deg_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_LEF_CIE_2008_PHOTOPIC_2DEG
/* ---- alwan_table_lef_cie_2008_photopic_2deg ----
 * extent ALWAN_TABLE_LEF_390_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: vision/lef/cie_2008_photopic_2deg_390_830_1nm.csv (BSD-3, colour-science SDS_LEFS) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_lef_cie_2008_photopic_2deg_f32[ALWAN_TABLE_LEF_390_830_1NM_SIZE] = {
#include "vision/lef/cie_2008_photopic_2deg_390_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_lef_cie_2008_photopic_2deg_f64[ALWAN_TABLE_LEF_390_830_1NM_SIZE] = {
#include "vision/lef/cie_2008_photopic_2deg_390_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_LEF_CIE_2008_PHOTOPIC_10DEG
/* ---- alwan_table_lef_cie_2008_photopic_10deg ----
 * extent ALWAN_TABLE_LEF_390_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: vision/lef/cie_2008_photopic_10deg_390_830_1nm.csv (BSD-3, colour-science SDS_LEFS) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_lef_cie_2008_photopic_10deg_f32[ALWAN_TABLE_LEF_390_830_1NM_SIZE] = {
#include "vision/lef/cie_2008_photopic_10deg_390_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_lef_cie_2008_photopic_10deg_f64[ALWAN_TABLE_LEF_390_830_1NM_SIZE] = {
#include "vision/lef/cie_2008_photopic_10deg_390_830_1nm.csv"
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_cmf_cie_1964_10deg_z_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/cie_1964_10deg_z_360_830_1nm.csv"
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_cmf_wright_guild_1931_b_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/wright_guild_1931_b_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_CMF_STOCKMAN_SHARPE_10DEG
/* ---- alwan_table_cmf_stockman_sharpe_10deg_x ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/stockman_sharpe_10deg_x_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_stockman_sharpe_10deg_x_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stockman_sharpe_10deg_x_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_cmf_stockman_sharpe_10deg_x_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stockman_sharpe_10deg_x_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_stockman_sharpe_10deg_y ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/stockman_sharpe_10deg_y_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_stockman_sharpe_10deg_y_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stockman_sharpe_10deg_y_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_cmf_stockman_sharpe_10deg_y_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stockman_sharpe_10deg_y_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_stockman_sharpe_10deg_z ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/stockman_sharpe_10deg_z_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_stockman_sharpe_10deg_z_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stockman_sharpe_10deg_z_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_cmf_stockman_sharpe_10deg_z_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stockman_sharpe_10deg_z_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_CMF_SMITH_POKORNY_1975
/* ---- alwan_table_cmf_smith_pokorny_1975_x ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/smith_pokorny_1975_x_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_smith_pokorny_1975_x_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/smith_pokorny_1975_x_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_cmf_smith_pokorny_1975_x_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/smith_pokorny_1975_x_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_smith_pokorny_1975_y ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/smith_pokorny_1975_y_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_smith_pokorny_1975_y_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/smith_pokorny_1975_y_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_cmf_smith_pokorny_1975_y_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/smith_pokorny_1975_y_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_smith_pokorny_1975_z ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/smith_pokorny_1975_z_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_smith_pokorny_1975_z_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/smith_pokorny_1975_z_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_cmf_smith_pokorny_1975_z_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/smith_pokorny_1975_z_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_CMF_STILES_BURCH_1955_2DEG
/* ---- alwan_table_cmf_stiles_burch_1955_2deg_r ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/stiles_burch_1955_2deg_r_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_stiles_burch_1955_2deg_r_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stiles_burch_1955_2deg_r_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_cmf_stiles_burch_1955_2deg_r_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stiles_burch_1955_2deg_r_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_stiles_burch_1955_2deg_g ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/stiles_burch_1955_2deg_g_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_stiles_burch_1955_2deg_g_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stiles_burch_1955_2deg_g_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_cmf_stiles_burch_1955_2deg_g_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stiles_burch_1955_2deg_g_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_stiles_burch_1955_2deg_b ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/stiles_burch_1955_2deg_b_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_stiles_burch_1955_2deg_b_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stiles_burch_1955_2deg_b_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_cmf_stiles_burch_1955_2deg_b_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stiles_burch_1955_2deg_b_360_830_1nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_CMF_STILES_BURCH_1959_10DEG
/* ---- alwan_table_cmf_stiles_burch_1959_10deg_r ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/stiles_burch_1959_10deg_r_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_stiles_burch_1959_10deg_r_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stiles_burch_1959_10deg_r_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_cmf_stiles_burch_1959_10deg_r_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stiles_burch_1959_10deg_r_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_stiles_burch_1959_10deg_g ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/stiles_burch_1959_10deg_g_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_stiles_burch_1959_10deg_g_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stiles_burch_1959_10deg_g_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_cmf_stiles_burch_1959_10deg_g_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stiles_burch_1959_10deg_g_360_830_1nm.csv"
};
#endif

/* ---- alwan_table_cmf_stiles_burch_1959_10deg_b ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: cmf/stiles_burch_1959_10deg_b_360_830_1nm.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_cmf_stiles_burch_1959_10deg_b_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stiles_burch_1959_10deg_b_360_830_1nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_cmf_stiles_burch_1959_10deg_b_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "cmf/stiles_burch_1959_10deg_b_360_830_1nm.csv"
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
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
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_mallett2019_blue_f64[ALWAN_TABLE_MALLETT2019_SIZE] = {
#include "spectral_basis/mallett2019/blue.csv"
};
#endif

#endif

#if ALWAN_TABLE_OTSU2018
/* ---- alwan_table_otsu2018_basis ----
 * extent ALWAN_TABLE_OTSU2018_BASIS_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: spectral_basis/otsu2018/basis.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_otsu2018_basis_f32[ALWAN_TABLE_OTSU2018_BASIS_SIZE] = {
#include "spectral_basis/otsu2018/basis.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_otsu2018_basis_f64[ALWAN_TABLE_OTSU2018_BASIS_SIZE] = {
#include "spectral_basis/otsu2018/basis.csv"
};
#endif

/* ---- alwan_table_otsu2018_means ----
 * extent ALWAN_TABLE_OTSU2018_MEANS_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: spectral_basis/otsu2018/means.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_otsu2018_means_f32[ALWAN_TABLE_OTSU2018_MEANS_SIZE] = {
#include "spectral_basis/otsu2018/means.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_otsu2018_means_f64[ALWAN_TABLE_OTSU2018_MEANS_SIZE] = {
#include "spectral_basis/otsu2018/means.csv"
};
#endif

/* ---- alwan_table_otsu2018_selector ----
 * extent ALWAN_TABLE_OTSU2018_SELECTOR_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: spectral_basis/otsu2018/selector.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_otsu2018_selector_f32[ALWAN_TABLE_OTSU2018_SELECTOR_SIZE] = {
#include "spectral_basis/otsu2018/selector.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_otsu2018_selector_f64[ALWAN_TABLE_OTSU2018_SELECTOR_SIZE] = {
#include "spectral_basis/otsu2018/selector.csv"
};
#endif

/* ---- alwan_table_otsu2018_m_inverse ----
 * extent ALWAN_TABLE_OTSU2018_M_INVERSE_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: spectral_basis/otsu2018/m_inverse.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_otsu2018_m_inverse_f32[ALWAN_TABLE_OTSU2018_M_INVERSE_SIZE] = {
#include "spectral_basis/otsu2018/m_inverse.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_otsu2018_m_inverse_f64[ALWAN_TABLE_OTSU2018_M_INVERSE_SIZE] = {
#include "spectral_basis/otsu2018/m_inverse.csv"
};
#endif

/* ---- alwan_table_otsu2018_xyz_mu ----
 * extent ALWAN_TABLE_OTSU2018_XYZ_MU_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: spectral_basis/otsu2018/xyz_mu.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_otsu2018_xyz_mu_f32[ALWAN_TABLE_OTSU2018_XYZ_MU_SIZE] = {
#include "spectral_basis/otsu2018/xyz_mu.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_otsu2018_xyz_mu_f64[ALWAN_TABLE_OTSU2018_XYZ_MU_SIZE] = {
#include "spectral_basis/otsu2018/xyz_mu.csv"
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
#if ALWAN_WITH_F64_FACADE
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

#if ALWAN_TABLE_ACES_RICD
/* ---- alwan_table_aces_ricd_r / _g / _b ----
 * extent ALWAN_TABLE_SPD_360_830_1NM_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: camera_sensitivities/aces_ricd_{r,g,b}.csv */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_aces_ricd_r_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "camera_sensitivities/aces_ricd_r.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_aces_ricd_r_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "camera_sensitivities/aces_ricd_r.csv"
};
#endif
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_aces_ricd_g_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "camera_sensitivities/aces_ricd_g.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_aces_ricd_g_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "camera_sensitivities/aces_ricd_g.csv"
};
#endif
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_aces_ricd_b_f32[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "camera_sensitivities/aces_ricd_b.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_aces_ricd_b_f64[ALWAN_TABLE_SPD_360_830_1NM_SIZE] = {
#include "camera_sensitivities/aces_ricd_b.csv"
};
#endif

#endif

#if ALWAN_TABLE_CAMERA_RAWTOACES
/* ---- alwan_table_camera_rawtoaces ----
 * extent ALWAN_TABLE_CAMERA_RAWTOACES_SIZE. Reader: alwan_table2d_row_at_{f32,f64}
 * Source: camera_sensitivities/rawtoaces/sensitivities.csv (Apache-2.0) */
alwan_f64 const alwan_table_camera_rawtoaces_f64[ALWAN_TABLE_CAMERA_RAWTOACES_SIZE] = {
#include "camera_sensitivities/rawtoaces/sensitivities.csv"
};

#endif

#if ALWAN_TABLE_IDT_TRAINING_190
/* ---- alwan_table_idt_training_190 ----
 * extent ALWAN_TABLE_IDT_TRAINING_SIZE. Reader: alwan_table2d_row_at_{f32,f64}
 * Source: camera_sensitivities/rawtoaces/training_190.csv (Apache-2.0) */
alwan_f64 const alwan_table_idt_training_190_f64[ALWAN_TABLE_IDT_TRAINING_SIZE] = {
#include "camera_sensitivities/rawtoaces/training_190.csv"
};

#endif

#if ALWAN_TABLE_ISO7589_TUNGSTEN
/* ---- alwan_table_iso7589_tungsten ----
 * extent ALWAN_TABLE_RAWTOACES_BANDS. Reader: alwan_table1d_row_{f32,f64}
 * Source: camera_sensitivities/rawtoaces/iso7589_tungsten.csv (Apache-2.0) */
alwan_f64 const alwan_table_iso7589_tungsten_f64[ALWAN_TABLE_RAWTOACES_BANDS] = {
#include "camera_sensitivities/rawtoaces/iso7589_tungsten.csv"
};

#endif

#if ALWAN_TABLE_CAMERA_BASIS
/* ---- alwan_table_camera_basis_rawtoaces ----
 * extent ALWAN_TABLE_CAMERA_BASIS_SIZE. Reader: alwan_table1d_row_{f32,f64}
 * Source: camera_sensitivities/rawtoaces/basis_pca6.csv (derived from Apache-2.0 data) */
alwan_f64 const alwan_table_camera_basis_rawtoaces_f64[ALWAN_TABLE_CAMERA_BASIS_SIZE] = {
#include "camera_sensitivities/rawtoaces/basis_pca6.csv"
};

#endif


#if ALWAN_TABLE_FILM_STATUS_A
/* ---- alwan_table_film_status_a ----
 * extent ALWAN_TABLE_FILM_41X3_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/status_a_380_780_10nm.csv, ISO 5-3 Status A responsivities, columns normalised to sum 1 (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_status_a_f32[ALWAN_TABLE_FILM_41X3_SIZE] = {
#include "film/status_a_380_780_10nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_status_a_f64[ALWAN_TABLE_FILM_41X3_SIZE] = {
#include "film/status_a_380_780_10nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_FILM_STATUS_M
/* ---- alwan_table_film_status_m ----
 * extent ALWAN_TABLE_FILM_41X3_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/status_m_380_780_10nm.csv, ISO 5-3 Status M responsivities, columns normalised to sum 1 (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_status_m_f32[ALWAN_TABLE_FILM_41X3_SIZE] = {
#include "film/status_m_380_780_10nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_status_m_f64[ALWAN_TABLE_FILM_41X3_SIZE] = {
#include "film/status_m_380_780_10nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_FILM_APD
/* ---- alwan_table_film_apd ----
 * extent ALWAN_TABLE_FILM_41X3_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/apd_380_780_10nm.csv, ACES printing density responsivities, columns normalised to sum 1 (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_apd_f32[ALWAN_TABLE_FILM_41X3_SIZE] = {
#include "film/apd_380_780_10nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_apd_f64[ALWAN_TABLE_FILM_41X3_SIZE] = {
#include "film/apd_380_780_10nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_FILM_PRINTER_LIGHTS
/* ---- alwan_table_film_printer_lights ----
 * extent ALWAN_TABLE_FILM_41X3_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/printer_lights_380_780_10nm.csv, red, green and blue printer lights split from the APD printer light (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_printer_lights_f32[ALWAN_TABLE_FILM_41X3_SIZE] = {
#include "film/printer_lights_380_780_10nm.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_printer_lights_f64[ALWAN_TABLE_FILM_41X3_SIZE] = {
#include "film/printer_lights_380_780_10nm.csv"
};
#endif

#endif

#if ALWAN_TABLE_FILM_KODAK_5203
/* ---- alwan_table_film_kodak_5203 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_5203.csv, Kodak Vision3 50D 5203 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_5203_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5203.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_5203_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5203.csv"
};
#endif

#endif

#if ALWAN_TABLE_FILM_KODAK_5207
/* ---- alwan_table_film_kodak_5207 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_5207.csv, Kodak Vision3 250D 5207 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_5207_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5207.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_5207_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5207.csv"
};
#endif

#endif

#if ALWAN_TABLE_FILM_KODAK_5213
/* ---- alwan_table_film_kodak_5213 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_5213.csv, Kodak Vision3 200T 5213 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_5213_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5213.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_5213_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5213.csv"
};
#endif

#endif

#if ALWAN_TABLE_FILM_KODAK_5219
/* ---- alwan_table_film_kodak_5219 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_5219.csv, Kodak Vision3 500T 5219 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_5219_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5219.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_5219_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5219.csv"
};
#endif

#endif

#if ALWAN_TABLE_FILM_KODAK_PORTRA_400
/* ---- alwan_table_film_kodak_portra_400 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_PORTRA_400.csv, Kodak Portra 400 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_portra_400_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_PORTRA_400.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_portra_400_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_PORTRA_400.csv"
};
#endif

#endif

#if ALWAN_TABLE_FILM_KODAK_EKTAR_100
/* ---- alwan_table_film_kodak_ektar_100 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_EKTAR_100.csv, Kodak Ektar 100 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_ektar_100_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_EKTAR_100.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_ektar_100_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_EKTAR_100.csv"
};
#endif

#endif

#if ALWAN_TABLE_FILM_FUJI_ETERNA_500
/* ---- alwan_table_film_fuji_eterna_500 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJI_ETERNA_500.csv, Fuji Eterna 500 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fuji_eterna_500_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_ETERNA_500.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fuji_eterna_500_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_ETERNA_500.csv"
};
#endif

#endif

#if ALWAN_TABLE_FILM_KODAK_5222
/* ---- alwan_table_film_kodak_5222 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_5222.csv, Kodak 5222 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_5222_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5222.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_5222_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5222.csv"
};
#endif

#endif

#if ALWAN_TABLE_FILM_KODAK_2383
/* ---- alwan_table_film_kodak_2383 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_2383.csv, Kodak Vision 2383 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_2383_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_2383.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_2383_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_2383.csv"
};
#endif

#endif

#if ALWAN_TABLE_FILM_KODAK_2393
/* ---- alwan_table_film_kodak_2393 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_2393.csv, Kodak Vision Premier 2393 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_2393_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_2393.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_2393_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_2393.csv"
};
#endif

#endif

#if ALWAN_TABLE_FILM_FUJI_3513DI
/* ---- alwan_table_film_fuji_3513di ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJI_3513DI.csv, Fuji Eterna-CP Type 3513DI profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fuji_3513di_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_3513DI.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fuji_3513di_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_3513DI.csv"
};
#endif

#endif

#if ALWAN_TABLE_FILM_KODAK_2302
/* ---- alwan_table_film_kodak_2302 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_2302.csv, Kodak 2302 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_2302_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_2302.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_2302_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_2302.csv"
};
#endif

#endif

#if ALWAN_TABLE_FILM_KODAK_EKTACHROME_100D
/* ---- alwan_table_film_kodak_ektachrome_100d ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_EKTACHROME_100D.csv, Kodak Ektachrome 100D profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_ektachrome_100d_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_EKTACHROME_100D.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_ektachrome_100d_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_EKTACHROME_100D.csv"
};
#endif

#endif

#if ALWAN_TABLE_FILM_FUJI_VELVIA_50
/* ---- alwan_table_film_fuji_velvia_50 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJI_VELVIA_50.csv, Fuji Velvia 50 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fuji_velvia_50_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_VELVIA_50.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fuji_velvia_50_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_VELVIA_50.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_AGFA_VISTA_100
/* ---- alwan_table_film_agfa_vista_100 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/AGFA_VISTA_100.csv, Agfa Vista 100 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_agfa_vista_100_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/AGFA_VISTA_100.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_agfa_vista_100_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/AGFA_VISTA_100.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_FUJI_C200
/* ---- alwan_table_film_fuji_c200 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJI_C200.csv, Fuji C200 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fuji_c200_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_C200.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fuji_c200_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_C200.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_FUJI_ETERNA_500_VIVID
/* ---- alwan_table_film_fuji_eterna_500_vivid ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJI_ETERNA_500_VIVID.csv, Fuji Eterna 500 Vivid profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fuji_eterna_500_vivid_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_ETERNA_500_VIVID.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fuji_eterna_500_vivid_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_ETERNA_500_VIVID.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_FUJI_NATURA_1600
/* ---- alwan_table_film_fuji_natura_1600 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJI_NATURA_1600.csv, Fuji Natura 1600 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fuji_natura_1600_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_NATURA_1600.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fuji_natura_1600_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_NATURA_1600.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_FUJI_PRO_160C
/* ---- alwan_table_film_fuji_pro_160c ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJI_PRO_160C.csv, Fuji Pro 160C profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fuji_pro_160c_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_PRO_160C.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fuji_pro_160c_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_PRO_160C.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_FUJI_PRO_160S
/* ---- alwan_table_film_fuji_pro_160s ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJI_PRO_160S.csv, Fuji Pro 160S profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fuji_pro_160s_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_PRO_160S.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fuji_pro_160s_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_PRO_160S.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_FUJI_PRO_400H
/* ---- alwan_table_film_fuji_pro_400h ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJI_PRO_400H.csv, Fuji Pro 400H profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fuji_pro_400h_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_PRO_400H.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fuji_pro_400h_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_PRO_400H.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_FUJI_SUPERIA_REALA
/* ---- alwan_table_film_fuji_superia_reala ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJI_SUPERIA_REALA.csv, Fuji Superia Reala profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fuji_superia_reala_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_SUPERIA_REALA.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fuji_superia_reala_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_SUPERIA_REALA.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_FUJI_SUPERIA_XTRA_400
/* ---- alwan_table_film_fuji_superia_xtra_400 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJI_SUPERIA_XTRA_400.csv, Fuji Superia X-Tra 400 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fuji_superia_xtra_400_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_SUPERIA_XTRA_400.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fuji_superia_xtra_400_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_SUPERIA_XTRA_400.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_5206
/* ---- alwan_table_film_kodak_5206 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_5206.csv, Kodak Verita 200D 5206 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_5206_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5206.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_5206_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5206.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_5247
/* ---- alwan_table_film_kodak_5247 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_5247.csv, Kodak 5247 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_5247_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5247.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_5247_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5247.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_5247_II
/* ---- alwan_table_film_kodak_5247_ii ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_5247_II.csv, Kodak 5247 II profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_5247_ii_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5247_II.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_5247_ii_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5247_II.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_5247_II_ALT
/* ---- alwan_table_film_kodak_5247_ii_alt ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_5247_II_ALT.csv, Kodak 5247 II Alt profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_5247_ii_alt_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5247_II_ALT.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_5247_ii_alt_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5247_II_ALT.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_5248
/* ---- alwan_table_film_kodak_5248 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_5248.csv, Kodak 5248 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_5248_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5248.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_5248_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5248.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_EXR_5248
/* ---- alwan_table_film_kodak_exr_5248 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_EXR_5248.csv, Kodak EXR 100T 5248 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_exr_5248_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_EXR_5248.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_exr_5248_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_EXR_5248.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_5250
/* ---- alwan_table_film_kodak_5250 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_5250.csv, Kodak 5250 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_5250_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5250.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_5250_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5250.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_5277
/* ---- alwan_table_film_kodak_5277 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_5277.csv, Kodak Vision 320T 5277 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_5277_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5277.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_5277_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5277.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_5293
/* ---- alwan_table_film_kodak_5293 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_5293.csv, Kodak EXR 200T 5293 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_5293_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5293.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_5293_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5293.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_AEROCOLOR
/* ---- alwan_table_film_kodak_aerocolor ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_AEROCOLOR.csv, Kodak Aerocolor IV 2460 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_aerocolor_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_AEROCOLOR.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_aerocolor_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_AEROCOLOR.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_AEROCOLOR_LOW
/* ---- alwan_table_film_kodak_aerocolor_low ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_AEROCOLOR_LOW.csv, Kodak Aerocolor IV 2460 Low profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_aerocolor_low_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_AEROCOLOR_LOW.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_aerocolor_low_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_AEROCOLOR_LOW.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_AEROCOLOR_HIGH
/* ---- alwan_table_film_kodak_aerocolor_high ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_AEROCOLOR_HIGH.csv, Kodak Aerocolor IV 2460 High profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_aerocolor_high_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_AEROCOLOR_HIGH.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_aerocolor_high_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_AEROCOLOR_HIGH.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_GOLD_200
/* ---- alwan_table_film_kodak_gold_200 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_GOLD_200.csv, Kodak Gold 200 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_gold_200_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_GOLD_200.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_gold_200_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_GOLD_200.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_PORTRA_160
/* ---- alwan_table_film_kodak_portra_160 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_PORTRA_160.csv, Kodak Portra 160 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_portra_160_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_PORTRA_160.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_portra_160_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_PORTRA_160.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_PORTRA_800
/* ---- alwan_table_film_kodak_portra_800 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_PORTRA_800.csv, Kodak Portra 800 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_portra_800_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_PORTRA_800.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_portra_800_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_PORTRA_800.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_PORTRA_800_AT_1600
/* ---- alwan_table_film_kodak_portra_800_at_1600 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_PORTRA_800_AT_1600.csv, Kodak Portra 800 @1600 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_portra_800_at_1600_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_PORTRA_800_AT_1600.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_portra_800_at_1600_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_PORTRA_800_AT_1600.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_PORTRA_800_AT_3200
/* ---- alwan_table_film_kodak_portra_800_at_3200 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_PORTRA_800_AT_3200.csv, Kodak Portra 800 @3200 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_portra_800_at_3200_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_PORTRA_800_AT_3200.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_portra_800_at_3200_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_PORTRA_800_AT_3200.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_ULTRAMAX_400
/* ---- alwan_table_film_kodak_ultramax_400 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_ULTRAMAX_400.csv, Kodak Ultramax 400 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_ultramax_400_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_ULTRAMAX_400.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_ultramax_400_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_ULTRAMAX_400.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_VERICOLOR_III
/* ---- alwan_table_film_kodak_vericolor_iii ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_VERICOLOR_III.csv, Kodak Vericolor III profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_vericolor_iii_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_VERICOLOR_III.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_vericolor_iii_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_VERICOLOR_III.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_5222_DEV_4
/* ---- alwan_table_film_kodak_5222_dev_4 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_5222_DEV_4.csv, Kodak 5222 Dev 4 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_5222_dev_4_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5222_DEV_4.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_5222_dev_4_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5222_DEV_4.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_5222_DEV_5
/* ---- alwan_table_film_kodak_5222_dev_5 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_5222_DEV_5.csv, Kodak 5222 Dev 5 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_5222_dev_5_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5222_DEV_5.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_5222_dev_5_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5222_DEV_5.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_5222_DEV_9
/* ---- alwan_table_film_kodak_5222_dev_9 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_5222_DEV_9.csv, Kodak 5222 Dev 9 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_5222_dev_9_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5222_DEV_9.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_5222_dev_9_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5222_DEV_9.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_5222_DEV_12
/* ---- alwan_table_film_kodak_5222_dev_12 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_5222_DEV_12.csv, Kodak 5222 Dev 12 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_5222_dev_12_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5222_DEV_12.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_5222_dev_12_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5222_DEV_12.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_TRI_X_400
/* ---- alwan_table_film_kodak_tri_x_400 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_TRI_X_400.csv, Kodak Tri-X 400 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_tri_x_400_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_TRI_X_400.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_tri_x_400_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_TRI_X_400.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_TRI_X_400_DEV_7
/* ---- alwan_table_film_kodak_tri_x_400_dev_7 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_TRI_X_400_DEV_7.csv, Kodak Trix-X 400 Dev 7 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_tri_x_400_dev_7_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_TRI_X_400_DEV_7.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_tri_x_400_dev_7_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_TRI_X_400_DEV_7.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_TRI_X_400_DEV_9
/* ---- alwan_table_film_kodak_tri_x_400_dev_9 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_TRI_X_400_DEV_9.csv, Kodak Trix-X 400 Dev 9 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_tri_x_400_dev_9_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_TRI_X_400_DEV_9.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_tri_x_400_dev_9_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_TRI_X_400_DEV_9.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_TRI_X_400_DEV_11
/* ---- alwan_table_film_kodak_tri_x_400_dev_11 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_TRI_X_400_DEV_11.csv, Kodak Trix-X 400 Dev 11 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_tri_x_400_dev_11_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_TRI_X_400_DEV_11.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_tri_x_400_dev_11_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_TRI_X_400_DEV_11.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_FUJI_3523XD
/* ---- alwan_table_film_fuji_3523xd ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJI_3523XD.csv, Fuji Eterna-CP 3523XD profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fuji_3523xd_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_3523XD.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fuji_3523xd_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_3523XD.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_FUJI_CA_DPII
/* ---- alwan_table_film_fuji_ca_dpii ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJI_CA_DPII.csv, Fuji Crystal Archive DPII profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fuji_ca_dpii_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_CA_DPII.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fuji_ca_dpii_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_CA_DPII.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_FUJI_CA_MAXIMA
/* ---- alwan_table_film_fuji_ca_maxima ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJI_CA_MAXIMA.csv, Fuji Crystal Archive Maxima profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fuji_ca_maxima_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_CA_MAXIMA.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fuji_ca_maxima_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_CA_MAXIMA.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_FUJI_CA_PRO_PDII
/* ---- alwan_table_film_fuji_ca_pro_pdii ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJI_CA_PRO_PDII.csv, Fuji Crystal Archive Pro PDII profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fuji_ca_pro_pdii_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_CA_PRO_PDII.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fuji_ca_pro_pdii_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_CA_PRO_PDII.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_FUJI_CA_SUPER_C
/* ---- alwan_table_film_fuji_ca_super_c ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJI_CA_SUPER_C.csv, Fuji Crystal Archive Super Type C profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fuji_ca_super_c_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_CA_SUPER_C.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fuji_ca_super_c_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_CA_SUPER_C.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_FUJIFLEX_NEW
/* ---- alwan_table_film_fujiflex_new ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJIFLEX_NEW.csv, Fujiflex Crystal Archive New Version profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fujiflex_new_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJIFLEX_NEW.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fujiflex_new_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJIFLEX_NEW.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_FUJIFLEX_OLD
/* ---- alwan_table_film_fujiflex_old ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJIFLEX_OLD.csv, Fujiflex Crystal Archive Old Version profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fujiflex_old_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJIFLEX_OLD.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fujiflex_old_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJIFLEX_OLD.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_5381
/* ---- alwan_table_film_kodak_5381 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_5381.csv, Kodak 5381 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_5381_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5381.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_5381_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5381.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_5383
/* ---- alwan_table_film_kodak_5383 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_5383.csv, Kodak 5383 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_5383_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5383.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_5383_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5383.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_5384
/* ---- alwan_table_film_kodak_5384 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_5384.csv, Kodak 5384 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_5384_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5384.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_5384_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_5384.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_DURAFLEX_PLUS
/* ---- alwan_table_film_kodak_duraflex_plus ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_DURAFLEX_PLUS.csv, Kodak Duraflex Plus profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_duraflex_plus_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_DURAFLEX_PLUS.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_duraflex_plus_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_DURAFLEX_PLUS.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_ENDURA_PREMIER
/* ---- alwan_table_film_kodak_endura_premier ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_ENDURA_PREMIER.csv, Kodak Endura Premier Paper profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_endura_premier_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_ENDURA_PREMIER.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_endura_premier_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_ENDURA_PREMIER.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_EXR_5386
/* ---- alwan_table_film_kodak_exr_5386 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_EXR_5386.csv, Kodak EXR 5386 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_exr_5386_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_EXR_5386.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_exr_5386_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_EXR_5386.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_PORTRA_ENDURA
/* ---- alwan_table_film_kodak_portra_endura ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_PORTRA_ENDURA.csv, Kodak Portra Endura Paper profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_portra_endura_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_PORTRA_ENDURA.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_portra_endura_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_PORTRA_ENDURA.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_SUPRA_ENDURA
/* ---- alwan_table_film_kodak_supra_endura ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_SUPRA_ENDURA.csv, Kodak Supra Endura Paper profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_supra_endura_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_SUPRA_ENDURA.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_supra_endura_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_SUPRA_ENDURA.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_2302_DEV_2
/* ---- alwan_table_film_kodak_2302_dev_2 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_2302_DEV_2.csv, Kodak 2302 Dev 2 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_2302_dev_2_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_2302_DEV_2.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_2302_dev_2_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_2302_DEV_2.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_2302_DEV_3
/* ---- alwan_table_film_kodak_2302_dev_3 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_2302_DEV_3.csv, Kodak 2302 Dev 3 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_2302_dev_3_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_2302_DEV_3.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_2302_dev_3_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_2302_DEV_3.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_2302_DEV_5
/* ---- alwan_table_film_kodak_2302_dev_5 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_2302_DEV_5.csv, Kodak 2302 Dev 5 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_2302_dev_5_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_2302_DEV_5.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_2302_dev_5_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_2302_DEV_5.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_2302_DEV_7
/* ---- alwan_table_film_kodak_2302_dev_7 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_2302_DEV_7.csv, Kodak 2302 Dev 7 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_2302_dev_7_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_2302_DEV_7.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_2302_dev_7_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_2302_DEV_7.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_2302_DEV_9
/* ---- alwan_table_film_kodak_2302_dev_9 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_2302_DEV_9.csv, Kodak 2302 Dev 9 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_2302_dev_9_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_2302_DEV_9.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_2302_dev_9_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_2302_DEV_9.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_POLYMAX
/* ---- alwan_table_film_kodak_polymax ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_POLYMAX.csv, Kodak Professional Polymax Fine-Art Paper profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_polymax_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_POLYMAX.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_polymax_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_POLYMAX.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_POLYMAX_GRADE_MINUS_1
/* ---- alwan_table_film_kodak_polymax_grade_minus_1 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_POLYMAX_GRADE_MINUS_1.csv, Kodak Polymax Fine-Art Paper Grade -1 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_polymax_grade_minus_1_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_POLYMAX_GRADE_MINUS_1.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_polymax_grade_minus_1_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_POLYMAX_GRADE_MINUS_1.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_POLYMAX_GRADE_0
/* ---- alwan_table_film_kodak_polymax_grade_0 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_POLYMAX_GRADE_0.csv, Kodak Polymax Fine-Art Paper Grade 0 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_polymax_grade_0_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_POLYMAX_GRADE_0.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_polymax_grade_0_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_POLYMAX_GRADE_0.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_POLYMAX_GRADE_1
/* ---- alwan_table_film_kodak_polymax_grade_1 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_POLYMAX_GRADE_1.csv, Kodak Polymax Fine-Art Paper Grade 1 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_polymax_grade_1_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_POLYMAX_GRADE_1.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_polymax_grade_1_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_POLYMAX_GRADE_1.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_POLYMAX_GRADE_2
/* ---- alwan_table_film_kodak_polymax_grade_2 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_POLYMAX_GRADE_2.csv, Kodak Polymax Fine-Art Paper Grade 2 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_polymax_grade_2_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_POLYMAX_GRADE_2.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_polymax_grade_2_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_POLYMAX_GRADE_2.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_POLYMAX_GRADE_3
/* ---- alwan_table_film_kodak_polymax_grade_3 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_POLYMAX_GRADE_3.csv, Kodak Polymax Fine-Art Paper Grade 3 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_polymax_grade_3_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_POLYMAX_GRADE_3.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_polymax_grade_3_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_POLYMAX_GRADE_3.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_POLYMAX_GRADE_4
/* ---- alwan_table_film_kodak_polymax_grade_4 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_POLYMAX_GRADE_4.csv, Kodak Polymax Fine-Art Paper Grade 4 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_polymax_grade_4_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_POLYMAX_GRADE_4.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_polymax_grade_4_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_POLYMAX_GRADE_4.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_POLYMAX_GRADE_5
/* ---- alwan_table_film_kodak_polymax_grade_5 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_POLYMAX_GRADE_5.csv, Kodak Polymax Fine-Art Paper Grade 5 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_polymax_grade_5_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_POLYMAX_GRADE_5.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_polymax_grade_5_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_POLYMAX_GRADE_5.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_FUJI_FP100C
/* ---- alwan_table_film_fuji_fp100c ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJI_FP100C.csv, Fuji FP-100C profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fuji_fp100c_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_FP100C.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fuji_fp100c_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_FP100C.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_FUJI_INSTAX_COLOR
/* ---- alwan_table_film_fuji_instax_color ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJI_INSTAX_COLOR.csv, Fuji Instax color profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fuji_instax_color_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_INSTAX_COLOR.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fuji_instax_color_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_INSTAX_COLOR.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_FUJI_PROVIA_100F
/* ---- alwan_table_film_fuji_provia_100f ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/FUJI_PROVIA_100F.csv, Fuji Provia 100F profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_fuji_provia_100f_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_PROVIA_100F.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_fuji_provia_100f_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/FUJI_PROVIA_100F.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODACHROME_64
/* ---- alwan_table_film_kodachrome_64 ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODACHROME_64.csv, Kodachrome 64 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodachrome_64_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODACHROME_64.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodachrome_64_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODACHROME_64.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_AEROCHROME_III
/* ---- alwan_table_film_kodak_aerochrome_iii ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_AEROCHROME_III.csv, Kodak Aerochrome III Infrared Film 1443 profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_aerochrome_iii_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_AEROCHROME_III.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_aerochrome_iii_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_AEROCHROME_III.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_EKTACHROME_100D_ALT
/* ---- alwan_table_film_kodak_ektachrome_100d_alt ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_EKTACHROME_100D_ALT.csv, Kodak Ektachrome 100D alt. profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_ektachrome_100d_alt_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_EKTACHROME_100D_ALT.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_ektachrome_100d_alt_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_EKTACHROME_100D_ALT.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_ILFOCHROME_MICROGRAPHIC_M
/* ---- alwan_table_film_ilfochrome_micrographic_m ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/ILFOCHROME_MICROGRAPHIC_M.csv, Ilfochrome Micrographic M profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_ilfochrome_micrographic_m_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/ILFOCHROME_MICROGRAPHIC_M.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_ilfochrome_micrographic_m_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/ILFOCHROME_MICROGRAPHIC_M.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_ILFOCHROME_MICROGRAPHIC_P
/* ---- alwan_table_film_ilfochrome_micrographic_p ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/ILFOCHROME_MICROGRAPHIC_P.csv, Ilfochrome Micrographic P profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_ilfochrome_micrographic_p_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/ILFOCHROME_MICROGRAPHIC_P.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_ilfochrome_micrographic_p_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/ILFOCHROME_MICROGRAPHIC_P.csv"
};
#endif

#endif
#if ALWAN_TABLE_FILM_KODAK_EKTACHROME_RADIANCE_III
/* ---- alwan_table_film_kodak_ektachrome_radiance_iii ----
 * extent ALWAN_TABLE_FILM_STOCK_SIZE. Reader: api/alwan_film_impl.inc, direct index
 * Source: film/KODAK_EKTACHROME_RADIANCE_III.csv, Kodak Ektachrome Radiance III Paper profiled (MIT, spectral_film_lut) */
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_film_kodak_ektachrome_radiance_iii_f32[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_EKTACHROME_RADIANCE_III.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_film_kodak_ektachrome_radiance_iii_f64[ALWAN_TABLE_FILM_STOCK_SIZE] = {
#include "film/KODAK_EKTACHROME_RADIANCE_III.csv"
};
#endif

#endif


ALWAN_DIAG_POP
