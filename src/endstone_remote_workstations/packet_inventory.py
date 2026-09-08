"""Cursor reservations and compare-before-write for a real inventory binding.

An unfinished cursor gesture changes only the presented snapshot. Closing it
discards the gesture, so there is no second persistent copy of the player's items.
"""
from collections import OrderedDict
from dataclasses import replace

from .model import Rejected, plan_storage


def equivalent(left, right):
    return (left is None and right is None) or (left is not None and right is not None
            and left.count == right.count and left.compatible(right))


def same_inventory(left, right):
    return all(equivalent(a, b) for area in ('player', 'storage')
               for a, b in zip(getattr(left, area), getattr(right, area), strict=True))


class Conflict(Rejected):
    """The real inventory changed outside this UI; close and resynchronize."""


class ReservedInventory:
    def __init__(self, state, read, write):
        self.state = self.baseline = state
        self.read, self.write = read, write
        self.history = OrderedDict()
        self.last_request = 0
        self.closed = False

    def check(self):
        if self.closed or not same_inventory(self.baseline, self.read()):
            self.closed = True
            raise Conflict('The real inventory changed; reopen the interface.')

    def apply(self, request):
        self.check()
        old = self.history.get(request.request_id)
        fingerprint = request.actions
        if old is not None:
            if old[0] != fingerprint:
                raise Rejected('Changed replay payload.')
            return old[1]
        if request.request_id >= self.last_request:
            raise Rejected('Expired or out-of-order request.')
        self.last_request = request.request_id
        # Entity spawning is not atomic with Endstone inventory writes. Keep the
        # release policy explicit until that crash boundary has a durable executor.
        if any(a.kind == 'drop' for a in request.actions):
            raise Rejected('Place the item first, then drop it outside this interface.')
        plan = plan_storage(self.state, request)
        if plan.after.cursor[0] is None:
            self.write(self.baseline, plan.after)
            self.baseline = plan.after
        self.state = plan.after
        self.history[request.request_id] = (fingerprint, plan)
        if len(self.history) > 128:
            self.history.popitem(last=False)
        return plan

    def close(self):
        self.closed = True
        self.state = replace(self.baseline, cursor=(None,))
        self.history.clear()
