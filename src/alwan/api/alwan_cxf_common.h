/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * CxF3 (ISO 17972-1:2015, Colour Exchange Format) reading, to the point where
 * a measurement is numbers rather than XML. The result feeds the same chart
 * object the CGATS reader fills, so every accessor a caller already uses
 * works on a CxF file without knowing it read one.
 *
 * NOT TESTABLE AGAINST A REFERENCE, and worth saying plainly. There is no
 * second implementation to call the way OpenColorIO is called for CLF and
 * colour-science for the colorimetry: colour-science has no CxF reader, and
 * neither does any package alwan already depends on. What the element shapes
 * below mean was written from:
 *
 *   ISO 17972-1:2015, Graphic technology -- Colour data exchange format
 *     (CxF/X) -- Part 1: Relationship to CxF3 (XML)
 *   The CxF3 core schema, http://colorexchangeformat.com/CxF3-core
 *   X-Rite's CxF3 developer documentation, which is the reference the
 *     instrument vendors' own files follow
 *
 * What suite 163 can still pin, and does: the colorimetry that comes out of a
 * spectrum is colour-science's answer for the same spectrum, and the same
 * measurement written as CGATS and as CxF has to load to the same chart. The
 * first pins the maths, the second pins the parse against a reader that is
 * itself pinned. Neither pins the XML shape, and no test here pretends to.
 *
 * Precision-independent on purpose. Numbers are parsed and held in double
 * whatever the chart's precision, and an f32 chart narrows once at the store,
 * which is the same rule alwan_chart_common.h states. Keeping the parse here
 * rather than in a per-precision .inc also means it is compiled once.
 */
#ifndef ALWAN_CXF_COMMON_H
#define ALWAN_CXF_COMMON_H

#include "alwan_chart_common.h"
#include "alwan_xml_common.h"

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

enum {
    ALWAN_CXF_NAME_CAP = 96,
    ALWAN_CXF_VALUE_CAP = 256,
    /* A measurement file with more objects than this is not a chart. The cap
     * bounds the allocation a declared count could otherwise ask for. */
    ALWAN_CXF_MAX_OBJECTS = 100000,
    /* 360 to 780 at 1 nm is 421, and no instrument reports finer. */
    ALWAN_CXF_MAX_BANDS = 4096
};

/* The device model a file's DeviceColorValues carried. */
enum { ALWAN_CXF_DEV_NONE = 0, ALWAN_CXF_DEV_CMYK = 1, ALWAN_CXF_DEV_RGB = 2 };

typedef struct {
    char  *name;                  /* Name, else Id, else the 1-based index */
    double lab[3];
    double xyz[3];                /* as written, on the 0 to 100 scale */
    double dev[4];
    int    has_lab, has_xyz, has_dev;
} alwan__cxf_object;

typedef struct {
    alwan__cxf_object *objects;
    size_t             count;

    double *spectra;              /* count * bands, or NULL */
    size_t  bands;
    double  start_nm, increment_nm;

    char  **hdr_keys;
    char  **hdr_values;
    size_t  hdr_count;

    char illuminant[ALWAN_CXF_VALUE_CAP];
    char observer[ALWAN_CXF_VALUE_CAP];
    int  dev_model;

    /* Where the grid came from. A ReflectanceSpectrum's own StartWL and
     * Increment win; a ColorSpecification's WavelengthRange fills in for a
     * file that states the grid once instead of on every spectrum; the
     * schema's 380 and 10 are the last resort. */
    int have_start, have_inc;
} alwan__cxf_doc;

static void alwan__cxf_free(alwan__cxf_doc *d) {
    size_t i;
    if (!d) return;
    if (d->objects) {
        for (i = 0; i < d->count; i++) ALWAN_FREE(d->objects[i].name);
        ALWAN_FREE(d->objects);
    }
    if (d->hdr_keys) {
        for (i = 0; i < d->hdr_count; i++) ALWAN_FREE(d->hdr_keys[i]);
        ALWAN_FREE(d->hdr_keys);
    }
    if (d->hdr_values) {
        for (i = 0; i < d->hdr_count; i++) ALWAN_FREE(d->hdr_values[i]);
        ALWAN_FREE(d->hdr_values);
    }
    ALWAN_FREE(d->spectra);
    memset(d, 0, sizeof *d);
}

