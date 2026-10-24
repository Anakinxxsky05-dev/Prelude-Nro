// Copyright (C) 2026 Nextendo Network. See LICENSE.md.
#include "prelude_http.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
static const unsigned char *line_end(const unsigned char *p, const unsigned char *end) {
    for (; p < end && end - p >= 2; ++p) if (p[0] == '\r' && p[1] == '\n') return p;
    return NULL;
}
static bool equal(const unsigned char *p, size_t n, const char *text) {
    if (strlen(text) != n) return false;
    for (size_t i = 0; i < n; ++i) if (tolower(p[i]) != tolower((unsigned char)text[i])) return false;
    return true;
}
bool prelude_http_payload(const unsigned char *response, size_t size, unsigned char **body, size_t *length) {
    *body = NULL; *length = 0;
    const unsigned char *end = response + size, *p = line_end(response, end);
    if (!p) return false;
    p += 2;
    bool chunked = false, has_length = false;
    size_t expected = 0;
    for (;;) {
        const unsigned char *line = line_end(p, end);
        if (!line) return false;
        if (line == p) { p += 2; break; }
        const unsigned char *colon = memchr(p, ':', (size_t)(line - p));
        if (!colon) return false;
        const unsigned char *value = colon + 1, *last = line;
        while (value < last && (*value == ' ' || *value == '\t')) ++value;
        while (last > value && (last[-1] == ' ' || last[-1] == '\t')) --last;
        if (equal(p, (size_t)(colon - p), "content-length")) {
            size_t n = 0;
            if (value == last) return false;
            for (; value < last; ++value) {
                if (!isdigit(*value) || n > (SIZE_MAX - (*value - '0')) / 10) return false;
                n = n * 10 + (*value - '0');
            }
            if (has_length && n != expected) return false;
            has_length = true; expected = n;
        } else if (equal(p, (size_t)(colon - p), "transfer-encoding")) {
            if (chunked || !equal(value, (size_t)(last - value), "chunked")) return false;
            chunked = true;
        }
        p = line + 2;
    }
    if (chunked && has_length) return false;
    size_t available = (size_t)(end - p);
    if (!chunked && has_length && available != expected) return false;
    unsigned char *out = malloc(available + 1);
    if (!out) return false;
    size_t written = 0;
    if (!chunked) { memcpy(out, p, available); written = available; }
    else for (;;) {
        const unsigned char *line = line_end(p, end);
        if (!line) goto bad;
        size_t n = 0;
        const unsigned char *digit = p;
        for (; digit < line && *digit != ';'; ++digit) {
            unsigned char c = (unsigned char)tolower(*digit);
            unsigned v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : 16;
            if (v > 15 || n > (SIZE_MAX - v) / 16) goto bad;
            n = n * 16 + v;
        }
        if (digit == p) goto bad;
        p = line + 2;
        if (!n) {
            // Validate and discard trailers. The final empty line is mandatory.
            for (;;) {
                line = line_end(p, end);
                if (!line) goto bad;
                if (line == p) { p += 2; break; }
                if (!memchr(p, ':', (size_t)(line - p))) goto bad;
                p = line + 2;
            }
            if (p != end) goto bad;
            break;
        }
        if (n > (size_t)(end - p) || (size_t)(end - p) - n < 2 || p[n] != '\r' || p[n + 1] != '\n') goto bad;
        memcpy(out + written, p, n); written += n; p += n + 2;
    }
    out[written] = 0; *body = out; *length = written;
    return true;
bad:
    free(out); return false;
}
