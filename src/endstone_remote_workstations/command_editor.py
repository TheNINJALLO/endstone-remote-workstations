"""Guarded real command-block editor. BDS owns filtering and metadata application."""
import struct
import time

from .jigsaw import JigsawEditors
from .editor_input import GameplayCloseObserver
from .protocol import CodecError, Reader, svar


def command_target(payload):
    reader = Reader(payload)
    target = reader.uvar()
    if target == 0:
        return None, reader
    if target != 1:
        raise CodecError('Invalid command target.')
    return tuple(reader.svar() for _ in range(3)), reader


def translate_command(payload, expected, destination):
    position, reader = command_target(payload)
    if position != expected:
        raise CodecError('Command position does not match this editor.')
    tail = reader.offset
    if reader.uvar() > 2:
        raise CodecError('Invalid command-block mode.')
    reader.boolean()
    reader.boolean()
    validate_command_fields(reader)
    return b'\x01'+b''.join(svar(v) for v in destination)+payload[tail:]


def validate_command_fields(reader, maximum_delay=2147483647):
    for limit in (32768, 16384, 256, 256):
        size = reader.uvar()
        if size > limit:
            raise CodecError('Command editor string exceeds its limit.')
        try:
            value = reader.raw(size).decode('utf-8', errors='strict')
        except UnicodeError as error:
            raise CodecError('Invalid command text encoding.') from error
        if '\0' in value:
            raise CodecError('Invalid command editor text.')
    reader.boolean()
    if not 0 <= struct.unpack('<i', reader.raw(4))[0] <= maximum_delay:
        raise CodecError('Command tick delay is outside its native range.')
    reader.boolean()
    reader.end()


def command_entity_target(payload):
    reader = Reader(payload)
    variant = reader.uvar()
    if variant == 1:
        return None, reader
    if variant != 0:
        raise CodecError('Invalid command target.')
    runtime = reader.uvar(64)
    if not runtime:
        raise CodecError('Invalid command entity runtime ID.')
    return runtime, reader


def validate_command_entity(payload, expected):
    runtime, reader = command_entity_target(payload)
    if runtime != expected:
        raise CodecError('Command entity does not match this editor.')
    validate_command_fields(reader, 99999)  # Actual native component clamps at 99,999.


class CommandEditors(GameplayCloseObserver, JigsawEditors):
    kind = 'commandblock'

    def reason(self, player, context):
        backend = self.backend
        if backend.settings.get('commandblock', {}).get('enabled') is not True:
            return 'Native command-block editors are disabled in config.toml.'
        from endstone import GameMode
        if not player.is_op or player.game_mode != GameMode.CREATIVE:
            return 'Command-block editing requires a Creative operator.'
        if not player.has_permission('remoteworkstations.admin'):
            return 'Command-block editing requires administrator permission.'
        if not all(callable(getattr(backend.bridge, method, None)) for method in
                   ('command_state', 'open_command', 'command_touch', 'close_command',
                    'abandon_command', 'shutdown_command_hooks')):
            return 'The installed native companion lacks the command editor contract.'
        session = backend.sessions.get(player.unique_id)
        generation = session.generation if session and session.kind == self.kind and session.context == context else 0
        state = backend.bridge.command_state(player, *context.position, generation)
        if not state['can_interact']:
            return 'The native operator-block permission does not allow this edit.'
        if session and session.kind == self.kind and session.sign_started and not state['active']:
            return 'The original command-block source changed.'
        return None

    def state(self, player, session):
        return self.backend.bridge.command_state(player, *session.context.position, session.generation)

    def touch(self, player, session, accept=False):
        self.backend.bridge.command_touch(player, session.generation, accept)

    def activate(self, player, session):
        session.sign_started = True
        window = self.backend.bridge.open_command(player, *session.context.position, session.generation,
                                                   session.context.permission or 'remoteworkstations.contexts')
        session.screen_open = session.window is not None
        return window

    def release(self, player, session):
        if session.sign_started:
            try:
                self.backend.bridge.close_command(player, session.generation)
            finally:
                session.sign_started = False


    def receive(self, event, session):
        if event.packet_id != 78:
            return
        backend = self.backend
        payload = bytes(event.payload)
        try:
            position, _ = command_target(payload)
        except CodecError:
            if session and session.kind == self.kind:
                event.is_cancelled = True
                session.close_pending = True
            return
        if position is None:
            return  # Entity command editors have their own native branch.
        if not session or session.kind != self.kind:
            key = (event.player.unique_id, event.player.dimension.name, position)
            if backend._passive_positions.get(key, 0) > time.monotonic():
                event.is_cancelled = True
            return
        if position != session.position:
            if position == session.context.position:
                event.is_cancelled = True
            return
        try:
            if not session.screen_open or session.closing_at is not None or session.sign_save_at is not None:
                raise CodecError('The command editor is not accepting a save.')
            reason = backend.reason(event.player, self.kind, session.context)
            if reason:
                raise CodecError(reason)
            backend._check_guards(event.player, self.kind)
            translated = translate_command(payload, session.position, session.context.position)
            self.touch(event.player, session, True)
            session.sign_save_at = time.monotonic()
            event.payload = translated
        except Exception:
            event.is_cancelled = True
            session.close_pending = True


