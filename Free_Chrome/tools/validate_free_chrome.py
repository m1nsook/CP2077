import json
from pathlib import Path
mod = Path(__file__).resolve().parents[1]
root = mod / "red4ext/plugins/m1arc/mods/m1nsook.free-chrome"
src = (mod / "r6/scripts/FreeChrome/FreeChromeSettings.reds").read_text(encoding="utf-8")
assert json.loads((root / "mod.json").read_text(encoding="utf-8"))["requires"]["m1arc"] == "1.0.1"
assert json.loads((root / "settings.json").read_text(encoding="utf-8"))[0]["default"] is True
assert not list(mod.glob("bin/**/*.lua"))
for x in ("M1Arc_GetBool", "M1Arc_Log", "TweakDBInterface.GetFlat", "TweakDBManager.SetFlat", "cost <= 200", "TinkererVariant_inline", "this.IsNumeric(value)", "this.IsZero(current)", "this.original[i]", "this.Restore()", "this.running = false"):
    assert x in src, x
for lang in ("en_US", "pl_PL", "ko_KR"):
    assert json.loads((root / "language" / (lang + ".json")).read_text(encoding="utf-8"))["settings"]["enabled"]["description"]
print("PASS: Free Chrome native patch structure and manifests (not a runtime test)")

assert 'Equals(typ, n"Float")' in src and 'Equals(typ, n"Int32")' in src, "Game 2.50 VariantTypeName returns CName"
assert 'NameToString(typ)' in src, "Convert CName before concatenation"
assert 'Equals(typ, "Float")' not in src and 'Equals(typ, "Int32")' not in src
assert 'TweakDBManager.UpdateRecord(record)' in src
assert 'TweakDBManager.UpdateRecord(this.ownedRecords[i])' in src
assert 'this.Probe("Before scan")' in src and 'this.Probe("After scan")' in src
assert 'this.Probe("After restore")' in src
