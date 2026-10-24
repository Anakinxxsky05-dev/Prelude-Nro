# Public-release download regression

The Switch download-only test reproduced two failures on `github.com` at
`HTTP_headers`, with HTTP status 0, SSL result 0 and no installed-NRO changes.
An equivalent HTTP/1.1 request returned a 302 redirect with **10,789 bytes** of
headers, including a 9,242-byte Content-Security-Policy line. The downloader's
8,192-byte header buffer filled before the header terminator. It then discarded
subsequent reads until EOF and reported a generic network error.

The fix accumulates headers on the heap, growing from 4 KiB up to a bounded
64 KiB. It preserves packet/body boundaries, frees the allocation on every exit,
and returns an explicit protocol or allocation error at the limit. Redirects are
followed after their headers without waiting for the redirect body.

`tools/test_download_stream.py` extracts the actual production downloader and
replays a large GitHub-style redirect through mocked TLS reads, followed by a
known CDN body. It checks both redirect handling and the exact stored body.
It also tests the explicit header-limit failure. An optional private HTTP capture
can be supplied as the second argument; captured signed URLs are not committed.
`--baseline` replays the 4.0.9 code and confirms the original HTTP=0/SSL=0 failure.

The 4.0.8 source at `9a6722` and the 4.0.9 source at `1cbd8a0` have identical
startup, UI, audio, apply-mode and networking implementations. The release delta
is the version/build bump and the GBA 3.4.0 IPS. The published NRO comparison has
identical segment sizes and data; the instruction differences are version/build
constants, with changed build IDs. Therefore reverting to 4.0.8 does not remove
the header limit. Historical startup failures are not established by this
download finding; the received 4.0.10/4.0.11 device traces show successful RomFS
mounting and UI initialization.
