# Linux inventory hook diagnostic

This is the source of private diagnostic `18cdb117c7d798d70552ead709dfbf5a44db311639e4cfa2de2cd4dcb2b671ea`.
It is research code, excluded from the framework's installed plugin and SDK.
The [recorded run](../../linux-inventory-hooks.json) includes exact module
identities, lifecycle results and the negative bundle-content control.

The probe forwards BDS inventory save, container-change and force-balance
setter calls. Only the console can arm a watch for the sole connected,
valid operator. `snapshot` compares two native inventory saves; `same-item`
replaces exactly six pre-existing plain stones in slot7 with their SDK copy.
Neither command suppresses saves. Watch state clears on quit and disable.
Callback storage and the module stay pinned until process exit.

The identical replacement advances the setter counter while BDS skips its
container-change notification. Both hooks observe ordinary quick-move.
However, vanilla bundle extraction changed saved bundle contents without
advancing its slot counter. These hooks **do not yet establish a complete
item identity or mutation lock**. Snapshot pairs during cursor gestures cover
the native inventory only, so they are not transaction snapshots including
the cursor.

`build.sh` records the exact private build command. It expects the pinned
Clang20/libc++ Docker environment, Endstone0.11.10 source and its dependencies
under `/headers`, generated dependency headers under `/generated`, this
directory mounted read-only at `/source`, and the repository at `/repo`.
Only `/output` is writable. The independently compiled `layout.cpp` checks
the actual Linux header calling conventions; it does not establish Windows
compatibility. Fingerprints refer only to the admitted Linux binary pair.

Execution was limited to the backed-up disposable world marked
`/data/.vcf-linux-fixture-v1`. No server binary, world, raw saved item data,
private account data, or prebuilt diagnostic plugin is distributed here.
