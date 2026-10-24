// Copyright (C) 2026 Nextendo Network. See LICENSE.md.
#include "prelude_io.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

bool prelude_read_config(const char *path, char **contents, size_t *length) {
    *contents = NULL;
    *length = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return errno == ENOENT;
    bool ok = false;
    char *text = NULL;
    if (fseek(f, 0, SEEK_END)) goto done;
    long size = ftell(f);
    if (size < 0 || size > 1024 * 1024 || fseek(f, 0, SEEK_SET)) goto done;
    text = malloc((size_t)size + 1);
    if (!text) goto done;
    if (fread(text, 1, (size_t)size, f) != (size_t)size || ferror(f)) goto done;
    // INI files are text: embedded NULs must not silently discard trailing keys.
    if (memchr(text, 0, (size_t)size)) goto done;
    text[size] = 0;
    *length = (size_t)size;
    ok = true;
done:
    if (fclose(f)) ok = false;
    if (ok) *contents = text;
    else { free(text); *length = 0; }
    return ok;
}

bool prelude_write_config(const char *path, const char *contents, bool (*commit)(void)) {
    char staged[1024], backup[1024];
    if (snprintf(staged, sizeof(staged), "%s.prelude-new", path) >= (int)sizeof(staged) ||
        snprintf(backup, sizeof(backup), "%s.prelude-backup", path) >= (int)sizeof(backup)) return false;
    char *old = NULL;
    size_t old_size = 0;
    if (!prelude_read_config(path, &old, &old_size)) return false;
    bool had_old = old != NULL;
    free(old);
    struct stat st;
    if (stat(backup, &st) == 0) {
        if (!had_old || remove(backup) || !commit()) return false;
    } else if (errno != ENOENT) return false;
    FILE *f = fopen(staged, "wb");
    if (!f) return false;
    size_t size = strlen(contents);
    bool ok = fwrite(contents, 1, size, f) == size && !ferror(f);
    if (fclose(f)) ok = false;
    if (!ok || !commit()) { remove(staged); return false; }
    char *stored = NULL;
    size_t stored_size = 0;
    ok = prelude_read_config(staged, &stored, &stored_size) && stored &&
         stored_size == size && !memcmp(stored, contents, size);
    free(stored);
    if (!ok) { remove(staged); return false; }
    if (had_old && rename(path, backup)) return false;
    if (!commit() || rename(staged, path)) {
        if (had_old) rename(backup, path);
        commit();
        return false;
    }
    if (!commit()) {
        if (rename(path, staged) == 0 && had_old) rename(backup, path);
        commit();
        return false;
    }
    // Keep the previous version for recovery, outside Atmosphere's *.txt files.
    return true;
}
