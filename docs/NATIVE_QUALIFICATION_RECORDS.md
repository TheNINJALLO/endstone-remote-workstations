# Native qualification gate

Compilation, loader smoke, catalog retention and interactive qualification are
separate results. `python tools/native/scope.py --require-qualified` currently
refuses all 138 entry/platform outcomes and 20 API-surface/platform outcomes.
Changing their acceptance labels cannot make the gate pass. The gate also
rejects changed aliases, commands, source contracts, historical statuses,
permissions, families, platforms and the immutable baseline hash.

A qualified mode needs an `implemented` adapter, no remaining blocker and one
or more `qualification_records` references. Each reference contains `path` and
`sha256`. Records belong below `research/native-evidence/interactive/`, are at
most 1 MiB, and must remain within that directory after path resolution. Never
commit private inventories, account identifiers, books, worlds or authentication
traffic to satisfy this requirement. Record redacted observations and approved
test-item outcomes. Original captures and synthetic fixtures remain separate.

An interactive record must declare:

- `kind`: `native-runtime-client-qualification`.
- Exact `platform`, `entry`, `mode`, `source_revision`, `artifact_sha256`,
  `bds_sha256` and `loader_sha256`, matching its platform report and tested build.
- `stock_client: true`, `public_sdk: true`, and `client` with device, version,
  input and protocol matching the declared profile.
- `example` with the standalone consumer, configuration and repeatable procedure;
  meaningful `expected_behavior` and `observed_behavior`.
- `checks` for permissions, stale requests, replay, closing, disconnect,
  source removal and durability. Values must be `passed`. Only source removal
  and durability can use `not-applicable`, with a specific reason under
  `not_applicable`; persistent mode cannot waive durability or restart.

Catalog mode records additionally require passed input and restoration checks.
An `original-native` presentation requires passed opening/rendering checks and
`native_bindings: fingerprinted`. Original real-source/native-context behavior
also requires `original_behavior: passed` and `source_contract: preserved`.
Custom modes require passed custom behavior and transaction callback checks.
Persistent mode additionally requires passed durability and restart checks.
These are attestations backed by reviewed test evidence, not switches which
cause an untested mode to become implemented.

Each originally implemented entry needs both original native behavior and
custom SDK behavior. A chest/form replacement cannot satisfy a different
original native screen. Correct-role and explicit-replacement presentations
must declare `native_available: false` and retain their explicit mode semantics.
For a former candidate without a native window, the entry additionally needs
a separately hashed `native_window_investigation` record, using mode
`investigation`, with `native_window_exists: false`, a technical reason and
primary sources. An unresolved hook or missing client is still incomplete.

Additional API surfaces require implementation, a passed client test and
similarly hashed records using mode `api-surface`, including passed custom
behavior and transaction callback checks. Core callback tests alone do not
qualify their in-game demonstration.

`python tools/native/test_scope.py` exercises dropped scope, changed source
contracts, forged green labels and attempts to substitute old captures/build
logs. It writes no qualification evidence. A human still reviews the recorded
behavior: matching hashes establish identity and integrity, not the truth of an
arbitrary claim in a file. Python optimization is refused so these checks cannot
be disabled through `-O` or `PYTHONOPTIMIZE`.
