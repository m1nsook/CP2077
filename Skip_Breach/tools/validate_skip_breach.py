"""Offline validation of the installable Skip Breach development package.

This is NOT a REDscript compiler and does NOT prove live game behavior.
"""
from pathlib import Path
import json
import re

root = Path(__file__).resolve().parents[1]
mod = root / "red4ext/plugins/m1arc/mods/m1nsook.skipbreach"
reds = root / "r6/scripts/M1ArcSkipBreach/M1ArcSkipBreach.reds"

manifest = json.loads((mod/"mod.json").read_text(encoding="utf8"))
schema = json.loads((mod/"settings.json").read_text(encoding="utf8"))
english = json.loads((mod/"language/en_US.json").read_text(encoding="utf8"))
template = json.loads((mod/"language/en_US.json.example").read_text(encoding="utf8"))
source = reds.read_text(encoding="utf8")
native_file = root / "r6/scripts/M1ArcSkipBreach/M1ArcBreachNative.reds"
native_source = native_file.read_text(encoding="utf8")
assert 'native func M1Arc_BreachInputAvailable() -> Bool;' in native_source
assert ('native func M1Arc_BreachSelectCell(controller: wref<inkLogicController>, '
        'position: Vector2) -> Bool;') in native_source
assert "@addMethod(inkLogicController)" not in native_source
assert "native func M1Arc_Breach" not in source  # contained in the optional API only

assert manifest["format"] == 1
assert manifest["id"] == mod.name
assert manifest["requires"]["m1arc"] == "0.1.0"
assert re.fullmatch(r"\d+\.\d+\.\d+", manifest["version"])
assert len(schema) == 1 and schema[0]["id"] == "enabled"
assert schema[0]["kind"] == "boolean"
assert schema[0]["default"] is False, "Unsafe: first-run auto breach must be opt-in"
assert english == template
assert {"mod.name","enabled.label","enabled.description"} == set(english)
for required in [
    "module MisoSkipBreach",
    "class M1ArcSkipBreachSystem extends ScriptableSystem",
    "class M1ArcSkipBreachCaptureRule extends MinigameGenerationRule",
    "OnProcessRule(size: Uint32",
    "grid[row][col].rarityValue",
    "chains[chain].rarities",
    "chains[i].ownerId",
    "@wrapMethod(HackingMinigameGameController)",
    "OnPositionSelected(position: Vector2)",
    "OnGameStateChanged(value: Int32)",
    "OnHandleInput(evt: ref<inkPointerEvent>)",
    "M1Arc_GetBool(\"m1nsook.skipbreach\",\"enabled\")",
    "M1Arc_BreachInputAvailable()",
    "M1Arc_BreachSelectCell(logic, Vector2(",
    "this.m_captureSerial += 1",
    "this.m_selectionSerial += 1",
    "this.m_controller.PauseTheTimer();",
    "this.m_controller.ResumeTheTimer();",
    "this.m_waitTicks > 15",
    "this.m_nodeCount >= 50000",
    "this.m_bestScore > 0 && ArraySize(this.m_bestPath) > 0",
    "LogChannel(n\"DEBUG\", \"[Skip Breach] \" + message);",
    "this.Report(44, \"Native selector unavailable:",
    "this.Report(54, \"No board captured after 2s.",
]:
    assert required in source, f"Missing expected game-side contract: {required}"
for forbidden in [
    "AutoBreachNativeSelectCell", "AutoBreachDirectInputAvailable",
    "RemoveAllModifiers", "registerForEvent(", "ObserveAfter(",
    "SetDaemonFulfilled", "SetProgramFulfilled",
    "SendInput(", "SetCursorPos(", "AutoBreachInput.dll", ".m1s", ".m1c",
]:
    assert forbidden not in source, f"Forbidden legacy/direct-success path: {forbidden}"

assert '&& M1Arc_BreachInputAvailable() {' not in source, "Don't hide capture behind selector readiness"
assert "return !this.m_budgetHit && this.m_bestScore > 0" not in source, "Budget must not discard valid paths"
print("PASS: game-root manifest, opt-in setting, native diagnostics, best-effort solver and no CET dependency")
