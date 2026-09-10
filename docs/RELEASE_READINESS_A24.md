> **Legacy v0.4.0 documentation.** This page describes the Python/native-companion implementation. For the native prerelease, use [installation](NATIVE_INSTALLATION.md), [examples](NATIVE_EXAMPLES.md) and [SDK 1.5](NATIVE_SDK.md).

# RemoteWorkstations 0.4.0a24 release readiness

This local Windows alpha candidate is packaged for controlled testing. It is
not a completed production release. Dependency API 1.5 exposes 53 implemented
catalog entry points with source, permission and lifecycle contracts; the
remaining 16 entries retain explicit unavailable dispositions.

## Qualification on the shipped wheel

| Check | Result | Scope |
| --- | --- | --- |
| Automated suite | 565 passed | Unit and contract tests; no failed or skipped tests |
| Installed catalog | 53/53 passed | Actual Windows screens and cleanup, including six feature-dependent entries |
| Console matrix | 95/95 passed | Catalog admission and diagnostics |
| Portable headless smoke | 17 commands passed | Plugin load and command checks; does not qualify the native ABI |
| Mounted merchant | 8 cases passed | Survival trade, Creative, cursor return, revocation and actual dismount changes; 10 accepted native item results |
| Dependency integration | 7 checks passed | Separate example plugin, protected pages, exported/private actions, anvil and actual Ender Chest routing |
| Installed payload | 35 provider files and 1 example file match | Exact wheel payloads |
| Wheel reproducibility | Both wheels matched | Byte-identical rebuilds before client qualification |

All live checks above use Windows Bedrock 1.26.45 Classic UI with keyboard and
mouse, Endstone 0.11.10, BDS 1.26.45.1, protocol 2169 and CPython 3.11 x86-64.
The original standard world was restored before its catalog and dependency
checks. Education features were tested in a separate clone. Selected earlier
Agent, NPC, chemistry and editor transactions retain their earlier wheel hashes
and are not counted as a24 transaction passes.

## Work still required for the full requested product

- Held shulker and bundle support needs actual item identity, escrow, nesting
  rules and durable writeback. Held book force-open/editing remains unbound.
- Crash reconciliation across BDS saves and the plugin journal lacks a durable
  commit marker. Full-inventory recovery and personal processing policy remain
  unqualified.
- Native editor gates need hardware shadow-stack and exception-path
  qualification. Command-block minecart command prefill remains unqualified.
- Complete recipe, enchantment, merchant-variant, load and plugin-coexistence
  coverage remains open. Earlier chemistry testing did not capture the invalid
  Lab Table recipe's garbage output.
- Linux native support is absent. Other devices and multiplayer remain
  untested; the user excluded them from this qualification run.

Twelve unavailable entries describe intrinsic gameplay or embedded views with
no bound standalone vanilla UI. They are documented in CAPABILITIES.md. A new
custom screen would require a separate contract and would not establish vanilla
parity.

## Local distribution

`RemoteWorkstations-0.4.0a24-windows-candidate.zip` contains the tested native
wheel, optional example wheel, pinned codec wheels, installation instructions,
licenses, configuration, capability catalog and bounded validation references.
`SHA256SUMS.json` covers every other archive entry. The refreshed source archive
includes the final tools and documentation; the tested wheel bytes are unchanged.

Raw screenshots, player snapshots, worlds and server executables are excluded.
Gameplay adapters are disabled by default. Follow INSTALL.md in an isolated
server, then opt into the required capabilities. No repository release was
published by this packaging step.
