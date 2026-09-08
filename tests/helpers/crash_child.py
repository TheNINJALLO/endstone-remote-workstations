from pathlib import Path
import os
import sys
from endstone_remote_workstations.journal import Journal
from endstone_remote_workstations.model import Action, Ref, Request, Slot, Snapshot, plan_storage

folder, boundary = Path(sys.argv[1]), sys.argv[2]
state = Snapshot.deserialize((folder / "initial.json").read_text())
plan = plan_storage(state, Request(state.generation, -1, state.version,
                    (Action("take", Ref(Slot("player", 0), 1), Ref(Slot("storage", 0), 0), 7),)))
journal = Journal(folder / "journal.sqlite3")
identity = journal.prepare("owner", -1, plan)
if boundary != "prepared":
    raw = plan.after.serialize() if boundary != "external-partial" else '{"partial":true}'
    with (folder / "external.json").open("w") as stream:
        stream.write(raw)
        stream.flush()
        os.fsync(stream.fileno())
if boundary == "applied-memory":
    journal.mark_applied(identity)
os._exit(73)
