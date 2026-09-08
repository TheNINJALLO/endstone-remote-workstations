"""Development-only scope validation. It never upgrades gameplay evidence."""
from pathlib import Path
import argparse, hashlib, json, re, sys
if not __debug__:
    raise SystemExit('Scope gates require assertions enabled; Python optimization is refused.')
ROOT=Path(__file__).resolve().parents[2]
EXPECTED_BASELINE="33242db49532f56835d955307491c2aff5d956ac990e0591697a500e6c6c0342"
MODES={"real-source","transient-virtual","persistent-virtual","native-context","custom-replacement"}
CHECKS={"permissions","stale_requests","replay","closing","disconnect","source_removal","durability"}

def validate_report(baseline,report,platform):
    assert report["platform"]==platform,"Mismatched server platform"
    assert report["baseline_sha256"]==EXPECTED_BASELINE,"Mismatched report baseline"
    expected={e["id"]:e for e in baseline["entries"]}
    assert len(report["entries"])==len(expected) and {e["id"] for e in report["entries"]}==set(expected),"Dropped or duplicated canonical entry"
    assert set(report["extra_surfaces"])==set(baseline["extra_ui_surfaces"]),"Dropped original API surface"
    for row in report["entries"]:
        original=expected[row["id"]]
        for field in ("permission","aliases","commands"):
            assert row[field]==original[field],f"Report {field} drift: {row['id']}"
        assert row["source_kind"]==original.get("context_kind","none"),"Source contract drift: "+row["id"]
        assert row["family"]==original["category"],"Family drift: "+row["id"]
        assert row["historical_status"]==original["status"],"Historical provenance drift: "+row["id"]
        assert set(row["modes"])==MODES,"Dropped presentation mode: "+row["id"]

def evidence(report,entry,mode,reference):
    """A reviewed attestation is still required; a label/hash alone is not proof.

    Only redacted, source-controlled interactive records can satisfy this gate.
    Baseline captures, generated fixtures and build logs cannot be substituted.
    """
    assert isinstance(reference,dict) and set(reference)=={"path","sha256"},"Missing evidence reference"
    relative=reference["path"]
    assert isinstance(relative,str) and "\\" not in relative,"Invalid evidence path"
    path=ROOT/relative
    parent=(ROOT/"research/native-evidence/interactive").resolve()
    assert path.resolve().is_relative_to(parent) and path.suffix==".json","Evidence is not an interactive record"
    assert path.is_file() and path.stat().st_size<=1024*1024,"Missing or oversized interactive evidence"
    raw=path.read_bytes()
    assert hashlib.sha256(raw).hexdigest()==reference["sha256"],"Changed interactive evidence"
    record=json.loads(raw)
    assert record["kind"]=="native-runtime-client-qualification","Historical, generated or build-only evidence"
    assert record["platform"]==report["platform"] and record["entry"]==entry and record["mode"]==mode,"Evidence applies to another entry/platform/mode"
    runtime=report["runtime"]
    for field in ("artifact_sha256","bds_sha256","loader_sha256"):
        assert re.fullmatch(r"[0-9a-f]{64}",runtime.get(field,"")),"Missing exact runtime/artifact hash"
        assert record[field]==runtime[field],"Evidence applies to a different installed artifact/runtime"
    assert re.fullmatch(r"[0-9a-f]{40}",record["source_revision"]),"Missing source revision"
    assert record["source_revision"]==report["native_tests"]["source_revision"],"Evidence belongs to another build revision"
    assert record["stock_client"] is True and record["public_sdk"] is True,"Not a stock-client SDK demonstration"
    assert record["client"]["version"] and record["client"]["device"] and record["client"]["input"],"Missing client profile"
    assert record["client"]["protocol"]==runtime["protocol"],"Client protocol mismatch"
    checks=record["checks"]
    for name in CHECKS:
        state=checks[name]
        assert state=="passed" or (name in {"source_removal","durability"} and state=="not-applicable" and record.get("not_applicable",{}).get(name)),"Incomplete safety check: "+name
    assert record["example"]["consumer"] and record["example"]["configuration"] and record["example"]["procedure"],"Missing runnable example"
    assert record["observed_behavior"] and record["expected_behavior"],"Missing measurable behavior"
    return record

