import struct
from types import SimpleNamespace

import pytest
from endstone import GameMode

from endstone_remote_workstations.command_editor import translate_command
from endstone_remote_workstations.protocol import CodecError, Open, Reader, svar, uvar
from test_native_backend import live_adapter
from test_linked_native import configure, acknowledge
from test_sign_editors import incoming, close_ack


# Actual Windows 1.26.45 client packet, captured while saving the isolated
# unpowered command block. The command only tests for player presence.
PAYLOAD = bytes.fromhex('010cc801460001000a74657374666f72204073000000010000000000')
SOURCE = (6, 100, 35)


def request(position, command=b'testfor @s', name=b'', delay=0):
    return (b'\x01'+b''.join(svar(v) for v in position)+b'\0\1\0'
            +uvar(len(command))+command+b'\0'+uvar(len(name))+name+b'\0\1'
            +struct.pack('<i', delay)+b'\0')


@pytest.fixture
def command_adapter(live_adapter):
    context, source = configure(live_adapter, 'commandblock')
    source.data = SimpleNamespace(runtime_id=991)
    backend, player, bridge, block, sent, calls = live_adapter
    backend.settings['commandblock'] = {'enabled': True}
    player.is_op, player.game_mode = True, GameMode.CREATIVE
    state = dict(active=False, can_interact=True, save_complete=False)
    bridge.command_state = lambda *args: dict(state)
    def activate(p, x, y, z, generation, permission):
        assert (x, y, z) == context.position and permission == context.permission
        assert generation == backend.sessions[p.unique_id].generation
        state['active'] = True
        p.send_packet(46, Open(23, 16, x, y, z).encode())
        return 23
    bridge.open_command = activate
    bridge.command_touch = lambda p, gen, accept=False: calls.append(('touch', accept))
    def release(p, generation):
        state['active'] = False
        calls.append(('release', generation))
    bridge.close_command = release
    bridge.abandon_command = lambda gen: calls.append(('abandon', gen))
    bridge.shutdown_command_hooks = lambda: calls.append(('shutdown', None))
    return live_adapter, context, state


def open_editor(adapter):
    live, context, state = adapter
    backend, player, bridge, block, sent, calls = live
    backend.open(player, 'commandblock', context)
    acknowledge(backend, player)
    return backend.sessions[player.unique_id]


def test_real_command_codec_preserves_all_save_fields():
    assert PAYLOAD == request(SOURCE)
    target = (-1, -20, 50)
    assert translate_command(PAYLOAD, SOURCE, target) == request(target)
    named = request(SOURCE, name='Custom Ω'.encode(), delay=2147483647)
    assert translate_command(named, SOURCE, target) == request(target, name='Custom Ω'.encode(), delay=2147483647)


@pytest.mark.parametrize('payload', [PAYLOAD[:-1], PAYLOAD+b'\0', b'\0'*65537,
    request(SOURCE, command=b'\xff'), request(SOURCE, command=b'bad\0text'),
    request(SOURCE, command=b'a'*32769), request(SOURCE, name=b'a'*257),
    request(SOURCE, delay=-1), PAYLOAD[:5]+b'\3'+PAYLOAD[6:], PAYLOAD[:6]+b'\2'+PAYLOAD[7:]],
    ids=['truncated', 'trailing', 'oversize', 'utf8', 'nul', 'command-limit', 'name-limit', 'delay', 'mode', 'boolean'])
def test_invalid_command_save_is_rejected(payload):
    with pytest.raises(CodecError):
        translate_command(payload, SOURCE, SOURCE)


def test_command_save_owns_only_display_then_waits_for_native_completion(command_adapter):
    live, context, state = command_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(command_adapter)
    assert session.window == 23 and session.screen_open and bridge.ready
    guessed = incoming(player, 78, request(context.position))
    backend.receive(guessed)
    assert guessed.is_cancelled and ('touch', True) not in calls
    foreign = incoming(player, 78, PAYLOAD)
    backend.receive(foreign)
    assert not foreign.is_cancelled and foreign.payload == PAYLOAD
    event = incoming(player, 78, request(session.position, name=b'RW test', delay=5))
    backend.receive(event)
    assert not event.is_cancelled and event.payload == request(context.position, name=b'RW test', delay=5)
    assert calls[-1] == ('touch', True)
    backend.poll()
    assert player.unique_id in backend.sessions
    state['save_complete'] = True
    backend.poll()
    assert player.unique_id not in backend.sessions and not state['active']


