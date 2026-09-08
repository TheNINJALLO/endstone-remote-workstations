from types import SimpleNamespace
import struct

import pytest

from endstone_remote_workstations.entity_display import EntityDisplays, delta_position, project_motion, actor_header
from endstone_remote_workstations.protocol import CodecError, Reader, svar, uvar
from test_entity_native import configure
from test_native_backend import live_adapter
from test_linked_native import acknowledge


def setup_equipment(adapter, known=False, kind='horse', size=2):
    context, actor = configure(adapter, kind)
    backend, player, bridge, block, sent, calls = adapter
    actor.runtime_id = 7
    actor.is_on_ground = True
    actor.location = SimpleNamespace(x=90.5, y=70.0, z=40.5)
    kind = actor.type.encode()
    packet = svar(actor.id, 64)+uvar(actor.runtime_id, 64)+uvar(len(kind))+kind+struct.pack('<fff', 90.5, 70, 40.5)+b'\x00'
    bridge.send_entity_actor = lambda p, a, k: p.send_packet(13, packet)
    def open_equipment(p, a, k):
        bridge.ready = False
        p.send_packet(81, bytes((11, 12))+svar(0)+svar(actor.id, 64)+b'\x0a\x00\x00')
        p.send_packet(49, uvar(11)+uvar(size))
        return 11
    bridge.open_entity = open_equipment
    if known:
        player.send_packet(13, packet)
        sent.clear()
    return context, actor, packet


@pytest.mark.parametrize('kind,size', [('horse', 2), ('donkey', 1), ('donkey', 16), ('mule', 1),
    ('mule', 16), ('llama', 4), ('llama', 7), ('llama', 10), ('llama', 13), ('llama', 16),
    ('traderllama', 1), ('traderllama', 4), ('camel', 1), ('camelhusk', 1), ('zombiehorse', 2),
    ('nautilus', 2), ('zombienautilus', 2)])
def test_equipment_layouts_retain_native_actor_and_open_handshake(live_adapter, kind, size):
    context, actor, _ = setup_equipment(live_adapter, kind=kind, size=size)
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, kind, context)
    acknowledge(backend, player)
    session = backend.sessions[player.unique_id]
    assert session.window == 11 and session.shape_count == size and not session.shape_invalid
    assert session.entity_id == actor.id
    assert not any(i in (21, 46) for i, p in sent)


@pytest.mark.parametrize('size', [0, 2, 15, 17, 54])
def test_invalid_donkey_capacity_rejects_open(live_adapter, size):
    context, actor, _ = setup_equipment(live_adapter, kind='donkey', size=size)
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'donkey', context)
    acknowledge(backend, player)
    assert backend.sessions[player.unique_id].closing_at
    assert any(i == 47 for i, p in sent)


def test_equipment_attachment_change_closes_existing_view(live_adapter):
    context, actor, _ = setup_equipment(live_adapter, kind='donkey', size=16)
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'donkey', context)
    acknowledge(backend, player)
    player.send_packet(49, uvar(11)+uvar(1))
    backend.poll()
    assert backend.sessions[player.unique_id].closing_at


