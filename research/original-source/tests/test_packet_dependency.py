from types import SimpleNamespace
import pytest

from endstone_remote_workstations.api import EnderChest, UIError
from test_developer_api import service
from test_native_backend import live_adapter


@pytest.fixture
def packet_service(service):
    svc, ui, other, owner, other_owner, player, forms, native = service
    sessions, calls = {}, []
    def open_screen(p, title, icons, buttons):
        session = SimpleNamespace(opened=False, closed=False, reason=None, selection=None)
        sessions[p.unique_id] = session
        calls.append(('open', title))
        return session
    def close(p, reason):
        session = sessions.pop(p.unique_id, None)
        if session:
            session.closed, session.reason = True, reason
        calls.append(('close', reason))
    svc.packet = SimpleNamespace(sessions=sessions, open=open_screen, close=close, reason=lambda p, **kwargs: None)
    return svc, ui, owner, player, sessions, calls


def test_real_ender_ticket_is_deferred_and_tracks_matching_session(packet_service):
    svc, ui, _, player, sessions, calls = packet_service
    observed = []
    ticket = ui.open_ender_chest(player, on_open=observed.append, on_close=observed.append)
    assert ticket.info.kind == 'packet:enderchest' and not calls
    svc.poll()
    assert ticket.info.state == 'opening' and calls == [('open', 'Ender Chest')]
    session = sessions[player.unique_id]
    session.opened = True
    svc.poll()
    assert ticket.info.state == 'open' and len(observed) == 1
    ticket.cancel()
    assert not session.closed
    svc.poll()
    assert ticket.info.state == 'cancelled' and session.closed and len(observed) == 2


def test_packet_request_blocks_native_and_second_dependency_menu(packet_service):
    svc, ui, _, player, _, _ = packet_service
    ui.open_ender_chest(player)
    assert not svc._native_guard(player, 'anvil')
    with pytest.raises(UIError, match='already'):
        ui.open_native(player, 'anvil')


def test_owner_disable_cancels_packet_view_without_callback(packet_service):
    svc, ui, owner, player, sessions, calls = packet_service
    observed = []
    ticket = ui.open_ender_chest(player, on_close=observed.append)
    svc.poll()
    owner.is_enabled = False
    svc.release(owner)
    svc.poll()
    assert ticket.info.state == 'cancelled' and not observed and not sessions


def test_external_close_finishes_ender_ticket_and_capability_uses_packet_backend(packet_service):
    svc, ui, _, player, sessions, _ = packet_service
    row = next(row for row in ui.capabilities(player) if row['id'] == 'enderchest')
    assert row['available'] and row['backend'] == 'packet_real_ender'
    ticket = ui.open_ender_chest(player)
    svc.poll()
    sessions[player.unique_id].opened = sessions[player.unique_id].closed = True
    sessions[player.unique_id].reason = 'dismissed'
    svc.poll()
    assert ticket.info.state == 'closed' and ticket.info.reason == 'dismissed'


def test_lifecycle_invalidates_packet_before_it_opens(packet_service):
    svc, ui, _, player, _, calls = packet_service
    ticket = ui.open_ender_chest(player)
    svc.invalidate_player(player, 'teleport')
    svc.poll()
    assert ticket.info.state == 'cancelled' and not calls
