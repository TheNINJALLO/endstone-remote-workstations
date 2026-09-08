from concurrent.futures import ThreadPoolExecutor
from types import SimpleNamespace
import uuid

import pytest

from endstone_remote_workstations.api import Button, Menu, UIError, get_api
from endstone_remote_workstations.protocol import uvar
from endstone_remote_workstations.ui_service import UIService
from test_native_backend import live_adapter, activate


class Form:
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)
        self.buttons = []

    def add_button(self, label, icon=None):
        self.buttons.append((label, icon))


@pytest.fixture
def service(live_adapter):
    native, player, bridge, block, sent, calls = live_adapter
    provider = native.plugin
    provider._native = native
    provider.name, provider.is_enabled = 'remote_workstations', True
    provider.ui_api_version = (1, 0)
    owner = SimpleNamespace(name='example', is_enabled=True, server=native.server)
    other = SimpleNamespace(name='other', is_enabled=True, server=native.server)
    plugins = {p.name: p for p in (provider, owner, other)}
    native.server.plugin_manager = SimpleNamespace(get_plugin=plugins.get)
    service = UIService(provider, form_factory=Form)
    provider.get_ui_api = service.bind
    forms = []
    def show(form):
        forms.append(form)
        service.observe('send', SimpleNamespace(player=player, is_cancelled=False,
                        packet_id=100, payload=uvar(len(forms))+b'{}'))
    player.send_form = show
    player.close_form = lambda: calls.append(('close_form',))
    return service, get_api(owner), get_api(other), owner, other, player, forms, live_adapter


def test_dependency_handshake_and_thread_admission(service):
    svc, ui, _, owner, _, player, _, _ = service
    assert get_api(owner) is ui
    with pytest.raises(UIError):
        get_api(owner, minimum=(2, 0))
    with ThreadPoolExecutor(max_workers=1) as executor:
        with pytest.raises(UIError, match='owner thread'):
            executor.submit(ui.open_native, player, 'anvil').result()
    assert not svc._active


def test_actions_export_only_explicit_functions_and_recheck_permission(service):
    svc, ui, other_ui, _, _, player, _, _ = service
    called = []
    ui.register_action('private', lambda context: called.append(context.action))
    exported = ui.register_action('public', lambda context: context.ui is ui,
                                  permission='example.allowed', export=True)
    assert [a['id'] for a in other_ui.actions()] == [exported]
    denied = other_ui.invoke(player, 'example:private')
    allowed = other_ui.invoke(player, exported)
    assert not denied.done() and not allowed.done()
    svc.poll()
    with pytest.raises(UIError, match='not exported'):
        denied.result()
    assert allowed.result() is True and not called
    later = other_ui.invoke(player, exported)
    player.has_permission = lambda _: False
    svc.poll()
    with pytest.raises(UIError, match='permission'):
        later.result()


def test_namespace_unregister_and_registration_bounds(service):
    _, ui, other_ui, _, _, _, _, _ = service
    action = ui.register_action('one', lambda _: None)
    for name in ('other:one', 'native:anvil', 'bad name', 'one'):
        with pytest.raises(UIError):
            ui.register_action(name, lambda _: None)
    with pytest.raises(UIError):
        other_ui.unregister_action(action)
    ui.unregister_action('one')
    assert not ui.actions()
    for index in range(128):
        ui.register_action('a'+str(index), lambda _: None)
    with pytest.raises(UIError, match='limit'):
        ui.register_action('overflow', lambda _: None)


def test_form_callbacks_deferred_and_replay_cannot_invoke_twice(service):
    svc, ui, _, _, _, player, forms, _ = service
    observed = []
    ui.register_action('choose', lambda context: observed.append(('action', context.player.unique_id)))
    ticket = ui.show_menu(player, Menu('Example', (Button('Choose', 'choose'),)),
                          on_open=lambda event: observed.append(('open', event.state)),
                          on_close=lambda event: observed.append(('close', event.reason)))
    assert ticket.info.state == 'queued' and not forms
    svc.poll()
    assert ticket.info.state == 'open' and observed == [('open', 'open')]
    forms[-1].on_submit(player, 0)
    forms[-1].on_submit(player, 0)
    assert len(observed) == 1
    svc.poll()
    assert observed == [('open', 'open'), ('close', 'selected'), ('action', player.unique_id)]
    forms[-1].on_submit(player, 0)
    svc.poll()
    assert len(observed) == 3 and ticket.info.done and not svc._active


@pytest.mark.parametrize('selection', [-1, 20, True, '0'])
def test_invalid_form_selections_never_dispatch(service, selection):
    svc, ui, _, _, _, player, forms, _ = service
    called = []
    ui.register_action('choose', lambda _: called.append(True))
    ticket = ui.show_menu(player, Menu('Example', (Button('Choose', 'choose'),)))
    svc.poll()
    forms[-1].on_submit(player, selection)
    svc.poll()
    assert ticket.info.state == 'cancelled' and not called


def test_foreign_player_response_does_not_consume_menu(service):
    svc, ui, _, _, _, player, forms, _ = service
    ticket = ui.show_menu(player, Menu('Example', (Button('Anvil', 'native:anvil'),)))
    svc.poll()
    forms[-1].on_submit(SimpleNamespace(unique_id=uuid.uuid4()), 0)
    assert ticket.info.state == 'open' and not ticket._entry.responded


