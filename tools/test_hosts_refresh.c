// Test of the hosts refresh decisions (source/nextendo_hosts_refresh.h). No libnx needed:
//   clang -std=gnu11 -Wall -Wextra -I source -o test_hosts_refresh tools/test_hosts_refresh.c && ./test_hosts_refresh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nextendo_hosts_refresh.h"

#define MARK "NEXTENDO NETWORK - Atmosphere DNS-MITM"
#define DEFAULT_IP "51.178.29.194"
#define ALT_IP "3.135.232.168"

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else printf("ok:   %s\n", msg); } while (0)

int main(void) {
    const char *current =
        "# ============================================================\n"
        "#  NEXTENDO NETWORK - Atmosphere DNS-MITM (mode NEXTENDO)\n"
        "# ============================================================\n"
        "51.178.29.194    *.nintendo.com\n";
    const char *newer = "# header\n#  NEXTENDO NETWORK - Atmosphere DNS-MITM (mode NEXTENDO)\n51.178.29.194    *.nintendo.com\n"
                        "51.178.29.194 api.braincloudservers.com\n";
    const char *user = "127.0.0.1 localhost\n0.0.0.0 ads.example.test\n";

    CHECK(nextendo_hosts_refresh_action(current, strlen(current), current, MARK) == HOSTS_KEEP_CURRENT,
          "identical content: nothing to do");
    CHECK(nextendo_hosts_refresh_action(current, strlen(current), newer, MARK) == HOSTS_REWRITE,
          "ours but older than this version: rewrite");
    CHECK(nextendo_hosts_refresh_action(user, strlen(user), newer, MARK) == HOSTS_KEEP_NOT_OURS,
          "a user's own hosts file is never touched");
    CHECK(nextendo_hosts_refresh_action(NULL, 0, newer, MARK) == HOSTS_KEEP_NOT_OURS,
          "a missing file is not created");
    CHECK(nextendo_hosts_refresh_action("", 0, newer, MARK) == HOSTS_KEEP_NOT_OURS, "an empty file is untouched");

    char *deep = (char *)malloc(4000);
    memset(deep, '#', 1500);
    deep[1500] = '\n';
    strcpy(deep + 1501, current);
    CHECK(nextendo_hosts_refresh_action(deep, strlen(deep), newer, MARK) == HOSTS_KEEP_NOT_OURS,
          "our text far below the header (a user file quoting it) is not ours");
    free(deep);

    const char *alt[3] = { "3.135.232.168 *.nintendo.com\n", NULL, NULL };
    const char *def[3] = { current, current, NULL };
    const char *none[3] = { NULL, NULL, NULL };
    CHECK(strcmp(nextendo_hosts_pick_ip(alt, 3, DEFAULT_IP, ALT_IP), ALT_IP) == 0, "alternative IP is kept");
    CHECK(strcmp(nextendo_hosts_pick_ip(def, 3, DEFAULT_IP, ALT_IP), DEFAULT_IP) == 0, "default IP is kept");
    CHECK(strcmp(nextendo_hosts_pick_ip(none, 3, DEFAULT_IP, ALT_IP), DEFAULT_IP) == 0, "no files: default IP");

    printf(fails ? "FAILED: %d\n" : "all ok\n", fails);
    return fails ? 1 : 0;
}
