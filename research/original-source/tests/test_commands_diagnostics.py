from dataclasses import replace
import json
from pathlib import Path
from types import SimpleNamespace
import pytest
from endstone_remote_workstations.catalog import capabilities, command_metadata, configuration, permission_metadata
from endstone_remote_workstations.diagnostics import Capture, sanitized
from endstone_remote_workstations.presentation import button, title
from endstone_remote_workstations.protocol import Open


def test_catalog_has_separate_permission_and_precise_disabled_reason():
    rows = capabilities()
    assert len(rows) == 69 and len({r["id"] for r in rows}) == 69
    permissions = permission_metadata()
    for row in rows:
        assert row["enabled_by_default"] is False
        assert len(row["reason"]) > 25
        assert row["permission"] in permissions
        if row["administrator_only"]:
            assert permissions[row["permission"]]["default"] == "op"
    assert next(r for r in rows if r["id"] == "craft")["slot_roles"]["craftinginputcontainer"] == 13
    assert next(r for r in rows if r["id"] == "anvil")["slot_roles"]["anvilinputcontainer"] == 0


def test_alias_configuration_rejects_enchant_duplicates_and_injection():
    commands, mapping = command_metadata(configuration())
    assert "enchant" not in commands and mapping["etable"] == "enchanting"
    assert mapping["ec"] == "enderchest" and mapping["workbench"] == "craft"
    assert command_metadata({"aliases": {"enabled": False}})[1] == {}
    for name in ("enchant", "workstations", "chest;give", "../anvil", "CHEST"):
        with pytest.raises(ValueError):
            command_metadata({"aliases": {"chest": [name]}})
    with pytest.raises(ValueError):
        command_metadata({"aliases": {"chest": ["same"], "anvil": ["same"]}})


def test_sensitive_and_unknown_packet_bytes_never_captured(tmp_path):
    secret = b"JWT-secret-book-text-command-password"
    for packet in (1, 3, 4, 9, 97, 98, 169, 78, 90, 132, 133, 303, 313, 999):
        assert sanitized(packet, secret) is None
    for packet in (49, 50, 51, 52, 56, 147, 148, 162):
        assert "payload_hex" not in sanitized(packet, secret)
    capture = Capture(tmp_path, capacity=8, maximum_file_bytes=4096)
    capture.record("receive", "private-uuid", 147, secret, True)
    capture.record("send", "private-uuid", 46, Open(1, 0, 0, 64, 0).encode(), True)
    capture.close()
    text = "".join(p.read_text() for p in tmp_path.glob("*.jsonl"))
    assert "private-uuid" not in text and secret.hex() not in text and "JWT" not in text
    rows = [json.loads(line) for line in text.splitlines()]
    assert len(rows) == 2 and rows[0]["player"] == rows[1]["player"]
    assert rows[1]["payload_hex"] == Open(1, 0, 0, 64, 0).encode().hex()


def test_bad_shape_no_raw_bytes_and_bounded_capture(tmp_path):
    assert "payload_hex" not in sanitized(46, b"secret")
    assert sanitized(147, b"x"*(1024*1024+1)) is None
    capture = Capture(tmp_path, capacity=1, maximum_file_bytes=4096, file_count=2)
    for _ in range(10000):
        capture.record("receive", "p", 147, b"anything", True)
    capture.close()
    assert len(list(tmp_path.glob("*.jsonl"))) <= 2
    assert all(p.stat().st_size <= 4096 for p in tmp_path.glob("*.jsonl"))


@pytest.mark.parametrize("size", [1, 5, 9, 18, 27, 36, 45, 54])
def test_presentation_contract_and_path_only_icons(size):
    assert title("Catalog", size).startswith("§c§h§e§s§t")
    assert title("Selector", furnace=True, lit=True) == "§f§u§r§n§a§c§e§l§i§t§rSelector"
    text, icon = button("Stone", "textures/blocks/stone", count=2, durability=7, lore=("Details",))
    assert text == "stack#02dur#07§rStone§r\nDetails" and icon == "textures/blocks/stone"
    with pytest.raises(ValueError):
        button("Stone", "minecraft:stone")


def test_real_endstone_import_metadata_and_disabled_handler():
    pytest.importorskip("endstone")
    from endstone_remote_workstations.plugin import RemoteWorkstations
    assert RemoteWorkstations.api_version == "0.11"
    sender = SimpleNamespace(messages=[], has_permission=lambda permission: True)
    sender.send_message = sender.messages.append
    sender.send_error_message = sender.messages.append
    # Methods are exercised without fabricating any inventory or native API.
    RemoteWorkstations._open(None, sender, "anvil")
    assert "player in Minecraft" in sender.messages[-1]
    sender.has_permission = lambda permission: False
    RemoteWorkstations._open(None, sender, "chest")
    assert "permission" in sender.messages[-1]
