# Windows Docker development environment

Docker Desktop with its WSL 2 backend supplies the Linux engine for this
project's pinned `linux/amd64` build. Docker's installation version is a host
tool; the compiler image, SDK and source dependencies remain separately pinned.
The existing stable RemoteWorkstations release remains available alongside the
native development branch.

On the development PC, Docker Desktop **4.90.0** and Microsoft WSL **2.7.13**
were installed on September 8, 2026. Both installer SHA-256 values and publisher
signatures were verified before execution. Virtual Machine Platform changed
from Disabled to Enabled using `-NoRestart`. Windows reported a required restart;
the hypervisor was still inactive at the installation checkpoint. After the
operator restarted Windows, the Linux x86-64 engine became reachable. The pinned
build, private ELF inspection, and actual Endstone/BDS server now execute locally.

Observed installed clients: Docker CLI 29.7.2, Compose 5.5.1, Buildx
0.36.1-desktop.1; WSL reports kernel 6.18.33.2-2. Docker is installed per user
under `%LOCALAPPDATA%\Programs\DockerDesktop`. The build scripts also detect
this location when a running terminal still has its pre-installation PATH.
The installer did not accept Docker's Subscription Service Agreement. The
operator completed first-launch setup; no paid subscription was purchased.

The installation follows [Docker's Windows guide](https://docs.docker.com/desktop/setup/install/windows-install/)
and [Microsoft's WSL guidance](https://learn.microsoft.com/windows/wsl/install).
Docker's own WSL distribution is sufficient for this container workflow; a
separate Ubuntu desktop/distribution is not a project requirement.

After the required Windows restart, open Docker Desktop, complete its
first-launch agreement and wait for the Linux engine. From the native checkout:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/native/build-linux.ps1
```

The preflight checks a reachable Linux x86-64 server, Compose and Buildx. Its
engine probe times out after 15 seconds with an explicit blocker. It does not
enable a TCP daemon endpoint, install privileged services or change global
virtualization settings. The build exports to `dist/linux-x64-dev`.

The local authorized BDS 1.26.45.1 Linux archive and official Endstone 0.11.10
Linux runtime wheel were staged outside the public checkout/build context.
Archive paths were checked against traversal and symlinks; both ELF headers
identify x86-64. The provider now admits the exact loaded files for public SDK
use; [Linux runtime evidence](NATIVE_LINUX_RUNTIME.md) describes the remaining
private-adapter and gameplay qualification boundaries.
The private `input-manifest.json` records provenance and exact executable
hashes; `SHA256SUMS` covers all staged files. Neither the private inputs nor
inspection output belongs in a release or public image.

Use the private input directory explicitly for research:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/native/inspect-linux.ps1 -Inputs 'C:\path\to\private-linux-inputs'
```

The input layout has `server/bedrock_server`, the extracted runtime below
`loader/`, `input-manifest.json` and `SHA256SUMS`. ELF inspection runs inside
Linux Docker and writes private reports below `out/linux-abi`. It verifies
file hashes, headers, loader/library dependencies, symbol versions and exported
symbols. It does not claim native function/object-layout compatibility.

Runtime smoke additionally needs a separately admitted private `launch.sh` and
explicit license confirmation. Both are present for the local disposable test
server; another operator must supply their own authorized inputs and launcher:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/native/smoke-linux.ps1 -Inputs 'C:\path\to\private-linux-inputs'
```

The Linux test endpoint defaults to `127.0.0.1:29179`, distinct from the isolated
Windows test server on port 29169. Its container still listens on 29169.
`VCF_LINUX_TEST_PORT` can select another loopback host port. Private input mounts
are read-only and must already exist. A dedicated named Linux volume stores
the disposable server data; it is separate from existing worlds. Smoke runs without root or capabilities;
only the explicit ABI research profile grants ptrace. Public Compose build
configuration no longer requires unrelated private input variables.

Linux contributors have `tools/native/build-linux.sh`, `inspect-linux.sh` and
`smoke-linux.sh`. The runtime and full catalog acceptance gates still apply:
installing Docker or inspecting an ELF does not make the native framework
release-qualified.
