"""Real native sign editor lifecycle; no text is recorded in diagnostics."""
import secrets
import struct
import time

from .protocol import CodecError, Reader, svar
from .linked import project_actor_data


def validate_sign_payload(payload, expected):
    """Strict bounded network-NBT reader for the native sign save schema.

    rapidnbt intentionally tolerates some truncated compounds. Validate the
    complete client structure before using it for coordinate translation.
    """
    if len(payload) > 16384:
        raise CodecError('Sign payload exceeds its limit.')
    reader = Reader(payload)
    if tuple(reader.svar() for _ in range(3)) != expected:
        raise CodecError('Sign position does not match the editor.')
    nodes = 0
    def string(limit):
        size = reader.uvar()
        if size > limit:
            raise CodecError('Sign string exceeds its limit.')
        try:
            return reader.raw(size).decode('utf-8', errors='strict')
        except UnicodeError as error:
            raise CodecError('Invalid sign text encoding.') from error
    def value(kind, depth):
        nonlocal nodes
        nodes += 1
        if nodes > 64 or depth > 3:
            raise CodecError('Sign structure exceeds its limit.')
        if kind == 1:
            return reader.byte()
        if kind == 3:
            return reader.svar()
        if kind == 4:
            return reader.svar(64)
        if kind == 8:
            return string(8192)
        if kind != 10:
            raise CodecError('Unexpected sign tag type.')
        result = {}
        while True:
            child = reader.byte()
            if child == 0:
                return result
            name = string(64)
            if name in result:
                raise CodecError('Duplicate sign field.')
            result[name] = (child, value(child, depth+1))
    if reader.byte() != 10 or string(64) != '':
        raise CodecError('Expected an unnamed sign compound.')
    root = value(10, 0)
    reader.end()
    types = {'id': 8, 'x': 3, 'y': 3, 'z': 3, 'FrontText': 10, 'BackText': 10,
             'BlockEntityVersion': 3, 'IsWaxed': 1, 'LockedForEditingBy': 4}
    if (set(root)-types.keys() or not {'id', 'x', 'y', 'z', 'FrontText', 'BackText'} <= root.keys()
            or any(kind != types[name] for name, (kind, _) in root.items())
            or root['id'][1] != 'Sign'
            or tuple(root[axis][1] for axis in 'xyz') != expected):
        raise CodecError('Invalid sign source schema.')
    side_types = {'Text': 8, 'FilteredText': 8, 'TextOwner': 8, 'HideGlowOutline': 1,
                  'IgnoreLighting': 1, 'PersistFormatting': 1, 'SignTextColor': 3}
    for name in ('FrontText', 'BackText'):
        side = root[name][1]
        if (set(side)-side_types.keys() or 'Text' not in side
                or any(kind != side_types[key] for key, (kind, _) in side.items())
                or any(kind == 1 and item not in (0, 1) for kind, item in side.values())):
            raise CodecError('Invalid sign text schema.')


