/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Measured chart files: CGATS.17-2009 and OpenQualia Measurement (OQM).
 * Parameterized over ALWAN_CORE_T; see alwan_chart.c. The lexing that does
 * not depend on the precision lives in alwan_chart_common.h.
 *
 * No include guard: included once per precision.
 */

/* ----------------------------------------------------------------
 * The object
 * ---------------------------------------------------------------- */

struct ALWAN_CORE_CHART_S {
    /* Colorimetry, three per patch, Y normalised to 1 under `illuminant`. */
    ALWAN_CORE_T *xyz;
    char        **patch_names;      /* one per patch, never NULL once loaded */
    size_t        patch_count;

    /* Per-patch reflectance, present only when the file carried spectra.
     * Row-major, `bands` values per patch, on a uniform grid. */
    ALWAN_CORE_T *spectra;
    ALWAN_CORE_T  spectral_min_nm;
    ALWAN_CORE_T  spectral_max_nm;
    size_t        bands;

    /* Header keys upper-cased, values as written, in file order. */
    char        **header_keys;
    char        **header_values;
    size_t        header_count;

    alwan_illuminant    illuminant;
    alwan_observer_type observer;
    alwan_chart_source  source;     /* which column family the XYZ came from */
};

/* ----------------------------------------------------------------
 * Destroy
 * ---------------------------------------------------------------- */

void ALWAN_CORE_FNLIT(alwan_chart_destroy)(ALWAN_CORE_CHART *chart, alwan_ctx *ctx) {
    size_t i;
    (void)ctx;
    if (!chart) return;
    if (chart->patch_names) {
        for (i = 0; i < chart->patch_count; i++) ALWAN_FREE(chart->patch_names[i]);
        ALWAN_FREE(chart->patch_names);
    }
    if (chart->header_keys) {
        for (i = 0; i < chart->header_count; i++) ALWAN_FREE(chart->header_keys[i]);
        ALWAN_FREE(chart->header_keys);
    }
    if (chart->header_values) {
        for (i = 0; i < chart->header_count; i++) ALWAN_FREE(chart->header_values[i]);
        ALWAN_FREE(chart->header_values);
    }
    ALWAN_FREE(chart->xyz);
    ALWAN_FREE(chart->spectra);
    ALWAN_FREE(chart);
}

/* ----------------------------------------------------------------
 * Header accumulation
 * ---------------------------------------------------------------- */