static alwan_status alwan__cxf_add_header(alwan__cxf_doc *d, char const *key, char const *val) {
    char **nk = (char **)ALWAN_ALLOC(sizeof(char *) * (d->hdr_count + 1), sizeof(void *));
    char **nv = (char **)ALWAN_ALLOC(sizeof(char *) * (d->hdr_count + 1), sizeof(void *));
    size_t i;
    if (!nk || !nv) { ALWAN_FREE(nk); ALWAN_FREE(nv); return ALWAN_E_NOMEM; }
    for (i = 0; i < d->hdr_count; i++) { nk[i] = d->hdr_keys[i]; nv[i] = d->hdr_values[i]; }
    ALWAN_FREE(d->hdr_keys);
    ALWAN_FREE(d->hdr_values);
    d->hdr_keys = nk;
    d->hdr_values = nv;
    nk[d->hdr_count] = alwan__chart_strdup_n(key, strlen(key));
    nv[d->hdr_count] = alwan__chart_strdup_n(val, strlen(val));
    if (!nk[d->hdr_count] || !nv[d->hdr_count]) return ALWAN_E_NOMEM;
    d->hdr_count++;
    return ALWAN_OK;
}

/* Case-insensitive compare of a NUL-terminated local name. */
static int alwan__cxf_is(char const *name, char const *want) {
    char const *local = alwan__xml_local_name(name);
    return alwan__chart_eq_n(local, strlen(local), want);
}

/* The text between an element's open and close tag, trimmed, as a counted
 * span. The scanner is left just before the close tag. */
static void alwan__cxf_text(alwan__xml_scan *s, char const **out, size_t *len) {
    char const *start = s->p;
    char const *stop;
    while (s->p < s->end && *s->p != '<') s->p++;
    stop = s->p;
    while (start < stop && alwan__chart_is_space(*start)) start++;
    while (stop > start && alwan__chart_is_space(stop[-1])) stop--;
    *out = start;
    *len = (size_t)(stop - start);
}

/* One number from an element's text content. */
static int alwan__cxf_text_number(alwan__xml_scan *s, double *out) {
    char const *t;
    size_t n;
    int ok = 0;
    alwan__cxf_text(s, &t, &n);
    if (n == 0) return 0;
    *out = alwan__chart_strtod(t, n, &ok);
    return ok;
}

/* Whitespace-separated numbers from an element's text content, into `dst` when
 * it is not NULL. Always reports how many there were, so the counting pass and
 * the filling pass share one reader and cannot disagree about the length. */
static int alwan__cxf_number_list(alwan__xml_scan *s, double *dst, size_t cap, size_t *count) {
    size_t n = 0;
    for (;;) {
        char const *start;
        size_t span;
        int ok = 0;
        double v;
        while (s->p < s->end && alwan__chart_is_space(*s->p)) s->p++;
        if (s->p >= s->end || *s->p == '<') break;
        start = s->p;
        while (s->p < s->end && *s->p != '<' && !alwan__chart_is_space(*s->p)) s->p++;
        span = (size_t)(s->p - start);
        v = alwan__chart_strtod(start, span, &ok);
        if (!ok) return 0;
        if (dst && n < cap) dst[n] = v;
        n++;
        if (n > ALWAN_CXF_MAX_BANDS) return 0;
    }
    *count = n;
    return 1;
}

/* Read the three or four children of a colour element into `v`, matching each
 * child by its local name against `names`. Returns the number matched, or -1
 * on malformed input. Stops on the parent's own close tag. */
