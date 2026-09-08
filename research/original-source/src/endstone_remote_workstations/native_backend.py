"""Native workstation orchestration. BDS exclusively processes every item request.

Client projections replace only layer zero at an initially empty position. They
never change world blocks, other block layers, block actors, items or XP.
"""
from dataclasses import dataclass, replace
from collections import OrderedDict, deque
import math
import secrets
import struct
import threading
import time
import uuid

from .protocol import CodecError, Open, Reader, close_payload, svar, uvar
from .api import BlockContext, EntityContext, UIError
from .linked import LINKED_STATIONS, configured_context, validate_context, project_actor_data
from .linked import ACTOR_DISPLAYS, PASSIVE_SCREENS, CHEMISTRY_SCREENS, lectern_page_request
from .entities import ENTITY_STATIONS, configured_entity, validate_entity_context
from .entity_display import EntityDisplays, EQUIPMENT_SCREENS, TRADE_SCREENS, ACTOR_SCREENS, equipment_header, trade_header
from .signs import SignEditors
from .jigsaw import JigsawEditors
from .command_editor import CommandEditors, CommandCartEditors
from .structure import StructureEditors, project_structure_actor
from .chemistry import LabControls


STATIONS = {
    'craft': ('minecraft:crafting_table', 1),
    'anvil': ('minecraft:anvil', 5),
    'stonecutter': ('minecraft:stonecutter_block', 29),
    'grindstone': ('minecraft:grindstone', 26),
    'smithing': ('minecraft:smithing_table', 33),
    'loom': ('minecraft:loom', 24),
    'cartography': ('minecraft:cartography_table', 30),
    'inventory2x2': (None, 255),
    'armor': (None, 255),
    'offhand': (None, 255),
    'recipebook': (None, 255),
}
PLAYER_SCREENS = frozenset(key for key, value in STATIONS.items() if value[1] == 255)
POSITION_CONTROL_SCREENS = PASSIVE_SCREENS | {'labtable'}
# Windows 1.26.45 live comparison: ~100-150 ms opens following chat produced a
# stale client Close(-1). A 500 ms screen-transition interval passed repeated
# command opens. The matching projection ping response is additionally required.
CLIENT_TRANSITION_SECONDS = 0.5
STATE_CHECK_INTERVAL_SECONDS = 0.25
POLL_BUDGET_SECONDS = 0.002
STORAGE_SIZES = dict(chest=27, doublechest=54, trappedchest=27, barrel=27, dispenser=9, dropper=9, hopper=5)
STORAGE_SIZES.update({key: spec[3] for key, spec in ENTITY_STATIONS.items() if spec[3] is not None})


def block_update(position, runtime_id):
    # Verified r26_u4 signed XYZ, including Y; layer zero only.
    return b''.join(svar(v) for v in position)+uvar(runtime_id)+uvar(2)+uvar(0)


@dataclass
class NativeSession:
    player_id: uuid.UUID
    generation: int
    kind: str
    dimension: str
    position: tuple[int, int, int]
    opened_at: float
    window: int | None = None
    closing_at: float | None = None
    restored: bool = False
    restore_pending: bool = False
    close_pending: bool = False
    next_state_check: float = 0
    activating: bool = False
    context: BlockContext | EntityContext | None = None
    entity_id: int | None = None
    entity_runtime_id: int | None = None
    entity_position: tuple | None = None
    entity_added: bool = False
    entity_seen: bool = False
    projecting_entity: bool = False
    entity_restoring: bool = False
    entity_view_prepared: bool = False
    entity_mount: dict | None = None
    entity_mount_detached: bool = False
    entity_link_sending: bytes | None = None
    entity_link_seen: bool = False
    projection_positions: tuple = ()
    shape_seen: bool = False
    shape_count: int | None = None
    shape_invalid: bool = False
    shape_deadline: float | None = None
    projecting_actor: bool = False
    actor_seen: bool = False
    actor_refresh_pending: bool = False
    terrain_sources: tuple = ()
    page_requests: tuple = ()
    superseded: bool = False
    close_acknowledged: bool = False
    close_nonce: int | None = None
    close_ping_sent: bool = False
    screen_open: bool = False
    sign_started: bool = False
    sign_save_at: float | None = None
    editor_neutral_ticks: int = 0
    editor_client_tick: int = 0


@dataclass
class PendingOpen:
    player_id: uuid.UUID
    kind: str
    generation: int
    queued_at: float
    nonce: int
    sent: bool = False
    acknowledged: bool = False
    context: BlockContext | EntityContext | None = None
    entity_id: int | None = None


