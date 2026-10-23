# GBA 3.4.0 patch

Title 010012F017576000; original main build C4744859A0EA4C05A67629274CD930ABC1B97DA2 (zero-padded in the filename). Original update NACP confirms 3.4.0. NSO section hashes verified. The three exact original instructions and unique 16-byte preceding contexts match the 3.3.0 equivalents at a +0x1AC0 displacement. A simulated application changed only the three declared four-byte sites. No executable image, key or account data is included.

Run python tools/verify-gba-340-patch.py. This is a static compatibility check; real online room/join/gameplay testing remains pending. The existing automatic folder installer includes the new file. Keep both 3.3.0 definitions. Maintainer release/version bump follows runtime acceptance; this PR does not publish a build.
