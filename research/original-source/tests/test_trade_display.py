from types import SimpleNamespace

import pytest

from endstone_remote_workstations.entity_display import trade_header
from endstone_remote_workstations.protocol import CodecError, svar, uvar
from test_equipment_display import setup_equipment
from test_native_backend import live_adapter
from test_linked_native import acknowledge


def trade_packet(actor_id, window=11):
    return bytes((window, 15))+svar(0)+svar(0)+svar(actor_id, 64)+svar(-1, 64)+b'\x04test\x01\x01\x0a\x00\x00'


@pytest.mark.parametrize('kind', ['villager', 'wanderingtrader'])
def test_actual_merchant_identity_and_deferred_inventory(live_adapter, kind):
    context, actor, _ = setup_equipment(live_adapter, kind=kind)
    backend, player, bridge, block, sent, calls = live_adapter
    def open_trade(p, a, k):
        bridge.ready = False
        p.send_packet(80, trade_packet(a.id))
        return 11
    bridge.open_entity = open_trade
    backend.open(player, kind, context)
    acknowledge(backend, player)
    session = backend.sessions[player.unique_id]
    assert session.entity_added and session.window == 11 and session.shape_deadline
    assert next(raw for packet, raw in sent if packet == 80) == trade_packet(actor.id)
    player.send_packet(49, uvar(11)+uvar(3))
    assert session.shape_seen and session.shape_deadline is None
    assert not any(packet in (21, 46) for packet, _ in sent)
    backend.close(player)
    assert session.restored


def test_foreign_trade_header_cannot_supply_managed_window(live_adapter):
    context, actor, _ = setup_equipment(live_adapter, kind='villager')
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'villager', context)
    backend.poll()
    session = backend.sessions[player.unique_id]
    session.activating = True
    payload = trade_packet(actor.id+1, 22)
    player.send_packet(80, payload)
    assert session.window is None and sent[-1] == (80, payload)


@pytest.mark.parametrize('payload', [b'', b'\x00\x0f\x00\x00\x01\x01\x00\x01\x01\x00',
    b'\x01\x0c\x00\x00\x01\x01\x00\x01\x01\x00',
    b'\x01\x0f\x00\x00\x01\x01\x00\x02\x01\x00',
    b'\x01\x0f\x00\x00\x01\x01\x00\x01\x01'])
def test_malformed_trade_headers_fail_closed(payload):
    with pytest.raises(CodecError):
        trade_header(payload)
