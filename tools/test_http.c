#include "../source/prelude_http.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(const char *wire, const char *expected) {
    unsigned char *body = NULL; size_t size = 0;
    bool ok = prelude_http_payload((const unsigned char *)wire, strlen(wire), &body, &size);
    if (expected) { assert(ok); assert(size == strlen(expected)); assert(!memcmp(body, expected, size)); }
    else { assert(!ok); assert(body == NULL && size == 0); }
    free(body);
}
int main(void) {
    check("HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\ntest", "test");
    check("HTTP/1.1 200 OK\r\n\r\ntest", "test");
    check("HTTP/1.1 200 OK\r\nTransfer-Encoding: Chunked\r\n\r\n2\r\nte\r\n2;x=1\r\nst\r\n0\r\nX-Trace: discarded\r\n\r\n", "test");
    check("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n", "");
    check("HTTP/1.1 200 OK\r\nContent-Length: 9\r\n\r\nshort", NULL);
    check("HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\ntoolong", NULL);
    check("HTTP/1.1 200 OK\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\nx", NULL);
    check("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Length: 0\r\n\r\n0\r\n\r\n", NULL);
    check("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nxx", NULL);
    check("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1\r\nx\r\n", NULL);
    check("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nFFFFFFFFFFFFFFFFFFFFFFFF\r\nx\r\n0\r\n\r\n", NULL);
    check("HTTP/1.1 200 OK\r\nContent-Length: -1\r\n\r\nx", NULL);
    check("HTTP/1.1 200 OK\r\nTransfer-Encoding: gzip\r\n\r\nx", NULL);
    const char *sample="HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\n{}\r\n0\r\n\r\n";
    for (size_t n = 0; n < strlen(sample); ++n) {
        unsigned char *body = NULL; size_t size = 0;
        assert(!prelude_http_payload((const unsigned char *)sample, n, &body, &size)); free(body);
    }
    puts("PASS: content length, chunk decoding, trailers, truncation and malformed responses");
}