@pytest.mark.parametrize('revoke', ['operator', 'creative', 'administrator', 'context', 'source', 'native', 'guard'])
def test_command_save_revalidates_access_before_admission(command_adapter, revoke):
    live, context, state = command_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(command_adapter)
    if revoke == 'operator': player.is_op = False
    if revoke == 'creative': player.game_mode = GameMode.SURVIVAL
    if revoke == 'administrator': player.has_permission = lambda p: p != 'remoteworkstations.admin'
    if revoke == 'context': player.has_permission = lambda p: p != context.permission
    if revoke == 'source': state['active'] = False
    if revoke == 'native': state['can_interact'] = False
    if revoke == 'guard': backend.add_guard('protection', lambda p, k: False)
    event = incoming(player, 78, request(session.position))
    backend.receive(event)
    assert event.is_cancelled and ('touch', True) not in calls
    backend.poll()
    assert session.closing_at is not None and not state['active']


def test_command_duplicate_cancel_and_late_save_cannot_rearm(command_adapter):
    live, context, state = command_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(command_adapter)
    backend.receive(incoming(player, 78, request(session.position)))
    duplicate = incoming(player, 78, request(session.position))
    backend.receive(duplicate)
    assert duplicate.is_cancelled and calls.count(('touch', True)) == 1
    backend.close(player)
    assert not state['active'] and session.restored
    close_ack(backend, player, session)
    late = incoming(player, 78, request(session.position))
    backend.receive(late)
    assert late.is_cancelled and not backend.sessions


def test_command_disconnect_abandons_native_generation(command_adapter):
    live, context, state = command_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(command_adapter)
    player.is_valid = False
    backend.poll()
    assert ('abandon', session.generation) in calls and not backend.sessions


def test_command_default_disabled(command_adapter):
    live, context, state = command_adapter
    backend, player, bridge, block, sent, calls = live
    backend.settings['commandblock'] = {}
    with pytest.raises(RuntimeError, match='disabled'):
        backend.open(player, 'commandblock', context)


@pytest.mark.parametrize('block_type', ['command_block', 'repeating_command_block', 'chain_command_block'])
def test_command_projection_retains_native_mode_and_conditional_block_state(command_adapter, block_type):
    live, context, state = command_adapter
    backend, player, bridge, block, sent, calls = live
    source = player.dimension.get_block_at(*context.position)
    source.type = 'minecraft:'+block_type
    session = open_editor(command_adapter)
    update = Reader(next(payload for kind, payload in sent if kind == 21))
    assert tuple(update.svar() for _ in range(3)) == session.position
    assert update.uvar() == source.data.runtime_id


def test_plugin_version_uses_package_version():
    from endstone_remote_workstations import __version__
    from endstone_remote_workstations.plugin import RemoteWorkstations
    assert RemoteWorkstations.version == __version__


def input_packet(tick, moving=False):
    return (struct.pack('<8f', 0, 0, 1, 100, 1, 0, float(moving), 0)+b'\1'
            +(b'\2'+svar(50)+svar(10) if moving else b'\1'+svar(50))
            +b'\1\2\0'+b'\0'*8+uvar(tick, 64))


def test_silent_editor_close_retires_on_resumed_gameplay(command_adapter):
    live, context, state = command_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(command_adapter)
    session.opened_at -= 1
    for tick, moving in ((10, True), (11, False), (12, False), (9, True)):
        backend.observe('receive', incoming(player, 144, input_packet(tick, moving)))
        assert not session.superseded
    backend.observe('receive', incoming(player, 144, input_packet(13, True)))
    assert session.superseded and state['active']  # Owner tick performs cleanup.
    backend.poll()
    assert not backend.sessions and not state['active'] and session.restored


def test_gameplay_does_not_cancel_a_submitted_native_save(command_adapter):
    live, context, state = command_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(command_adapter)
    session.opened_at -= 1
    session.editor_neutral_ticks = 2
    backend.receive(incoming(player, 78, request(session.position)))
    backend.observe('receive', incoming(player, 144, input_packet(100, True)))
    assert not session.superseded and state['active']


def test_native_close_starts_owned_cleanup_before_waiting_for_ack(command_adapter):
    live, context, state = command_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(command_adapter)
    player.send_packet(47, bytes((session.window, 247, 0)))
    assert session.closing_at is not None and not session.close_ping_sent
    backend.poll()
    assert session.close_ping_sent and session.restored and not state['active']
    close_ack(backend, player, session)
    assert not backend.sessions and not any(kind == 'kick' for kind, _ in calls)
