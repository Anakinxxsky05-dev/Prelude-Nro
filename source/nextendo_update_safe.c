// Copyright (C) 2026 Nextendo Network
// Licensed under the PolyForm Shield License 1.0.0. See LICENSE.md.
#include "nextendo_update_safe.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct { const char *begin, *end; } Json;
bool prelude_nro_path(const char *input, char *output, size_t capacity) {
    if (!capacity) return false;
    output[0] = 0;
    if (!input) return false;
    size_t n = strlen(input);
    if (n < 5 || input[n-4] != '.' || tolower((unsigned char)input[n-3]) != 'n' ||
        tolower((unsigned char)input[n-2]) != 'r' || tolower((unsigned char)input[n-1]) != 'o') return false;
    size_t prefix = 0;
    if (strncmp(input, "sdmc:/", 6)) {
        if (*input != '/') return false;
        prefix = 5;
    }
    if (n >= capacity || prefix >= capacity - n) return false;
    if (prefix) memcpy(output, "sdmc:", prefix);
    memcpy(output + prefix, input, n + 1);
    return true;
}
static const char *space(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t')) ++p;
    return p;
}
static const char *string_end(const char *p, const char *end) {
    if (p == end || *p++ != '"') return NULL;
    while (p < end) {
        unsigned char c = (unsigned char)*p++;
        if (c == '"') return p;
        if (c < 32) return NULL;
        if (c == '\\') {
            if (p == end) return NULL;
            c = (unsigned char)*p++;
            if (c == 'u') {
                for (int i = 0; i < 4; ++i)
                    if (p == end || !isxdigit((unsigned char)*p++)) return NULL;
            } else if (!strchr("\"\\/bfnrt", c)) return NULL;
        }
    }
    return NULL;
}
static const char *skip(const char *p, const char *end, unsigned depth) {
    p = space(p, end);
    if (p == end || depth > 32) return NULL;
    if (*p == '"') return string_end(p, end);
    if (*p == '{' || *p == '[') {
        bool object = *p++ == '{';
        char close = object ? '}' : ']';
        p = space(p, end);
        if (p < end && *p == close) return p + 1;
        for (;;) {
            if (object) {
                p = string_end(p, end);
                if (!p) return NULL;
                p = space(p, end);
                if (p == end || *p++ != ':') return NULL;
            }
            p = skip(p, end, depth + 1);
            if (!p) return NULL;
            p = space(p, end);
            if (p == end) return NULL;
            if (*p == close) return p + 1;
            if (*p++ != ',') return NULL;
            p = space(p, end);
        }
    }
    const char *literals[] = {"true", "false", "null"};
    for (unsigned i = 0; i < 3; ++i) {
        size_t n = strlen(literals[i]);
        if ((size_t)(end - p) >= n && !memcmp(p, literals[i], n)) return p + n;
    }
    if (*p == '-') ++p;
    if (p == end || !isdigit((unsigned char)*p)) return NULL;
    if (*p++ != '0') while (p < end && isdigit((unsigned char)*p)) ++p;
    if (p < end && *p == '.') {
        ++p;
        if (p == end || !isdigit((unsigned char)*p)) return NULL;
        while (p < end && isdigit((unsigned char)*p)) ++p;
    }
    if (p < end && (*p == 'e' || *p == 'E')) {
        ++p;
        if (p < end && (*p == '+' || *p == '-')) ++p;
        if (p == end || !isdigit((unsigned char)*p)) return NULL;
        while (p < end && isdigit((unsigned char)*p)) ++p;
    }
    return p;
}
static bool text(Json value, char *out, size_t capacity) {
    if (value.end - value.begin < 2 || *value.begin != '"') return false;
    size_t n = 0;
    for (const char *p = value.begin + 1; p < value.end - 1; ++p) {
        char c = *p;
        if (c == '\\') {
            c = *++p;
            // Selected release fields are ASCII. Reject unsupported escapes rather
            // than silently changing a filename, version or download URL.
            if (c != '/' && c != '\\' && c != '"') return false;
        }
        if (n + 1 >= capacity) return false;
        out[n++] = c;
    }
    out[n] = 0;
    return true;
}
static bool field(Json object, const char *name, Json *value) {
    const char *p = space(object.begin, object.end);
    if (p == object.end || *p++ != '{') return false;
    while ((p = space(p, object.end)) < object.end && *p != '}') {
        const char *key_end = string_end(p, object.end);
        if (!key_end) return false;
        bool match = (size_t)(key_end - p) == strlen(name) + 2 &&
                     !memcmp(p + 1, name, strlen(name));
        p = space(key_end, object.end);
        if (p == object.end || *p++ != ':') return false;
        p = space(p, object.end);
        const char *end = skip(p, object.end, 0);
        if (!end) return false;
        if (match) { *value = (Json){p, end}; return true; }
        p = space(end, object.end);
        if (p < object.end && *p == ',') ++p;
    }
    return false;
}
bool prelude_parse_release(const char *json, size_t length, PreludeRelease *release) {
    memset(release, 0, sizeof(*release));
    const char *end = json + length;
    const char *parsed = skip(json, end, 0);
    if (!parsed || space(parsed, end) != end) return false;
    Json root = {json, end}, tag, assets;
    char version[40];
    if (!field(root, "tag_name", &tag) || !text(tag, version, sizeof(version)) ||
        !field(root, "assets", &assets) || *assets.begin != '[') return false;
    const char *v = version;
    if (*v == 'v' || *v == 'V') ++v;
    int *parts[] = {&release->major, &release->minor, &release->patch};
    for (unsigned i = 0; i < 3; ++i) {
        if (!isdigit((unsigned char)*v)) goto bad;
        char *next;
        errno = 0;
        long n = strtol(v, &next, 10);
        if (errno || n > INT_MAX || (i < 2 ? *next != '.' : *next != 0)) goto bad;
        *parts[i] = (int)n;
        v = next + (i < 2);
    }
    for (const char *p = space(assets.begin + 1, assets.end); p < assets.end && *p != ']';) {
        const char *asset_end = skip(p, assets.end, 0);
        Json asset = {p, asset_end}, name, url, size;
        char filename[64];
        if (!asset_end) goto bad;
        if (field(asset, "name", &name) && text(name, filename, sizeof(filename)) &&
            !strcmp(filename, "nextendo.nro")) {
            static const char prefix[] = "https://github.com/NextendoNetwork/Prelude-Nextendo/releases/download/";
            if (!field(asset, "browser_download_url", &url) ||
                !text(url, release->url, sizeof(release->url)) ||
                strncmp(release->url, prefix, sizeof(prefix) - 1) ||
                !field(asset, "size", &size)) goto bad;
            long n = 0;
            for (const char *s = size.begin; s < size.end; ++s) {
                if (!isdigit((unsigned char)*s) || n > (LONG_MAX - (*s - '0')) / 10) goto bad;
                n = n * 10 + (*s - '0');
            }
            if (n < 4096 || n > 128L * 1024 * 1024) goto bad;
            release->size = n;
            return true;
        }
        p = space(asset_end, assets.end);
        if (p < assets.end && *p == ',') p = space(p + 1, assets.end);
    }
bad:
    memset(release, 0, sizeof(*release));
    return false;
}
static uint64_t little(const unsigned char *p, unsigned count) {
    uint64_t n = 0;
    for (unsigned i = 0; i < count; ++i) n |= (uint64_t)p[i] << (8 * i);
    return n;
}
bool prelude_validate_nro(const char *path, long expected_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    unsigned char header[0x38], asset[0x38];
    bool ok = false;
    if (fseek(f, 0, SEEK_END)) goto done;
    long length = ftell(f);
    if (length < 4096 || (expected_size > 0 && length != expected_size) || fseek(f, 0, SEEK_SET)) goto done;
    if (fread(header, 1, sizeof(header), f) != sizeof(header) || memcmp(header + 0x10, "NRO0", 4)) goto done;
    uint64_t nro_size = little(header + 0x18, 4);
    if (nro_size < 0x80 || nro_size > (uint64_t)length - sizeof(asset) || fseek(f, (long)nro_size, SEEK_SET)) goto done;
    if (fread(asset, 1, sizeof(asset), f) != sizeof(asset) || memcmp(asset, "ASET", 4)) goto done;
    for (unsigned i = 0; i < 3; ++i) {
        uint64_t offset = little(asset + 8 + i * 16, 8), size = little(asset + 16 + i * 16, 8);
        uint64_t available = (uint64_t)length - nro_size;
        if (offset < sizeof(asset) || offset > available || size > available - offset || size == 0) goto done;
    }
    // Re-read the entire stored file: a successful network write count does not
    // prove that later SD reads succeed, including its RomFS payload.
    if (fseek(f, 0, SEEK_SET)) goto done;
    unsigned char buffer[8192];
    long count = 0;
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), f))) count += (long)n;
    ok = !ferror(f) && count == length;
