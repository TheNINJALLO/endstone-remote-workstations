"""Fault injection into the real journal plus an independent simulated save file."""
from dataclasses import replace
import json
import os
from pathlib import Path
import sys
import uuid

from endstone import nbt
from endstone_remote_workstations import item_nbt
from endstone_remote_workstations.held_items import HeldSelection, HeldStack
from endstone_remote_workstations.journal import Journal

folder, boundary = Path(sys.argv[1]), sys.argv[2]
stack = HeldStack('minecraft:white_shulker_box', 1, 0, item_nbt.encode(nbt.CompoundTag()))
before = HeldSelection(str(uuid.uuid4()), 0, stack.with_identity(str(uuid.uuid4())))
tag = item_nbt.decode(before.stack.nbt)
tag['Items'] = nbt.ListTag([nbt.CompoundTag({'Slot': nbt.ByteTag(0),
    'Name': nbt.StringTag('minecraft:stone'), 'Count': nbt.ByteTag(1)})])
after = replace(before, stack=replace(before.stack, nbt=item_nbt.encode(tag)))
(folder / 'external.json').write_text(json.dumps(before.record()))
journal = Journal(folder / 'held.sqlite3')
lease = journal.reserve_held('crash_probe', before.record())
if boundary == 'reserved':
    os._exit(73)
intent = journal.prepare_held(lease, 0, before.record(), after.record())
if boundary == 'prepared':
    os._exit(73)
journal.begin_held_apply(intent)
if boundary == 'applying':
    os._exit(73)
(folder / 'external.json').write_text(json.dumps(after.record()))
if boundary == 'external-written':
    os._exit(73)
journal.mark_held_applied(intent)
os._exit(73)
