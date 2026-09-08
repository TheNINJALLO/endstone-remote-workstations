"""Distribution boundaries: source and documentation, without local test data."""
from pathlib import PurePosixPath
import tarfile


def inspect_source(path):
    with tarfile.open(path) as archive:
        roots = set()
        for member in archive.getmembers():
            name = PurePosixPath(member.name)
            if name.is_absolute() or '..' in name.parts or member.issym() or member.islnk():
                raise RuntimeError('Source archive contains a path outside its distribution root')
            roots.add(name.parts[0])
            if (any(p in ('.runtime', '.research', '.venv', 'research', '__pycache__') for p in name.parts)
                    or name.suffix.lower() in ('.exe', '.dll', '.so', '.pyd', '.log', '.hex', '.mcstructure')):
                raise RuntimeError('Source archive contains local runtime or raw test material: '+member.name)
        if len(roots) != 1:
            raise RuntimeError('Source archive must have exactly one distribution root')
