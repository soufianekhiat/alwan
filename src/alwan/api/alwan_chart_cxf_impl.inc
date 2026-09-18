/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * CxF3 into the chart object. The parse itself is precision-independent and
 * lives in alwan_cxf_common.h; this is only the copy, and the one place where
 * a double becomes an ALWAN_CORE_T.
 *
 * A CxF file loads into the same object a CGATS file does, so every accessor
 * works without knowing which format it came from. That is the whole point of
 * reading a second format: alwan_chart_xyz, alwan_chart_reflectance and
 * alwan_cmyk_model_from_chart do not grow a variant.
 *
 * No include guard: included once per precision from alwan_chart_impl.inc.
 */

/* Which family every object carries, in the same order the CGATS path uses:
 * a file stating its own tristimulus has already been asked this question by
 * the measuring lab, and re-deriving it would answer it again differently.
 * Every object has to carry the family, because a chart with a hole in it is
 * worse than a refusal. */
static alwan_status ALWAN_CORE_FNLIT(alwan__cxf_pick_source)(alwan_chart_source *out,
                                                             alwan__cxf_doc const *d) {
    size_t i;
    int all_xyz = 1, all_lab = 1;
    for (i = 0; i < d->count; i++) {
        if (!d->objects[i].has_xyz) all_xyz = 0;
        if (!d->objects[i].has_lab) all_lab = 0;
    }
    if (all_xyz) { *out = ALWAN_CHART_SOURCE_XYZ; return ALWAN_OK; }
    if (all_lab) { *out = ALWAN_CHART_SOURCE_LAB; return ALWAN_OK; }
    if (d->spectra && d->bands >= 2) { *out = ALWAN_CHART_SOURCE_SPECTRAL; return ALWAN_OK; }
    return ALWAN_E_NODATA;
}

