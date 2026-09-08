from dataclasses import replace
from types import SimpleNamespace

import pytest

from endstone_remote_workstations.model import Item, Snapshot, Request, Action, Ref, Slot, Rejected
from endstone_remote_workstations.packet_inventory import ReservedInventory, Conflict
from endstone_remote_workstations.packet_backend import PacketBackend
from endstone_remote_workstations.protocol import ItemDescriptor, inventory_content, inventory_slot
from endstone_remote_workstations.protocol import uvar, svar
from endstone_remote_workstations.packet_codec import decode_responses
from endstone_remote_workstations.packet_items import read_content, read_slot
from test_native_backend import live_adapter


def setup_engine():
    items = (Item('minecraft:diamond_sword', 1, 1, 7, b'complete-NBT'),) + (None,) * 35
    state = Snapshot('epoch', 0, items, (None,) * 27, next_net_id=8)
    real, writes = [state], []
    def commit(before, after):
        assert real[0] == before
        writes.append(after)
        real[0] = after
    return ReservedInventory(state, lambda: real[0], commit), real, writes


def move(engine, request_id, source, destination, net_id=7):
    return Request('epoch', request_id, engine.state.version, (
        Action('place', Ref(Slot(*source), net_id), Ref(Slot(*destination), 0), 1),))


def test_cursor_reservation_close_preserves_original_real_items():
    engine, real, writes = setup_engine()
    engine.apply(move(engine, -1, ('player', 0), ('cursor', 0)))
    assert engine.state.cursor[0].metadata == b'complete-NBT'
    assert real[0].player[0] is not None and not writes
    engine.close()
    assert engine.state.cursor == (None,) and engine.state.player[0] is not None


def test_placing_cursor_commits_once_to_real_ender_and_replay_is_idempotent():
    engine, real, writes = setup_engine()
    engine.apply(move(engine, -1, ('player', 0), ('cursor', 0)))
    request = move(engine, -3, ('cursor', 0), ('storage', 0))
    first = engine.apply(request)
    assert real[0].player[0] is None and real[0].storage[0].metadata == b'complete-NBT'
    assert engine.apply(replace(request, version=engine.state.version)) is first
    assert len(writes) == 1
    with pytest.raises(Rejected, match='replay'):
        engine.apply(move(engine, -3, ('storage', 0), ('player', 1)))


@pytest.mark.parametrize('area', ['player', 'storage'])
def test_external_write_invalidates_reservation_without_overwriting_it(area):
    engine, real, writes = setup_engine()
    engine.apply(move(engine, -1, ('player', 0), ('cursor', 0)))
    changed = list(getattr(real[0], area))
    changed[2] = Item('minecraft:stone', 3, 64, 20)
    real[0] = replace(real[0], **{area: tuple(changed)}, next_net_id=21)
    with pytest.raises(Conflict):
        engine.apply(move(engine, -3, ('cursor', 0), ('storage', 0)))
    assert not writes and getattr(real[0], area)[2].count == 3


def test_late_invalid_action_and_drop_have_no_partial_commit():
    engine, real, writes = setup_engine()
    action = move(engine, -1, ('player', 0), ('storage', 0)).actions[0]
    invalid = Action('place', Ref(Slot('player', 99), 0), Ref(Slot('cursor', 0), 0), 1)
    with pytest.raises(Rejected):
        engine.apply(Request('epoch', -1, 0, (action, invalid)))
    with pytest.raises(Rejected, match='drop'):
        engine.apply(Request('epoch', -3, 0, (Action('drop', action.source, None, 1),)))
    assert not writes and real[0].player[0] is not None


def test_native_inventory_cache_retains_server_ids_and_ignores_owned_overlay(live_adapter):
    native, player, _, _, _, _ = live_adapter
    native.plugin._native = native
    backend = PacketBackend(native.plugin, {'enabled': True})
    item = ItemDescriptor(302, 1, 0, 21, 0, b'complete-wire-NBT')
    content = inventory_content(0, [item] + [ItemDescriptor()] * 35, role=12)
    event = SimpleNamespace(player=player, packet_id=49, payload=content, is_cancelled=False)
    backend.outgoing(event)
    assert backend.actual[player.unique_id][0] == item and not event.is_cancelled
    backend.sessions[player.unique_id] = SimpleNamespace(reason=None)
    changed = replace(item, net_id=35)
    event = SimpleNamespace(player=player, packet_id=50,
        payload=inventory_slot(0, 0, changed, role=12), is_cancelled=False)
    backend.outgoing(event)
    assert event.is_cancelled and backend.actual[player.unique_id][0] == changed
    backend.sending = True
    event.is_cancelled = False
    event.payload = inventory_slot(0, 0, replace(item, net_id=1000000), role=12)
    backend.outgoing(event)
    assert backend.actual[player.unique_id][0].net_id == 35
    assert read_content(content)[1][0] == item
    assert read_slot(inventory_slot(0, 0, item))[2] == item


