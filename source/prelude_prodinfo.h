// Copyright (C) 2026 Nextendo Network. See LICENSE.md.
#ifndef PRELUDE_PRODINFO_H
#define PRELUDE_PRODINFO_H
#include <string.h>
#include <stddef.h>

// Read only an actual key in [exosphere], never a comment or another section.
// 0 = missing/disabled, 1 = enabled, -1 = ambiguous/invalid value.
static inline int prelude_prodinfo_flag(const char *text, const char *key) {
    int result = 0, section = 0, seen = 0;
    const size_t keylen = strlen(key);
    for (const char *line = text; line && *line;) {
        const char *end = strchr(line, '\n');
        if (!end) end = line + strlen(line);
        const char *p = line;
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r')) ++p;
        if (p < end && *p == '[') {
            const char *close = memchr(p, ']', (size_t)(end-p));
            section = close && close-p == 10 && !memcmp(p, "[exosphere", 10);
        } else if (section && (size_t)(end-p) >= keylen && !memcmp(p,key,keylen)) {
            p += keylen;
            while (p < end && (*p==' ' || *p=='\t')) ++p;
            if (p < end && *p == '=') {
                ++p; while (p < end && (*p==' ' || *p=='\t')) ++p;
                if (p == end || (*p != '0' && *p != '1')) return -1;
                int value = *p++ - '0';
                while (p < end && (*p==' ' || *p=='\t' || *p=='\r')) ++p;
                if (p < end && *p != '#' && *p != ';') return -1;
                if (seen && result != value) return -1;
                result = value;
                seen = 1;
            }
        }
        line = *end ? end+1 : NULL;
    }
    return result;
}
#endif
