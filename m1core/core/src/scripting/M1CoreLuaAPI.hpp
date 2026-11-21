#pragma once

#include <sol/sol.hpp>

namespace m1core {
// Bind the existing data-only native settings registry to the embedded Lua VM.
// A per-game m1core DLL exposes this table; it is not a dependency on CET.
void BindLuaRegistryAPI(sol::state& lua, sol::table& globals);
} // namespace m1core
