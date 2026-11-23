// Skip Breach ONLY: optional game-version-gated RED4ext native functions.
// Implementation is registered by m1arc.dll. The regular M1Arc.reds settings
// API does NOT declare experimental game-controller native methods.
// Remove the whole M1ArcSkipBreach directory to recover if the game updates.
// Cyberpunk 2077 2.01+ removed the REDscript declaration for this
// already-existing game-native logging function. Declare it only for this
// experimental addon, so LogChannel calls don't fail compilation.
// Source: jac3km4/redscript discussion #97; redmodding logging reference.
// This declaration is not a new dependency or an m1arc.dll native.
// Before a public release, consolidate this with other logging headers
// to avoid duplicated global LogChannel declarations in user mod stacks.
native func LogChannel(channel: CName, const text: script_ref<String>);

native func M1Arc_BreachInputAvailable() -> Bool;
native func M1Arc_BreachSelectCell(controller: wref<inkLogicController>, position: Vector2) -> Bool;
