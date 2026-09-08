# Contributing

Use Python 3.11. Install the pinned build/test dependencies and run `python -m
pytest -q`. The public tests use fixtures/stubs and do not start a live BDS server.
See [building](docs/BUILDING.md) for native compilation.

Keep the dependency API backward compatible or explicitly version its contract.
Native changes must remain pinned to verified executable/runtime identities.
Test real inventory/source behavior in an isolated world before claiming support.
Do not treat a packet shape or successful screen open as transaction parity.

Reports and pull requests should identify the changed behavior, tested artifact,
validation and remaining limits. Do not commit proprietary server executables,
worlds, credentials or raw player captures. Screenshots for documentation must
be reviewed for private information and carry an accurate caption.