static int alwan__cxf_components(alwan__xml_scan *s, char const *const *names,
                                 int n_names, double *v) {
    char name[ALWAN_CXF_NAME_CAP], attr[ALWAN_CXF_NAME_CAP], value[ALWAN_CXF_VALUE_CAP];
    int matched = 0;
    for (;;) {
        int self = 0, r, k;
        alwan__xml_skip_space(s);
        if (s->p >= s->end) return -1;
        if (alwan__xml_starts_with(s, "<!--")) {
            if (!alwan__xml_skip_past(s, "-->")) return -1;
            continue;
        }
        if (s->p + 1 < s->end && s->p[0] == '<' && s->p[1] == '/') {
            if (!alwan__xml_skip_past(s, ">")) return -1;      /* the element's own close */
            return matched;
        }
        if (*s->p != '<') { s->p++; continue; }
        s->p++;
        if (!alwan__xml_read_name(s, name, sizeof name)) return -1;
        while ((r = alwan__xml_read_attr(s, attr, sizeof attr,
                                         value, sizeof value, &self)) == 1) { /* ignored */ }
        if (r < 0) return -1;
        for (k = 0; k < n_names; k++) {
            if (alwan__cxf_is(name, names[k])) break;
        }
        if (k < n_names && !self) {
            double d = 0.0;
            if (!alwan__cxf_text_number(s, &d)) return -1;
            v[k] = d;
            matched++;
        }
        if (!self && !alwan__xml_skip_element(s, name)) return -1;
    }
}

/* ----------------------------------------------------------------
 * The scan
 *
 * One walk over the buffer per pass. Pass 0 counts the objects and settles the
 * spectral grid; pass 1 fills. Two passes rather than a growth policy, for the
 * same reason the CGATS reader takes two: one exact allocation per array, and
 * a declared length can never disagree with what follows it.
 * ---------------------------------------------------------------- */

