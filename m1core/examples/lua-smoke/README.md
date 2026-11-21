# m1core Lua smoke example

Developer fixture, NOT included in the normal game-root m1core DLL ZIP.

Source scripts live under `red4ext/plugins/m1core/mods/<mod-folder>/`.

- `init.m1s`: source entry script using `registerForEvent('onInit')`
- `helper.m1s`: extensionless `require('helper')` module
- `compat.lua`: legacy extension compatibility

Windows CI loads these files using the actual LuaJIT engine linked into
the m1core DLL, checks return values, generates real LuaJIT bytecode via
`lua_dump`, executes it, and rejects invalid source/binary modes.

For manual game integration verification, copy only the three script files
into `red4ext/plugins/m1core/mods/m1core-smoke/` and launch the game.
Look for `[m1core] Lua smoke test loaded` in the scripting log.
Remove the example folder afterward.

This fixture proves LuaJIT and the shared file resolver, not yet the
full in-game LuaSandbox/ScriptContext lifecycle. No CET installation needed.
