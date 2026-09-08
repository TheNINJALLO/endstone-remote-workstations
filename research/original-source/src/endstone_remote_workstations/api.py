"""Versioned dependency API. Importing this module does not load Endstone or BDS.

Acquire a plugin-scoped client with get_api(self) during on_enable. Game operations
and callback dispatch take place on the Endstone owner thread. See DEVELOPER_API.md.
"""
from dataclasses import dataclass
from typing import Any, Callable

API_VERSION = (1, 7)


class UIError(RuntimeError):
    """A request failed admission or a dependency contract is unavailable."""


@dataclass(frozen=True)
class HeldItemInfo:
    """Detached preflight information, without item contents or mutation rights.

    The digest detects content changes; it is not a unique item identity. Native
    opening remains unavailable during held-item backend development.
    """
    kind: str
    type_id: str
    slot: int
    amount: int
    digest: str
    durable_id: str | None
    metadata_bytes: int
    native_open_available: bool = False


@dataclass(frozen=True)
class Button:
    label: str
    action: str
    icon: str | None = None


@dataclass(frozen=True)
class Menu:
    title: str
    buttons: tuple[Button, ...]
    content: str = ''
    permission: str | None = None


@dataclass(frozen=True)
class SlotButton:
    """A server ItemStack icon, cloned when the menu is queued."""
    slot: int
    item: Any
    action: str


@dataclass(frozen=True)
class InventoryMenu:
    """A 27-slot chest screen with protected icons and deferred actions."""
    title: str
    buttons: tuple[SlotButton, ...]
    permission: str | None = None


@dataclass(frozen=True)
class EnderChest:
    """The requesting player's real online Ender Chest."""
    title: str = 'Ender Chest'


@dataclass(frozen=True)
class HeldItem:
    """The actual selected shulker; its backing hotbar slot remains locked."""
    title: str = 'Shulker Box'


@dataclass(frozen=True)
class BlockContext:
    """An actual permitted block. Supply only server-authorized positions.

    A permission authorizes access to this source, in addition to the UI's
    normal permission and protection guards. Without one, the requesting
    player needs the operator-default remoteworkstations.contexts permission.
    """
    dimension: str
    position: tuple[int, int, int]
    permission: str | None = None


@dataclass(frozen=True)
class SignContext(BlockContext):
    """A permitted real sign, with the side selected for the native editor."""
    front: bool = True


@dataclass(frozen=True)
class LinkedBlock:
    kind: str
    context: BlockContext


@dataclass(frozen=True)
class EntityContext:
    """A real server-selected Actor, retained only through its public wrapper.

    Source permission is additional to the interface permission. Without one,
    remoteworkstations.contexts is required. Resolve actors on the game thread.
    """
    dimension: str
    actor: Any
    permission: str | None = None


@dataclass(frozen=True)
class LinkedEntity:
    kind: str
    context: EntityContext


@dataclass(frozen=True)
class NpcDialogue:
    """A real NPC and installed native scene, with explicitly allowed branches.

    BDS executes the scene's commands. Cancellation revokes future requests;
    it cannot undo native commands that have already been accepted.
    """
    context: EntityContext
    scene: str = ''
    branches: tuple[str, ...] = ()


@dataclass(frozen=True)
class SessionInfo:
    id: str
    player_id: Any
    owner: str
    kind: str
    state: str
    reason: str | None = None

    @property
    def done(self):
        return self.state in ('closed', 'failed', 'cancelled')


@dataclass(frozen=True)
class ActionContext:
    player: Any
    ui: 'UIClient'
    action: str


class Ticket:
    """A handle to one queued/open UI, including its terminal result."""
    def __init__(self, service, entry):
        self._service, self._entry = service, entry

    @property
    def info(self) -> SessionInfo:
        return self._entry.info

    def cancel(self):
        """Request cancellation on the game thread; safe in a packet callback."""
        self._service.cancel(self._entry)


