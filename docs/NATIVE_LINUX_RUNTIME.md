# Linux runtime development

The local Docker Desktop Linux engine now runs on the Windows development PC.
The pinned Linux build and private ELF inspection have executed locally.
This removes the installation/restart blocker; it does not complete the native
UI migration or qualify a production release.

The Linux provider identifies `/proc/self/exe` and the mapped
`endstone_get_server` module. It hashes bounded regular files with OpenSSL 3,
checks the runtime pathname against the mapped inode, and refuses unknown
fingerprints. The exact allowed files are:

| File | SHA-256 | ELF Build ID |
|---|---|---|
| BDS 1.26.45.1 | `8ba803f23d681816495c7ac83bdba4b9cd7165a3bee5aedd18fa0c8c3d408ec2` | `0b40c886132941f2cd98d45e73062bc75e66726e` |
| Endstone 0.11.10, CPython 3.11 Linux wheel runtime | `ac665adb20c9d5c640da9e88de956d728f6dd7771a4461ce8205824ec9b300bc` | `ba6af2ee50effc20b461a2fbef80e145ad6f6a51` |

Admission enables the public SDK integration only. Linux private workstation
adapters remain unavailable. Onistone and other Endstone builds need separate
evidence and admission; a matching version label does not suffice.

The provider promotes its existing loaded shadow copy with `RTLD_NOLOAD` and
`RTLD_NODELETE` before registering callbacks. It does not load a second copy.
Each enable operation also creates a new callback lifetime token, so re-enabling
the same plugin object cannot reactivate an old form callback. The Linux tests
exercise SHA-256 vectors, streaming and bounds, a fake runtime with the expected
filename/export, replacement of a mapped pathname, and retained code after
`dlclose`. These tests do not substitute for real loader/client tests.

## Dependencies and the isolated fixture

The inspected Endstone runtime records Clang **20.1.8** in its ELF compiler
metadata, matching the pinned compiler family. Its wheel bundles renamed
libc++, libc++abi and libunwind libraries. The private Linux fixture resolves
the plugin's standard library names to these same bundled files, avoiding
loading an additional compiler-image C++ runtime into BDS. Actual process maps
confirmed one loaded libc++ family, and both independent native consumers loaded
and executed through the C ABI. Endstone's NumPy and frozenlist dependencies also
load libstdc++; the provider and native consumers do not link against it.

A stock Windows Bedrock 1.26.45 client connected to the isolated Linux server,
rendered the consumer's action form, and invoked its registered action. This
exposed a Linux player-conversion failure: cross-module `dynamic_cast` returned
null. Provider and consumers now use the pinned SDK's virtual `asPlayer()` API.
Full catalog and transaction qualification remain separate from this form test.

The [Linux SDK example](examples/linux-native-sdk.md) includes screenshots,
commands and the final artifact's redacted client smoke record. The verified
form action, close-with-X and disconnect paths return to zero outstanding
tickets. The actual item registry and full inventory now decode with zero
refusals. A zero-initialized container name is accepted only within the bounded
player-inventory observation path; it grants no inventory-write authority.

OpenSSL's `libcrypto.so.3` is now a direct plugin dependency for SHA-256.
The Docker package snapshot supplies OpenSSL 3.0.20 and `libssl-dev` at build
time. OpenSSL 3 uses the Apache-2.0 license; it is dynamically linked rather
than copied into the plugin. Its [digest API](https://docs.openssl.org/3.0/man3/EVP_DigestInit/)
and the Linux [dynamic loader flags](https://man7.org/linux/man-pages/man3/dlopen.3.html)
describe the interfaces used here. Transitive glibc/library requirements still
apply; this is not certification of an arbitrary Pterodactyl image.

The private Endstone loader wheelhouse contains exact versions and hashes for
offline installation. Endstone's own Python loader is distinct from the project
plugin, which remains native C++ with no project Python companion.

The Compose smoke profile uses a dedicated Linux data volume owned by UID
10001, read-only private input mounts, no added capabilities, and loopback UDP
port 29179. The fixture seeds its disposable world from a verified private
archive to avoid Windows bind-mount overhead for thousands of asset files.
It refuses to initialize over an existing unmarked server directory. Both
independent SDK consumer `.so` files accompany the provider in this test.

No private BDS/runtime files, dependency wheelhouse, world volume, or raw runtime
captures belong in the public build context, source repository or release.
The smoke command requires separate confirmation of the supplied server's
license and an authorized private launcher; no script accepts a license.