static alwan_status ALWAN_CORE_FNLIT(alwan__chart_add_header)(ALWAN_CORE_CHART *c,
                                                              char const *key, size_t klen,
                                                              char const *val, size_t vlen) {
    char **nk = (char **)ALWAN_ALLOC(sizeof(char *) * (c->header_count + 1), sizeof(void *));
    char **nv = (char **)ALWAN_ALLOC(sizeof(char *) * (c->header_count + 1), sizeof(void *));
    size_t i;
    if (!nk || !nv) { ALWAN_FREE(nk); ALWAN_FREE(nv); return ALWAN_E_NOMEM; }
    for (i = 0; i < c->header_count; i++) { nk[i] = c->header_keys[i]; nv[i] = c->header_values[i]; }
    ALWAN_FREE(c->header_keys);
    ALWAN_FREE(c->header_values);
    c->header_keys = nk;
    c->header_values = nv;
    nk[c->header_count] = alwan__chart_strdup_n(key, klen);
    nv[c->header_count] = alwan__chart_strdup_n(val, vlen);
    if (!nk[c->header_count] || !nv[c->header_count]) return ALWAN_E_NOMEM;
    for (i = 0; i < klen; i++) nk[c->header_count][i] = alwan__chart_upper(nk[c->header_count][i]);
    c->header_count++;
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Load from a buffer
 *
 * Two passes over the same buffer: the first reads the header and the
 * DATA_FORMAT block and counts the rows, the second fills them in. Counting
 * first means one exact allocation per array instead of a growth policy, and
 * the buffer is never written to, so the caller keeps ownership and can
 * parse the same bytes twice.
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_chart_load_buffer)(ALWAN_CORE_CHART **out,
                                                       char const *buf, size_t len,
                                                       alwan_ctx *ctx) {
    ALWAN_CORE_CHART *c = NULL;
    alwan__chart_tok *fields = NULL, *cells = NULL;
    size_t nfields = 0, declared_sets = 0, row = 0;
    size_t i, pos = 0, bom = 0;
    int in_format = 0, in_data = 0, seen_format_end = 0;
    /* column indices; (size_t)-1 when the file declares no such column */
    size_t ix_id = (size_t)-1, ix_name = (size_t)-1;
    size_t ix_X = (size_t)-1, ix_Y = (size_t)-1, ix_Z = (size_t)-1;
    size_t ix_L = (size_t)-1, ix_a = (size_t)-1, ix_b = (size_t)-1;
    size_t *spec_col = NULL;
    double *spec_nm = NULL;
    size_t nspec = 0;
    alwan_status st = ALWAN_E_INVALID;

    if (out) *out = NULL;
    if (!out || !buf || len == 0) return ALWAN_E_INVALID;

    c = (ALWAN_CORE_CHART *)ALWAN_ALLOC(sizeof(*c), sizeof(void *));
    if (!c) return ALWAN_E_NOMEM;
    memset(c, 0, sizeof(*c));
    c->illuminant = ALWAN_ILLUMINANT_D50;
    c->observer   = ALWAN_OBSERVER_CIE_1931_2DEG;

    fields = (alwan__chart_tok *)ALWAN_ALLOC(sizeof(*fields) * ALWAN_CHART_MAX_FIELDS, sizeof(void *));
    cells  = (alwan__chart_tok *)ALWAN_ALLOC(sizeof(*cells)  * ALWAN_CHART_MAX_FIELDS, sizeof(void *));
    if (!fields || !cells) { st = ALWAN_E_NOMEM; goto fail; }

    /* Skip a UTF-8 BOM: instruments write one often enough to matter. */
    if (len >= 3 && (unsigned char)buf[0] == 0xEF &&
        (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF) bom = 3;

    /* --- pass 1 --- */
    pos = bom;
    while (pos < len) {
        char const *line = buf + pos;
        size_t llen = 0;
        while (pos + llen < len && buf[pos + llen] != '\n') llen++;
        pos += llen + (pos + llen < len ? 1 : 0);
        while (llen > 0 && alwan__chart_is_space(line[llen - 1])) llen--;
        while (llen > 0 && alwan__chart_is_space(line[0])) { line++; llen--; }
        if (llen == 0 || line[0] == '#') continue;

        if (alwan__chart_eq_n(line, llen, "BEGIN_DATA_FORMAT")) { in_format = 1; continue; }
        if (alwan__chart_eq_n(line, llen, "END_DATA_FORMAT"))   { in_format = 0; seen_format_end = 1; continue; }
        if (alwan__chart_eq_n(line, llen, "BEGIN_DATA"))        { in_data = 1; continue; }
        if (alwan__chart_eq_n(line, llen, "END_DATA"))          { in_data = 0; continue; }

        if (in_format) {
            size_t n = alwan__chart_split(line, llen, fields + nfields,
                                          ALWAN_CHART_MAX_FIELDS - nfields);
            if (n == (size_t)-1) { st = ALWAN_E_RANGE; goto fail; }
            nfields += n;
        } else if (in_data) {
            row++;
        } else {
            /* KEYWORD value, the value optionally quoted. A bare identifier
             * line (the "OQM" or "IT8.7/2" that opens the file) becomes a key
             * with an empty value, which is how CGATS names its format. */
            size_t n = alwan__chart_split(line, llen, cells, ALWAN_CHART_MAX_FIELDS);
            if (n == (size_t)-1 || n == 0) continue;
            if (n > 1) {
                /* everything after the key, as written, quotes already stripped */
                char const *vs = cells[1].p;
                char const *ve = cells[n - 1].p + cells[n - 1].len;
                st = ALWAN_CORE_FNLIT(alwan__chart_add_header)(c, cells[0].p, cells[0].len,
                                                               vs, (size_t)(ve - vs));
            } else {
                st = ALWAN_CORE_FNLIT(alwan__chart_add_header)(c, cells[0].p, cells[0].len, "", 0);
            }
            if (st != ALWAN_OK) goto fail;
        }
    }

    if (!seen_format_end || nfields == 0) { st = ALWAN_E_INVALID; goto fail; }  /* no DATA_FORMAT block */
    if (row == 0) { st = ALWAN_E_INVALID; goto fail; }                          /* no DATA block */

    for (i = 0; i < c->header_count; i++) {
        char const *k = c->header_keys[i];
        size_t klen = strlen(k);
        if (alwan__chart_eq_n(k, klen, "NUMBER_OF_SETS")) {
            /* When it is present and disagrees with the row count the file is
             * damaged, and guessing which of the two is right helps nobody. */
            if (alwan__chart_atoz(c->header_values[i], &declared_sets) && declared_sets != row) {
                st = ALWAN_E_INVALID;
                goto fail;
            }
        } else if (alwan__chart_eq_n(k, klen, "ILLUMINANT")) {
            c->illuminant = alwan__chart_illuminant_from_name(c->header_values[i]);
        } else if (alwan__chart_eq_n(k, klen, "OBSERVER")) {
            c->observer = alwan__chart_observer_from_name(c->header_values[i]);
        }
    }

    /* --- resolve the columns --- */
    spec_col = (size_t *)ALWAN_ALLOC(sizeof(size_t) * nfields, sizeof(void *));
    spec_nm  = (double *)ALWAN_ALLOC(sizeof(double) * nfields, sizeof(double));
    if (!spec_col || !spec_nm) { st = ALWAN_E_NOMEM; goto fail; }
    for (i = 0; i < nfields; i++) {
        char const *f = fields[i].p;
        size_t n = fields[i].len;
        double nm;
        if      (alwan__chart_eq_n(f, n, "SAMPLE_ID"))   ix_id = i;
        else if (alwan__chart_eq_n(f, n, "SAMPLE_NAME")) ix_name = i;
        else if (alwan__chart_eq_n(f, n, "XYZ_X")) ix_X = i;
        else if (alwan__chart_eq_n(f, n, "XYZ_Y")) ix_Y = i;
        else if (alwan__chart_eq_n(f, n, "XYZ_Z")) ix_Z = i;
        else if (alwan__chart_eq_n(f, n, "LAB_L")) ix_L = i;
        else if (alwan__chart_eq_n(f, n, "LAB_A")) ix_a = i;
        else if (alwan__chart_eq_n(f, n, "LAB_B")) ix_b = i;
        else if (alwan__chart_spectral_nm(f, n, &nm)) {
            spec_col[nspec] = i;
            spec_nm[nspec] = nm;
            nspec++;
        }
    }

    /* XYZ wins, then Lab, then the spectrum: a file carrying both states its
     * own tristimulus, and re-deriving it would answer a question the
     * measuring lab already answered. */
    if      (ix_X != (size_t)-1 && ix_Y != (size_t)-1 && ix_Z != (size_t)-1) c->source = ALWAN_CHART_SOURCE_XYZ;
    else if (ix_L != (size_t)-1 && ix_a != (size_t)-1 && ix_b != (size_t)-1) c->source = ALWAN_CHART_SOURCE_LAB;
    else if (nspec >= 2)                                                    c->source = ALWAN_CHART_SOURCE_SPECTRAL;
    else { st = ALWAN_E_NODATA; goto fail; }   /* no XYZ, no Lab, no spectrum */

    /* alwan_spd is uniformly sampled, so the bands have to be evenly spaced
     * and in order. A file that is neither is likelier damaged than exotic:
     * reject it when the spectrum is the only colorimetry, and otherwise
     * keep the tristimulus and drop the spectra. */
    if (nspec >= 2) {
        double step = spec_nm[1] - spec_nm[0];
        int uniform = (step > 0.0);
        if (uniform) {
            double tol = step * 0.01;
            for (i = 2; i < nspec; i++) {
                double d = spec_nm[i] - spec_nm[i - 1] - step;
                if (d < -tol || d > tol) { uniform = 0; break; }
            }
        }
        if (!uniform) {
            if (c->source == ALWAN_CHART_SOURCE_SPECTRAL) { st = ALWAN_E_INVALID; goto fail; }
            nspec = 0;
        }
    }

    /* --- allocate --- */
    c->patch_count = row;
    c->xyz = (ALWAN_CORE_T *)ALWAN_ALLOC(alwan_safe_array_size(row * 3, sizeof(ALWAN_CORE_T)), sizeof(ALWAN_CORE_T));
    c->patch_names = (char **)ALWAN_ALLOC(sizeof(char *) * row, sizeof(void *));
    if (!c->xyz || !c->patch_names) { st = ALWAN_E_NOMEM; goto fail; }
    memset(c->xyz, 0, sizeof(ALWAN_CORE_T) * row * 3);
    memset(c->patch_names, 0, sizeof(char *) * row);
    if (nspec >= 2) {
        c->spectra = (ALWAN_CORE_T *)ALWAN_ALLOC(alwan_safe_array_size(row * nspec, sizeof(ALWAN_CORE_T)), sizeof(ALWAN_CORE_T));
        if (!c->spectra) { st = ALWAN_E_NOMEM; goto fail; }
        c->bands = nspec;
        c->spectral_min_nm = (ALWAN_CORE_T)spec_nm[0];
        c->spectral_max_nm = (ALWAN_CORE_T)spec_nm[nspec - 1];
    }

    /* --- pass 2 --- */
    pos = bom;
    in_data = 0;
    in_format = 0;
    row = 0;
    while (pos < len) {
        char const *line = buf + pos;
        size_t llen = 0, n;
        while (pos + llen < len && buf[pos + llen] != '\n') llen++;
        pos += llen + (pos + llen < len ? 1 : 0);
        while (llen > 0 && alwan__chart_is_space(line[llen - 1])) llen--;
        while (llen > 0 && alwan__chart_is_space(line[0])) { line++; llen--; }
        if (llen == 0 || line[0] == '#') continue;
        if (alwan__chart_eq_n(line, llen, "BEGIN_DATA_FORMAT")) { in_format = 1; continue; }
        if (alwan__chart_eq_n(line, llen, "END_DATA_FORMAT"))   { in_format = 0; continue; }
        if (alwan__chart_eq_n(line, llen, "BEGIN_DATA")) { in_data = 1; continue; }
        if (alwan__chart_eq_n(line, llen, "END_DATA"))   { in_data = 0; continue; }
        if (!in_data || in_format) continue;

        n = alwan__chart_split(line, llen, cells, ALWAN_CHART_MAX_FIELDS);
        if (n != nfields) { st = ALWAN_E_INVALID; goto fail; }  /* row and format disagree on width */

        /* SAMPLE_NAME, else SAMPLE_ID, else the 1-based row number. */
        if (ix_name != (size_t)-1) {
            c->patch_names[row] = alwan__chart_strdup_n(cells[ix_name].p, cells[ix_name].len);
        } else if (ix_id != (size_t)-1) {
            c->patch_names[row] = alwan__chart_strdup_n(cells[ix_id].p, cells[ix_id].len);
        } else {
            char tmp[24];
            size_t v = row + 1, d = 0, k;
            do { tmp[d++] = (char)('0' + (v % 10)); v /= 10; } while (v && d < sizeof(tmp));
            for (k = 0; k < d / 2; k++) { char t = tmp[k]; tmp[k] = tmp[d - 1 - k]; tmp[d - 1 - k] = t; }
            c->patch_names[row] = alwan__chart_strdup_n(tmp, d);
        }
        if (!c->patch_names[row]) { st = ALWAN_E_NOMEM; goto fail; }

        if (c->spectra) {
            /* Reflectance is [0,1] or percent. The standard permits both and
             * marks neither, so the magnitude decides, per row. */
            int is_percent = 0;
            for (i = 0; i < nspec; i++) {
                int ok;
                double v = alwan__chart_strtod(cells[spec_col[i]].p, cells[spec_col[i]].len, &ok);
                if (!ok) { st = ALWAN_E_INVALID; goto fail; }
                if (v > 1.5) is_percent = 1;
                c->spectra[row * nspec + i] = (ALWAN_CORE_T)v;
            }
            if (is_percent) {
                for (i = 0; i < nspec; i++) c->spectra[row * nspec + i] *= (ALWAN_CORE_T)0.01;
            }
        }

        if (c->source == ALWAN_CHART_SOURCE_XYZ) {
            int okx, oky, okz;
            double X = alwan__chart_strtod(cells[ix_X].p, cells[ix_X].len, &okx);
            double Y = alwan__chart_strtod(cells[ix_Y].p, cells[ix_Y].len, &oky);
            double Z = alwan__chart_strtod(cells[ix_Z].p, cells[ix_Z].len, &okz);
            if (!okx || !oky || !okz) { st = ALWAN_E_INVALID; goto fail; }
            /* CGATS normalises Y to 100; alwan's XYZ is Y=1. */
            c->xyz[row * 3 + 0] = (ALWAN_CORE_T)(X * 0.01);
            c->xyz[row * 3 + 1] = (ALWAN_CORE_T)(Y * 0.01);
            c->xyz[row * 3 + 2] = (ALWAN_CORE_T)(Z * 0.01);
        } else if (c->source == ALWAN_CHART_SOURCE_LAB) {
            int okl, oka, okb;
            ALWAN_CORE_LAB lab;
            ALWAN_CORE_XYZ wp, xyz;
            lab.L = (ALWAN_CORE_T)alwan__chart_strtod(cells[ix_L].p, cells[ix_L].len, &okl);
            lab.a = (ALWAN_CORE_T)alwan__chart_strtod(cells[ix_a].p, cells[ix_a].len, &oka);
            lab.b = (ALWAN_CORE_T)alwan__chart_strtod(cells[ix_b].p, cells[ix_b].len, &okb);
            if (!okl || !oka || !okb) { st = ALWAN_E_INVALID; goto fail; }
            if (ALWAN_CORE_FNLIT(alwan_illuminant_white_point)(&wp, c->illuminant, c->observer) != ALWAN_OK) {
                st = ALWAN_E_INVALID;
                goto fail;
            }
            /* The _v core: a file's Lab is native whatever
             * ALWAN_NORMALIZE_RANGES does to the public API. */
            xyz = ALWAN_CORE_FNVLIT(alwan_lab_to_xyz)(lab, wp);
            c->xyz[row * 3 + 0] = xyz.x;
            c->xyz[row * 3 + 1] = xyz.y;
            c->xyz[row * 3 + 2] = xyz.z;
        }
        row++;
    }

    /* A spectral file integrates once, here, so alwan_chart_xyz is a lookup
     * whichever family the file carried. The spectra stay reachable through
     * alwan_chart_reflectance for a caller who wants another illuminant. */
    if (c->source == ALWAN_CHART_SOURCE_SPECTRAL) {
        ALWAN_CORE_SPD refl, illum, diffuser;
        ALWAN_CORE_XYZ white;
        memset(&refl, 0, sizeof(refl));
        memset(&illum, 0, sizeof(illum));
        memset(&diffuser, 0, sizeof(diffuser));
        st = ALWAN_CORE_FNLIT(alwan_spd_illuminant)(&illum, c->illuminant, ctx);
        if (st != ALWAN_OK) goto fail;
        st = ALWAN_CORE_FNLIT(alwan_spd_create)(&refl, c->spectral_min_nm, c->spectral_max_nm, c->bands, ctx);
        if (st != ALWAN_OK) { ALWAN_CORE_FNLIT(alwan_spd_destroy)(&illum, ctx); goto fail; }
        st = ALWAN_CORE_FNLIT(alwan_spd_create)(&diffuser, c->spectral_min_nm, c->spectral_max_nm, c->bands, ctx);
        if (st != ALWAN_OK) {
            ALWAN_CORE_FNLIT(alwan_spd_destroy)(&refl, ctx);
            ALWAN_CORE_FNLIT(alwan_spd_destroy)(&illum, ctx);
            goto fail;
        }

        /* alwan_xyz_from_spd returns the raw integral, which is what an
         * emissive SPD wants. A reflectance is relative: Y = 1 for a perfect
         * diffuser under the same light, on the same grid. Same normalisation
         * alwan_color_checker_data applies to its spectral targets, so the
         * embedded and loaded paths answer alike. */
        for (i = 0; i < diffuser.count; i++) diffuser.values[i] = (ALWAN_CORE_T)1;
        st = ALWAN_CORE_FNLIT(alwan_xyz_from_spd)(&white, &diffuser, &illum, c->observer,
                                                  ALWAN_INTEGRATE_SIMPSON, (ALWAN_CORE_T)0, ctx);
        if (st == ALWAN_OK && white.y <= (ALWAN_CORE_T)0) st = ALWAN_E_DIVZERO;
        if (st == ALWAN_OK) {
            for (row = 0; row < c->patch_count; row++) {
                ALWAN_CORE_XYZ xyz;
                for (i = 0; i < c->bands; i++) refl.values[i] = c->spectra[row * c->bands + i];
                st = ALWAN_CORE_FNLIT(alwan_xyz_from_spd)(&xyz, &refl, &illum, c->observer,
                                                          ALWAN_INTEGRATE_SIMPSON, (ALWAN_CORE_T)0, ctx);
                if (st != ALWAN_OK) break;
                c->xyz[row * 3 + 0] = xyz.x / white.y;
                c->xyz[row * 3 + 1] = xyz.y / white.y;
                c->xyz[row * 3 + 2] = xyz.z / white.y;
            }
        }
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&refl, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&illum, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&diffuser, ctx);
        if (st != ALWAN_OK) goto fail;
    }

    ALWAN_FREE(fields);
    ALWAN_FREE(cells);
    ALWAN_FREE(spec_col);
    ALWAN_FREE(spec_nm);
    *out = c;
    return ALWAN_OK;

fail:
    ALWAN_FREE(fields);
    ALWAN_FREE(cells);
    ALWAN_FREE(spec_col);
    ALWAN_FREE(spec_nm);
    ALWAN_CORE_FNLIT(alwan_chart_destroy)(c, ctx);
    return st;
}

