"""Owner-scoped developer menus, actions, and deferred native screen requests."""
from collections import OrderedDict, deque
from concurrent.futures import Future
from dataclasses import dataclass, replace
import re
import threading
import time
import uuid

from .api import ActionContext, Button, Menu, InventoryMenu, SlotButton, EnderChest, SessionInfo, Ticket, UIClient, UIError
from .catalog import capabilities, unavailable
from .native_backend import STATIONS
from .api import LinkedBlock, LinkedEntity, NpcDialogue
from .npc import NpcBackend, validate_dialogue
from .linked import LINKED_STATIONS, validate_context
from .entities import ENTITY_STATIONS, validate_entity_context
from .protocol import Reader, CodecError

NAME = re.compile(r'[a-z][a-z0-9_.-]{0,63}')


@dataclass
class _Action:
    client: UIClient
    callback: object
    permission: str | None
    exported: bool


@dataclass
class _Entry:
    client: UIClient
    info: SessionInfo
    target: Menu | str
    created: float
    on_open: object = None
    on_close: object = None
    generation: int | None = None
    cancel_reason: str | None = None
    selection: str | None = None
    responded: bool = False
    wire_id: int | None = None
    opened: bool = False
    packet_session: object = None
    npc_session: object = None


@dataclass
class _Invocation:
    client: UIClient
    player_id: object
    name: str
    future: Future
    invalidated: str | None = None


