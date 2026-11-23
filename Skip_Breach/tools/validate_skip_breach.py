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
assert 'native func M1Arc_BreachClickAt(controller: wref<inkLogicController>, normalized: Vector2) -> Bool;' in native_source
assert 'native func M1Arc_BreachTrace(message: String) -> Bool;' in native_source
assert 'native func LogChannel(' not in native_source, (
    "Skip Breach diagnostics must use the native M1 Arc log bridge, not an unverified DEBUG channel")
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
    'Get(n"MisoSkipBreach.M1ArcSkipBreachSystem")',
    "class M1ArcSkipBreachCaptureRule extends MinigameGenerationRule",
    "OnProcessRule(size: Uint32",
    "grid[row][col].rarityValue",
    "chains[chain].rarities",
    "chains[i].ownerId",
    "@wrapMethod(HackingMinigameGameController)",
    "OnPositionSelected(position: Vector2)",
    "OnGameStateChanged(value: Int32)",
    "M1Arc_GetBool(\"m1nsook.skipbreach\",\"enabled\")",
    "M1Arc_BreachInputAvailable()",
    "M1Arc_BreachSelectCell(",
    "this.m_captureSerial += 1",
    "this.m_selectionSerial += 1",
    "this.m_waitTicks > 120",
    "class M1ArcSkipBreachPulseEvent extends Event",
    "ui.QueueEvent(event);",
    "sys.ReceivePulse(this,evt.session,evt.captureSerial);",
    "@addMethod(HackingMinigameGameController)",
    "this.m_pulseCount >= 1500",
    "this.m_nodeCount >= 50000",
    "private func MatchMask(codes: array<Int32>) -> Int32",
    "private func RankMask(mask: Int32) -> Int32",
    "private func PrefixLength(codes: array<Int32>, daemon: Int32) -> Int32",
    "private func UpperScore(codes: array<Int32>, mask: Int32, remaining: Int32) -> Int32",
    "private func CandidatePriority(codes: array<Int32>) -> Int32",
    "let maxDepth: Int32 = Min(this.m_bufferSize,ArraySize(this.m_grid));",
    "this.m_firstCellRejects < 20",
    "Waiting for first-try hacking UI controller",
    "Native grid input not dispatched; retrying same untouched board",
    "this.m_handledCapture = this.m_captureSerial;",
    "this.m_bestScore > 0 && ArraySize(this.m_bestPath) > 0",
    "M1Arc_BreachTrace(message);",
    'M1Arc_BreachTrace("Matching OnPositionSelected callback received");',
    "this.Report(44, \"Native input not ready:",
    "this.Report(54, \"No board captured after 120 UI updates;",
]:
    assert required in source, f"Missing expected game-side contract: {required}"
for forbidden in [
    "AutoBreachNativeSelectCell", "AutoBreachDirectInputAvailable",
    "RemoveAllModifiers", "registerForEvent(", "ObserveAfter(",
    "SetDaemonFulfilled", "SetProgramFulfilled",
    "SendInput(", "SetCursorPos(", "AutoBreachInput.dll", ".m1s", ".m1c",
]:
    assert forbidden not in source, f"Forbidden legacy/direct-success path: {forbidden}"

# A first-screen no-op cannot be diagnosed from the puzzle screenshot alone.
# The installed capture-rule and generation callback must each leave a
# distinct trace, including the null-player and unready-system cases.
for trace in [
    'M1Arc_BreachTrace("Hacking UI OnInitialize entered")',
    'M1Arc_BreachTrace("Passive capture rule installed on initial board defaults")',
    'M1Arc_BreachTrace("Passive generation observer invoked")',
    'M1Arc_BreachTrace("Capture skipped: missing minigame controller")',
    'M1Arc_BreachTrace("Capture skipped: player object unavailable")',
    'M1Arc_BreachTrace("Capture skipped: ScriptableSystem unavailable")',
]:
    assert trace in source, f"Missing first-screen lifecycle diagnostic: {trace}"