def test_only_owned_requests_are_cancelled_and_queue_is_bounded(live_adapter):
    native, player, _, _, _, _ = live_adapter
    native.plugin._native = native
    backend = PacketBackend(native.plugin, {})
    event = SimpleNamespace(player=player, packet_id=147, payload=b'x', is_cancelled=False)
    backend.receive(event)
    assert not event.is_cancelled

    from collections import deque
    session = SimpleNamespace(window=70, nonce=7, opened=True, reason=None, queue=deque(), binding=SimpleNamespace())
    backend.sessions[player.unique_id] = session
    import struct
    backend.receive(SimpleNamespace(player=player, packet_id=115,
        payload=struct.pack('<QB', 7000000, 1), is_cancelled=False))
    assert session.acknowledged
    for _ in range(33):
        event.is_cancelled = False
        backend.receive(event)
        assert event.is_cancelled
    assert len(session.queue) == 32 and session.reason == 'request_limit'
    event.packet_id, event.payload, event.is_cancelled = 47, b'\x45\x00\x00', False
    backend.receive(event)
    assert not event.is_cancelled


def test_menu_icon_click_is_rejected_as_transfer_then_selected_once(live_adapter):
    import struct
    native, player, _, _, sent, _ = live_adapter
    native.plugin._native = native
    backend = PacketBackend(native.plugin, {})
    state = Snapshot('icons', 0, (None,) * 36,
        (Item('minecraft:diamond', 1, 64, 8),) + (None,) * 26, next_net_id=9)
    writes = []
    engine = ReservedInventory(state, lambda: state, lambda *args: writes.append(args))
    binding = SimpleNamespace(icons=[object()], name=lambda i: '',
        descriptor=lambda i: ItemDescriptor(5, 1, net_id=i.net_id) if i else ItemDescriptor())
    session = SimpleNamespace(engine=engine, binding=binding, buttons={0: 'example:hello'},
        reason=None, selection=None, window=70)
    source = b'\x07\x00\x00' + struct.pack('<i', 8)
    destination = b'\x3b\x00\x00' + bytes(4)
    payload = uvar(1) + svar(-1) + b'\x01\x00\x00\x01' + source + destination + b'\x00' + bytes.fromhex('ffffffff')
    backend._requests(player, session, payload)
    assert session.selection == 'example:hello' and session.reason == 'selected'
    assert not writes and engine.state == state
    result = decode_responses(next(data for packet, data in sent if packet == 148))
    assert result[0].result != 0
    backend._requests(player, session, payload)
    assert not writes


def test_forced_close_calls_current_bds_refresh_instead_of_cached_items(live_adapter):
    from collections import deque
    native, player, bridge, _, sent, calls = live_adapter
    native.plugin._native = native
    native.plugin.logger.info = lambda message: None
    bridge.refresh_inventory = lambda p: calls.append(('refresh', p.unique_id))
    backend = PacketBackend(native.plugin, {})
    engine, _, _ = setup_engine()
    backend.sessions[player.unique_id] = SimpleNamespace(engine=engine,
        binding=SimpleNamespace(next_id=10), window=70, stage=0, queue=deque(), generation='test')
    backend.actual[player.unique_id] = [ItemDescriptor()] * 36
    backend.close(player)
    assert ('refresh', player.unique_id) in calls
    assert not any(packet == 49 for packet, _ in sent)
    assert not backend.sessions and engine.closed
    event = SimpleNamespace(player=player, packet_id=47, payload=b'\x46\xf7\x00', is_cancelled=False)
    backend.receive(event)
    assert event.is_cancelled  # Retired packet-view echo must never close a new BDS inventory.


@pytest.mark.parametrize('acknowledged', [True, False])
def test_forced_close_waits_for_reply_and_confirms_it_from_tick(live_adapter, acknowledged):
    import time
    from endstone_remote_workstations.packet_backend import PacketSession
    native, player, bridge, _, sent, _ = live_adapter
    native.plugin._native = native
    native.plugin.logger.info = lambda message: None
    bridge.refresh_inventory = lambda p: None
    backend = PacketBackend(native.plugin, {})
    engine, _, _ = setup_engine()
    session = PacketSession(player.unique_id, 'test', 70, player.dimension.name, (0, 100, 0),
        'Test', SimpleNamespace(next_id=10), engine, {}, time.monotonic(), 7, stage=4, opened=True)
    session.selection = 'example:next'
    backend.sessions[player.unique_id] = session
    backend.close(player, 'selected')
    assert not session.closed and not backend._native_guard(player, 'anvil')
    if acknowledged:
        count = len(sent)
        event = SimpleNamespace(player=player, packet_id=47, payload=b'\x46\xf7\x00', is_cancelled=False)
        backend.receive(event)
        assert event.is_cancelled and not session.closed and len(sent) == count
    else:
        backend.retiring[player.unique_id, 70] = session, 0
    backend.poll()
    assert session.closed and not backend.retiring
    if acknowledged:
        assert sent[-1] == (47, b'\x46\xf7\x00') and session.selection == 'example:next'
    else:
        assert session.selection is None and session.reason == 'close_ack_timeout'
