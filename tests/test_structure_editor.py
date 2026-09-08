import struct

import pytest
from endstone import GameMode

from endstone_remote_workstations.protocol import CodecError, Open, svar
from endstone_remote_workstations.structure import translate_structure, structure_target, project_structure_actor
from test_native_backend import live_adapter
from test_linked_native import configure, acknowledge
from test_sign_editors import incoming, close_ack


# Actual Windows client Save and Export controls from the 5x5x5 fixture.
SAVE = bytes.fromhex('0cc8014a0e7277746573743a636f6e74726f6c00000001020764656661756c740100010a0a0a000100fdffffff7f000000000000000000c84200000000000000400000004000000040000100')
EXPORT = bytes.fromhex('0e7277746573743a636f6e74726f6c0cc8014a0764656661756c740100010a0a0a000100fdffffff7f000000000000000000c8420000000000000040000000400000004001')
SOURCE = (6, 100, 37)


def at(payload, packet_id, position):
    _, start, reader = structure_target(payload, packet_id)
    return payload[:start]+b''.join(svar(v) for v in position)+payload[reader.offset:]


def displayed(payload, packet_id, session):
    return translate_structure(at(payload, packet_id, session.position), packet_id,
        session.position, session.position,
        offset_delta=tuple(source-display for source, display in zip(session.context.position, session.position)))[0]


@pytest.mark.parametrize('packet_id,payload', [(90, SAVE), (132, EXPORT)])
def test_native_structure_controls_preserve_every_field(packet_id, payload):
    target = (-10, 80, 500)
    translated, state = translate_structure(payload, packet_id, SOURCE, target)
    assert translated == at(payload, packet_id, target)
    assert state['size'] == (5, 5, 5) and state['offset'] == (0, -1, 0)
    assert state['ignore_entities'] and not state['ignore_blocks']
    assert translate_structure(translated, packet_id, target, SOURCE)[0] == payload


@pytest.mark.parametrize('packet_id,payload', [(90, SAVE), (132, EXPORT)])
def test_every_truncated_structure_packet_is_rejected(packet_id, payload):
    for end in range(len(payload)):
        with pytest.raises(CodecError):
            translate_structure(payload[:end], packet_id, SOURCE, SOURCE)
    with pytest.raises(CodecError):
        translate_structure(payload+b'\0', packet_id, SOURCE, SOURCE)
    with pytest.raises(CodecError):
        translate_structure(payload, packet_id, SOURCE, SOURCE, 124)
    with pytest.raises(CodecError):
        translate_structure(payload, packet_id, SOURCE, (30000000, 100, 0))


@pytest.mark.parametrize('name', [b'../bad', b'foo:/bad', b'foo:a/../b', b'C:\\Windows', b'foo\0bar', b'\xff'])
def test_template_names_cannot_escape_native_resource_namespace(name):
    payload = bytes((len(name),))+name+EXPORT[15:]
    with pytest.raises(CodecError):
        translate_structure(payload, 132, SOURCE, SOURCE)


@pytest.mark.parametrize('value', [float('nan'), float('inf'), -1.0, 101.0])
def test_invalid_structure_integrity_rejected(value):
    # Integrity follows animation seconds, before the seed/pivot/operation.
    payload = EXPORT[:-21]+struct.pack('<f', value)+EXPORT[-17:]
    with pytest.raises(CodecError):
        translate_structure(payload, 132, SOURCE, SOURCE)


@pytest.fixture
def structure_adapter(live_adapter):
    context, _ = configure(live_adapter, 'structure')
    backend, player, bridge, block, sent, calls = live_adapter
    backend.settings['structure'] = {'enabled': True}
    player.is_op, player.game_mode = True, GameMode.CREATIVE
    state = dict(active=False, can_interact=True, save_complete=False, pending=False)
    bridge.structure_state = lambda *args: dict(state)
    def activate(p, x, y, z, generation, permission):
        assert (x, y, z) == context.position and permission == context.permission
        state['active'] = True
        p.send_packet(46, Open(23, 14, x, y, z).encode())
        return 23
    bridge.open_structure = activate
    def touch(p, generation, accept=False):
        calls.append(('touch', accept))
        if accept:
            assert not state['pending']
            state.update(pending=True, save_complete=False)
    bridge.structure_touch = touch
    def release(p, generation):
        state['active'] = False
        calls.append(('release', generation))
    bridge.close_structure = release
    bridge.abandon_structure = lambda gen: calls.append(('abandon', gen))
    bridge.shutdown_structure_hooks = lambda: calls.append(('shutdown', None))
    return live_adapter, context, state


