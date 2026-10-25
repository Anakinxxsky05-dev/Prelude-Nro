"""Compile the production hosts builder and verify the GGST entries and refresh.

No Switch, real DNS requests or libnx are needed.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "source/nextendo_apply.c").read_text(encoding="utf-8")
start = source.index("char *nextendo_hosts_build(")
end = source.index("\nstatic bool commitConfig", start)
builder = source[start:end]
prefix = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nextendo_hosts_refresh.h"
#define NEXTENDO_SERVER_IP_NNCSD2 "192.0.2.20"
'''
checks = r'''
static void check_hosts(const char *ip) {
    const char *hosts[] = {
        "api.epicgames.dev",
        "switch-ggst-game.guiltygear.com",
        "prod-switch-match-ggst-game.guiltygear.com",
        "prod-to3match-ggst-game.guiltygear.com"
    };
    char *text = nextendo_hosts_build(ip);
    assert(text);
    for (size_t i = 0; i < sizeof(hosts) / sizeof(hosts[0]); i++) {
        char expected[256];
        snprintf(expected, sizeof(expected), "%s %s\n", ip, hosts[i]);
        const char *match = strstr(text, expected);
        assert(match && strstr(match + strlen(expected), expected) == NULL);
    }
    assert(strstr(text, "*.guiltygear.com") == NULL);
    assert(strstr(text, "*.epicgames.dev") == NULL);
    assert(strstr(text, "backend.innersloth.com\n") != NULL);
    assert(strstr(text, "wwz.wwzapi.com\n") != NULL);
    assert(strstr(text, "192.0.2.20  nncs2-*.n.n.srv.nintendo.net\n") != NULL);
    const char *old = "# NEXTENDO NETWORK - Atmosphere DNS-MITM\n192.0.2.1 api.epicgames.dev\n";
    const char *custom = "192.0.2.30 custom.example\n";
    const char *mark = "NEXTENDO NETWORK - Atmosphere DNS-MITM";
    assert(nextendo_hosts_refresh_action(old, strlen(old), text, mark) == HOSTS_REWRITE);
    assert(nextendo_hosts_refresh_action(text, strlen(text), text, mark) == HOSTS_KEEP_CURRENT);
    assert(nextendo_hosts_refresh_action(custom, strlen(custom), text, mark) == HOSTS_KEEP_NOT_OURS);
    free(text);
}
int main(void) {
    check_hosts("192.0.2.1");
    check_hosts("198.51.100.2");
    puts("PASS: four GGST hosts, shared EOS deduplication, server selection, existing rules and managed refresh");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="prelude-ggst-") as directory:
    folder = Path(directory)
    test = folder / "test.c"
    binary = folder / ("test.exe" if sys.platform == "win32" else "test")
    test.write_text(prefix + builder + checks, encoding="utf-8")
    subprocess.run([sys.argv[1] if len(sys.argv) > 1 else "cc", "-std=c11",
                    "-Wall", "-Wextra", "-Werror", "-I", str(root / "source"),
                    str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