class UIService:
    def __init__(self, plugin, *, form_factory=None, clock=time.monotonic):
        self.plugin, self.server, self.native = plugin, plugin.server, plugin._native
        self.packet = getattr(plugin, '_packet', None)
        self.owner_thread = threading.get_ident()
        self.clock = clock
        self._clients, self._actions = {}, {}
        self._active = OrderedDict()
        self._foreground = {}
        self._sending = None
        self._closed = False
        self._invocations = deque()
        self._form_factory = form_factory
        self.native.add_guard('remote_workstations.developer_menus', self._native_guard)
        self.npc = NpcBackend(self, clock)

    def _thread(self):
        if threading.get_ident() != self.owner_thread:
            raise UIError('Use the Endstone scheduler: the UI API requires the game owner thread.')

    def _check(self, client):
        self._thread()
        if (self._closed or self._clients.get(client._plugin.name) is not client
                or not client._plugin.is_enabled):
            raise UIError('This UI dependency client is disabled or disposed.')

    @staticmethod
    def _text(value, maximum, label):
        if not isinstance(value, str) or len(value) > maximum or '\x00' in value:
            raise UIError(f'Invalid {label}.')
        return value

    def bind(self, plugin):
        self._thread()
        if (self._closed or not plugin.is_enabled or not NAME.fullmatch(plugin.name)
                or plugin.name in ('native', 'custom')):
            raise UIError('An enabled plugin with a valid namespace is required.')
        if self.server.plugin_manager.get_plugin(plugin.name) is not plugin:
            raise UIError('The caller must be a plugin registered on this server.')
        previous = self._clients.get(plugin.name)
        if previous is not None and previous._plugin is not plugin:
            raise UIError('An earlier plugin instance still owns this namespace.')
        if previous is None:
            previous = self._clients[plugin.name] = UIClient(self, plugin)
        return previous

    def _name(self, client, name):
        if not isinstance(name, str):
            raise UIError('Action names must be strings.')
        if ':' not in name:
            name = client._plugin.name + ':' + name
        parts = name.split(':')
        if len(parts) != 2 or not all(NAME.fullmatch(p) for p in parts):
            raise UIError('Use a namespaced action such as my_plugin:choose_recipe.')
        return name

    def register_action(self, client, name, callback, permission, exported):
        self._check(client)
        name = self._name(client, name)
        if name.split(':')[0] != client._plugin.name or name in self._actions:
            raise UIError('The action must be unique and belong to your plugin namespace.')
        if not callable(callback) or type(exported) is not bool:
            raise UIError('A callable action and boolean export flag are required.')
        if permission is not None:
            if not self._text(permission, 128, 'permission'):
                raise UIError('A permission cannot be empty.')
        if sum(a.client is client for a in self._actions.values()) >= 128 or len(self._actions) >= 4096:
            raise UIError('Action registration limit reached.')
        self._actions[name] = _Action(client, callback, permission, exported)
        return name

    def unregister_action(self, client, name):
        self._check(client)
        name = self._name(client, name)
        action = self._actions.get(name)
        if action is not None and action.client is not client:
            raise UIError('Cannot unregister another plugin action.')
        self._actions.pop(name, None)

    def actions(self, client):
        self._check(client)
        return tuple({'id': name, 'permission': a.permission, 'exported': a.exported}
                     for name, a in self._actions.items()
                     if a.client._plugin.is_enabled and (a.client is client or a.exported))

    def _action(self, client, player, name):
        name = self._name(client, name)
        if name.startswith('native:'):
            row = unavailable(name[7:])
            if row is None:
                raise UIError('Unknown native interface.')
            reason = self.native.reason(player, row['id'])
            if reason:
                raise UIError(reason if row['id'] in STATIONS or row['id'] in LINKED_STATIONS else row['reason'])
            return name, None
        action = self._actions.get(name)
        if action is None or (action.client is not client and not action.exported):
            raise UIError('Action is unavailable or is not exported to other plugins.')
        self._check(action.client)
        if action.permission and not player.has_permission(action.permission):
            raise UIError('You do not have permission to use this action.')
        return name, action

    def invoke(self, client, player, name):
        self._check(client)
        name = self._name(client, name)
        if len(self._invocations) >= 256:
            raise UIError('Pending action limit reached.')
        future = Future()
        self._invocations.append(_Invocation(client, player.unique_id, name, future))
        return future

    def _invoke_now(self, client, player, name):
        self._check(client)
        if not player.is_valid or player.is_dead:
            raise UIError('A living, connected player is required.')
        name, action = self._action(client, player, name)
        if action is None:
            return client.open_native(player, name[7:])
        return action.callback(ActionContext(player, action.client, name))

    def capabilities(self, client, player):
        self._check(client)
        rows = capabilities()
        for row in rows:
            if row['id'] == 'npc':
                reason = self.npc.configured_reason(player)
                row.update(available=reason is None, unavailable_reason=reason, backend='native-npc-windows')
                continue
            if row['id'] == 'enderchest' and self.packet is not None:
                reason = self.packet.reason(player) if player is not None else 'Player admission is required.'
                row.update(available=reason is None, unavailable_reason=reason, backend='packet_real_ender',
                           status='candidate' if reason is None else row['status'])
                continue
            if row['id'] not in STATIONS and row['id'] not in LINKED_STATIONS and row['id'] not in ENTITY_STATIONS:
                reason = row['reason']
            elif player is not None:
                reason = self.native.reason(player, row['id'])
            else:
                reason = self.native.unavailable or ('Player and linked source admission are required.'
                    if row['id'] in LINKED_STATIONS or row['id'] in ENTITY_STATIONS else None)
            row['available'] = (row['id'] in STATIONS or row['id'] in LINKED_STATIONS or row['id'] in ENTITY_STATIONS) and reason is None and not self.native.closed
            row['unavailable_reason'] = reason
        return tuple(rows)

    def enqueue(self, client, player, target, on_open, on_close):
        self._check(client)
        for callback in (on_open, on_close):
            if callback is not None and not callable(callback):
                raise UIError('Lifecycle callbacks must be callable.')
        if not player.is_valid or player.is_dead:
            raise UIError('A living, connected player is required.')
        if player.unique_id in self._active:
            raise UIError('This player already has a dependency UI request; close it first.')
        if len(self._active) >= 100:
            raise UIError('Dependency UI session limit reached.')
        if isinstance(target, Menu):
            self._text(target.title, 128, 'menu title')
            self._text(target.content, 4096, 'menu content')
            if not isinstance(target.buttons, (tuple, list)) or not 1 <= len(target.buttons) <= 54:
                raise UIError('A menu must have between one and 54 buttons.')
            if target.permission is not None and not self._text(target.permission, 128, 'permission'):
                raise UIError('A permission cannot be empty.')
            buttons = []
            for button in target.buttons:
                if not isinstance(button, Button):
                    raise UIError('Menus contain Button objects.')
                self._text(button.label, 256, 'button label')
                if button.icon is not None and (not isinstance(button.icon, str)
                        or not re.fullmatch(r'textures/[a-zA-Z0-9_/-]{1,160}', button.icon)):
                    raise UIError('Icons must be resource-pack texture paths.')
                buttons.append(replace(button, action=self._name(client, button.action)))
            target = replace(target, buttons=tuple(buttons))
            kind = 'custom:action_form'
        elif isinstance(target, InventoryMenu):
            from .packet_items import clone
            self._text(target.title, 128, 'inventory title')
            if target.permission is not None and not self._text(target.permission, 128, 'permission'):
                raise UIError('A permission cannot be empty.')
            if not isinstance(target.buttons, (tuple, list)) or not 1 <= len(target.buttons) <= 27:
                raise UIError('An inventory menu requires one to 27 buttons.')
            slots, buttons = set(), []
            for button in target.buttons:
                if (not isinstance(button, SlotButton) or type(button.slot) is not int
                        or not 0 <= button.slot < 27 or button.slot in slots):
                    raise UIError('Inventory buttons require distinct slots from 0 to 26.')
                icon = clone(button.item)
                if icon is None:
                    raise UIError('An inventory button requires a nonempty server ItemStack.')
                slots.add(button.slot)
                buttons.append(replace(button, item=icon, action=self._name(client, button.action)))
            target = replace(target, buttons=tuple(buttons))
            kind = 'custom:packet_inventory'
        elif isinstance(target, EnderChest):
            self._text(target.title, 128, 'Ender title')
            kind = 'packet:enderchest'
        elif isinstance(target, NpcDialogue):
            validate_dialogue(target)
            target = replace(target, branches=tuple(target.branches))
            kind = 'entity:npc'
        elif isinstance(target, LinkedBlock):
            validate_context(target.context)
            self._text(target.kind, 64, 'linked interface')
            row = unavailable(target.kind.lower())
            key = row['id'] if row else target.kind.lower()
            if key not in LINKED_STATIONS:
                raise UIError('This interface has no linked block backend.')
            target = replace(target, kind=key)
            kind = 'linked:' + key
        elif isinstance(target, LinkedEntity):
            validate_entity_context(target.context)
            self._text(target.kind, 64, 'entity interface')
            row = unavailable(target.kind.lower())
            key = row['id'] if row else target.kind.lower()
            if key not in ENTITY_STATIONS:
                raise UIError('This interface has no admitted entity backend.')
            target = replace(target, kind=key)
            kind = 'entity:' + key
        elif isinstance(target, str):
            self._text(target, 64, 'native interface')
            row = unavailable(target.lower())
            target = row['id'] if row else target.lower()
            kind = 'native:' + target
        else:
            raise UIError('Expected a Menu or native interface name.')
        info = SessionInfo(uuid.uuid4().hex, player.unique_id, client._plugin.name, kind, 'queued')
        entry = _Entry(client, info, target, self.clock(), on_open, on_close)
        self._active[player.unique_id] = entry
        return Ticket(self, entry)

    def cancel(self, entry):
        self._thread()
        if not entry.info.done:
            entry.cancel_reason = 'cancelled'

    def _native_guard(self, player, kind):
        entry = self._active.get(player.unique_id)
        return entry is None or isinstance(entry.target, (str, LinkedBlock, LinkedEntity))

    def _notify(self, entry, callback):
        if (callback is not None and entry.client._plugin.is_enabled and not self._closed
                and self._clients.get(entry.info.owner) is entry.client):
            try:
                callback(entry.info)
            except Exception as error:
                self.plugin.logger.error(f'UI callback {entry.info.owner} failed: {type(error).__name__}')

    def _finish(self, entry, state, reason=None):
        if entry.info.done:
            return
        if entry.npc_session is not None and not entry.npc_session.closed:
            self.npc.close(self.server.get_player(entry.info.player_id), entry.npc_session, reason or state)
        identity = entry.info.player_id
        entry.info = replace(entry.info, state=state, reason=reason)
        if self._active.get(identity) is entry:
            self._active.pop(identity)
        if self._foreground.get(identity) == entry.info.id:
            self._foreground.pop(identity)
        self._notify(entry, entry.on_close)
        entry.on_open = entry.on_close = None

    def _opened(self, entry):
        if not entry.opened:
            entry.opened = True
            entry.info = replace(entry.info, state='open')
            self._notify(entry, entry.on_open)

    def _respond(self, entry, player, selection):
        # Endstone invokes forms while handling a packet. Record only; no user
        # callback, new UI, inventory operation, or outgoing packet here.
        if threading.get_ident() != self.owner_thread:
            return
        if (self._active.get(player.unique_id) is not entry or entry.info.done
                or entry.responded or player.unique_id != entry.info.player_id):
            return
        entry.responded = True
        if selection is not None:
            if type(selection) is not int or not 0 <= selection < len(entry.target.buttons):
                entry.cancel_reason = 'invalid_selection'
                return
            entry.selection = entry.target.buttons[selection].action

    def _show(self, entry, player):
        menu = entry.target
        if menu.permission and not player.has_permission(menu.permission):
            raise UIError('You do not have permission to open this menu.')
        if (player.unique_id in self.native.sessions or player.unique_id in self.native.pending
                or (self.packet is not None and player.unique_id in self.packet.sessions)
                or (self.native.bridge and not self.native.bridge.state(player)['ready'])):
            raise UIError('Close the current native inventory before opening a custom menu.')
        if self._form_factory is None:
            from endstone.form import ActionForm
            factory = ActionForm
        else:
            factory = self._form_factory
        form = factory(title=menu.title, content=menu.content,
                       on_submit=lambda p, choice: self._respond(entry, p, choice),
                       on_close=lambda p: self._respond(entry, p, None))
        for button in menu.buttons:
            form.add_button(button.label, icon=button.icon)
        self._sending = entry
        try:
            player.send_form(form)
        finally:
            self._sending = None
        if entry.wire_id is None:
            raise UIError('The menu packet was not observed or was cancelled.')
        self._opened(entry)

    def observe(self, direction, event):
        """Observe owned screen delivery/replacement; never intercept transactions."""
        self._thread()
        self.npc.observe(direction, event)
        if event.player is None or event.is_cancelled or direction != 'send':
            return
        identity = event.player.unique_id
        entry = self._active.get(identity)
        if entry is None or not isinstance(entry.target, Menu):
            return
        if event.packet_id == 100 and self._sending is entry:
            try:
                entry.wire_id = Reader(bytes(event.payload)).uvar()
                self._foreground[identity] = entry.info.id
            except CodecError:
                entry.cancel_reason = 'invalid_form_packet'
        elif event.packet_id in (46, 100):
            self._foreground.pop(identity, None)
            entry.cancel_reason = 'superseded'

    def _cancel_view(self, entry, player):
        if isinstance(entry.target, NpcDialogue):
            if entry.npc_session is not None:
                self.npc.close(player, entry.npc_session, entry.cancel_reason or 'cancelled')
        elif isinstance(entry.target, (InventoryMenu, EnderChest)):
            if (self.packet is not None and player and entry.packet_session is not None
                    and self.packet.sessions.get(player.unique_id) is entry.packet_session):
                self.packet.close(player, entry.cancel_reason or 'cancelled')
        elif isinstance(entry.target, Menu):
            if self._foreground.get(entry.info.player_id) == entry.info.id and player and player.is_valid:
                player.close_form()
        else:
            session = self.native.sessions.get(entry.info.player_id) or self.native.pending.get(entry.info.player_id)
            if session and session.generation == entry.generation and player and player.is_valid:
                self.native.close(player)

    def _poll(self, entry):
        player = self.server.get_player(entry.info.player_id)
        if player is None or not player.is_valid:
            self._finish(entry, 'cancelled', 'disconnected')
            return
        if not entry.client._plugin.is_enabled:
            entry.cancel_reason = 'owner_disabled'
        if player.is_dead:
            entry.cancel_reason = 'death'
        if self.clock() - entry.created > (300 if isinstance(entry.target, Menu) else 1220):
            entry.cancel_reason = 'timeout'
        if entry.cancel_reason:
            self._cancel_view(entry, player)
            self._finish(entry, 'cancelled', entry.cancel_reason)
            return
        if entry.info.state == 'queued' and entry.target == 'npc':
            entry.target = self.npc.configured(player)
        if isinstance(entry.target, NpcDialogue):
            if entry.info.state == 'queued':
                entry.npc_session = self.npc.open(player, entry.target)
                entry.info = replace(entry.info, state='opening')
            else:
                session = entry.npc_session
                self.npc.poll(player, session)
                if session.opened:
                    self._opened(entry)
                if session.closed:
                    self._finish(entry, 'closed' if entry.opened else 'failed', session.reason)
        elif isinstance(entry.target, (InventoryMenu, EnderChest)):
            if self.packet is None:
                raise UIError('The packet inventory backend is unavailable.')
            if isinstance(entry.target, InventoryMenu) and entry.target.permission and not player.has_permission(entry.target.permission):
                raise UIError('Inventory menu permission was revoked.')
            if entry.info.state == 'queued':
                icons, buttons = None, None
                if isinstance(entry.target, InventoryMenu):
                    icons, buttons = [None] * 27, {}
                    for button in entry.target.buttons:
                        icons[button.slot], buttons[button.slot] = button.item, button.action
                entry.packet_session = self.packet.open(player, entry.target.title, icons, buttons)
                entry.info = replace(entry.info, state='opening')
            else:
                session = entry.packet_session
                if session.opened:
                    self._opened(entry)
                if session.closed:
                    self._finish(entry, 'closed' if entry.opened else 'failed', session.reason)
                    if session.selection:
                        self._invoke_now(entry.client, player, session.selection)
        elif isinstance(entry.target, Menu):
            if entry.responded:
                # Retire the old generation before callbacks may open another UI.
                self._foreground.pop(entry.info.player_id, None)
                if entry.target.permission and not player.has_permission(entry.target.permission):
                    self._finish(entry, 'failed', 'Menu permission was revoked.')
                    return
                self._finish(entry, 'closed', 'selected' if entry.selection else 'dismissed')
                if entry.selection:
                    try:
                        self._invoke_now(entry.client, player, entry.selection)
                    except Exception as error:
                        self.plugin.logger.error(f'UI action {entry.selection} failed: {type(error).__name__}')
                        player.send_error_message(str(error) if isinstance(error, UIError) else 'The menu action failed.')
            elif entry.info.state == 'queued':
                self._show(entry, player)
        elif entry.info.state == 'queued':
            key = entry.target.kind if isinstance(entry.target, (LinkedBlock, LinkedEntity)) else entry.target
            row = unavailable(key)
            if row is None or (key not in STATIONS and key not in LINKED_STATIONS and key not in ENTITY_STATIONS):
                raise UIError(row['reason'] if row else 'Unknown native interface.')
            if isinstance(entry.target, (LinkedBlock, LinkedEntity)):
                self.native.open(player, key, entry.target.context)
            else:
                self.native.open(player, key)
            entry.generation = self.native.pending[player.unique_id].generation
            entry.info = replace(entry.info, state='opening')
        else:
            pending = self.native.pending.get(player.unique_id)
            session = self.native.sessions.get(player.unique_id)
            if (session and session.generation == entry.generation
                    and (session.window is not None or getattr(session, 'screen_open', False))
                    and session.closing_at is None and not session.close_pending
                    and getattr(session, 'shape_deadline', None) is None):
                self._opened(entry)
            if (pending is None or pending.generation != entry.generation) and (session is None or session.generation != entry.generation):
                self._finish(entry, 'closed' if entry.opened else 'failed',
                             'native_closed' if entry.opened else 'Native opening did not complete; see workstation diagnostics.')

    def poll(self):
        self._thread()
        # Bound work to active requests and at most sixteen visits per tick.
        # Native calls retain the backend's existing soft timing limits.
        for identity in tuple(self._active)[:16]:
            entry = self._active.get(identity)
            if entry is None:
                continue
            self._active.move_to_end(identity)
            try:
                self._poll(entry)
            except Exception as error:
                try:
                    self._cancel_view(entry, self.server.get_player(identity))
                except Exception as cleanup_error:
                    self.plugin.logger.error(f'UI cleanup failed: {type(cleanup_error).__name__}')
                finally:
                    self._finish(entry, 'failed', str(error))
        for _ in range(min(16, len(self._invocations))):
            request = self._invocations.popleft()
            client, identity, name, future = request.client, request.player_id, request.name, request.future
            if not future.set_running_or_notify_cancel():
                continue
            try:
                if request.invalidated:
                    raise UIError('Action invalidated by player lifecycle: ' + request.invalidated)
                player = self.server.get_player(identity)
                if player is None:
                    raise UIError('The player disconnected before the action ran.')
                result = self._invoke_now(client, player, name)
            except Exception as error:
                future.set_exception(error)
            else:
                future.set_result(result)

    def invalidate_player(self, player, reason):
        self._thread()
        entry = self._active.get(player.unique_id)
        if entry:
            entry.cancel_reason = reason
        for request in self._invocations:
            if request.player_id == player.unique_id:
                request.invalidated = reason

    def release(self, plugin):
        self._thread()
        client = self._clients.get(plugin.name)
        if client is None or client._plugin is not plugin:
            return
        self._clients.pop(plugin.name)
        self._actions = {name: action for name, action in self._actions.items() if action.client is not client}
        for entry in self._active.values():
            if entry.client is client:
                entry.cancel_reason = 'owner_disabled'

    def shutdown(self):
        self._thread()
        self._closed = True
        for entry in tuple(self._active.values()):
            try:
                self._cancel_view(entry, self.server.get_player(entry.info.player_id))
            except Exception as error:
                self.plugin.logger.error(f'UI shutdown cleanup failed: {type(error).__name__}')
            finally:
                self._finish(entry, 'cancelled', 'provider_disabled')
        while self._invocations:
            self._invocations.popleft().future.cancel()
        self._clients.clear()
        self._actions.clear()
        self._foreground.clear()
