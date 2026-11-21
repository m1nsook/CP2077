# m1core — scripting API documentation moved

The canonical developer-facing scripting API, reference, Lua editor definitions and examples are maintained in the separate **m1script** repository:

- **Repository:** https://github.com/m1nsook/m1script
- **API foundation PR:** https://github.com/m1nsook/m1script/pull/10
- **Published documentation:** https://m1nsook.github.io/m1script/
- **Machine-readable contract:** https://github.com/m1nsook/m1script/blob/main/api/contract.json

**m1core remains the standalone RED4ext DLL runtime:** LuaJIT, Win32 input, game hooks, native RTTI bridge, settings/persistence and installed game-root ZIP are implemented here. No additional CET or m1script runtime is required.

**Licensing:** m1script is GPL-3.0 and m1core retains its own MIT project license. No GPL-covered native library is linked into m1core.

For current implementations see `core/src/scripting/M1CoreLuaAPI.cpp`, `core/src/m1core_registry_api.cpp`, `core/src/scripting/ScriptContext.cpp`, and `native/registry.cpp`. The public API contract should be updated in m1script before adding new external API bindings here.

Maintainer: **민숙 MISO @m1nsook**