# A plain Get(n"SystemName") compiles, but silently returns null if the
# ScriptableSystem class was declared inside a REDscript module.
# This was the root cause of the first in-game no-op despite READY selector.
assert 'module MisoSkipBreach' in source
assert 'Get(n"MisoSkipBreach.M1ArcSkipBreachSystem")' in source
assert 'Get(n"M1ArcSkipBreachSystem") as M1ArcSkipBreachSystem' not in source
assert 'this.m_controller.PauseTheTimer()' not in source, (
    "Never pause a one-shot gig puzzle before first confirmed input")
assert 'this.m_controller.ResumeTheTimer()' not in source, (
    "No timer state should be owned when we never pause")
assert 'this.m_handledCapture = this.m_captureSerial;\n        this.TryStart();' not in source, (
    "Do not consume the first puzzle before transient UI readiness")
assert 'class M1ArcSkipBreachTick extends DelayCallback' not in source, (
    "No DelayCallback game-time pump: first puzzle waits for initial input")
assert 'GameInstance.GetDelaySystem' not in source, "Use UISystem event updates"
# Verified cold-launch first-puzzle regression fixes (user-provided REDscript).
# The old validator insisted on a strict terminal-state and InProgress gate:
# those assumptions were wrong, and the uploaded script passed seven genuine
# cold-launch first attempts. Preserve its observed behavior, not our guess.
assert 'private let m_firstIntroReadyAt:' not in source
assert 'this.m_firstIntroReadyAt' not in source
assert 'private let m_unknownTicks: Int32;' in source
assert 'private let m_terminalTicks: Int32;' in source
assert 'private let m_ackRetries: Int32;' in source
assert 'private let m_earlyTwoLogged: Bool;' in source
assert 'this.m_unknownTicks = 0;' in source
assert 'this.m_terminalTicks = 0;' in source
assert 'this.m_earlyTwoLogged = false;' in source
assert 'this.m_selectionSerial = 0;' in source
assert 'this.m_pendingSerial = 0;' in source
assert 'private func LiveState() -> Int32' in source
assert 'live blackboard=' in source
assert 'Minigame entered terminal state (success/failure), raw=' in source

start_logic = source.split('private func TryStart() -> Void {', 1)[1].split(
    'public func Tick() -> Void {', 1)[0]
state_logic = source.split('public func RecordGameState(', 1)[1].split(
    'public func RecordSelection(', 1)[0]
tick_logic = source.split('public func Tick() -> Void {', 1)[1].split(
    'public class M1ArcSkipBreachCaptureRule', 1)[0]

assert 'this.m_gameState == 0 && this.m_unknownTicks < 600' in start_logic
assert 'Waiting for InProgress; grid is visible but intro is still running' in source
assert 'this.m_gameState == 1 && this.m_inProgressTicks < 20' in start_logic
assert start_logic.index('this.m_unknownTicks < 600') < start_logic.index(
    'this.m_handledCapture = this.m_captureSerial;\\n    this.m_firstCellRejects = 0;'.replace('\\n','\n')
)  # first captured board is not consumed by the opening intro
assert 'private func ResolveClickPoint(row: Int32, col: Int32) -> Vector2' in source
assert 'GetScreenPosition(root)' in source
assert 'GetScreenPosition(cell)' in source
assert 'grid.GetWrappingWidgetCount() != Cast<Uint32>(this.m_dimension)' in source
assert 'grid.GetNumChildren() < this.m_dimension * this.m_dimension' in source
assert 'AllParts/Top/NetworkCodeMapAndButton/wrapper/NetworkCodeMap/NetworkCodeMap/ContentWithBorder/ContentContainer/GridContainer/Grid' in source
assert 'this.ResolveClickPoint(0,0)' in source
assert 'this.m_ready && this.m_captureSerial != this.m_handledCapture' in source
assert 'let target: Vector2 = this.ResolveClickPoint(row,col);' in source
assert 'if target.X < 0.0 {' in source
assert source.index('if target.X < 0.0 {') < source.index(
    'M1Arc_BreachSelectCell('), 'Do not dispatch with invalid cell geometry'
