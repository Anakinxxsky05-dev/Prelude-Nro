// Copyright (C) 2026 Nextendo Network. See LICENSE.md.
#ifndef PRELUDE_HTTP_H
#define PRELUDE_HTTP_H
#include <stdbool.h>
#include <stddef.h>
typedef struct {
    unsigned char *data;
    size_t length, capacity;
    bool complete;
} PreludeHttpHeaders;
#define PRELUDE_HTTP_HEADER_LIMIT (64 * 1024)
// 1: complete, 0: need more data, -1: size limit, -2: allocation failure.
// consumed excludes any body bytes delivered in the same network read.
int prelude_http_headers_feed(PreludeHttpHeaders *headers, const unsigned char *data,
                              size_t size, size_t *consumed);
bool prelude_http_payload(const unsigned char *response, size_t size, unsigned char **body, size_t *length);
#endif
