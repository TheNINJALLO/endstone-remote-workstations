"""Bounded native lab controls; BDS owns inputs, recipes and world reactions."""
import time
from .protocol import CodecError, Reader, read_ref, svar


def cleanup_request(payload, kind):
    """Admit only native returns/drops and disposal of chemistry previews.

    These pinned UI slot ranges come from the actual 2169 chemistry requests.
    This function grants no items and rewrites no payload; BDS still validates
    counts, stack IDs, request aliases and the current manager's slot ownership.
    """
    inputs = {
        'elementconstructor': {},
        'compoundcreator': {35: range(18, 27)},
        'materialreducer': {38: (8,), 39: range(41, 50)},
        'labtable': {40: range(9)},
        'agent': {},  # Persistent Agent storage is never returned on close.
    }[kind]
    previews = {'elementconstructor': (37,), 'compoundcreator': (36,),
                'materialreducer': (39,), 'labtable': (), 'agent': ()}[kind]
    reader = Reader(payload)
    count = reader.uvar()
    if not 1 <= count <= 100:
        raise CodecError('Invalid native cleanup batch.')
    for _ in range(count):
        identity, actions = reader.svar(), reader.uvar()
        if identity >= 0 or identity % 2 != 1 or not 1 <= actions <= 100:
            raise CodecError('Invalid native cleanup request.')
        for _ in range(actions):
            variant, inner, amount = reader.uvar(), reader.byte(), reader.byte()
            if variant not in (0, 1, 3, 4) or inner != variant or not 1 <= amount <= 64:
                raise CodecError('Cleanup cannot craft, create, consume, swap or combine.')
            source = read_ref(reader)
            if source.dynamic_id is not None:
                raise CodecError('Cleanup cannot access item-backed containers.')
            if variant == 4:
                if source.role not in previews:
                    raise CodecError('Cleanup may only discard native preview slots.')
                continue
            if not ((source.role == 59 and source.slot == 0) or source.slot in inputs.get(source.role, ())):
                raise CodecError('Cleanup source is outside the owned chemistry inputs.')
            if variant == 3:
                reader.boolean()
                continue
            destination = read_ref(reader)
            allowed = {12: range(36), 28: range(9), 29: range(9, 36)}
            if destination.dynamic_id is not None or destination.slot not in allowed.get(destination.role, ()):
                raise CodecError('Cleanup may only return inputs to the player inventory.')
        if reader.uvar() != 0:
            raise CodecError('Cleanup cannot supply filter strings.')
        reader.raw(4)
    reader.end()


def lab_control(payload):
    reader = Reader(payload, limit=32)
    action = reader.byte()
    position = tuple(reader.svar() for _ in range(3))
    reaction = reader.byte()
    reader.end()
    if action not in (0, 1, 2) or not 0 <= reaction <= 12:
        raise CodecError('Invalid native laboratory control.')
    return action, position, reaction


def lab_payload(action, position, reaction):
    return bytes((action,))+b''.join(svar(v) for v in position)+bytes((reaction,))


class LabControls:
    def __init__(self, backend):
        self.backend = backend
        self.positions = {}

    def claim(self, session):
        if session.kind != 'labtable':
            return
        now = time.monotonic()
        self.positions = {key: deadline for key, deadline in self.positions.items() if deadline > now}
        if len(self.positions) >= 4096:
            raise RuntimeError('Too many recent laboratory controls; retry after one minute.')
        self.positions[(session.player_id, session.dimension, session.position)] = float('inf')

    def release(self, session):
        if session and session.kind == 'labtable':
            self.positions[(session.player_id, session.dimension, session.position)] = time.monotonic()+60

    def project(self, event, session):
        if session.kind != 'labtable':
            return
        action, position, reaction = lab_control(bytes(event.payload))
        if position == session.context.position:
            event.payload = lab_payload(action, session.position, reaction)

    def check_active(self, player, session):
        backend = self.backend
        state = backend.bridge.state(player)
        if (session.window is None or session.closing_at or session.close_pending
                or state['ready'] or state.get('window') != session.window
                or backend.reason(player, session.kind, session.context)):
            raise RuntimeError('The owned chemistry manager is unavailable.')
        backend._check_guards(player, session.kind)

    def validate_inventory(self, event, session):
        # Modern clients combine through ItemStackRequest action 9. Revalidate
        # every owned chemistry request at dispatch; BDS decodes and executes
        # its original payload, including recipe and inventory validation.
        try:
            self.check_active(event.player, session)
        except Exception:
            session.close_pending = True
            try:
                state = self.backend.bridge.state(event.player)
                if (session.window is None or state['ready'] or state.get('window') != session.window
                        or event.player.dimension.name != session.dimension):
                    raise CodecError('The native cleanup manager changed.')
                cleanup_request(bytes(event.payload), session.kind)
            except Exception:
                event.is_cancelled = True

    def receive(self, event, session):
        backend, player = self.backend, event.player
        try:
            action, position, reaction = lab_control(bytes(event.payload))
        except CodecError:
            if session and session.kind == 'labtable':
                event.is_cancelled = True
                session.close_pending = True
            return
        owned = session and session.kind == 'labtable' and position == session.position
        if not owned:
            retired = (player.unique_id, player.dimension.name, position)
            if self.positions.get(retired, 0) > time.monotonic() or (session and session.kind == 'labtable'
                    and position == session.context.position):
                event.is_cancelled = True
            return
        try:
            if action != 0 or reaction != 0:
                raise RuntimeError('The owned laboratory control is unavailable.')
            self.check_active(player, session)
        except Exception:
            event.is_cancelled = True
            session.close_pending = True
            return
        # The original server handler resolves the real LabTableBlockActor and
        # consumes its own inputs. Never synthesize a reaction or item result.
        event.payload = lab_payload(action, session.context.position, reaction)
