from dataclasses import replace
from types import SimpleNamespace
import struct

import pytest

from endstone_remote_workstations.api import BlockContext, UIError
from endstone_remote_workstations.linked import LINKED_STATIONS, CHEMISTRY_SCREENS, validate_context, configured_context
from endstone_remote_workstations.protocol import Open
from endstone_remote_workstations.native_backend import STORAGE_SIZES
from endstone_remote_workstations.protocol import uvar
from test_native_backend import live_adapter
from test_developer_api import service


def configure(adapter, kind='chest'):
    backend, player, bridge, block, sent, calls = adapter
    context = BlockContext('Overworld', (90, -20, 40), 'example.storage')
    source = SimpleNamespace(type=LINKED_STATIONS[kind][0])
    player.dimension.get_block_at = lambda *p: source if p == context.position else block
    backend.settings['linked'] = {'enabled': True}
    backend.settings['chemistry'] = {'enabled': True}
    bridge.education_state = lambda p: {'education_features_enabled': True}
    if kind in CHEMISTRY_SCREENS:
        bridge.state = lambda p: {'ready': bridge.ready, 'window': 11}
    backend.linked_available = True
    def send(packet_id, payload):
        event = SimpleNamespace(player=player, packet_id=packet_id, payload=payload, is_cancelled=False)
        backend.project(event)
        sent.append((packet_id, event.payload))
        backend.observe('send', event)
    player.send_packet = send
    def open_linked(p, key, *position):
        assert position == context.position  # BDS receives the real source.
        bridge.ready = False
        calls.append(('linked', key))
        p.send_packet(46, Open(11, LINKED_STATIONS[key][1], *position).encode())
        if key in STORAGE_SIZES:
            p.send_packet(49, uvar(11)+uvar(STORAGE_SIZES[key]))
        return 11
    bridge.open_linked_station = open_linked
    def send_actor(p, *position):
        from bstream import BinaryStream
        from rapidnbt import CompoundTag
        from endstone_remote_workstations.protocol import svar
        tag, stream = CompoundTag(), BinaryStream()
        tag.set('disabled_slots', 256)
        if kind == 'structure':
            for axis, offset in zip('xyz', (0, -1, 0)):
                tag.set(axis+'StructureOffset', offset)
        tag.serialize(stream)
        p.send_packet(56, b''.join(svar(v) for v in position)+stream.copy_buffer())
    bridge.send_block_actor = send_actor
    bridge.lectern_state = lambda *args: {'page': 0, 'total_pages': 3}
    bridge.lectern_page = lambda p, *args: calls.append(('page', args))
    return context, source


def acknowledge(backend, player):
    backend.poll()
    pending = backend.pending[player.unique_id]
    pending.acknowledged = True
    pending.queued_at -= 0.6
    backend.poll()


@pytest.mark.parametrize('kind', [key for key in LINKED_STATIONS if key not in ('sign', 'jigsaw', 'commandblock', 'structure')])
def test_linked_open_keeps_source_context_and_projects_only_owned_position(live_adapter, kind):
    context, source = configure(live_adapter, kind)
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, kind, context)
    acknowledge(backend, player)
    session = backend.sessions[player.unique_id]
    assert session.context == context and session.position != context.position
    opened = Open.decode(next(p for i, p in sent if i == 46))
    assert (opened.x, opened.y, opened.z) == session.position and session.window == 11
    assert calls == [('linked', kind)]
    foreign = SimpleNamespace(player=player, packet_id=46, is_cancelled=False,
                              payload=Open(12, 0, *context.position).encode())
    original = foreign.payload
    backend.project(foreign)
    assert foreign.payload == original
    backend.close(player)
    bridge.ready = True
    backend.observe('receive', SimpleNamespace(player=player, packet_id=47,
        payload=bytes((session.window, LINKED_STATIONS[kind][1], 0)), is_cancelled=False))
    backend.poll()
    assert not backend.sessions


