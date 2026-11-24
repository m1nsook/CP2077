from pathlib import Path
import json
import re
from zipfile import ZipFile, ZIP_DEFLATED

root = Path(__file__).resolve().parents[1]
manifest_dir = root / "red4ext/plugins/m1arc/mods/m1nsook.delamain-av"
manifest = json.loads((manifest_dir / "mod.json").read_text(encoding="utf-8"))
settings = json.loads((manifest_dir / "settings.json").read_text(encoding="utf-8"))
assert manifest["id"] == "m1nsook.delamain-av"
assert manifest["scope"] == "Gameplay"
assert re.fullmatch(r"[0-9]{1,5}\.[0-9]{1,5}\.[0-9]{1,5}", manifest["version"]), "M1 Arc only accepts numeric x.y.z manifest versions"
assert manifest["version"] == "0.1.1"
required = manifest["requires"]["m1arc"]
assert re.fullmatch(r"[0-9]{1,5}\.[0-9]{1,5}\.[0-9]{1,5}", required)
assert tuple(map(int, required.split("."))) <= (1, 0, 1), "Requires exceeds M1 Arc loader compatibility constant"
ids = {item["id"] for item in settings}
assert ids == {"spawn", "open", "board", "close", "exit", "status", "remove"}
assert all(item["kind"] == "action" for item in settings)
script = (root / "r6/scripts/DelamainAV/DelamainAV.reds").read_text()
assert set(re.findall(r'M1Arc_ConsumeAction\("m1nsook.delamain-av", "([^"]+)"\)', script)) == ids
assert 'GetBackLeftPassengerSlotID()' in script
assert 'evt.slotID = n"seat_front_left"' in script
assert 'new UnmountingRequest()' in script
yaml = (root / "r6/tweaks/DelamainAV/DelamainAV.yaml").read_text()
assert "Vehicle.m1nsook_delamain_av:" in yaml
assert "$base: Vehicle.q203_delamain_av" in yaml
assert "VehicleMountFront" not in yaml and "VehicleHijack" not in yaml
out = Path("dist/Delamain-AV-v0.1.1-dev.3.zip")
out.parent.mkdir(exist_ok=True)
with ZipFile(out, "w", ZIP_DEFLATED) as z:
    for folder in ("r6", "red4ext"):
        for file in sorted((root / folder).rglob("*")):
            if file.is_file():
                z.write(file, file.relative_to(root).as_posix())
with ZipFile(out) as z:
    assert z.testzip() is None
print("Static checks passed. REDscript compilation and in-game behavior remain unverified.")
print(out)
