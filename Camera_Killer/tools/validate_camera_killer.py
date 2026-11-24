"""Static M1 Arc integration checks; in-game behavior still needs testing."""
import json
from pathlib import Path
mod = Path(__file__).resolve().parents[1]
root = mod / "red4ext/plugins/m1arc/mods/m1nsook.camera-killer"
manifest = json.loads((root / "mod.json").read_text(encoding="utf-8"))
settings = json.loads((root / "settings.json").read_text(encoding="utf-8"))
source = (mod / "r6/scripts/CameraKiller/CameraKiller.reds").read_text(encoding="utf-8")
assert manifest["id"] == "m1nsook.camera-killer"
assert manifest["requires"]["m1arc"] == "1.0.1"
assert len(settings) == 1 and settings[0]["id"] == "enabled"
assert settings[0]["kind"] == "boolean" and settings[0]["default"] is True
for lang in ("en_US", "pl_PL", "ko_KR"):
    loc = json.loads((root / "language" / (lang + ".json")).read_text(encoding="utf-8"))
    assert set(loc["settings"]) == {"enabled"}
    assert all(loc["settings"]["enabled"][key] for key in ("label", "description", "group"))
assert 'M1Arc_GetBool("m1nsook.camera-killer", "enabled")' in source
assert 'M1Arc_Log("m1nsook.camera-killer", "INFO"' in source
assert source.count("wrappedMethod(evt)") == 2
assert "camera.m_cameraKillerIncoming = false" in source
assert "!camera.IsMarkedAsQuest()" in source
assert "this.QueuePSEvent(camera, camera.ActionSetDeviceOFF())" in source
assert "if !this.running { return; }" in source
print("PASS: M1 Arc manifest, locales, propagation gate and original guards")
