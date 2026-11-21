#pragma once

#include <sol/sol.hpp>
#include <exception>
#include <string>
#include <utility>

namespace m1core::scripts {

// Shared by the native ScriptContext and real LuaJIT regression tests.
// The reporter provides the mod/event context and must not throw.
template <typename Reporter, typename... Args>
bool InvokeLuaCallback(const sol::function& callback, Reporter&& report, Args&&... args)
{
    if (!callback)
        return true; // No registered event is not a failure.

    try
    {
        const sol::protected_function_result result = callback(std::forward<Args>(args)...);
        if (!result.valid())
        {
            const sol::error error = result;
            report(std::string(error.what()));
            return false;
        }
        return true;
    }
    catch (const std::exception& error)
    {
        report(std::string(error.what()));
    }
    catch (const char* error)
    {
        report(std::string(error ? error : "(null)"));
    }
    catch (...)
    {
        report(std::string("unknown exception"));
    }
    return false;
}

} // namespace m1core::scripts
