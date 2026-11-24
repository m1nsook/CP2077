from pathlib import Path
import json
import re
import shutil

root = Path(__file__).resolve().parents[1]
manifest_dir = root / "red4ext/plugins/m1arc/mods/m1nsook.delamain-av"
manifest = json.loads((manifest_dir / "mod.json").read_text(encoding="utf-8"))
settings = json.loads((manifest_dir / "settings.json").read_text(encoding="utf-8"))
assert manifest["id"] == "m1nsook.delamain-av"
assert manifest["scope"] == "Gameplay"
assert re.fullmatch(r"[0-9]{1,5}\.[0-9]{1,5}\.[0-9]{1,5}", manifest["version"]), "M1 Arc only accepts numeric x.y.z manifest versions"
assert manifest["version"] == "0.2.13"
required = manifest["requires"]["m1arc"]
assert re.fullmatch(r"[0-9]{1,5}\.[0-9]{1,5}\.[0-9]{1,5}", required)
assert tuple(map(int, required.split("."))) <= (1, 0, 1), "Requires exceeds M1 Arc loader compatibility constant"
ids = {item["id"] for item in settings if item["kind"] == "action"}
assert ids == {"spawn", "exit", "status", "remove"}
assert any(item["id"] == "ground_clearance" and item["kind"] == "float" for item in settings)
script = (root / "r6/scripts/DelamainAV/DelamainAV.reds").read_text()
assert set(re.findall(r'M1Arc_ConsumeAction\("m1nsook.delamain-av", "([^"]+)"\)', script)) == ids
assert 'GetBackLeftPassengerSlotID()' in script
assert 'evt.slotID = n"seat_front_left"' in script
assert 'request.preservePositionAfterMounting = true' in script
assert 'SetShouldForceExitDelamain(true)' in script
assert 'CanStartAVDisembark' in script
assert 'new UnmountingRequest()' not in script
assert 'GetTeleportationFacility' not in script
assert 'AV_EXIT_STATE_RELEASED' in script
assert 'AV_EXIT_TRACE' in script
assert 'AV_EXIT_WORKSPOT' in script
assert 'SendJumpCommandEnt' not in script
assert 'GetWorkspotInstance(player.GetEntityID())' in script
assert 'GetIdOfExitAnim(player,' in script
assert 'IsAVExitRequested' in script
assert 'this.SetIsCar(stateContext, false)' in script
assert 'UnmountFromVehicle(scriptInterface.owner, scriptInterface.executionOwner, false,' in script
assert 'SendSlowExitSignal' not in script
assert 'SyncRaycastByCollisionGroup' in script
assert 'GROUND_DRIFT' in script
assert 'POSITION_INVALID' in script
assert 'GetWorldPosition().Z' not in script
assert 'EnumInt(gamePSMVehicle.Passenger)' in script
assert 'Cast<Int32>(gamePSMVehicle.Passenger)' not in script
yaml = (root / "r6/tweaks/DelamainAV/DelamainAV.yaml").read_text()
assert "Vehicle.m1nsook_delamain_av:" in yaml
assert "$base: Vehicle.av_rayfield_excalibur" in yaml
assert "Vehicle.q203_delamain_av_inline0" in yaml
assert "VehicleMountFront" not in yaml and "VehicleHijack" not in yaml
resource = root / "archive/pc/mod/DelamainAV.archive"
assert resource.is_file() and resource.stat().st_size > 100, "Native resource archive required"
assert resource.read_bytes()[:4] == b"RDAR"
assert (root / "archive/pc/mod/DelamainAV.xl").is_file()
assert 'entityTemplatePath: m1nsook\\\\delamain_av\\\\excalibur.ent'.replace('\\\\', '\\') in yaml
ui = (root / "r6/scripts/DelamainAV/DelamainAVSummon.reds").read_text(encoding="utf-8")
assert "AVSummonRows" in ui and "QueueSummon()" in ui
assert "TAXI_EXIT_BLOCKED" in script and "TAXI_PICKUP_READY" in script
# Reject actual transform calls, not historical comments describing dev.14.
assert not re.search(r"\bSetWorldTransform\s*\(", re.sub(r"//[^\n]*", "", script)), "Transform-based movement is prohibited"
assert "DelamainAV_PhysicsBegin(vehicle)" in script
assert "DelamainAV_PhysicsPulse(vehicle, speed)" in script
assert "TAXI_NATIVE_TOUCHDOWN" in script
assert "TAXI_NATIVE_ABORT" in script
cpp = (root / "native/src/physics.cpp").read_text(encoding="utf-8")
assert "kTickHash = 2526549425" in cpp and "ResolveBody(" in cpp
assert "g_tickOriginal(physics, deltaTime)" in cpp and "g_forceOriginal" not in cpp
native = root / "red4ext/plugins/DelamainAV/DelamainAV.dll"
assert native.is_file() and native.read_bytes()[:2] == b"MZ", "Native physics plugin required"
assert "TAXI_NATIVE_SETTLING" in script and "TAXI_LANDING_HANDOFF" not in script
assert script.count("system.CreateEntity(spec)") == 1
assert "vehicle as CarObject" in script
assert "kGravity = 0x1B0" in cpp
assert "CREATE_BEFORE" in script and "CREATE_AFTER" in script
assert "DelamainAV_DiagnosticLog(level, message)" in script
assert "DelamainAV_DiagnosticSnapshot(vehicle" in script
assert "body.vehicle mismatch" in cpp
assert "diagnostics.hpp" in cpp
assert "TAXI_MOUNT_BLOCKED" in script
assert "TAXI_BOARD_POSITION_INVALID" in script
out = Path("dist/Delamain-AV-v0.2.13-dev.20")
if out.exists():
    shutil.rmtree(out)
out.mkdir(parents=True)
for folder in ("r6", "red4ext", "archive"):
    shutil.copytree(root / folder, out / folder)
assert not list(out.rglob("*.zip")), "No nested ZIPs"
print("Static checks passed. REDscript compilation and in-game behavior remain unverified.")
print(out)
