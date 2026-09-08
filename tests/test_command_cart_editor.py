import struct
import time

import pytest
from endstone import GameMode

from endstone_remote_workstations.command_editor import validate_command_entity
from endstone_remote_workstations.protocol import CodecError, Open, uvar
from test_native_backend import live_adapter
from test_equipment_display import setup_equipment
from test_linked_native import acknowledge
from test_sign_editors import incoming, close_ack


# Actual unpowered fixture save from a Windows 1.26.45 client.
CAPTURED = bytes.fromhex('000f0a74657374666f72204073000c52572063617274207465737400010000000001')


def request(runtime, delay=0):
    return b'\0'+uvar(runtime, 64)+CAPTURED[2:-5]+struct.pack('<i', delay)+b'\1'


@pytest.fixture
def cart_adapter(live_adapter):
    context, actor, packet = setup_equipment(live_adapter, kind='commandblockminecart')
    backend, player, bridge, block, sent, calls = live_adapter
    backend.settings['commandblockminecart'] = {'enabled': True}
    player.is_op, player.game_mode = True, GameMode.CREATIVE
    state = dict(active=False, can_interact=True, save_complete=False)
    bridge.command_entity_state = lambda *args: dict(state, actor_id=actor.id, runtime_id=actor.runtime_id)
    def activate(p, source, generation, permission):
        assert source is actor and permission == context.permission
        state['active'] = True
        p.send_packet(46, Open(23, 16, 90, 70, 40, actor.id).encode())
        return 23
    bridge.open_command_entity = activate
    bridge.command_touch = lambda p, gen, accept=False: calls.append(('touch', accept))
    def release(p, generation):
        state['active'] = False
        calls.append(('release', generation))
    bridge.close_command = release
    bridge.abandon_command = lambda gen: calls.append(('abandon', gen))
    bridge.shutdown_command_hooks = lambda: None
    return live_adapter, context, state


def open_editor(adapter):
    live, context, state = adapter
    backend, player, bridge, block, sent, calls = live
    backend.open(player, 'commandblockminecart', context)
    acknowledge(backend, player)
    return backend.sessions[player.unique_id]


def test_actual_cart_schema_uses_runtime_id_and_native_delay_range():
    assert request(15) == CAPTURED
    validate_command_entity(CAPTURED, 15)
    validate_command_entity(request(2**64-1, 99999), 2**64-1)
    for payload, target in ((CAPTURED, 16), (request(0), 0), (request(15, 100000), 15),
                            (request(15, -1), 15), (CAPTURED[:-1], 15), (CAPTURED+b'\0', 15)):
        with pytest.raises(CodecError):
            validate_command_entity(payload, target)


def test_cart_open_preserves_actor_id_and_save_preserves_runtime_payload(cart_adapter):
    live, context, state = cart_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(cart_adapter)
    opened = Open.decode(next(payload for kind, payload in sent if kind == 46))
    assert opened.actor_id == context.actor.id and session.screen_open
    assert session.window == 23 and session.entity_runtime_id == context.actor.runtime_id
    assert not any(kind in (21, 49) for kind, _ in sent)
    payload = request(context.actor.runtime_id, 5)
    event = incoming(player, 78, payload)
    backend.receive(event)
    assert not event.is_cancelled and event.payload == payload and calls[-1] == ('touch', True)
    state['save_complete'] = True
    backend.poll()
    assert not backend.sessions and not state['active'] and session.restored


@pytest.mark.parametrize('revoke', ['operator', 'creative', 'administrator', 'permission', 'guard', 'source', 'runtime'])
def test_cart_save_revalidates_live_source_and_permissions(cart_adapter, revoke):
    live, context, state = cart_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(cart_adapter)
    if revoke == 'operator': player.is_op = False
    if revoke == 'creative': player.game_mode = GameMode.SURVIVAL
    if revoke == 'administrator': player.has_permission = lambda p: p != 'remoteworkstations.admin'
    if revoke == 'permission': player.has_permission = lambda p: p != context.permission
    if revoke == 'guard': backend.add_guard('protection', lambda *args: False)
    if revoke == 'source': state['active'] = False
    if revoke == 'runtime': context.actor.runtime_id += 1
    event = incoming(player, 78, request(session.entity_runtime_id))
    backend.receive(event)
    assert event.is_cancelled and ('touch', True) not in calls
    backend.poll()
    assert not state['active']


def test_cancel_duplicate_late_save_and_reopen_cooldown(cart_adapter):
    live, context, state = cart_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(cart_adapter)
    foreign = incoming(player, 78, CAPTURED)
    backend.receive(foreign)
    assert not foreign.is_cancelled
    payload = request(session.entity_runtime_id)
    backend.receive(incoming(player, 78, payload))
    duplicate = incoming(player, 78, payload)
    backend.receive(duplicate)
    assert duplicate.is_cancelled and calls.count(('touch', True)) == 1
    backend.close(player)
    close_ack(backend, player, session)
    late = incoming(player, 78, payload)
    backend.receive(late)
    assert late.is_cancelled and not state['active']
    backend.open(player, 'commandblockminecart', context)
    backend.poll()
    assert not backend.sessions and any('retiring' in str(value) for _, value in calls)
    assert all(expiry < time.monotonic()+61 for expiry in backend._command_entities.values())


def test_cart_disconnect_releases_native_generation(cart_adapter):
    live, context, state = cart_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(cart_adapter)
    player.is_valid = False
    backend.poll()
    assert not backend.sessions and ('abandon', session.generation) in calls


def test_cart_default_disabled(cart_adapter):
    live, context, state = cart_adapter
    backend, player, bridge, block, sent, calls = live
    backend.settings['commandblockminecart'] = {}
    with pytest.raises(RuntimeError, match='disabled'):
        backend.open(player, 'commandblockminecart', context)
