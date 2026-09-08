# Native chemistry tables

The Windows companion binds BDS 1.26.45.1's real Compound Creator (20), Element
Constructor (21), Material Reducer (22), and Lab Table (23) managers. Source and
subcontainer checks remain in the original native validity functions; an instance
adapter supplies the permitted remote distance. BDS owns inventory requests and
chemistry execution.

All four remain disabled by default. Enable `native.enabled`,
`native.linked.enabled`, and `native.chemistry.enabled` only in a qualified world
with Education features enabled. The companion reads the actual native LevelData
flag. It does not turn on world features or change the Education licensing offer.
The admitted client is Windows Bedrock 1.26.45, with Endstone runtime 0.11.10 and
public API 0.11.

Microsoft documents the Education toggle for chemistry in the standard Bedrock
client: [Using Education Features](https://edusupport.minecraft.net/hc/en-us/articles/360047556971-Learning-with-Minecraft-Bedrock-and-Java).
The current server exposes separate block names: `minecraft:compound_creator`,
`minecraft:element_constructor`, `minecraft:material_reducer`, and
`minecraft:lab_table`. Legacy `chemistry_table` commands convert to these names.

Dependency plugins use the existing versioned API:

```python
from endstone_remote_workstations.api import BlockContext, get_api

ui = get_api(self, minimum=(1, 4))
ticket = ui.open_linked(player, "elementconstructor",
    BlockContext("Overworld", (20, 64, 30), "myplugin.chemistry"))
```

The other keys are `compoundcreator`, `materialreducer`, and `labtable`.
The context must be selected by the server and point to the matching real loaded
table in the player's dimension. Interface permissions, source permissions and
registered protection guards are required. Configured command sources use
`native.linked.sources.<key>` and the same checks.

Every owned chemistry item request rechecks world features, source and interface
permissions, protection guards, and the actual active native window. The observed
Windows client combines through native ItemStackRequest action 9; its original
payload goes to BDS unchanged. Legacy Lab controls accept only an owned
start-combine request. Client position is
translated to the authorized real table before the original BDS handler runs.
The server decides the reaction; the client cannot select an output or reaction.
Native reaction updates are projected back to the display. Reactions affect the
actual source world, and cancellation does not undo a completed native reaction.
Retired lab display positions reject late controls for 60 seconds and are not
reused during that interval.

After cancellation or authorization revocation, a20 permits only bounded native
returns from the owned input/cursor slots, native drops when required for cleanup,
and disposal of previews. The actual manager/window must still match. New recipes,
combines, deposits and preview extraction remain blocked. This repairs an a19
failure in which an unconditional close guard blocked the client's input returns.
The failed a19 capture is retained; its missing stack returned after restart/rejoin,
but it did not pass immediate cancellation restoration. Installed a20 and a22
passed the actual 63-cursor plus 1-input cancellation regression. Installed a22
also passed a reducer cancellation with eight preview stacks and one input:
all nine native cleanup results succeeded and exact inventory/XP was restored.

Development evidence demonstrates native hydrogen creation, water output and
input return, cobblestone reduction into eight element outputs, an invalid lab
reaction producing garbage, and a valid lab reaction producing an ice bomb.
Physical Compound Creator and Lab Table controls reproduced the output behavior.
The Compound Creator retained its recipe inputs; the Lab Table consumed its
inputs, closed the screen and spawned its product at the actual table. The remote
adapter preserves this source-world behavior; it does not deliver dropped results
to the remote player's inventory. One manually dropped physical-control cursor
stack was recovered by the operator; no plugin recovery is claimed for that step.

The test recipe follows Microsoft's [Chemistry Lab Journal](https://education.minecraft.net/content/dam/education-edition/learning-experiences/chemistry/Chemistry-Lab-Journal.pdf).
`tools/collect_chemistry_evidence.py` checks the five native transaction cases and
the physical comparisons in `research/catalog-evidence/chemistry-development-index.json`.
That report identifies development catalog31 on the installed a18 host. It does
not qualify an installed a19 wheel or every recipe, isotope, device or crash path.

`tools/collect_chemistry_installed.py` binds seven selected a22 transaction and
lifecycle checks to the exact installed wheel. Hydrogen, water, material reduction
and the ice-bomb world output passed. The invalid Lab Table consumed its one water
and sent native failure reaction 7, but the world scans did not capture garbage;
that output remains unqualified. This does not certify all recipes or devices.

The ABI audit is produced by `tools/audit_education_native.py`. It binds exact
factory, vtable, validity and LevelData references to the supplied BDS hash.
The isolated `rw-chemistry` world is a verified clone; the original `rw-smoke`
world remains unchanged. Restore the original settings only after a clean stop
with `tools/chemistry_test_world.py restore`.
