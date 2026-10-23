# GBA 3.4.0 patch

Title 010012F017576000; original main build C4744859A0EA4C05A67629274CD930ABC1B97DA2 (zero-padded in the filename). Original update NACP confirms 3.4.0. NSO section hashes verified. The three exact original instructions and unique 16-byte preceding contexts match the 3.3.0 equivalents at a +0x1AC0 displacement. A simulated application changed only the three declared four-byte sites. No executable image, key or account data is included.

Run python tools/verify-gba-340-patch.py. Switch online entry with GBA 3.4.0 was confirmed after installing this exact patch. Room creation, joining and gameplay with the new version still require broader testing. The existing automatic folder installer includes the new file. Keep both 3.3.0 definitions.

Release version: 4.0.9, internal build 94. Makefile APP_VERSION and the nextendo_update.h semver fallback agree, allowing the automatic updater to distinguish this build from 4.0.8. The maintainer can merge, build and publish the matching v4.0.9 release; this PR does not create a tag or publish a release.