def open_editor(adapter):
    live, context, state = adapter
    backend, player, *_ = live
    backend.open(player, 'structure', context)
    acknowledge(backend, player)
    return backend.sessions[player.unique_id]


def test_repeated_native_saves_keep_structure_editor_open(structure_adapter):
    live, context, state = structure_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(structure_adapter)
    for _ in range(3):
        event = incoming(player, 90, displayed(SAVE, 90, session))
        backend.receive(event)
        assert not event.is_cancelled and event.payload == at(SAVE, 90, context.position)
        state.update(pending=False, save_complete=True)
        backend.poll()
        assert backend.sessions[player.unique_id] is session and session.sign_save_at is None
    assert calls.count(('touch', True)) == 3 and state['active']


def test_export_translates_source_without_arming_metadata_save(structure_adapter):
    live, context, state = structure_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(structure_adapter)
    event = incoming(player, 132, displayed(EXPORT, 132, session))
    backend.receive(event)
    assert not event.is_cancelled and event.payload == at(EXPORT, 132, context.position)
    assert ('touch', True) not in calls and not state['pending']


def test_pending_filter_updates_are_never_reordered(structure_adapter):
    live, _, state = structure_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(structure_adapter)
    backend.receive(incoming(player, 90, displayed(SAVE, 90, session)))
    second = incoming(player, 90, displayed(SAVE, 90, session))
    backend.receive(second)
    assert second.is_cancelled and calls.count(('touch', True)) == 1
    backend.poll()
    assert not state['active'] and session.restored


@pytest.mark.parametrize('revoke', ['operator', 'creative', 'administrator', 'context', 'source', 'native', 'guard'])
def test_structure_controls_revalidate_source_and_access(structure_adapter, revoke):
    live, context, state = structure_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(structure_adapter)
    if revoke == 'operator': player.is_op = False
    if revoke == 'creative': player.game_mode = GameMode.SURVIVAL
    if revoke == 'administrator': player.has_permission = lambda p: p != 'remoteworkstations.admin'
    if revoke == 'context': player.has_permission = lambda p: p != context.permission
    if revoke == 'source': state['active'] = False
    if revoke == 'native': state['can_interact'] = False
    if revoke == 'guard': backend.add_guard('protection', lambda p, k: False)
    event = incoming(player, 90, displayed(SAVE, 90, session))
    backend.receive(event)
    assert event.is_cancelled and ('touch', True) not in calls
    backend.poll()
    assert not state['active']


def test_structure_rejects_guessed_source_and_retired_display(structure_adapter):
    live, context, state = structure_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(structure_adapter)
    guessed = incoming(player, 90, at(SAVE, 90, context.position))
    backend.receive(guessed)
    assert guessed.is_cancelled
    foreign = incoming(player, 90, SAVE)
    backend.receive(foreign)
    assert not foreign.is_cancelled
    backend.close(player)
    close_ack(backend, player, session)
    for packet_id, payload in ((90, SAVE), (132, EXPORT)):
        late = incoming(player, packet_id, displayed(payload, packet_id, session))
        backend.receive(late)
        assert late.is_cancelled


def test_structure_disconnect_abandons_native_lease(structure_adapter):
    live, _, _ = structure_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(structure_adapter)
    player.is_valid = False
    backend.poll()
    assert ('abandon', session.generation) in calls and not backend.sessions


@pytest.mark.parametrize('packet_id,payload', [(90, SAVE), (132, EXPORT)])
def test_selection_world_coordinates_survive_display_and_native_round_trip(packet_id, payload):
    display = (23, 101, -4)
    outward = tuple(s-d for s, d in zip(SOURCE, display))
    visible, fields = translate_structure(payload, packet_id, SOURCE, display, offset_delta=outward)
    assert fields['offset'] == (-17, -2, 41)
    assert tuple(p+o for p, o in zip(display, fields['offset'])) == (6, 99, 37)
    restored, fields = translate_structure(visible, packet_id, display, SOURCE,
                                           offset_delta=tuple(-v for v in outward))
    assert restored == payload and fields['offset'] == (0, -1, 0)


