/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * The list of switchable tables, internal. One entry per table compiled into
 * this build (its ALWAN_TABLE_* switch on), generated into
 * data/alwan_data_registry_gen.c by tools/gen_table_registry.py. The data API
 * (api/alwan_data_runtime.c) walks it: alwan_data_pack_write reads each table
 * through it, alwan_data_load and alwan_data_free set the pointers it holds.
 */

#ifndef ALWAN_DATA_REGISTRY_H
#define ALWAN_DATA_REGISTRY_H

#include "../alwan.h"

typedef struct {
    char const *name;    /* the table's symbol without the precision suffix */
    unsigned group;      /* one ALWAN_DATA_GROUP_* bit */
    size_t extent;       /* element count, the same for both precisions */
#if ALWAN_EMBED_DATA
    alwan_f32 const *f32;    /* the array, NULL when the build has no f32 twin */
    alwan_f64 const *f64;
#else
    alwan_f32 const **f32;   /* the pointer the readers index, NULL when no f32 twin */
    alwan_f64 const **f64;
    alwan_free_fn release;   /* how the loaded copies are freed; NULL = ALWAN_FREE */
    int demand_failed;       /* an on-demand load found no table: not tried again until
                                the root changes or a load or free runs */
#endif
} alwan__table_entry;

/* A DUAL table's f32 twin exists only with ALWAN_WITH_F32. F32_ONLY and
 * F64_ONLY tables have one precision in every build. */
#if ALWAN_EMBED_DATA
#  define ALWAN__REG_F32(name) name##_f32
#  define ALWAN__REG_F64(name) name##_f64
#else
#  define ALWAN__REG_F32(name) &name##_f32__slot
#  define ALWAN__REG_F64(name) &name##_f64__slot
#endif
#if ALWAN_WITH_F32
#  define ALWAN__REG_DUAL_F32(name) ALWAN__REG_F32(name)
#else
#  define ALWAN__REG_DUAL_F32(name) NULL
#endif
#define ALWAN__REG_NONE(name) NULL
/* A table whose symbol has no precision suffix (the refractive arrays). */
#if ALWAN_EMBED_DATA
#  define ALWAN__REG_PLAIN(sym) sym
#else
#  define ALWAN__REG_PLAIN(sym) &sym##__slot
#endif

/* Terminated by an all-NULL entry, which the count leaves out. */
extern alwan__table_entry alwan__table_registry[];
extern size_t const alwan__table_registry_count;

#endif /* ALWAN_DATA_REGISTRY_H */
