// Copyright (C) 2026 Nextendo Network. See LICENSE.md.
#ifndef PRELUDE_IO_H
#define PRELUDE_IO_H
#include <stdbool.h>
#include <stddef.h>
// A missing file is allowed; unreadable/truncated/oversized files are errors.
bool prelude_read_config(const char *path, char **contents, size_t *length);
bool prelude_write_config(const char *path, const char *contents, bool (*commit)(void));
#endif
