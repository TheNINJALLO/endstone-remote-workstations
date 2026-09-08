from dataclasses import replace
from types import SimpleNamespace
import pytest

from endstone_remote_workstations.chemistry import lab_control, lab_payload
from endstone_remote_workstations.linked import CHEMISTRY_SCREENS
from test_native_backend import live_adapter
from test_linked_native import configure, acknowledge


def event(player, payload):
    return SimpleNamespace(player=player, packet_id=109, payload=payload, is_cancelled=False)


@pytest.mark.parametrize('kind', sorted(CHEMISTRY_SCREENS))
def test_chemistry_requires_enabled_world_and_config_before_open_and_after_queue(live_adapter, kind):
    context, _ = configure(live_adapter, kind)
    backend, player, bridge, block, sent, calls = live_adapter
    backend.settings['chemistry']['enabled'] = False
    with pytest.raises(RuntimeError, match='Remote chemistry is disabled'):
        backend.open(player, kind, context)
    backend.settings['chemistry']['enabled'] = True
    bridge.education_state = lambda p: {'education_features_enabled': False}
    with pytest.raises(RuntimeError, match='disabled in this world'):
        backend.open(player, kind, context)
    assert not calls and not sent
    bridge.education_state = lambda p: {'education_features_enabled': True}
    backend.open(player, kind, context)
    bridge.education_state = lambda p: {'education_features_enabled': False}
    backend.poll()
    assert not backend.sessions and not backend.pending
    assert not any(row[0] == 'linked' for row in calls)


def test_lab_native_control_and_reaction_preserve_authoritative_source(live_adapter):
    context, _ = configure(live_adapter, 'labtable')
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'labtable', context)
    acknowledge(backend, player)
    session = backend.sessions[player.unique_id]
    incoming = event(player, lab_payload(0, session.position, 0))
    backend.receive(incoming)
    assert not incoming.is_cancelled and lab_control(incoming.payload) == (0, context.position, 0)
    outgoing = event(player, lab_payload(1, context.position, 2))
    backend.project(outgoing)
    assert not outgoing.is_cancelled and lab_control(outgoing.payload) == (1, session.position, 2)
    foreign = event(player, lab_payload(0, (11, 25, 39), 0))
    original = foreign.payload
    backend.receive(foreign)
    assert not foreign.is_cancelled and foreign.payload == original
    guessed_source = event(player, lab_payload(0, context.position, 0))
    backend.receive(guessed_source)
    assert guessed_source.is_cancelled


@pytest.mark.parametrize('change', ['world', 'source_permission', 'guard', 'closing', 'source_type'])
def test_lab_revokes_control_before_native_execution(live_adapter, change):
    context, source = configure(live_adapter, 'labtable')
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'labtable', context)
    acknowledge(backend, player)
    session = backend.sessions[player.unique_id]
    if change == 'world': bridge.education_state = lambda p: {'education_features_enabled': False}
    if change == 'source_permission': player.has_permission = lambda key: key != context.permission
    if change == 'guard': backend.add_guard('denied', lambda *args: False)
    if change == 'closing': backend.close(player)
    if change == 'source_type': source.type = 'minecraft:air'
    incoming = event(player, lab_payload(0, session.position, 0))
    original = incoming.payload
    backend.receive(incoming)
    assert incoming.is_cancelled and incoming.payload == original and session.close_pending


@pytest.mark.parametrize('payload', [b'', b'\x00\x80', b'\x00\x00\x00\x00\x00\x00'])
def test_lab_rejects_malformed_controls(live_adapter, payload):
    context, _ = configure(live_adapter, 'labtable')
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'labtable', context)
    acknowledge(backend, player)
    incoming = event(player, payload)
    backend.receive(incoming)
    assert incoming.is_cancelled and backend.sessions[player.unique_id].close_pending


@pytest.mark.parametrize('action,reaction', [(1, 2), (2, 0), (0, 2), (3, 0), (0, 13)])
def test_lab_cannot_request_a_chosen_reaction_or_server_update(live_adapter, action, reaction):
    context, _ = configure(live_adapter, 'labtable')
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'labtable', context)
    acknowledge(backend, player)
    incoming = event(player, lab_payload(action, backend.sessions[player.unique_id].position, reaction))
    backend.receive(incoming)
    assert incoming.is_cancelled


def test_lab_late_control_is_rejected_after_close_and_display_is_not_reused(live_adapter):
    context, _ = configure(live_adapter, 'labtable')
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'labtable', context)
    acknowledge(backend, player)
    old = backend.sessions[player.unique_id].position
    backend.forget(player)
    bridge.ready = True
    backend.open(player, 'labtable', context)
    acknowledge(backend, player)
    assert backend.sessions[player.unique_id].position != old
    incoming = event(player, lab_payload(0, old, 0))
    backend.receive(incoming)
    assert incoming.is_cancelled
    other = replace(context, position=(13, 30, 50))
    foreign = event(player, lab_payload(0, other.position, 0))
    backend.receive(foreign)
    assert not foreign.is_cancelled


