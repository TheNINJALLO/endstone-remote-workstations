from types import SimpleNamespace
import struct

import pytest

from endstone_remote_workstations.protocol import CodecError
from endstone_remote_workstations.riding import decode_link, encode_link, native_link
from test_equipment_display import setup_equipment
from test_native_backend import live_adapter
from test_linked_native import acknowledge
from test_trade_display import trade_packet


CAPTURED = bytes.fromhex('fdffffffff0ffdffffffbf0b01000100000000')


def setup_riding(adapter, *, known=True, kind='horse'):
    context, actor, _ = setup_equipment(adapter, known=known, kind=kind)
    backend, player, bridge, block, sent, calls = adapter
    link = dict(vehicle_id=-2222, passenger_id=actor.id, type=1, immediate=False,
                passenger_initiated=True, angular_velocity=0.0,
                vehicle_runtime_id=99, passenger_runtime_id=actor.runtime_id)
    current = [link]
    bridge.entity_mount_link = lambda p, a: None if current[0] is None else dict(current[0])
    bridge.entity_mount_state = lambda p, a: None if current[0] is None else {
        k: current[0][k] for k in ('vehicle_id', 'vehicle_runtime_id')}
    backend.entity_displays._visibility(player).actors[-2222] = (99, 'minecraft:boat')
    if kind in ('villager', 'wanderingtrader'):
        def open_trade(p, a, k):
            calls.append(('trade', a.id))
            bridge.ready = False
            p.send_packet(80, trade_packet(a.id))
            return 11
        bridge.open_entity = open_trade
    backend.open(player, kind, context)
    return context, actor, link, current


def link_packets(sent):
    return [decode_link(raw) for packet, raw in sent if packet == 41]


def test_exact_native_link_capture():
    assert decode_link(CAPTURED) == (-274877906943, -197568495615, 1, False, True, 0.0)


@pytest.mark.parametrize('payload', [b'', CAPTURED[:-1], CAPTURED+b'\0',
    CAPTURED[:-7]+b'\x03'+CAPTURED[-6:], CAPTURED[:-6]+b'\x02'+CAPTURED[-5:],
    CAPTURED[:-4]+struct.pack('<f', float('nan')), b'\0'*33])
def test_malformed_link_rejected(payload):
    with pytest.raises(CodecError):
        decode_link(payload)


@pytest.mark.parametrize('kind', ['horse', 'villager', 'wanderingtrader'])
def test_remote_detach_and_restore_preserve_native_mount(live_adapter, kind):
    context, actor, original, current = setup_riding(live_adapter, kind=kind)
    backend, player, bridge, block, sent, calls = live_adapter
    acknowledge(backend, player)
    session = backend.sessions[player.unique_id]
    assert session.window == 11 and session.entity_mount == original
    assert link_packets(sent) == [(-2222, actor.id, 0, True, True, 0.0)]
    assert next(i for i, (packet, _) in enumerate(sent) if packet == 41) < next(i for i, (packet, _) in enumerate(sent) if packet == 111)
    backend.close(player)
    assert link_packets(sent)[-1] == (-2222, actor.id, 1, True, True, 0.0)
    assert session.restored and current[0] == original


def test_private_entity_removed_without_leaving_a_phantom_mount(live_adapter):
    _, _, _, current = setup_riding(live_adapter, known=False)
    backend, player, bridge, block, sent, calls = live_adapter
    acknowledge(backend, player)
    backend.close(player)
    assert len(link_packets(sent)) == 1
    assert [packet for packet, _ in sent].count(14) == 1
    assert current[0]['vehicle_id'] == -2222