/* ----------------------------------------------------------------
 * Load from a path
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_chart_load)(ALWAN_CORE_CHART **out, char const *path, alwan_ctx *ctx) {
    FILE *fh;
    char *buf;
    long size;
    size_t got;
    alwan_status st;

    if (out) *out = NULL;
    if (!out || !path) return ALWAN_E_INVALID;

    fh = fopen(path, "rb");
    if (!fh) return ALWAN_E_INVALID;
    if (fseek(fh, 0, SEEK_END) != 0) { fclose(fh); return ALWAN_E_INVALID; }
    size = ftell(fh);
    if (size < 0) { fclose(fh); return ALWAN_E_INVALID; }
    if (fseek(fh, 0, SEEK_SET) != 0) { fclose(fh); return ALWAN_E_INVALID; }

    buf = (char *)ALWAN_ALLOC((size_t)size + 1, 1);
    if (!buf) { fclose(fh); return ALWAN_E_NOMEM; }
    got = fread(buf, 1, (size_t)size, fh);
    fclose(fh);
    buf[got] = '\0';

    st = ALWAN_CORE_FNLIT(alwan_chart_load_buffer)(out, buf, got, ctx);
    ALWAN_FREE(buf);
    return st;
}

/* ----------------------------------------------------------------
 * Accessors
 * ---------------------------------------------------------------- */