@pytest.mark.parametrize('change', ['source', 'permission', 'dimension', 'guard'])
def test_linked_lifecycle_rechecks_source_and_authorization(live_adapter, change):
    context, source = configure(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'chest', context)
    acknowledge(backend, player)
    if change == 'source': source.type = 'minecraft:air'
    if change == 'permission': player.has_permission = lambda key: key != context.permission
    if change == 'dimension': player.dimension.name = 'Nether'
    if change == 'guard': backend.add_guard('new_protection', lambda *args: False)
    backend.sessions[player.unique_id].next_state_check = 0
    backend.poll()
    assert backend.sessions[player.unique_id].closing_at is not None
    assert len([i for i, p in sent if i == 47]) == 1


def test_context_admission_fails_without_permissions_or_configuration(live_adapter):
    context, source = configure(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    with pytest.raises(RuntimeError, match='server-authorized'): backend.open(player, 'chest')
    player.has_permission = lambda key: key != 'remoteworkstations.contexts'
    with pytest.raises(RuntimeError, match='permission'):
        backend.open(player, 'chest', replace(context, permission=None))
    assert not calls and not sent


def test_double_chest_restores_both_current_states_and_detects_second_block_change(live_adapter):
    context, _ = configure(live_adapter, 'doublechest')
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'doublechest', context)
    acknowledge(backend, player)
    session = backend.sessions[player.unique_id]
    assert len(session.projection_positions) == 2
    from endstone_remote_workstations.native_backend import block_update
    second = session.projection_positions[1]
    event = SimpleNamespace(player=player, packet_id=21, is_cancelled=False, payload=block_update(second, 71))
    block.data.runtime_id = 71
    backend.observe('send', event)
    assert session.close_pending and not event.is_cancelled
    backend.poll()
    assert session.restored
    assert sent[-2:] == [(21, block_update(p, 71)) for p in session.projection_positions]


def test_single_chest_source_cannot_claim_double_chest_shape(live_adapter):
    context, _ = configure(live_adapter, 'doublechest')
    backend, player, bridge, block, sent, calls = live_adapter
    original = bridge.open_linked_station
    def wrong_shape(p, kind, *pos):
        result = original(p, kind, *pos)
        p.send_packet(49, uvar(result)+uvar(27))
        return result
    bridge.open_linked_station = wrong_shape
    backend.open(player, 'doublechest', context)
    acknowledge(backend, player)
    assert backend.sessions[player.unique_id].closing_at
    assert any('inventory size' in message for kind, message in calls if kind == 'error')


@pytest.mark.parametrize('context', [None, BlockContext('', (0, 64, 0)),
    BlockContext('Overworld', (True, 64, 0)), BlockContext('Overworld', (0, 320, 0)),
    BlockContext('Overworld', (0, 64, 0), '*'), BlockContext('Overworld', [0, 64, 0])])
def test_invalid_source_context_never_admitted(context):
    with pytest.raises(UIError): validate_context(context)


def test_configured_source_preserves_permission():
    context = configured_context({'linked': {'sources': {'chest': {
        'dimension': 'Overworld', 'position': [1, 64, 1], 'permission': 'example.storage'}}}}, 'chest')
    assert context == BlockContext('Overworld', (1, 64, 1), 'example.storage')


def test_owned_crafter_controls_translate_position_and_reject_invalid_slot(live_adapter):
    context, _ = configure(live_adapter, 'crafter')
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'crafter', context)
    acknowledge(backend, player)
    session = backend.sessions[player.unique_id]
    event = SimpleNamespace(player=player, packet_id=306, is_cancelled=False,
        payload=struct.pack('<iiiBB', *session.position, 3, 1))
    backend.receive(event)
    assert not event.is_cancelled and event.payload == struct.pack('<iiiBB', *context.position, 3, 1)
    foreign = struct.pack('<iiiBB', 0, 0, 0, 2, 1)
    event.payload = foreign
    backend.receive(event)
    assert event.payload == foreign and not event.is_cancelled
    event.payload = struct.pack('<iiiBB', *session.position, 9, 1)
    backend.receive(event)
    assert event.is_cancelled and session.close_pending


