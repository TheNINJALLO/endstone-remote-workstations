import json
from pathlib import Path
from types import SimpleNamespace

import pytest
from endstone import GameMode

from endstone_remote_workstations.jigsaw import validate_jigsaw_payload
from endstone_remote_workstations.protocol import CodecError, Open, Reader
from endstone_remote_workstations.linked import project_actor_data
from test_native_backend import live_adapter
from test_linked_native import configure, acknowledge
from test_sign_editors import incoming, close_ack


CAPTURE = json.loads((Path(__file__).resolve().parent/'fixtures/jigsaw-editor-2169.json').read_text())
PAYLOAD = bytes.fromhex(CAPTURE['payload_hex'])
DISPLAY = tuple(CAPTURE['display'])


@pytest.fixture
def jigsaw_adapter(live_adapter):
    context, source = configure(live_adapter, 'jigsaw')
    backend, player, bridge, block, sent, calls = live_adapter
    backend.settings['jigsaw'] = {'enabled': True}
    player.is_op, player.game_mode = True, GameMode.CREATIVE
    state = dict(active=False, can_interact=True, save_complete=False)
    bridge.jigsaw_state = lambda *args: dict(state)
    def activate(p, x, y, z, generation):
        assert (x, y, z) == context.position
        assert generation == backend.sessions[p.unique_id].generation
        state['active'] = True
        p.send_packet(46, Open(19, 32, x, y, z).encode())
        return 19
    bridge.open_jigsaw = activate
    bridge.sign_touch = lambda p, gen, accept=False: calls.append(('touch', accept))
    def release(p, generation):
        state['active'] = False
        calls.append(('release', generation))
    bridge.close_sign = release
    bridge.abandon_sign = lambda gen: calls.append(('abandon', gen))
    bridge.shutdown_sign_hooks = lambda: None
    return live_adapter, context, state


def open_jigsaw(adapter):
    live, context, state = adapter
    backend, player, bridge, block, sent, calls = live
    backend.open(player, 'jigsaw', context)
    acknowledge(backend, player)
    return backend.sessions[player.unique_id]


def test_live_jigsaw_codec_and_bounded_schema():
    validate_jigsaw_payload(PAYLOAD, DISPLAY)
    for bad in (PAYLOAD[:-1], PAYLOAD+b'\0', b'\0'*4097, PAYLOAD.replace(b'rollable', b'badjoint')):
        with pytest.raises(CodecError):
            validate_jigsaw_payload(bad, DISPLAY)
    with pytest.raises(CodecError):
        validate_jigsaw_payload(PAYLOAD, (0, 1, 2))


def test_jigsaw_registers_real_window_and_translates_only_owned_save(jigsaw_adapter):
    live, context, state = jigsaw_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_jigsaw(jigsaw_adapter)
    assert session.window == 19 and session.screen_open and bridge.ready
    backend.poll()
    assert player.unique_id in backend.sessions  # No slot manager is expected.
    event = incoming(player, 56, project_actor_data(PAYLOAD, DISPLAY, session.position))
    backend.receive(event)
    assert not event.is_cancelled and calls[-1] == ('touch', True)
    assert event.payload == project_actor_data(PAYLOAD, DISPLAY, context.position)
    duplicate = incoming(player, 56, project_actor_data(PAYLOAD, DISPLAY, session.position))
    backend.receive(duplicate)
    assert duplicate.is_cancelled


def test_jigsaw_waits_for_native_completion_then_releases(jigsaw_adapter):
    live, context, state = jigsaw_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_jigsaw(jigsaw_adapter)
    event = incoming(player, 56, project_actor_data(PAYLOAD, DISPLAY, session.position))
    backend.receive(event)
    backend.poll()
    assert player.unique_id in backend.sessions
    state['save_complete'] = True
    backend.poll()
    assert player.unique_id not in backend.sessions and not state['active']


@pytest.mark.parametrize('revoke', ['operator', 'creative', 'administrator', 'source', 'native', 'guard'])
def test_jigsaw_revocation_closes_and_rejects_queued_save(jigsaw_adapter, revoke):
    live, context, state = jigsaw_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_jigsaw(jigsaw_adapter)
    if revoke == 'operator': player.is_op = False
    if revoke == 'creative': player.game_mode = GameMode.SURVIVAL
    if revoke == 'administrator': player.has_permission = lambda p: p != 'remoteworkstations.admin'
    if revoke == 'source': state['active'] = False
    if revoke == 'native': state['can_interact'] = False
    if revoke == 'guard': backend.add_guard('protection', lambda p, k: False)
    event = incoming(player, 56, project_actor_data(PAYLOAD, DISPLAY, session.position))
    backend.receive(event)
    assert event.is_cancelled and ('touch', True) not in calls
    backend.poll()
    assert session.closing_at is not None and not state['active']


def test_jigsaw_cancel_releases_before_air_and_denies_late_packet(jigsaw_adapter):
    live, context, state = jigsaw_adapter
    backend, player, bridge, block, sent, calls = live
    session = open_jigsaw(jigsaw_adapter)
    backend.close(player)
    assert calls[-1][0] == 'release' and session.restored
    close_ack(backend, player, session)
    late = incoming(player, 56, project_actor_data(PAYLOAD, DISPLAY, session.position))
    backend.receive(late)
    assert late.is_cancelled and player.unique_id not in backend.sessions


def test_jigsaw_defaults_disabled_and_rejects_real_source_guess(jigsaw_adapter):
    live, context, state = jigsaw_adapter
    backend, player, bridge, block, sent, calls = live
    backend.settings['jigsaw'] = {}
    with pytest.raises(RuntimeError, match='disabled'):
        backend.open(player, 'jigsaw', context)
    backend.settings['jigsaw'] = {'enabled': True}
    session = open_jigsaw(jigsaw_adapter)
    guessed = incoming(player, 56, project_actor_data(PAYLOAD, DISPLAY, context.position))
    backend.receive(guessed)
    assert guessed.is_cancelled and ('touch', True) not in calls
    foreign = incoming(player, 56, PAYLOAD)
    backend.receive(foreign)
    assert not foreign.is_cancelled and foreign.payload == PAYLOAD
