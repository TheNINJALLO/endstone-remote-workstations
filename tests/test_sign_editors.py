from dataclasses import replace
from types import SimpleNamespace
import struct

import pytest

from endstone_remote_workstations.api import SignContext, UIError
from endstone_remote_workstations.linked import validate_context
from endstone_remote_workstations.protocol import Reader, svar
from test_native_backend import live_adapter
from test_linked_native import configure, acknowledge


def sign_packet(position, text='fixture'):
    from rapidnbt import CompoundTag
    from bstream import BinaryStream
    tag, stream = CompoundTag(), BinaryStream()
    tag.set('id', 'Sign')
    front, back = CompoundTag(), CompoundTag()
    front.set('Text', text)
    back.set('Text', '')
    tag.set('FrontText', front)
    tag.set('BackText', back)
    for axis, value in zip('xyz', position):
        tag.set(axis, value)
    tag.serialize(stream)
    return b''.join(svar(v) for v in position)+stream.copy_buffer()


@pytest.fixture
def sign_adapter(live_adapter):
    context, source = configure(live_adapter, 'sign')
    backend, player, bridge, block, sent, calls = live_adapter
    context = SignContext(context.dimension, context.position, context.permission, False)
    source.data = SimpleNamespace(runtime_id=91)
    backend.settings['signs'] = {'enabled': True}
    state = dict(waxed=False, locked=False, occupied=False, owned=False, active=False,
                 can_interact=True, open_pending=False)
    bridge.sign_state = lambda *args: dict(state)
    def open_sign(p, x, y, z, front, generation):
        assert (x, y, z) == context.position and front is False
        assert generation == backend.sessions[p.unique_id].generation
        state.update(active=True, locked=True, occupied=True, owned=True, open_pending=True)
        calls.append(('sign-open', front))
    def close_sign(p, generation):
        calls.append(('sign-release', generation))
        state.update(active=False, locked=False, occupied=False, owned=False)
    bridge.open_sign = open_sign
    bridge.close_sign = close_sign
    bridge.sign_touch = lambda p, generation, accept=False: calls.append(('sign-touch', accept))
    bridge.abandon_sign = lambda generation: calls.append(('sign-abandon', generation))
    bridge.shutdown_sign_hooks = lambda: calls.append(('sign-hooks-off', True))
    bridge.send_block_actor = lambda p, *position: p.send_packet(56, sign_packet(position))
    def emit_open():
        state['open_pending'] = False
        player.send_packet(303, b''.join(svar(v) for v in context.position)+b'\x00')
    return live_adapter, context, source, state, emit_open


def open_editor(adapter):
    live, context, source, state, emit_open = adapter
    backend, player, bridge, block, sent, calls = live
    backend.open(player, 'sign', context)
    acknowledge(backend, player)
    session = backend.sessions[player.unique_id]
    assert session.window is None and not session.screen_open
    emit_open()
    assert session.screen_open
    return session


def incoming(player, packet, payload):
    return SimpleNamespace(player=player, packet_id=packet, payload=payload, is_cancelled=False)


def close_ack(backend, player, session):
    backend.observe('receive', incoming(player, 115, struct.pack('<QB', session.close_nonce*1_000_000, 1)))
    backend.poll()


def test_deferred_sign_open_has_no_inventory_window_and_preserves_back_side(sign_adapter):
    live, context, source, state, emit_open = sign_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(sign_adapter)
    backend.poll()
    assert player.unique_id in backend.sessions and bridge.ready
    assert not any(packet == 46 for packet, _ in sent)
    reader = Reader(next(payload for packet, payload in sent if packet == 303))
    assert tuple(reader.svar() for _ in range(3)) == session.position
    assert reader.boolean() is False
    reader.end()
    assert (21, b''.join(svar(v) for v in session.position)+bytes((91, 2, 0))) in sent


def test_owned_save_changes_only_coordinates_and_waits_for_native_unlock(sign_adapter):
    live, context, source, state, emit_open = sign_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(sign_adapter)
    event = incoming(player, 56, sign_packet(session.position, 'retained text'))
    backend.receive(event)
    assert not event.is_cancelled
    assert event.payload == sign_packet(context.position, 'retained text')
    assert ('sign-touch', True) in calls
    backend.poll()
    assert player.unique_id in backend.sessions
    state.update(locked=False, occupied=False, owned=False)
    backend.poll()
    assert not backend.sessions and ('sign-release', session.generation) in calls
    assert not any(packet == 47 for packet, _ in sent)


