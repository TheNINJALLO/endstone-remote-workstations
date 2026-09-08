"""Endstone API 0.11 commands with exact-build native workstation admission."""
from importlib.resources import files
from pathlib import Path
import threading
from endstone import Player
from endstone.command import Command, CommandSender
from endstone.event import event_handler, EventPriority, PacketReceiveEvent, PacketSendEvent
from endstone.event import PlayerQuitEvent, PlayerDeathEvent, PlayerTeleportEvent, PlayerDimensionChangeEvent, PluginDisableEvent
from endstone.form import ActionForm
from endstone.plugin import Plugin
from .catalog import capabilities, command_metadata, configuration, permission_metadata, unavailable
from .diagnostics import Capture
from .journal import JournalWorker
from .presentation import title as cosmetic_title, button as cosmetic_button
from .native_backend import NativeBackend, STATIONS
from .linked import LINKED_STATIONS
from .entities import ENTITY_STATIONS
from .api import API_VERSION
from . import __version__
from .ui_service import UIService
from .packet_backend import PacketBackend

_config_path = Path("plugins/remote_workstations/config.toml")
_startup = configuration(_config_path if _config_path.is_file() else None)
_commands, _aliases = command_metadata(_startup)


class RemoteWorkstations(Plugin):
    api_version = "0.11"
    version = __version__
    ui_api_version = API_VERSION
    description = "Native remote workstations with exact-build compatibility checks"
    commands = _commands
    permissions = permission_metadata()

    def on_enable(self):
        self._owner_thread = threading.get_ident()
        self._capture = None
        self._retired_capture = None
        self._journal = None
        self._journal_counts = {}
        self._journal_error = None
        self._alive = True
        folder = Path(self.data_folder)
        folder.mkdir(parents=True, exist_ok=True)
        config_path = folder / "config.toml"
        if not config_path.exists():
            config_path.write_text(files(__package__).joinpath("config.toml").read_text(encoding="utf-8"), encoding="utf-8")
        self._settings = configuration(config_path)
        self._native = NativeBackend(self, self._settings.get("native", {}))
        self._packet = PacketBackend(self, self._settings.get('packet_inventory', {}))
        self._ui = UIService(self)
        self._journal = JournalWorker(folder / "recovery.sqlite3")
        self._recovery = self._journal.submit("recover")
        self._task = self.server.scheduler.run_task(self, self._poll_startup, delay=0, period=1)
        self._native_task = self.server.scheduler.run_task(self, self._poll_native, delay=0, period=1)
        self.register_events(self)
        if self._settings.get("diagnostics", {}).get("enabled") is True:
            self._start_capture()
        self.logger.info(f"RemoteWorkstations {self.version} / dependency API {'.'.join(map(str, API_VERSION))}: " + (self._native.unavailable or "native " + ", ".join(STATIONS) + " available for the admitted Windows client."))

    def _poll_native(self):
        try:
            self._native.poll()
        except Exception as error:
            self._native.unavailable = f"Native session check failed: {type(error).__name__}"
            self.logger.error(self._native.unavailable)
            self._native.shutdown()
        self._packet.poll()
        self._ui.poll()

    def add_open_guard(self, name, callback):
        self._native.add_guard(name, callback)

    def get_ui_api(self, plugin):
        """Return a versioned, owner-scoped dependency client for an enabled plugin."""
        return self._ui.bind(plugin)

    def _poll_startup(self):
        # Constant-cost polling of one future, no player/inventory scans.
        if self._recovery.done():
            try:
                rows = self._recovery.result()
                self._journal_counts = {"quarantined": len(rows)}
            except Exception as error:
                self._journal_error = type(error).__name__
                self.logger.error(f"Recovery journal unavailable: {self._journal_error}")
            self._task.cancel()

    def _start_capture(self):
        if self._retired_capture is not None:
            if self._retired_capture.thread.is_alive():
                return False
            self._retired_capture = None
        if self._capture is None:
            settings = self._settings.get("diagnostics", {})
            self._capture = Capture(Path(self.data_folder) / "diagnostics",
                                    capacity=settings.get("queue_capacity", 512),
                                    maximum_file_bytes=settings.get("maximum_file_bytes", 2097152),
                                    file_count=settings.get("file_count", 3))
        return True

    def _open(self, sender, key):
        row = unavailable(key.lower())
        if row is None:
            sender.send_error_message("Unknown workstation. Use /workstations list.")
            return
        if not sender.has_permission(row["permission"]):
            sender.send_error_message("You do not have permission to open this interface.")
            return
        if row['id'] == 'npc':
            if not isinstance(sender, Player):
                sender.send_error_message('NPC dialogue must be opened by a player in Minecraft.')
                return
            try:
                self._ui.bind(self).open_native(sender, 'npc', on_close=lambda info:
                    sender.send_error_message(info.reason or 'NPC dialogue opening failed.')
                    if info.state == 'failed' and sender.is_valid else None)
            except Exception as error:
                sender.send_error_message(str(error))
            return
        if row['id'] == 'enderchest' and isinstance(sender, Player):
            try:
                self._ui.bind(self).open_ender_chest(sender, on_close=lambda info:
                    sender.send_error_message(info.reason or 'Ender Chest opening failed.')
                    if info.state == 'failed' and sender.is_valid else None)
            except Exception as error:
                sender.send_error_message(str(error))
            return
        if row['id'] in STATIONS or row['id'] in LINKED_STATIONS or row['id'] in ENTITY_STATIONS:
            if not isinstance(sender, Player):
                sender.send_error_message("This workstation must be opened by a player in Minecraft.")
                return
            if not self._alive:
                return
            try:
                self._native.open(sender, row['id'])
            except Exception as error:
                sender.send_error_message(str(error))
            return
        sender.send_message(f"{row['id']}: {row['status']}; disabled. {row['reason']}")

    def _menu(self, player, category=None):
        if not self._alive or not player.has_permission("remoteworkstations.use"):
            return
        choices = []
        if category is None:
            for group in dict.fromkeys(row["category"] for row in capabilities()):
                choices.append((group.replace("-", " ").title(), lambda p, g=group: self._menu(p, g)))
        else:
            for row in capabilities():
                if row["category"] == category and player.has_permission(row["permission"]):
                    available = (self._packet.reason(player) is None if row['id'] == 'enderchest' else
                                 (row['id'] in STATIONS or row['id'] in LINKED_STATIONS or row['id'] in ENTITY_STATIONS) and self._native.reason(player, row['id']) is None)
                    if row['id'] == 'npc':
                        available = self._ui.npc.configured_reason(player) is None
                    label = row['id'].replace('_', ' ').title()
                    if row['id'] == 'craft':
                        label = 'Crafting table (3x3)'
                    choices.append((label + ("" if available else " - unavailable"), lambda p, key=row["id"]: self._open(p, key)))
        presentation = self._settings.get("presentation", {})
        cosmetic = presentation.get("chest_forms") is True
        size = max(9, min(54, int(presentation.get("rows", 3))*9), ((len(choices)+8)//9)*9)
        heading = cosmetic_title("Workstations", size) if cosmetic else "RemoteWorkstations"
        form = ActionForm(title=heading, content="Choose a category." if category is None else "Choose a workstation. Unavailable entries explain their requirements.")
        for label, callback in choices:
            if cosmetic:
                label, _ = cosmetic_button(label)
            form.add_button(label, on_click=callback)
        if cosmetic:
            for _ in range(size-len(choices)):
                label, _ = cosmetic_button("")
                form.add_button(label)
        player.send_form(form)

    def on_command(self, sender: CommandSender, command: Command, args: list[str]) -> bool:
        if command.name in _aliases:
            self._open(sender, _aliases[command.name])
            return True
        if not sender.has_permission("remoteworkstations.use"):
            sender.send_error_message("You do not have permission to use RemoteWorkstations.")
            return True
        if not args:
            if isinstance(sender, Player):
                self._menu(sender)
            else:
                sender.send_message("/workstations list | /workstations open <type> | /workstations status")
            return True
        action = args[0].lower()
        if action == "list" and len(args) == 1:
            for category in dict.fromkeys(row["category"] for row in capabilities()):
                rows = [row for row in capabilities() if row["category"] == category]
                sender.send_message(f"{category}: " + ", ".join(f"{r['id']} [{r['status']}]" for r in rows))
        elif action == "open" and len(args) == 2:
            self._open(sender, args[1])
        elif action == "status" and len(args) == 1:
            if not sender.has_permission("remoteworkstations.status"):
                sender.send_error_message("Administrator permission required.")
                return True
            sender.send_message(f"RemoteWorkstations API 0.11; Endstone runtime {self.server.version}; BDS {self.server.minecraft_version}; protocol {self.server.protocol_version}.")
            sender.send_message("Target: Endstone 0.11.10 / BDS 1.26.45.1 / protocol 2169. " + (self._native.unavailable or "Native " + ", ".join(STATIONS) + "; Windows client 1.26.45; BDS owns transactions."))
            sender.send_message(f"Journal: {self._journal_error or self._journal_counts}; diagnostics: {'on' if self._capture else 'off'}; active native sessions: {len(self._native.sessions)}.")
            sender.send_message(f"Observed active-session polling: {self._native.timing()}.")
            if self._capture:
                sender.send_message(f"Capture queue drops: {self._capture.dropped}; disk error: {self._capture.error or 'none'}.")
        elif action == "diagnostics" and len(args) == 2 and args[1] in ("on", "off"):
            if not sender.has_permission("remoteworkstations.diagnostics"):
                sender.send_error_message("Administrator permission required.")
                return True
            if args[1] == "on":
                if not self._start_capture():
                    sender.send_message("Diagnostics are still stopping; retry shortly.")
                    return True
            elif self._capture:
                self._capture.close(wait=False)
                self._retired_capture = self._capture
                self._capture = None
            sender.send_message(f"Sanitized packet diagnostics {args[1]}.")
        else:
            sender.send_error_message("Usage: /workstations [list|open <type>|status|diagnostics on|off]")
        return True

    def _observe(self, direction, event):
        if event.player is None:
            return
        on_owner = threading.get_ident() == self._owner_thread
        if not on_owner:
            # Never dereference player/game objects from an unexpected callback thread.
            return
        self._native.observe(direction, event)
        self._ui.observe(direction, event)
        if self._capture is not None:
            self._capture.record(direction, str(event.player.unique_id), event.packet_id, bytes(event.payload), on_owner)

    @event_handler(priority=EventPriority.MONITOR)
    def on_packet_receive(self, event: PacketReceiveEvent):
        self._observe("receive", event)

    @event_handler(priority=EventPriority.HIGHEST)
    def on_owned_inventory_request(self, event: PacketReceiveEvent):
        if threading.get_ident() == self._owner_thread:
            self._ui.npc.receive(event)
            self._native.receive(event)
            self._packet.receive(event)

    @event_handler(priority=EventPriority.HIGHEST)
    def on_authoritative_inventory(self, event: PacketSendEvent):
        if threading.get_ident() == self._owner_thread:
            self._ui.npc.outgoing(event)
            self._native.project(event)
            self._packet.outgoing(event)

    @event_handler(priority=EventPriority.MONITOR)
    def on_packet_send(self, event: PacketSendEvent):
        self._observe("send", event)

    @event_handler(priority=EventPriority.MONITOR)
    def on_player_quit(self, event: PlayerQuitEvent):
        self._native.entity_displays.disconnect(event.player.unique_id)
        self._packet.forget(event.player)
        self._ui.invalidate_player(event.player, 'disconnected')
        self._native.forget(event.player, restore=False)

    @event_handler(priority=EventPriority.MONITOR)
    def on_player_death(self, event: PlayerDeathEvent):
        self._packet.close(event.actor, 'death')
        self._ui.invalidate_player(event.actor, 'death')
        self._native.close(event.actor)

    @event_handler(priority=EventPriority.MONITOR)
    def on_player_teleport(self, event: PlayerTeleportEvent):
        if not event.is_cancelled:
            self._packet.close(event.player, 'teleport')
            self._ui.invalidate_player(event.player, 'teleport')
            self._native.close(event.player)

    @event_handler(priority=EventPriority.MONITOR)
    def on_dimension_change(self, event: PlayerDimensionChangeEvent):
        self._packet.close(event.player, 'dimension_change')
        self._ui.invalidate_player(event.player, 'dimension_change')
        self._native.close(event.player)

    @event_handler(priority=EventPriority.MONITOR)
    def on_dependency_disable(self, event: PluginDisableEvent):
        self._ui.release(event.plugin)

    def on_disable(self):
        self._alive = False
        if hasattr(self, '_ui'):
            self._ui.shutdown()
        if hasattr(self, '_packet'):
            self._packet.shutdown()
        if hasattr(self, "_native"):
            self._native.shutdown()
        if hasattr(self, "_native_task"):
            self._native_task.cancel()
        if hasattr(self, "_task"):
            self._task.cancel()
        if getattr(self, "_capture", None):
            self._capture.close()
        if getattr(self, "_retired_capture", None):
            self._retired_capture.close()
        if getattr(self, "_journal", None):
            self._journal.close()
