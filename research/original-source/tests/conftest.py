import pytest
from endstone_remote_workstations.model import Item, Snapshot


@pytest.fixture
def state():
    return Snapshot("generation-one", 0,
                    (Item("minecraft:stone", 32, 64, 1, b"opaque-custom-nbt"),)+(None,)*35,
                    (None,)*27, next_net_id=2)

