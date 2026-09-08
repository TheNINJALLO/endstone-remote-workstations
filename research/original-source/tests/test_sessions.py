from types import SimpleNamespace
import threading
import pytest
from endstone_remote_workstations.model import Rejected
from endstone_remote_workstations.sessions import Sessions, Position, Phase, restore_temporary


def test_windows_generation_timeouts_and_no_violation_ack():
    sessions = Sessions(maximum=1)
    first = sessions.begin("player", "model-harness", "overworld", 0, occupied=(1,))
    assert first.window == 2
    with pytest.raises(Rejected):
        sessions.acknowledge("player", first.generation, "PacketViolationWarning")
    with pytest.raises(Rejected):
        sessions.begin("player", "model-harness", "overworld", 0)
    with pytest.raises(Rejected):
        sessions.begin("other", "model-harness", "overworld", 0)
    sessions.acknowledge("player", first.generation, "model-harness-ready")
    assert first.phase == Phase.OPEN
    closed = []
    sessions.request_close("player", "manual", 2)
    sessions.expire(7, closed.append)
    assert closed == [first]
    second = sessions.begin("player", "model-harness", "nether", 8)
    assert second.generation != first.generation and second.window != first.window
    assert not sessions.finish("player", first.generation, closed.append)
    sessions.expire(18, closed.append)
    sessions.disconnect("player", closed.append)
    assert not sessions.active and not sessions.used


@pytest.mark.parametrize("reason", ["manual", "forced", "timeout", "disconnect", "death", "teleport", "dimension", "disable", "reload", "shutdown"])
def test_cleanup_idempotence_for_lifecycle_reasons(reason):
    sessions = Sessions()
    session = sessions.begin("p", "model-harness", "overworld", 0)
    done = []
    sessions.request_close("p", reason, 1)
    sessions.finish("p", session.generation, done.append)
    sessions.finish("p", session.generation, done.append)
    sessions.shutdown(done.append)
    assert done == [session]


def test_cleanup_failure_retains_session():
    sessions = Sessions()
    session = sessions.begin("p", "model-harness", "overworld", 0)
    with pytest.raises(OSError):
        sessions.finish("p", session.generation, lambda s: (_ for _ in ()).throw(OSError("disk full")))
    assert sessions.active["p"] is session


def test_thread_guard_and_no_live_readiness():
    sessions = Sessions()
    session = sessions.begin("p", "python-packet", "overworld", 0)
    with pytest.raises(Rejected):
        sessions.acknowledge("p", session.generation, "model-harness-ready")
    result = []
    def worker():
        try:
            sessions.assert_thread()
        except RuntimeError:
            result.append(True)
    thread = threading.Thread(target=worker)
    thread.start()
    thread.join()
    assert result == [True]


def test_restore_uses_current_complete_layers_and_dimension():
    sessions = Sessions()
    session = sessions.begin("p", "model-harness", "overworld", 0)
    pos = Position("overworld", 1, 2, 3)
    session.temporary = [pos, Position("nether", 1, 2, 3)]
    current = SimpleNamespace(complete=True, layers=("changed-facing", "water"), actor=b"current-nbt")
    sent = []
    restore_temporary(session, "overworld", lambda p: current, lambda p, s: sent.append((p, s)))
    assert sent == [(pos, current)] and session.temporary == []
