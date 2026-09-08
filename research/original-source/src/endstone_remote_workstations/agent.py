"""Authorization at each native Agent request; BDS owns every item operation."""
from .chemistry import cleanup_request


def validate_inventory(backend, event, session):
    try:
        state = backend.bridge.state(event.player)
        if (state['ready'] or session.window is None or state.get('window') != session.window
                or event.player.dimension.name != session.dimension):
            raise RuntimeError('The owned Agent inventory manager changed.')
        try:
            actor = session.context.actor
            if (session.closing_at or session.close_pending or not actor.is_valid
                    or actor.id != session.entity_id
                    or actor.runtime_id != session.entity_runtime_id
                    or backend.reason(event.player, 'agent', session.context)):
                raise RuntimeError('Agent source authority was revoked.')
            backend._check_guards(event.player, 'agent')
            return  # Original ItemStackRequest goes to the native validator.
        except Exception:
            session.close_pending = True
            # The cursor already belongs to an accepted native transaction.
            # Permit its return/drop even if source ownership was revoked.
            # No Agent storage reads, deposits, crafting or preview operations.
            cleanup_request(bytes(event.payload), 'agent')
    except Exception:
        event.is_cancelled = True
        session.close_pending = True
