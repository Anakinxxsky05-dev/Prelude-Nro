# Mode-switch regression: 4.0.12 candidate

## Reproduced failure in 4.0.11

`prelude_write_config` retains a `.prelude-backup` sibling. Nintendo mode removes
the generated hosts, leaving that sibling. On the next Nextendo activation the
writer rejects `backup exists && target missing`, producing the generic SD error.
The added sequence in `test_config_io.c` fails on 4.0.11 and passes with this fix.
Keep the recovery sibling when creating an absent target; do not erase it first.

## Hosts isolation

The previous apply path overwrote sysMMC, emuMMC and shared default hosts. Its
mode indicator looked at both NANDs. The candidate modifies the current NAND's
explicit hosts only, honors an existing `emummc_<id>.txt`, reads that environment
for its indicator and refuses changes when boot detection fails.

Nintendo mode writes an explicit normal-DNS file (or the validated user backup)
instead of deleting it and falling back to a legacy Nextendo default. Shared
`default.txt` and the other NAND's hosts remain unchanged. Applying from sysMMC
does not edit the emuMMC PRODINFO setting. Firmware resources and Atmosphere's
system settings remain shared on the SD; this change does not create per-NAND
copies of system modules or promise any protection from Nintendo account bans.

Nintendo routing is enabled only after resource removal succeeds. Account-link
removal now checks its files, settings and commit; failures propagate to the UI.
Legacy resource removal is also verified instead of silently ignoring errors.

`test_mode_isolation.py` compiles the actual production functions with an in-memory
SD and stubs for firmware resources. It covers repeated switching, both NANDs,
shared fallback, emuMMC-ID precedence, restoring a custom per-ID backup, unknown
boot and leaving emuMMC PRODINFO untouched from sysMMC. It is not a physical
Switch test and does not simulate stock firmware.

## Still requires hardware verification

Do not call this a verified public fix until Nintendo -> Nextendo -> Nintendo
has been tested after reboot on the affected hardware. Check the actually selected
DNS hosts and that certificate/resource cleanup completed. Keep SD backups.

The separately reported Horizon account-link 2306-0807 (5006) is not established
as the same defect. A failed direct BCAT HTTP probe is not evidence of a TLS
failure on the account endpoint. Do not enable the HOS>=23 fallback module as a
workaround: the existing known-crash guard remains intact. New SD trace entries
include path/errno and filesystem commit result, not credentials or file contents.


## 4.0.13 policy correction

The user clarified that Nintendo mode means disconnecting Nextendo while keeping
Nintendo DNS blocked. Unlike 4.0.12, normal mode switches append loopback rules
for Nintendo services after restored user entries, regardless of the PRODINFO
choice. The current NAND only is modified. DNS interception remains enabled.
The explicitly warned, hidden maintainer capture mode retains its separate DNS
policy; it is not selected by either normal Nintendo/Nextendo mode action.

`test_nintendo_dns_block.py` covers all 35 original 90DNS Tester domains plus
account/telemetry endpoints with the real production generated hosts and
Atmosphere's documented wildcard precedence. Physical green/red results remain
to be checked after reboot; on firmware 23.x this also requires the upstream
Atmosphere 1.12.1 resolver fix. DNS blocking is not a ban-free guarantee.
