import json
from pathlib import Path
mod = Path(__file__).resolve().parents[1]
root = mod / "red4ext/plugins/m1arc/mods/m1nsook.free-chrome"
src = (mod / "r6/scripts/FreeChrome/FreeChromeSettings.reds").read_text(encoding="utf-8")
assert json.loads((root / "mod.json").read_text(encoding="utf-8"))["requires"]["m1arc"] == "1.0.1"
assert json.loads((root / "settings.json").read_text(encoding="utf-8"))[0]["default"] is True
assert not list(mod.glob("bin/**/*.lua"))
for x in ("M1Arc_GetBool", "M1Arc_Log", "TweakDBInterface.GetFlat", "TweakDBManager.SetFlat", "cost <= 200", "TinkererVariant_inline", 'Equals(typ, n"Float")', 'NameToString(typ)', "TweakDBManager.UpdateRecord(record)", "extends ScriptableTweak", "protected cb func OnApply()", "M1nsook.FreeChrome.Backup.", "this.Write(backupId, value)", "this.Write(id, original)"):
    assert x in src, x
assert src.index("this.Write(backupId, value)") < src.index("this.Write(id, this.Zero(value))")
detach = src.split("private func OnDetach()", 1)[1].split("public func Tick()", 1)[0]
assert "this.running = false" in detach and "patch.Run" not in detach and "SetFlat" not in detach
for lang in ("en_US", "pl_PL", "ko_KR"):
    assert json.loads((root / "language" / (lang + ".json")).read_text(encoding="utf-8"))["settings"]["enabled"]["description"]
print("PASS: early TweakXL startup, process-local backups, no restore on reload, and native manifests")
