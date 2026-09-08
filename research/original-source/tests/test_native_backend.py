from types import SimpleNamespace
import uuid
import struct

import pytest

from endstone_remote_workstations.native_backend import NativeBackend, STATIONS, block_update
from endstone_remote_workstations.protocol import Open, Reader
from endstone_remote_workstations.native_backend import CLIENT_TRANSITION_SECONDS
from endstone_remote_workstations.native_backend import NativeSession


@pytest.fixture
def live_adapter():
    sent, calls = [], []
    block = SimpleNamespace(type='minecraft:air', data=SimpleNamespace(runtime_id=17))
    dimension = SimpleNamespace(name='Overworld', get_block_at=lambda *p: block)
    player = SimpleNamespace(unique_id=uuid.uuid4(), game_version='1.26.45', device_os='Windows',
                             is_valid=True, is_dead=False, dimension=dimension,
                             location=SimpleNamespace(x=-2.5, y=-20, z=3.5, yaw=0),
                             has_permission=lambda p: True, kick=lambda message: calls.append(('kick', message)))
    player.send_error_message = lambda message: calls.append(('error', message))
    server = SimpleNamespace(version='0.11.10', protocol_version=2169,
                             create_block_data=lambda kind: SimpleNamespace(runtime_id=19),
                             get_player=lambda identity: player if identity == player.unique_id else None)
    bridge = SimpleNamespace(runtime_info=lambda: dict(bridge_api=1, transaction_owner='BDS', workstations=tuple(STATIONS)),
                             ready=True, state=lambda p: {'ready': bridge.ready})
    plugin = SimpleNamespace(server=server, logger=SimpleNamespace(error=lambda message: calls.append(('error', message))))
    backend = NativeBackend(plugin, {'enabled': True}, bridge)
    def send(packet_id, payload):
        sent.append((packet_id, payload))
        backend.observe('send', SimpleNamespace(player=player, packet_id=packet_id, payload=payload, is_cancelled=False))
    def open_native(p, kind, *position):
        calls.append(('open', kind))
        bridge.ready = False
        p.send_packet(46, Open(7, STATIONS[kind][1], *position).encode())
        return 7
    player.send_packet = send
    bridge.open_station = open_native
    return backend, player, bridge, block, sent, calls


def activate(backend, player, key):
    backend.open(player, key)
    backend.poll()
    pending = backend.pending[player.unique_id]
    event = SimpleNamespace(player=player, packet_id=115, payload=struct.pack('<QB', pending.nonce*1_000_000, 1), is_cancelled=False)
    backend.observe('receive', event)
    pending.queued_at -= 0.5
    backend.poll()


def test_signed_negative_y_update_fixture():
    reader = Reader(block_update((-3, -19, 2), 19))
    assert tuple(reader.svar() for _ in range(3)) == (-3, -19, 2)
    assert tuple(reader.uvar() for _ in range(3)) == (19, 2, 0)
    reader.end()


@pytest.mark.parametrize('kind', ['inventory2x2', 'armor', 'offhand', 'recipebook'])
def test_player_inventory_opens_without_world_access_or_projection(live_adapter, kind):
    backend, player, bridge, block, sent, calls = live_adapter
    player.dimension.get_block_at = lambda *p: pytest.fail('Player inventory must not read or project world blocks')
    backend.server.create_block_data = lambda *p: pytest.fail('Player inventory has no fake backing block')
    activate(backend, player, kind)
    assert backend.sessions[player.unique_id].window == 7
    assert Open.decode(next(payload for packet, payload in sent if packet == 46)).container_type == 255
    backend.close(player)
    assert not any(packet == 21 for packet, payload in sent)
    assert not any(packet == 47 for packet, payload in sent)
    assert not backend.sessions
    assert not bridge.ready  # Native contents remain owned by vanilla.
    with pytest.raises(RuntimeError, match='Close the current inventory'):
        backend.open(player, 'anvil')
    bridge.ready = True
    backend.poll()
    assert not backend.sessions


def test_player_inventory_uses_current_position_after_readiness_roundtrip(live_adapter):
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'inventory2x2')
    backend.poll()
    pending = backend.pending[player.unique_id]
    player.location.x = 14.5
    pending.acknowledged = True
    pending.queued_at -= CLIENT_TRANSITION_SECONDS
    backend.poll()
    assert backend.sessions[player.unique_id].position[0] == 14
    assert not any(kind == 'error' for kind, _ in calls)


def test_player_inventory_accepts_native_eye_position(live_adapter):
    backend, player, bridge, block, sent, calls = live_adapter
    def open_inventory(p, kind, x, y, z):
        bridge.ready = False
        p.send_packet(46, Open(7, 255, x, y+1, z).encode())
        return 7
    bridge.open_station = open_inventory
    activate(backend, player, 'inventory2x2')
    assert backend.sessions[player.unique_id].position[1] == -19
    assert not any(packet == 47 for packet, _ in sent)


