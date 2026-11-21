#pragma once

// Independent, native C++ safety boundary for a mod's lifecycle callback.
// Lua exceptions/errors remain handled by ScriptContext's protected Lua calls.
// This handles unexpected C++ exceptions around each context invocation;
// Windows structured exceptions / access violations are NOT recoverable here.
#include <exception>
#include <string_view>
#include <utility>

namespace m1core::scripts {

template <typename Mods, typename Callback, typename Reporter>
void ForEachModSafely(const Mods& mods, std::string_view event,
                      Callback&& invoke, Reporter&& report)
{
    // Report failures without letting a logger/sink exception interrupt other
    // mods. Script contexts are not removed or reloaded during iteration.
    const auto reportFailure = [&](std::string_view mod, const char* reason)
    {
        try { report(mod, event, reason); }
        catch (...) {} // Diagnostics must never break the game update loop.
    };

    for (const auto& [name, context] : mods)
    {
        try
        {
            invoke(context);
        }
        catch (const std::exception& error)
        {
            reportFailure(name, error.what());
        }
        catch (...)
        {
            reportFailure(name, "Unknown native C++ exception");
        }
    }
}

} // namespace m1core::scripts
