/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Structural scanning for the two XML colour formats alwan reads: CLF
 * (SMPTE ST 2136-1) and CxF3 (ISO 17972-1). Neither needs a general XML
 * reader. Both have a known, shallow shape, and a scanner that understands
 * exactly that shape is less surface than a dependency would be. What it
 * does NOT do is the point of the contract below.
 *
 * Numbers are deliberately absent. CLF parses with strtod under a saved
 * LC_NUMERIC, and the chart reader hand-parses so a comma-decimal locale
 * cannot change what a file means. Putting a number reader here would force
 * one of those choices on the other, so each caller keeps its own.
 *
 * What this handles:
 *   - element open tags, their attributes, self-closing forms and close tags
 *   - comments, the XML declaration and a DOCTYPE, all skipped
 *   - skipping an element whole, nested children included
 *   - namespace prefixes, through the local-name comparison
 *
 * What this does NOT handle, because neither format needs it:
 *   - entity references beyond the five predefined ones, which are left as
 *     written; a caller comparing element names never sees one
 *   - CDATA sections
 *   - attribute values longer than the cap the caller passes, which are an
 *     error rather than a truncation
 *
 * Every function takes the scanner by pointer and advances it. A return of 0
 * or -1 leaves the scanner somewhere inside the construct that failed, so a
 * caller that means to continue has to reposition rather than retry.
 */
#ifndef ALWAN_XML_COMMON_H
#define ALWAN_XML_COMMON_H

#include <string.h>
#include <ctype.h>

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

typedef struct {
    char const *p;
    char const *end;
} alwan__xml_scan;

static void alwan__xml_skip_space(alwan__xml_scan *s) {
    while (s->p < s->end &&
           (*s->p == ' ' || *s->p == '\t' || *s->p == '\r' || *s->p == '\n')) {
        s->p++;
    }
}

static int alwan__xml_starts_with(alwan__xml_scan const *s, char const *lit) {
    size_t n = strlen(lit);
    return (size_t)(s->end - s->p) >= n && strncmp(s->p, lit, n) == 0;
}

/* Skip to just past the first occurrence of lit. Returns 0 at end of input,
 * with the scanner parked at the end rather than left mid-buffer. */
static int alwan__xml_skip_past(alwan__xml_scan *s, char const *lit) {
    size_t n = strlen(lit);
    while (s->p + n <= s->end) {
        if (strncmp(s->p, lit, n) == 0) { s->p += n; return 1; }
        s->p++;
    }
    s->p = s->end;
    return 0;
}

/* An element or attribute name character. The colon is included so a
 * namespace-prefixed name reads as one token; alwan__xml_local_name is what
 * strips the prefix afterwards. */
static int alwan__xml_name_char(char c) {
    return isalnum((unsigned char)c) || c == '_' || c == ':' || c == '.' || c == '-';
}

static int alwan__xml_read_name(alwan__xml_scan *s, char *buf, size_t cap) {
    size_t n = 0;
    while (s->p < s->end && alwan__xml_name_char(*s->p)) {
        if (n + 1 < cap) buf[n] = *s->p;
        n++;
        s->p++;
    }
    buf[n < cap ? n : cap - 1] = '\0';
    return n > 0 && n < cap;                     /* a name that did not fit is an error */
}

/* The part of a name after the last colon. CxF files in the wild use cc:, cxf:
 * and no prefix at all for the same elements, so every comparison has to be
 * against the local name or the reader only works on one vendor's files. */
static char const *alwan__xml_local_name(char const *name) {
    char const *colon = strrchr(name, ':');
    return colon ? colon + 1 : name;
}

/* One attribute of the element being read. Returns 1 on an attribute, 0 when
 * the element's tag ends, -1 on malformed input. *self_closing is set when the
 * tag ended with "/>", and is only meaningful on a return of 0. */
static int alwan__xml_read_attr(alwan__xml_scan *s, char *name, size_t name_cap,
                                char *value, size_t value_cap, int *self_closing) {
    char quote;
    size_t n = 0;
    alwan__xml_skip_space(s);
    if (s->p >= s->end) return -1;
    if (*s->p == '/') {
        s->p++;
        if (s->p >= s->end || *s->p != '>') return -1;
        s->p++;
        *self_closing = 1;
        return 0;
    }
    if (*s->p == '>') { s->p++; *self_closing = 0; return 0; }
    if (!alwan__xml_read_name(s, name, name_cap)) return -1;
    alwan__xml_skip_space(s);
    if (s->p >= s->end || *s->p != '=') return -1;
    s->p++;
    alwan__xml_skip_space(s);
    if (s->p >= s->end || (*s->p != '"' && *s->p != '\'')) return -1;
    quote = *s->p++;
    while (s->p < s->end && *s->p != quote) {
        if (n + 1 < value_cap) value[n] = *s->p;
        n++;
        s->p++;
    }
    if (s->p >= s->end) return -1;
    s->p++;
    if (n >= value_cap) return -1;               /* a value that did not fit is an error */
    value[n] = '\0';
    return 1;
}

/* Skip an element's content to its close tag, nested children included.
 * `name` is the name as it was read, prefix and all, because that is what the
 * close tag carries. */
static int alwan__xml_skip_element(alwan__xml_scan *s, char const *name) {
    char close[132];
    size_t n = strlen(name);
    if (n + 4 >= sizeof close) return 0;
    close[0] = '<'; close[1] = '/';
    memcpy(close + 2, name, n);
    close[n + 2] = '>';
    close[n + 3] = '\0';
    return alwan__xml_skip_past(s, close);
}

/* Step over the prologue: the XML declaration, comments and a DOCTYPE, in any
 * order and any number, leaving the scanner on the root element's '<'.
 * Returns 0 if the input runs out or a construct is unterminated. */
static int alwan__xml_skip_prologue(alwan__xml_scan *s) {
    for (;;) {
        alwan__xml_skip_space(s);
        if (s->p >= s->end) return 0;
        if (alwan__xml_starts_with(s, "<?")) {
            if (!alwan__xml_skip_past(s, "?>")) return 0;
            continue;
        }
        if (alwan__xml_starts_with(s, "<!--")) {
            if (!alwan__xml_skip_past(s, "-->")) return 0;
            continue;
        }
        if (alwan__xml_starts_with(s, "<!")) {
            if (!alwan__xml_skip_past(s, ">")) return 0;
            continue;
        }
        return *s->p == '<';
    }
}

ALWAN_DIAG_POP

#endif /* ALWAN_XML_COMMON_H */