class SignEditors:
    kind = 'sign'

    def __init__(self, backend):
        self.backend = backend

    def state(self, player, session):
        return self.backend.bridge.sign_state(player, *session.context.position, session.generation)

    def validate(self, payload, expected):
        validate_sign_payload(payload, expected)

    def touch(self, player, session, accept=False):
        self.backend.bridge.sign_touch(player, session.generation, accept)

    def reason(self, player, context):
        backend = self.backend
        if backend.settings.get('signs', {}).get('enabled') is not True:
            return 'Native sign editors are disabled in config.toml.'
        if not all(callable(getattr(backend.bridge, method, None)) for method in
                   ('sign_state', 'open_sign', 'sign_touch', 'close_sign', 'shutdown_sign_hooks')):
            return 'The installed native companion lacks the sign editor contract.'
        session = backend.sessions.get(player.unique_id)
        generation = session.generation if session and session.kind == 'sign' and session.context == context else 0
        state = backend.bridge.sign_state(player, *context.position, generation)
        if state['waxed'] or not state['can_interact']:
            return 'This sign is waxed or cannot be edited in the current game mode.'
        if state['occupied'] and not (state['active'] and state['owned']):
            return 'This sign is already being edited.'
        if session and session.sign_started and not state['active']:
            return 'The original native sign source changed.'
        return None

    def activate(self, player, session):
        session.sign_started = True
        self.backend.bridge.open_sign(player, *session.context.position,
                                      getattr(session.context, 'front', True), session.generation)

    def project(self, event, session):
        if event.packet_id != 303 or session.kind != 'sign' or not session.sign_started:
            return False
        reader = Reader(bytes(event.payload))
        position = tuple(reader.svar() for _ in range(3))
        front = reader.boolean()
        reader.end()
        if position != session.context.position:
            return False
        if session.closing_at is not None or front != getattr(session.context, 'front', True):
            event.is_cancelled = True
            session.close_pending = True
        else:
            event.payload = b''.join(svar(v) for v in session.position)+bytes((front,))
        return True

    def observe(self, event, session):
        if event.packet_id != 303 or session.kind != 'sign' or not session.sign_started:
            return
        reader = Reader(bytes(event.payload))
        position = tuple(reader.svar() for _ in range(3))
        front = reader.boolean()
        reader.end()
        if position == session.position and front == getattr(session.context, 'front', True):
            session.screen_open = True

    def receive(self, event, session):
        if event.packet_id != 56:
            return
        payload = bytes(event.payload)
        try:
            reader = Reader(payload)
            position = tuple(reader.svar() for _ in range(3))
        except CodecError:
            if session and session.kind == self.kind:
                event.is_cancelled = True
                session.close_pending = True
            return
        backend = self.backend
        if not session or session.kind != self.kind:
            key = (event.player.unique_id, event.player.dimension.name, position)
            if backend._passive_positions.get(key, 0) > time.monotonic():
                event.is_cancelled = True
            return
        if position != session.position:
            # During this lease only its displayed sign may submit the source.
            # A guessed real-source coordinate never receives remote privileges.
            if position == session.context.position:
                event.is_cancelled = True
            return
        try:
            if (len(payload) > 16384 or not session.screen_open or session.closing_at is not None
                    or session.sign_save_at is not None):
                raise CodecError('The sign editor is not accepting a save.')
            reason = backend.reason(event.player, self.kind, session.context)
            if reason:
                raise CodecError(reason)
            backend._check_guards(event.player, self.kind)
            self.validate(payload, session.position)
            translated = project_actor_data(payload, session.position, session.context.position)
            self.touch(event.player, session, True)
            session.sign_save_at = time.monotonic()
            event.payload = translated
        except Exception:
            event.is_cancelled = True
            session.close_pending = True

    def release(self, player, session):
        if session.sign_started:
            try:
                self.backend.bridge.close_sign(player, session.generation)
            finally:
                session.sign_started = False

    def dismiss(self, player, session):
        self.backend._restore(player, session)

    def close(self, player, session):
        backend = self.backend
        if session.closing_at is None:
            session.closing_at = time.monotonic()
        if not session.close_ping_sent:
            try:
                self.release(player, session)
            except (RuntimeError, ValueError):
                backend.plugin.logger.error('The sign source disappeared during editor cleanup.')
            finally:
                # Removing the projected sign closes this client's real editor.
                # Its final BlockActorData is rejected while closing/retired.
                self.dismiss(player, session)
                session.close_nonce = secrets.randbelow(2**31-1)+1
                session.close_ping_sent = True
                player.send_packet(115, struct.pack('<QB', session.close_nonce, 1))

    def poll(self, player, session, now):
        backend = self.backend
        if session.closing_at is not None:
            if not session.close_ping_sent:
                # BDS can acknowledge a client close before the API calls close.
                # Complete our lease/projection cleanup and issue the ping whose
                # reply this passive-editor state machine actually waits for.
                self.close(player, session)
            if session.close_acknowledged:
                backend.forget(player)
            elif now-session.closing_at > 5:
                player.kick('Sign editor closure was not acknowledged. Rejoin to continue.')
                backend.forget(player, restore=False)
            return
        try:
            state = self.state(player, session)
            if not state['active']:
                raise RuntimeError('The native sign source changed.')
            if session.sign_save_at is not None and state.get('save_complete', not state.get('locked', True)):
                backend.forget(player)
                return
            if not session.screen_open and now-session.opened_at > 5:
                raise RuntimeError('The native sign open was not observed.')
            if session.sign_save_at is not None and now-session.sign_save_at > 8:
                raise RuntimeError('The native sign save did not finish.')
            if player.is_dead or player.dimension.name != session.dimension:
                raise RuntimeError('The player left the sign context.')
            if now-session.opened_at > backend.settings.get('maximum_duration_seconds', 1200):
                raise RuntimeError('The sign editor session expired.')
            if now >= session.next_state_check:
                session.next_state_check = now+0.25
                reason = backend.reason(player, self.kind, session.context)
                if reason:
                    raise RuntimeError(reason)
                backend._check_guards(player, self.kind)
                self.touch(player, session)
        except Exception:
            backend.close(player)
