from dataclasses import replace
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import pytest
from endstone_remote_workstations.journal import Journal, JournalWorker
from endstone_remote_workstations.model import Action, Item, Ref, Rejected, Request, Slot, plan_storage, return_cursor


def make_plan(state):
    return plan_storage(state, Request(state.generation, -1, state.version,
                        (Action("take", Ref(Slot("player", 0), 1), Ref(Slot("storage", 0), 0), 7),)))


def test_storage_deposit_close_reopen_with_full_metadata(tmp_path, state):
    path = tmp_path / "journal.sqlite3"
    journal = Journal(path)
    deposited = make_plan(state).after
    journal.save_model("owner", deposited)
    journal.close()
    journal = Journal(path)
    restored = journal.load_model("owner")
    assert restored == deposited and restored.storage[0].metadata == state.player[0].metadata
    take = Request(restored.generation, -3, restored.version,
                   (Action("take", Ref(Slot("storage", 0), restored.storage[0].net_id), Ref(Slot("player", 0), 1), 7),))
    returned = plan_storage(restored, take).after
    assert returned.player[0] == state.player[0] and not any(returned.storage)
    journal.close()


def test_external_journal_idempotency_conflict_and_quarantine(tmp_path, state):
    journal = Journal(tmp_path / "journal.sqlite3")
    plan = make_plan(state)
    identity = journal.prepare("owner", -1, plan)
    with pytest.raises(Rejected):
        journal.prepare("owner", -1, plan)
    journal.mark_applied(identity)
    with pytest.raises(Rejected):
        journal.mark_applied(identity)
    assert journal.recover()[0][2] == "quarantined"
    assert journal.recover()[0][2] == "quarantined"
    journal.close()


def test_cursor_overflow_persisted_in_same_model_transaction(tmp_path, state):
    state = replace(state, player=tuple(Item("minecraft:dirt", 64, 64, n+1) for n in range(36)),
                    cursor=(Item("minecraft:stone", 8, 64, 37, b"metadata"),), next_net_id=38)
    cleaned, overflow = return_cursor(state)
    journal = Journal(tmp_path / "journal.sqlite3")
    journal.save_model("owner", cleaned, "cleanup-one", overflow)
    journal.save_model("owner", cleaned, "cleanup-one", overflow)
    assert journal.db.execute("SELECT COUNT(*) FROM recovery").fetchone()[0] == 1
    assert journal.load_model("owner").cursor == (None,)
    with pytest.raises(Rejected):
        journal.save_model("different-owner", cleaned, "cleanup-one", overflow)
    assert journal.load_model("different-owner") is None
    journal.close()


@pytest.mark.parametrize("boundary", ["prepared", "external-partial", "external-applied", "applied-memory"])
def test_actual_process_death_across_two_independent_model_stores(tmp_path, state, boundary):
    # The second store SIMULATES BDS durability. These are real subprocess deaths,
    # not real BDS crash tests. Every outcome must quarantine without issuing items.
    child = Path(__file__).parent / "helpers/crash_child.py"
    initial = tmp_path / "initial.json"
    initial.write_text(state.serialize())
    env = {**os.environ, "PYTHONPATH": str(Path(__file__).resolve().parents[1] / "src")}
    result = subprocess.run([sys.executable, str(child), str(tmp_path), boundary], env=env, capture_output=True)
    assert result.returncode == 73, result.stderr.decode()
    external = (tmp_path / "external.json").read_bytes() if (tmp_path / "external.json").exists() else None
    journal = Journal(tmp_path / "journal.sqlite3")
    quarantined = journal.recover()
    assert len(quarantined) == 1 and quarantined[0][2] == "quarantined"
    assert journal.db.execute("SELECT COUNT(*) FROM recovery").fetchone()[0] == 0
    assert ((tmp_path / "external.json").read_bytes() if (tmp_path / "external.json").exists() else None) == external
    journal.close()


def test_disk_worker_serializes_and_shuts_down(tmp_path, state):
    worker = JournalWorker(tmp_path / "journal.sqlite3", capacity=10)
    worker.submit("save_model", "owner", state).result(timeout=5)
    assert worker.submit("load_model", "owner").result(timeout=5) == state
    worker.close()
    worker.close()
    with pytest.raises(Rejected):
        worker.submit("counts")