def test_real_actor_projection_preserves_selection_and_other_native_metadata():
    from rapidnbt import CompoundTag
    from endstone_remote_workstations.protocol import Reader
    tag = CompoundTag.from_snbt('{id:StructureBlock,x:6,y:100,z:37,xStructureOffset:2,yStructureOffset:-1,zStructureOffset:0,xStructureSize:1,yStructureSize:1,zStructureSize:1,structureName:"rwtest:a16_stone",ignoreEntities:1b,data:2}')
    original = b''.join(svar(v) for v in SOURCE)+tag.to_network_nbt()
    display = (23, 101, -4)
    payload = project_structure_actor(original, SOURCE, display)
    reader = Reader(payload)
    assert tuple(reader.svar() for _ in range(3)) == display
    visible = CompoundTag.from_network_nbt(payload[reader.offset:])
    assert [int(visible[a+'StructureOffset']) for a in 'xyz'] == [-15, -2, 41]
    for key in ('id', 'structureName', 'ignoreEntities', 'data', 'xStructureSize', 'yStructureSize', 'zStructureSize'):
        assert visible[key] == tag[key]
    assert project_structure_actor(payload, display, SOURCE) == original
    with pytest.raises(CodecError):
        project_structure_actor(original, (0, 0, 0), display)


@pytest.mark.parametrize('offset', [30000000, -2147483648])
def test_invalid_projected_world_offset_never_reaches_native_save(structure_adapter, offset):
    live, context, state = structure_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(structure_adapter)
    # A syntactically valid displayed offset could translate outside world
    # bounds. Validation must use the final real-world selection.
    raw = at(SAVE, 90, session.position)
    marker = bytes.fromhex('0a0a0a000100')  # Captured size then offset tuple.
    assert raw.count(marker) == 1
    raw = raw.replace(marker, bytes.fromhex('0a0a0a')+svar(offset)+svar(-1)+svar(0), 1)
    event = incoming(player, 90, raw)
    backend.receive(event)
    assert event.is_cancelled and ('touch', True) not in calls


def test_structure_back_without_close_packet_retires_after_completed_metadata(structure_adapter):
    from test_command_editor import input_packet
    live, context, state = structure_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_editor(structure_adapter)
    session.opened_at -= 1
    for tick in (100, 101):
        backend.observe('receive', incoming(player, 144, input_packet(tick)))
    backend.receive(incoming(player, 90, displayed(SAVE, 90, session)))
    # Movement must never discard a submitted native save/filter operation.
    backend.observe('receive', incoming(player, 144, input_packet(102, True)))
    assert state['pending'] and not session.superseded
    state.update(pending=False, save_complete=True)
    backend.poll()
    assert state['active'] and session.sign_save_at is None
    backend.observe('receive', incoming(player, 144, input_packet(103, True)))
    assert session.superseded and state['active']
    backend.poll()
    assert not backend.sessions and not state['active'] and session.restored
    late = incoming(player, 90, displayed(SAVE, 90, session))
    backend.receive(late)
    assert late.is_cancelled


def test_structure_input_requires_valid_fresh_gameplay_after_neutral_frames(structure_adapter):
    from test_command_editor import input_packet
    live, _, state = structure_adapter
    backend, player, *_ = live
    session = open_editor(structure_adapter)
    session.opened_at -= 1
    for tick, moving in ((100, True), (101, False), (102, False), (99, True)):
        backend.observe('receive', incoming(player, 144, input_packet(tick, moving)))
        assert not session.superseded
    for bad in (b'', input_packet(103, True)[:30], struct.pack('<f', float('nan'))+input_packet(103, True)[4:]):
        backend.observe('receive', incoming(player, 144, bad))
        assert not session.superseded and state['active']
    backend.observe('receive', incoming(player, 144, input_packet(103, True)))
    assert session.superseded
