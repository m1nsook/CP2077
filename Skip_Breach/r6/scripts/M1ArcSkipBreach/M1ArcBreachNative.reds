// Skip Breach ONLY: optional game-version-gated RED4ext native functions.
// Implementation is registered by m1arc.dll. The regular M1Arc.reds settings
// API does NOT declare experimental game-controller native methods.
// Remove the whole M1ArcSkipBreach directory to recover if the game updates.
native func M1Arc_BreachInputAvailable() -> Bool;
native func M1Arc_BreachSelectCell(controller: wref<inkLogicController>, position: Vector2) -> Bool;
