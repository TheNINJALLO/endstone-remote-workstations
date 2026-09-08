"""Owned packet inventory screens; callbacks and writes run only from poll()."""
from collections import OrderedDict, deque
from dataclasses import dataclass, field
import secrets
import struct
import time
import uuid

from .api import UIError
from .model import Rejected
from .native_backend import block_update, CLIENT_TRANSITION_SECONDS
from .packet_codec import Container, Response, ResponseContainer, ResponseSlot, encode_responses
from .packet_inventory import ReservedInventory, Conflict
from .packet_items import ItemBinding, read_content, read_slot, registry
from .protocol import (CodecError, Open, StorageAdapter, ItemDescriptor, decode_storage_requests,
                       close_payload, inventory_content, inventory_slot, svar, uvar)


@dataclass
class PacketSession:
    player_id: object
    generation: str
    window: int
    dimension: str
    position: tuple
    title: str
    binding: object
    engine: object
    buttons: dict
    created: float
    nonce: int
    stage: int = 0
    acknowledged: bool = False
    opened: bool = False
    reason: str | None = None
    selection: str | None = None
    closed: bool = False
    close_acknowledged: bool = False
    queue: deque = field(default_factory=deque)


class PacketBackend:
    def __init__(self, plugin, settings):
        self.plugin, self.server, self.native = plugin, plugin.server, plugin._native
        self.settings = settings
        self.sessions = OrderedDict()
        self.retiring = OrderedDict()
        self.registries, self.actual, self.allocators, self.windows = {}, {}, {}, {}
        self.registry_queue = OrderedDict()
        self.sending = False
        self.closed = False
        self.native.add_guard('remote_workstations.packet_inventory', self._native_guard)

    def _native_guard(self, player, kind):
        if player.unique_id in self.sessions or any(key[0] == player.unique_id for key in self.retiring):
            return False
        # Exact-build window allocation increments and wraps at 100 to 1.
        # Do not reuse a packet close lease for the next native window.
        if self.native.bridge is not None:
            current = self.native.bridge.state(player).get('window')
            if isinstance(current, int):
                upcoming = current + 1 if current < 99 else 1
                if self.windows.get(player.unique_id, {}).get(upcoming, 0) > time.monotonic():
                    return False
        return True

    def reason(self, player, *, ender=True):
        if not self.settings.get('enabled', False) or self.closed:
            return 'Packet inventory screens are disabled in config.toml.'
        if self.native.unavailable or self.native.closed:
            return self.native.unavailable or 'The native compatibility guard is stopping.'
        if not hasattr(self.native.bridge, 'refresh_inventory'):
            return 'The native companion lacks the verified inventory refresh function.'
        if player.game_version != '1.26.45' or str(player.device_os) != 'Windows':
            return 'Packet screens currently admit Windows Bedrock 1.26.45.'
        if not player.is_valid or player.is_dead:
            return 'A living, connected player is required.'
        if player.dimension.name in self.native.settings.get('denied_dimensions', []):
            return 'Remote interfaces are disabled in this dimension.'
        if not player.has_permission('remoteworkstations.use') or (ender and not
                player.has_permission('remoteworkstations.open.enderchest')):
            return 'You do not have permission to open this interface.'
        if player.unique_id not in self.registries or player.unique_id not in self.actual:
            return 'Reconnect to obtain the server item registry and inventory snapshot.'
        return None

    def send(self, player, packet, payload):
        self.sending = True
        try:
            player.send_packet(packet, payload)
        finally:
            self.sending = False

    def open(self, player, title='Ender Chest', icons=None, buttons=None):
        self.native._owner()
        reason = self.reason(player, ender=icons is None)
        if reason:
            raise UIError(reason)
        identity = player.unique_id
        if identity in self.sessions or identity in self.native.sessions or identity in self.native.pending:
            raise UIError('Close the current interface first.')
        if any(key[0] == identity for key in self.retiring):
            raise UIError('The client is still acknowledging the previous close.')
        if not self.native.bridge.state(player)['ready']:
            raise UIError('Close the current native inventory first.')
        # Respect external protection guards; the developer service owns this
        # queued request and therefore its own menu guard is intentionally skipped.
        for name, guard in self.native.guards.items():
            if name != 'remote_workstations.developer_menus' and guard(player, 'enderchest' if icons is None else 'inventory_menu') is not True:
                raise UIError('A protection guard refused this interface.')
        if len(self.sessions) >= min(100, self.settings.get('maximum_sessions', 100)):
            raise UIError('Packet session limit reached.')
        windows = self.windows.setdefault(identity, {})
        now = time.monotonic()
        window = next((w for w in (*range(70, 100), *range(20, 70))
                       if windows.get(w, 0) <= now), None)
        if window is None:
            raise UIError('Too many rapid screen changes; retry after one minute.')
        generation = uuid.uuid4().hex
        binding = ItemBinding(player, generation, self.allocators.get(identity, 1000000), self.registries[identity], icons)
        # Serialize every template before displaying or taking transaction ownership.
        descriptor_bytes = sum(len(binding.descriptor(item).encode())
                               for item in (*binding.initial.player, *binding.initial.storage))
        if descriptor_bytes > 65000:
            raise UIError('Inventory metadata exceeds the bounded packet screen size.')
        engine = ReservedInventory(binding.initial, binding.read, binding.commit)
        session = PacketSession(identity, generation, window, player.dimension.name,
            self.native._position(player), title, binding, engine, buttons or {}, time.monotonic(),
            secrets.randbelow(2**31-1)+1)
        windows[window] = float('inf')
        self.sessions[identity] = session
        return session

    def receive(self, event):
        if self.sending or event.player is None or event.is_cancelled:
            return
        payload = bytes(event.payload)
        # A forced-close echo can arrive after a callback opened a native screen.
        # BDS would close its CURRENT window for that old packet-view echo.
        if event.packet_id == 47 and payload:
            deadline = self.windows.get(event.player.unique_id, {}).get(payload[0], 0)
            current = self.sessions.get(event.player.unique_id)
            if deadline > time.monotonic() and (current is None or current.window != payload[0]):
                event.is_cancelled = True
                retired = self.retiring.get((event.player.unique_id, payload[0]))
                if retired:
                    retired[0].close_acknowledged = True
                return
        session = self.sessions.get(event.player.unique_id)
        if session is None or session.reason == 'superseded':
            return
        if (event.packet_id == 115 and len(payload) == 9
                and struct.unpack('<QB', payload) == (session.nonce * 1000000, 1)):
            session.acknowledged = True
        elif event.packet_id == 47 and payload and payload[0] == session.window:
            event.is_cancelled = True
            session.reason = 'dismissed'
        elif event.packet_id == 147:
            event.is_cancelled = True
            if len(payload) > 65536 or len(session.queue) >= 32:
                session.reason = 'request_limit'
            elif session.opened and session.reason is None:
                session.queue.append(payload)
            else:
                session.reason = 'request_before_ready'

    def outgoing(self, event):
        if self.sending or event.player is None or event.is_cancelled:
            return
        identity = event.player.unique_id
        payload = bytes(event.payload)
        if event.packet_id == 162:
            if len(payload) <= 8 * 1024 * 1024 and len(self.registry_queue) < 100:
                self.registry_queue[identity] = payload
            return
        session = self.sessions.get(identity)
        if session and event.packet_id in (46, 100):
            session.reason = 'superseded'
        try:
            if event.packet_id == 49:
                window, content = read_content(payload)
                if window == 0 and len(content) == 36:
                    self.actual[identity] = list(content)
                    if session:
                        event.is_cancelled = True
            elif event.packet_id == 50:
                window, slot, item = read_slot(payload)
                if window == 0 and 0 <= slot < 36 and identity in self.actual:
                    self.actual[identity][slot] = item
                    if session:
                        event.is_cancelled = True
        except (ValueError, IndexError):
            if session:
                session.reason = 'invalid_server_inventory_packet'

    def _content(self, player, session):
        state, binding = session.engine.state, session.binding
        self.send(player, 49, inventory_content(0, [binding.descriptor(i) for i in state.player], role=12))
        self.send(player, 49, inventory_content(session.window, [binding.descriptor(i) for i in state.storage]))
        self.send(player, 50, inventory_slot(124, 0, binding.descriptor(state.cursor[0]), role=59))

    def _responses(self, session, wire, state):
        groups = OrderedDict()
        adapter = StorageAdapter('chest')
        for action in wire.actions:
            for ref in (action.source, action.destination):
                if ref is None:
                    continue
                slot = adapter.ref(ref).slot
                item = getattr(state, slot.area)[slot.index]
                slots = groups.setdefault(ref.role, {})
                slots[ref.slot] = ResponseSlot(ref.slot, ref.slot, item.count if item else 0,
                    item.net_id if item else None, session.binding.name(item))
        return Response(0, wire.request_id, [ResponseContainer(Container(role), list(slots.values()))
                                             for role, slots in groups.items()])

    def _requests(self, player, session, payload):
        requests = decode_storage_requests(payload)
        responses = []
        adapter = StorageAdapter('chest')
        for wire in requests:
            try:
                if session.reason:
                    raise Rejected('Session is closing.')
                request = adapter.request(wire, session.engine.state)
                if session.binding.icons is not None:
                    session.engine.check()
                    # A menu click may refer only to the selected icon and an
                    # empty cursor; reject every transfer before dispatching it.
                    if len(request.actions) != 1:
                        raise Rejected('A menu selection requires one action.')
                    action = request.actions[0]
                    ref = action.source
                    if ref.slot.area != 'storage' or action.kind not in ('take', 'place'):
                        raise Rejected('Select a menu icon.')
                    item = session.engine.state.storage[ref.slot.index]
                    if (item is None or item.net_id != ref.net_id or ref.slot.index not in session.buttons
                            or not 1 <= action.count <= item.count):
                        raise Rejected('Unknown menu icon identity.')
                    if action.destination is None or action.destination.slot.area != 'cursor' or action.destination.net_id != 0:
                        raise Rejected('Unsupported menu gesture.')
                    session.selection = session.buttons[ref.slot.index]
                    session.reason = 'selected'
                    raise Rejected('Menu icons cannot leave the interface.')
                plan = session.engine.apply(request)
                responses.append(self._responses(session, wire, plan.after))
            except Conflict as error:
                session.reason = str(error)
                responses.append(Response(1, wire.request_id))
            except Rejected:
                responses.append(Response(1, wire.request_id))
        self.send(player, 148, encode_responses(responses))
        self._content(player, session)

    def _tick(self, player, session):
        if session.reason:
            self.close(player, session.reason)
            return
        if not player.is_valid or player.is_dead or player.dimension.name != session.dimension:
            self.close(player, 'player_lifecycle', restore=player.is_valid)
            return
        reason = self.reason(player, ender=session.binding.icons is None)
        if reason:
            self.close(player, reason)
            return
        elapsed = time.monotonic() - session.created
        if elapsed > (300 if session.opened else 10):
            self.close(player, 'timeout')
            return
        if session.stage == 0 and elapsed >= CLIENT_TRANSITION_SECONDS:
            block = self.server.create_block_data('minecraft:chest')
            self.send(player, 21, block_update(session.position, block.runtime_id))
            session.stage = 1
            self.send(player, 115, struct.pack('<QB', session.nonce, 1))
        elif session.stage == 1 and session.acknowledged:
            from bstream import BinaryStream
            from rapidnbt import CompoundTag
            tag = CompoundTag()
            tag.set('id', 'Chest')
            tag.set('CustomName', session.title)
            stream = BinaryStream()
            tag.serialize(stream)
            self.send(player, 56, b''.join(svar(v) for v in session.position) + stream.copy_buffer())
            session.stage, session.acknowledged = 2, False
            session.nonce += 1
            self.send(player, 115, struct.pack('<QB', session.nonce, 1))
        elif session.stage == 2 and session.acknowledged:
            self.send(player, 46, Open(session.window, 0, *session.position).encode())
            session.stage, session.acknowledged = 3, False
            session.nonce += 1
            self.send(player, 115, struct.pack('<QB', session.nonce, 1))
        elif session.stage == 3 and session.acknowledged:
            session.engine.check()
            self._content(player, session)
            session.stage, session.opened = 4, True
            self.plugin.logger.info(f'RWPACKET open {session.generation} window={session.window} real_ender={session.binding.icons is None}')
        elif session.opened:
            session.engine.check()
            for _ in range(min(4, len(session.queue))):
                self._requests(player, session, session.queue.popleft())

    def poll(self):
        self.native._owner()
        for key, (session, deadline) in tuple(self.retiring.items()):
            if session.close_acknowledged:
                player = self.server.get_player(session.player_id)
                if player and player.is_valid:
                    # BDS also confirms the client's reply to a server-initiated
                    # close. Without this final acknowledgement the old client
                    # screen can swallow the next native opening.
                    self.send(player, 47, close_payload(session.window, 247, False))
                else:
                    session.selection = None
                session.closed = True
                self.retiring.pop(key, None)
            elif time.monotonic() >= deadline:
                session.closed, session.reason, session.selection = True, 'close_ack_timeout', None
                self.retiring.pop(key, None)
        if self.registry_queue:
            identity, payload = self.registry_queue.popitem(last=False)
            try:
                self.registries[identity] = registry(payload)
            except Exception as error:
                self.plugin.logger.error(f'Packet registry refused: {type(error).__name__}: {error}')
        for identity in tuple(self.sessions)[:16]:
            session = self.sessions.get(identity)
            if session is None:
                continue
            self.sessions.move_to_end(identity)
            player = self.server.get_player(identity)
            if player is None:
                session.engine.close()
                session.closed, session.reason = True, 'disconnected'
                self.sessions.pop(identity, None)
                continue
            try:
                self._tick(player, session)
            except Exception as error:
                self.plugin.logger.error(f'Packet session closed: {type(error).__name__}: {error}')
                self.close(player, str(error))

    def close(self, player, reason='cancelled', *, restore=True):
        session = self.sessions.pop(player.unique_id, None)
        if session is None:
            return
        self.allocators[player.unique_id] = max(session.engine.state.next_net_id, session.binding.next_id)
        self.windows.setdefault(player.unique_id, {})[session.window] = time.monotonic() + 60
        session.engine.close()
        needs_ack = restore and player.is_valid and session.stage >= 3 and reason not in ('dismissed', 'superseded')
        session.closed, session.reason = not needs_ack, reason
        if needs_ack:
            self.retiring[player.unique_id, session.window] = (session, time.monotonic() + 5)
        session.queue.clear()
        if restore and player.is_valid:
            if session.stage >= 3 and reason != 'superseded':
                # A client close needs the same acknowledgement as BDS (type
                # 247, serverInitiated=false). A fresh forced close uses true.
                self.send(player, 47, close_payload(session.window, 247, reason != 'dismissed'))
            if session.stage and player.dimension.name == session.dimension:
                block = player.dimension.get_block_at(*session.position)
                self.send(player, 21, block_update(session.position, block.data.runtime_id))
            if reason != 'superseded':
                self.send(player, 50, inventory_slot(124, 0, ItemDescriptor(), role=59))
                # Endstone set_item changes BDS state without sending a native
                # descriptor. Serialize its current inventories and stack IDs;
                # a pre-open packet cache would be stale after a committed move.
                self.native.bridge.refresh_inventory(player)
        self.plugin.logger.info(f'RWPACKET close {session.generation} reason={reason}')

    def forget(self, player):
        self.close(player, 'disconnected', restore=False)
        for key, (session, _) in tuple(self.retiring.items()):
            if key[0] == player.unique_id:
                session.closed, session.reason, session.selection = True, 'disconnected', None
                self.retiring.pop(key, None)
        for cache in (self.registries, self.actual, self.allocators, self.windows, self.registry_queue):
            cache.pop(player.unique_id, None)

    def shutdown(self):
        self.closed = True
        for identity in tuple(self.sessions):
            player = self.server.get_player(identity)
            if player:
                self.close(player, 'provider_disabled')
