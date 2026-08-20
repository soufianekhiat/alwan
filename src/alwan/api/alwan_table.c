/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Public table readers: argument validation, mode acceptance and the STRICT
 * path over the shared addressing gate in core/alwan_table_core.
 *
 * This TU holds no table data of its own. The embedded arrays are declared in
 * data/alwan_data_tables.h and defined in data/alwan_data_tables*.c, so adding
 * a named reader here costs no preprocessing weight.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_table_core.h"
#include "../core/alwan_vision_core.h"   /* MACHADO_* stay core-tier; see data/alwan_data_tables.h */
#include "../data/alwan_data_tables.h"

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_table_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_table_impl.inc"
#include "alwan_api_teardown.h"
#endif
