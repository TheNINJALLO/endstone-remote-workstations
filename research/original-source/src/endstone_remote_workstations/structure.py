"""Real structure editor controls; BDS owns template storage and world changes."""
import math
import re
import struct
import time

from .jigsaw import JigsawEditors
from .editor_input import GameplayCloseObserver
from .protocol import CodecError, Reader, svar


def string(reader, limit=256):
    size = reader.uvar()
    if size > limit:
        raise CodecError('Structure text exceeds its limit.')
    try:
        value = reader.raw(size).decode('utf-8', errors='strict')
    except UnicodeError as error:
        raise CodecError('Invalid structure text encoding.') from error
    if any(ord(c) < 32 for c in value):
        raise CodecError('Invalid structure text.')
    return value


def structure_name(value):
    # Native template names, never OS paths. BDS retains its namespace and
    # storage rules. Empty names occur in ordinary initial metadata updates.
    if value and (not re.fullmatch(r'(?:[a-zA-Z0-9_.-]+:)?[a-zA-Z0-9_./-]+', value)
                  or any(part in ('', '.', '..') for part in value.split(':')[-1].split('/'))):
        raise CodecError('Invalid native structure name.')
    return value


def settings(reader, destination, maximum_volume, offset_delta=(0, 0, 0)):
    palette = string(reader)
    ignore_entities, ignore_blocks, allow_non_ticking = (reader.boolean() for _ in range(3))
    size = tuple(reader.svar() for _ in range(3))
    offset_start = reader.offset
    offset = tuple(reader.svar()+delta for delta in offset_delta)
    offset_end = reader.offset
    # Check the translated values before serializing them back to native i32.
    for value in offset:
        svar(value)
    reader.svar(64)  # Native last-edit ActorUniqueID metadata, never an access grant.
    rotation, mirror, animation = (reader.byte() for _ in range(3))
    seconds, integrity = struct.unpack('<ff', reader.raw(8))
    reader.raw(4)  # Integrity seed.
    pivot = struct.unpack('<fff', reader.raw(12))
    if (any(not 0 <= value <= limit for value, limit in zip(size, (64, 384, 64)))
            or math.prod(max(1, v) for v in size) > maximum_volume
            or rotation > 3 or mirror > 3 or animation > 2
            or not math.isfinite(seconds) or not 0 <= seconds <= 3600
            or not math.isfinite(integrity) or not 0 <= integrity <= 100
            or any(not math.isfinite(v) or abs(v) > 384 for v in pivot)):
        raise CodecError('Structure settings exceed the admitted native bounds.')
    for i, (origin, delta, width) in enumerate(zip(destination, offset, size)):
        if i == 1:
            if abs(delta) > 384:
                raise CodecError('Structure vertical offset exceeds world height.')
            # The original native apply clamps Y to this dimension height.
        elif not -30000000 <= origin+delta <= origin+delta+max(1, width)-1 <= 30000000:
            raise CodecError('Structure volume exceeds world bounds.')
    return dict(size=size, offset=offset, ignore_entities=ignore_entities,
                ignore_blocks=ignore_blocks, allow_non_ticking=allow_non_ticking, palette=palette,
                offset_bytes=(offset_start, offset_end))


def structure_target(payload, packet_id):
    reader = Reader(payload)
    if packet_id == 132:
        structure_name(string(reader))
    elif packet_id != 90:
        raise CodecError('Unexpected structure control.')
    start = reader.offset
    position = tuple(reader.svar() for _ in range(3))
    return position, start, reader


def translate_structure(payload, packet_id, expected, destination, maximum_volume=262144, *, offset_delta=(0, 0, 0)):
    position, start, reader = structure_target(payload, packet_id)
    if position != expected:
        raise CodecError('Structure position does not match this editor.')
    tail = reader.offset
    if packet_id == 90:
        structure_name(string(reader))
        if reader.boolean():  # RedactableString optional filtered value.
            structure_name(string(reader))
        string(reader, 1024)
        reader.boolean()  # Include players.
        reader.boolean()  # Bounding box.
        mode = reader.svar()
        if mode not in (0, 1, 2, 3, 5):
            raise CodecError('Invalid structure editor mode.')
    result = settings(reader, destination, maximum_volume, offset_delta)
    if packet_id == 90:
        if reader.byte() > 1:
            raise CodecError('Invalid structure save destination.')
        result.update(mode=mode, trigger=reader.boolean(), waterlogged=reader.boolean())
    else:
        operation = reader.byte()
        if operation not in (1, 2, 3):
            raise CodecError('Invalid structure template operation.')
        result['operation'] = operation
    reader.end()
    offset_start, offset_end = result.pop('offset_bytes')
    rewritten = (payload[:start]+b''.join(svar(v) for v in destination)+payload[tail:offset_start]
                 +b''.join(svar(v) for v in result['offset'])+payload[offset_end:])
    return rewritten, result


