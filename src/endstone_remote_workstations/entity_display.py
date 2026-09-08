"""Bounded client views for genuine native equipment screens, protocol 2169."""
from dataclasses import dataclass, field
import math
import struct

from .protocol import CodecError, Reader, svar, uvar
from .entities import EQUIPMENT_SCREENS, TRADE_SCREENS, ACTOR_SCREENS
from .riding import decode_link, encode_link, mount_stamp, native_link


def actor_header(payload):
    reader = Reader(payload, limit=262144)
    identity, runtime = reader.svar(64), reader.uvar(64)
    length = reader.uvar()
    if not 1 <= length <= 128:
        raise CodecError('Invalid actor type length.')
    try:
        kind = reader.raw(length).decode('utf-8')
    except UnicodeError as error:
        raise CodecError('Invalid actor type.') from error
    offset = reader.offset
    reader.raw(12)
    return identity, runtime, kind, offset


def equipment_header(payload):
    reader = Reader(payload, limit=262144)
    window, kind, size = reader.byte(), reader.byte(), reader.svar()
    identity = reader.svar(64)
    if not 1 <= window < 100 or kind != 12 or not 0 <= size <= 256:
        raise CodecError('Unexpected equipment screen header.')
    if reader.offset == len(payload):
        raise CodecError('Missing native equipment data.')
    return window, identity


def trade_header(payload):
    reader = Reader(payload, limit=262144)
    window, kind, size, tier = reader.byte(), reader.byte(), reader.svar(), reader.svar()
    identity = reader.svar(64)
    reader.svar(64)  # Native last customer; do not modify merchant state.
    length = reader.uvar()
    if not 1 <= window < 100 or kind != 15 or not 0 <= size <= 256 or not 0 <= tier <= 255 or length > 4096:
        raise CodecError('Unexpected trade screen header.')
    reader.raw(length)
    reader.boolean()
    reader.boolean()
    if reader.offset == len(payload):
        raise CodecError('Missing native trade data.')
    return window, identity


def point(position):
    if len(position) != 3 or any(not math.isfinite(v) or abs(v) > 30000001 for v in position):
        raise CodecError('Invalid entity display position.')
    return struct.pack('<fff', *position)


def delta_position(runtime, position, on_ground):
    # r26_u4: independent coordinate/rotation optionals, then four booleans.
    raw = point(position)
    return (uvar(runtime, 64)+b''.join(b'\x01'+raw[i:i+4] for i in (0, 4, 8))
            + bytes((0, 0, 0, bool(on_ground), 1, 0, 0)))


def project_motion(packet, payload, position):
    reader = Reader(payload)
    reader.uvar(64)
    offset = reader.offset
    raw = point(position)
    if packet == 18:
        header = reader.byte()
        if header & ~15:
            raise CodecError('Unknown absolute movement flags.')
        reader.raw(12)
        reader.raw(3)
        reader.end()
        return payload[:offset]+bytes((header | 2,))+raw+payload[offset+13:]
    if packet != 111:
        raise CodecError('Unknown movement packet.')
    for _ in range(3):
        if reader.boolean():
            reader.raw(4)
    tail = payload[reader.offset:]
    for _ in range(3):
        if reader.boolean():
            reader.raw(1)
    for _ in range(4):
        reader.boolean()
    reader.end()
    return payload[:offset]+b''.join(b'\x01'+raw[i:i+4] for i in (0, 4, 8))+tail[:-3]+b'\x01\x00\x00'


@dataclass
class Visibility:
    dimension: str
    actors: dict = field(default_factory=dict)
    complete: bool = True


