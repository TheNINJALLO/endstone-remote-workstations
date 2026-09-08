from importlib.resources import files
import json
from pathlib import Path
import re
import tomllib


def capabilities():
    return json.loads(files(__package__).joinpath("capabilities.json").read_text(encoding="utf-8"))


def configuration(path=None):
    raw = files(__package__).joinpath("config.toml").read_text(encoding="utf-8") if path is None else Path(path).read_text(encoding="utf-8")
    value = tomllib.loads(raw)
    if not isinstance(value.get("aliases", {}), dict):
        raise ValueError("aliases must be a table")
    return value


def command_metadata(config):
    """Endstone's registry checks names and aliases against vanilla AND plugins.

    Configuration is consumed before PythonPluginLoader builds PluginDescription.
    Conflicting names are declined by EndstoneCommandMap, never overwritten.
    """
    commands = {"workstations": {"description": "RemoteWorkstations catalog, open requests and diagnostics",
                 "usages": ["/workstations [action: string] [type: string]"],
                 "permissions": ["remoteworkstations.use"]}}
    mappings = {}
    aliases = config.get("aliases", {})
    if aliases.get("enabled", True) is not True:
        return commands, mappings
    reserved = {"enchant", "workstations"}
    for row in capabilities():
        requested = aliases.get(row["id"], row["aliases"])
        if not isinstance(requested, list) or len(requested) > 8:
            raise ValueError("alias entries must be lists with at most eight names")
        for name in requested:
            if not isinstance(name, str) or not re.fullmatch(r"[a-z][a-z0-9_]{0,31}", name) or name in reserved:
                raise ValueError(f"invalid or reserved alias: {name!r}")
            if name in mappings:
                raise ValueError(f"duplicate configured alias: {name}")
            mappings[name] = row["id"]
            commands[name] = {"description": f"Request {row['id']} (availability checked)",
                              "permissions": [row["permission"]], "usages": [f"/{name}"]}
    return commands, mappings


def permission_metadata():
    return {"remoteworkstations.use": {"description": "Browse workstation capabilities", "default": True},
            "remoteworkstations.admin": {"description": "Use privileged native editors in Creative as an operator", "default": "op"},
            "remoteworkstations.status": {"description": "Inspect runtime and journal status", "default": "op"},
            "remoteworkstations.diagnostics": {"description": "Control sanitized packet diagnostics", "default": "op"},
            "remoteworkstations.contexts": {"description": "Access configured linked blocks without a separate source permission", "default": "op"},
            **{row["permission"]: {"description": f"Request {row['id']}",
                                   "default": "op" if row["administrator_only"] else True}
               for row in capabilities()}}


def unavailable(key):
    for row in capabilities():
        if key == row["id"] or key in row["aliases"]:
            return row
    return None