class CommandCartEditors(CommandEditors):
    kind = 'commandblockminecart'

    def reason(self, player, context):
        backend = self.backend
        if backend.settings.get(self.kind, {}).get('enabled') is not True:
            return 'Native command-minecart editors are disabled in config.toml.'
        from endstone import GameMode
        if not player.is_op or player.game_mode != GameMode.CREATIVE:
            return 'Command-minecart editing requires a Creative operator.'
        if not player.has_permission('remoteworkstations.admin'):
            return 'Command-minecart editing requires administrator permission.'
        if not all(callable(getattr(backend.bridge, method, None)) for method in
                   ('command_entity_state', 'open_command_entity', 'command_touch', 'close_command',
                    'abandon_command', 'shutdown_command_hooks')):
            return 'The installed native companion lacks the command entity editor contract.'
        session = backend.sessions.get(player.unique_id)
        generation = session.generation if session and session.kind == self.kind and session.context == context else 0
        state = backend.bridge.command_entity_state(player, context.actor, generation)
        if not state['can_interact']:
            return 'The native operator permission does not allow this edit.'
        if state['actor_id'] != context.actor.id or state['runtime_id'] != context.actor.runtime_id:
            return 'The original command-minecart identity changed.'
        if session and session.kind == self.kind and session.sign_started and (
                not state['active'] or session.entity_id != context.actor.id
                or session.entity_runtime_id != context.actor.runtime_id):
            return 'The original command-minecart source changed.'
        return None

    def state(self, player, session):
        return self.backend.bridge.command_entity_state(player, session.context.actor, session.generation)

    def activate(self, player, session):
        if session.context.actor.runtime_id != session.entity_runtime_id:
            raise RuntimeError('The command-minecart runtime identity changed while opening.')
        session.sign_started = True
        window = self.backend.bridge.open_command_entity(player, session.context.actor, session.generation,
                                                          session.context.permission or 'remoteworkstations.contexts')
        session.screen_open = session.window is not None
        return window

    def receive(self, event, session):
        if event.packet_id != 78:
            return
        backend = self.backend
        payload = bytes(event.payload)
        try:
            runtime, _ = command_entity_target(payload)
        except CodecError:
            if session and session.kind == self.kind:
                event.is_cancelled = True
                session.close_pending = True
            return
        if runtime is None:
            return
        if not session or session.kind != self.kind:
            key = (event.player.unique_id, event.player.dimension.name, runtime)
            if backend._command_entities.get(key, 0) > time.monotonic():
                event.is_cancelled = True
            return
        if runtime != session.entity_runtime_id:
            return
        try:
            if not session.screen_open or session.closing_at is not None or session.sign_save_at is not None:
                raise CodecError('The command editor is not accepting a save.')
            reason = backend.reason(event.player, self.kind, session.context)
            if reason:
                raise CodecError(reason)
            backend._check_guards(event.player, self.kind)
            validate_command_entity(payload, session.entity_runtime_id)
            self.touch(event.player, session, True)
            session.sign_save_at = time.monotonic()
        except Exception:
            event.is_cancelled = True
            session.close_pending = True
