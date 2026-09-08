from concurrent.futures import ThreadPoolExecutor

import pytest

from endstone_remote_workstations.api import API_VERSION, UIError
from test_developer_api import service
from test_native_backend import live_adapter
from test_held_items import selected


def test_held_preflight_is_an_additive_owner_scoped_dependency_api(service):
    svc, ui, _, owner, _, player, _, _ = service
    player.inventory = selected().inventory
    assert API_VERSION == (1, 6)
    info = ui.inspect_held_item(player)
    assert info.kind == 'shulker' and not info.native_open_available
    assert not svc._active
    row = next(r for r in ui.capabilities(player) if r['id'] == 'shulker')
    assert not row['available'] and row['backend'] == 'none'
    with ThreadPoolExecutor(max_workers=1) as executor:
        with pytest.raises(UIError, match='owner thread'):
            executor.submit(ui.inspect_held_item, player).result()
    ui.dispose()
    with pytest.raises(UIError, match='disposed'):
        ui.inspect_held_item(player)


def test_preflight_rechecks_permission_and_player_lifecycle(service):
    svc, ui, _, _, _, player, _, _ = service
    player.inventory = selected().inventory
    player.has_permission = lambda permission: False
    with pytest.raises(UIError, match='permission'):
        ui.inspect_held_item(player)
    player.has_permission = lambda permission: True
    player.is_dead = True
    with pytest.raises(UIError, match='living'):
        ui.inspect_held_item(player)
