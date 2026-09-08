from dataclasses import replace
import importlib.util
from pathlib import Path
import pytest
from endstone_remote_workstations.compatibility import LINUX_SHA256, WINDOWS_SHA256, require_native
from endstone_remote_workstations.model import Action, Ref, Rejected, Request, Slot
from endstone_remote_workstations.storage import StorageHarness


@pytest.mark.parametrize("binary,platform,runtime,protocol", [
    ("0"*64, "linux-x86_64", "0.11.10", 2169),
    (WINDOWS_SHA256, "windows-x86_64", "0.11.10", 2169),
    (LINUX_SHA256, "linux-x86_64", "0.11.0", 2169),
    (LINUX_SHA256, "linux-x86_64", "0.11.10", 2168),
    (LINUX_SHA256, "linux-x86_64", "0.11.10", 2169),
])
def test_no_known_or_unknown_native_build_can_call_unverified_code(binary, platform, runtime, protocol):
    with pytest.raises(Rejected):
        require_native(binary, platform, runtime, protocol)


def test_malformed_request_id_cannot_poison_replay_watermark(state):
    engine = StorageHarness(state)
    request = Request(state.generation, -1, 0, (Action("take", Ref(Slot("player", 0), 1), Ref(Slot("storage", 0), 0), 1),))
    assert not engine.handle(replace(request, request_id=-(2**99)+1)).ok
    assert engine.handle(request).ok


def test_manual_harness_rejects_unsubstantiated_success():
    path = Path(__file__).resolve().parents[1] / "tools/integration_harness.py"
    spec = importlib.util.spec_from_file_location("integration_harness", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    record = module.new_record()
    assert len(record["cases"]) == 54 and not module.validate(record)
    next(c for c in record["cases"] if c["id"] == "anvil/rename")["status"] = "passed"
    errors = module.validate(record)
    assert any("evidence" in e for e in errors) and any("vanilla comparison" in e for e in errors)


def test_plugin_never_registers_transactional_packet_cancellation():
    # Release safety invariant: enabling catalog/diagnostics must not claim BDS requests.
    path = Path(__file__).resolve().parents[1] / "src/endstone_remote_workstations/plugin.py"
    import ast
    tree = ast.parse(path.read_text(encoding="utf-8"))
    calls = [n for n in ast.walk(tree) if isinstance(n, ast.Call) and isinstance(n.func, ast.Attribute)]
    assert not any(n.func.attr in ("send_packet", "set_item", "drop_item", "set_data") for n in calls)
    assert not any(n.func.attr == "cancel" and isinstance(n.func.value, ast.Name) and n.func.value.id == "event" for n in calls)
