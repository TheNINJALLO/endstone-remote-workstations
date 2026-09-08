"""Server-authorized entity contexts. Never resolve client-supplied actor IDs."""
from .api import BlockContext, EntityContext, UIError
from .linked import validate_context

ENTITY_STATIONS = {
    'chestminecart': ('minecraft:chest', 0, 'minecraft:chest_minecart', 27),
    'hopperminecart': ('minecraft:hopper', 8, 'minecraft:hopper_minecart', 5),
    'chestboat': ('minecraft:chest', 0, 'minecraft:chest_boat', 27),
    'agent': ('minecraft:chest', 0, 'minecraft:agent', 27),
    'horse': (None, 12, 'minecraft:horse', 2),
    'donkey': (None, 12, 'minecraft:donkey', (1, 16)),
    'mule': (None, 12, 'minecraft:mule', (1, 16)),
    'llama': (None, 12, 'minecraft:llama', (1, 4, 7, 10, 13, 16)),
    'traderllama': (None, 12, 'minecraft:trader_llama', (1, 4, 7, 10, 13, 16)),
    'camel': (None, 12, 'minecraft:camel', 1),
    'camelhusk': (None, 12, 'minecraft:camel_husk', 1),
    'zombiehorse': (None, 12, 'minecraft:zombie_horse', 2),
    'nautilus': (None, 12, 'minecraft:nautilus', 2),
    'zombienautilus': (None, 12, 'minecraft:zombie_nautilus', 2),
    'villager': (None, 15, 'minecraft:villager_v2', 3),
    'wanderingtrader': (None, 15, 'minecraft:wandering_trader', 3),
    'commandblockminecart': (None, 16, 'minecraft:command_block_minecart', None),
}

EQUIPMENT_SCREENS = frozenset(key for key, spec in ENTITY_STATIONS.items() if spec[1] == 12)
TRADE_SCREENS = frozenset(key for key, spec in ENTITY_STATIONS.items() if spec[1] == 15)
ACTOR_SCREENS = EQUIPMENT_SCREENS | TRADE_SCREENS | {'commandblockminecart'}


def validate_entity_context(context):
    if not isinstance(context, EntityContext):
        raise UIError('An EntityContext is required.')
    validate_context(BlockContext(context.dimension, (0, 0, 0), context.permission))
    actor = context.actor
    try:
        # Do not query properties of an expired public Actor wrapper.
        if not actor.is_valid:
            raise UIError('The source entity is no longer available.')
        identity = actor.id
        if (type(identity) is not int or not -(2**63) <= identity < 2**63
                or identity in (-1, 0) or actor.dimension.name != context.dimension):
            raise UIError('The source entity identity or dimension changed.')
    except (AttributeError, TypeError, ValueError, RuntimeError) as error:
        raise UIError('Invalid or unavailable source entity.') from error
    return context


def configured_entity(settings, player, kind):
    raw = settings.get('entities', {}).get('sources', {}).get(kind)
    if raw is None:
        return None
    if not isinstance(raw, dict):
        raise UIError('Invalid configured entity source.')
    identity = raw.get('actor_id')
    if type(identity) is not int or identity in (-1, 0) or not -(2**63) <= identity < 2**63:
        raise UIError('A configured entity requires its actual signed ActorUniqueID.')
    if raw.get('dimension') != player.dimension.name:
        raise UIError('The configured entity must be in the player dimension.')
    # One scan only when a configured command opens, never during active ticks.
    actor = next((a for a in player.dimension.actors if a.is_valid and a.id == identity), None)
    if actor is None:
        raise UIError('The configured source entity is not loaded.')
    return validate_entity_context(EntityContext(raw['dimension'], actor, raw.get('permission')))
