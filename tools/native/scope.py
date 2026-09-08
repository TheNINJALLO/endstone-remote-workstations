"""Development-only scope validation. It never upgrades gameplay evidence."""
from pathlib import Path
import argparse, hashlib, json, re, sys
ROOT=Path(__file__).resolve().parents[2]
EXPECTED_BASELINE="33242db49532f56835d955307491c2aff5d956ac990e0591697a500e6c6c0342"
def check():
    path=ROOT/"research/original-ui-catalog.json"
    assert hashlib.sha256(path.read_bytes()).hexdigest()==EXPECTED_BASELINE,"Frozen baseline changed"
    baseline=json.loads(path.read_text(encoding="utf-8"))
    assert len(baseline["entries"])==69
    for relative,digest in baseline["files_sha256"].items():
        source=ROOT/"research/original-source"/relative
        assert hashlib.sha256(source.read_bytes()).hexdigest()==digest,"Raw baseline changed: "+relative
    native=(ROOT/"framework/catalog.cpp").read_text(encoding="utf-8")
    assert len(re.findall(r'^ \{"',native,re.M))==len(baseline["entries"])
    for entry in baseline["entries"]:
        prefix=" {"+", ".join(json.dumps(x) for x in (entry["id"],entry["category"],entry["permission"],"|".join(entry["aliases"]),entry.get("context_kind","none")))+", "
        assert prefix in native,"Native scope/permission/source drift: "+entry["id"]
    reports=[]
    for platform in ("windows-x64","linux-x64"):
        report=json.loads((ROOT/f"research/native-evidence/{platform}.json").read_text())
        assert {r["id"] for r in report["entries"]}=={r["id"] for r in baseline["entries"]}
        assert len(report["entries"])==69
        assert set(report["extra_surfaces"])==set(baseline["extra_ui_surfaces"])
        for row in report["entries"]:
            assert row["permission"]==next(e["permission"] for e in baseline["entries"] if e["id"]==row["id"])
            assert set(row["modes"])=={"real-source","transient-virtual","persistent-virtual","native-context","custom-replacement"}
        reports.append(report)
    return baseline,reports
def main():
    parser=argparse.ArgumentParser();parser.add_argument("--require-qualified",action="store_true");args=parser.parse_args()
    baseline,reports=check()
    print("Scope retained: 69 canonical entries, original aliases/permissions/source contracts, 2 platform reports and 10 extra surfaces.")
    if args.require_qualified:
        missing=[(r["platform"],e["id"]) for r in reports for e in r["entries"] if e["acceptance"]!="qualified"]
        surfaces=[(r["platform"],key) for r in reports for key,value in r["extra_surfaces"].items() if value["acceptance"]!="qualified"]
        if missing or surfaces:
            print(f"NOT QUALIFIED: {len(missing)} canonical/platform outcomes and {len(surfaces)} additional surface/platform outcomes remain incomplete.")
            return 2
    return 0
if __name__=="__main__":
    sys.exit(main())