@pytest.mark.parametrize('kind', sorted(CHEMISTRY_SCREENS))
@pytest.mark.parametrize('change', ['none', 'world', 'permission', 'guard', 'window', 'closed', 'source'])
def test_chemistry_native_inventory_revalidates_at_dispatch(live_adapter, kind, change):
    context, source = configure(live_adapter, kind)
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, kind, context)
    acknowledge(backend, player)
    if change == 'world': bridge.education_state = lambda p: {'education_features_enabled': False}
    if change == 'permission': player.has_permission = lambda key: key != context.permission
    if change == 'guard': backend.add_guard('revoked', lambda *args: False)
    if change == 'window': bridge.state = lambda p: {'ready': False, 'window': 12}
    if change == 'closed': bridge.ready = True
    if change == 'source': source.type = 'minecraft:air'
    # Real captured LabTableCombine + native destroy action. The admission
    # guard must preserve bytes; only BDS may interpret or execute the recipe.
    payload = bytes.fromhex('01ed030207090404012800002b00000000ffffffff')
    incoming = SimpleNamespace(player=player, packet_id=147, payload=payload, is_cancelled=False)
    backend.receive(incoming)
    assert incoming.payload == payload
    assert incoming.is_cancelled == (change != 'none')
    assert backend.sessions[player.unique_id].close_pending == (change != 'none')


def test_chemistry_inventory_does_not_claim_unmanaged_or_other_native_managers(live_adapter):
    context, _ = configure(live_adapter, 'chest')
    backend, player, bridge, block, sent, calls = live_adapter
    incoming = SimpleNamespace(player=player, packet_id=147, payload=b'foreign-native-payload', is_cancelled=False)
    backend.receive(incoming)
    assert not incoming.is_cancelled
    backend.open(player, 'chest', context)
    acknowledge(backend, player)
    backend.add_guard('revoked', lambda *args: False)
    backend.receive(incoming)
    assert not incoming.is_cancelled


@pytest.mark.parametrize('change', ['cancel', 'world', 'permission', 'guard'])
def test_captured_compound_close_returns_grid_and_cursor_after_revocation(live_adapter, change):
    context, _ = configure(live_adapter, 'compoundcreator')
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'compoundcreator', context)
    acknowledge(backend, player)
    if change == 'cancel': backend.close(player)
    if change == 'world': bridge.education_state = lambda p: {'education_features_enabled': False}
    if change == 'permission': player.has_permission = lambda key: key != context.permission
    if change == 'guard': backend.add_guard('revoked', lambda *args: False)
    # Actual a19 failed close: 63 cursor + 1 Compound Creator input, returned
    # through native request aliases to inventory slot 14 before ContainerClose.
    payload = bytes.fromhex('02c1040101013f3b0000290000000c000e0000000000ffffffffc504010101012300123f0000000c000edffeffff00ffffffff')
    incoming = SimpleNamespace(player=player, packet_id=147, payload=payload, is_cancelled=False)
    backend.receive(incoming)
    assert not incoming.is_cancelled and incoming.payload == payload
    # The exception admits returns only; a subsequent combine remains blocked.
    craft = SimpleNamespace(player=player, packet_id=147,
        payload=bytes.fromhex('01ed030207090404012800002b00000000ffffffff'), is_cancelled=False)
    backend.receive(craft)
    assert craft.is_cancelled


@pytest.mark.parametrize('mutation', ['foreign_window', 'closed_manager', 'other_dimension', 'deposit', 'preview_take', 'dynamic', 'mixed_recipe', 'truncated'])
def test_cleanup_exception_cannot_expand_native_inventory_authority(live_adapter, mutation):
    from endstone_remote_workstations.protocol import uvar, svar, full_container
    import struct
    context, _ = configure(live_adapter, 'compoundcreator')
    backend, player, bridge, block, sent, calls = live_adapter
    backend.open(player, 'compoundcreator', context)
    acknowledge(backend, player)
    backend.close(player)
    source = full_container(35)+bytes((18,))+struct.pack('<i', 63)
    dest = full_container(12)+bytes((14,))+struct.pack('<i', 0)
    if mutation == 'foreign_window': bridge.state = lambda p: {'ready': False, 'window': 12}
    if mutation == 'closed_manager': bridge.ready = True
    if mutation == 'other_dimension': player.dimension.name = 'Nether'
    if mutation == 'deposit': source, dest = dest, source
    if mutation == 'preview_take': source = full_container(36)+bytes((50,))+struct.pack('<i', 63)
    if mutation == 'dynamic': source = full_container(35, 7)+bytes((18,))+struct.pack('<i', 63)
    payload = uvar(1)+svar(-291)+uvar(1)+bytes((1,1,1))+source+dest+b'\0'+struct.pack('<i', -1)
    if mutation == 'mixed_recipe': payload = payload[:3]+bytes((2,))+payload[4:-5]+bytes((7,9))+payload[-5:]
    if mutation == 'truncated': payload = payload[:-1]
    incoming = SimpleNamespace(player=player, packet_id=147, payload=payload, is_cancelled=False)
    backend.receive(incoming)
    assert incoming.is_cancelled
