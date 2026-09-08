# Troubleshooting

## Native companion is absent or unavailable

Check the installed wheel. The portable `py3-none-any` wheel has no native bridge.
Windows requires the `cp311-cp311-win_amd64` wheel, CPython 3.11, the exact
Endstone/BDS pair and matching module hashes. Keep one provider wheel in `plugins/`.
Other builds are refused intentionally; renaming a wheel does not change its ABI.

## The plugin loaded, but a UI is disabled

Check `/workstations status` and `ui.capabilities(player)`. Enable the relevant
backend in the existing configuration and restart. Linked blocks/entities,
editors, Agent, NPC and chemistry have separate switches and real source/world
requirements. The 16 unavailable catalog entries are not enabled by a switch.

## Permission or source access was denied

Grant the base UI permission, the specific `remoteworkstations.open.<type>`
permission and the source permission. A context without an explicit permission
requires `remoteworkstations.contexts`. Protection guards, owner checks, source
type, loaded state and dimension admission still apply. Resolve sources on the
server; do not accept unchecked coordinates or entity IDs from players.

## Another inventory is already open

Close it normally before opening a remote screen. API calls are queued to respect
native lifecycle and packet ordering. Player-inventory cancellation relinquishes
tracking, but that screen remains client-owned until the player closes it.

## A menu button doesn't give the displayed item

Inventory-menu items are protected icons. Clicking selects the registered action;
the icon itself cannot be taken. Implement and validate rewards or recipe
execution in your own callback/service.

## A dependency call never returns

Do not block the server thread on an unfinished Future. Attach a short completion
callback or inspect `done()` on a later tick. Keep the consuming plugin enabled
and call `dispose()` when it is disabled. Failed tickets include `info.reason`.

## Missing bstream or rapidnbt

Use the server's Python to install the pinned dependencies into
`plugins/.local`. With `--prefix`, include `--ignore-installed` so packages in a
different environment do not cause the target installation to be skipped.
[Installation](https://github.com/TheNINJALLO/endstone-remote-workstations/wiki/Installation) contains the command.

## A crash or unexpected item state

Preserve the world, plugin data and sanitized logs before trying recovery.
There is no exactly-once cross-store recovery guarantee. Do not edit journal
rows or issue replacement items on the assumption that a journal state proves
whether BDS saved the item change. See [recovery](https://github.com/TheNINJALLO/endstone-remote-workstations/blob/main/docs/RECOVERY.md).
