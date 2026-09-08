"""Native dialogue command dispatch and per-presentation scene request leases.

Only the scene field of authentic BDS 169/98 packets is translated. BDS owns
the actor, scene lookup, action bounds and every command. No client command
text is executed or copied into an NPC component.
"""
from collections import OrderedDict
from dataclasses import dataclass, field
import json
import re
import secrets
import struct
import time

from .api import NpcDialogue, UIError
from .entities import validate_entity_context, configured_entity
from .protocol import CodecError, Reader, uvar, svar

PREFIX = 'rwui_'
SCENE = re.compile(r'[A-Za-z0-9_.:-]{1,128}')


def text(reader, maximum=65536):
    count = reader.uvar()
    if count > maximum:
        raise CodecError('NPC string exceeds its bound')
    try:
        value = reader.raw(count).decode('utf-8')
    except UnicodeDecodeError as error:
        raise CodecError('Invalid NPC UTF-8') from error
    if '\x00' in value:
        raise CodecError('NPC string contains a null')
    return value


def wire_text(value):
    raw = value.encode('utf-8')
    return uvar(len(raw))+raw


def close_packet(actor_id):
    # Captured native BDS 169 Close: signed varint32 action 1, four empty strings.
    # Unsigned encoding 01 decodes to -1 and opens a blank dialogue on Windows.
    return struct.pack('<q', actor_id)+svar(1)+b'\x00'*4


def validate_dialogue(target):
    if not isinstance(target, NpcDialogue):
        raise UIError('An NpcDialogue is required.')
    validate_entity_context(target.context)
    if (not isinstance(target.scene, str) or (target.scene and not SCENE.fullmatch(target.scene))
            or target.scene.startswith(PREFIX)):
        raise UIError('Invalid native NPC scene name.')
    if (not isinstance(target.branches, (tuple, list)) or len(target.branches) > 32
            or any(not isinstance(s, str) or not SCENE.fullmatch(s) or s.startswith(PREFIX) for s in target.branches)
            or len(set(target.branches)) != len(target.branches)):
        raise UIError('NPC branches must be distinct installed scene names (maximum 32).')
    return target


@dataclass
class Presentation:
    scene: str
    alias: str
    buttons: frozenset
    packet: bytes
    delivered: bool = False
    requests: set = field(default_factory=set)
    action_at: float | None = None
    closed_at: float | None = None


@dataclass
class DialogueSession:
    player_id: object
    target: NpcDialogue
    actor_id: int
    runtime_id: int
    created: float
    presentations: OrderedDict = field(default_factory=OrderedDict)
    current: str | None = None
    opening: bool = True
    opened: bool = False
    closed: bool = False
    reason: str | None = None
    next_check: float = 0
    native_close_at: float | None = None


