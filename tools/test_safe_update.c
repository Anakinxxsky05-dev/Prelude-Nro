// Host-side failure injection for the production updater core. No Switch SDK needed.
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int rename_calls, commit_calls, fail_rename, fail_rename_also, fail_commit;
static int fail_read, fail_close;
static int injected_rename(const char *from, const char *to) {
    ++rename_calls;
    if (rename_calls == fail_rename || rename_calls == fail_rename_also) { errno = EIO; return -1; }
    return rename(from, to);
}
static size_t injected_read(void *p, size_t size, size_t count, FILE *f) {
    if (fail_read) { errno = EIO; return 0; }
    return fread(p, size, count, f);
}
static int injected_close(FILE *f) {
    int result = fclose(f);
    return fail_close ? EOF : result;
}
#define rename injected_rename
#define fread injected_read
#define fclose injected_close
#include "../source/nextendo_update_safe.c"
#undef rename
#undef fread
#undef fclose

static const char *target = "safe-update-target.tmp", *staged = "safe-update-new.tmp", *backup = "safe-update-backup.tmp";
static bool commit(void) { return ++commit_calls != fail_commit; }
static void put_le(unsigned char *p, unsigned long value, unsigned n) {
    for (unsigned i = 0; i < n; ++i) { p[i] = value & 255; value >>= 8; }
}
static void fixture(const char *path, unsigned char marker) {
    unsigned char data[32768] = {0};
    memcpy(data + 0x10, "NRO0", 4); put_le(data + 0x18, 128, 4);
    memcpy(data + 128, "ASET", 4);
    for (unsigned i = 0; i < 3; ++i) {
        put_le(data + 128 + 8 + i * 16, 56 + i * 8, 8);
        put_le(data + 128 + 16 + i * 16, 8, 8);
    }
    data[sizeof(data) - 1] = marker;
    FILE *f = fopen(path, "wb"); assert(f);
    assert(fwrite(data, 1, sizeof(data), f) == sizeof(data)); assert(fclose(f) == 0);
}
static unsigned char marker(const char *path) {
    FILE *f = fopen(path, "rb"); assert(f); assert(fseek(f, -1, SEEK_END) == 0);
    int value = fgetc(f); assert(fclose(f) == 0); return (unsigned char)value;
}
static void reset(void) {
    remove(target); remove(staged); remove(backup);
    rename_calls = commit_calls = fail_rename = fail_rename_also = fail_commit = fail_read = fail_close = 0;
    fixture(target, 'O'); fixture(staged, 'N');
}
static void storage_tests(void) {
    reset(); assert((prelude_install_nro(staged, target, backup, commit) == PRELUDE_INSTALL_OK));
    assert(marker(target) == 'N' && marker(backup) == 'O');
    fixture(staged, 'X'); assert((prelude_install_nro(staged, target, backup, commit) == PRELUDE_INSTALL_OK));
    assert(marker(target) == 'X' && marker(backup) == 'N');
    reset(); fail_read = 1; assert(!(prelude_install_nro(staged, target, backup, commit) == PRELUDE_INSTALL_OK));
    assert(marker(target) == 'O' && rename_calls == 0);
    reset(); fail_close = 1; assert(!(prelude_install_nro(staged, target, backup, commit) == PRELUDE_INSTALL_OK));
    assert(marker(target) == 'O' && rename_calls == 0);
    reset(); fail_rename = 1; assert(!(prelude_install_nro(staged, target, backup, commit) == PRELUDE_INSTALL_OK));
    assert(marker(target) == 'O' && marker(staged) == 'N');
    reset(); fail_rename = 2; assert(!(prelude_install_nro(staged, target, backup, commit) == PRELUDE_INSTALL_OK));
    assert(marker(target) == 'O' && marker(staged) == 'N');
    reset(); fail_commit = 1; assert(!(prelude_install_nro(staged, target, backup, commit) == PRELUDE_INSTALL_OK));
    assert(marker(target) == 'O' && marker(staged) == 'N');
    reset(); fail_commit = 2; assert(!(prelude_install_nro(staged, target, backup, commit) == PRELUDE_INSTALL_OK));
    assert(marker(target) == 'O' && marker(staged) == 'N');
    reset(); fail_rename = 2; fail_rename_also = 3;
    assert(!(prelude_install_nro(staged, target, backup, commit) == PRELUDE_INSTALL_OK));
    assert(marker(backup) == 'O' && marker(staged) == 'N');
    // A second attempt must not destroy the only recovery copy.
    fail_rename = fail_rename_also = 0;
    assert(!(prelude_install_nro(staged, target, backup, commit) == PRELUDE_INSTALL_OK)); assert(marker(backup) == 'O');
    reset(); FILE *f = fopen(staged, "wb"); assert(f); fputs("truncated", f); fclose(f);
    assert(!(prelude_install_nro(staged, target, backup, commit) == PRELUDE_INSTALL_OK)); assert(marker(target) == 'O');
    reset(); assert(!prelude_validate_nro(staged, 42));
    remove(target); remove(staged); remove(backup);
}
static void parser_tests(void) {
    const char *good = "{\"body\":\"\\\"name\\\":\\\"other\\\"\",\"tag_name\":\"v4.0.10\",\"assets\":["
        "{\"name\":\"sources.zip\",\"size\":999999,\"browser_download_url\":\"https://example.com/file\"},"
        "{\"size\" : 25055144,\"name\":\"nextendo.nro\",\"browser_download_url\":"
        "\"https://github.com/NextendoNetwork/Prelude-Nextendo/releases/download/v4.0.10/nextendo.nro\"}]}";
    PreludeRelease release;
    assert(prelude_parse_release(good, strlen(good), &release));
    assert(release.major == 4 && release.minor == 0 && release.patch == 10 && release.size == 25055144);
    assert(strstr(release.url, "/v4.0.10/nextendo.nro"));
    assert(!prelude_parse_release(good, strlen(good) - 1, &release)); assert(!release.url[0]);
    const char *bad[] = {"{}", "{\"tag_name\":\"v4.0.10oops\",\"assets\":[]}",
        "{\"tag_name\":\"v4.0.10\",\"assets\":[{\"name\":\"nextendo.nro\",\"size\":9000}]}",
        "{\"tag_name\":\"v4.0.10\",\"assets\":[]} junk", "{\"tag_name\":\"v4.0.10\",\"assets\":[,]}"};
    for (unsigned i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i) {
        assert(!prelude_parse_release(bad[i], strlen(bad[i]), &release)); assert(!release.url[0]);
    }
}
int main(int argc, char **argv) {
    parser_tests(); storage_tests();
    if (argc == 3) {
        FILE *f = fopen(argv[1], "rb"); assert(f); fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
        char *json = malloc((size_t)size); assert(json); assert(fread(json, 1, (size_t)size, f) == (size_t)size); fclose(f);
        PreludeRelease release; assert(prelude_parse_release(json, (size_t)size, &release)); free(json);
        assert(prelude_validate_nro(argv[2], release.size));
    }
    puts("PASS: release selection, stored NRO validation, read/close/rename/commit failures and rollback recovery");
    return 0;
}
