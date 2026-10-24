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
    // GitHub's current release redirect exceeds the former 8192-byte buffer.
    unsigned char large[12000];
    const char *prefix = "HTTP/1.1 302 Found\r\nLocation: https://example.test/asset\r\nContent-Security-Policy: ";
    size_t prefix_size = strlen(prefix);
    memcpy(large, prefix, prefix_size);
    memset(large + prefix_size, 'x', 10789 - prefix_size - 4);
    memcpy(large + 10789 - 4, "\r\n\r\nBODY", 8);
    const size_t fragments[] = {1, 7, 4096, 12000};
    for (unsigned i = 0; i < sizeof(fragments)/sizeof(fragments[0]); ++i) {
        PreludeHttpHeaders headers = {0};
        size_t offset = 0;
        while (!headers.complete) {
            size_t n = sizeof(large) - offset;
            if (n > fragments[i]) n = fragments[i];
            size_t used = 0;
            assert(prelude_http_headers_feed(&headers, large + offset, n, &used) >= 0);
            assert(used > 0); offset += used;
        }
        assert(offset == 10789 && headers.length == 10789);
        assert(!memcmp(headers.data, large, 10789));
        free(headers.data);
    }
    PreludeHttpHeaders too_large = {0};
    unsigned char block[4096]; memset(block, 'x', sizeof(block));
    for (int i = 0; i < 16; ++i) {
        size_t used; assert(prelude_http_headers_feed(&too_large, block, sizeof(block), &used) == 0);
    }
    size_t used; assert(prelude_http_headers_feed(&too_large, block, 1, &used) == -1);
    assert(too_large.capacity == PRELUDE_HTTP_HEADER_LIMIT);
    free(too_large.data);
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