static alwan_status ALWAN_CORE_FNLIT(alwan__chart_load_cxf)(ALWAN_CORE_CHART **out,
                                                            char const *buf, size_t len,
                                                            int *is_cxf, alwan_ctx *ctx) {
    alwan__cxf_doc d;
    ALWAN_CORE_CHART *c = NULL;
    alwan_status st;
    size_t i, k;
    int any_lab = 0;

    st = alwan__cxf_parse(&d, buf, len, is_cxf);
    if (st != ALWAN_OK) return st;

    c = (ALWAN_CORE_CHART *)ALWAN_ALLOC(sizeof(*c), sizeof(void *));
    if (!c) { alwan__cxf_free(&d); return ALWAN_E_NOMEM; }
    memset(c, 0, sizeof(*c));

    /* CxF states the condition in a ColorSpecification. When it states none,
     * the same D50 and 2 degree default the CGATS path takes applies, because
     * that is what a print measurement means when it says nothing. */
    c->illuminant = alwan__chart_illuminant_from_name(d.illuminant[0] ? d.illuminant : NULL);
    c->observer   = alwan__chart_observer_from_name(d.observer[0] ? d.observer : NULL);

    st = ALWAN_CORE_FNLIT(alwan__cxf_pick_source)(&c->source, &d);
    if (st != ALWAN_OK) goto done;

    c->patch_count = d.count;
    c->xyz = (ALWAN_CORE_T *)ALWAN_ALLOC(alwan_safe_array_size(d.count * 3, sizeof(ALWAN_CORE_T)),
                                          sizeof(ALWAN_CORE_T));
    c->patch_names = (char **)ALWAN_ALLOC(sizeof(char *) * d.count, sizeof(void *));
    if (!c->xyz || !c->patch_names) { st = ALWAN_E_NOMEM; goto done; }
    memset(c->xyz, 0, sizeof(ALWAN_CORE_T) * d.count * 3);
    memset(c->patch_names, 0, sizeof(char *) * d.count);

    for (i = 0; i < d.count; i++) {
        if (d.objects[i].has_lab) any_lab = 1;
    }
    if (any_lab) {
        c->lab = (ALWAN_CORE_T *)ALWAN_ALLOC(alwan_safe_array_size(d.count * 3, sizeof(ALWAN_CORE_T)),
                                              sizeof(ALWAN_CORE_T));
        if (!c->lab) { st = ALWAN_E_NOMEM; goto done; }
        memset(c->lab, 0, sizeof(ALWAN_CORE_T) * d.count * 3);
    }
    if (d.dev_model != ALWAN_CXF_DEV_NONE) {
        c->device_model = (d.dev_model == ALWAN_CXF_DEV_CMYK)
                          ? ALWAN_CHART_DEVICE_CMYK : ALWAN_CHART_DEVICE_RGB;
        c->device_channels = (d.dev_model == ALWAN_CXF_DEV_CMYK) ? 4 : 3;
        c->device = (ALWAN_CORE_T *)ALWAN_ALLOC(alwan_safe_array_size(d.count * 4, sizeof(ALWAN_CORE_T)),
                                                 sizeof(ALWAN_CORE_T));
        if (!c->device) { st = ALWAN_E_NOMEM; goto done; }
        memset(c->device, 0, sizeof(ALWAN_CORE_T) * d.count * 4);
    }
    if (d.spectra && d.bands >= 2) {
        c->spectra = (ALWAN_CORE_T *)ALWAN_ALLOC(alwan_safe_array_size(d.count * d.bands,
                                                                       sizeof(ALWAN_CORE_T)),
                                                  sizeof(ALWAN_CORE_T));
        if (!c->spectra) { st = ALWAN_E_NOMEM; goto done; }
        c->bands = d.bands;
        c->spectral_min_nm = (ALWAN_CORE_T)d.start_nm;
        c->spectral_max_nm = (ALWAN_CORE_T)(d.start_nm + d.increment_nm * (double)(d.bands - 1));
        for (i = 0; i < d.count; i++) {
            /* Reflectance is a fraction or a percentage, and CxF marks neither,
             * so the magnitude decides, per object. Same rule and same
             * threshold the CGATS path uses, because a file converted from one
             * format to the other must not change meaning. */
            int is_percent = 0;
            for (k = 0; k < d.bands; k++) {
                if (d.spectra[i * d.bands + k] > 1.5) { is_percent = 1; break; }
            }
            for (k = 0; k < d.bands; k++) {
                double v = d.spectra[i * d.bands + k];
                c->spectra[i * d.bands + k] = (ALWAN_CORE_T)(is_percent ? v * 0.01 : v);
            }
        }
    }

    for (i = 0; i < d.count; i++) {
        c->patch_names[i] = d.objects[i].name;
        d.objects[i].name = NULL;                    /* moved, not copied */
        if (!c->patch_names[i]) { st = ALWAN_E_NOMEM; goto done; }

        if (c->lab && d.objects[i].has_lab) {
            for (k = 0; k < 3; k++) c->lab[i * 3 + k] = (ALWAN_CORE_T)d.objects[i].lab[k];
        }
        if (c->device && d.objects[i].has_dev) {
            for (k = 0; k < 4; k++) c->device[i * 4 + k] = (ALWAN_CORE_T)d.objects[i].dev[k];
        }

        if (c->source == ALWAN_CHART_SOURCE_XYZ) {
            /* The CxF3 schema puts CIEXYZ on the 0 to 100 scale, as CGATS
             * does, and alwan's XYZ is Y = 1. */
            for (k = 0; k < 3; k++) c->xyz[i * 3 + k] = (ALWAN_CORE_T)(d.objects[i].xyz[k] * 0.01);
        } else if (c->source == ALWAN_CHART_SOURCE_LAB) {
            ALWAN_CORE_LAB lab;
            ALWAN_CORE_XYZ wp, xyz;
            lab.L = (ALWAN_CORE_T)d.objects[i].lab[0];
            lab.a = (ALWAN_CORE_T)d.objects[i].lab[1];
            lab.b = (ALWAN_CORE_T)d.objects[i].lab[2];
            if (ALWAN_CORE_FNLIT(alwan_illuminant_white_point)(&wp, c->illuminant, c->observer) != ALWAN_OK) {
                st = ALWAN_E_INVALID;
                goto done;
            }
            xyz = ALWAN_CORE_FNVLIT(alwan_lab_to_xyz)(lab, wp);
            c->xyz[i * 3 + 0] = xyz.x;
            c->xyz[i * 3 + 1] = xyz.y;
            c->xyz[i * 3 + 2] = xyz.z;
        }
    }

    /* FileInformation becomes header keys, so alwan_chart_header answers for a
     * CxF the way it answers for a CGATS file. */
    for (i = 0; i < d.hdr_count; i++) {
        st = ALWAN_CORE_FNLIT(alwan__chart_add_header)(c, d.hdr_keys[i], strlen(d.hdr_keys[i]),
                                                       d.hdr_values[i], strlen(d.hdr_values[i]));
        if (st != ALWAN_OK) goto done;
    }

    if (c->source == ALWAN_CHART_SOURCE_SPECTRAL) {
        st = ALWAN_CORE_FNLIT(alwan__chart_integrate_spectra)(c, ctx);
        if (st != ALWAN_OK) goto done;
    }

    st = ALWAN_OK;

done:
    alwan__cxf_free(&d);
    if (st != ALWAN_OK) {
        ALWAN_CORE_FNLIT(alwan_chart_destroy)(c, ctx);
        return st;
    }
    *out = c;
    return ALWAN_OK;
}