def test_cancel_revokes_native_before_restoring_and_rejects_late_save(sign_adapter):
    live, context, source, state, emit_open = sign_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(sign_adapter)
    backend.close(player)
    assert not state['active'] and not state['locked']
    late = incoming(player, 56, sign_packet(session.position))
    backend.receive(late)
    assert late.is_cancelled and not any(call == ('sign-touch', True) for call in calls)
    assert not any(packet == 47 for packet, _ in sent)
    close_ack(backend, player, session)
    assert not backend.sessions
    retired = incoming(player, 56, sign_packet(session.position))
    backend.receive(retired)
    assert retired.is_cancelled


@pytest.mark.parametrize('change', ['permission', 'guard', 'wax', 'source', 'dimension', 'native_identity'])
def test_sign_lifecycle_revokes_invalid_contexts(sign_adapter, change):
    live, context, source, state, emit_open = sign_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(sign_adapter)
    if change == 'permission':
        player.has_permission = lambda permission: permission != context.permission
    elif change == 'guard':
        backend.add_guard('fixture-protection', lambda *args: False)
    elif change == 'wax':
        state['waxed'] = True
    elif change == 'source':
        source.type = 'minecraft:air'
    elif change == 'dimension':
        player.dimension.name = 'Nether'
    else:
        state['active'] = False
    session.next_state_check = 0
    backend.poll()
    assert session.closing_at is not None
    assert ('sign-release', session.generation) in calls


def test_duplicate_and_guessed_source_saves_do_not_gain_remote_authority(sign_adapter):
    live, context, source, state, emit_open = sign_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(sign_adapter)
    guessed = incoming(player, 56, sign_packet(context.position))
    backend.receive(guessed)
    assert guessed.is_cancelled
    foreign = incoming(player, 56, sign_packet((1, 2, 3)))
    original = foreign.payload
    backend.receive(foreign)
    assert not foreign.is_cancelled and foreign.payload == original
    valid = incoming(player, 56, sign_packet(session.position))
    backend.receive(valid)
    duplicate = incoming(player, 56, sign_packet(session.position))
    backend.receive(duplicate)
    assert not valid.is_cancelled and duplicate.is_cancelled
    assert calls.count(('sign-touch', True)) == 1


@pytest.mark.parametrize('bad', ['malformed', 'oversize', 'before_open', 'permission'])
def test_unaccepted_sign_payload_never_arms_native_hook(sign_adapter, bad):
    live, context, source, state, emit_open = sign_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(sign_adapter)
    payload = sign_packet(session.position)
    if bad == 'malformed':
        payload = b''.join(svar(v) for v in session.position)+b'\x0a'
    elif bad == 'oversize':
        payload += b'\0'*16384
    elif bad == 'before_open':
        session.screen_open = False
    else:
        player.has_permission = lambda permission: permission != context.permission
    event = incoming(player, 56, payload)
    backend.receive(event)
    assert event.is_cancelled and session.close_pending
    assert ('sign-touch', True) not in calls


@pytest.mark.parametrize('condition', ['disabled', 'waxed', 'locked', 'spectator'])
def test_sign_admission_refuses_without_touching_source(sign_adapter, condition):
    live, context, source, state, emit_open = sign_adapter
    backend, player, bridge, block, sent, calls = live
    if condition == 'disabled':
        backend.settings['signs']['enabled'] = False
    elif condition == 'waxed':
        state['waxed'] = True
    elif condition == 'locked':
        state.update(locked=True, occupied=True)
    else:
        state['can_interact'] = False
    with pytest.raises(RuntimeError):
        backend.open(player, 'sign', context)
    assert not sent and not calls


def test_disconnected_sign_lease_is_abandoned_without_native_player_access(sign_adapter):
    live, context, source, state, emit_open = sign_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(sign_adapter)
    player.is_valid = False
    backend.poll()
    assert not backend.sessions
    assert ('sign-abandon', session.generation) in calls


def test_sign_side_requires_a_boolean():
    with pytest.raises(UIError, match='boolean'):
        validate_context(SignContext('Overworld', (0, 64, 0), front=1))


@pytest.mark.parametrize('mutation', ['trailing', 'truncated', 'root_name', 'wrong_coordinates', 'wrong_id', 'extra_field'])
def test_sign_codec_rejects_noncanonical_source_structures(mutation):
    from endstone_remote_workstations.signs import validate_sign_payload
    from endstone_remote_workstations.protocol import CodecError
    payload = sign_packet((1, 2, 3))
    if mutation == 'trailing':
        payload += b'\0'
    elif mutation == 'truncated':
        payload = payload[:-1]
    elif mutation == 'root_name':
        payload = payload[:4]+b'\x01x'+payload[5:]
    elif mutation == 'wrong_coordinates':
        payload = bytes((0,))+payload[1:]
    elif mutation == 'wrong_id':
        payload = payload.replace(b'Sign', b'Fake')
    else:
        payload = payload[:-1]+b'\x01\x01Z\x00\x00'
    with pytest.raises(CodecError):
        validate_sign_payload(payload, (1, 2, 3))
