"""Read-only, allowlisted packet diagnostics. No authentication or item NBT bytes.

Only ContainerOpen/Close get raw bytes, after exact shape validation. All other
allowed inventory packets record size/timing only. Books, signs, NPC, command
and structure editors are excluded entirely. No addresses, names, UUIDs or JWTs.
"""
import hashlib
import hmac
import json
import os
from pathlib import Path
import queue
import threading
import time
from .protocol import Open, Reader, CodecError

ALLOWED = frozenset((21, 46, 47, 49, 50, 51, 52, 56, 146, 147, 148, 162, 306, 307, 317))


def sanitized(packet_id, payload):
    if packet_id not in ALLOWED or not isinstance(payload, bytes) or len(payload) > 1024 * 1024:
        return None
    record = {"packet_id": packet_id, "size": len(payload)}
    try:
        if packet_id == 46:
            opened = Open.decode(payload)
            if opened.container_type not in (*range(37), 247, 255):
                raise CodecError("unknown container type")
            record["payload_hex"] = payload.hex()
        elif packet_id == 47:
            reader = Reader(payload)
            reader.byte()
            reader.byte()
            reader.boolean()
            reader.end()
            record["payload_hex"] = payload.hex()
    except CodecError:
        record["shape"] = "invalid"
    return record


class Capture:
    def __init__(self, folder, capacity=512, maximum_file_bytes=2097152, file_count=3):
        if not 1 <= capacity <= 8192 or not 4096 <= maximum_file_bytes <= 16777216 or not 1 <= file_count <= 10:
            raise ValueError("diagnostic bounds invalid")
        self.folder = Path(folder)
        self.queue = queue.Queue(capacity)
        self.limit, self.file_count = maximum_file_bytes, file_count
        self.salt = os.urandom(32)
        self.dropped = 0
        self.closed = False
        self.error = None
        self.thread = threading.Thread(target=self._run, name="rw-capture", daemon=True)
        self.thread.start()

    def record(self, direction, player_id, packet_id, payload, on_owner_thread):
        if self.closed or direction not in ("send", "receive"):
            return
        record = sanitized(packet_id, payload)
        if record is None:
            return
        record.update(direction=direction, player=hmac.new(self.salt, player_id.encode(), hashlib.sha256).hexdigest()[:16],
                      monotonic_ns=time.monotonic_ns(), owner_thread=bool(on_owner_thread))
        try:
            self.queue.put_nowait(record)
        except queue.Full:
            self.dropped += 1

    def _run(self):
        try:
            self.folder.mkdir(parents=True, exist_ok=True)
            index, size = 0, 0
            stream = (self.folder / "packets-0.jsonl").open("w", encoding="utf-8")
            try:
                while True:
                    record = self.queue.get()
                    if record is None:
                        break
                    line = json.dumps(record, separators=(",", ":"))+"\n"
                    if size+len(line.encode()) > self.limit:
                        stream.close()
                        index = (index+1) % self.file_count
                        stream = (self.folder / f"packets-{index}.jsonl").open("w", encoding="utf-8")
                        size = 0
                    stream.write(line)
                    stream.flush()
                    size += len(line.encode())
            finally:
                stream.close()
        except Exception as error:
            self.error = type(error).__name__

    def close(self, wait=True):
        if self.closed:
            if wait:
                self.thread.join(timeout=5)
            return
        self.closed = True
        # Don't deadlock if the disk thread failed or the queue is full.
        while self.thread.is_alive():
            try:
                self.queue.put_nowait(None)
                break
            except queue.Full:
                # Diagnostic samples are lossy; never block the server tick on a
                # full queue or a slow/failing filesystem just to enqueue stop.
                try:
                    self.queue.get_nowait()
                    self.dropped += 1
                except queue.Empty:
                    pass
        if wait:
            self.thread.join(timeout=5)
