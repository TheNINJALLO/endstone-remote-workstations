from dataclasses import replace
import os
from pathlib import Path
import subprocess
import sys
import threading
import uuid

import pytest
from endstone import nbt

from endstone_remote_workstations import item_nbt
from endstone_remote_workstations.held_items import HeldSelection, HeldStack
from endstone_remote_workstations.journal import Journal, JournalWorker
from endstone_remote_workstations.model import Rejected
from test_held_items import Stack


def source():
    stack = HeldStack.capture(Stack()).with_identity(str(uuid.uuid4()))
    return HeldSelection(str(uuid.uuid4()), 3, stack)


def changed(before, count=1):
    tag = item_nbt.decode(before.stack.nbt)
    tag['Items'] = nbt.ListTag([nbt.CompoundTag({'Slot': nbt.ByteTag(0),
        'Name': nbt.StringTag('minecraft:stone'), 'Count': nbt.ByteTag(count)})])
    return replace(before, stack=replace(before.stack, nbt=item_nbt.encode(tag)))


def test_persistent_global_item_and_player_exclusivity_survive_restart(tmp_path):
    path = tmp_path / 'held.sqlite3'
    a, b = Journal(path), Journal(path)
    before = source()
    lease = a.reserve_held('first', before.record())
    for contender in (before, replace(before, owner=str(uuid.uuid4())),
                      replace(source(), owner=before.owner)):
        with pytest.raises(Rejected, match='reservation'):
            b.reserve_held('second', contender.record())
    a.close()
    assert b.recover()[0][0] == lease
    assert b.held_counts() == {'quarantined': 1}
    with pytest.raises(Rejected):
        b.reserve_held('first', before.record())
    assert b.db.execute('SELECT COUNT(*) FROM recovery').fetchone()[0] == 0
    b.close()


def test_unidentified_stack_is_never_admitted_or_silently_tagged(tmp_path):
    journal = Journal(tmp_path / 'held.sqlite3')
    before = source()
    raw = replace(before, stack=HeldStack.capture(Stack()))
    with pytest.raises(Rejected, match='identified'):
        journal.reserve_held('example', raw.record())
    assert raw.stack.durable_id is None and journal.held_counts() == {}
    journal.close()


def test_unwritten_cleanup_is_idempotent_and_allows_a_new_generation(tmp_path):
    journal = Journal(tmp_path / 'held.sqlite3')
    before = source()
    lease = journal.reserve_held('example', before.record())
    assert journal.close_held(lease, 'cancel before opening') == 'released'
    assert journal.close_held(lease, 'duplicate cleanup') == 'released'
    other = journal.reserve_held('example', before.record())
    assert other != lease
    with pytest.raises(Rejected, match='Stale'):
        journal.prepare_held(lease, 0, before.record(), changed(before).record())
    journal.close_held(other, 'shutdown')
    assert journal.recover() == []
    journal.close()


def test_write_order_stale_revision_replay_and_quarantine(tmp_path):
    journal = Journal(tmp_path / 'held.sqlite3')
    before = source()
    after, later = changed(before), changed(before, 2)
    lease = journal.reserve_held('example', before.record())
    intent = journal.prepare_held(lease, 0, before.record(), after.record())
    with pytest.raises(Rejected):
        journal.mark_held_applied(intent)
    with pytest.raises(Rejected):
        journal.prepare_held(lease, 0, before.record(), after.record())
    journal.begin_held_apply(intent)
    with pytest.raises(Rejected):
        journal.begin_held_apply(intent)
    journal.mark_held_applied(intent)
    with pytest.raises(Rejected):
        journal.mark_held_applied(intent)
    for revision, previous in ((0, after), (1, before)):
        with pytest.raises(Rejected):
            journal.prepare_held(lease, revision, previous.record(), later.record())
    next_intent = journal.prepare_held(lease, 1, after.record(), later.record())
    assert journal.close_held(lease, 'disconnect') == 'quarantined'
    assert journal.close_held(lease, 'shutdown') == 'quarantined'
    with pytest.raises(Rejected):
        journal.begin_held_apply(next_intent)
    assert journal.db.execute('SELECT COUNT(*) FROM recovery').fetchone()[0] == 0
    journal.close()


