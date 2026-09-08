"""Durable held-item reservations and write intents, run on Journal's disk worker.

These methods never read or write BDS. A recorded intent does not authorize a
transaction. Native identity assignment, movement exclusion, item conservation
and BDS save reconciliation remain mandatory before enabling a mutating UI.
"""
import json
import sqlite3
import time
import uuid

from .held_items import HeldSelection
from .model import Rejected

SCHEMA = """
CREATE TABLE IF NOT EXISTS held_leases (
 id TEXT PRIMARY KEY, item_id TEXT NOT NULL, owner TEXT NOT NULL,
 plugin TEXT NOT NULL, source TEXT NOT NULL, revision INTEGER NOT NULL,
 phase TEXT NOT NULL, note TEXT NOT NULL DEFAULT '', created REAL NOT NULL);
CREATE UNIQUE INDEX IF NOT EXISTS held_item_exclusive ON held_leases(item_id)
 WHERE phase != 'released';
CREATE UNIQUE INDEX IF NOT EXISTS held_player_exclusive ON held_leases(owner)
 WHERE phase != 'released';
CREATE TABLE IF NOT EXISTS held_writes (
 id TEXT PRIMARY KEY, lease_id TEXT NOT NULL, revision INTEGER NOT NULL,
 before_state TEXT NOT NULL, after_state TEXT NOT NULL, phase TEXT NOT NULL,
 created REAL NOT NULL, UNIQUE(lease_id, revision));
"""


def _source(value):
    source = value if isinstance(value, HeldSelection) else HeldSelection.from_record(value)
    if source.stack.kind != 'shulker' or source.stack.durable_id is None:
        raise Rejected('A single identified actual shulker is required for a reservation.')
    return source, json.dumps(source.record(), sort_keys=True, separators=(',', ':'))


def detach_arguments(method, args):
    """Freeze caller-owned records before queuing them on the disk worker."""
    positions = {'reserve_held': (1,), 'prepare_held': (2, 3)}.get(method, ())
    if not positions:
        return args
    detached = list(args)
    for index in positions:
        if index >= len(detached):
            raise Rejected('Missing held-item journal arguments.')
        detached[index] = _source(detached[index])[0]
    return tuple(detached)


class HeldJournalMixin:
    def reserve_held(self, plugin, source_record):
        self.check_thread()
        if not isinstance(plugin, str) or not 1 <= len(plugin) <= 64 or not all(
                c in 'abcdefghijklmnopqrstuvwxyz0123456789_.-' for c in plugin):
            raise Rejected('Invalid held-item lease plugin.')
        source, raw = _source(source_record)
        lease = str(uuid.uuid4())
        try:
            with self.db:
                self.db.execute('INSERT INTO held_leases VALUES (?,?,?,?,?,0,\'reserved\',\'\',?)',
                    (lease, source.stack.durable_id, source.owner, plugin, raw, time.time()))
        except sqlite3.IntegrityError as error:
            raise Rejected('The item or player already has a reservation or quarantined write.') from error
        return lease

    def prepare_held(self, lease, revision, before_record, after_record):
        self.check_thread()
        before, old = _source(before_record)
        after, new = _source(after_record)
        if (type(revision) is not int or revision < 0 or before == after
                or (before.owner, before.slot, before.stack.type_id, before.stack.amount,
                    before.stack.data, before.stack.durable_id) !=
                   (after.owner, after.slot, after.stack.type_id, after.stack.amount,
                    after.stack.data, after.stack.durable_id)):
            raise Rejected('A held write cannot change backing identity, owner, slot or item type.')
        intent = str(uuid.uuid4())
        with self.db:
            changed = self.db.execute("UPDATE held_leases SET phase='prepared' WHERE id=? "
                                     "AND phase='reserved' AND revision=? AND source=?", (lease, revision, old))
            if changed.rowcount != 1:
                raise Rejected('Stale, busy or quarantined held-item reservation.')
            self.db.execute("INSERT INTO held_writes VALUES (?,?,?,?,?,'prepared',?)",
                            (intent, lease, revision, old, new, time.time()))
        return intent

    def begin_held_apply(self, intent):
        self.check_thread()
        with self.db:
            changed = self.db.execute("UPDATE held_writes SET phase='applying' WHERE id=? AND phase='prepared' "
                "AND EXISTS (SELECT 1 FROM held_leases WHERE id=held_writes.lease_id AND phase='prepared' "
                "AND revision=held_writes.revision)", (intent,))
            if changed.rowcount != 1:
                raise Rejected('Held write is not prepared or has already been dispatched.')
            self.db.execute("UPDATE held_leases SET phase='applying' WHERE id=(SELECT lease_id FROM held_writes WHERE id=?)", (intent,))

    def mark_held_applied(self, intent):
        """Record in-memory verification, never a claim of BDS save durability."""
        self.check_thread()
        with self.db:
            row = self.db.execute("SELECT lease_id,revision,after_state FROM held_writes WHERE id=? AND phase='applying'", (intent,)).fetchone()
            if row is None:
                raise Rejected('Held write is not awaiting in-memory verification.')
            lease, revision, after = row
            changed = self.db.execute("UPDATE held_leases SET phase='reserved',source=?,revision=revision+1 "
                "WHERE id=? AND phase='applying' AND revision=?", (after, lease, revision))
            if changed.rowcount != 1:
                raise Rejected('The held reservation was invalidated during apply.')
            self.db.execute("UPDATE held_writes SET phase='applied-memory' WHERE id=?", (intent,))

    def close_held(self, lease, reason):
        """Only a reservation that never prepared a write can be released cleanly."""
        self.check_thread()
        if not isinstance(reason, str):
            raise Rejected('A cleanup reason is required.')
        with self.db:
            row = self.db.execute('SELECT phase FROM held_leases WHERE id=?', (lease,)).fetchone()
            if row is None:
                raise Rejected('Unknown held-item reservation.')
            if row[0] in ('released', 'quarantined'):
                return row[0]
            dirty = self.db.execute('SELECT 1 FROM held_writes WHERE lease_id=? LIMIT 1', (lease,)).fetchone()
            phase = 'quarantined' if dirty else 'released'
            self.db.execute('UPDATE held_leases SET phase=?,note=? WHERE id=?', (phase, reason[:512], lease))
            if dirty:
                self.db.execute("UPDATE held_writes SET phase='quarantined' WHERE lease_id=?", (lease,))
        return phase

    def recover_held(self):
        self.check_thread()
        with self.db:
            self.db.execute("UPDATE held_leases SET phase='quarantined',note='Native ownership and BDS durability unknown after restart' WHERE phase NOT IN ('released','quarantined')")
            self.db.execute("UPDATE held_writes SET phase='quarantined' WHERE phase!='quarantined'")
        return self.db.execute("SELECT id,owner,phase,note FROM held_leases WHERE phase='quarantined'").fetchall()

    def held_counts(self):
        self.check_thread()
        return dict(self.db.execute('SELECT phase,COUNT(*) FROM held_leases GROUP BY phase').fetchall())
