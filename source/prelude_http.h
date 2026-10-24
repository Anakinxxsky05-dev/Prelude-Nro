// Copyright (C) 2026 Nextendo Network. See LICENSE.md.
#ifndef PRELUDE_HTTP_H
#define PRELUDE_HTTP_H
#include <stdbool.h>
#include <stddef.h>
bool prelude_http_payload(const unsigned char *response, size_t size, unsigned char **body, size_t *length);
#endif