class UIClient:
    """Registrations and sessions owned by one enabled Endstone plugin."""
    def __init__(self, service, plugin):
        self._service, self._plugin = service, plugin

    def register_action(self, name: str, callback: Callable[[ActionContext], Any], *,
                        permission: str | None = None, export: bool = False) -> str:
        return self._service.register_action(self, name, callback, permission, export)

    def unregister_action(self, name: str):
        self._service.unregister_action(self, name)

    def actions(self) -> tuple[dict, ...]:
        """Own actions and explicit exports, as detached descriptions."""
        return self._service.actions(self)

    def invoke(self, player, action: str):
        """Queue an exported/owned callback and return a concurrent.futures.Future.

        Completion occurs on a server tick. Never wait on result() on the game
        thread; inspect done() or attach a short completion callback instead.
        """
        return self._service.invoke(self, player, action)

    def show_menu(self, player, menu: Menu, *, on_open=None, on_close=None) -> Ticket:
        return self._service.enqueue(self, player, menu, on_open, on_close)

    def open_native(self, player, kind: str, *, on_open=None, on_close=None) -> Ticket:
        """Queue a native screen; unsupported catalog entries produce failed tickets."""
        return self._service.enqueue(self, player, kind, on_open, on_close)

    def open_ender_chest(self, player, *, on_open=None, on_close=None) -> Ticket:
        return self._service.enqueue(self, player, EnderChest(), on_open, on_close)

    def open_held_item(self, player, *, title='Shulker Box', on_open=None, on_close=None) -> Ticket:
        """Queue the held shulker screen; admission occurs on the next game tick."""
        return self._service.enqueue(self, player, HeldItem(title), on_open, on_close)

    def open_linked(self, player, kind: str, context: BlockContext, *, on_open=None, on_close=None) -> Ticket:
        """Open a real loaded block; BDS retains its storage and processing."""
        return self._service.enqueue(self, player, LinkedBlock(kind, context), on_open, on_close)

    def open_sign(self, player, context: BlockContext, *, front=True, on_open=None, on_close=None) -> Ticket:
        """Edit a real sign; BDS validates, filters and saves the submitted text."""
        if not isinstance(context, BlockContext) or type(front) is not bool:
            raise UIError('A BlockContext and boolean sign side are required.')
        source = SignContext(context.dimension, context.position, context.permission, front)
        return self._service.enqueue(self, player, LinkedBlock('sign', source), on_open, on_close)

    def show_inventory(self, player, menu: InventoryMenu, *, on_open=None, on_close=None) -> Ticket:
        return self._service.enqueue(self, player, menu, on_open, on_close)

    def open_entity(self, player, kind: str, context: EntityContext, *, on_open=None, on_close=None) -> Ticket:
        """Open an actual permitted entity inventory; BDS owns every item."""
        return self._service.enqueue(self, player, LinkedEntity(kind, context), on_open, on_close)

    def capabilities(self, player=None) -> tuple[dict, ...]:
        return self._service.capabilities(self, player)

    def inspect_held_item(self, player) -> HeldItemInfo:
        """Read the actual main-hand item on the game thread; never tags or moves it."""
        return self._service.inspect_held_item(self, player)

    def open_npc(self, player, context: EntityContext, *, scene='', branches=(),
                 on_open=None, on_close=None) -> Ticket:
        """Open genuine NPC dialogue using server-authored native scene commands."""
        return self._service.enqueue(self, player, NpcDialogue(context, scene, branches), on_open, on_close)

    def dispose(self):
        """Invalidate actions and cancel only this plugin's managed sessions."""
        self._service.release(self._plugin)


def get_api(plugin, *, minimum=(1, 0)) -> UIClient:
    provider = plugin.server.plugin_manager.get_plugin('remote_workstations')
    if provider is None or not provider.is_enabled:
        raise UIError('RemoteWorkstations must be enabled; declare depend = ["remote_workstations"].')
    version = getattr(provider, 'ui_api_version', None)
    if (not isinstance(version, tuple) or len(version) != 2 or version[0] != minimum[0]
            or version < minimum):
        raise UIError(f'Incompatible RemoteWorkstations dependency API: {version!r}')
    return provider.get_ui_api(plugin)