def project_structure_actor(payload, source, display):
    """Preserve the selected world volume in the actual client renderer.

    The visible editor uses offsets from its displayed block. Its selection
    therefore needs source+offset-display. Incoming controls apply the inverse.
    Full template/export NBT is never modified by this presentation mapping.
    """
    from rapidnbt import CompoundTag
    reader = Reader(payload)
    if tuple(reader.svar() for _ in range(3)) != source:
        raise CodecError('Structure block actor source does not match its context.')
    tag = CompoundTag.from_network_nbt(payload[reader.offset:])
    if tag is None or any(axis+'StructureOffset' not in tag for axis in 'xyz'):
        raise CodecError('Native structure selection offsets are missing.')
    for axis, origin, anchor in zip('xyz', source, display):
        key = axis+'StructureOffset'
        value = int(tag[key])+origin-anchor
        svar(value)
        tag.set(key, value)
        if axis in tag:
            tag.set(axis, anchor)
    return b''.join(svar(v) for v in display)+tag.to_network_nbt()


class StructureEditors(GameplayCloseObserver, JigsawEditors):
    kind = 'structure'

    def reason(self, player, context):
        backend = self.backend
        if backend.settings.get('structure', {}).get('enabled') is not True:
            return 'Native structure editors are disabled in config.toml.'
        from endstone import GameMode
        if not player.is_op or player.game_mode != GameMode.CREATIVE:
            return 'Structure editing requires a Creative operator.'
        if not player.has_permission('remoteworkstations.admin'):
            return 'Structure editing requires administrator permission.'
        if not all(callable(getattr(backend.bridge, name, None)) for name in
                   ('structure_state', 'open_structure', 'structure_touch', 'close_structure',
                    'abandon_structure', 'shutdown_structure_hooks')):
            return 'The installed native companion lacks the structure editor contract.'
        session = backend.sessions.get(player.unique_id)
        generation = session.generation if session and session.kind == self.kind and session.context == context else 0
        state = backend.bridge.structure_state(player, *context.position, generation)
        if not state['can_interact']:
            return 'The native operator-block permission does not allow this edit.'
        if session and session.kind == self.kind and session.sign_started and not state['active']:
            return 'The original structure source changed.'
        return None

    def state(self, player, session):
        return self.backend.bridge.structure_state(player, *session.context.position, session.generation)

    def touch(self, player, session, accept=False):
        self.backend.bridge.structure_touch(player, session.generation, accept)

    def activate(self, player, session):
        session.sign_started = True
        window = self.backend.bridge.open_structure(player, *session.context.position, session.generation,
                                                     session.context.permission or 'remoteworkstations.contexts')
        session.screen_open = session.window is not None
        return window

    def release(self, player, session):
        if session.sign_started:
            try:
                self.backend.bridge.close_structure(player, session.generation)
            finally:
                session.sign_started = False

    def receive(self, event, session):
        if event.packet_id not in (90, 132):
            return
        backend = self.backend
        payload = bytes(event.payload)
        try:
            position, _, _ = structure_target(payload, event.packet_id)
        except CodecError:
            if session and session.kind == self.kind:
                event.is_cancelled = True
                session.close_pending = True
            return
        if not session or session.kind != self.kind:
            if backend._passive_positions.get((event.player.unique_id, event.player.dimension.name, position), 0) > time.monotonic():
                event.is_cancelled = True
            return
        if position != session.position:
            if position == session.context.position:
                event.is_cancelled = True
            return
        try:
            if not session.screen_open or session.closing_at is not None:
                raise CodecError('This structure editor is closing.')
            reason = backend.reason(event.player, self.kind, session.context)
            if reason:
                raise CodecError(reason)
            backend._check_guards(event.player, self.kind)
            options = backend.settings.get('structure', {})
            maximum = options.get('maximum_volume', 262144)
            if type(maximum) is not int or not 1 <= maximum <= 64*384*64:
                raise CodecError('Invalid structure volume policy.')
            translated, _ = translate_structure(payload, event.packet_id, position, session.context.position, maximum,
                offset_delta=tuple(display-source for display, source in zip(session.position, session.context.position)))
            if self.state(event.player, session).get('pending'):
                # Never reorder deferred Save/Load triggers or silently discard
                # a previous accepted edit in favour of newer text.
                raise CodecError('The previous native structure update is still pending.')
            self.touch(event.player, session, event.packet_id == 90)
            if event.packet_id == 90:
                session.sign_save_at = time.monotonic()
            event.payload = translated
        except Exception:
            event.is_cancelled = True
            session.close_pending = True

    def poll(self, player, session, now):
        # Unlike signs/commands, vanilla structures remain open after Save and
        # also submit metadata when fields change. Each native completion ends
        # that update, while the editor lease stays open for the next one.
        if session.closing_at is None and session.sign_save_at is not None:
            try:
                if self.state(player, session).get('save_complete'):
                    session.sign_save_at = None
            except Exception:
                self.backend.close(player)
        super().poll(player, session, now)