def test_native_navigation_waits_until_form_callback_and_does_not_write_items(service):
    svc, ui, _, _, _, player, forms, adapter = service
    native, _, _, _, sent, calls = adapter
    menu = ui.show_menu(player, Menu('Stations', (Button('Anvil', 'native:anvil'),)))
    svc.poll()
    forms[-1].on_submit(player, 0)
    assert not native.pending and not sent
    svc.poll()
    assert menu.info.done and not native.pending  # New request gets the next poll.
    svc.poll()
    assert native.pending[player.unique_id].kind == 'anvil' and not sent
    assert not calls


def test_native_ticket_reports_open_and_close_from_matching_generation(service):
    svc, ui, _, _, _, player, _, adapter = service
    native, _, bridge, _, _, _ = adapter
    events = []
    ticket = ui.open_native(player, 'anvil', on_open=lambda info: events.append(info.state),
                             on_close=lambda info: events.append(info.state))
    svc.poll()
    pending = native.pending[player.unique_id]
    native._prepare(player, pending)
    native.pending.pop(player.unique_id)
    native._activate(player, pending)
    svc.poll()
    assert ticket.info.state == 'open' and events == ['open']
    bridge.ready = True
    native.poll()
    svc.poll()
    assert ticket.info.state == 'closed' and events == ['open', 'closed']


def test_native_ticket_cannot_close_a_replacement_owned_by_commands(service):
    svc, ui, _, _, _, player, _, adapter = service
    native, _, _, _, sent, _ = adapter
    ticket = ui.open_native(player, 'anvil')
    svc.poll()
    native.forget(player)
    activate(native, player, 'craft')
    before = len(sent)
    ticket.cancel()
    svc.poll()
    assert native.sessions[player.unique_id].kind == 'craft' and len(sent) == before


def test_owner_disable_revokes_queued_actions_and_stale_forms(service):
    svc, ui, _, owner, _, player, forms, _ = service
    called = []
    ui.register_action('choose', lambda _: called.append(True))
    ticket = ui.show_menu(player, Menu('Example', (Button('Choose', 'choose'),)),
                          on_close=lambda _: called.append('close'))
    svc.poll()
    future = ui.invoke(player, 'choose')
    forms[-1].on_submit(player, 0)
    owner.is_enabled = False
    svc.release(owner)
    svc.poll()
    assert ticket.info.state == 'cancelled' and not called
    with pytest.raises(UIError, match='disabled'):
        future.result()
    with pytest.raises(UIError):
        ui.register_action('late', lambda _: None)


def test_external_screen_replacement_is_never_closed_by_sdk(service):
    svc, ui, _, _, _, player, forms, adapter = service
    ticket = ui.show_menu(player, Menu('Example', (Button('Anvil', 'native:anvil'),)))
    svc.poll()
    svc.observe('send', SimpleNamespace(player=player, is_cancelled=False, packet_id=100, payload=b'\x7f{}'))
    svc.poll()
    assert ticket.info.reason == 'superseded'
    assert ('close_form',) not in adapter[-1]
    forms[-1].on_submit(player, 0)
    svc.poll()
    assert not adapter[0].pending


def test_permission_revocation_and_action_exceptions_do_not_escape_poll(service):
    svc, ui, _, _, _, player, forms, _ = service
    called = []
    ui.register_action('choose', lambda _: called.append(True), permission='example.choose')
    ticket = ui.show_menu(player, Menu('Example', (Button('Choose', 'choose'),)))
    svc.poll()
    player.has_permission = lambda _: False
    forms[-1].on_submit(player, 0)
    svc.poll()
    assert ticket.info.done and not called
    ui.register_action('broken', lambda _: 1/0)
    failed = ui.invoke(player, 'broken')
    ui.register_action('good', lambda _: 42)
    good = ui.invoke(player, 'good')
    svc.poll()
    assert isinstance(failed.exception(), ZeroDivisionError) and good.result() == 42


def test_unsupported_screens_are_discoverable_and_fail_without_mutations(service):
    svc, ui, _, _, _, player, _, adapter = service
    rows = ui.capabilities(player)
    assert len(rows) == 69
    ender = next(r for r in rows if r['id'] == 'enderchest')
    assert not ender['available']
    ticket = ui.open_native(player, 'ec')
    svc.poll()
    assert ticket.info.state == 'failed' and ticket.info.reason
    assert not adapter[0].pending and not adapter[-2] and not adapter[-1]


def test_session_bounds_immutable_menu_and_shutdown_completion(service):
    svc, ui, _, _, _, player, forms, adapter = service
    buttons = [Button('Anvil', 'native:anvil')]
    ticket = ui.show_menu(player, Menu('Example', buttons))
    buttons.clear()
    svc.poll()
    assert len(forms[-1].buttons) == 1
    with pytest.raises(UIError, match='already'):
        ui.open_native(player, 'anvil')
    queued = ui.invoke(player, 'anything')
    svc.shutdown()
    assert ticket.info.done and queued.cancelled() and not svc._active
    assert ('close_form',) in adapter[-1]


def test_disconnect_reconnect_does_not_replay_an_old_action(service):
    svc, ui, _, _, _, player, _, _ = service
    called = []
    ui.register_action('choose', lambda _: called.append(True))
    request = ui.invoke(player, 'choose')
    svc.invalidate_player(player, 'disconnected')
    # Even if get_player finds the same UUID after reconnect, retire the old call.
    svc.poll()
    assert not called
    with pytest.raises(UIError, match='lifecycle'):
        request.result()