class NativeBackend:
    def __init__(self, plugin, settings, bridge=None):
        self.plugin, self.server = plugin, plugin.server
        self.owner = threading.get_ident()
        self.settings = settings
        self.sessions = {}
        self.pending = {}
        self._poll_order = OrderedDict()
        self._poll_samples = deque(maxlen=1200)
        self._passive_positions = OrderedDict()
        self.guards = {}
        self.generation = 0
        self.bridge = None
        self.linked_available = False
        self.entities_available = False
        self.entity_displays = EntityDisplays(self)
        self.signs = SignEditors(self)
        self.jigsaw = JigsawEditors(self)
        self.commands = CommandEditors(self)
        self.command_carts = CommandCartEditors(self)
        self.structures = StructureEditors(self)
        self.labs = LabControls(self)
        self._command_entities = OrderedDict()
        self.editors = {'sign': self.signs, 'jigsaw': self.jigsaw, 'commandblock': self.commands,
                        'commandblockminecart': self.command_carts, 'structure': self.structures}
        self.unavailable = 'Native companion is absent from this platform wheel.'
        self.sending_projection = False
        self.closed = False
        if settings.get('enabled', False) is not True:
            self.unavailable = 'Native workstations are disabled in config.toml.'
            return
        if self.server.version != '0.11.10' or self.server.protocol_version != 2169:
            self.unavailable = 'Requires Endstone runtime 0.11.10 and protocol 2169.'
            return
        try:
            if bridge is None:
                from . import _rw_native as bridge
            info = bridge.runtime_info()
            if info['bridge_api'] != 1 or info['transaction_owner'] != 'BDS' or set(info['workstations']) != set(STATIONS):
                raise RuntimeError('Unexpected native companion contract')
            self.bridge = bridge
            self.linked_available = (set(info.get('linked_workstations', ())) == set(LINKED_STATIONS)
                                     and callable(getattr(bridge, 'open_linked_station', None)))
            self.entities_available = (set(info.get('entity_workstations', ())) == set(ENTITY_STATIONS)
                                       and callable(getattr(bridge, 'open_entity', None))
                                       and callable(getattr(bridge, 'send_entity_actor', None))
                                       and callable(getattr(bridge, 'entity_mount_state', None))
                                       and callable(getattr(bridge, 'entity_mount_link', None))
                                       and callable(getattr(bridge, 'entity_state', None)))
            self.unavailable = None
        except (ImportError, OSError, RuntimeError, KeyError) as error:
            self.unavailable = f'Native companion unavailable: {type(error).__name__}: {error}'

    def _owner(self):
        if threading.get_ident() != self.owner:
            raise RuntimeError('Game owner thread required')

    def add_guard(self, name, callback):
        """Protection plugins may veto any command or menu open on the game thread."""
        self._owner()
        if not isinstance(name, str) or not name or name in self.guards or not callable(callback):
            raise ValueError('A unique guard name and callable are required')
        self.guards[name] = callback

    def reason(self, player, key, context=None):
        if key not in STATIONS and key not in LINKED_STATIONS and key not in ENTITY_STATIONS:
            return 'No admitted native implementation for this interface.'
        if self.unavailable:
            return self.unavailable
        if self.closed:
            return 'The workstation backend is stopping.'
        if player.game_version != '1.26.45' or str(player.device_os) != 'Windows':
            return 'This native build currently admits Windows Bedrock 1.26.45 clients.'
        if not player.is_valid or player.is_dead:
            return 'A living, connected player is required.'
        if player.dimension.name in self.settings.get('denied_dimensions', []):
            return 'Remote workstations are disabled in this dimension.'
        if not player.has_permission('remoteworkstations.use') or not player.has_permission('remoteworkstations.open.'+key):
            return 'You do not have permission to open this interface.'
        if key in ENTITY_STATIONS:
            if not self.entities_available or self.settings.get('entities', {}).get('enabled') is not True:
                return 'Linked entity containers are disabled or unavailable in this build.'
            if context is None:
                return 'A server-authorized EntityContext or configured entity source is required.'
            try:
                validate_entity_context(context)
                if player.dimension.name != context.dimension:
                    return 'The source entity must be in the player dimension.'
                if not player.has_permission(context.permission or 'remoteworkstations.contexts'):
                    return 'You do not have permission to access this source entity.'
                if context.actor.type != ENTITY_STATIONS[key][2]:
                    return 'The source entity type does not match this interface.'
                if key == 'agent':
                    if self.settings.get('agent', {}).get('enabled') is not True:
                        return 'Remote Agent inventory is disabled in config.toml.'
                    if not self.bridge.education_state(player)['education_features_enabled']:
                        return 'Education features are disabled in this world.'
                    if context.actor.is_dead:
                        return 'The source Agent is no longer alive.'
                native = self.bridge.entity_state(player, context.actor, key)
                if native['actor_id'] != context.actor.id:
                    return 'The native source entity identity changed.'
                if key in self.editors:
                    reason = self.editors[key].reason(player, context)
                    if reason:
                        return reason
                if key in ACTOR_SCREENS:
                    session = self.sessions.get(player.unique_id)
                    if (session and session.kind == key and session.context is context and session.entity_view_prepared
                            and not self.entity_displays.mount_matches(player, session)):
                        return 'The source entity changed its vehicle.'
                    return self.entity_displays.reason(player)
            except (UIError, AttributeError, TypeError, ValueError, RuntimeError):
                return 'The source entity context is invalid or unavailable.'
        elif key in LINKED_STATIONS:
            if not self.linked_available or self.settings.get('linked', {}).get('enabled') is not True:
                return 'Linked native containers are disabled or unavailable in this build.'
            try:
                context = context or configured_context(self.settings, key)
                if context is None:
                    return 'A server-authorized linked block is required; configure a source or use open_linked().'
                validate_context(context)
                if player.dimension.name != context.dimension:
                    return 'The linked block must be in the player\'s current dimension.'
                if not player.has_permission(context.permission or 'remoteworkstations.contexts'):
                    return 'You do not have permission to access this linked block.'
                if player.dimension.get_block_at(*context.position).type not in LINKED_STATIONS[key][2]:
                    return 'The linked block is unavailable, unloaded, or has changed type.'
                if key in CHEMISTRY_SCREENS:
                    if self.settings.get('chemistry', {}).get('enabled') is not True:
                        return 'Remote chemistry is disabled in config.toml.'
                    if not self.bridge.education_state(player)['education_features_enabled']:
                        return 'Chemistry features are disabled in this world.'
                if key == 'lectern':
                    self.bridge.lectern_state(player, *context.position)
                if key in self.editors:
                    return self.editors[key].reason(player, context)
            except (UIError, AttributeError, TypeError, ValueError, RuntimeError):
                return 'The linked block context is invalid or unavailable.'
        elif context is not None:
            return 'This interface does not accept a linked block context.'
        return None

    @staticmethod
    def _station(key):
        return (STATIONS[key] if key in STATIONS else ENTITY_STATIONS[key][:2]
                if key in ENTITY_STATIONS else LINKED_STATIONS[key][:2])

    def _position(self, player, excluded=()):
        loc = player.location
        x, y, z = math.floor(loc.x), math.floor(loc.y), math.floor(loc.z)
        # Behind the player first; all candidates are close and initially air.
        yaw = math.radians(loc.yaw)
        dx, dz = round(math.sin(yaw)), -round(math.cos(yaw))
        candidates = [(dx, 1, dz), (dx, 2, dz), (1, 1, 0), (-1, 1, 0), (0, 1, 1), (0, 1, -1)]
        if excluded:
            candidates += [(ox, oy, oz) for oy in (1, 2, 3) for ox in range(-2, 3) for oz in range(-2, 3)]
        for ox, oy, oz in candidates:
            position = (x+ox, y+oy, z+oz)
            if not -63 <= position[1] <= 318 or position in excluded:
                continue
            if player.dimension.get_block_at(*position).type == 'minecraft:air':
                return position
        raise RuntimeError('No nearby empty space is available for the temporary client display.')

    def _update(self, player, position, runtime_id):
        self.sending_projection = True
        try:
            player.send_packet(21, block_update(position, runtime_id))
        finally:
            self.sending_projection = False

    def _beacon_terrain(self, player, source):
        # The Bedrock client derives offered tiers from its displayed pyramid.
        # Copy only actual valid source layers into initially empty client space.
        # BDS still validates the real beacon, payment and spatial effects.
        base = self._position(player)
        display = (base[0], base[1]+4, base[2])
        if display[1] > 318:
            raise RuntimeError('Insufficient display height for a beacon.')
        metals = {'minecraft:'+name+'_block' for name in ('iron', 'gold', 'emerald', 'diamond', 'netherite')}
        projected, watched = [], []
        for depth in range(1, 5):
            layer = []
            valid = True
            for dx in range(-depth, depth+1):
                for dz in range(-depth, depth+1):
                    actual = (source[0]+dx, source[1]-depth, source[2]+dz)
                    block = player.dimension.get_block_at(*actual)
                    runtime = block.data.runtime_id
                    watched.append((actual, runtime))
                    valid = valid and block.type in metals
                    layer.append(((display[0]+dx, display[1]-depth, display[2]+dz), runtime))
            if not valid:
                break
            projected.extend(layer)
        for position in (display, *(p for p, _ in projected)):
            if player.dimension.get_block_at(*position).type != 'minecraft:air':
                raise RuntimeError('The beacon display requires clear space above the player.')
        return display, tuple(projected), tuple(watched)

    def _sync_actor(self, player, session):
        reason = self.reason(player, session.kind, session.context)
        if reason:
            raise RuntimeError(reason)
        if not callable(getattr(self.bridge, 'send_block_actor', None)):
            raise RuntimeError('This native companion lacks block actor display synchronization.')
        session.projecting_actor, session.actor_seen = True, False
        try:
            self.bridge.send_block_actor(player, *session.context.position)
        finally:
            session.projecting_actor = False
        if not session.actor_seen:
            raise RuntimeError('The native block actor update was changed or cancelled.')

    def _restore(self, player, session):
        if session.restored:
            return
        if session.kind in ACTOR_SCREENS:
            self.entity_displays.restore(player, session)
            session.restored = True
            return
        if player.dimension.name == session.dimension:
            for position in session.projection_positions or (session.position,):
                current = player.dimension.get_block_at(*position).data
                self._update(player, position, current.runtime_id)
        # A dimension change discards that client's previous dimension chunks.
        session.restored = True

    def open(self, player, key, context=None):
        self._owner()
        if key in LINKED_STATIONS and context is None:
            context = configured_context(self.settings, key)
        if key in ENTITY_STATIONS and context is None:
            context = configured_entity(self.settings, player, key)
        reason = self.reason(player, key, context)
        if reason:
            raise RuntimeError(reason)
        identity = player.unique_id
        # A native close can complete between periodic checks. Allow an immediate
        # new command once BDS confirms that the previous context has gone.
        if (identity in self.sessions and identity not in self.pending
                and self.sessions[identity].kind not in PASSIVE_SCREENS and self.bridge.state(player)['ready']):
            self.forget(player)
        if identity in self.pending or identity in self.sessions or not self.bridge.state(player)['ready']:
            raise RuntimeError('Close the current inventory before opening another workstation.')
        if len(self.sessions.keys() | self.pending.keys()) >= self.settings.get('maximum_sessions', 100):
            raise RuntimeError('The workstation session limit has been reached.')
        self._check_guards(player, key)
        self.generation += 1
        self.pending[identity] = PendingOpen(identity, key, self.generation, time.monotonic(), secrets.randbelow(2**31-1)+1)
        self.pending[identity].context = context
        if isinstance(context, EntityContext):
            self.pending[identity].entity_id = context.actor.id
        self._poll_order[identity] = None

    def _check_guards(self, player, key):
        for name, guard in tuple(self.guards.items()):
            try:
                allowed = guard(player, key)
            except Exception as error:
                self.plugin.logger.error(f'Workstation guard {name} failed: {type(error).__name__}')
                allowed = False
            if allowed is not True:
                raise RuntimeError('A server protection rule denied this workstation.')

    def _prepare(self, player, pending):
        # Send the client projection BEFORE the readiness ping. The client's
        # response establishes ordering for that block update, not just chat.
        reason = self.reason(player, pending.kind, pending.context)
        if reason:
            raise RuntimeError(reason)
        self._check_guards(player, pending.kind)
        if not self.bridge.state(player)['ready']:
            raise RuntimeError('Close the current inventory before opening a workstation.')
        key, identity = pending.kind, pending.player_id
        block_type = self._station(key)[0]
        position = (self._position(player) if block_type else
                    tuple(math.floor(getattr(player.location, axis)) for axis in ('x', 'y', 'z')))
        session = NativeSession(identity, pending.generation, key, player.dimension.name, position, time.monotonic())
        session.context = pending.context
        session.entity_id = pending.entity_id
        if key == 'agent':
            session.entity_runtime_id = pending.context.actor.runtime_id
        if isinstance(pending.context, EntityContext) and pending.context.actor.id != pending.entity_id:
            raise RuntimeError('The queued source entity identity changed.')
        if key == 'commandblockminecart':
            now = time.monotonic()
            self._command_entities = OrderedDict((k, t) for k, t in self._command_entities.items() if t > now)
            target = (identity, session.dimension, pending.context.actor.runtime_id)
            if target in self._command_entities:
                raise RuntimeError('This command cart is retiring its previous editor; retry after one minute.')
            if len(self._command_entities) >= 4096:
                raise RuntimeError('Too many recent command entity editors; retry after one minute.')
        if key in POSITION_CONTROL_SCREENS:
            now = time.monotonic()
            self._passive_positions = OrderedDict((k, t) for k, t in self._passive_positions.items() if t > now)
            if len(self._passive_positions) >= 4096:
                raise RuntimeError('Too many recent interfaces with position controls; retry after one minute.')
            excluded = {p for (owner, dimension, p), _ in self._passive_positions.items()
                        if owner == identity and dimension == player.dimension.name}
            position = self._position(player, excluded)
            session.position = position
            self._passive_positions[(identity, session.dimension, position)] = float('inf')
        terrain = ()
        if key == 'beacon':
            position, terrain, session.terrain_sources = self._beacon_terrain(player, pending.context.position)
            session.position = position
        session.projection_positions = (position,) if block_type else ()
        session.projection_positions += tuple(p for p, _ in terrain)
        if key == 'doublechest':
            # The client pairs adjacent chest blocks. The parity ordering is
            # the pinned InventoryUI helper's tested pair graphic contract.
            other = (position[0]+(1 if position[0] & 1 else -1), position[1], position[2])
            if player.dimension.get_block_at(*other).type != 'minecraft:air':
                raise RuntimeError('Two adjacent empty display positions are required for a double chest.')
            session.projection_positions = (position, other)
        self.sessions[identity] = session
        self.labs.claim(session)
        if key == 'commandblockminecart':
            session.entity_runtime_id = pending.context.actor.runtime_id
            self._command_entities[target] = float('inf')
        if key in ACTOR_SCREENS:
            self.entity_displays.prepare(player, session)
        elif block_type:
            block = (player.dimension.get_block_at(*session.context.position).data if key in ('sign', 'commandblock')
                     else self.server.create_block_data(block_type))
            for projection in session.projection_positions[:2 if key == 'doublechest' else 1]:
                self._update(player, projection, block.runtime_id)
            for projection, runtime in terrain:
                self._update(player, projection, runtime)
            if key in ACTOR_DISPLAYS:
                self._sync_actor(player, session)
        else:
            session.restored = True  # The real player inventory has no projected block.

    def _activate(self, player, pending):
        session = self.sessions[pending.player_id]
        reason = self.reason(player, pending.kind, pending.context)
        if reason:
            raise RuntimeError(reason)
        self._check_guards(player, pending.kind)
        if player.dimension.name != session.dimension:
            raise RuntimeError('The player changed dimension while opening.')
        if self._station(pending.kind)[0] is None:
            session.position = tuple(math.floor(getattr(player.location, axis)) for axis in ('x', 'y', 'z'))
        if any(player.dimension.get_block_at(*p).type != 'minecraft:air' for p in session.projection_positions):
            raise RuntimeError('The temporary display position changed while opening.')
        try:
            session.activating = True
            if pending.kind == 'sign':
                self.signs.activate(player, session)
                return  # Native OpenSign is emitted by the block actor later.
            elif pending.kind == 'jigsaw':
                window = self.jigsaw.activate(player, session)
            elif pending.kind == 'commandblock':
                window = self.commands.activate(player, session)
            elif pending.kind == 'commandblockminecart':
                window = self.command_carts.activate(player, session)
            elif pending.kind == 'structure':
                window = self.structures.activate(player, session)
            elif isinstance(session.context, EntityContext):
                if session.context.actor.id != session.entity_id:
                    raise RuntimeError('The source entity identity changed while opening.')
                window = self.bridge.open_entity(player, session.context.actor, pending.kind)
            elif session.context is not None:
                window = self.bridge.open_linked_station(player, pending.kind, *session.context.position)
            else:
                window = self.bridge.open_station(player, pending.kind, *session.position)
            if session.window != window:
                # PacketSendEvent must observe the exact native open synchronously.
                session.window = window
                raise RuntimeError('The native open packet was changed or cancelled by another plugin.')
            if session.context is not None and session.kind in STORAGE_SIZES:
                if session.shape_invalid:
                    raise RuntimeError('The actual inventory size does not match this interface.')
                if not session.shape_seen:
                    if session.kind in ACTOR_SCREENS:
                        # Native UpdateEquip is synchronous; BDS sends its real
                        # inventory content on the following tick in this build.
                        session.shape_deadline = time.monotonic()+2.0
                    else:
                        raise RuntimeError('The actual storage size does not match this interface; use its matching chest variant.')
        except Exception:
            self.close(player)
            if session.window is None and session.kind != 'sign':
                self.sessions.pop(pending.player_id, None)
            raise
        finally:
            session.activating = False

    def close(self, player):
        self._owner()
        self.pending.pop(player.unique_id, None)
        session = self.sessions.get(player.unique_id)
        if session is None:
            return
        if session.kind in self.editors:
            self.editors[session.kind].close(player, session)
            return
        if session.kind in PLAYER_SCREENS:
            # The Windows player inventory ignores server ContainerClose,
            # including its actual window/type and the special -1 window.
            # There is no projected block or plugin-owned item state to return.
            # Relinquish the lease to vanilla; BDS keeps the real grid/cursor
            # until manual close. ready() continues to reject a new native open.
            self.forget(player)
            return
        if session.closing_at is None:
            session.closing_at = time.monotonic()
            if session.window is not None:
                # Native containers close through the vanilla close handshake.
                # The BDS close handler owns input return and stack context cleanup.
                player.send_packet(47, close_payload(session.window, 247, True))
            if session.kind in PASSIVE_SCREENS:
                # Windows book screens do not echo ContainerClose. A matching
                # ordered ping proves the client processed the preceding close.
                session.close_nonce = secrets.randbelow(2**31-1)+1
        self._restore(player, session)
        if session.close_nonce is not None and not session.close_ping_sent:
            session.close_ping_sent = True
            player.send_packet(115, struct.pack('<QB', session.close_nonce, 1))
        if session.window is None:
            self.forget(player)

    def project(self, event):
        """Rewrite only an owned native open; BDS retains the actual block."""
        self._owner()
        if event.player is None or event.is_cancelled or event.packet_id not in (13, 18, 41, 46, 49, 56, 109, 111, 303):
            return
        session = self.sessions.get(event.player.unique_id)
        if session is None or session.context is None:
            return
        if event.packet_id in (13, 18, 41, 111):
            self.entity_displays.project(event, session)
            return
        try:
            if event.packet_id == 109:
                self.labs.project(event, session)
                return
            if event.packet_id == 303:
                self.signs.project(event, session)
                return
            if event.packet_id == 56:
                if isinstance(session.context, EntityContext):
                    return
                reader = Reader(bytes(event.payload))
                if tuple(reader.svar() for _ in range(3)) != session.context.position:
                    return
                if session.projecting_actor:
                    projector = project_structure_actor if session.kind == 'structure' else project_actor_data
                    event.payload = projector(bytes(event.payload), session.context.position, session.position)
                elif session.kind in ACTOR_DISPLAYS:
                    session.actor_refresh_pending = True
                return
            if event.packet_id == 49:
                reader = Reader(bytes(event.payload))
                window, count = reader.uvar(), reader.uvar()
                current = session.window if session.window is not None else self.bridge.state(event.player).get('window')
                if session.kind in STORAGE_SIZES and window == current:
                    session.shape_seen = True
                    sizes = STORAGE_SIZES[session.kind]
                    allowed = sizes if isinstance(sizes, tuple) else (sizes,)
                    session.shape_invalid = session.shape_invalid or count not in allowed
                    if session.shape_count is not None and session.shape_count != count:
                        session.close_pending = True
                    session.shape_count = count
                    if session.shape_invalid:
                        session.close_pending = True
                    else:
                        session.shape_deadline = None
                return
            if not session.activating:
                return
            if session.window is not None:
                return
            opened = Open.decode(bytes(event.payload))
            owned = (opened.actor_id == session.entity_id if isinstance(session.context, EntityContext) else
                     (opened.x, opened.y, opened.z) == session.context.position)
            if (owned
                    and opened.container_type == self._station(session.kind)[1]):
                event.payload = replace(opened, x=session.position[0], y=session.position[1], z=session.position[2],
                                        actor_id=-1 if isinstance(session.context, EntityContext)
                                        and session.kind != 'commandblockminecart' else opened.actor_id).encode()
        except CodecError:
            session.close_pending = True

    def receive(self, event):
        """Own only matching station controls; gameplay changes occur on ticks."""
        self._owner()
        if event.player is None or event.is_cancelled or event.packet_id not in (56, 78, 90, 109, 125, 132, 147, 306):
            return
        session = self.sessions.get(event.player.unique_id)
        if event.packet_id == 147:
            if session and session.kind in CHEMISTRY_SCREENS:
                self.labs.validate_inventory(event, session)
            elif session and session.kind == 'agent':
                from .agent import validate_inventory
                validate_inventory(self, event, session)
            return
        if event.packet_id == 109:
            self.labs.receive(event, session)
            return
        if event.packet_id in (90, 132):
            self.structures.receive(event, session)
            return
        if event.packet_id == 78:
            self.commands.receive(event, session)
            if not event.is_cancelled:
                self.command_carts.receive(event, session)
            return
        if event.packet_id == 56:
            (self.jigsaw if session and session.kind == 'jigsaw' else self.signs).receive(event, session)
            return
        if session is None or session.context is None:
            return
        payload = bytes(event.payload)
        if event.packet_id == 125:
            if session.kind != 'lectern':
                return
            try:
                page, total, position = lectern_page_request(payload)
            except CodecError:
                event.is_cancelled = True
                session.close_pending = True
                return
            if position != session.position:
                return
            # The stock packet handler applies physical distance. This owned
            # control uses the verified native setter after retaining all other
            # checks; BDS must not also process the same request.
            event.is_cancelled = True
            if (session.window is None or session.closing_at or len(session.page_requests) >= 16
                    or self.reason(event.player, session.kind, session.context)):
                session.close_pending = True
            else:
                session.page_requests += ((page, total),)
            return
        if session.kind != 'crafter':
            return
        if len(payload) != 14:
            event.is_cancelled = True
            session.close_pending = True
            return
        x, y, z, slot, disabled = struct.unpack('<iiiBB', payload)
        if (x, y, z) != session.position:
            return  # Never claim another world's control packet.
        if (session.window is None or session.closing_at or slot > 8 or disabled > 1
                or self.reason(event.player, session.kind, session.context)):
            event.is_cancelled = True
            session.close_pending = True
            return
        event.payload = struct.pack('<iiiBB', *session.context.position, slot, disabled)
        session.actor_refresh_pending = True

    def observe(self, direction, event):
        self._owner()
        if event.player is None or event.is_cancelled:
            return
        if direction == 'send' and (event.player.unique_id in self.entity_displays.link_sends
                or self.entities_available and self.settings.get('entities', {}).get('enabled') is True):
            self.entity_displays.track(event.player, event.packet_id, bytes(event.payload),
                                       self.sessions.get(event.player.unique_id))
        # Never send any packet from a packet callback: Endstone's outgoing
        # serialization storage is reentrant and nested sends can overwrite it.
        pending = self.pending.get(event.player.unique_id)
        if pending and direction == 'receive' and event.packet_id == 115:
            payload = bytes(event.payload)
            if len(payload) == 9:
                timestamp, from_server = struct.unpack('<QB', payload)
                # Windows 1.26.45 preserves the "from server" bit in its echo.
                # Captured fixture: milliseconds become nanoseconds, flag stays 1.
                if pending.sent and timestamp == pending.nonce*1_000_000 and from_server == 1:
                    pending.acknowledged = True
            return
        session = self.sessions.get(event.player.unique_id)
        if session is None:
            return
        payload = bytes(event.payload)
        try:
            if direction == 'receive' and session.kind in ('commandblock', 'commandblockminecart', 'structure') and event.packet_id == 144:
                self.editors[session.kind].observe_input(event, session)
                return
            if direction == 'send' and event.packet_id == 303:
                self.signs.observe(event, session)
                return
            if (direction == 'receive' and event.packet_id == 115 and session.close_nonce
                    and len(payload) == 9):
                if struct.unpack('<QB', payload) == (session.close_nonce*1_000_000, 1):
                    session.close_acknowledged = True
                    session.next_state_check = 0
                return
            if (direction == 'send' and session.kind in PASSIVE_SCREENS and not session.activating
                    and event.packet_id in (46, 100)):
                # A book has no native inventory manager to prevent a foreign
                # UI. Retire only our display; closing here would close the new UI.
                session.superseded = True
                return
            if (direction == 'send' and session.kind in EQUIPMENT_SCREENS | TRADE_SCREENS
                    and event.packet_id == (80 if session.kind in TRADE_SCREENS else 81)):
                window, identity = (trade_header if session.kind in TRADE_SCREENS else equipment_header)(payload)
                if session.activating and session.window is None and identity == session.entity_id:
                    session.window = window
            elif direction == 'send' and event.packet_id == 56 and session.projecting_actor:
                reader = Reader(payload)
                session.actor_seen = tuple(reader.svar() for _ in range(3)) == session.position
            elif direction == 'send' and event.packet_id == 46:
                opened = Open.decode(payload)
                embedded = self._station(session.kind)[0] is None
                if not session.activating and session.window is None:
                    session.close_pending = True
                    return
                if (session.activating and opened.container_type == self._station(session.kind)[1]
                        and (embedded or (opened.x, opened.y, opened.z) == session.position)
                        and (session.kind != 'commandblockminecart' or opened.actor_id == session.entity_id)
                        and session.window is None):
                    session.window = opened.window
                    if embedded:
                        # The player opener uses BDS's actor position (eye Y),
                        # not the public Endstone feet location. No block is
                        # bound or projected; retain the native packet position.
                        session.position = (opened.x, opened.y, opened.z)
            elif event.packet_id == 47 and len(payload) == 3 and payload[0] == session.window:
                session.closing_at = session.closing_at or time.monotonic()
                session.restore_pending = True
                session.close_acknowledged = session.close_acknowledged or direction == 'receive'
            elif (direction == 'send' and event.packet_id == 21 and not self.sending_projection
                  and self._station(session.kind)[0] is not None):
                reader = Reader(payload)
                if tuple(reader.svar() for _ in range(3)) in (session.projection_positions or (session.position,)):
                    # Let authoritative world updates and following block-actor
                    # data reach the client. Never cover a newly changed block.
                    session.close_pending = True
        except CodecError:
            session.close_pending = True

    def forget(self, player, restore=True):
        self._owner()
        self.pending.pop(player.unique_id, None)
        self._poll_order.pop(player.unique_id, None)
        session = self.sessions.pop(player.unique_id, None)
        self.labs.release(session)
        if session and session.kind in self.editors:
            try:
                self.editors[session.kind].release(player, session)
            except (RuntimeError, ValueError):
                self.plugin.logger.error('The sign source disappeared during editor cleanup.')
        if session and session.kind in POSITION_CONTROL_SCREENS:
            self._passive_positions[(player.unique_id, session.dimension, session.position)] = time.monotonic()+60
        self._retire_command_entity(session)
        if session and restore:
            self._restore(player, session)

    def poll(self):
        self._owner()
        if not self._poll_order:
            return
        started = time.perf_counter_ns()
        deadline = time.monotonic()+POLL_BUDGET_SECONDS
        # Only managed players are visited, in fair rotation. The budget is soft:
        # one in-flight BDS call cannot be interrupted safely and may exceed it.
        for identity in tuple(self._poll_order):
            self._poll_order.move_to_end(identity)
            self._poll_player(identity, time.monotonic())
            if identity not in self.pending and identity not in self.sessions:
                self._poll_order.pop(identity, None)
            if time.monotonic() >= deadline:
                break
        self._poll_samples.append((time.perf_counter_ns()-started)/1000)

    def timing(self):
        """Measured active-session poll duration; queried only by administrators."""
        self._owner()
        values = sorted(self._poll_samples)
        if not values:
            return {'samples': 0}
        return {'samples': len(values), 'median_us': round(values[len(values)//2], 1),
                'p95_us': round(values[int((len(values)-1)*0.95)], 1),
                'maximum_us': round(values[-1], 1), 'soft_budget_us': POLL_BUDGET_SECONDS*1_000_000}

    def _poll_player(self, identity, now):
        player = self.server.get_player(identity)
        if player is None or not player.is_valid:
            self.entity_displays.disconnect(identity)
            self.pending.pop(identity, None)
            session = self.sessions.pop(identity, None)
            self.labs.release(session)
            if session and session.kind in self.editors:
                self._abandon_editor(session)
            if session and session.kind in POSITION_CONTROL_SCREENS:
                self._passive_positions[(identity, session.dimension, session.position)] = time.monotonic()+60
            self._retire_command_entity(session)
            return
        pending = self.pending.get(identity)
        session = self.sessions.get(identity)
        if session and session.superseded:
            self.forget(player)
            return
        if pending is not None:
            if now-pending.queued_at > 10:
                self.pending.pop(identity, None)
                self.close(player)
                player.send_error_message('Workstation opening timed out waiting for the client.')
                return
            if not pending.sent:
                try:
                    self._prepare(player, pending)
                    pending.sent = True
                    player.send_packet(115, struct.pack('<QB', pending.nonce, 1))
                except Exception as error:
                    self.close(player)
                    player.send_error_message(str(error))
            elif pending.acknowledged and now-pending.queued_at >= CLIENT_TRANSITION_SECONDS:
                self.pending.pop(identity, None)
                try:
                    self._activate(player, pending)
                except Exception as error:
                    self.close(player)
                    player.send_error_message(str(error))
        session = self.sessions.get(identity)
        if session is None:
            return
        if identity in self.pending:
            if session.close_pending:
                self.close(player)
            return
        if session.kind in self.editors:
            if session.close_pending:
                session.close_pending = False
                self.close(player)
            self.editors[session.kind].poll(player, session, now)
            return
        if session.shape_deadline is not None and now >= session.shape_deadline and session.closing_at is None:
            self.close(player)
            player.send_error_message('Native equipment contents were not received in time.')
        if session.page_requests and session.closing_at is None and not session.close_pending:
            page, total = session.page_requests[0]
            session.page_requests = session.page_requests[1:]
            try:
                reason = self.reason(player, session.kind, session.context)
                if reason:
                    raise RuntimeError(reason)
                self._check_guards(player, session.kind)
                current = self.bridge.lectern_state(player, *session.context.position)
                if current['total_pages'] != total:
                    raise RuntimeError('The real lectern book changed.')
                if current['page'] == page:
                    # This client sends its unchanged page when dismissing the
                    # book. A duplicate page is a no-op; retire that view safely.
                    self.close(player)
                else:
                    self.bridge.lectern_page(player, *session.context.position, page, total)
                    session.actor_refresh_pending = True
            except Exception:
                self.close(player)
        if session.actor_refresh_pending and session.closing_at is None:
            session.actor_refresh_pending = False
            try:
                self._sync_actor(player, session)
            except Exception:
                self.close(player)
        urgent = session.close_pending or session.restore_pending
        if session.close_pending:
            session.close_pending = False
            self.close(player)
        elif session.restore_pending:
            session.restore_pending = False
            self._restore(player, session)
        if urgent or now >= session.next_state_check:
            session.next_state_check = now+STATE_CHECK_INTERVAL_SECONDS
            if (self.bridge.state(player)['ready']
                    and (session.kind not in PASSIVE_SCREENS or session.close_acknowledged)):
                self.forget(player)
                return
            if session.context is not None and self.reason(player, session.kind, session.context):
                self.close(player)
            if (isinstance(session.context, EntityContext) and session.closing_at is None
                    and session.context.actor.id != session.entity_id):
                self.close(player)
            if session.closing_at is None and any(player.dimension.get_block_at(*p).data.runtime_id != runtime
                                                  for p, runtime in session.terrain_sources):
                self.close(player)
            if session.context is not None and session.closing_at is None:
                try:
                    self._check_guards(player, session.kind)
                except RuntimeError:
                    self.close(player)
        if (player.is_dead or player.dimension.name != session.dimension
                or now-session.opened_at > self.settings.get('maximum_duration_seconds', 1200)):
            self.close(player)
        if session.closing_at and now-session.closing_at > 5:
            # An unresponsive close cannot authorize another session. Disconnect
            # invokes BDS's own player cleanup instead of guessing item writeback.
            player.kick('Workstation close was not acknowledged. Rejoin to continue safely.')
            self.forget(player, restore=False)

    def _abandon_editor(self, session):
        if session.kind == 'structure':
            self.bridge.abandon_structure(session.generation)
        elif session.kind in ('commandblock', 'commandblockminecart'):
            self.bridge.abandon_command(session.generation)
        else:
            self.bridge.abandon_sign(session.generation)

    def _retire_command_entity(self, session):
        if session and session.kind == 'commandblockminecart' and session.entity_runtime_id is not None:
            self._command_entities[(session.player_id, session.dimension, session.entity_runtime_id)] = time.monotonic()+60

    def shutdown(self):
        self._owner()
        self.closed = True
        self.pending.clear()
        for identity in tuple(self.sessions):
            player = self.server.get_player(identity)
            if player and player.is_valid:
                self.close(player)
            elif self.sessions[identity].kind in self.editors:
                self._abandon_editor(self.sessions[identity])
        self.sessions.clear()
        self._poll_order.clear()
        self.entity_displays.visible.clear()
        self.entity_displays.untracked.clear()
        if self.bridge and callable(getattr(self.bridge, 'shutdown_sign_hooks', None)):
            self.bridge.shutdown_sign_hooks()
        if self.bridge and callable(getattr(self.bridge, 'shutdown_command_hooks', None)):
            self.bridge.shutdown_command_hooks()
        if self.bridge and callable(getattr(self.bridge, 'shutdown_structure_hooks', None)):
            self.bridge.shutdown_structure_hooks()