assert 'private let m_plannedCapture: Int32;' in source
assert 'this.m_plannedCapture = this.m_captureSerial;' in source
assert 'let firstIndex: Int32 = this.m_bestPath[0];' in source
assert 'firstIndex / this.m_dimension, firstIndex % this.m_dimension' in source
assert 'if actualFirst.X < 0.0 {' in source
assert start_logic.index('if actualFirst.X < 0.0 {') < start_logic.index(
    'this.m_firstCellRejects = 0;'), 'Actual planned first cell must be ready'
assert 'M1Arc_BreachSelectCell(' in source
assert 'M1Arc_BreachClickAt(logic,' not in source

# The first game's Blackboard can report Succeeded(2) while the puzzle
# continues accepting cells. The script MUST disregard that early value.
assert 'state == 2 && (this.m_selectionSerial == 0' in state_logic
assert 'this.m_active && this.m_nextStep < ArraySize(this.m_path)' in state_logic
assert '[diag] ignoring Succeeded(2)' in state_logic
assert state_logic.index('state == 2 &&') < state_logic.index(
    'this.m_gameState = state;'), 'Ignore early 2 before caching terminal'
assert state_logic.index('this.ScheduleNext();\n      return;') < state_logic.index(
    'this.m_gameState = state;')
assert 'if state > 1 {' in state_logic
assert state_logic.rstrip().endswith('this.ScheduleNext();\n  }'), (
    'Terminal state must not permanently kill UI event pump')
assert 'this.m_gameState > 1 && this.m_terminalTicks < 300' in tick_logic
assert 'this.m_gameState == 0 && this.m_unknownTicks < 1000' in tick_logic

# Only the first cell, and only with unchanged acknowledgement serial,
# may be retried after a native dispatch failed to reach game callbacks.
assert 'this.m_waitTicks > 120' in tick_logic
assert 'this.m_nextStep == 0 && this.m_selectionSerial == this.m_pendingSerial' in tick_logic
assert 'this.m_ackRetries < 3' in tick_logic
assert 'this.m_ackRetries += 1;' in tick_logic
assert 'this.m_delayTicks = 30;' in tick_logic
assert 'this.Report(63, "No acknowledgement for first cell; board untouched, retrying")' in tick_logic
assert 'this.m_nextStep += 1;' in source
assert 'Matching OnPositionSelected callback received' in source

assert 'OnHandleInput(evt:' not in source, "Do not cancel on generic UI click"
assert 'RecordUserClick(' not in source, "Only native matrix selection can cancel"
assert ('this.m_gameState >= 0 && this.m_gameState <= 1\n        && M1Arc_GetBool("m1nsook.skipbreach","enabled")' in source), (
    "A mid-puzzle toggle must not mark the capture handled before ON")
assert 'Install the observer regardless of settings AND system readiness' in source, (
    "The first board must capture even if the ScriptableSystem is still attaching")
assert source.index('ArrayPush(data.rules,rule);') > source.index('if IsDefined(sys) {\n    sys.BeginSession('), (
    "The passive observer must be registered outside the system-ready branch")
assert 'First-board capture initialized its own session' in source
assert 'if this.m_controller == ctrl {' in source, (
    "Do not erase a board captured before the initialization handshake")
assert 'this.RecordGameState(this.m_controller,observedState);' in source, (
    "Initial Blackboard state must be observed even if the delayed listener misses it")
assert 'this.m_controller.PauseTheTimer()' not in source, (
    "Do not pause a one-shot puzzle before dispatch")
assert 'LogChannel(' not in source, (
    "All runtime lifecycle diagnostics must reach red4ext/logs/m1arc*.log via the bridge")
assert '&& M1Arc_BreachInputAvailable() {' not in source, "Don't hide capture behind selector readiness"
assert "return !this.m_budgetHit && this.m_bestScore > 0" not in source, "Budget must not discard valid paths"
print("PASS: game-root manifest, opt-in setting, native diagnostics, best-effort solver and no CET dependency")
