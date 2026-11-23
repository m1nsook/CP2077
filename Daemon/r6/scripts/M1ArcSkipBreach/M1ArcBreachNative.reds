// Skip Breach ONLY: native functions are implemented INSIDE m1arc.dll.
// All four symbols are registered by the matching M1 Arc PR #1 DLL.
// Without this mod installed and enabled, the native input bridge is inert.\n// Keep the experimental declarations in this addon, NOT in the stable
// r6/scripts/m1arc/M1Arc.reds core. Install both matching builds together.
// If the game updates and native validation fails, remove this addon folder.
native func M1Arc_BreachInputAvailable() -> Bool;
native func M1Arc_BreachSelectCell(controller: wref<inkLogicController>, position: Vector2) -> Bool;
// Physical Windows mouse click at a validated, screen-normalized cell
// centre from the live ink widget hierarchy. This bypasses the original
// native selector. True means SendInput was submitted, NOT game success.
native func M1Arc_BreachClickAt(controller: wref<inkLogicController>, normalized: Vector2) -> Bool;

// Delivers first-screen REDscript diagnostic transitions to the SAME
// red4ext/logs/m1arc*.log as native game-input dispatch, so testers only
// need one log. This does not change game state or register a scripting VM.
// A return value of true only means the diagnostic was logged.
native func M1Arc_BreachTrace(message: String) -> Bool;
