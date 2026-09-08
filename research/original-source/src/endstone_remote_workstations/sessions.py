"""Bounded session ownership; window IDs are never reused within a connection."""
from dataclasses import dataclass, field
from enum import Enum
import threading
import uuid
from .model import Rejected


class Phase(str, Enum):
    OPENING = "opening"
    OPEN = "open"
    CLOSING = "closing"
    CLOSED = "closed"


@dataclass(frozen=True)
class Position:
    dimension: str
    x: int
    y: int
    z: int


@dataclass
class Session:
    player: str
    generation: str
    window: int
    backend: str
    dimension: str
    deadline: float
    phase: Phase = Phase.OPENING
    temporary: list[Position] = field(default_factory=list)
    close_reason: str | None = None


class Sessions:
    def __init__(self, maximum=100, opening_timeout=10.0, closing_timeout=5.0):
        if not 1 <= maximum <= 10000 or opening_timeout <= 0 or closing_timeout <= 0:
            raise ValueError("invalid session limits")
        self.maximum, self.opening_timeout, self.closing_timeout = maximum, opening_timeout, closing_timeout
        self.active = {}
        self.used = {}
        self.thread = threading.get_ident()

    def assert_thread(self):
        if threading.get_ident() != self.thread:
            raise RuntimeError("game-object access must run on the owning server thread")

    def begin(self, player, backend, dimension, now, occupied=()):
        self.assert_thread()
        if player in self.active or len(self.used) >= self.maximum and player not in self.used:
            raise Rejected("one session per connection; session limit reached")
        retired = self.used.setdefault(player, set())
        # 0 is inventory, special IDs >= 100 are not allocated by this prototype.
        available = set(range(1, 100)) - retired - set(occupied)
        if not available:
            raise Rejected("window IDs exhausted; reconnect before reopening")
        window = min(available)
        retired.add(window)
        session = Session(player, uuid.uuid4().hex, window, backend, dimension, now+self.opening_timeout)
        self.active[player] = session
        return session

    def acknowledge(self, player, generation, evidence):
        self.assert_thread()
        session = self.active.get(player)
        if not session or session.generation != generation or session.phase != Phase.OPENING:
            raise Rejected("stale readiness event")
        # A caller needs a backend-specific, capture-validated readiness event.
        # No BDS adapter supplies one in this release.
        if evidence != "model-harness-ready":
            raise Rejected("no verified container readiness evidence")
        if session.backend != "model-harness":
            raise Rejected("model evidence cannot activate a live backend")
        session.phase = Phase.OPEN
        session.deadline = float("inf")

    def request_close(self, player, reason, now):
        self.assert_thread()
        session = self.active.get(player)
        if session and session.phase not in (Phase.CLOSING, Phase.CLOSED):
            session.phase = Phase.CLOSING
            session.deadline = now+self.closing_timeout
            session.close_reason = reason
        return session

    def finish(self, player, generation, cleanup):
        self.assert_thread()
        session = self.active.get(player)
        if not session or session.generation != generation:
            return False
        # Cleanup failures remain tracked and retryable, never silently discarded.
        cleanup(session)
        session.phase = Phase.CLOSED
        del self.active[player]
        return True

    def expire(self, now, cleanup):
        self.assert_thread()
        for session in tuple(self.active.values()):
            if now >= session.deadline:
                session.close_reason = "timeout"
                self.finish(session.player, session.generation, cleanup)

    def disconnect(self, player, cleanup):
        self.assert_thread()
        session = self.active.get(player)
        if session:
            self.finish(player, session.generation, cleanup)
        self.used.pop(player, None)

    def shutdown(self, cleanup):
        self.assert_thread()
        for player in tuple(self.used):
            self.disconnect(player, cleanup)


def restore_temporary(session, current_dimension, authoritative, send):
    """Query CURRENT complete layer + block-actor payloads; never replay stale states.

    Callbacks are an interface contract, not fabricated Endstone methods. A live
    adapter is unavailable at API 0.11.0 and is intentionally not constructed.
    """
    for position in tuple(session.temporary):
        if position.dimension != current_dimension:
            # Dimension transition discarded the old client chunk. Do not send an
            # old-dimension block into the new dimension at the same coordinates.
            session.temporary.remove(position)
            continue
        state = authoritative(position)
        if state is None or not state.complete:
            raise Rejected("complete authoritative block layers and actor NBT unavailable")
        send(position, state)
        session.temporary.remove(position)

