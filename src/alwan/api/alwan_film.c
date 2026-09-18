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
        { "Agfa Vista 100", "Agfa" },
        { "Fuji C200", "Fujifilm" },
        { "Fuji Eterna 500 Vivid", "Fujifilm" },
        { "Fuji Natura 1600", "Fujifilm" },
        { "Fuji Pro 160C", "Fujifilm" },
        { "Fuji Pro 160S", "Fujifilm" },
        { "Fuji Pro 400H", "Fujifilm" },
        { "Fuji Superia Reala", "Fujifilm" },
        { "Fuji Superia X-Tra 400", "Fujifilm" },
        { "Kodak Verita 200D 5206", "Kodak" },
        { "Kodak 5247", "Kodak" },
        { "Kodak 5247 II", "Kodak" },
        { "Kodak 5247 II Alt", "Kodak" },
        { "Kodak 5248", "Kodak" },
        { "Kodak EXR 100T 5248", "Kodak" },
        { "Kodak 5250", "Kodak" },
        { "Kodak Vision 320T 5277", "Kodak" },
        { "Kodak EXR 200T 5293", "Kodak" },
        { "Kodak Aerocolor IV 2460", "Kodak" },
        { "Kodak Aerocolor IV 2460 Low", "Kodak" },
        { "Kodak Aerocolor IV 2460 High", "Kodak" },
        { "Kodak Gold 200", "Kodak" },
        { "Kodak Portra 160", "Kodak" },
        { "Kodak Portra 800", "Kodak" },
        { "Kodak Portra 800 @1600", "Kodak" },
        { "Kodak Portra 800 @3200", "Kodak" },
        { "Kodak Ultramax 400", "Kodak" },
        { "Kodak Vericolor III", "Kodak" },
        { "Kodak 5222 Dev 4", "Kodak" },
        { "Kodak 5222 Dev 5", "Kodak" },
        { "Kodak 5222 Dev 9", "Kodak" },
        { "Kodak 5222 Dev 12", "Kodak" },
        { "Kodak Tri-X 400", "Kodak" },
        { "Kodak Trix-X 400 Dev 7", "Kodak" },
        { "Kodak Trix-X 400 Dev 9", "Kodak" },
        { "Kodak Trix-X 400 Dev 11", "Kodak" },
        { "Fuji Eterna-CP 3523XD", "Fujifilm" },
        { "Fuji Crystal Archive DPII", "Fujifilm" },
        { "Fuji Crystal Archive Maxima", "Fujifilm" },
        { "Fuji Crystal Archive Pro PDII", "Fujifilm" },
        { "Fuji Crystal Archive Super Type C", "Fujifilm" },
        { "Fujiflex Crystal Archive New Version", "Fujifilm" },
        { "Fujiflex Crystal Archive Old Version", "Fujifilm" },
        { "Kodak 5381", "Kodak" },
        { "Kodak 5383", "Kodak" },
        { "Kodak 5384", "Kodak" },
        { "Kodak Duraflex Plus", "" },
        { "Kodak Endura Premier Paper", "Kodak" },
        { "Kodak EXR 5386", "Kodak" },
        { "Kodak Portra Endura Paper", "Kodak" },
        { "Kodak Supra Endura Paper", "Kodak" },
        { "Kodak 2302 Dev 2", "Kodak" },
        { "Kodak 2302 Dev 3", "Kodak" },
        { "Kodak 2302 Dev 5", "Kodak" },
        { "Kodak 2302 Dev 7", "Kodak" },
        { "Kodak 2302 Dev 9", "Kodak" },
        { "Kodak Professional Polymax Fine-Art Paper", "Kodak" },
        { "Kodak Polymax Fine-Art Paper Grade -1", "Kodak" },
        { "Kodak Polymax Fine-Art Paper Grade 0", "Kodak" },
        { "Kodak Polymax Fine-Art Paper Grade 1", "Kodak" },
        { "Kodak Polymax Fine-Art Paper Grade 2", "Kodak" },
        { "Kodak Polymax Fine-Art Paper Grade 3", "Kodak" },
        { "Kodak Polymax Fine-Art Paper Grade 4", "Kodak" },
        { "Kodak Polymax Fine-Art Paper Grade 5", "Kodak" },
        { "Fuji FP-100C", "Fujifilm" },
        { "Fuji Instax color", "Fujifilm" },
        { "Fuji Provia 100F", "Fujifilm" },
        { "Kodachrome 64", "Kodak" },
        { "Kodak Aerochrome III Infrared Film 1443", "Kodak" },
        { "Kodak Ektachrome 100D alt.", "Kodak" },
        { "Ilfochrome Micrographic M", "Ilford" },
        { "Ilfochrome Micrographic P", "Ilford" },
        { "Kodak Ektachrome Radiance III Paper", "Kodak" },
    };
    if ((int)stock < 0 || (int)stock >= (int)ALWAN_FILM_STOCK_COUNT) return ALWAN_E_INVALID;
    if (name) *name = k_names[(int)stock][0];
    if (manufacturer) *manufacturer = k_names[(int)stock][1];
    return ALWAN_OK;
}

alwan_status alwan_film_look_default(alwan_film_look *look, alwan_film_stock negative, alwan_film_stock print) {
    if (!look) return ALWAN_E_INVALID;
    if ((int)negative < 0 || (int)negative >= (int)ALWAN_FILM_STOCK_COUNT) return ALWAN_E_INVALID;
    if (print != ALWAN_FILM_NONE && ((int)print < 0 || (int)print >= (int)ALWAN_FILM_STOCK_COUNT)) return ALWAN_E_INVALID;
    look->negative = negative;
    look->print = print;
    look->light = ALWAN_ILLUMINANT_D65;
    look->gamut = ALWAN_JAKOB2019_SRGB;
    look->stops = 0.0;
    look->red = 0.0;
    look->green = 0.0;
    look->blue = 0.0;
    look->balance_on_grey = 1;
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
