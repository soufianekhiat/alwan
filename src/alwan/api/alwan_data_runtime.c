/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Runtime data: the switchable tables written to, and loaded from, a data pack.
 *
 * PACK FORMAT, version 1, every integer little-endian:
 *
 *   "ALWNDATA"                      8 bytes
 *   u32 version                     1
 *   u32 record count
 *   per record:
 *     u32 name length, then the name (no terminator): the table's symbol
 *         without its _f32 / _f64 suffix, as data/alwan_data_tables.h spells it
 *     u32 element size              4 (f32) or 8 (f64)
 *     u64 element count             the table's extent
 *     the elements, IEEE-754, little-endian
 *
 * A table that has both precisions is written once, in f64; the loader narrows
 * it to the f32 twin, which is what the compiler does to the CSV literal, so
 * the loaded twin is the embedded one bit for bit. Where narrowing would NOT
 * reproduce the f32 twin, the writer adds an f32 record of the same name and
 * the loader takes that instead. Tables with one precision are written in it.
 *
 * The pack is untrusted input: every length is checked against the bytes
 * left before it is read, and the whole pack is validated before anything is
 * allocated or replaced.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../data/alwan_data_tables.h"
#include "../data/alwan_data_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_MSC_VER)
#  include <intrin.h>
#endif

#define PACK_MAGIC "ALWNDATA"
#define PACK_VERSION 1u

/* The data root: the folder holding ALWAN_DATA_PACK_NAME. alwan_data_set_root,
 * else the ALWAN_DATA_ROOT environment variable, else ALWAN_DATA_ROOT_DEFAULT
 * (the current directory unless the library was built with another). */
#ifndef ALWAN_DATA_ROOT_DEFAULT
#  define ALWAN_DATA_ROOT_DEFAULT "."
#endif
#ifndef ALWAN_DATA_PACK_NAME
#  define ALWAN_DATA_PACK_NAME "alwan_data.pack"
#endif
#define ROOT_MAX 1024

static char g_root[ROOT_MAX];
static int g_root_set;

/* ---------------------------------------------------------------- */
/* Little-endian fields                                              */
/* ---------------------------------------------------------------- */

static int host_is_le(void) {
    uint16_t const one = 1;
    unsigned char b;
    memcpy(&b, &one, 1);
    return b == 1;
}

static uint32_t rd_u32(unsigned char const *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t rd_u64(unsigned char const *p) {
    return (uint64_t)rd_u32(p) | ((uint64_t)rd_u32(p + 4) << 32);
}

static void wr_u32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}

static void wr_u64(unsigned char *p, uint64_t v) {
    wr_u32(p, (uint32_t)v);
    wr_u32(p + 4, (uint32_t)(v >> 32));
}

/* Copy n elements of size es between host order and little-endian. */
static void copy_le(void *dst, void const *src, size_t n, size_t es) {
    if (host_is_le()) {
        memcpy(dst, src, n * es);
        return;
    }
    {
        unsigned char *d = (unsigned char *)dst;
        unsigned char const *s = (unsigned char const *)src;
        size_t i, k;
        for (i = 0; i < n; i++)
            for (k = 0; k < es; k++) d[i * es + k] = s[i * es + es - 1 - k];
    }
}

/* ---------------------------------------------------------------- */
/* The registry, one precision at a time                             */
/* ---------------------------------------------------------------- */

static alwan_f32 const *entry_f32(alwan__table_entry const *e) {
#if ALWAN_EMBED_DATA
    return e->f32;
#else
    return e->f32 ? *e->f32 : NULL;
#endif
}

static alwan_f64 const *entry_f64(alwan__table_entry const *e) {
#if ALWAN_EMBED_DATA
    return e->f64;
#else
    return e->f64 ? *e->f64 : NULL;
#endif
}

