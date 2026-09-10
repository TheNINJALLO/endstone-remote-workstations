> **Legacy Python integration.** Use the [native C++ examples](../docs/NATIVE_EXAMPLES.md) for v0.5.0-native.1.

# Inventory helpers for RemoteWorkstations

The requested repositories are `TheNINJALLO/endstone-ninjos-backpacks` and
`TheNINJALLO/endstone-inventory-manager`. Their common packet helper is
`TheNINJALLO/endstone-inventoryui`, pinned by Backpacks to v2.0.6. Its main branch
currently resolves to the same commit. Exact source/archive identities are in
`research/helper-references.lock.json`.

Inventory Manager demonstrates access to the actual online player's Ender Chest:
`Player.ender_chest.get_item()` supplies the contents and `set_item()` writes them
back. Its client screen is an InventoryUI chest. That is a viable alternative to
the native BDS container model whose remote requests returned result 50. It does
not require creating another persistent vault. Backpacks contributes a separate
example of slot buttons, pagination, per-backpack ownership and typed NBT storage.

## Adapter implemented here

`inventoryui_2169.py` converts the **captured Windows BDS 1.26.45.1** request/response
layout to and from the object shape used by that InventoryUI release. It uses
RemoteWorkstations' bounded storage request decoder and implements the nested
response optionals observed on this exact build. It depends only on Python and
the existing RemoteWorkstations package.

```python
from integrations.inventoryui_2169 import RequestPacket, ResponsePacket

packet = RequestPacket()
packet.deserialize(received_payload)
requests = packet.request.request_data  # InventoryUI listener-compatible shape

# After the transaction owner has validated and committed server-owned state:
payload = ResponsePacket(server_responses).serialize()
```

These two classes are the codec replacement points for InventoryUI's listener.
They are explicitly used in a locally adapted provider; importing them does not
modify installed libraries or register game listeners. They are **not a complete
drop-in safety patch** for the existing InventoryUI plugin. Gameplay admission,
session ownership and transaction execution remain the provider's responsibility.
The existing installed 0.3.0a1 wheel/server were not replaced by this source work.

The adapter intentionally accepts only bounded storage take/place/swap/drop
requests. It rejects unsupported crafting/creative actions and trailing bytes.
The helper's misspelled `distination` field is retained for compatibility. Filter
strings must be empty, and the unused drop randomization hint is normalized to
false. An empty helper redacted-name string means no redacted value.

## Verification

The fixture `tests/fixtures/helper-ender-2169.json` contains four actual recorded
requests and four responses, with source capture hashes. Three requests are the
real-block Ender deposit/cursor-withdrawal/return comparison; the fourth is from
the failed remote native experiment. No new live helper transaction is implied.

Run:

```powershell
.venv\Scripts\python.exe -m pytest -q tests/test_inventoryui_helper_adapter.py
.venv\Scripts\python.exe tools/audit_inventory_helpers.py
```

The audit expects the three fixed source checkouts under `.research` and the
helper's exact packet dependency 0.0.9 under `.runtime/helper-codec-deps`. It loads
only the request/response codec files, so Backpacks' global import patches and
database code never execute. It compares both the original helper and the adapter
against the captured packets, and passes real helper-created response objects to
the adapter. Results are preserved in `research/inventory-helper-audit.json`.

See `docs/INVENTORY_HELPERS.md` for the integration findings and remaining work.
