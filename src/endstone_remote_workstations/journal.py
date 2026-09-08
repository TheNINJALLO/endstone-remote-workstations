"""Durable escrow journal. SQLite does NOT transact with BDS saves.

All unfinished external-store records are quarantined after restart, even when
an in-memory BDS apply was reported successful. No automatic item issuance.
"""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import sqlite3
import threading
import time
from .model import Snapshot, Rejected


class Journal:
    def __init__(self, path):
        self.path = str(path)
        self.owner = threading.get_ident()
        self.db = sqlite3.connect(self.path)
        self.db.execute("PRAGMA journal_mode=WAL")
        self.db.execute("PRAGMA synchronous=FULL")
        self.db.executescript("""
            CREATE TABLE IF NOT EXISTS transfers (
              id TEXT PRIMARY KEY, owner TEXT NOT NULL, generation TEXT NOT NULL,
              request_id INTEGER NOT NULL, before_state TEXT NOT NULL,
              after_state TEXT NOT NULL, effects TEXT NOT NULL, phase TEXT NOT NULL,
              note TEXT NOT NULL DEFAULT '', created REAL NOT NULL,
              UNIQUE(owner, generation, request_id));
            CREATE TABLE IF NOT EXISTS model_storage (
              owner TEXT PRIMARY KEY, state TEXT NOT NULL);
            CREATE TABLE IF NOT EXISTS recovery (
              id TEXT PRIMARY KEY, owner TEXT NOT NULL, items TEXT NOT NULL,
              state TEXT NOT NULL, reason TEXT NOT NULL);
        """)

    def check_thread(self):
        if threading.get_ident() != self.owner:
            raise RuntimeError("journal accessed outside its disk worker")

    def prepare(self, owner, request_id, plan):
        self.check_thread()
        identity = hashlib.sha256(f"{owner}\0{plan.before.generation}\0{request_id}".encode()).hexdigest()
        values = (identity, owner, plan.before.generation, request_id,
                  plan.before.serialize(), plan.after.serialize(),
                  json.dumps([i.record() for i in plan.drops], sort_keys=True))
        existing = self.db.execute("SELECT id,owner,generation,request_id,before_state,after_state,effects FROM transfers WHERE id=?",
                                   (identity,)).fetchone()
        if existing:
            if existing != values:
                raise Rejected("request id reused with different transaction")
            raise Rejected("request already journaled; reconcile instead of replaying")
        with self.db:
            self.db.execute("INSERT INTO transfers VALUES (?,?,?,?,?,?,?,'prepared','',?)", (*values, time.time()))
        return identity

    def mark_applied(self, identity):
        self.check_thread()
        with self.db:
            result = self.db.execute("UPDATE transfers SET phase='applied-memory' WHERE id=? AND phase='prepared'", (identity,))
            if result.rowcount != 1:
                raise Rejected("invalid journal transition")

    def quarantine(self, identity, note):
        self.check_thread()
        with self.db:
            self.db.execute("UPDATE transfers SET phase='quarantined',note=? WHERE id=? AND phase!='reconciled'",
                            (note[:512], identity))

    def recover(self):
        self.check_thread()
        with self.db:
            self.db.execute("UPDATE transfers SET phase='quarantined',note='BDS durability unknown after restart' WHERE phase IN ('prepared','applied-memory')")
        return self.db.execute("SELECT id,owner,phase,note FROM transfers WHERE phase='quarantined'").fetchall()

    def save_model(self, owner, state, recovery_id=None, overflow=()):
        """Atomic only for this offline model; NEVER writes a real BDS inventory."""
        self.check_thread()
        with self.db:
            if overflow:
                if not recovery_id:
                    raise Rejected("overflow requires idempotency identity")
                payload = json.dumps([i.record() for i in overflow], sort_keys=True)
                existing = self.db.execute("SELECT owner,items FROM recovery WHERE id=?", (recovery_id,)).fetchone()
                if existing and existing != (owner, payload):
                    raise Rejected("recovery identity conflict")
                self.db.execute("INSERT OR IGNORE INTO recovery VALUES (?,?,?,'held','model cursor overflow')",
                                (recovery_id, owner, payload))
            self.db.execute("INSERT INTO model_storage VALUES (?,?) ON CONFLICT(owner) DO UPDATE SET state=excluded.state",
                            (owner, state.serialize()))

    def load_model(self, owner):
        self.check_thread()
        row = self.db.execute("SELECT state FROM model_storage WHERE owner=?", (owner,)).fetchone()
        return Snapshot.deserialize(row[0]) if row else None

    def counts(self):
        self.check_thread()
        return dict(self.db.execute("SELECT phase,COUNT(*) FROM transfers GROUP BY phase").fetchall())

    def close(self):
        self.check_thread()
        self.db.close()


class JournalWorker:
    """Bounded disk queue. Futures must be polled on ticks, never awaited on them."""
    def __init__(self, path, capacity=128):
        self.pool = ThreadPoolExecutor(max_workers=1, thread_name_prefix="rw-journal")
        self.slots = threading.BoundedSemaphore(capacity)
        self.closed = False
        self.ready = self.pool.submit(Journal, Path(path))

    def submit(self, method, *args):
        if self.closed or not self.slots.acquire(blocking=False):
            raise Rejected("journal unavailable or queue full")
        try:
            future = self.pool.submit(lambda: getattr(self.ready.result(), method)(*args))
        except BaseException:
            self.slots.release()
            raise
        future.add_done_callback(lambda _: self.slots.release())
        return future

    def close(self):
        if self.closed:
            return
        self.closed = True
        # Shutdown is the sole blocking boundary; no BDS objects enter the worker.
        try:
            if self.ready.exception() is None:
                self.pool.submit(lambda: self.ready.result().close()).result()
        finally:
            self.pool.shutdown(wait=True)
