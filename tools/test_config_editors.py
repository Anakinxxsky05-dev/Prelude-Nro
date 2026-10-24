"""Exercise the actual production INI editors with unrelated user settings."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "source/nextendo_apply.c").read_text(encoding="utf-8")

def function(name):
    start = source.index("static bool " + name + "(")
    brace = source.index("{", start)
    depth, pos = 1, brace + 1
    while depth:
        depth += (source[pos] == "{") - (source[pos] == "}")
        pos += 1
    return source[start:pos]

prefix = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "prelude_io.h"
#define NEXTENDO_SETTINGS_INI "settings.ini"
#define NEXTENDO_EXOSPHERE_INI "exosphere.ini"
#define SETTINGS_DIR "."
static void ensureDir(const char *p) { (void)p; }
static void nextendo_trace(const char *p) { (void)p; }
static bool commit(void) { return true; }
static bool writeTextFile(const char *p, const char *s) { return prelude_write_config(p,s,commit); }
'''
main = r'''
int main(void) {
    const char *settings="; user comment\n[atmosphere]\nuser_setting = u8!0x9\n[other]\ncustom = retained\n";
    assert(writeTextFile(NEXTENDO_SETTINGS_INI,settings));
    assert(writeTextFile(NEXTENDO_EXOSPHERE_INI,"[exosphere]\nblank_prodinfo_sysmmc=0\nuser_setting=retained\n"));
    assert(iniSetDnsMitm(true,false));
    assert(iniSetBlankProdinfoEmummc(true));
    assert(iniSetNetworkMitm(true));
    char *text;size_t size;
    assert(prelude_read_config(NEXTENDO_SETTINGS_INI,&text,&size));
    assert(strstr(text,"user_setting = u8!0x9"));assert(strstr(text,"custom = retained"));
    assert(strstr(text,"; user comment"));assert(strstr(text,"enable_dns_mitm = u8!0x1"));
    assert(strstr(text,"add_defaults_to_dns_hosts = u8!0x0"));free(text);
    assert(iniSetNetworkMitm(false));assert(iniSetDnsMitm(true,true));
    assert(prelude_read_config(NEXTENDO_SETTINGS_INI,&text,&size));
    assert(strstr(text,"custom = retained"));assert(strstr(text,"enable_device_cert_fallback = u8!0x0"));free(text);
    assert(prelude_read_config(NEXTENDO_EXOSPHERE_INI,&text,&size));
    assert(strstr(text,"blank_prodinfo_sysmmc=0"));assert(strstr(text,"user_setting=retained"));
    assert(strstr(text,"blank_prodinfo_emummc=1"));free(text);
    puts("PASS: actual mode/account-link INI editors preserve comments, other sections and sysMMC settings");
    return 0;
}
'''
# The raw C strings above contain escaped line separators for the fixtures.
main = main.replace("\\\\n", "\\n")
with tempfile.TemporaryDirectory(prefix="prelude-editors-") as directory:
    folder = Path(directory)
    test = folder / "test.c"
    test.write_text(prefix + "\n".join(function(n) for n in (
        "iniSetDnsMitm", "iniSetBlankProdinfoEmummc", "iniSetNetworkMitm")) + main, encoding="utf-8")
    binary = folder / ("test.exe" if sys.platform == "win32" else "test")
    subprocess.run([sys.argv[1] if len(sys.argv) > 1 else "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I", str(root / "source"), str(test), str(root / "source/prelude_io.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], cwd=folder, check=True)
