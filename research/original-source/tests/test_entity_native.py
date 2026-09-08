from dataclasses import replace
from types import SimpleNamespace

import pytest

from endstone_remote_workstations.api import EntityContext, UIError
from endstone_remote_workstations.entities import ENTITY_STATIONS, configured_entity, validate_entity_context
from endstone_remote_workstations.protocol import Open, uvar
from test_native_backend import live_adapter
from test_linked_native import acknowledge
from test_developer_api import service


def configure(adapter, kind='chestminecart'):
    backend, player, bridge, block, sent, calls = adapter
    actor = SimpleNamespace(id=-1234567, is_valid=True, type=ENTITY_STATIONS[kind][2], dimension=player.dimension)
    context = EntityContext(player.dimension.name, actor, 'example.vehicle')
    backend.settings['entities'] = {'enabled': True}
    backend.entities_available = True
    bridge.entity_state = lambda p, a, k: {'actor_id': a.id}
    bridge.entity_mount_state = lambda p, a: None
    bridge.entity_mount_link = lambda p, a: None
    bridge.state = lambda p: {'ready': bridge.ready, 'window': 11}
    def send(packet, payload):
        event = SimpleNamespace(player=player, packet_id=packet, payload=payload, is_cancelled=False)
        backend.project(event)
        sent.append((packet, event.payload))
        backend.observe('send', event)
    player.send_packet = send
    def open_entity(p, source, key):
        assert source is actor  # Same real public Actor; no snapshot inventory.
        calls.append(('entity', source.id))
        bridge.ready = False
        p.send_packet(46, Open(11, ENTITY_STATIONS[key][1], 90, 70, 40, actor.id).encode())
        p.send_packet(49, uvar(11)+uvar(ENTITY_STATIONS[key][3]))
        return 11
    bridge.open_entity = open_entity
    return context, actor


@pytest.mark.parametrize('kind', ['chestminecart', 'hopperminecart', 'chestboat'])
def test_real_entity_identity_and_shape_with_owned_display(live_adapter, kind):
    context, actor = configure(live_adapter, kind)
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, kind, context)
    acknowledge(backend, player)
    session = backend.sessions[player.unique_id]
    opened = Open.decode(next(p for i, p in sent if i == 46))
    assert session.window == 11 and session.entity_id == actor.id
    assert opened.actor_id == -1 and (opened.x, opened.y, opened.z) == session.position
    assert session.shape_seen and not session.shape_invalid
    assert calls == [('entity', actor.id)]
    backend.close(player)
    bridge.ready = True
    backend.poll()
    assert not backend.sessions


@pytest.mark.parametrize('change', ['expired', 'identity', 'dimension', 'type', 'permission', 'guard'])
def test_entity_lifecycle_retires_view_without_item_writes(live_adapter, change):
    context, actor = configure(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'chestminecart', context)
    acknowledge(backend, player)
    if change == 'expired': actor.is_valid = False
    if change == 'identity': actor.id -= 1
    if change == 'dimension': actor.dimension = SimpleNamespace(name='Nether')
    if change == 'type': actor.type = 'minecraft:player'
    if change == 'permission': player.has_permission = lambda p: p != context.permission
    if change == 'guard': backend.add_guard('protection', lambda *args: False)
    backend.sessions[player.unique_id].next_state_check = 0
    backend.poll()
    assert backend.sessions[player.unique_id].closing_at is not None
    assert len([i for i, p in sent if i == 47]) == 1


def test_identity_change_during_readiness_never_opens_entity(live_adapter):
    context, actor = configure(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'chestminecart', context)
    backend.poll()
    actor.id -= 1
    pending = backend.pending[player.unique_id]
    pending.acknowledged = True
    pending.queued_at -= 1
    backend.poll()
    assert not any(i == 46 for i, p in sent)
    assert not any(k == 'entity' for k, _ in calls)


def test_foreign_entity_open_is_never_projected_or_adopted(live_adapter):
    context, actor = configure(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'chestminecart', context)
    backend.poll()
    session = backend.sessions[player.unique_id]
    session.activating = True
    payload = Open(22, 0, 90, 70, 40, actor.id+1).encode()
    event = SimpleNamespace(player=player, packet_id=46, payload=payload, is_cancelled=False)
    backend.project(event)
    backend.observe('send', event)
    assert event.payload == payload and session.window is None


def test_entity_permission_and_native_identity_checks_fail_closed(live_adapter):
    context, actor = configure(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    player.has_permission = lambda p: p != 'remoteworkstations.contexts'
    with pytest.raises(RuntimeError, match='permission'):
        backend.open(player, 'chestminecart', replace(context, permission=None))
    bridge.entity_state = lambda *a: {'actor_id': actor.id+1}
    with pytest.raises(RuntimeError, match='identity'):
        backend.open(player, 'chestminecart', context)
    assert not sent and not calls


def test_configured_entity_only_resolves_explicit_source_once(live_adapter):
    context, actor = configure(live_adapter)
    backend, player, bridge, block, sent, calls = live_adapter
    player.dimension.actors = [actor]
    backend.settings['entities']['sources'] = {'chestminecart': {
        'dimension': 'Overworld', 'actor_id': actor.id, 'permission': context.permission}}
    backend.open(player, 'chestminecart')
    del player.dimension.actors
    acknowledge(backend, player)
    backend.sessions[player.unique_id].next_state_check = 0
    backend.poll()  # No dimension-wide actor enumeration while active.
    assert backend.sessions[player.unique_id].window == 11


def test_dependency_entity_ticket_uses_normal_owner_lifecycle(service):
    svc, ui, _, owner, _, player, _, adapter = service
    context, actor = configure(adapter)
    backend = adapter[0]
    ticket = ui.open_entity(player, 'chestminecart', context)
    svc.poll()
    acknowledge(backend, player)
    svc.poll()
    assert ticket.info.kind == 'entity:chestminecart' and ticket.info.state == 'open'
    ui.dispose()
    svc.poll()
    assert backend.sessions[player.unique_id].closing_at is not None


def test_expired_wrapper_is_never_dereferenced():
    class Expired:
        is_valid = False
        @property
        def id(self):
            pytest.fail('Expired actor was dereferenced')
    with pytest.raises(UIError):
        validate_entity_context(EntityContext('Overworld', Expired()))
