import json
from pathlib import Path
import pytest
from endstone_remote_workstations.model import Rejected
from endstone_remote_workstations.protocol import (
    CodecError, ItemDescriptor, Open, Reader, Registry, StorageAdapter, WireRef,
    close_payload, decode_storage_requests, error_responses, inventory_content, inventory_slot, svar, uvar,
)

FIXTURES = json.loads((Path(__file__).parent / "fixtures/protocol-2169.json").read_text())
LIVE_CONTROLS = json.loads((Path(__file__).parent / "fixtures/windows-live-controls-2169.json").read_text())

def payload(name):
    return bytes.fromhex(FIXTURES[name])


def test_open_close_exact_r26_schema():
    assert Open(1, 0, 1, 64, -2).encode() == payload("container_open")
    assert Open.decode(payload("container_open")) == Open(1, 0, 1, 64, -2)
    assert close_payload(1, 0, True) == payload("container_close")


@pytest.mark.parametrize("session", LIVE_CONTROLS["sessions"], ids=lambda row: f"live-window-{row['window']}")
def test_actual_windows_player_inventory_control_capture(session):
    from bedrock_protocol.packets.packet import ContainerOpenPacket, ContainerClosePacket
    raw = bytes.fromhex(session["open_send"])
    opened = Open.decode(raw)
    assert opened == Open(session["window"], 255, *session["position"], -1)
    assert opened.encode() == raw
    external = ContainerOpenPacket()
    external.deserialize(raw)
    assert external.serialize() == raw
    # These real native-player-inventory closes use 247, while their opens use
    # 255. Do not generalize an open/close type-equality assumption to this path.
    for direction in ("close_receive", "close_send"):
        closed = bytes.fromhex(session[direction])
        assert closed == close_payload(session["window"], 247, False)
        external_close = ContainerClosePacket()
        external_close.deserialize(closed)
        assert external_close.serialize() == closed


def test_storage_request_slot_netid_and_discriminants(state):
    wire, = decode_storage_requests(payload("take_five"))
    request = StorageAdapter("chest").request(wire, state)
    assert request.request_id == -1 and request.actions[0].count == 5
    assert request.actions[0].source.net_id == 1
    assert request.actions[0].source.slot.area == "player"
    bad = bytearray(payload("take_five"))
    bad[4] = 1  # inner type must match outer variant
    with pytest.raises(CodecError):
        decode_storage_requests(bytes(bad))


def test_every_truncated_prefix_and_extra_data_are_rejected():
    raw = payload("take_five")
    for length in range(len(raw)):
        with pytest.raises(CodecError):
            decode_storage_requests(raw[:length])
    with pytest.raises(CodecError):
        decode_storage_requests(raw+b"\x00")
    for data in (b"\x80\x00", b"\xff\xff\xff\xff\x1f", b"\x80"*5):
        with pytest.raises(CodecError):
            Reader(data).uvar()


@pytest.mark.parametrize("role,slot,dynamic", [(0, 0, None), (13, 0, None), (30, 0, None), (59, 1, None),
                                            (28, 9, None), (29, 0, None), (7, 27, None), (7, 0, 1), (63, 0, 4)])
def test_storage_rejects_unowned_workstation_and_dynamic_slots(role, slot, dynamic):
    with pytest.raises(Rejected):
        StorageAdapter("chest").ref(WireRef(role, dynamic, slot, 0))


def test_content_slot_and_error_fixtures():
    assert inventory_content(1, []) == payload("empty_content")
    assert inventory_slot(1, 0, ItemDescriptor()) == payload("empty_slot")
    assert error_responses([-1]) == payload("error_response")
    # InventoryContent uses uvarint32; InventorySlot uses uint8 in r26_u4.
    assert inventory_content(200, [])[:2] == b"\xc8\x01"
    assert inventory_slot(200, 0, ItemDescriptor())[:2] == b"\xc8\x00"


def test_pinned_dependency_open_close_request_response_agree():
    from bedrock_protocol.packets.packet import ContainerOpenPacket, ContainerClosePacket, ItemStackRequestPacket, ItemStackResponsePacket
    from bedrock_protocol.packets.types import BlockPos
    from bedrock_protocol.packets.types.item_stack_response import ItemStackResponse
    assert ContainerOpenPacket(1, 0, BlockPos(1, 64, -2), -1).serialize() == payload("container_open")
    assert ContainerClosePacket(1, 0, True).serialize() == payload("container_close")
    request = ItemStackRequestPacket()
    request.deserialize(payload("take_five"))
    assert request.requests[0].actions[0].amount == 5
    assert request.serialize() == payload("take_five")
    assert ItemStackResponsePacket([ItemStackResponse(1, -1)]).serialize() == payload("error_response")


def test_pinned_dependency_does_not_validate_inner_discriminant():
    # Audit finding, not an exploit claim: our bounded wrapper must validate this.
    from bedrock_protocol.packets.packet import ItemStackRequestPacket
    raw = bytearray(payload("take_five"))
    raw[4] = 3
    packet = ItemStackRequestPacket()
    packet.deserialize(bytes(raw))
    assert packet.requests[0].actions[0].amount == 5
    with pytest.raises(CodecError):
        decode_storage_requests(bytes(raw))


def test_nbt_type_and_opaque_metadata_preservation():
    from rapidnbt import CompoundTag, ShortTag
    tag = CompoundTag()
    tag.set("RepairCost", ShortTag(7))
    assert tag.to_binary_nbt() == payload("item_nbt_short")
    restored = CompoundTag.from_binary_nbt(payload("item_nbt_short"))
    assert restored.to_binary_nbt() == payload("item_nbt_short")
    assert "7s" in restored.to_snbt()
    # A wire descriptor preserves NBT/restrictions verbatim; no lossy ItemStack conversion.
    data = b"\xff\xff\x01"+payload("item_nbt_short")+b"\x00"*8
    encoded = ItemDescriptor(123, 1, 0, 5, 0, data).encode()
    assert encoded.endswith(data)


def test_registry_provenance_change_and_duplicate_validation():
    registry = Registry()
    provenance = dict(source="server-item-registry", bds_hash="a"*64, protocol=2169, behavior_pack_hash="b"*64)
    assert not registry.replace([("minecraft:stone", 2, 64)], **provenance)
    before = registry.epoch
    assert registry.replace([("minecraft:stone", 3, 64)], **provenance)
    assert registry.epoch != before
    with pytest.raises(Rejected):
        registry.replace([("minecraft:stone", 3, 64)], **{**provenance, "source": "client"})
    with pytest.raises(Rejected):
        registry.replace([("minecraft:stone", 3, 64), ("minecraft:dirt", 3, 64)], **provenance)


@pytest.mark.parametrize("number", [-(2**31), -32768, -1, 0, 1, 127, 128, 2**31-1])
def test_signed_int_boundaries(number):
    assert Reader(svar(number)).svar() == number