def entry_qualified(report,row):
    if row.get("acceptance")!="qualified":
        return False
    try:
        original=False;custom=False;any_record=False
        for mode,details in row["modes"].items():
            references=details.get("qualification_records",[])
            if details.get("adapter")=="implemented":
                assert references,"Implemented mode has no interactive evidence"
            for reference in references:
                assert details.get("adapter")=="implemented" and not details.get("blocker"),"Mode is still blocked"
                record=evidence(report,row["id"],mode,reference);any_record=True
                assert all(record["client"][key]==details["client"][key] for key in ("device","version","input")),"Evidence belongs to another declared client profile"
                checks=record["checks"]
                assert checks["input"]=="passed" and checks["restoration"]=="passed","Incomplete interaction/cleanup"
                presentation=record["presentation"]
                assert presentation in {"original-native","correct-role","explicit-replacement"},"Unknown presentation"
                if presentation=="original-native":
                    assert checks["opening"]=="passed" and checks["rendering"]=="passed" and record["native_bindings"]=="fingerprinted","Native presentation not qualified"
                    if mode in {"real-source","native-context"}:
                        assert checks["original_behavior"]=="passed" and record["source_contract"]=="preserved","Original source operation not demonstrated"
                        original=True
                else:
                    assert record["native_available"] is False,"Alternative misrepresented as native"
                    assert mode=="custom-replacement" or presentation=="correct-role","Implicit native replacement"
                if mode in {"transient-virtual","persistent-virtual","custom-replacement"}:
                    assert checks["custom_behavior"]=="passed" and checks["transaction_callback"]=="passed","Custom operation not demonstrated"
                    custom=True
                if mode=="persistent-virtual":
                    assert checks["durability"]=="passed" and checks["restart"]=="passed","Persistent mode lacks restart proof"
        assert any_record and custom,"No per-entry custom SDK behavior evidence"
        assert row["custom_available"] is True,"Custom result conflicts with capability data"
        # A real original native path cannot be replaced by a generic custom
        # menu. Former candidates/non-window roles require a separate reviewed
        # runtime investigation, not a blanket exclusion or skipped test.
        if not original:
            assert not row["historical_status"].startswith("implemented"),"Missing original native behavior"
            investigation=evidence(report,row["id"],"investigation",row["native_window_investigation"])
            assert investigation["native_window_exists"] is False and investigation["technical_reason"] and investigation["primary_sources"],"Unresolved native-screen investigation"
            assert row["native_available"] is False,"Non-window entry advertises native availability"
        return True
    except (AssertionError,KeyError,TypeError,ValueError,OSError):
        return False

def surface_qualified(report,name,value):
    if value.get("acceptance")!="qualified" or value.get("implementation")!="implemented" or value.get("client_test")!="passed":
        return False
    try:
        refs=value["qualification_records"];assert refs
        for ref in refs:
            record=evidence(report,name,"api-surface",ref)
            assert record["checks"]["custom_behavior"]=="passed" and record["checks"]["transaction_callback"]=="passed"
        return True
    except (AssertionError,KeyError,TypeError,ValueError,OSError):
        return False

def incomplete(reports):
    entries=[(r["platform"],e["id"]) for r in reports for e in r["entries"] if not entry_qualified(r,e)]
    surfaces=[(r["platform"],name) for r in reports for name,value in r["extra_surfaces"].items() if not surface_qualified(r,name,value)]
    return entries,surfaces
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
        validate_report(baseline,report,platform)
        reports.append(report)
    return baseline,reports
def main():
    parser=argparse.ArgumentParser();parser.add_argument("--require-qualified",action="store_true");args=parser.parse_args()
    baseline,reports=check()
    print("Scope retained: 69 canonical entries, original aliases/permissions/source contracts, 2 platform reports and 10 extra surfaces.")
    if args.require_qualified:
        missing,surfaces=incomplete(reports)
        if missing or surfaces:
            print(f"NOT QUALIFIED: {len(missing)} canonical/platform outcomes and {len(surfaces)} additional surface/platform outcomes remain incomplete.")
            return 2
    return 0
if __name__=="__main__":
    sys.exit(main())
