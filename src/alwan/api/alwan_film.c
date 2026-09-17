/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Film: the profiled stocks and the negative, print and projection pipeline.
 * See the film section of alwan.h. The stock enum's names live here so a stock
 * compiled out still has one.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_table_core.h"
#include "../data/alwan_data_tables.h"

alwan_status alwan_film_stock_info(alwan_film_stock stock, char const **name, char const **manufacturer) {
    /* One static row per stock, the same strings the profile carries. */
    static char const *const k_names[ALWAN_FILM_STOCK_COUNT][2] = {
        { "Kodak Vision3 50D 5203", "Kodak" },
        { "Kodak Vision3 250D 5207", "Kodak" },
        { "Kodak Vision3 200T 5213", "Kodak" },
        { "Kodak Vision3 500T 5219", "Kodak" },
        { "Kodak Portra 400", "Kodak" },
        { "Kodak Ektar 100", "Kodak" },
        { "Fuji Eterna 500", "Fujifilm" },
        { "Kodak 5222", "Kodak" },
        { "Kodak Vision 2383", "Kodak" },
        { "Kodak Vision Premier 2393", "Kodak" },
        { "Fuji Eterna-CP Type 3513DI", "Fujifilm" },
        { "Kodak 2302", "Kodak" },
        { "Kodak Ektachrome 100D", "Kodak" },
        { "Fuji Velvia 50", "Fujifilm" },
    };
    if ((int)stock < 0 || (int)stock >= (int)ALWAN_FILM_STOCK_COUNT) return ALWAN_E_INVALID;
    if (name) *name = k_names[(int)stock][0];
    if (manufacturer) *manufacturer = k_names[(int)stock][1];
    return ALWAN_OK;
}

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#define ALWAN_CORE_FILM_PROFILE alwan_film_profile_f32
#include "alwan_film_impl.inc"
#undef ALWAN_CORE_FILM_PROFILE
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64_FACADE
#include "alwan_api_f64_setup.h"
#define ALWAN_CORE_FILM_PROFILE alwan_film_profile_f64
#include "alwan_film_impl.inc"
#undef ALWAN_CORE_FILM_PROFILE
#include "alwan_api_teardown.h"
#endif
