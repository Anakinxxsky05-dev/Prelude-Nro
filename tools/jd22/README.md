# JD2022 deployed public trust

These are public certificates, ARM64 source and exact-build IPS metadata. No private key, player data or supplied game file is included.
The patches retain the original X509 verifier, add the deployed JD CA and retained Ubisoft frontend certificate, and accept only their
three exact SHA256 public-key pins. Unknown pins still call the original checker. Existing game unlock requirements stay intact.

Run `python tools/jd22/build-patches.py` with clang/llvm-objcopy installed to reproduce both ROMFS IPS files. The builder checks public
anchor hashes, branch ranges, padding boundaries and the reviewed IPS SHA256. It needs no game executable.

Builds: E63D7A75EE916F07C9AAC2985A9F5CFC3114456A (base), ED8426574258F6728BE883C22010730D5D6DB5310 (update v131072).
The base game was observed in live Switch–Citron testing against the deployed Nextendo backend; the update patch has offline ARM64 tests.
The earlier domestic trust implementation was tested in Citron and Ryujinx. The maintainer reports shared players and scores on the
deployed backend after installing these IPS and the five exact JD hosts. Separate-home/large-group and phone-controller tests remain open.

Prelude copies the patches through its existing ROMFS install process and lists them under Just Dance 2022. Existing lab users should
remove obsolete JD TLS patches from other exefs_patches folders before testing; unrelated game patches must be retained.
Certificates expire and future certificate rotation may require a patch update. This does not grant Unlimited content or entitlements.
