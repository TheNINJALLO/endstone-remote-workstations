"""Observe resumed gameplay after a native editor closes without packet 47."""
import math
import struct
import time

from .protocol import CodecError, Reader


def gameplay_input(payload):
    """Read the bounded input prefix, without changing native movement handling."""
    reader = Reader(payload)
    header = struct.unpack('<8f', reader.raw(32))
    if not all(math.isfinite(value) for value in header) or not reader.boolean():
        raise CodecError('Invalid native player input prefix.')
    count = reader.uvar()
    if count > 66:
        raise CodecError('Too many input flags.')
    flags = [reader.svar() for _ in range(count)]
    if len(set(flags)) != count or any(not 0 <= flag <= 65 for flag in flags):
        raise CodecError('Invalid native input flags.')
    reader.uvar()  # Input mode
    reader.uvar()  # Play mode: Windows reports screen for both UI and HUD.
    reader.svar()  # Interaction model
    reader.raw(8)  # Interaction rotation
    tick = reader.uvar(64)
    x, z = header[5:7]
    if abs(x) > 1.001 or abs(z) > 1.001:
        raise CodecError('Invalid movement vector.')
    return tick, abs(x)+abs(z) > 0.001 and bool(set(flags) & {10, 11, 12, 13, 14, 15, 51, 52})


class GameplayCloseObserver:
    def observe_input(self, event, session):
        if (event.packet_id != 144 or not session.screen_open or session.sign_save_at is not None
                or session.closing_at is not None or time.monotonic()-session.opened_at < 0.5):
            return
        try:
            tick, moving = gameplay_input(bytes(event.payload))
        except CodecError:
            return
        if tick <= session.editor_client_tick:
            return
        session.editor_client_tick = tick
        if moving:
            if session.editor_neutral_ticks >= 2:
                session.superseded = True  # Revoke on the next game tick; no packet sends here.
        else:
            session.editor_neutral_ticks = min(2, session.editor_neutral_ticks+1)