size_t ALWAN_CORE_FNLIT(alwan_chart_num_patches)(ALWAN_CORE_CHART const *chart) {
    return chart ? chart->patch_count : 0;
}

size_t ALWAN_CORE_FNLIT(alwan_chart_num_bands)(ALWAN_CORE_CHART const *chart) {
    return chart ? chart->bands : 0;
}

alwan_chart_source ALWAN_CORE_FNLIT(alwan_chart_get_source)(ALWAN_CORE_CHART const *chart) {
    return chart ? chart->source : ALWAN_CHART_SOURCE_XYZ;
}

alwan_status ALWAN_CORE_FNLIT(alwan_chart_native_illuminant)(alwan_illuminant *illuminant,
                                                             alwan_observer_type *observer,
                                                             ALWAN_CORE_CHART const *chart) {
    if (!chart) return ALWAN_E_INVALID;
    if (illuminant) *illuminant = chart->illuminant;
    if (observer)   *observer   = chart->observer;
    return ALWAN_OK;
}

char const *ALWAN_CORE_FNLIT(alwan_chart_patch_name)(ALWAN_CORE_CHART const *chart, size_t patch_index) {
    if (!chart || patch_index >= chart->patch_count) return NULL;
    return chart->patch_names[patch_index];
}

char const *ALWAN_CORE_FNLIT(alwan_chart_header)(ALWAN_CORE_CHART const *chart, char const *key) {
    size_t i;
    if (!chart || !key) return NULL;
    for (i = 0; i < chart->header_count; i++) {
        if (alwan__chart_eq_n(chart->header_keys[i], strlen(chart->header_keys[i]), key)) {
            return chart->header_values[i];
        }
    }
    return NULL;
}

