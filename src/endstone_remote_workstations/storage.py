"""Transactional offline storage harness with replay and stale-state handling."""
from collections import OrderedDict
from dataclasses import asdict, dataclass
import hashlib
import json
from .model import Rejected, plan_storage


@dataclass(frozen=True)
class Response:
    request_id: int
    ok: bool
    reason: str
    version: int
    resync: bool


class StorageHarness:
    def __init__(self, state, cache_limit=128):
        if not 1 <= cache_limit <= 1024:
            raise ValueError("invalid replay cache bound")
        self.state = state
        self.responses = OrderedDict()
        self.lowest_request_id = 0
        self.cache_limit = cache_limit
        self.committed_drops = []

    def handle(self, request, before_commit=None):
        fingerprint = hashlib.sha256(json.dumps(asdict(request), sort_keys=True).encode()).hexdigest()
        key = request.request_id
        if type(key) is not int or not -(2**31) < key < 0 or key % 2 != 1:
            return Response(key, False, "invalid client request id", self.state.version, True)
        if request.generation != self.state.generation:
            return Response(key, False, "stale generation", self.state.version, True)
        if key in self.responses:
            old_fingerprint, old = self.responses[key]
            if fingerprint == old_fingerprint:
                return old
            return Response(key, False, "replay changed payload", self.state.version, True)
        if key >= self.lowest_request_id:
            return Response(key, False, "stale or replayed request", self.state.version, True)
        # Bound retained history without permitting old evicted IDs to execute.
        self.lowest_request_id = key
        try:
            plan = plan_storage(self.state, request)
            if before_commit:
                before_commit(self)
            if self.state != plan.before:
                raise Rejected("inventory changed during prepare")
            self.state = plan.after
            self.committed_drops.extend(plan.drops)
            response = Response(key, True, "committed in offline model", self.state.version, False)
        except Rejected as error:
            response = Response(key, False, str(error), self.state.version, True)
        self.responses[key] = (fingerprint, response)
        while len(self.responses) > self.cache_limit:
            self.responses.popitem(last=False)
        return response

    def handle_batch(self, requests):
        if not 1 <= len(requests) <= 100:
            raise Rejected("invalid request batch size")
        return tuple(self.handle(request) for request in requests)
