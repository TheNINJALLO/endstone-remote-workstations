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

The final source passes **565 tests** locally and in the public-tree validation.
Windows and Ubuntu CI also passed the full suite, documentation checks and the
portable build. Installed 0.4.0 passed **95 console checks**; the portable wheel
passed **17 headless commands** and clean shutdown. All **35 installed native
package files** match the final wheel. Native startup reports the admitted bridge
and zero active sessions. The companion refuses import outside its host.

Both wheels were created twice with identical bytes. The rebuilt native binary
and 34 non-version package files are byte-identical to a24; the remaining package
file changes only the version constant. Archive boundaries and wheel RECORD
hashes are checked before upload. The validation record retains exact artifact
identities.

Minecraft was signed out during the final reconnect attempt. No new 0.4.0
interactive client pass is claimed. Client behavior is supported by the a24
qualification and verified gameplay-payload equivalence described above.

CI runs the public unit/contract suite and portable build on Windows and Linux.
CI does not launch a Bedrock client or establish a Linux native implementation.

[Passing Windows/Ubuntu CI](https://github.com/TheNINJALLO/endstone-remote-workstations/actions/runs/34247969420)
includes the clean-environment Endstone test-dependency fix.

Raw captures, worlds, account/item snapshots and server executables are private
test material and are excluded. Only the selected, reviewed documentation images
are published. See [support scope](SUPPORT.md) and the historical
[a24 report](CATALOG_TEST_REPORT.md).