def test_player_inventory_does_not_adopt_foreign_open_during_readiness(live_adapter):
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'inventory2x2')
    backend.poll()
    player.send_packet(46, Open(9, 255, 0, 64, 0).encode())
    assert backend.sessions[player.unique_id].window is None
    backend.poll()
    assert not backend.pending and not backend.sessions
    assert not any(packet == 47 for packet, _ in sent)


@pytest.mark.parametrize('change', ['permission', 'client', 'dead', 'busy', 'guard'])
def test_admission_denies_before_packets_or_native_mutation(live_adapter, change):
    backend, player, bridge, block, sent, calls = live_adapter
    if change == 'permission': player.has_permission = lambda p: False
    if change == 'client': player.device_os = 'Android'
    if change == 'dead': player.is_dead = True
    if change == 'busy': bridge.ready = False
    if change == 'guard': backend.add_guard('protection', lambda p, key: False)
    with pytest.raises(RuntimeError): backend.open(player, 'anvil')
    assert not sent and not calls and not backend.sessions


def test_native_session_restores_current_state_and_waits_for_bds_cleanup(live_adapter):
    backend, player, bridge, block, sent, calls = live_adapter
    activate(backend, player, 'anvil')
    session = backend.sessions[player.unique_id]
    assert session.window == 7 and session.generation == 1
    # Restoration must use current authoritative full block data, never saved type.
    block.data.runtime_id = 99
    backend.close(player)
    assert sent[-2] == (47, bytes((7, 247, 1)))
    assert sent[-1] == (21, block_update(session.position, 99))
    assert player.unique_id in backend.sessions
    with pytest.raises(RuntimeError): backend.open(player, 'craft')
    bridge.ready = True
    backend.poll()
    assert not backend.sessions
    assert len([packet for packet, _ in sent if packet == 47]) == 1


def test_unrelated_close_and_dimension_change_do_not_restore_wrong_world(live_adapter):
    backend, player, bridge, block, sent, calls = live_adapter
    activate(backend, player, 'craft')
    event = SimpleNamespace(player=player, packet_id=47, payload=bytes((8, 247, 0)), is_cancelled=False)
    backend.observe('receive', event)
    assert backend.sessions[player.unique_id].closing_at is None
    player.dimension = SimpleNamespace(name='Nether', get_block_at=lambda *p: pytest.fail('Read from wrong dimension'))
    backend.close(player)
    assert sent[-1][0] == 47


def test_authoritative_world_change_closes_projection_without_cancelling_packet(live_adapter):
    backend, player, bridge, block, sent, calls = live_adapter
    activate(backend, player, 'stonecutter')
    session = backend.sessions[player.unique_id]
    block.data.runtime_id = 42
    event = SimpleNamespace(player=player, packet_id=21, payload=block_update(session.position, 42), is_cancelled=False)
    backend.observe('send', event)
    assert not session.restored and session.close_pending
    backend.poll()
    assert session.restored and session.closing_at and not event.is_cancelled


def test_missing_native_open_packet_closes_session_instead_of_claiming_success(live_adapter):
    backend, player, bridge, block, sent, calls = live_adapter
    bridge.open_station = lambda *args: 12
    activate(backend, player, 'anvil')
    assert any('changed or cancelled' in message for kind, message in calls if kind == 'error')
    assert any(packet == 47 and payload[0] == 12 for packet, payload in sent)
    assert not backend.sessions


def test_repeated_close_is_idempotent_and_quit_removes_only_own_session(live_adapter):
    backend, player, bridge, block, sent, calls = live_adapter
    activate(backend, player, 'anvil')
    backend.close(player)
    before = len(sent)
    backend.close(player)
    assert len(sent) == before
    backend.forget(player, restore=False)
    backend.forget(player, restore=False)
    assert not backend.sessions


def test_packet_callbacks_never_send_nested_packets(live_adapter):
    backend, player, bridge, block, sent, calls = live_adapter
    activate(backend, player, 'anvil')
    before = len(sent)
    event = SimpleNamespace(player=player, packet_id=47, payload=bytes((7,247,0)), is_cancelled=False)
    backend.observe('send', event)
    assert len(sent) == before
    backend.poll()
    assert len(sent) == before+1


def test_wrong_or_unsolicited_ping_does_not_open_native_container(live_adapter):
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'anvil')
    pending = backend.pending[player.unique_id]
    event = SimpleNamespace(player=player, packet_id=115, payload=struct.pack('<QB', pending.nonce*1_000_000,1), is_cancelled=False)
    backend.observe('receive', event)
    assert not pending.acknowledged
    backend.poll()
    event.payload = struct.pack('<QB', (pending.nonce+1)*1_000_000, 1)
    backend.observe('receive', event)
    backend.poll()
    assert not calls and backend.sessions[player.unique_id].window is None
    assert [packet for packet, payload in sent] == [21, 115]