@pytest.mark.parametrize('what', ['owner', 'slot', 'type', 'identity', 'revision'])
def test_write_cannot_rebind_to_another_backing_item(tmp_path, what):
    journal = Journal(tmp_path / 'held.sqlite3')
    before = source()
    after = changed(before)
    lease = journal.reserve_held('example', before.record())
    if what == 'owner':
        after = replace(after, owner=str(uuid.uuid4()))
    elif what == 'slot':
        after = replace(after, slot=4)
    elif what == 'type':
        after = replace(after, stack=replace(after.stack, type_id='minecraft:blue_shulker_box'))
    elif what == 'identity':
        after = replace(after, stack=source().stack)
    with pytest.raises(Rejected):
        journal.prepare_held(lease, True if what == 'revision' else 0, before.record(), after.record())
    assert journal.held_counts() == {'reserved': 1}
    assert journal.db.execute('SELECT COUNT(*) FROM held_writes').fetchone()[0] == 0
    journal.close()


@pytest.mark.parametrize('boundary', ['reserved', 'prepared', 'applying', 'external-written', 'applied-memory'])
def test_process_death_quarantines_without_replaying_external_item_write(tmp_path, boundary):
    # Real process death, with a file standing in for BDS's independent save store.
    # This deliberately makes NO claim of live BDS crash qualification.
    child = Path(__file__).parent / 'helpers/held_crash_child.py'
    env = {**os.environ, 'PYTHONPATH': str(Path(__file__).resolve().parents[1] / 'src')}
    result = subprocess.run([sys.executable, str(child), str(tmp_path), boundary],
                            env=env, capture_output=True, timeout=15)
    assert result.returncode == 73, result.stderr.decode('utf-8', 'replace')
    external = (tmp_path / 'external.json').read_bytes()
    journal = Journal(tmp_path / 'held.sqlite3')
    rows = journal.recover()
    assert len(rows) == 1 and rows[0][2] == 'quarantined'
    assert journal.recover() == rows
    assert (tmp_path / 'external.json').read_bytes() == external
    assert journal.db.execute('SELECT COUNT(*) FROM recovery').fetchone()[0] == 0
    journal.close()


def test_held_operations_use_the_bounded_disk_worker_and_detached_records(tmp_path):
    worker = JournalWorker(tmp_path / 'held.sqlite3')
    before = source()
    lease = worker.submit('reserve_held', 'example', before.record()).result(timeout=5)
    assert worker.submit('held_counts').result(timeout=5) == {'reserved': 1}
    assert worker.submit('close_held', lease, 'cancel').result(timeout=5) == 'released'
    worker.close()


def test_queued_record_is_detached_before_caller_can_change_owner_or_item(tmp_path):
    worker = JournalWorker(tmp_path / 'held.sqlite3')
    worker.ready.result(timeout=5)
    gate = threading.Event()
    worker.pool.submit(gate.wait, 5)
    before = source()
    record = before.record()
    try:
        future = worker.submit('reserve_held', 'example', record)
        record['owner'] = str(uuid.uuid4())
        record['stack']['nbt'] = 'corrupted after submission'
    finally:
        gate.set()
    lease = future.result(timeout=5)
    with pytest.raises(Rejected, match='reservation'):
        worker.submit('reserve_held', 'example', before.record()).result(timeout=5)
    assert worker.submit('close_held', lease, 'cancel').result(timeout=5) == 'released'
    worker.close()


def test_invalid_submission_releases_queue_capacity(tmp_path):
    worker = JournalWorker(tmp_path / 'held.sqlite3', capacity=1)
    with pytest.raises(Rejected):
        worker.submit('reserve_held', 'example', {})
    assert worker.submit('held_counts').result(timeout=5) == {}
    worker.close()