class NpcBackend:
    def __init__(self, service, clock=time.monotonic):
        self.service, self.native, self.server = service, service.native, service.server
        self.settings = self.native.settings.get('npc', {})
        self.clock = clock
        self.sessions = OrderedDict()
        self.native.add_guard('remote_workstations.npc', self._guard)

    def _guard(self, player, kind):
        return player.unique_id not in self.sessions

    def configured(self, player):
        raw = self.settings.get('source')
        if not isinstance(raw, dict):
            raise UIError('Configure native.npc.source or use open_npc() with a permitted NPC context.')
        context = configured_entity({'entities': {'sources': {'npc': raw}}}, player, 'npc')
        return validate_dialogue(NpcDialogue(context, raw.get('scene', ''), tuple(raw.get('branches', ()))))

    def configured_reason(self, player):
        if player is None:
            return self.reason(None)
        try:
            return self.reason(player, self.configured(player))
        except (UIError, AttributeError, TypeError, ValueError, RuntimeError) as error:
            return str(error)

    def reason(self, player, target=None):
        if self.settings.get('enabled') is not True:
            return 'Native NPC dialogue is disabled in config.toml.'
        if self.native.unavailable or self.native.closed:
            return self.native.unavailable or 'The native compatibility guard is stopping.'
        if player is None or target is None:
            return 'A player and server-authorized NPC scene context are required.'
        try:
            validate_dialogue(target)
            if not player.is_valid or player.is_dead:
                return 'A living, connected player is required.'
            if player.game_version != '1.26.45' or str(player.device_os) != 'Windows':
                return 'NPC dialogue currently admits Windows Bedrock 1.26.45.'
            if player.dimension.name in self.native.settings.get('denied_dimensions', []):
                return 'Remote interfaces are disabled in this dimension.'
            if any(not player.has_permission(p) for p in ('remoteworkstations.use', 'remoteworkstations.open.npc',
                                                          target.context.permission or 'remoteworkstations.contexts')):
                return 'You do not have permission to access this NPC dialogue.'
            actor = target.context.actor
            if (actor.type != 'minecraft:npc' or actor.is_dead or target.context.dimension != player.dimension.name
                    or actor.dimension.level.name != player.dimension.level.name):
                return 'An actual living NPC in the player level and dimension is required.'
            if not self.native.bridge.education_state(player)['education_features_enabled']:
                return 'NPC world features are disabled.'
        except (UIError, AttributeError, TypeError, ValueError, RuntimeError):
            return 'The native NPC context or feature state is unavailable.'
        return None

    def check(self, player, session):
        reason = self.reason(player, session.target)
        if reason:
            raise UIError(reason)
        actor = session.target.context.actor
        if actor.id != session.actor_id or actor.runtime_id != session.runtime_id:
            raise UIError('The source NPC identity changed.')
        entry = self.service._active.get(player.unique_id)
        if (entry is None or entry.cancel_reason or not entry.client._plugin.is_enabled
                or self.service._clients.get(entry.info.owner) is not entry.client):
            raise UIError('NPC dependency authority was revoked.')
        for name, guard in self.native.guards.items():
            if name in ('remote_workstations.npc', 'remote_workstations.developer_menus'):
                continue
            try:
                allowed = guard(player, 'npc')
            except Exception as error:
                raise UIError('An NPC protection guard failed.') from error
            if allowed is not True:
                raise UIError('A protection guard refused this NPC dialogue.')

    def open(self, player, target):
        self.service._thread()
        reason = self.reason(player, target)
        if reason:
            raise UIError(reason)
        identity = player.unique_id
        if (identity in self.sessions or identity in self.native.sessions or identity in self.native.pending
                or not self.native.bridge.state(player)['ready']):
            raise UIError('Close the current interface first.')
        if len(self.sessions) >= 100:
            raise UIError('NPC session limit reached.')
        actor = target.context.actor
        session = DialogueSession(identity, target, actor.id, actor.runtime_id, self.clock())
        self.check(player, session)
        nonce = secrets.token_hex(16)
        tags = ((actor, 'rwui_n_'+nonce), (player, 'rwui_p_'+nonce))
        added = []
        self.sessions[identity] = session
        try:
            # Public Endstone methods retain wrapper/owner-thread semantics.
            # Both selectors contain only generated tags, never a player name.
            for owner, tag in tags:
                if tag in owner.scoreboard_tags or not owner.add_scoreboard_tag(tag):
                    raise UIError('Could not reserve the native NPC command target.')
                added.append((owner, tag))
            command = f'dialogue open @e[type=npc,tag={tags[0][1]}] @a[tag={tags[1][1]}]'
            if target.scene:
                command += ' '+target.scene
            if not self.server.dispatch_command(self.server.command_sender, command):
                raise UIError('The native dialogue command was unavailable.')
            if session.current is None or not session.presentations[session.current].delivered:
                raise UIError('The native NPC dialogue packet was not delivered.')
        except Exception:
            self.close(player, session, 'opening_failed')
            raise
        finally:
            session.opening = False
            cleanup_failed = False
            for owner, tag in reversed(added):
                if not owner.is_valid:
                    continue
                try:
                    # Pinned Endstone IndexSet::remove updates sparse indices
                    # incorrectly when deleting a non-final entry. Native TagCommand
                    # owns removal, including tags appended by intervening plugins.
                    selector = f'@e[type=npc,tag={tag}]' if owner is actor else f'@a[tag={tag}]'
                    self.server.dispatch_command(self.server.command_sender, f'tag {selector} remove {tag}')
                    if tag in owner.scoreboard_tags:
                        raise UIError('The native target tag was not removed.')
                except Exception:
                    cleanup_failed = True
                    self.service.plugin.logger.error('NPC temporary target tag cleanup failed.')
            if cleanup_failed:
                self.close(player, session, 'target_cleanup_failed')
                raise UIError('Native NPC target cleanup failed; inspect server diagnostics.')
        return session

    def outgoing(self, event):
        if event.player is None or event.is_cancelled or event.packet_id != 169:
            return
        session = self.sessions.get(event.player.unique_id)
        if session is None or session.closed:
            return
        raw = bytes(event.payload)
        try:
            r = Reader(raw, 262144)
            actor_id, action = struct.unpack('<q', r.raw(8))[0], r.svar()
            if actor_id != session.actor_id or action != 0:
                return
            text(r)
            start = r.offset
            scene = text(r, 128)
            end = r.offset
            text(r)
            actions = text(r, 131072)
            r.end()
            if scene not in (session.target.scene, *session.target.branches):
                return  # An unrelated native scene supersedes this lease at MONITOR.
            self.check(event.player, session)
            if not session.opening:
                current = session.presentations[session.current]
                if current.action_at is None or self.clock()-current.action_at > 2:
                    return
            values = json.loads(actions) if actions else []
            if not isinstance(values, list) or len(values) > 256:
                raise CodecError('Unexpected native NPC actions')
            if any(not isinstance(a, dict) or type(a.get('mode')) is not int or a['mode'] not in (0,1,2)
                   or type(a.get('type')) is not int or a['type'] not in (0,1) for a in values):
                raise CodecError('Unexpected native NPC action type')
            alias = PREFIX+secrets.token_hex(16)
            packet = raw[:start]+wire_text(alias)+raw[end:]
            view = Presentation(scene, alias, frozenset(i for i,a in enumerate(values) if a['mode'] == 0), packet)
            session.presentations[alias] = view
            session.current = alias
            session.native_close_at = None
            while len(session.presentations) > 8:
                session.presentations.popitem(last=False)
            event.payload = packet
        except (CodecError, ValueError, KeyError, TypeError, UIError, RuntimeError):
            event.is_cancelled = True
            session.reason = 'invalid_native_scene'

    def observe(self, direction, event):
        if direction != 'send' or event.player is None:
            return
        session = self.sessions.get(event.player.unique_id)
        if session is None or session.closed:
            return
        if event.packet_id == 169:
            view = session.presentations.get(session.current)
            if event.is_cancelled:
                return
            raw = bytes(event.payload)
            if raw == close_packet(session.actor_id):
                # BDS closes the UI after ordinary buttons. Keep this generation
                # alive for its one native closing request/on_close commands.
                session.native_close_at = self.clock()
            elif len(raw) == 13 and raw[8:] == b'\x02\x00\x00\x00\x00':
                return  # Closing a different NPC does not supersede this one.
            elif view and raw == view.packet:
                view.delivered = True
            elif not event.is_cancelled:
                # Never close a replacement belonging to BDS or another plugin.
                session.closed, session.reason = True, 'superseded'
                self.sessions.pop(session.player_id, None)
        elif event.packet_id in (46, 80, 81, 100, 303) and not event.is_cancelled:
            session.closed, session.reason = True, 'superseded'
            self.sessions.pop(session.player_id, None)

    def receive(self, event):
        if event.player is None or event.is_cancelled or event.packet_id != 98:
            return
        session = self.sessions.get(event.player.unique_id)
        try:
            raw = bytes(event.payload)
            r = Reader(raw)
            runtime, kind, actions, index = r.uvar(64), r.byte(), text(r), r.byte()
            start = r.offset
            alias = text(r, 128)
            r.end()
            if session is None:
                if alias.startswith(PREFIX):
                    event.is_cancelled = True
                return
            # While owned, all NPC requests must use this exact source and lease.
            self.check(event.player, session)
            view = session.presentations.get(alias)
            if (session.reason or runtime != session.runtime_id or view is None or not view.delivered
                    or actions or kind not in (1,2,6) or kind in view.requests):
                raise CodecError('Unowned, repeated or privileged NPC request')
            if session.native_close_at is not None and kind != 2:
                raise CodecError('Only the native closing callback remains authorized')
            if kind == 1:
                if alias != session.current or 6 not in view.requests or 2 in view.requests or index not in view.buttons:
                    raise CodecError('Invalid native NPC button')
                view.action_at = self.clock()
            else:
                if index != 0 or (kind == 6 and alias != session.current):
                    raise CodecError('Invalid native NPC lifecycle request')
                if kind == 2:
                    if 6 not in view.requests or (alias != session.current and
                            (view.action_at is None or self.clock()-view.action_at > 2)):
                        raise CodecError('Expired NPC closing request')
                    view.closed_at = self.clock()
            event.payload = raw[:start]+wire_text(view.scene)
            view.requests.add(kind)
            if kind == 6:
                session.opened = True  # Actual client opening acknowledgment.
        except (CodecError, ValueError, KeyError, TypeError, UIError, RuntimeError):
            if session is not None:
                event.is_cancelled = True

    def close(self, player, session, reason='cancelled'):
        if session.closed:
            return
        session.closed, session.reason = True, reason
        if self.sessions.get(session.player_id) is session:
            self.sessions.pop(session.player_id)
        if player and player.is_valid and session.current is not None:
            # Native 169 Close carries no inventory or command changes. Its alias
            # remains revoked, so the client's closing callback cannot run commands.
            player.send_packet(169, close_packet(session.actor_id))

    def poll(self, player, session):
        if session.closed:
            return
        now = self.clock()
        if session.reason:
            self.close(player, session, session.reason)
            return
        view = session.presentations.get(session.current)
        if view and view.closed_at is not None and now-view.closed_at >= .5:
            session.closed, session.reason = True, 'native_closed'
            self.sessions.pop(session.player_id, None)
            return
        if (not session.opened and now-session.created > 5) or now-session.created > 1200:
            self.close(player, session, 'timeout')
            return
        if session.native_close_at is not None and now-session.native_close_at > 5:
            self.close(player, session, 'native_close_timeout')
            return
        if now >= session.next_check:
            session.next_check = now+.25
            try:
                self.check(player, session)
            except (UIError, RuntimeError):
                self.close(player, session, 'authority_revoked')
