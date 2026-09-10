> **Legacy v0.4.0 documentation.** This page describes the Python/native-companion implementation. For the native prerelease, use [installation](NATIVE_INSTALLATION.md), [examples](NATIVE_EXAMPLES.md) and [SDK 1.5](NATIVE_SDK.md).

# Reproducible build and runtime requirements

Public API: Endstone **0.11.0**, Python plugin metadata `api_version = "0.11"`.
Runtime target: Endstone **0.11.10**, BDS **1.26.45.1**, protocol **2169**.
Plugin Python requirement: **3.11+**. A pure Python wheel does not certify every Python/runtime platform.

## Build

Create an isolated venv, install `requirements-build.lock`, then run:

```text
python -m pytest -q
python -m build --no-isolation
```

The lock pins the entire small build/offline-test dependency set. The separate
`requirements-runtime-windows-cp311.lock` records the actual smoke-test environment, including Endstone.
`research/dependency-audit.json` records inspected dependency file hashes/licenses; it is not a
portable binary-wheel lock. Windows wheels must not be reused for Linux. Native wheels for bstream,
rapidnbt and Endstone must match the test interpreter/platform. A minimal build environment may skip
the one optional Endstone import test until its compatible runtime package is installed.

`tools/release.py` runs the final checks, builds wheel/sdist with a fixed SOURCE_DATE_EPOCH,
and writes artifact hashes. Rebuilding unchanged source under the same pinned tools is checked for
identical wheel bytes. The source archive includes tests/fixtures, tools, research records and docs;
it excludes `.research`, `.venv`, `.runtime`, BDS binaries and test worlds.

## Refresh local research files

Source distributions contain code, tests and bounded test fixtures. Raw live
captures, player snapshots, exported structures and server worlds stay in the
local development workspace. The candidate kit includes report hashes and
qualification limits in `VALIDATION.json`. Live qualification collectors require
the original local evidence; ordinary source builds and unit tests do not.

```text
python tools/fetch_references.py
python tools/generate_catalog.py
python tools/audit_api.py
python tools/inspect_bds.py /explicit/path/to/bedrock-server-Linux-1.26.45.1.zip /explicit/path/to/bedrock-server-Windows-1.26.45.1.zip
python tools/native_static_audit.py /explicit/path/to/bedrock-server-Linux-1.26.45.1.zip
```

Reference fetches use existing locked commits and verify archive hashes. They do not silently repin main.
The native static audit additionally needs `pyelftools==0.32`. It only scans an upstream recipe-payload
builder signature, never executes native methods. Inspection tools read only explicitly supplied archives.

## Isolated runtime smoke test

The headless test requires Endstone 0.11.10 inside `.venv` and the portable wheel already
built in dist. Do not also install the project into that interpreter: Endstone discovers
plugin entry points there as well as in its plugins folder.

```powershell
.venv\Scripts\python tools/smoke_windows.py 'C:\explicit\path\bedrock-server-Windows-1.26.45.1.zip'
```

The helper verifies the exact Windows executable hash, creates `.runtime/windows-headless-smoke`,
uses an empty allowlist and test ports 29171/29172, invokes Endstone's own DLL launcher, runs console
commands and stops automatically. Online Minecraft authentication needs network access. It does not
alter firewall/loopback exemptions, install a service or use an existing server/world.
The embedded interpreter receives a local `sitecustomize.py` that excludes base-site packages;
this is necessary because the launcher venv alone did not prevent discovery of globally installed plugins.
Only `remote_workstations` appeared in the successful run's plugin list.

This is **public Python plugin smoke support on Windows**, not a Windows native companion build or
ABI certification. Linux BDS/Endstone execution and live remote-workstation tests remain outstanding;
Windows command/form client checks are recorded in [WINDOWS_CLIENT_TESTS.md](WINDOWS_CLIENT_TESTS.md). No `.so`/`.dll`
companion is produced or promised by these instructions.

## Optional cosmetic resource pack

```text
python tools/build_presentation_pack.py
```

This produces `dist/RemoteWorkstations-forms-unverified.mcpack` from the locked Chest-UI RP with
attribution. It is an optional, unverified action-form presentation pack. Set `presentation.chest_forms`
only on a test server whose clients have the matching pack. Supported sizes are 1, 5 and 9–54 in rows
of 9. The plugin's catalog chooses sufficient rows automatically; the separate encoder also supports
lit/unlit furnace-style selectors. Icons use explicit texture paths, never guessed numeric item mappings.
The base catalog works without this pack. No native workstation depends on it.
