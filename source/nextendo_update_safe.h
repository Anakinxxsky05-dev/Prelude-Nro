// Copyright (C) 2026 Nextendo Network
// Licensed under the PolyForm Shield License 1.0.0. See LICENSE.md.
#ifndef NEXTENDO_UPDATE_SAFE_H
#define NEXTENDO_UPDATE_SAFE_H
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    int major, minor, patch;
    long size;
    char url[512];
} PreludeRelease;

bool prelude_parse_release(const char *json, size_t length, PreludeRelease *release);
bool prelude_validate_nro(const char *path, long expected_size);
typedef bool (*PreludeCommit)(void);
// All three paths must be siblings. Never copies over or deletes the installed NRO.
// A failed rollback retains the backup and staged download for manual recovery.
typedef enum { PRELUDE_INSTALL_OK, PRELUDE_INSTALL_FAILED, PRELUDE_INSTALL_RECOVERY } PreludeInstallResult;
PreludeInstallResult prelude_install_nro(const char *staged, const char *target, const char *backup,
                                        PreludeCommit commit);
#endif
