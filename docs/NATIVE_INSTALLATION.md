# Install the native prerelease

Use **v0.5.0-native.1** in an isolated Endstone instance. Keep stable v0.4.0 in a separate instance and retain its world/configuration backups. This prerelease does not automatically migrate legacy storage or journals.

## Requirements

| Component | Admitted/tested target |
| --- | --- |
| Server | Endstone 0.11.10, BDS 1.26.45.1; exact binary hashes checked |
| Experimental inventory client | Stock Windows Bedrock 1.26.45, protocol 2169, Classic keyboard/mouse |
| Linux | x86-64, glibc 2.34+, compatible libc++20/libc++abi20, libunwind and OpenSSL 3 |
| Windows | x64, compatible Endstone/MSVC runtime; current DLL gameplay qualification incomplete |

The Linux fixture used Debian Bookworm and Clang/libc++20. Other Pterodactyl images need dependency/runtime validation. Android/touch, controllers and multiplayer are untested. Server executables, private worlds and proprietary symbols are not included; supply your authorized server installation and accept its license yourself.

## Linux and Pterodactyl

1. Download the Linux provider archive and `SHA256SUMS` from the [prerelease](https://github.com/TheNINJALLO/endstone-remote-workstations/releases/tag/v0.5.0-native.1). Verify its SHA-256.
2. Stop the isolated server. Copy `plugins/endstone_onistone_vcf.so` into its `plugins/` directory.
3. Run `ldd plugins/endstone_onistone_vcf.so` and resolve missing libraries in the server container image. The included ELF report lists this build's requirements.
4. In Pterodactyl, upload using Files or SFTP. The selected egg/image must already supply compatible Endstone and Linux libraries. Uploading the plugin cannot install system dependencies. Docker Desktop is a development tool, not an operator requirement.
5. Start once to create `plugins/onistone_vcf/config.json`, then stop before editing it. Review startup admission messages.

Safe defaults:

```json
{
  "schema_version": 1,
  "experimental_original_linux": false,
  "experimental_original_windows": false,
  "experimental_inventory_writes": false,
  "experimental_held_storage": false
}
```

Enable `experimental_original_linux` for documented original contexts/linked sources. Held shulker testing requires **both** `experimental_inventory_writes` and `experimental_held_storage`. Restart after changes. Unsupported runtimes must remain refused.

## Windows

Copy `plugins/endstone_onistone_vcf.dll` from the Windows provider archive into the stopped isolated server's `plugins/` directory. Start once, stop, and optionally enable `experimental_original_windows` in the generated configuration. Restart and check admission.

Linux-only linked adapters, inventory writes and held editing remain unavailable. The Windows package is a development build and does not inherit Linux gameplay evidence. No project wheel is required.

## Examples and permissions

Example binaries are a separate download. Install only your platform's `endstone_vcf_catalog_showcase` and/or `endstone_vcf_native_passthrough`. They depend on `onistone_vcf`.

Examples are operator-only by default. Grant explicit permissions through your normal permissions plugin; the framework does not grant Creative/operator privileges. Start with `/vcf status`, `/vcf capabilities`, `/vcf sessions` and `/vcf diagnose`, then follow [the examples](NATIVE_EXAMPLES.md).

## Recovery and upgrades

After restart, surviving inventory edits require `/vcf recovery` review. Check the actual inventory before using the displayed short-lived `accept-current` token. Acknowledgement records the accepted inventory without granting or restoring items. Never remove `inventory-edits.vcf` to bypass quarantine or retry a quarantined transfer as if it had not happened.

Stop before replacing binaries. Preserve provider data and world backups together, verify checksums and test upgrades in a copy. [Recovery details](NATIVE_INVENTORY_WRITES.md) explain the unresolved BDS/plugin save boundary.
