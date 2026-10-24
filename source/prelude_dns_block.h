// Copyright (C) 2026 Nextendo Network. See LICENSE.md.
#ifndef PRELUDE_DNS_BLOCK_H
#define PRELUDE_DNS_BLOCK_H
#include <stdlib.h>
#include <string.h>

// These rules are appended last: an original user file cannot reopen Nintendo
// through an earlier wildcard. Atmosphere matches bare domains separately from
// subdomains. Third-party hosts and unrelated user entries are retained.
static inline char *prelude_nintendo_block_hosts(const char *original) {
    static const char rules[] =
        "\n# PRELUDE NINTENDO DNS BLOCK - official Nintendo network disabled\n"
        "127.0.0.1 nintendo.*\n"
        "127.0.0.1 *.nintendo.*\n"
        "127.0.0.1 nintendo-europe.com\n"
        "127.0.0.1 *.nintendo-europe.com\n"
        "127.0.0.1 nintendowifi.net\n"
        "127.0.0.1 *.nintendowifi.net\n"
        "127.0.0.1 nintendoswitch.*\n"
        "127.0.0.1 *.nintendoswitch.*\n"
        "127.0.0.1 nintendods.cz\n"
        "127.0.0.1 *.nintendods.cz\n";
    size_t n = original ? strlen(original) : 0;
    if (n > 1024 * 1024) return NULL;
    char *out = malloc(n + sizeof(rules));
    if (!out) return NULL;
    if (n) memcpy(out, original, n);
    memcpy(out + n, rules, sizeof(rules));
    return out;
}
#endif
