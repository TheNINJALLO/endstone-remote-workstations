"""Server-authorized source contexts for genuine native block containers."""
import re
from .api import BlockContext, SignContext, UIError
from .protocol import CodecError, Reader, svar

# Display block, container type, allowed live block types. Burning variants keep
# their actual native block actor and processing state.
LINKED_STATIONS = {
    'chest': ('minecraft:chest', 0, ('minecraft:chest',)),
    'doublechest': ('minecraft:chest', 0, ('minecraft:chest',)),
    'trappedchest': ('minecraft:trapped_chest', 0, ('minecraft:trapped_chest',)),
    'barrel': ('minecraft:barrel', 0, ('minecraft:barrel',)),
    'dispenser': ('minecraft:dispenser', 6, ('minecraft:dispenser',)),
    'dropper': ('minecraft:dropper', 7, ('minecraft:dropper',)),
    'hopper': ('minecraft:hopper', 8, ('minecraft:hopper',)),
    'furnace': ('minecraft:furnace', 2, ('minecraft:furnace', 'minecraft:lit_furnace')),
    'blastfurnace': ('minecraft:blast_furnace', 27, ('minecraft:blast_furnace', 'minecraft:lit_blast_furnace')),
    'smoker': ('minecraft:smoker', 28, ('minecraft:smoker', 'minecraft:lit_smoker')),
    'brewing': ('minecraft:brewing_stand', 4, ('minecraft:brewing_stand',)),
    'enchanting': ('minecraft:enchanting_table', 3, ('minecraft:enchanting_table',)),
    'beacon': ('minecraft:beacon', 13, ('minecraft:beacon',)),
    'crafter': ('minecraft:crafter', 36, ('minecraft:crafter',)),
    'lectern': ('minecraft:lectern', 25, ('minecraft:lectern',)),
    'compoundcreator': ('minecraft:compound_creator', 20, ('minecraft:compound_creator',)),
    'elementconstructor': ('minecraft:element_constructor', 21, ('minecraft:element_constructor',)),
    'materialreducer': ('minecraft:material_reducer', 22, ('minecraft:material_reducer',)),
    'labtable': ('minecraft:lab_table', 23, ('minecraft:lab_table',)),
    'jigsaw': ('minecraft:jigsaw', 32, ('minecraft:jigsaw',)),
    'structure': ('minecraft:structure_block', 14, ('minecraft:structure_block',)),
    'commandblock': ('minecraft:command_block', 16, ('minecraft:command_block', 'minecraft:repeating_command_block', 'minecraft:chain_command_block')),
    'sign': ('minecraft:standing_sign', None, tuple('minecraft:'+wood+style for wood in
        ('', 'spruce_', 'birch_', 'jungle_', 'acacia_', 'darkoak_', 'crimson_', 'warped_',
         'mangrove_', 'bamboo_', 'cherry_', 'pale_oak_') for style in ('standing_sign', 'wall_sign'))),
}

ACTOR_DISPLAYS = frozenset(('beacon', 'crafter', 'lectern', 'sign', 'jigsaw', 'commandblock', 'structure'))
PASSIVE_SCREENS = frozenset(('lectern', 'sign', 'jigsaw', 'commandblock', 'commandblockminecart', 'structure'))
CHEMISTRY_SCREENS = frozenset(('compoundcreator', 'elementconstructor', 'materialreducer', 'labtable'))


def lectern_page_request(payload):
    """r26_u4: byte spread index, byte page count, signed BlockPos."""
    reader = Reader(payload)
    page, total = reader.byte(), reader.byte()
    position = tuple(reader.svar() for _ in range(3))
    reader.end()
    if not total or page >= total:
        raise CodecError('Invalid lectern page.')
    return page, total, position


def validate_context(context):
    if not isinstance(context, BlockContext):
        raise UIError('A BlockContext is required.')
    if isinstance(context, SignContext) and type(context.front) is not bool:
        raise UIError('The sign side must be boolean.')
    if not isinstance(context.dimension, str) or not 1 <= len(context.dimension) <= 64 or '\x00' in context.dimension:
        raise UIError('Invalid source dimension.')
    if (not isinstance(context.position, tuple) or len(context.position) != 3
            or any(type(v) is not int for v in context.position)):
        raise UIError('A source position requires three integer coordinates.')
    x, y, z = context.position
    if not (-30000000 <= x <= 30000000 and -64 <= y <= 319 and -30000000 <= z <= 30000000):
        raise UIError('Source position is outside the admitted world bounds.')
    if context.permission is not None and (not isinstance(context.permission, str)
            or not re.fullmatch(r'[a-zA-Z0-9_][a-zA-Z0-9_.-]{0,127}', context.permission)):
        raise UIError('Invalid source access permission.')
    return context


def configured_context(settings, kind):
    raw = settings.get('linked', {}).get('sources', {}).get(kind)
    if raw is None:
        return None
    if not isinstance(raw, dict) or not isinstance(raw.get('position'), (tuple, list)):
        raise UIError('Invalid configured linked source.')
    return validate_context(BlockContext(raw.get('dimension'), tuple(raw['position']), raw.get('permission')))


def project_actor_data(payload, source, display):
    """Copy authoritative network NBT, changing only existing coordinates."""
    from bstream import BinaryStream, ReadOnlyBinaryStream
    from rapidnbt import CompoundTag
    reader = Reader(payload)
    if tuple(reader.svar() for _ in range(3)) != source:
        raise CodecError('Block actor source does not match the owned context.')
    tag = CompoundTag()
    tag.deserialize(ReadOnlyBinaryStream(payload[reader.offset:]))
    for axis, coordinate in zip(('x', 'y', 'z'), display):
        if axis in tag:
            tag.set(axis, coordinate)
    stream = BinaryStream()
    tag.serialize(stream)
    return b''.join(svar(value) for value in display)+stream.copy_buffer()
