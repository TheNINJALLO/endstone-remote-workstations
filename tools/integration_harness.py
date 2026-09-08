"""Generate or validate honest manual test records; never marks a case passed itself."""
import argparse
import json
from pathlib import Path

CASES = {
    "commands": ["load-enable-disable", "permissions-member-operator", "alias-conflicts", "vanilla-enchant-preserved", "unknown-type-no-mutation", "unrelated-forms", "unrelated-inventory-plugin"],
    "storage": ["deposit-withdraw-close-reopen", "split-drag-quickmove", "full-inventory-cursor-overflow", "renamed-enchanted-custom-items", "stale-and-replayed-requests", "multi-action-atomic-rejection", "drop-after-commit", "rapid-switch-high-latency", "death-teleport-dimension-disconnect", "shutdown-and-reload"],
    "craft": ["3x3-versus-2x2", "shaped-shapeless-tags", "behavior-pack-override", "recipe-book-repeated-craft", "remainders", "forged-output-full-inventory", "close-reopen"],
    "anvil": ["rename", "item-and-material-repair", "enchant-merging", "prior-work", "insufficient-xp", "creative-survival-costs", "forged-output-close-reopen"],
    "enderchest-shulker": ["actual-ender-chest-concurrent-change", "held-box-full-nbt", "backing-item-locks", "nesting-restriction", "crash-during-writeback"],
    "processing": ["native-fuel-and-timing", "recipe-outputs-and-xp", "online-after-close", "offline-pause", "shutdown-restart", "loaded-linked-blocks"],
    "contextual-editors": ["real-trades-stock-and-xp", "entity-ownership-and-lifecycle", "variant-equipment-slots", "editor-permissions-and-context", "education-feature-gates"],
    "recovery": ["crash-before-journal", "crash-after-prepare", "crash-mid-bds-apply", "crash-after-drop", "crash-before-bds-save", "crash-after-bds-save", "quarantine-no-auto-issue"],
}

def new_record():
    return {"bds_sha256": None, "endstone_runtime": None, "client_game_version": None,
            "negotiated_protocol": None, "platform": None, "input_method": None, "ui_profile": None,
            "behavior_pack_hash": None, "plugins": [],
            "cases": [{"id": f"{group}/{case}", "status": "not-run", "evidence": [],
                       "vanilla_comparison": None, "notes": ""} for group, cases in CASES.items() for case in cases]}

def validate(record):
    errors = []
    expected = {case["id"] for case in new_record()["cases"]}
    if {case.get("id") for case in record.get("cases", [])} != expected:
        errors.append("test case set differs from this harness")
    for case in record.get("cases", []):
        if case.get("status") not in ("not-run", "blocked", "passed", "failed"):
            errors.append(f"{case.get('id')}: invalid status")
        if case.get("status") == "passed":
            for key in ("bds_sha256", "endstone_runtime", "client_game_version", "negotiated_protocol", "platform", "input_method", "ui_profile", "behavior_pack_hash"):
                if not record.get(key):
                    errors.append(f"{case['id']}: missing {key}")
            if not case.get("evidence"):
                errors.append(f"{case['id']}: passing requires evidence")
            if case["id"].split("/")[0] in ("craft", "anvil", "processing") and not case.get("vanilla_comparison"):
                errors.append(f"{case['id']}: passing requires a same-world vanilla comparison")
    return errors

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("init", "validate"))
    parser.add_argument("path", type=Path)
    args = parser.parse_args()
    if args.mode == "init":
        # Never overwrite manually collected results.
        with args.path.open("x", encoding="utf-8") as stream:
            json.dump(new_record(), stream, indent=2)
            stream.write("\n")
    else:
        errors = validate(json.loads(args.path.read_text()))
        print("\n".join(errors) if errors else "Record is internally consistent; this is not a certification.")
        raise SystemExit(bool(errors))
