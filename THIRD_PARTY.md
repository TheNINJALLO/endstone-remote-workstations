# Third-party notices

Original RemoteWorkstations code is MIT, except the attributed optional Chest-UI encoding port, for which CC BY 4.0 attribution is retained.
The Windows wheel contains our native companion compiled against Endstone public headers.
It calls verified private BDS entry points in the installed server. BDS binaries and proprietary code are not distributed.

| Reference | License inspected | Treatment |
|---|---|---|
| Herobrine643928/Chest-UI | CC BY 4.0 (`License`) | Optional encoding port and optional resource pack; author/source/license/change notices included. Original authors LeGend077 and Herobrine64; pattern contribution Aex66. |
| Shock95/endstone-inventoryui | MIT | Reviewed, not vendored or imported by the plugin; license retained for attribution. |
| TheNINJALLO/endstone-inventoryui 2.0.6 | MIT | User's fork reviewed separately; optional integration adapter targets its object contract and captured BDS wire data. Full helper source is not vendored. |
| TheNINJALLO/endstone-inventory-manager 1.0.15 | MIT declared in project metadata | Reviewed actual online Ender Chest access/writeback; not vendored. |
| TheNINJALLO/endstone-ninjos-backpacks 1.0.90 | No root license declaration located | User-requested source review of pagination, metadata and storage; not vendored. |
| EndstoneMC/endstone, both pinned versions | Apache 2.0 | Public API/runtime used; no BDS internals copied into plugin. License notice included. |
| EndstoneMC/protocol-docs r26_u4 | No root license located | Factual protocol field/enum observations recorded; full documentation tree excluded from release artifacts. |
| Mojang/bedrock-protocol-docs | Mojang copyright / Minecraft EULA notice | Consulted for cross-checks; documentation files and BDS executables excluded from release artifacts. |
| bedrock-protocol-packets-ng 0.0.11 | MPL 2.0 | Optional test dependency, unmodified, not vendored; upstream Shock95 fork of GlacieTeam package. |
| bstream 1.0.1, rapidnbt 1.3.5 | MPL 2.0 | Pinned runtime codecs for the packet backend, installed as unmodified separate dependencies. |
| pybind11 3.0.1 | BSD 3-Clause | Header-only Python bridge compiled into our Windows companion; license included. |
| expected-lite 0.9.0 | Boost Software License 1.0 | Endstone public-header dependency; license included. |
| OpenSSL 3 | Apache 2.0 | Native Linux provider dynamically links `libcrypto.so.3` for loaded-file SHA-256 verification. The Docker build pins the Debian package snapshot; no OpenSSL binary is bundled in plugin artifacts. |

The Windows companion and disposable tracing probe compile MinHook 1.3.4
(BSD 2-Clause) for scoped native hooks. Its license is included in
`licenses/MinHook-BSD.txt`; source/archive identities are pinned in research.
Other header dependencies used only for the independent SDK layout audit are recorded in
`research/native-header-dependencies.json`; those headers and probe binaries are excluded from release artifacts.

Exact Git commits and downloaded archive hashes are in `research/references.lock.json`.
User-requested helper revisions are recorded separately in `research/helper-references.lock.json`.
Installed package metadata and source/binary file hashes are in `research/dependency-audit.json`.
Text notices are under `licenses/`. The optional `.mcpack` includes its own attribution and license.

Optional pack changes: new pack identity/version, removed the behavior-pack dependency because Endstone produces the forms,
disabled the decorative player inventory section, and enabled the supported layouts. No vanilla item numeric mapping is copied into Python.
The untouched normal-form visibility condition from upstream remains; coexistence still requires actual client validation.
