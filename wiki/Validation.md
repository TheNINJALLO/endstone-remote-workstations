# Release validation

Release 0.4.0 promotes the final 0.4.0a24 implementation. The Python version
constant and package metadata/documentation change; gameplay modules, configuration,
catalog and the native companion are checked for exact payload equivalence.
The release's `validation.json` and `SHA256SUMS.txt` identify the final artifacts.

## Final alpha evidence

| Qualification | Result |
| --- | --- |
| Automated suite | 565 passed |
| Implemented catalog lifecycle | 53/53 passed |
| Console matrix | 95/95 passed |
| Portable headless smoke | 17 commands passed |
| Mounted-merchant cases | 8 passed; 10 accepted native item results |
| Separate dependency consumer | 7 checks passed; 5 protected icon transfers refused |
| Installed package identity | All 35 provider files and the separate consumer matched their wheels |

The 53 lifecycle cases used one Windows Bedrock 1.26.45 Classic keyboard/mouse
client: 47 entries in the standard test world and six in a separate Education
clone. Four embedded player-inventory views released tracking before normal
manual close. Six reader/editor views have explicit reviewed HUD-close evidence
because they do not emit a client ContainerClose on API cancellation.

The mounted-merchant checks covered an actual Survival trade, Creative open,
cursor return, protection/configuration revocation and real dismount changes.
The dependency checks covered protected inventory pages, callbacks, cross-plugin
exports/private-action denial, native anvil and actual Ender Chest navigation,
and complete inventory/session cleanup.

The public screenshots come from that a24 run. Earlier recipe, chemistry, NPC,
Agent and editor transaction evidence remains attributed to its earlier tested
wheel. A successful lifecycle check does not qualify every recipe or a crash.

## Final release checks

The final source passes the automated suite. Packaging checks compare the
portable/native payloads, verify the Windows companion against the tested binary,
check archive boundaries and wheel RECORD hashes, and repeat wheel creation for
byte identity. Installed startup, console and portable smoke results are attached
to the final release validation record with their actual artifact hashes.

CI runs the public unit/contract suite and portable build on Windows and Linux.
CI does not launch a Bedrock client or establish a Linux native implementation.

Raw captures, worlds, account/item snapshots and server executables are private
test material and are excluded. Only the selected, reviewed documentation images
are published. See [support scope](https://github.com/TheNINJALLO/endstone-remote-workstations/wiki/Compatibility-and-Limits) and the historical
[a24 report](https://github.com/TheNINJALLO/endstone-remote-workstations/blob/main/docs/CATALOG_TEST_REPORT.md).
