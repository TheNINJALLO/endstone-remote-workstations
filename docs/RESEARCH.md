# Source and API audit

This records the original public API and protocol audit. The later implemented Windows
native backend and live captures are documented in [NATIVE_WINDOWS.md](NATIVE_WINDOWS.md)
and [TEST_REPORT.md](TEST_REPORT.md); original capture gaps below are historical.

Reference snapshots were downloaded from the requested public repositories into `.research/`.
`research/references.lock.json` records both resolved commits and downloaded archive SHA-256 values.
No moving branch is used as a runtime compatibility gate.

| Reference | Exact commit consulted |
|---|---|
| [Chest-UI](https://github.com/Herobrine643928/Chest-UI/tree/115d95c8239a0ee2f578a9a7710699a8f6627f26) | `115d95c8239a0ee2f578a9a7710699a8f6627f26` |
| [endstone-inventoryui](https://github.com/Shock95/endstone-inventoryui/tree/a463cf110d2f799b4382ff5fbca829d1f912d2a7) | `a463cf110d2f799b4382ff5fbca829d1f912d2a7` |
| [Endstone v0.11.0](https://github.com/EndstoneMC/endstone/tree/f534d435043029e6010b435e4480bfdb76fe39ae) | `f534d435043029e6010b435e4480bfdb76fe39ae` |
| [Endstone v0.11.10](https://github.com/EndstoneMC/endstone/tree/8f84d6f5b556916597ed5b6b71329b2ed3ca8fc8) | `8f84d6f5b556916597ed5b6b71329b2ed3ca8fc8` |
| [Protocol r26_u4](https://github.com/EndstoneMC/protocol-docs/tree/e125b570c1cec03199083eaa845a23fe91b2a9c5) | `e125b570c1cec03199083eaa845a23fe91b2a9c5` |
| [Mojang protocol docs](https://github.com/Mojang/bedrock-protocol-docs/tree/990a3b62f030dbd934a031ad07b2aa7d017f389d) | `990a3b62f030dbd934a031ad07b2aa7d017f389d` |

The Mojang snapshot's `json/ContainerOpenPacketPayload.json` identifies **1.26.60-beta.21 / protocol 2207**.
It is a cross-check, not the target schema. The Endstone r26_u4 README explicitly identifies **1.26.45.1 / 2169**.

## Public API capability matrix (0.11.0)

Headers under `include/endstone/`, Python stubs under `endstone/`, Python bindings under
`src/endstone/python/`, and corresponding core/runtime implementations were consulted.

| Capability | Evidence in pinned 0.11.0 | Consequence |
|---|---|---|
| Plugin metadata and commands | `endstone/plugin/__init__.py`, `plugin_loader.py`, `include/endstone/plugin/plugin_description.h` | Entry point `remote_workstations` matches distribution `endstone-remote-workstations`. Python loader supports the string `0.11`. |
| Conflict-free command registration | `src/endstone/core/command/command_map.cpp`, `registerCommand` | Existing vanilla or plugin command names are declined; occupied aliases are omitted. Config is read before metadata construction; restart required. |
| Forms | `endstone/form/__init__.pyi`, `include/endstone/form/action_form.h`, `Player.send_form` | Real action forms are available. They do not implement workstation transactions. |
| Player inventory | `inventory/__init__.pyi`, `inventory/inventory.h`, `player.h` | Get/set/add/clear and returned overflow exist. No public multi-inventory compare-and-swap or BDS save transaction. |
| Actual Ender Chest | `Player.ender_chest`, `Player::getEnderChest`, `src/endstone/core/player.cpp` | Returns the real BDS EnderChestContainer wrapper. Remote open and coordination remain separate unsolved requirements. |
| Item metadata | `ItemStack.nbt`, `item_meta`, `max_stack_size`, `ItemType`, NBT stubs/headers | Typed NBT and metadata are available. A public item object is not automatically a complete wire descriptor or a persistent unique backing identity. |
| Blocks | `Block.data`, `BlockData.block_states`, `runtime_id`, `Server.create_block_data` | Authoritative primary block state/runtime IDs available. Complete secondary layers plus arbitrary block-actor serialization/restoration are not established by these methods alone. |
| Packet observation/cancellation | `PacketReceiveEvent`, `PacketSendEvent`; payload excludes header | Can observe/cancel raw traffic. This grants no container ownership, item creation authority or native executor. |
| Scheduling/threading | `Scheduler.run_task`; C++ `Server::isPrimaryThread`; plugin manager `callEvent` | Synchronous packet event dispatch is guarded by the server thread check. Python plugin additionally checks its enable thread; diagnostics never dereference players on an unexpected thread. Live callback sequencing still needs client tests. |
| Runtime/client versions | `Server.version`, `minecraft_version`, `protocol_version`, `Player.game_version` | Readable. Successful admission is not a raw-packet certification. |
| Recipe execution and workstation opening | No `open_anvil`, `open_workbench`, public container factory, recipe executor or complete recipe registry found | Craft/anvil and other machinery cannot be implemented by inventing Python or public C++ calls. |
| Native service bridge | `plugin/service.h`, `ServiceManager`, Python `Service` binding | Base Service has no custom callable methods. Loading a named C++ service does not automatically expose its derived methods to Python. |

`tools/audit_api.py` records a narrow mechanical check of 27 used/examined baseline symbols.
It is not a full ABI checker and does not replace runtime tests.

## Runtime and client paths

Endstone v0.11.10 `CHANGELOG.md` explicitly adds BDS 1.26.45 and admits clients 1.26.40–1.26.44.
`src/bedrock/shared_constants.h` sets protocol 2169. `runtime/bedrock_hooks/packet.cpp` upgrades
2168 RequestNetworkSettings; `batched_network_peer.cpp` upgrades 2168 login bytes and applies
selected version-specific outgoing shims. Its scoreboard workaround even distinguishes 1.26.44
from clients sharing that protocol. These are concrete reasons to test each game-version path separately.

Observed Windows startup: exact supplied BDS reports 1.26.45.1, build 49559486, branch r/26_u4,
commit `0dc2e0d8f6dbcf54498113d1d7f8a8a2accb448c`; Endstone reports 0.11.10 and protocol 2169.
No admitted client was connected in this session. Linux runtime execution is untested; WSL is absent.

## InventoryUI findings (source review, not demonstrated exploits)

At commit `a463cf1...`, `pyproject.toml` identifies version 2.0.5 and pins
`bedrock-protocol-packets-ng==0.0.11`. `MenuType` offers chest, double chest, hopper and dispenser.

| Concern | Observed code | Treatment in this project |
|---|---|---|
| Multi-action early return | `listener.py` returns on `len(actions)>1`, before final responses; count `>=100` also returns | Whole requests validate on shadow state; offline batch harness produces one response per request, including rejections. |
| Drop action/type mismatch | Drop branch passes `action.type` to `_apply_menu_action`, while transaction construction expects an action object with `.type` | Typed action records; no mixed enum/object call path. |
| Precommit side effects | `container_manager.handle_drop` calls `dimension.drop_item` before staged containers commit | Planner only returns drop intents; no world drop API is called. Live durable side-effect integration remains disabled. |
| Stack identities | `ItemStackTracker.seed_from_request` accepts previously unknown positive client IDs | Model compares against server-owned pre-request identities; it never seeds authority from requests. BDS network identity acquisition remains unresolved. |
| Generic mapping | `LEVEL_ENTITY`, inventory/hotbar/combined, cursor mapped; station roles absent | Adapter deliberately limited to four storage types; catalog records distinct station role IDs. |
| Slot/batch validation | Generic adapters index directly; no complete recipe/XP/preview validation | Bounds, counts, metadata equality, stack maxima, filters and version checks; creative/create/craft actions rejected for storage. |
| Cursor overflow | `Session.close` ignores `Inventory.add_item` leftovers then clears cursor | Offline cleanup retains overflow and writes it to durable model recovery atomically. No claim of real BDS return safety. |
| Restoration | `BlockGraphic.remove` reads `player.dimension`, then sends `block.type`; paired graphic has the same class of issue | Restoration contract includes original dimension and current complete layers/actor data; no incomplete live restoration is attempted. |
| Readiness/lifecycle | Fixed window, latency retry chain and PacketViolationWarning treated as open evidence; quit/destructor cleanup | Unique generations, retired windows, explicit deadlines and tested idempotent model cleanup. Violation warnings never acknowledge readiness. |
| InventorySlot width | Reference writes container ID as unsigned varint; target schema says uint8 | Prototype uses uint8, with a >127 regression fixture. This is a schema discrepancy, not a captured failure. |

## Codec audit

Read ContainerOpen/Close, InventoryContent/Slot, UpdateBlock, BlockActorData, ItemStackRequest/Response,
ContainerSetData, CraftingData, PlayerEnchantOptions, ItemRegistry, trade/book/editor/crafter packets and their relevant types/enums.
`research/packet-inventory.json` indexes 29 packet schemas. Catalog roles are generated from r26_u4 `ContainerEnumName`.

Details covered by fixtures: signed x/y/z varints, signed-varlong actor ID, extra close type byte,
fixed int32 stack identity, both outer action variant and inner action byte, negative odd request IDs,
dynamic-container optional uint32, double optional response fields, fixed int16 item IDs,
InventoryContent uvarint window versus InventorySlot uint8 window, and little-endian typed NBT.
The pinned library accepts mismatching inner action discriminants; our wrapper rejects them. Its
request decoder supports only take/place/swap/drop, lacks our allocation/size bounds, and does not
establish a complete crafting codec. Do not attach it directly to arbitrary client input.

Only synthetic storage/open/close/error/NBT fixtures pass. No successful-response name/durability
fixture from a real client, complete custom-item registry capture, CraftingData recipe execution,
enchant seed/offer capture, editor or crafter interaction capture exists. Live adapter remains disabled.
Item IDs are not a handwritten registry; the Registry prototype takes authoritative entries with a
BDS hash, protocol and behavior-pack provenance. Replacing entries changes its epoch. Actual
live registry collection and consumer invalidation must be wired before a transactional release.

## Archive discovery and additional entities

Only the workspace/attachment area and then the user-named Downloads directory were searched.
Read `bedrock-server-Linux-1.26.45.1.zip` and `bedrock-server-Windows-1.26.45.1.zip` without modifying them.
`tools/inspect_bds.py` parses ELF/PE headers and JSON/JSONC entity definitions. Hashes and complete
inventory/equipment/trade component observations are in `research/bds-builds.json`.

The Linux ELF is x86-64, no `.symtab`, no `.debug*`; `.dynsym` remains. There are 3932 recipe JSON
paths across bundled pack/version directories; that count is not the effective loaded recipe set.
Discovered contexts include nautilus/zombie nautilus, camel husk, zombie horse, trader llama,
agent, allay, panda and piglin. Inventory/equippable components do not prove a remote GUI.
Actual selected behavior packs and vanilla comparison tests remain necessary.
