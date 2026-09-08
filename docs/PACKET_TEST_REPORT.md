# Packet adapter test report — 0.3.0a2

The Ender Chest and developer inventory menu adaptations are implemented. The
overall requested product is still an alpha; the unimplemented catalog entries
and remaining platform/crash qualifications prevent a production-ready claim.

The suite passes **169 automated tests**, and the existing pinned Endstone 0.11.0
baseline audit finds **52/52 symbols**. Live tests use Endstone 0.11.10, BDS
1.26.45.1/protocol 2169, and Windows Bedrock 1.26.45 with Classic UI and mouse/keyboard.

| Actual client case | Verified result |
|---|---|
| Inventory menu to native anvil | The protected button closed its packet screen and opened a rendered native anvil. The named Sharpness sword entered the native input and returned unchanged. All three native item requests succeeded. |
| Real Ender round trip | Shift-click deposited the sword into the actual Ender slot, closing/reopening retained it, and withdrawal succeeded. No separate vault was used. |
| Forced cursor close | The displayed cursor held the sword while its real source remained populated. Forced closure preserved it and refreshed the native hotbar correctly. |
| Drop rejection | The packet drop request received an error response, with no item loss or world drop. This is an explicit candidate restriction. |
| External inventory edit | Adding one test paper item during a reservation closed the UI. The external change and original sword were preserved; removing the test paper restored the baseline. |
| Protected icons and pages | Shift-transfer of the icon was rejected. Page two and back navigation rendered, and the exported function ran once without granting an icon. |

All four final captures have identical before/after inventory slots, counts, NBT
hashes, XP, dimension and position. The snapshots include the named enchanted
sword, Netherite pickaxe, patterned banner and filled map. The external-edit test
explicitly restores its one added paper item before the final comparison.

The evidence collector verifies **19 installed implementation files**, including
the native companion, against the source and installed wheel. It preserves
captures, authoritative Ender snapshots, 19 screenshots and filtered server events
in the private `research/packet-evidence/index.json`. Run
`python tools/collect_packet_evidence.py` only against that captured test run.
The index records its exact installed wheel hash; a later artifact's manifest must
state whether it matches that whole wheel or only the verified implementation.

Earlier test attempts exposed timestamp scaling, close acknowledgement, stale
native-window closure and hotbar resynchronization issues. They were fixed and
the final cases rerun. Earlier failed captures remain separate from this index.
A prior clean server restart also retained the committed Ender sword; that earlier
candidate's snapshot is `research/packet-ender-after-restart.json`, not evidence
of crash atomicity for the final wheel.

After the capability catalog refresh, the final packaged Windows wheel was
installed and the server restarted with the named sword reserved on a packet
cursor. Rejoining preserved the exact inventory, NBT, XP, position and empty
Ender Chest, with no residual session. The developer inventory menu rendered on
this installed wheel. Its Ender navigation reached the acknowledged open stage;
Windows foreground refusal prevented an additional final Ender screenshot. The
earlier four-capture run retains the rendered Ender evidence. The separate
private `research/packet-evidence/final-install.json` installation record
pins this wheel and its installed files; it does not replace the original
four-capture identity or claim process-crash atomicity.

Remaining limits: direct drops inside packet screens, shield/adventure predicate
descriptor fixtures, process-crash/save atomicity, native transient-input death
recovery, Linux/mobile/controller coverage, broad plugin coexistence and performance
qualification. General storage, held shulkers, processing and contextual editors
are not completed by this adapter. Native and packet gameplay remain disabled in
the shipped default configuration. See [the API contract](PACKET_INVENTORY.md).