done:
    if (fclose(f)) ok = false;
    return ok;
}
PreludeInstallResult prelude_install_nro(const char *staged, const char *target, const char *backup, PreludeCommit commit) {
    struct stat st;
    if (!prelude_validate_nro(staged, 0)) return PRELUDE_INSTALL_FAILED;
    // Do not overwrite a recovery copy left by an interrupted update. A valid
    // target must exist before it is safe to retire that old backup.
    if (stat(backup, &st) == 0) {
        if (!prelude_validate_nro(target, 0) || remove(backup) || !commit()) return PRELUDE_INSTALL_FAILED;
    } else if (errno != ENOENT) return PRELUDE_INSTALL_FAILED;
    if (rename(target, backup)) return PRELUDE_INSTALL_FAILED;
    if (!commit() || rename(staged, target)) {
        return rename(backup, target) == 0 && commit() ? PRELUDE_INSTALL_FAILED : PRELUDE_INSTALL_RECOVERY;
    }
    if (!commit() || !prelude_validate_nro(target, 0)) {
        // Move the candidate aside before restoring the old NRO. Never delete
        // either complete copy when a filesystem operation fails.
        return rename(target, staged) == 0 && rename(backup, target) == 0 && commit()
            ? PRELUDE_INSTALL_FAILED : PRELUDE_INSTALL_RECOVERY;
    }
    // Retain the backup until the next fully validated update. It is also a
    // recoverable copy if power is lost immediately after this function returns.
    return PRELUDE_INSTALL_OK;
}