class EntityDisplays:
    def __init__(self, backend):
        self.backend = backend
        self.visible = {}
        self.link_sends = {}
        self.limit = 4096
        # Reloading mid-world cannot reconstruct what clients already know.
        self.untracked = {p.unique_id for p in getattr(backend.server, 'online_players', ())}

    def _visibility(self, player):
        state = self.visible.get(player.unique_id)
        if state and state.dimension != player.dimension.name:
            self.visible.pop(player.unique_id, None)
            self.untracked.discard(player.unique_id)
            state = None
        if state is None:
            if len(self.visible) >= self.backend.settings.get('maximum_sessions', 100):
                return None
            state = self.visible[player.unique_id] = Visibility(player.dimension.name,
                complete=player.unique_id not in self.untracked)
        return state

    def reason(self, player):
        state = self._visibility(player)
        if state is None or not state.complete:
            return 'Client entity visibility is incomplete or full; rejoin before opening equipment.'
        return None

    def track(self, player, packet, payload, session):
        sending = self.link_sends.get(player.unique_id)
        if packet == 41 and sending is not None:
            sending.entity_link_seen = payload == sending.entity_link_sending
            return
        if session and session.entity_restoring:
            return
        if packet not in (13, 14):
            return
        state = self._visibility(player)
        if state is None:
            return
        try:
            if packet == 13:
                identity, runtime, kind, offset = actor_header(payload)
                if session and session.projecting_entity and identity == session.entity_id:
                    session.entity_seen = (runtime == session.entity_runtime_id
                        and kind == session.context.actor.type
                        and payload[offset:offset+12] == point(session.entity_position))
                    return
                if len(state.actors) >= self.limit and identity not in state.actors:
                    state.complete = False  # Never forget an ID then add a duplicate.
                else:
                    state.actors[identity] = (runtime, kind)
                if session and session.kind in ACTOR_SCREENS and identity == session.entity_id:
                    session.entity_added = False  # The real world now tracks it.
                    session.close_pending = True
            else:
                reader = Reader(payload)
                identity = reader.svar(64)
                reader.end()
                state.actors.pop(identity, None)
                if session and session.kind in ACTOR_SCREENS and identity == session.entity_id:
                    session.close_pending = True
        except CodecError:
            state.complete = False

    def prepare(self, player, session):
        reason = self.reason(player)
        if reason:
            raise RuntimeError(reason)
        source = session.context.actor
        session.entity_runtime_id = source.runtime_id
        anchor = self.backend._position(player)
        session.entity_position = (anchor[0]+0.5, float(anchor[1]), anchor[2]+0.5)
        known = self._visibility(player).actors.get(session.entity_id)
        expected = (source.runtime_id, source.type)
        if known is not None and known != expected:
            raise RuntimeError('The client entity identity does not match its actual source.')
        session.entity_mount = native_link(self.backend.bridge.entity_mount_link(player, source),
                                           session.entity_id, session.entity_runtime_id)
        session.entity_added = known is None
        if session.entity_added:
            session.projecting_entity = True
            try:
                self.backend.bridge.send_entity_actor(player, source, session.kind)
            finally:
                session.projecting_entity = False
            if not session.entity_seen:
                raise RuntimeError('The native entity display packet was changed or cancelled.')
        if session.entity_mount is not None:
            session.entity_mount_detached = True
            self._send_link(player, session, session.entity_mount, detached=True)
        player.send_packet(111, delta_position(source.runtime_id, session.entity_position, source.is_on_ground))
        session.entity_view_prepared = True

    def _send_link(self, player, session, link, *, detached=False):
        if player.unique_id in self.link_sends:
            raise RuntimeError('A riding view send is already in progress.')
        session.entity_link_sending = encode_link(link, detached=detached)
        session.entity_link_seen = False
        self.link_sends[player.unique_id] = session
        try:
            player.send_packet(41, session.entity_link_sending)
            if not session.entity_link_seen:
                raise RuntimeError('The native riding view packet was changed or cancelled.')
        finally:
            self.link_sends.pop(player.unique_id, None)
            session.entity_link_sending = None

    def mount_matches(self, player, session):
        return mount_stamp(self.backend.bridge.entity_mount_state(player, session.context.actor)) == mount_stamp(session.entity_mount)

    def project(self, event, session):
        if (session.kind not in ACTOR_SCREENS or session.closing_at or session.close_pending
                or session.entity_restoring):
            return
        raw = bytes(event.payload)
        try:
            if event.packet_id == 41 and session.entity_link_sending is None:
                vehicle, passenger, kind, *_ = decode_link(raw)
                if passenger == session.entity_id:
                    if (session.entity_mount is not None and vehicle == session.entity_mount['vehicle_id']
                            and kind == session.entity_mount['type']):
                        # Repeated native announcements must not reattach this
                        # one client while it owns the remote view.
                        event.is_cancelled = True
                    else:
                        # Preserve genuine mount/dismount packets. The next
                        # tick closes the view and restores current native state.
                        session.close_pending = True
            elif event.packet_id == 13 and session.projecting_entity:
                identity, runtime, kind, offset = actor_header(raw)
                if identity == session.entity_id and runtime == session.entity_runtime_id and kind == session.context.actor.type:
                    event.payload = raw[:offset]+point(session.entity_position)+raw[offset+12:]
            elif event.packet_id in (18, 111):
                if Reader(raw).uvar(64) == session.entity_runtime_id:
                    event.payload = project_motion(event.packet_id, raw, session.entity_position)
        except CodecError:
            session.close_pending = True

    def restore(self, player, session):
        session.entity_restoring = True
        try:
            self._restore(player, session)
        finally:
            session.entity_restoring = False

    def _restore(self, player, session):
        if player.dimension.name != session.dimension:
            return
        if session.entity_added:
            player.send_packet(14, svar(session.entity_id, 64))
        elif session.entity_runtime_id is not None:
            source = session.context.actor
            known = self._visibility(player)
            if (source.is_valid and source.id == session.entity_id and source.runtime_id == session.entity_runtime_id
                    and source.dimension.name == player.dimension.name and known
                    and known.actors.get(session.entity_id) == (source.runtime_id, source.type)):
                loc = source.location
                player.send_packet(111, delta_position(source.runtime_id, (loc.x, loc.y, loc.z), source.is_on_ground))
                if session.entity_mount_detached:
                    current = native_link(self.backend.bridge.entity_mount_link(player, source),
                                          session.entity_id, session.entity_runtime_id)
                    if current is not None:
                        vehicle = known.actors.get(current['vehicle_id'])
                        if vehicle is not None and vehicle[0] == current['vehicle_runtime_id']:
                            self._send_link(player, session, current)

    def disconnect(self, identity):
        self.visible.pop(identity, None)
        self.untracked.discard(identity)