static alwan_status alwan__cxf_walk(alwan__cxf_doc *d, char const *buf, size_t len,
                                    int fill, int *is_cxf) {
    alwan__xml_scan s;
    char name[ALWAN_CXF_NAME_CAP], attr[ALWAN_CXF_NAME_CAP], value[ALWAN_CXF_VALUE_CAP];
    size_t obj = 0;
    int in_file_info = 0;

    s.p = buf;
    s.end = buf + len;
    if (!alwan__xml_skip_prologue(&s)) return ALWAN_E_NODATA;
    s.p++;
    if (!alwan__xml_read_name(&s, name, sizeof name)) return ALWAN_E_NODATA;
    if (!alwan__cxf_is(name, "CxF")) return ALWAN_E_NODATA;      /* not a CxF file */
    /* Past this point the buffer IS a CxF, and every later failure is a fault
     * in the file rather than a reason to try another reader on it. */
    if (is_cxf) *is_cxf = 1;
    {
        int self = 0, r;
        while ((r = alwan__xml_read_attr(&s, attr, sizeof attr,
                                         value, sizeof value, &self)) == 1) { /* namespaces */ }
        if (r < 0) return ALWAN_E_INVALID;
        if (self) return ALWAN_E_NODATA;                          /* an empty CxF */
    }

    for (;;) {
        int self = 0, r;
        alwan__xml_skip_space(&s);
        if (s.p >= s.end) break;
        if (alwan__xml_starts_with(&s, "<!--")) {
            if (!alwan__xml_skip_past(&s, "-->")) return ALWAN_E_INVALID;
            continue;
        }
        if (s.p + 1 < s.end && s.p[0] == '<' && s.p[1] == '/') {
            char const *save = s.p;
            if (!alwan__xml_skip_past(&s, ">")) return ALWAN_E_INVALID;
            /* The close of FileInformation ends the header block; every other
             * close tag here just ends a container we walked into. */
            if (in_file_info) {
                alwan__xml_scan probe;
                probe.p = save + 2;
                probe.end = s.end;
                if (alwan__xml_read_name(&probe, name, sizeof name) &&
                    alwan__cxf_is(name, "FileInformation")) {
                    in_file_info = 0;
                }
            }
            continue;
        }
        if (*s.p != '<') { s.p++; continue; }
        s.p++;
        if (!alwan__xml_read_name(&s, name, sizeof name)) return ALWAN_E_INVALID;

        /* Containers are walked into rather than skipped, which is what keeps
         * this a flat loop: Resources, ObjectCollection, ColorValues and the
         * rest carry no data of their own. */
        if (alwan__cxf_is(name, "Resources") || alwan__cxf_is(name, "ObjectCollection") ||
            alwan__cxf_is(name, "ColorSpecificationCollection") ||
            alwan__cxf_is(name, "ColorValues") || alwan__cxf_is(name, "DeviceColorValues") ||
            alwan__cxf_is(name, "TristimulusSpec") || alwan__cxf_is(name, "MeasurementSpec") ||
            alwan__cxf_is(name, "ColorSpecification")) {
            while ((r = alwan__xml_read_attr(&s, attr, sizeof attr,
                                             value, sizeof value, &self)) == 1) { /* ignored */ }
            if (r < 0) return ALWAN_E_INVALID;
            continue;
        }

        if (alwan__cxf_is(name, "FileInformation")) {
            while ((r = alwan__xml_read_attr(&s, attr, sizeof attr,
                                             value, sizeof value, &self)) == 1) { /* ignored */ }
            if (r < 0) return ALWAN_E_INVALID;
            if (!self) in_file_info = 1;
            continue;
        }

        if (alwan__cxf_is(name, "Object")) {
            char id[ALWAN_CXF_VALUE_CAP], nm[ALWAN_CXF_VALUE_CAP];
            id[0] = nm[0] = '\0';
            while ((r = alwan__xml_read_attr(&s, attr, sizeof attr,
                                             value, sizeof value, &self)) == 1) {
                if (alwan__cxf_is(attr, "Id")) memcpy(id, value, strlen(value) + 1);
                else if (alwan__cxf_is(attr, "Name")) memcpy(nm, value, strlen(value) + 1);
            }
            if (r < 0) return ALWAN_E_INVALID;
            if (fill) {
                char fallback[24];
                char const *use = nm[0] ? nm : (id[0] ? id : NULL);
                if (obj >= d->count) return ALWAN_E_INVALID;
                if (!use) {
                    size_t v = obj + 1, k = 0, j;
                    char tmp[24];
                    do { tmp[k++] = (char)('0' + (v % 10)); v /= 10; } while (v && k < sizeof tmp);
                    for (j = 0; j < k; j++) fallback[j] = tmp[k - 1 - j];
                    fallback[k] = '\0';
                    use = fallback;
                }
                d->objects[obj].name = alwan__chart_strdup_n(use, strlen(use));
                if (!d->objects[obj].name) return ALWAN_E_NOMEM;
            }
            /* An Object with no body still counts. It stays a hole, which the
             * chart loader's family check refuses rather than fills in. */
            obj++;
            if (!fill && obj > ALWAN_CXF_MAX_OBJECTS) return ALWAN_E_RANGE;
            continue;
        }

        if (alwan__cxf_is(name, "ReflectanceSpectrum")) {
            double start = 380.0, inc = 10.0;
            int have_start = 0, have_inc = 0;
            size_t got = 0;
            while ((r = alwan__xml_read_attr(&s, attr, sizeof attr,
                                             value, sizeof value, &self)) == 1) {
                int ok = 0;
                if (alwan__cxf_is(attr, "StartWL")) {
                    start = alwan__chart_strtod(value, strlen(value), &ok);
                    have_start = ok;
                } else if (alwan__cxf_is(attr, "Increment")) {
                    inc = alwan__chart_strtod(value, strlen(value), &ok);
                    have_inc = ok;
                }
            }
            if (r < 0) return ALWAN_E_INVALID;
            if (self) continue;
            if (obj == 0) return ALWAN_E_INVALID;                /* a spectrum with no object */
            if (!fill) {
                if (!alwan__cxf_number_list(&s, NULL, 0, &got)) return ALWAN_E_INVALID;
                if (got < 2) return ALWAN_E_INVALID;
                if (d->bands == 0) {
                    d->bands = got;
                    if (have_start) { d->start_nm = start; d->have_start = 1; }
                    if (have_inc) { d->increment_nm = inc; d->have_inc = 1; }
                } else if (got != d->bands ||
                           (have_start && d->have_start && start != d->start_nm) ||
                           (have_inc && d->have_inc && inc != d->increment_nm)) {
                    /* Two grids in one file. Resampling one onto the other
                     * would be a guess about which the caller wanted. */
                    return ALWAN_E_INVALID;
                }
            } else {
                double *dst = d->spectra ? d->spectra + (obj - 1) * d->bands : NULL;
                if (!alwan__cxf_number_list(&s, dst, d->bands, &got)) return ALWAN_E_INVALID;
                if (got != d->bands) return ALWAN_E_INVALID;
            }
            if (!alwan__xml_skip_element(&s, name)) return ALWAN_E_INVALID;
            continue;
        }

        if (alwan__cxf_is(name, "ColorCIELab") || alwan__cxf_is(name, "ColorCIEXYZ") ||
            alwan__cxf_is(name, "ColorCMYK") || alwan__cxf_is(name, "ColorRGB") ||
            alwan__cxf_is(name, "ColorSRGB")) {
            static char const *const lab_names[3] = { "L", "A", "B" };
            static char const *const xyz_names[3] = { "X", "Y", "Z" };
            static char const *const cmyk_names[4] = { "Cyan", "Magenta", "Yellow", "Black" };
            static char const *const rgb_names[3] = { "R", "G", "B" };
            double v[4] = { 0.0, 0.0, 0.0, 0.0 };
            int n;
            while ((r = alwan__xml_read_attr(&s, attr, sizeof attr,
                                             value, sizeof value, &self)) == 1) { /* ignored */ }
            if (r < 0) return ALWAN_E_INVALID;
            if (self) continue;
            if (obj == 0) return ALWAN_E_INVALID;
            if (alwan__cxf_is(name, "ColorCIELab")) {
                n = alwan__cxf_components(&s, lab_names, 3, v);
                if (n < 0) return ALWAN_E_INVALID;
                if (n == 3 && fill) {
                    d->objects[obj - 1].lab[0] = v[0];
                    d->objects[obj - 1].lab[1] = v[1];
                    d->objects[obj - 1].lab[2] = v[2];
                    d->objects[obj - 1].has_lab = 1;
                }
            } else if (alwan__cxf_is(name, "ColorCIEXYZ")) {
                n = alwan__cxf_components(&s, xyz_names, 3, v);
                if (n < 0) return ALWAN_E_INVALID;
                if (n == 3 && fill) {
                    d->objects[obj - 1].xyz[0] = v[0];
                    d->objects[obj - 1].xyz[1] = v[1];
                    d->objects[obj - 1].xyz[2] = v[2];
                    d->objects[obj - 1].has_xyz = 1;
                }
            } else if (alwan__cxf_is(name, "ColorCMYK")) {
                n = alwan__cxf_components(&s, cmyk_names, 4, v);
                if (n < 0) return ALWAN_E_INVALID;
                if (n == 4) {
                    if (!fill) d->dev_model = ALWAN_CXF_DEV_CMYK;
                    else {
                        int k;
                        for (k = 0; k < 4; k++) d->objects[obj - 1].dev[k] = v[k];
                        d->objects[obj - 1].has_dev = 1;
                    }
                }
            } else {
                n = alwan__cxf_components(&s, rgb_names, 3, v);
                if (n < 0) return ALWAN_E_INVALID;
                if (n == 3) {
                    if (!fill) {
                        if (d->dev_model == ALWAN_CXF_DEV_NONE) d->dev_model = ALWAN_CXF_DEV_RGB;
                    } else if (d->dev_model == ALWAN_CXF_DEV_RGB) {
                        d->objects[obj - 1].dev[0] = v[0];
                        d->objects[obj - 1].dev[1] = v[1];
                        d->objects[obj - 1].dev[2] = v[2];
                        d->objects[obj - 1].dev[3] = 0.0;
                        d->objects[obj - 1].has_dev = 1;
                    }
                }
            }
            continue;
        }

        if (alwan__cxf_is(name, "WavelengthRange")) {
            while ((r = alwan__xml_read_attr(&s, attr, sizeof attr,
                                             value, sizeof value, &self)) == 1) {
                int ok = 0;
                double x = alwan__chart_strtod(value, strlen(value), &ok);
                if (!ok) continue;
                if (alwan__cxf_is(attr, "StartWL") && !d->have_start) {
                    d->start_nm = x;
                    d->have_start = 1;
                } else if (alwan__cxf_is(attr, "Increment") && !d->have_inc) {
                    d->increment_nm = x;
                    d->have_inc = 1;
                }
            }
            if (r < 0) return ALWAN_E_INVALID;
            if (!self && !alwan__xml_skip_element(&s, name)) return ALWAN_E_INVALID;
            continue;
        }

        if (alwan__cxf_is(name, "Illuminant") || alwan__cxf_is(name, "Observer")) {
            char const *t;
            size_t n;
            int is_ill = alwan__cxf_is(name, "Illuminant");
            while ((r = alwan__xml_read_attr(&s, attr, sizeof attr,
                                             value, sizeof value, &self)) == 1) { /* ignored */ }
            if (r < 0) return ALWAN_E_INVALID;
            if (self) continue;
            alwan__cxf_text(&s, &t, &n);
            if (n >= ALWAN_CXF_VALUE_CAP) n = ALWAN_CXF_VALUE_CAP - 1;
            /* The first specification wins. A file with several is describing
             * several measurement conditions, and picking the last would make
             * the answer depend on file order. */
            if (is_ill && !d->illuminant[0]) {
                memcpy(d->illuminant, t, n);
                d->illuminant[n] = '\0';
            } else if (!is_ill && !d->observer[0]) {
                memcpy(d->observer, t, n);
                d->observer[n] = '\0';
            }
            if (!alwan__xml_skip_element(&s, name)) return ALWAN_E_INVALID;
            continue;
        }

        /* Inside FileInformation every leaf is a header. Elsewhere an element
         * this reader does not know is skipped whole: CxF carries custom
         * namespaces by design, and refusing a file for one would make the
         * reader useless on real instrument output. */
        {
            char elem[ALWAN_CXF_NAME_CAP];
            size_t ln = strlen(name);
            if (ln + 1 > sizeof elem) return ALWAN_E_INVALID;
            memcpy(elem, name, ln + 1);
            while ((r = alwan__xml_read_attr(&s, attr, sizeof attr,
                                             value, sizeof value, &self)) == 1) { /* ignored */ }
            if (r < 0) return ALWAN_E_INVALID;
            if (self) continue;
            if (in_file_info && fill) {
                char const *t;
                size_t n;
                char key[ALWAN_CXF_NAME_CAP], val[ALWAN_CXF_VALUE_CAP];
                char const *local = alwan__xml_local_name(elem);
                size_t k = strlen(local);
                alwan__cxf_text(&s, &t, &n);
                if (n >= sizeof val) n = sizeof val - 1;
                if (k >= sizeof key) k = sizeof key - 1;
                for (ln = 0; ln < k; ln++) key[ln] = alwan__chart_upper(local[ln]);
                key[k] = '\0';
                memcpy(val, t, n);
                val[n] = '\0';
                if (n > 0) {
                    alwan_status hst = alwan__cxf_add_header(d, key, val);
                    if (hst != ALWAN_OK) return hst;
                }
            }
            if (!alwan__xml_skip_element(&s, elem)) return ALWAN_E_INVALID;
        }
    }

    if (!fill) d->count = obj;
    else if (obj != d->count) return ALWAN_E_INVALID;
    return ALWAN_OK;
}

