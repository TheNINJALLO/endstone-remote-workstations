"""Real JigsawBlock metadata editor with native save ownership and validation."""
import re

from .protocol import CodecError, Reader, close_payload
from .signs import SignEditors


def validate_jigsaw_payload(payload, expected):
    if len(payload) > 4096:
        raise CodecError('Jigsaw update exceeds its limit.')
    reader = Reader(payload)
    if tuple(reader.svar() for _ in range(3)) != expected:
        raise CodecError('Jigsaw position does not match this editor.')

    def string(limit):
        size = reader.uvar()
        if size > limit:
            raise CodecError('Jigsaw string exceeds its limit.')
        try:
            value = reader.raw(size).decode('utf-8', errors='strict')
        except UnicodeError as error:
            raise CodecError('Invalid jigsaw text encoding.') from error
        if any(char in value for char in ('\0', '\r', '\n')):
            raise CodecError('Invalid jigsaw text.')
        return value

    if reader.byte() != 10 or string(32):
        raise CodecError('Expected an unnamed jigsaw compound.')
    types = {'BlockEntityVersion': 3, 'id': 8, 'x': 3, 'y': 3, 'z': 3,
             'name': 8, 'target': 8, 'target_pool': 8, 'final_state': 8, 'joint': 8,
             'placement_priority': 3, 'selection_priority': 3}
    fields = {}
    while True:
        kind = reader.byte()
        if kind == 0:
            break
        name = string(32)
        if name in fields or name not in types or types[name] != kind:
            raise CodecError('Unexpected jigsaw field.')
        fields[name] = reader.svar() if kind == 3 else string(1024 if name == 'final_state' else 256)
    reader.end()
    if (not types.keys()-{'BlockEntityVersion'} <= fields.keys() or fields['id'] != 'JigsawBlock'
            or tuple(fields[axis] for axis in 'xyz') != expected
            or fields['joint'] not in ('aligned', 'rollable')):
        raise CodecError('Invalid jigsaw source schema.')
    identifier = r'[a-z0-9_.-]+:[a-z0-9_./-]+'
    if any(not re.fullmatch(identifier, fields[name]) for name in ('name', 'target', 'target_pool')):
        raise CodecError('Invalid jigsaw resource identifier.')
    if not re.fullmatch(identifier+r'(?:\[[^\[\]]*\])?', fields['final_state']):
        raise CodecError('Invalid final block state.')


class JigsawEditors(SignEditors):
    kind = 'jigsaw'

    def reason(self, player, context):
        backend = self.backend
        if backend.settings.get('jigsaw', {}).get('enabled') is not True:
            return 'Native jigsaw editors are disabled in config.toml.'
        from endstone import GameMode
        if not player.is_op or player.game_mode != GameMode.CREATIVE:
            return 'Jigsaw editing requires a Creative operator.'
        if not player.has_permission('remoteworkstations.admin'):
            return 'Jigsaw editing requires administrator permission.'
        if not all(callable(getattr(backend.bridge, method, None)) for method in
                   ('jigsaw_state', 'open_jigsaw', 'sign_touch', 'close_sign', 'shutdown_sign_hooks')):
            return 'The installed native companion lacks the jigsaw editor contract.'
        session = backend.sessions.get(player.unique_id)
        generation = session.generation if session and session.kind == self.kind and session.context == context else 0
        state = backend.bridge.jigsaw_state(player, *context.position, generation)
        if not state['can_interact']:
            return 'The native operator-block permission does not allow this edit.'
        if session and session.kind == self.kind and session.sign_started and not state['active']:
            return 'The original jigsaw source changed.'
        return None

    def state(self, player, session):
        return self.backend.bridge.jigsaw_state(player, *session.context.position, session.generation)

    def validate(self, payload, expected):
        validate_jigsaw_payload(payload, expected)

    def activate(self, player, session):
        session.sign_started = True
        window = self.backend.bridge.open_jigsaw(player, *session.context.position, session.generation)
        session.screen_open = session.window is not None
        return window

    def dismiss(self, player, session):
        try:
            if session.window is not None:
                player.send_packet(47, close_payload(session.window, 247, True))
        finally:
            self.backend._restore(player, session)
