# Native riding views

A mounted merchant could open successfully on the server while the Windows
client immediately closed its trade screen. The client continued positioning
the passenger at its distant vehicle after the plugin moved the displayed actor.

a24 reads the actual `PassengerComponent` through the matching Endstone
`Actor::getVehicle` implementation and verifies both unique and runtime IDs.
The real vehicle's native AddActor serializer supplies its passenger link and
seat type. Its temporary packet is destroyed through the native destructor;
no pointer or native allocation escapes the call.

Only the requesting client's link is temporarily detached before the remote
actor view opens. The server keeps the original vehicle, passenger, position,
trade manager, offers, inventory and transaction handling. Repeated native link
announcements for that passenger are suppressed during the view. Actual mount
changes pass through and retire the view; periodic native identity checks also
catch changes while opening. Cleanup reads the current native relationship and
restores it only when both client identities are still known and match. A private
actor display is removed instead of leaving an extra passenger behind.

Read-only cleanup remains available after ownership or Creative permissions are
revoked. Inventory admission continues using the original permission and native
owner checks. Packet callbacks do not send nested packets.

The exact ABI evidence covers Endstone runtime `0x112a60/251`, BDS runtime-ID
getter `0xde88f0/231`, AddActor factory `0xdef160/408`, payload constructor
`0xab8fa0/4250`, move constructor `0xab2970/369`, link producer `0xdf2300/490`,
and deleting destructor `0xaf5750/57`. Builds verify the full server/runtime
hashes, function bodies and relevant dispatch entries.

Development catalog39 on installed a23 passed a mounted native merchant trade:
ten operator-supplied clay balls became one emerald, the original items and XP
were unchanged, and the native boat relationship survived opening and closing.
The captured client detach and restore packets match the native passenger IDs.
See `research/catalog-evidence/riding-development-index.json`.

a24 passed eight installed cases: mounted open/cancel, a real ten-clay-for-one-
emerald Survival trade and manual close, cursor cancellation, protection and
configuration revocation, native dismount with automatic reboarding, native
dismount with the boat moved out of reach, and Creative open/cancel. Ten native
item results were accepted. The unchanged items, auxiliary data, NBT and XP were
checked exactly, including the named Sharpness sword returned from the cursor.
All 35 installed package files match the immutable native wheel. The unmounted
case left no obsolete attachment; other closes retained the actual relationship.
See `research/catalog-evidence/riding-installed-a24-index.json`.

The interrupted first trade and failed separated-dismount fixture attempt are
preserved separately and do not count as passes. This evidence does not establish
crash recovery, long-duration load, or other clients. The user scoped the current
qualification run to the available Windows client.