/* Read a CxF3 buffer. `*is_cxf` is set as soon as the root element proves the
 * bytes are one, which is what separates "try another reader" from "this file
 * is broken": the chart loader falls through to CGATS only while it is 0. */
static alwan_status alwan__cxf_parse(alwan__cxf_doc *d, char const *buf, size_t len, int *is_cxf) {
    alwan_status st;
    memset(d, 0, sizeof *d);
    if (is_cxf) *is_cxf = 0;
    st = alwan__cxf_walk(d, buf, len, 0, is_cxf);
    if (st != ALWAN_OK) { alwan__cxf_free(d); return st; }
    if (d->count == 0) { alwan__cxf_free(d); return ALWAN_E_NODATA; }

    d->objects = (alwan__cxf_object *)ALWAN_ALLOC(sizeof(*d->objects) * d->count, sizeof(void *));
    if (!d->objects) { alwan__cxf_free(d); return ALWAN_E_NOMEM; }
    memset(d->objects, 0, sizeof(*d->objects) * d->count);

    if (d->bands >= 2) {
        d->spectra = (double *)ALWAN_ALLOC(sizeof(double) * d->count * d->bands, sizeof(double));
        if (!d->spectra) { alwan__cxf_free(d); return ALWAN_E_NOMEM; }
        memset(d->spectra, 0, sizeof(double) * d->count * d->bands);
    }

    st = alwan__cxf_walk(d, buf, len, 1, NULL);
    if (st != ALWAN_OK) { alwan__cxf_free(d); return st; }
    if (d->increment_nm <= 0.0) d->increment_nm = 10.0;
    if (d->start_nm <= 0.0) d->start_nm = 380.0;
    return ALWAN_OK;
}

ALWAN_DIAG_POP

#endif /* ALWAN_CXF_COMMON_H */