@pytest.mark.parametrize('key', tuple(STATIONS))
def test_each_native_station_has_its_own_window_type(live_adapter, key):
    backend, player, bridge, block, sent, calls = live_adapter
    activate(backend, player, key)
    packets = [Open.decode(payload) for packet, payload in sent if packet == 46]
    assert len(packets) == 1 and packets[0].container_type == STATIONS[key][1]
    assert calls == [('open', key)]


def test_acknowledgement_alone_does_not_bypass_measured_chat_transition(live_adapter):
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'craft')
    backend.poll()
    pending = backend.pending[player.unique_id]
    pending.acknowledged = True
    backend.poll()
    assert not calls and pending.player_id in backend.pending
    pending.queued_at -= CLIENT_TRANSITION_SECONDS
    backend.poll()
    assert calls == [('open', 'craft')]


@pytest.mark.parametrize('change', ['guard', 'permission', 'block', 'dimension'])
def test_conditions_are_rechecked_after_client_acknowledgement(live_adapter, change):
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'anvil')
    backend.poll()
    pending = backend.pending[player.unique_id]
    pending.acknowledged = True
    pending.queued_at -= CLIENT_TRANSITION_SECONDS
    if change == 'guard': backend.add_guard('late-deny', lambda p, key: False)
    if change == 'permission': player.has_permission = lambda p: False
    if change == 'block': block.type = 'minecraft:stone'
    if change == 'dimension': player.dimension = SimpleNamespace(name='Nether')
    backend.poll()
    assert not any(kind == 'open' for kind, value in calls)
    assert any(kind == 'error' for kind, value in calls)
    assert not backend.sessions and not backend.pending and bridge.ready


def test_open_timeout_restores_projection_without_creating_a_manager(live_adapter):
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'anvil')
    backend.poll()
    pending = backend.pending[player.unique_id]
    position = backend.sessions[player.unique_id].position
    pending.queued_at -= 11
    backend.poll()
    assert sent[-1] == (21, block_update(position, block.data.runtime_id))
    assert not backend.sessions and not backend.pending
    assert not any(kind == 'open' for kind, value in calls)


def test_native_bridge_contract_mismatch_fails_closed(live_adapter):
    backend, player, bridge, block, sent, calls = live_adapter
    bridge.runtime_info = lambda: dict(bridge_api=1, transaction_owner='BDS', workstations=('craft',))
    other = NativeBackend(backend.plugin, {'enabled': True}, bridge)
    with pytest.raises(RuntimeError, match='contract'):
        other.open(player, 'craft')
    assert not sent and not calls


def test_poll_budget_rotates_fairly_without_scanning_server_players(live_adapter, monkeypatch):
    backend, player, bridge, block, sent, calls = live_adapter
    clock = [100.0]
    monkeypatch.setattr('endstone_remote_workstations.native_backend.time.monotonic', lambda: clock[0])
    players = {uuid.uuid4(): SimpleNamespace(**vars(player)) for _ in range(3)}
    for identity, p in players.items():
        p.unique_id = identity
        backend.sessions[identity] = NativeSession(identity, 1, 'anvil', 'Overworld', (0, 0, 0), 100, window=7)
        backend._poll_order[identity] = None
    backend.server.get_player = players.get
    visited = []
    def costly_state(p):
        visited.append(p.unique_id)
        clock[0] += 0.003  # A single native call may overrun the soft 2 ms budget.
        return {'ready': False}
    bridge.state = costly_state
    for expected in range(1, 4):
        backend.poll()
        assert len(visited) == expected
    assert visited == list(players)
    backend.poll()
    assert len(visited) == 3  # No repeat native calls before the 250 ms interval.


def test_new_command_can_retire_a_completed_native_close_before_periodic_check(live_adapter):
    backend, player, bridge, block, sent, calls = live_adapter
    activate(backend, player, 'anvil')
    bridge.ready = True
    backend.open(player, 'craft')
    assert backend.pending[player.unique_id].kind == 'craft'
    assert player.unique_id not in backend.sessions
    backend.close(player)
    backend.poll()
    assert not backend._poll_order


def test_captured_windows_ping_fixture_matches_the_admitted_wire_contract():
    import json
    from pathlib import Path
    fixture = json.loads((Path(__file__).parent/'fixtures/windows-native-controls-2169.json').read_text())
    sent, send_flag = struct.unpack('<QB', bytes.fromhex(fixture['send_hex']))
    received, receive_flag = struct.unpack('<QB', bytes.fromhex(fixture['receive_hex']))
    assert received == sent*1_000_000 and send_flag == receive_flag == 1
