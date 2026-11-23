// Skip Breach ONLY: optional game-version-gated RED4ext native functions.
// All three symbols are registered by the matching M1 Arc PR #1 DLL.
// Keep the experimental declarations in this addon, NOT in the stable
// r6/scripts/m1arc/M1Arc.reds core. Install both matching builds together.
// If the game updates and native validation fails, remove this addon folder.
native func M1Arc_BreachInputAvailable() -> Bool;
native func M1Arc_BreachSelectCell(controller: wref<inkLogicController>, position: Vector2) -> Bool;

// Delivers first-screen REDscript diagnostic transitions to the SAME
// red4ext/logs/m1arc*.log as native game-input dispatch, so testers only
// need one log. This does not change game state or register a scripting VM.
// A return value of true only means the diagnostic was logged.
native func M1Arc_BreachTrace(message: String) -> Bool;
