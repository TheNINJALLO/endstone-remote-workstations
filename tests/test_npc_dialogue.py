from dataclasses import replace
import json
import re
import struct
from types import SimpleNamespace

import pytest

from endstone_remote_workstations.api import EntityContext, UIError
from endstone_remote_workstations.npc import PREFIX, text, wire_text, close_packet
from endstone_remote_workstations.protocol import Reader, uvar, svar
from test_native_backend import live_adapter
from test_developer_api import service


def dialogue(actor, scene='test_scene', action=0):
    # Shape confirmed against an actual BDS 169 scene, including native modes.
    actions = [dict(button_name='Next', data=[dict(cmd_line='/tag @initiator add fixture', cmd_ver=39)],
                    mode=0, text='/tag @initiator add fixture\n', type=1),
               dict(button_name='', data=[], mode=2, text='', type=1),
               dict(button_name='', data=[], mode=1, text='', type=1)]
    return struct.pack('<q', actor.id)+svar(action)+b''.join(map(wire_text,
        ('Native dialogue', scene, 'Native NPC', json.dumps(actions))))


@pytest.fixture
def npc(service):
    svc, ui, other, owner, _, player, _, adapter = service
    native, _, bridge, _, sent, calls = adapter
    native.settings['npc'] = {'enabled': True}
    svc.npc.settings = native.settings['npc']
    now = [10.]
    svc.npc.clock = lambda: now[0]
    bridge.education_state = lambda p: dict(education_features_enabled=True)
    player.dimension.level = SimpleNamespace(name='test-world')
    actor = SimpleNamespace(id=-123, runtime_id=16, type='minecraft:npc', is_valid=True,
                            is_dead=False, dimension=player.dimension)
    for obj in (player, actor):
        obj.scoreboard_tags = []
        def add(tag, obj=obj):
            obj.scoreboard_tags.append(tag)
            return True
        def remove(tag, obj=obj):
            obj.scoreboard_tags.remove(tag)
            return True
        obj.add_scoreboard_tag, obj.remove_scoreboard_tag = add, remove
    context = EntityContext('Overworld', actor, 'example.npc_source')
    wire = []
    def send(packet, payload):
        event = SimpleNamespace(player=player, packet_id=packet, payload=payload, is_cancelled=False)
        svc.npc.outgoing(event)
        svc.observe('send', event)
        if not event.is_cancelled:
            wire.append((packet, bytes(event.payload)))
    player.send_packet = send
    svc.server.command_sender = object()
    def dispatch(sender, command):
        assert sender is svc.server.command_sender
        if command.startswith('tag '):
            for obj, selector in ((actor,'@e[type=npc,tag='),(player,'@a[tag=')):
                for tag in tuple(obj.scoreboard_tags):
                    if command == f'tag {selector}{tag}] remove {tag}':
                        obj.scoreboard_tags.remove(tag)
                        calls.append(('native_remove_tag',tag))
                        return True
            pytest.fail('Native cleanup targeted an unowned tag')
        assert re.fullmatch(r'dialogue open @e\[type=npc,tag=rwui_n_[a-f0-9]{32}\] @a\[tag=rwui_p_[a-f0-9]{32}\] test_scene', command)
        assert len(actor.scoreboard_tags) == len(player.scoreboard_tags) == 1
        calls.append(('command', command))
        send(169, dialogue(actor))
        return True
    svc.server.dispatch_command = dispatch
    def start(**kwargs):
        ticket = ui.open_npc(player, context, scene='test_scene', branches=('next_scene',), **kwargs)
        svc.poll()
        return ticket
    def request(kind=6, index=0, alias=None, runtime=16, actions='', suffix=b''):
        session = svc.npc.sessions.get(player.unique_id)
        if alias is None:
            alias = session.current
        payload = uvar(runtime,64)+bytes((kind,))+wire_text(actions)+bytes((index,))+wire_text(alias)+suffix
        event = SimpleNamespace(player=player, packet_id=98, payload=payload, is_cancelled=False)
        svc.npc.receive(event)
        return event
    return SimpleNamespace(svc=svc, ui=ui, player=player, actor=actor, bridge=bridge, context=context,
        start=start, request=request, wire=wire, calls=calls, now=now, owner=owner, send=send)