@pytest.mark.parametrize('arrives', [True, False])
def test_native_equipment_contents_may_arrive_after_open_returns(live_adapter, arrives):
    context, actor, _ = setup_equipment(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    def deferred(p, a, k):
        bridge.ready = False
        p.send_packet(81, bytes((11, 12))+svar(0)+svar(actor.id, 64)+b'\x0a\x00\x00')
        return 11
    bridge.open_entity = deferred
    backend.open(player, 'horse', context)
    acknowledge(backend, player)
    session = backend.sessions[player.unique_id]
    assert session.shape_deadline is not None and session.closing_at is None
    if arrives:
        player.send_packet(49, uvar(11)+uvar(2))
        assert session.shape_deadline is None and session.shape_seen
        backend.poll()
        assert session.closing_at is None
    else:
        session.shape_deadline -= 3
        backend.poll()
        assert session.closing_at and any(i == 47 for i, p in sent)


def test_live_delta_fixture_and_optional_rotation_preservation():
    fixture = bytes.fromhex('07010000184100010000e44100010001ca01010000')
    projected = project_motion(111, fixture, (1.5, -20.0, 3.5))
    reader = Reader(projected)
    assert reader.uvar(64) == 7
    for expected in (1.5, -20.0, 3.5):
        assert reader.boolean()
        assert struct.unpack('<f', reader.raw(4))[0] == expected
    assert projected[reader.offset:] == bytes.fromhex('00010001ca01010000')
    assert project_motion(111, delta_position(7, (10, 20, 30), True), (1, 2, 3)) == delta_position(7, (1, 2, 3), True)
    with pytest.raises(CodecError): project_motion(111, b'\x07\x02', (0, 0, 0))
    with pytest.raises(CodecError): project_motion(111, fixture+b'\x00', (0, 0, 0))


def test_unknown_client_entity_uses_actual_source_data_and_removes_only_its_view(live_adapter):
    context, actor, native_packet = setup_equipment(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'horse', context)
    acknowledge(backend, player)
    session = backend.sessions[player.unique_id]
    assert session.window == 11 and session.entity_seen and session.entity_added and not session.restored
    payload = next(p for i, p in sent if i == 13)
    identity, runtime, kind, offset = actor_header(payload)
    assert (identity, runtime, kind) == (actor.id, actor.runtime_id, actor.type)
    assert payload[offset+12:] == native_packet[offset+12:]  # Native metadata unchanged.
    assert struct.unpack('<fff', payload[offset:offset+12]) == session.entity_position
    assert not any(i in (21, 46) for i, p in sent)
    assert actor.id not in backend.entity_displays.visible[player.unique_id].actors
    backend.close(player)
    assert [p for i, p in sent if i == 14] == [svar(actor.id, 64)]
    backend.close(player)
    assert len([i for i, p in sent if i == 14]) == 1


def test_existing_entity_restores_current_position_without_duplicate_add_or_remove(live_adapter):
    context, actor, _ = setup_equipment(live_adapter, known=True)
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'horse', context)
    acknowledge(backend, player)
    session = backend.sessions[player.unique_id]
    assert not session.entity_added
    actor.location.x = 150.5
    motion = delta_position(7, (150.5, 70, 40.5), True)
    player.send_packet(111, motion)
    assert sent[-1][1] == delta_position(7, session.entity_position, True)
    backend.forget(player)  # Also restores if BDS closed without a close packet.
    assert sent[-1] == (111, motion)
    assert not any(i in (13, 14, 21) for i, p in sent)


def test_foreign_motion_and_equipment_packets_are_not_claimed(live_adapter):
    context, actor, _ = setup_equipment(live_adapter, known=True)
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'horse', context)
    backend.poll()
    session = backend.sessions[player.unique_id]
    payload = delta_position(8, (1, 2, 3), False)
    player.send_packet(111, payload)
    assert sent[-1] == (111, payload)
    session.activating = True
    player.send_packet(81, bytes((22, 12))+svar(0)+svar(actor.id+1, 64)+b'\x00')
    assert session.window is None


def test_world_tracking_transition_keeps_the_real_entity(live_adapter):
    context, actor, native_packet = setup_equipment(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'horse', context)
    acknowledge(backend, player)
    player.send_packet(13, native_packet)  # World starts tracking the real source.
    session = backend.sessions[player.unique_id]
    assert not session.entity_added and session.close_pending
    backend.poll()
    assert session.restored and session.closing_at
    assert not any(i == 14 for i, p in sent)


def test_visibility_capacity_fails_closed_without_eviction(live_adapter):
    context, actor, native_packet = setup_equipment(live_adapter, known=True)
    backend, player, bridge, block, sent, calls = live_adapter
    backend.entity_displays.limit = 1
    player.send_packet(13, svar(actor.id+1, 64)+native_packet[len(svar(actor.id, 64)):])
    state = backend.entity_displays.visible[player.unique_id]
    assert not state.complete and actor.id in state.actors
    with pytest.raises(RuntimeError, match='visibility'):
        backend.open(player, 'horse', context)
    assert not backend.pending


def test_reload_requires_fresh_client_visibility(live_adapter):
    backend, player, *_ = live_adapter
    backend.server.online_players = [player]
    display = EntityDisplays(backend)
    assert display.reason(player)
    display.disconnect(player.unique_id)
    assert display.reason(player) is None


@pytest.mark.parametrize('change', ['expired', 'owner_revoked'])
def test_equipment_source_checks_continue_while_open(live_adapter, change):
    context, actor, _ = setup_equipment(live_adapter, known=True)
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'horse', context)
    acknowledge(backend, player)
    if change == 'expired':
        actor.is_valid = False
    else:
        def denied(*args): raise RuntimeError('Native owner mismatch')
        bridge.entity_state = denied
    backend.sessions[player.unique_id].next_state_check = 0
    backend.poll()
    assert backend.sessions[player.unique_id].closing_at is not None


def test_cancelled_native_actor_data_cannot_complete_open(live_adapter):
    context, actor, _ = setup_equipment(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    bridge.send_entity_actor = lambda *args: None
    backend.open(player, 'horse', context)
    backend.poll()
    assert not backend.pending and not any(i == 81 for i, p in sent)
