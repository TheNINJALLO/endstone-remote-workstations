"""Lectern owns page controls but has no BDS transactional container manager."""
from types import SimpleNamespace
import struct
import pytest

from endstone_remote_workstations.protocol import CodecError, Open, svar
from endstone_remote_workstations.linked import lectern_page_request
from test_native_backend import live_adapter
from test_linked_native import configure, acknowledge


def open_lectern(adapter):
    context, source = configure(adapter, 'lectern')
    backend, player, bridge, _, _, _ = adapter
    original = bridge.open_linked_station
    def open_passive(*args):
        result = original(*args)
        bridge.ready = True  # Actual openBook creates no native inventory manager.
        return result
    bridge.open_linked_station = open_passive
    backend.open(player, 'lectern', context)
    acknowledge(backend, player)
    return context, source, backend.sessions[player.unique_id]


def control(player, position, page=1, total=3):
    return SimpleNamespace(player=player, packet_id=125, is_cancelled=False,
        payload=bytes((page, total))+b''.join(svar(v) for v in position))


def test_real_wire_lectern_page_fixture():
    assert lectern_page_request(bytes.fromhex('01032eca0107')) == (1, 3, (23, 101, -4))
    with pytest.raises(CodecError): lectern_page_request(bytes.fromhex('01032eca01'))
    with pytest.raises(CodecError): lectern_page_request(bytes.fromhex('01032eca010700'))
    with pytest.raises(CodecError): lectern_page_request(bytes.fromhex('03032eca0107'))


def test_page_is_exclusively_owned_and_applied_on_poll(live_adapter):
    context, _, session = open_lectern(live_adapter)
    backend, player, bridge, _, sent, calls = live_adapter
    event = control(player, session.position)
    before = len(sent)
    backend.receive(event)
    assert event.is_cancelled and not any(kind == 'page' for kind, _ in calls)
    assert len(sent) == before
    backend.poll()
    assert ('page', (*context.position, 1, 3)) in calls
    assert player.unique_id in backend.sessions  # Native ready does not end a book UI.
    assert sent[-1][0] == 56
    with pytest.raises(RuntimeError, match='current inventory'): backend.open(player, 'craft')


@pytest.mark.parametrize('change', ['permission', 'source', 'guard', 'book'])
def test_queued_page_revalidates_source_before_mutation(live_adapter, change):
    context, source, session = open_lectern(live_adapter)
    backend, player, bridge, _, _, calls = live_adapter
    backend.receive(control(player, session.position))
    if change == 'permission': player.has_permission = lambda key: key != context.permission
    if change == 'source': source.type = 'minecraft:air'
    if change == 'guard': backend.add_guard('revoked', lambda *args: False)
    if change == 'book':
        def removed(*args): raise RuntimeError('Real book removed')
        bridge.lectern_state = removed
    backend.poll()
    assert not any(kind == 'page' for kind, _ in calls)
    assert session.closing_at is not None


def test_page_queue_is_bounded_and_foreign_positions_are_untouched(live_adapter):
    _, _, session = open_lectern(live_adapter)
    backend, player, _, _, _, calls = live_adapter
    foreign = control(player, (0, 70, 0))
    backend.receive(foreign)
    assert not foreign.is_cancelled and not session.page_requests
    for _ in range(17): backend.receive(control(player, session.position))
    assert len(session.page_requests) == 16 and session.close_pending
    backend.poll()
    assert not any(kind == 'page' for kind, _ in calls)


@pytest.mark.parametrize('packet', [46, 100])
def test_foreign_screen_retires_book_without_closing_new_screen(live_adapter, packet):
    _, _, session = open_lectern(live_adapter)
    backend, player, _, _, sent, _ = live_adapter
    payload = Open(18, 5, 0, 64, 0).encode() if packet == 46 else b'foreign form'
    player.send_packet(packet, payload)
    backend.poll()
    assert session.restored and not backend.sessions
    assert not any(packet == 47 for packet, _ in sent)


def test_forced_book_close_waits_for_matching_client_ack(live_adapter):
    _, _, session = open_lectern(live_adapter)
    backend, player, _, _, _, _ = live_adapter
    backend.close(player)
    backend.poll()
    assert player.unique_id in backend.sessions
    nonce = session.close_nonce
    backend.observe('receive', SimpleNamespace(player=player, packet_id=115,
        payload=struct.pack('<QB', (nonce+1)*1_000_000, 1), is_cancelled=False))
    assert not session.close_acknowledged
    backend.observe('receive', SimpleNamespace(player=player, packet_id=115,
        payload=struct.pack('<QB', nonce*1_000_000, 1), is_cancelled=False))
    backend.poll()
    assert not backend.sessions


def test_dismissal_page_is_noop_and_retired_position_is_not_reused(live_adapter):
    context, _, session = open_lectern(live_adapter)
    backend, player, _, _, sent, calls = live_adapter
    backend.receive(control(player, session.position, page=0))
    backend.poll()
    assert not any(kind == 'page' for kind, _ in calls)
    assert session.closing_at is not None
    closing = len(sent)
    backend.close(player)
    assert len(sent) == closing  # Close and ordering ping are each sent once.
    backend.observe('receive', SimpleNamespace(player=player, packet_id=115,
        payload=struct.pack('<QB', session.close_nonce*1_000_000, 1), is_cancelled=False))
    backend.poll()
    backend.open(player, 'lectern', context)
    acknowledge(backend, player)
    current = backend.sessions[player.unique_id]
    assert current.position != session.position
    stale = control(player, session.position)
    backend.receive(stale)
    assert not stale.is_cancelled and not current.page_requests
