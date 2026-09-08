"""Real recorded packets establish the helper adapter's wire expectations."""
import json
from pathlib import Path
from types import SimpleNamespace

import pytest

from integrations.inventoryui_2169 import (
    Container, RequestPacket, Response, ResponseContainer, ResponsePacket, ResponseSlot,
    decode_request, decode_responses, encode_responses,
)
from endstone_remote_workstations.protocol import CodecError

FIXTURE = json.loads((Path(__file__).parent/'fixtures/helper-ender-2169.json').read_text())
REQUESTS = [bytes.fromhex(p['payload_hex']) for p in FIXTURE['packets'] if p['packet_id'] == 147]
RESPONSES = [bytes.fromhex(p['payload_hex']) for p in FIXTURE['packets'] if p['packet_id'] == 148]


def test_actual_ender_transfers_have_the_correct_helper_slot_and_item_id():
    expected = [(-237, 1, 1, 28, 1, 21, 7, 0, 0),
                (-239, 0, 1, 7, 0, 21, 59, 0, 0),
                (-241, 1, 1, 59, 0, 21, 28, 1, 0)]
    for raw, expected_action in zip(REQUESTS[:3], expected, strict=True):
        packet = RequestPacket()
        packet.deserialize(raw)
        request, = packet.request.request_data
        action, = request.request_actions
        data = action.action_data
        a, b = data.source, data.distination
        assert (request.client_request_id, action.action_type, data.amount,
                a.container.container_enum, a.slot, a.net_id,
                b.container.container_enum, b.slot, b.net_id) == expected_action


@pytest.mark.parametrize('raw', RESPONSES, ids=lambda b: b.hex()[:12])
def test_actual_bds_response_round_trip_preserves_all_optional_markers(raw):
    responses = decode_responses(raw)
    assert ResponsePacket(responses).serialize() == raw
    assert ResponsePacket(responses).get_packet_id() == 148


def test_helper_object_shape_encodes_a_real_success_response():
    # InventoryUI's response builder returns structurally identical objects;
    # there is no requirement to replace its container manager to encode them.
    response = SimpleNamespace(result=0, request_id=-237, container_infos=[
        SimpleNamespace(container=SimpleNamespace(container_enum=28, dynamic_slot=None), slots=[
            SimpleNamespace(slot=1, hotbar_slot=1, count=0, item_stack_id=0,
                            custom_name='', filtered_custom_name='', durability_correction=0)]),
        SimpleNamespace(container=SimpleNamespace(container_enum=7, dynamic_slot=None), slots=[
            SimpleNamespace(slot=0, hotbar_slot=0, count=1, item_stack_id=21,
                            custom_name='Remote Blade', filtered_custom_name='Remote Blade', durability_correction=0)])])
    assert encode_responses([response]) == RESPONSES[0]


def test_real_error_retains_result_code_and_both_container_markers():
    assert encode_responses([Response(result=50, request_id=-255)]) == RESPONSES[-1]


def test_every_actual_request_and_response_truncation_is_rejected():
    for raw in REQUESTS:
        for length in range(len(raw)):
            with pytest.raises(CodecError):
                decode_request(raw[:length])
        with pytest.raises(CodecError):
            decode_request(raw+b'\x00')
    for raw in RESPONSES:
        for length in range(len(raw)):
            with pytest.raises(CodecError):
                decode_responses(raw[:length])
        with pytest.raises(CodecError):
            decode_responses(raw+b'\x00')


@pytest.mark.parametrize('net_id', [None, 0, -1, True, 2**31])
def test_nonempty_helper_responses_require_a_real_positive_stack_id(net_id):
    response = Response(container_infos=[ResponseContainer(Container(7), [ResponseSlot(count=1, item_stack_id=net_id)])])
    with pytest.raises(CodecError):
        encode_responses([response])


def test_response_collection_and_string_bounds():
    with pytest.raises(CodecError):
        encode_responses([Response()]*101)
    response = Response(container_infos=[ResponseContainer(Container(7), [ResponseSlot(custom_name='x'*4097)])])
    with pytest.raises(CodecError):
        encode_responses([response])


def test_packet_import_does_not_monkey_patch_helper_or_register_game_events():
    import sys
    assert 'endstone_inventoryui.listener' not in sys.modules
