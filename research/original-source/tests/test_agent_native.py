from types import SimpleNamespace
import struct
import pytest

from endstone_remote_workstations.protocol import full_container, svar, uvar
from test_native_backend import live_adapter
from test_entity_native import configure
from test_linked_native import acknowledge

# Unchanged packets from the real Agent deposit/withdrawal capture.
DEPOSIT = bytes.fromhex('019506010101011c00011e0000000700000000000000ffffffff')
WITHDRAW = bytes.fromhex('019906010101010700001e0000000c00010000000000ffffffff')


def setup(adapter):
    context, actor = configure(adapter, 'agent')
    backend, player, bridge, *_ = adapter
    backend.settings['agent'] = {'enabled': True}
    bridge.education_state = lambda p: {'education_features_enabled': True}
    actor.is_dead, actor.runtime_id, actor.owned = False, 19, True
    def native_state(p, source, kind):
        if not source.owned:
            raise RuntimeError('Actual native owner mismatch')
        return {'actor_id': source.id}
    bridge.entity_state = native_state
    return context, actor


def packet(player, payload):
    return SimpleNamespace(player=player, packet_id=147, payload=payload, is_cancelled=False)


@pytest.mark.parametrize('change', ['config','world','owner','dead'])
def test_agent_rechecks_source_before_native_creation(live_adapter, change):
    context, actor = setup(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'agent', context)
    if change == 'config': backend.settings['agent']['enabled'] = False
    if change == 'world': bridge.education_state = lambda p: {'education_features_enabled': False}
    if change == 'owner': actor.owned = False
    if change == 'dead': actor.is_dead = True
    backend.poll()
    assert not backend.sessions and not backend.pending
    assert not any(row[0] == 'entity' for row in calls)
    assert not any(number == 46 for number,payload in sent)


@pytest.mark.parametrize('change', ['config','world','owner','dead','expired','identity','runtime','permission','guard','closing'])
def test_agent_revocation_rejects_native_storage_requests_immediately(live_adapter, change):
    context, actor = setup(live_adapter)
    backend, player, bridge, *_ = live_adapter
    backend.open(player, 'agent', context)
    acknowledge(backend, player)
    for payload in (DEPOSIT, WITHDRAW):
        event = packet(player, payload)
        backend.receive(event)
        assert not event.is_cancelled and event.payload == payload
    if change == 'config': backend.settings['agent']['enabled'] = False
    if change == 'world': bridge.education_state = lambda p: {'education_features_enabled': False}
    if change == 'owner': actor.owned = False
    if change == 'dead': actor.is_dead = True
    if change == 'expired': actor.is_valid = False
    if change == 'identity': actor.id -= 1
    if change == 'runtime': actor.runtime_id += 1
    if change == 'permission': player.has_permission = lambda name: name != context.permission
    if change == 'guard': backend.add_guard('protection', lambda *args: False)
    if change == 'closing': backend.close(player)
    for payload in (DEPOSIT, WITHDRAW):
        event = packet(player, payload)
        backend.receive(event)  # Before the next scheduled lifecycle poll.
        assert event.is_cancelled and event.payload == payload
    assert backend.sessions[player.unique_id].close_pending


@pytest.mark.parametrize('mutation', ['none','storage_source','storage_dest','preview','dynamic','foreign_window','other_world','closed_manager','truncated'])
def test_agent_cleanup_only_returns_its_existing_native_cursor(live_adapter, mutation):
    context, actor = setup(live_adapter)
    backend, player, bridge, *_ = live_adapter
    backend.open(player, 'agent', context)
    acknowledge(backend, player)
    actor.owned = False
    source = full_container(59)+b'\0'+struct.pack('<i',30)
    dest = full_container(12)+b'\x01'+struct.pack('<i',0)
    if mutation == 'storage_source': source = full_container(7)+b'\0'+struct.pack('<i',30)
    if mutation == 'storage_dest': dest = full_container(7)+b'\0'+struct.pack('<i',0)
    if mutation == 'preview': source = full_container(37)+b'\0'+struct.pack('<i',30)
    if mutation == 'dynamic': source = full_container(59,7)+b'\0'+struct.pack('<i',30)
    if mutation == 'foreign_window': bridge.state = lambda p: {'ready': False,'window': 12}
    if mutation == 'other_world': player.dimension.name = 'Nether'
    if mutation == 'closed_manager': bridge.ready = True
    payload = uvar(1)+svar(-399)+uvar(1)+b'\x01\x01\x01'+source+dest+b'\0'+struct.pack('<i',-1)
    if mutation == 'truncated': payload = payload[:-1]
    event = packet(player,payload)
    backend.receive(event)
    assert event.is_cancelled == (mutation != 'none')
    assert event.payload == payload and backend.sessions[player.unique_id].close_pending