def test_native_command_has_exact_targets_and_requires_client_ack(npc):
    events=[]
    ticket = npc.start(on_open=lambda info: events.append(info.state))
    assert ticket.info.state == 'opening' and not events
    assert npc.actor.scoreboard_tags == npc.player.scoreboard_tags == []
    raw = npc.wire[-1][1]
    r=Reader(raw); assert struct.unpack('<q',r.raw(8))[0] == npc.actor.id and r.uvar() == 0
    assert text(r) == 'Native dialogue'
    assert text(r).startswith(PREFIX)
    event=npc.request()
    assert not event.is_cancelled and event.payload.endswith(wire_text('test_scene'))
    npc.svc.poll()
    assert ticket.info.state == 'open' and events == ['open']


def test_scene_branch_keeps_native_commands_and_rotates_request_token(npc):
    ticket=npc.start(); npc.request(); npc.svc.poll()
    session=ticket._entry.npc_session
    old=session.current
    assert not npc.request(1).is_cancelled
    assert not npc.request(2).is_cancelled
    npc.send(169, dialogue(npc.actor,'next_scene'))
    assert session.current != old
    assert npc.request(1,alias=old).is_cancelled
    assert not npc.request(6).is_cancelled
    assert not npc.request(1).is_cancelled
    assert not npc.request(2).is_cancelled
    npc.now[0]+=.6; npc.svc.poll()
    assert ticket.info.state == 'closed' and ticket.info.reason == 'native_closed'
    assert not npc.svc.npc.sessions


@pytest.mark.parametrize('args', [dict(kind=0),dict(kind=3),dict(kind=4),dict(kind=5),dict(kind=7),
    dict(kind=1,index=1),dict(kind=1,index=255),dict(kind=1,actions='[{"cmd_line":"/give @s diamond"}]'),
    dict(kind=1,runtime=17),dict(kind=1,alias='test_scene'),dict(kind=1,alias='rwui_old'),
    dict(kind=2,index=1),dict(kind=6,index=1),dict(kind=1,suffix=b'\0')])
def test_unowned_editor_payloads_and_bad_buttons_never_reach_native(npc,args):
    ticket=npc.start(); npc.request()
    original=list(npc.calls)
    assert npc.request(**args).is_cancelled
    assert npc.calls == original


def test_each_lifecycle_and_button_request_is_one_use(npc):
    npc.start()
    assert npc.request(1).is_cancelled  # No actual client opening yet.
    for kind in (6,1,2):
        assert not npc.request(kind).is_cancelled
        assert npc.request(kind).is_cancelled


@pytest.mark.parametrize('revoke', ['cancel','dispose','disable','permission','guard','actor','world','dimension'])
def test_authority_revocation_blocks_commands_before_next_poll(npc,revoke):
    ticket=npc.start(); npc.request()
    if revoke=='cancel':ticket.cancel()
    if revoke=='dispose':npc.ui.dispose()
    if revoke=='disable':npc.owner.is_enabled=False
    if revoke=='permission':npc.player.has_permission=lambda p:False
    if revoke=='guard':npc.svc.native.add_guard('protection',lambda p,k:False)
    if revoke=='actor':npc.actor.is_valid=False
    if revoke=='world':npc.bridge.education_state=lambda p:dict(education_features_enabled=False)
    if revoke=='dimension':npc.player.dimension=SimpleNamespace(name='Nether',level=SimpleNamespace(name='test-world'))
    assert npc.request(1).is_cancelled
    npc.svc.poll()
    assert ticket.info.done and not npc.svc.npc.sessions


def test_cancel_and_immediate_reopen_cannot_replay_prior_scene(npc):
    ticket=npc.start(); npc.request(); npc.svc.poll()
    alias=ticket._entry.npc_session.current
    ticket.cancel()
    assert npc.request(2).is_cancelled  # No on_close commands after API revocation.
    npc.svc.poll()
    assert npc.wire[-1][1] == struct.pack('<q',npc.actor.id)+b'\x02\x00\x00\x00\x00'
    assert npc.request(1,alias=alias).is_cancelled
    next_ticket=npc.start(); npc.request()
    assert next_ticket._entry.npc_session.current != alias
    assert npc.request(1,alias=alias).is_cancelled
    assert not npc.request(1).is_cancelled


def test_other_plugins_replacement_is_never_closed(npc):
    ticket=npc.start(); npc.request(); npc.svc.poll()
    npc.send(169, dialogue(npc.actor, 'unrelated'))
    last=list(npc.wire)
    npc.svc.poll()
    assert ticket.info.reason == 'superseded' and npc.wire == last