size_t ALWAN_CORE_FNLIT(alwan_chart_num_headers)(ALWAN_CORE_CHART const *chart) {
    return chart ? chart->header_count : 0;
}

alwan_status ALWAN_CORE_FNLIT(alwan_chart_header_at)(char const **key, char const **value,
                                                     ALWAN_CORE_CHART const *chart, size_t index) {
    if (!chart || index >= chart->header_count) return ALWAN_E_INVALID;
    if (key)   *key   = chart->header_keys[index];
    if (value) *value = chart->header_values[index];
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_chart_xyz)(ALWAN_CORE_XYZ *xyz,
                                               ALWAN_CORE_CHART const *chart, size_t patch_index) {
    if (!xyz || !chart || patch_index >= chart->patch_count) return ALWAN_E_INVALID;
    xyz->x = chart->xyz[patch_index * 3 + 0];
    xyz->y = chart->xyz[patch_index * 3 + 1];
    xyz->z = chart->xyz[patch_index * 3 + 2];
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_chart_reflectance)(ALWAN_CORE_SPD *out,
                                                       ALWAN_CORE_CHART const *chart,
                                                       size_t patch_index, alwan_ctx *ctx) {
    alwan_status st;
    size_t i;
    if (!out || !chart || patch_index >= chart->patch_count) return ALWAN_E_INVALID;
    if (!chart->spectra) return ALWAN_E_NODATA;
    st = ALWAN_CORE_FNLIT(alwan_spd_create)(out, chart->spectral_min_nm, chart->spectral_max_nm,
                                            chart->bands, ctx);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < chart->bands; i++) out->values[i] = chart->spectra[patch_index * chart->bands + i];
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Write
 *
 * Emits the dialect the reader accepts, so a round trip through
 * alwan_chart_write and alwan_chart_load is the format's own test. Spectra
 * are written when the chart carries them, since that is the measurement,
 * and the tristimulus goes alongside so a reader wanting only XYZ does not
 * have to integrate.
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_chart_write_buffer)(char *buf, size_t *bytes_written, size_t buf_size,
                                                        ALWAN_CORE_CHART const *chart) {
    alwan__chart_sink s;
    size_t p, i;
    static char const *const passthrough[] = {
        "DESCRIPTOR", "ORIGINATOR", "MANUFACTURER", "SERIAL", "CREATED", "TARGET_INSTRUMENT"
    };

    if (!chart || !bytes_written) return ALWAN_E_INVALID;
    s.buf = buf;
    s.cap = buf ? buf_size : 0;
    s.len = 0;

    alwan__chart_puts(&s, "OQM\n");
    for (i = 0; i < sizeof(passthrough) / sizeof(passthrough[0]); i++) {
        char const *v = ALWAN_CORE_FNLIT(alwan_chart_header)(chart, passthrough[i]);
        if (!v || !*v) continue;
        alwan__chart_puts(&s, passthrough[i]);
        alwan__chart_puts(&s, " \"");
        alwan__chart_puts(&s, v);
        alwan__chart_puts(&s, "\"\n");
    }
    {   /* Always written: a reader that has to guess these is a reader that
         * will guess wrong on somebody's file. */
        char const *ill = ALWAN_CORE_FNLIT(alwan_chart_header)(chart, "ILLUMINANT");
        alwan__chart_puts(&s, "ILLUMINANT \"");
        alwan__chart_puts(&s, ill && *ill ? ill : "D50");
        alwan__chart_puts(&s, "\"\nOBSERVER \"");
        alwan__chart_puts(&s, chart->observer == ALWAN_OBSERVER_CIE_1964_10DEG ? "10" : "2");
        alwan__chart_puts(&s, "\"\n");
    }

    alwan__chart_puts(&s, "NUMBER_OF_FIELDS ");
    alwan__chart_put_size(&s, 5 + chart->bands);
    alwan__chart_puts(&s, "\nBEGIN_DATA_FORMAT\nSAMPLE_ID SAMPLE_NAME XYZ_X XYZ_Y XYZ_Z");
    for (i = 0; i < chart->bands; i++) {
        double nm = chart->bands < 2
            ? (double)chart->spectral_min_nm
            : (double)chart->spectral_min_nm +
              ((double)chart->spectral_max_nm - (double)chart->spectral_min_nm) *
              (double)i / (double)(chart->bands - 1);
        alwan__chart_puts(&s, " SPEC_");
        alwan__chart_put_size(&s, (size_t)(nm + 0.5));
    }
    alwan__chart_puts(&s, "\nEND_DATA_FORMAT\nNUMBER_OF_SETS ");
    alwan__chart_put_size(&s, chart->patch_count);
    alwan__chart_puts(&s, "\nBEGIN_DATA\n");

    for (p = 0; p < chart->patch_count; p++) {
        alwan__chart_put_size(&s, p + 1);
        alwan__chart_puts(&s, " \"");
        alwan__chart_puts(&s, chart->patch_names[p] ? chart->patch_names[p] : "");
        alwan__chart_puts(&s, "\"");
        for (i = 0; i < 3; i++) {
            alwan__chart_puts(&s, " ");
            /* back to the Y=100 the standard normalises to */
            alwan__chart_put_num(&s, (double)chart->xyz[p * 3 + i] * 100.0, 4);
        }
        for (i = 0; i < chart->bands; i++) {
            alwan__chart_puts(&s, " ");
            alwan__chart_put_num(&s, (double)chart->spectra[p * chart->bands + i], 6);
        }
        alwan__chart_puts(&s, "\n");
    }
    alwan__chart_puts(&s, "END_DATA\n");

    *bytes_written = s.len;
    if (buf && s.len >= buf_size) return ALWAN_E_RANGE;   /* would not fit, counting the NUL */
    if (buf) buf[s.len] = '\0';
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_chart_write)(char const *path, ALWAN_CORE_CHART const *chart) {
    FILE *fh;
    char *buf;
    size_t need = 0;
    alwan_status st;

    if (!path || !chart) return ALWAN_E_INVALID;
    st = ALWAN_CORE_FNLIT(alwan_chart_write_buffer)(NULL, &need, 0, chart);
    if (st != ALWAN_OK) return st;

    buf = (char *)ALWAN_ALLOC(need + 1, 1);
    if (!buf) return ALWAN_E_NOMEM;
    st = ALWAN_CORE_FNLIT(alwan_chart_write_buffer)(buf, &need, need + 1, chart);
    if (st != ALWAN_OK) { ALWAN_FREE(buf); return st; }

    fh = fopen(path, "wb");
    if (!fh) { ALWAN_FREE(buf); return ALWAN_E_INVALID; }
    if (fwrite(buf, 1, need, fh) != need) { fclose(fh); ALWAN_FREE(buf); return ALWAN_E_INVALID; }
    fclose(fh);
    ALWAN_FREE(buf);
    return ALWAN_OK;
}