@pytest.mark.parametrize('change', ['unloaded_vehicle', 'reused_vehicle', 'source_id', 'source_runtime', 'expired_source', 'dimension'])
def test_cleanup_never_attaches_to_stale_or_foreign_entity(live_adapter, change):
    _, actor, _, current = setup_riding(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    acknowledge(backend, player)
    visible = backend.entity_displays.visible[player.unique_id].actors
    if change == 'unloaded_vehicle': visible.pop(-2222)
    if change == 'reused_vehicle': visible[-2222] = (100, 'minecraft:boat')
    if change == 'source_id': actor.id -= 1
    if change == 'source_runtime': actor.runtime_id += 1
    if change == 'expired_source': actor.is_valid = False
    if change == 'dimension': player.dimension = SimpleNamespace(name='Nether')
    backend.close(player)
    assert len(link_packets(sent)) == 1


@pytest.mark.parametrize('change', ['dismount', 'vehicle_id', 'vehicle_runtime'])
def test_mount_change_during_readiness_prevents_native_open(live_adapter, change):
    _, actor, _, current = setup_riding(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    backend.poll()
    if change == 'dismount': current[0] = None
    if change == 'vehicle_id': current[0] = dict(current[0], vehicle_id=-3333)
    if change == 'vehicle_runtime': current[0] = dict(current[0], vehicle_runtime_id=100)
    acknowledge(backend, player)
    assert not any(packet in (80, 81) for packet, _ in sent)


def test_cleanup_restores_current_native_vehicle_after_reparenting(live_adapter):
    _, actor, _, current = setup_riding(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    acknowledge(backend, player)
    current[0] = dict(current[0], vehicle_id=-3333, vehicle_runtime_id=100, type=2)
    backend.entity_displays.visible[player.unique_id].actors[-3333] = (100, 'minecraft:boat')
    session = backend.sessions[player.unique_id]
    assert not backend.entity_displays.mount_matches(player, session)
    backend.close(player)
    assert link_packets(sent)[-1] == (-3333, actor.id, 2, True, True, 0.0)


def test_dismounted_source_never_restores_previous_mount(live_adapter):
    _, _, _, current = setup_riding(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    acknowledge(backend, player)
    current[0] = None
    backend.close(player)
    assert len(link_packets(sent)) == 1


def test_ownership_revocation_does_not_block_read_only_view_restoration(live_adapter):
    _, actor, _, _ = setup_riding(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    acknowledge(backend, player)
    def revoked(*args):
        raise RuntimeError('Owner revoked')
    bridge.entity_state = revoked
    backend.close(player)
    assert link_packets(sent)[-1][0:3] == (-2222, actor.id, 1)


def test_native_close_without_managed_session_restores_link(live_adapter):
    _, actor, _, _ = setup_riding(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    acknowledge(backend, player)
    session = backend.sessions[player.unique_id]
    backend.forget(player)
    assert link_packets(sent)[-1][0:3] == (-2222, actor.id, 1)
    assert session.restored and not backend.sessions and not backend.entity_displays.link_sends


def test_config_revocation_still_observes_synchronous_cleanup_delivery(live_adapter):
    _, actor, _, _ = setup_riding(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    acknowledge(backend, player)
    backend.settings['entities']['enabled'] = False
    backend.forget(player)
    assert link_packets(sent)[-1][0:3] == (-2222, actor.id, 1)
    assert not backend.entity_displays.link_sends


@pytest.mark.parametrize('changed', [False, True])
def test_changed_or_cancelled_detach_never_claims_delivery(live_adapter, changed):
    _, actor, original, _ = setup_riding(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    acknowledge(backend, player)
    session = backend.sessions[player.unique_id]
    def intercept(packet, payload):
        event = SimpleNamespace(player=player, packet_id=packet,
                                payload=payload+b'\0' if changed else payload, is_cancelled=not changed)
        backend.observe('send', event)
    player.send_packet = intercept
    with pytest.raises(RuntimeError, match='changed or cancelled'):
        backend.entity_displays._send_link(player, session, original, detached=True)
    assert session.entity_link_sending is None and not backend.entity_displays.link_sends


@pytest.mark.parametrize('change', ['duplicate', 'foreign_passenger', 'new_vehicle', 'dismount'])
def test_native_link_updates_are_scoped_to_owned_passenger(live_adapter, change):
    _, actor, original, _ = setup_riding(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    acknowledge(backend, player)
    link = dict(original)
    if change == 'foreign_passenger': link['passenger_id'] -= 1
    if change == 'new_vehicle': link['vehicle_id'] = -3333
    raw = encode_link(link, detached=change == 'dismount')
    event = SimpleNamespace(player=player, packet_id=41, payload=raw, is_cancelled=False)
    backend.project(event)
    assert event.payload == raw
    assert event.is_cancelled is (change == 'duplicate')
    assert backend.sessions[player.unique_id].close_pending is (change in ('new_vehicle', 'dismount'))


@pytest.mark.parametrize('field,value', [('passenger_id', 55), ('passenger_runtime_id', 8),
    ('vehicle_id', -1), ('vehicle_runtime_id', 0), ('type', 0), ('type', True),
    ('immediate', 1), ('passenger_initiated', 0), ('angular_velocity', float('inf'))])
def test_native_link_identity_and_abi_shape_are_checked(live_adapter, field, value):
    _, actor, original, _ = setup_riding(live_adapter)
    with pytest.raises(CodecError):
        native_link(dict(original, **{field: value}), actor.id, actor.runtime_id)