/* The f32 twin is not the narrowed f64: the writer must add an f32 record. */
static int f32_not_narrowed(alwan_f32 const *t32, alwan_f64 const *t64, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        alwan_f32 const v = (alwan_f32)t64[i];
        if (memcmp(&v, &t32[i], sizeof v) != 0) return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------- */
/* Writing                                                           */
/* ---------------------------------------------------------------- */

typedef struct {
    unsigned char *dst;
    size_t cap, pos;
} pack_out;

static void put(pack_out *o, void const *src, size_t n) {
    if (o->dst && o->pos + n <= o->cap) memcpy(o->dst + o->pos, src, n);
    o->pos += n;
}

static void put_record(pack_out *o, char const *name, void const *data, size_t es, size_t n) {
    unsigned char h[8];
    size_t const len = strlen(name);
    wr_u32(h, (uint32_t)len);
    put(o, h, 4);
    put(o, name, len);
    wr_u32(h, (uint32_t)es);
    put(o, h, 4);
    wr_u64(h, (uint64_t)n);
    put(o, h, 8);
    if (o->dst && o->pos + n * es <= o->cap) copy_le(o->dst + o->pos, data, n, es);
    o->pos += n * es;
}

alwan_status alwan_data_pack_write(void *dst, size_t capacity, size_t *size_out, unsigned groups) {
    pack_out o;
    unsigned char h[8];
    uint32_t records = 0;
    size_t i, count_at;
    if (!size_out) return ALWAN_E_INVALID;
    o.dst = (unsigned char *)dst;
    o.cap = dst ? capacity : 0;
    o.pos = 0;

    put(&o, PACK_MAGIC, 8);
    wr_u32(h, PACK_VERSION);
    put(&o, h, 4);
    count_at = o.pos;
    put(&o, h, 4);   /* record count, written below */

    for (i = 0; i < alwan__table_registry_count; i++) {
        alwan__table_entry const *e = &alwan__table_registry[i];
        alwan_f32 const *t32 = entry_f32(e);
        alwan_f64 const *t64 = entry_f64(e);
        if (!(e->group & groups)) continue;
        if (t64) {
            put_record(&o, e->name, t64, 8, e->extent);
            records++;
        }
        if (t32 && (!t64 || f32_not_narrowed(t32, t64, e->extent))) {
            put_record(&o, e->name, t32, 4, e->extent);
            records++;
        }
    }

    if (o.dst && count_at + 4 <= o.cap) wr_u32(o.dst + count_at, records);
    *size_out = o.pos;
    if (dst && o.pos > capacity) return ALWAN_E_RANGE;
    return ALWAN_OK;
}

/* ---------------------------------------------------------------- */
/* Reading                                                           */
/* ---------------------------------------------------------------- */

typedef struct {
    char const *name;
    size_t name_len;
    size_t es;
    size_t n;
    unsigned char const *data;
} pack_record;

/* Walk the pack, each record in turn through `fn`. ALWAN_E_INVALID on anything
 * malformed. */
static alwan_status pack_walk(unsigned char const *p, size_t size,
                              alwan_status (*fn)(pack_record const *, void *), void *user) {
    uint32_t count, r;
    size_t pos = 16;
    if (!p || size < 16 || memcmp(p, PACK_MAGIC, 8) != 0) return ALWAN_E_INVALID;
    if (rd_u32(p + 8) != PACK_VERSION) return ALWAN_E_INVALID;
    count = rd_u32(p + 12);
    for (r = 0; r < count; r++) {
        pack_record rec;
        uint64_t n64;
        if (size - pos < 4) return ALWAN_E_INVALID;
        rec.name_len = rd_u32(p + pos);
        pos += 4;
        if (rec.name_len == 0 || rec.name_len > 256 || size - pos < rec.name_len) return ALWAN_E_INVALID;
        rec.name = (char const *)(p + pos);
        pos += rec.name_len;
        if (size - pos < 12) return ALWAN_E_INVALID;
        rec.es = rd_u32(p + pos);
        n64 = rd_u64(p + pos + 4);
        pos += 12;
        if (rec.es != 4 && rec.es != 8) return ALWAN_E_INVALID;
        if (n64 > (uint64_t)((size - pos) / rec.es)) return ALWAN_E_INVALID;
        rec.n = (size_t)n64;
        rec.data = p + pos;
        pos += rec.n * rec.es;
        {
            alwan_status const st = fn(&rec, user);
            if (st != ALWAN_OK) return st;
        }
    }
    return pos == size ? ALWAN_OK : ALWAN_E_INVALID;
}

static int name_is(pack_record const *rec, char const *name) {
    return strlen(name) == rec->name_len && memcmp(name, rec->name, rec->name_len) == 0;
}

/* A table of `groups` this build has, at another extent: the pack is refused. */
static alwan_status check_record(pack_record const *rec, void *user) {
    unsigned const groups = *(unsigned const *)user;
    size_t i;
    for (i = 0; i < alwan__table_registry_count; i++) {
        alwan__table_entry const *e = &alwan__table_registry[i];
        if ((e->group & groups) && name_is(rec, e->name))
            return rec->n == e->extent ? ALWAN_OK : ALWAN_E_INVALID;
    }
    return ALWAN_OK;
}

#if !ALWAN_EMBED_DATA

/* What a load will take for one registry entry: the record each precision
 * comes from (NULL: none in the pack), and the copies made for it. */
typedef struct {
    pack_record r32, r64;
    int has32, has64;
    alwan_f32 *c32;
    alwan_f64 *c64;
} load_slot;

typedef struct {
    load_slot *slots;
    unsigned groups;
} load_plan;

static alwan_status plan_record(pack_record const *rec, void *user) {
    load_plan *pl = (load_plan *)user;
    size_t i;
    for (i = 0; i < alwan__table_registry_count; i++) {
        alwan__table_entry const *e = &alwan__table_registry[i];
        if (!(e->group & pl->groups) || !name_is(rec, e->name)) continue;
        if (rec->es == 8) { pl->slots[i].r64 = *rec; pl->slots[i].has64 = 1; }
        else              { pl->slots[i].r32 = *rec; pl->slots[i].has32 = 1; }
        return ALWAN_OK;
    }
    return ALWAN_OK;   /* a table this build does not have */
}

static void release(alwan__table_entry const *e, void *p) {
    if (!p) return;
    if (e->release) e->release(p); else ALWAN_FREE(p);
}

static void unload(alwan__table_entry *e) {
    if (e->f32 && *e->f32) { release(e, (void *)*e->f32); *e->f32 = NULL; }
    if (e->f64 && *e->f64) { release(e, (void *)*e->f64); *e->f64 = NULL; }
    e->release = NULL; /* the on-demand loader allocates with ALWAN_ALLOC */
}

#endif /* !ALWAN_EMBED_DATA */

alwan_status alwan_data_load(void const *pack, size_t size, unsigned groups, alwan_ctx *ctx) {
    alwan_status st = pack_walk((unsigned char const *)pack, size, check_record, &groups);
    if (st != ALWAN_OK) return st;
#if ALWAN_EMBED_DATA
    (void)groups; (void)ctx;
    return ALWAN_OK;
#else
    {
        size_t const n = alwan__table_registry_count;
        load_plan pl;
        size_t i;
        int failed = 0;
        pl.groups = groups;
        pl.slots = (load_slot *)alwan_ctx_alloc((n ? n : 1) * sizeof(load_slot), 0, ctx);
        if (!pl.slots) return ALWAN_E_NOMEM;
        memset(pl.slots, 0, (n ? n : 1) * sizeof(load_slot));
        st = pack_walk((unsigned char const *)pack, size, plan_record, &pl);

        /* Allocate every copy before touching a table, so a failure changes nothing. */
        for (i = 0; st == ALWAN_OK && i < n && !failed; i++) {
            alwan__table_entry const *e = &alwan__table_registry[i];
            load_slot *s = &pl.slots[i];
            size_t const ext = e->extent;
            /* A table with an f64 twin loads only when the pack has the f64 record:
             * an f32 copy alone would leave the f64 slot to the on-demand loader,
             * and one entry would then hold memory from two allocators. */
            int const whole = !e->f64 || s->has64;
            int const want64 = e->f64 && s->has64;
            int const want32 = whole && e->f32 && (s->has32 || (e->f64 && s->has64));
            if (want64) {
                s->c64 = (alwan_f64 *)alwan_ctx_alloc(ext * sizeof(alwan_f64), 0, ctx);
                if (!s->c64) { failed = 1; break; }
                copy_le(s->c64, s->r64.data, ext, 8);
            }
            if (want32) {
                s->c32 = (alwan_f32 *)alwan_ctx_alloc(ext * sizeof(alwan_f32), 0, ctx);
                if (!s->c32) { failed = 1; break; }
                if (s->has32) {
                    copy_le(s->c32, s->r32.data, ext, 4);
                } else {
                    size_t k;
                    for (k = 0; k < ext; k++) {
                        alwan_f64 v;
                        copy_le(&v, s->r64.data + k * 8, 1, 8);
                        s->c32[k] = (alwan_f32)v;
                    }
                }
            }
        }

        if (st != ALWAN_OK || failed) {
            for (i = 0; i < n; i++) {
                alwan_ctx_free(pl.slots[i].c32, ctx);
                alwan_ctx_free(pl.slots[i].c64, ctx);
            }
            alwan_ctx_free(pl.slots, ctx);
            return st != ALWAN_OK ? st : ALWAN_E_NOMEM;
        }

        for (i = 0; i < n; i++) {
            alwan__table_entry *e = &alwan__table_registry[i];
            load_slot const *s = &pl.slots[i];
            if (!s->c32 && !s->c64) continue;
            unload(e);
            e->release = ctx ? ctx->free_fn : NULL;
            if (s->c32) *e->f32 = s->c32;
            if (s->c64) *e->f64 = s->c64;
        }
        for (i = 0; i < n; i++) alwan__table_registry[i].demand_failed = 0;
        alwan_ctx_free(pl.slots, ctx);
        return ALWAN_OK;
    }
#endif
}

alwan_status alwan_data_load_file(char const *path, unsigned groups, alwan_ctx *ctx) {
    FILE *f;
    long len;
    void *buf;
    alwan_status st;
    if (!path) return ALWAN_E_INVALID;
    f = fopen(path, "rb");
    if (!f) return ALWAN_E_NODATA;
    if (fseek(f, 0, SEEK_END) != 0 || (len = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return ALWAN_E_NODATA;
    }
    buf = alwan_ctx_alloc(len ? (size_t)len : 1, 0, ctx);
    if (!buf) { fclose(f); return ALWAN_E_NOMEM; }
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        fclose(f);
        alwan_ctx_free(buf, ctx);
        return ALWAN_E_NODATA;
    }
    fclose(f);
    st = alwan_data_load(buf, (size_t)len, groups, ctx);
    alwan_ctx_free(buf, ctx);
    return st;
}

void alwan_data_free(unsigned groups) {
#if ALWAN_EMBED_DATA
    (void)groups;
#else
    size_t i;
    for (i = 0; i < alwan__table_registry_count; i++) {
        if (!(alwan__table_registry[i].group & groups)) continue;
        unload(&alwan__table_registry[i]);
        alwan__table_registry[i].demand_failed = 0;
    }
#endif
}

size_t alwan_data_loaded_bytes(unsigned groups) {
    size_t bytes = 0;
#if ALWAN_EMBED_DATA
    (void)groups;
#else
    size_t i;
    for (i = 0; i < alwan__table_registry_count; i++) {
        alwan__table_entry const *e = &alwan__table_registry[i];
        if (!(e->group & groups)) continue;
        if (entry_f32(e)) bytes += e->extent * sizeof(alwan_f32);
        if (entry_f64(e)) bytes += e->extent * sizeof(alwan_f64);
    }
#endif
    return bytes;
}

size_t alwan_data_missing(unsigned groups) {
    size_t missing = 0;
#if ALWAN_EMBED_DATA
    (void)groups;
#else
    size_t i;
    for (i = 0; i < alwan__table_registry_count; i++) {
        alwan__table_entry const *e = &alwan__table_registry[i];
        if (!(e->group & groups)) continue;
        if ((e->f32 && !*e->f32) || (e->f64 && !*e->f64)) missing++;
    }
#endif
    return missing;
}

/* ---------------------------------------------------------------- */
/* The data root, preload and on-demand loading                      */
/* ---------------------------------------------------------------- */

alwan_status alwan_data_set_root(char const *folder) {
    size_t i;
    if (folder) {
        size_t const len = strlen(folder);
        if (len + 1 + sizeof(ALWAN_DATA_PACK_NAME) > ROOT_MAX) return ALWAN_E_RANGE;
        memcpy(g_root, folder, len + 1);
        g_root_set = 1;
    } else {
        g_root_set = 0;
    }
    /* A new root may hold what the old one did not. */
#if !ALWAN_EMBED_DATA
    for (i = 0; i < alwan__table_registry_count; i++) alwan__table_registry[i].demand_failed = 0;
#else
    (void)i;
#endif
    return ALWAN_OK;
}

#if !ALWAN_EMBED_DATA
/* "<root>/alwan_data.pack" into out (ROOT_MAX bytes). */
static void pack_path(char *out) {
    char const *root = g_root_set ? g_root : NULL;
    size_t len;
#if defined(_MSC_VER)
    char *env = NULL;
    size_t envlen = 0;
    if (!root && _dupenv_s(&env, &envlen, "ALWAN_DATA_ROOT") == 0 && env && *env
        && strlen(env) + 1 + sizeof(ALWAN_DATA_PACK_NAME) <= ROOT_MAX) {
        memcpy(out, env, strlen(env) + 1);
        root = out;
    }
    free(env);
#else
    char const *env = getenv("ALWAN_DATA_ROOT");
    if (!root && env && *env && strlen(env) + 1 + sizeof(ALWAN_DATA_PACK_NAME) <= ROOT_MAX) root = env;
#endif
    if (!root) root = ALWAN_DATA_ROOT_DEFAULT;
    if (root != out) memcpy(out, root, strlen(root) + 1);
    len = strlen(out);
    if (len && out[len - 1] != '/' && out[len - 1] != '\\') out[len++] = '/';
    memcpy(out + len, ALWAN_DATA_PACK_NAME, sizeof(ALWAN_DATA_PACK_NAME));
}
#endif

alwan_status alwan_data_preload(unsigned groups, alwan_ctx *ctx) {
#if ALWAN_EMBED_DATA
    (void)groups; (void)ctx;
    return ALWAN_OK;
#else
    char path[ROOT_MAX];
    /* A context's own root, when it was created with one; else the process's. */
    if (ctx && ctx->runtime_data_root) {
        size_t const len = strlen(ctx->runtime_data_root);
        size_t at = len;
        if (len + 1 + sizeof(ALWAN_DATA_PACK_NAME) > ROOT_MAX) return ALWAN_E_RANGE;
        memcpy(path, ctx->runtime_data_root, len);
        if (at && path[at - 1] != '/' && path[at - 1] != '\\') path[at++] = '/';
        memcpy(path + at, ALWAN_DATA_PACK_NAME, sizeof(ALWAN_DATA_PACK_NAME));
    } else {
        pack_path(path);
    }
    return alwan_data_load_file(path, groups, ctx);
#endif
}

#if !ALWAN_EMBED_DATA

/* Publish p into an empty slot; 0 when another thread got there first. */
static int publish(void const **slot, void const *p) {
#if defined(_MSC_VER)
    return _InterlockedCompareExchangePointer((void *volatile *)slot, (void *)p, NULL) == NULL;
#else
    return __sync_bool_compare_and_swap(slot, (void const *)0, p);
#endif
}

static int read_exact(FILE *f, void *dst, size_t n) { return fread(dst, 1, n, f) == n; }

/* Little-endian elements read straight into memory, put in host order. */
static void swap_to_host(void *p, size_t n, size_t es) {
    unsigned char *b = (unsigned char *)p;
    size_t i, k;
    if (host_is_le()) return;
    for (i = 0; i < n; i++)
        for (k = 0; k < es / 2; k++) {
            unsigned char const t = b[i * es + k];
            b[i * es + k] = b[i * es + es - 1 - k];
            b[i * es + es - 1 - k] = t;
        }
}

/* Find table e in the pack at path and load it, both precisions, into its empty
 * slots. Every length is checked as alwan_data_load checks it. 1 on success. */
static int demand_entry(alwan__table_entry *e) {
    char path[ROOT_MAX];
    unsigned char h[16];
    FILE *f;
    uint32_t count, r;
    alwan_f64 *c64 = NULL;
    alwan_f32 *c32 = NULL;
    size_t const name_len = strlen(e->name);
    int found64 = 0, found32 = 0, ok = 0;
    if (name_len > 256) return 0;
    pack_path(path);
    f = fopen(path, "rb");
    if (!f) return 0;
    if (!read_exact(f, h, 16) || memcmp(h, PACK_MAGIC, 8) != 0 || rd_u32(h + 8) != PACK_VERSION) goto done;
    count = rd_u32(h + 12);
    for (r = 0; r < count; r++) {
        char name[257];
        uint32_t nl, es;
        uint64_t n;
        int mine;
        if (!read_exact(f, h, 4)) goto done;
        nl = rd_u32(h);
        if (nl == 0 || nl > 256 || !read_exact(f, name, nl)) goto done;
        if (!read_exact(f, h, 12)) goto done;
        es = rd_u32(h);
        n = rd_u64(h + 4);
        if (es != 4 && es != 8) goto done;
        mine = nl == name_len && memcmp(name, e->name, nl) == 0;
        if (mine && n != (uint64_t)e->extent) goto done;
        if (mine && es == 8 && e->f64 && !c64) {
            c64 = (alwan_f64 *)ALWAN_ALLOC(e->extent * 8, 8);
            if (!c64 || !read_exact(f, c64, e->extent * 8)) goto done;
            swap_to_host(c64, e->extent, 8);
            found64 = 1;
        } else if (mine && es == 4 && e->f32 && !c32) {
            c32 = (alwan_f32 *)ALWAN_ALLOC(e->extent * 4, 4);
            if (!c32 || !read_exact(f, c32, e->extent * 4)) goto done;
            swap_to_host(c32, e->extent, 4);
            found32 = 1;
        } else {
            if (n > (uint64_t)0x7fffffff / es || fseek(f, (long)(n * es), SEEK_CUR) != 0) goto done;
            /* A table's records are adjacent (f64, then f32 if narrowing would not
             * give it): past them, there is nothing more to find. */
            if (!mine && (found64 || found32)) break;
        }
        if (found64 && (found32 || !e->f32)) break;
        if (found32 && !e->f64) break;
    }
    if (e->f32 && !c32 && c64) {
        size_t k;
        c32 = (alwan_f32 *)ALWAN_ALLOC(e->extent * sizeof(alwan_f32), sizeof(alwan_f32));
        if (!c32) goto done;
        for (k = 0; k < e->extent; k++) c32[k] = (alwan_f32)c64[k];
    }
    if ((e->f64 && !c64) || (e->f32 && !c32)) goto done;
    if (e->f64 && !publish((void const **)e->f64, c64)) ALWAN_FREE(c64);
    if (e->f32 && !publish((void const **)e->f32, c32)) ALWAN_FREE(c32);
    c64 = NULL;
    c32 = NULL;
    ok = 1;
done:
    fclose(f);
    if (c64) ALWAN_FREE(c64);
    if (c32) ALWAN_FREE(c32);
    return ok;
}

static alwan__table_entry *entry_of(void const *slot) {
    size_t i;
    for (i = 0; i < alwan__table_registry_count; i++) {
        alwan__table_entry *e = &alwan__table_registry[i];
        if ((void const *)e->f32 == slot || (void const *)e->f64 == slot) return e;
    }
    return NULL;
}

static void demand(void const *slot) {
    alwan__table_entry *e = entry_of(slot);
    if (!e || e->demand_failed) return;
    if (!demand_entry(e)) e->demand_failed = 1;
}

alwan_f32 const *alwan__data_demand_f32(alwan_f32 const **slot) {
    demand(slot);
    return *slot;
}

alwan_f64 const *alwan__data_demand_f64(alwan_f64 const **slot) {
    demand(slot);
    return *slot;
}

#endif /* !ALWAN_EMBED_DATA */