def test_unauthorized_native_branch_is_not_adopted(npc):
    ticket=npc.start(); npc.request(); npc.svc.poll()
    npc.send(169,dialogue(npc.actor,'next_scene'))  # No accepted branch action.
    assert ticket._entry.npc_session.closed


def test_unacknowledged_dialogue_times_out_and_native_open_is_excluded(npc):
    ticket=npc.start()
    with pytest.raises(RuntimeError,match='protection'):
        npc.svc.native.open(npc.player,'anvil')
    npc.now[0]+=6; npc.svc.poll()
    assert ticket.info.state == 'failed' and ticket.info.reason == 'timeout'


@pytest.mark.parametrize('scene', ['x\nstop','x @a','x"','rwui_fake','a'*129])
def test_scene_names_cannot_inject_native_commands(npc,scene):
    with pytest.raises(UIError):
        npc.ui.open_npc(npc.player,npc.context,scene=scene)
    assert not npc.calls and not npc.svc._active


def test_failed_native_dispatch_cleans_tags_and_leases(npc):
    dispatch=npc.svc.server.dispatch_command
    npc.svc.server.dispatch_command=lambda sender,cmd:False if cmd.startswith('dialogue ') else dispatch(sender,cmd)
    ticket=npc.start()
    assert ticket.info.state == 'failed' and not npc.svc.npc.sessions
    assert npc.actor.scoreboard_tags == npc.player.scoreboard_tags == []


def test_cancelled_native_open_is_not_reported_as_open(npc):
    original=npc.svc.server.dispatch_command
    def dispatch(sender,cmd):
        if cmd.startswith('tag '):return original(sender,cmd)
        event=SimpleNamespace(player=npc.player,packet_id=169,payload=dialogue(npc.actor),is_cancelled=False)
        npc.svc.npc.outgoing(event)
        event.is_cancelled=True
        npc.svc.observe('send',event)
        return True
    npc.svc.server.dispatch_command=dispatch
    ticket=npc.start()
    assert ticket.info.state == 'failed' and not npc.svc.npc.sessions


def test_disconnect_releases_owned_dialogue(npc):
    ticket=npc.start();npc.request()
    npc.svc.server.get_player=lambda _:None
    npc.svc.poll()
    assert ticket.info.state == 'cancelled' and not npc.svc.npc.sessions


def test_actual_bds_button_close_preserves_once_only_native_closing_commands(npc):
    ticket=npc.start();npc.request();npc.svc.poll()
    assert not npc.request(1).is_cancelled
    # Canonical payload captured from BDS after a native NPC button action.
    npc.send(169,struct.pack('<q',npc.actor.id)+bytes.fromhex('0200000000'))
    assert not ticket._entry.npc_session.closed
    assert not npc.request(2).is_cancelled
    assert npc.request(2).is_cancelled
    npc.now[0]+=.6;npc.svc.poll()
    assert ticket.info.state == 'closed' and ticket.info.reason == 'native_closed'


def test_server_close_does_not_authorize_late_button(npc):
    npc.start();npc.request()
    npc.send(169,close_packet(npc.actor.id))
    assert npc.request(1).is_cancelled
    assert not npc.request(2).is_cancelled


def test_other_npc_close_does_not_displace_owned_scene(npc):
    ticket=npc.start();npc.request();npc.svc.poll()
    npc.send(169,close_packet(-99))
    assert not ticket._entry.npc_session.closed
    assert not npc.request(1).is_cancelled


def test_revocation_after_native_button_close_still_blocks_closing_commands(npc):
    ticket=npc.start();npc.request();npc.svc.poll();npc.request(1)
    npc.send(169,close_packet(npc.actor.id))
    ticket.cancel()
    assert npc.request(2).is_cancelled
    npc.svc.poll()
    assert ticket.info.state == 'cancelled'


def test_native_cleanup_preserves_tags_added_during_dialogue_dispatch(npc):
    original=npc.svc.server.dispatch_command
    def dispatch(sender,cmd):
        result=original(sender,cmd)
        if cmd.startswith('dialogue '):
            npc.player.scoreboard_tags.append('another_plugin_tag')
        return result
    npc.svc.server.dispatch_command=dispatch
    npc.player.remove_scoreboard_tag=lambda tag:pytest.fail('Must use native removal, not the buggy IndexSet helper')
    ticket=npc.start()
    assert ticket.info.state == 'opening'
    assert npc.player.scoreboard_tags == ['another_plugin_tag']
