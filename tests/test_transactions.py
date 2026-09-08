from dataclasses import replace
import random
import pytest
from endstone_remote_workstations.model import Action, Item, Ref, Rejected, Request, Slot, plan_storage, return_cursor
from endstone_remote_workstations.storage import StorageHarness


def ref(area, index, net_id=0):
    return Ref(Slot(area, index), net_id)


def request(state, *actions, number=-1):
    return Request(state.generation, number, state.version, tuple(actions))


def test_multi_action_split_drag_is_atomic(state):
    plan = plan_storage(state, request(state,
        Action("take", ref("player", 0, 1), ref("cursor", 0), 16),
        Action("place", ref("cursor", 0), ref("storage", 0), 7),
        Action("place", ref("cursor", 0), ref("storage", 1), 9)))
    assert state.player[0].count == 32 and state.storage[0] is None
    assert (plan.after.player[0].count, plan.after.storage[0].count, plan.after.storage[1].count) == (16, 7, 9)
    assert plan.after.cursor == (None,)
    assert plan.after.storage[0].metadata == b"opaque-custom-nbt"


def test_drop_is_not_committed_when_later_action_invalid(state):
    engine = StorageHarness(state)
    response = engine.handle(request(state, Action("drop", ref("player", 0, 1), count=3),
                                     Action("create", ref("storage", 0), count=64)))
    assert not response.ok and response.resync
    assert engine.state == state and not engine.committed_drops


@pytest.mark.parametrize("count", [-1, 0, 33, 65, 255, True])
def test_reject_bad_counts_without_mutation(state, count):
    with pytest.raises(Rejected):
        plan_storage(state, request(state, Action("take", ref("player", 0, 1), ref("storage", 0), count)))
    assert state.player[0].count == 32


@pytest.mark.parametrize("slot", [Slot("player", -1), Slot("storage", 27), Slot("cursor", 1), Slot("anvil_result", 0)])
def test_bounds_and_workstation_roles_rejected(state, slot):
    with pytest.raises(Rejected):
        plan_storage(state, request(state, Action("take", ref("player", 0, 1), Ref(slot, 0), 1)))


def test_stale_id_version_generation_filters_and_locked_slots(state):
    good = Action("take", ref("player", 0, 1), ref("storage", 0), 1)
    for req in (replace(request(state, good), generation="old"), replace(request(state, good), version=10),
                request(state, replace(good, source=ref("player", 0, 99))), request(state, good, number=-2)):
        with pytest.raises(Rejected):
            plan_storage(state, req)
    with pytest.raises(Rejected):
        plan_storage(state, request(state, good), locked=frozenset([Slot("player", 0)]))
    with pytest.raises(Rejected):
        plan_storage(state, request(state, good), accept=lambda slot, item: slot.area != "storage")


def test_overflow_and_different_nbt_rejected(state):
    for item in (Item("minecraft:stone", 64, 64, 2, b"opaque-custom-nbt"),
                 Item("minecraft:stone", 1, 64, 2, b"renamed-and-enchanted")):
        altered = replace(state, storage=(item,)+(None,)*26, next_net_id=3)
        with pytest.raises(Rejected):
            plan_storage(altered, request(altered, Action("place", ref("player", 0, 1), ref("storage", 0, 2), 1)))


def test_swaps_and_drop_preserve_items_and_xp(state):
    swapped = plan_storage(state, request(state, Action("swap", ref("player", 0, 1), ref("storage", 0)))).after
    result = plan_storage(swapped, request(swapped, Action("drop", ref("storage", 0, 1), count=5), number=-3))
    assert result.after.storage[0].count == 27
    assert result.drops[0].count == 5 and result.drops[0].metadata == state.player[0].metadata
    assert result.after.xp == state.xp


def test_every_request_gets_response_and_replay_never_reapplies(state):
    engine = StorageHarness(state, cache_limit=1)
    req = request(state, Action("take", ref("player", 0, 1), ref("storage", 0), 2))
    ok = engine.handle(req)
    assert ok.ok and engine.handle(req) == ok and engine.state.version == 1
    assert not engine.handle(replace(req, actions=())).ok
    result = engine.handle_batch((replace(req, request_id=-3), replace(req, request_id=-5)))
    assert len(result) == 2 and all(not r.ok and r.resync for r in result)
    assert not engine.handle(req).ok  # evicted ID remains stale


def test_cas_rejects_intervening_inventory_plugin_change(state):
    engine = StorageHarness(state)
    def concurrent_change(harness):
        harness.state = replace(harness.state, xp=9, version=1)
    response = engine.handle(request(state, Action("take", ref("player", 0, 1), ref("storage", 0), 2)), concurrent_change)
    assert not response.ok and engine.state.xp == 9 and engine.state.storage[0] is None


def test_cleanup_overflow_is_retained_and_idempotent(state):
    full = tuple(Item("minecraft:stone", 64, 64, i+1) for i in range(36))
    item = Item("minecraft:diamond", 5, 64, 37, b"full-NBT")
    full_state = replace(state, player=full, cursor=(item,), next_net_id=38)
    cleaned, overflow = return_cursor(full_state)
    assert overflow == (item,) and cleaned.cursor == (None,)
    again, second_overflow = return_cursor(cleaned)
    assert again == cleaned and second_overflow == ()


def test_random_storage_sequences_conserve_all_metadata(state):
    rng = random.Random(2169)
    current = state
    for index in range(1000):
        occupied = [Slot(area, i) for area in ("player", "storage", "cursor")
                    for i, item in enumerate(getattr(current, area)) if item]
        source = rng.choice(occupied)
        item = getattr(current, source.area)[source.index]
        area = rng.choice(("player", "storage", "cursor"))
        dest = Slot(area, rng.randrange(len(getattr(current, area))))
        old = getattr(current, area)[dest.index]
        action = Action("take", Ref(source, item.net_id), Ref(dest, old.net_id if old else 0), rng.randint(1, item.count))
        try:
            current = plan_storage(current, request(current, action, number=-(index*2+1))).after
        except Rejected:
            pass
        items = [i for i in (*current.player, *current.storage, *current.cursor) if i]
        assert sum(i.count for i in items) == 32
        assert all(i.type_id == "minecraft:stone" and i.metadata == b"opaque-custom-nbt" for i in items)
