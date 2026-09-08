"""Optional Chest-UI form encoding, never a transactional inventory.

Contract derived from Chest-UI commit 115d95c8239a0ee2f578a9a7710699a8f6627f26,
CC BY 4.0, Herobrine64 / LeGend077 / Aex66. This port uses texture paths only.
Client rendering and unrelated form coexistence still require client testing.
"""
SIZES = (1, 5, 9, 18, 27, 36, 45, 54)


def title(text, size=27, *, furnace=False, lit=False):
    if "§" in text:
        raise ValueError("presentation title may not contain formatting markers")
    if furnace:
        prefix = "§f§u§r§n§a§c§e" + ("§l§i§t" if lit else "") + "§r"
    else:
        if size not in SIZES:
            raise ValueError("unsupported Chest-UI size")
        digits = f"{size:02}"
        prefix = "§c§h§e§s§t§" + digits[0] + "§" + digits[1] + "§r"
    return prefix + text


def button(name, texture=None, *, lore=(), count=1, durability=0):
    if texture is not None and (not texture.startswith("textures/") or ".." in texture):
        raise ValueError("use a verified resource-pack texture path, not a numeric item ID")
    if type(count) is not int or type(durability) is not int or not 1 <= count <= 99 or not 0 <= durability <= 99:
        raise ValueError("invalid form decoration")
    text = f"stack#{count:02}dur#{durability:02}§r{name or '§r'}"
    if name:
        text += "§r"
    text += "".join("\n"+line for line in lore)
    return text, texture