def test_crafter_native_state_is_copied_to_display_and_refreshed_after_toggle(live_adapter):
    context, _ = configure(live_adapter, 'crafter')
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'crafter', context)
    acknowledge(backend, player)
    from endstone_remote_workstations.protocol import Reader
    actor = next(payload for packet, payload in sent if packet == 56)
    reader = Reader(actor)
    session = backend.sessions[player.unique_id]
    assert tuple(reader.svar() for _ in range(3)) == session.position
    event = SimpleNamespace(player=player, packet_id=306, is_cancelled=False,
        payload=struct.pack('<iiiBB', *session.position, 8, 0))
    backend.receive(event)
    before = sum(packet == 56 for packet, payload in sent)
    backend.poll()
    assert sum(packet == 56 for packet, payload in sent) == before+1


def test_actor_projection_preserves_real_native_disabled_slot_fixture():
    from endstone_remote_workstations.linked import project_actor_data
    from endstone_remote_workstations.protocol import Reader
    from bstream import ReadOnlyBinaryStream
    from rapidnbt import CompoundTag
    payload = bytes.fromhex('0cc8011e0a0003186372616674696e675f7469636b735f72656d61696e696e6700020e64697361626c65645f736c6f7473000100')
    projected = project_actor_data(payload, (6, 100, 15), (23, 101, -4))
    reader = Reader(projected)
    assert tuple(reader.svar() for _ in range(3)) == (23, 101, -4)
    assert projected[reader.offset:] == payload[4:]  # All original crafter NBT retained byte-for-byte.


def test_beacon_projects_only_real_base_and_closes_when_it_changes(live_adapter):
    context, source = configure(live_adapter, 'beacon')
    backend, player, bridge, air, sent, calls = live_adapter
    iron = SimpleNamespace(type='minecraft:iron_block', data=SimpleNamespace(runtime_id=81))
    base = {(context.position[0]+dx, context.position[1]-1, context.position[2]+dz): iron
            for dx in range(-1, 2) for dz in range(-1, 2)}
    player.dimension.get_block_at = lambda *p: source if p == context.position else base.get(p, air)
    backend.open(player, 'beacon', context)
    acknowledge(backend, player)
    session = backend.sessions[player.unique_id]
    assert len(session.projection_positions) == 10  # Beacon plus the actual 3x3 base.
    assert len(session.terrain_sources) == 34  # Also watch the first invalid second tier.
    base.pop(next(iter(base)))
    session.next_state_check = 0
    backend.poll()
    assert session.closing_at and session.restored


def test_cancelled_actor_copy_does_not_claim_display_ready(live_adapter):
    context, _ = configure(live_adapter, 'crafter')
    backend, player, bridge, block, sent, calls = live_adapter
    bridge.send_block_actor = lambda *args: None  # Another listener cancels the native update.
    backend.open(player, 'crafter', context)
    backend.poll()
    assert not backend.sessions and not backend.pending
    assert not any(packet == 46 for packet, payload in sent)
    assert any('changed or cancelled' in message for kind, message in calls if kind == 'error')


def test_dependency_linked_ticket_callbacks_and_cancel_are_scoped(service):
    svc, ui, other, owner, _, player, _, adapter = service
    context, _ = configure(adapter)
    native = adapter[0]
    callbacks = []
    ticket = ui.open_linked(player, 'chest', context,
                            on_open=lambda info: callbacks.append(info.state),
                            on_close=lambda info: callbacks.append(info.state))
    svc.poll()
    acknowledge(native, player)
    svc.poll()
    assert ticket.info.state == 'open' and callbacks == ['open']
    other.dispose()
    svc.poll()
    assert ticket.info.state == 'open'
    ticket.cancel()
    svc.poll()
    assert ticket.info.state == 'cancelled' and callbacks == ['open', 'cancelled']
    assert native.sessions[player.unique_id].closing_at
