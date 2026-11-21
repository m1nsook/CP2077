#pragma once

// M7: best-effort native sandbox resource cleanup during script reload.
// Every sandbox is visited even when a previous sandbox fails to close its
// resources; the reporter itself is also fault-contained. This does NOT
// recover Windows SEH access violations or corrupted Lua runtime state.
#include <cstddef>
#include <exception>
#include <utility>

namespace m1core::scripts {

template <typename Close, typename Report>
void ForEachSandboxSafely(std::size_t sandboxCount, Close&& close, Report&& report)
{
    for (std::size_t id = 1; id < sandboxCount; ++id)
    {
        try
        {
            close(id);
        }
        catch (const std::exception& error)
        {
            try { report(id, error.what()); }
            catch (...) {} // Logging must never prevent later cleanup.
        }
        catch (...)
        {
            try { report(id, "Unknown native cleanup exception"); }
            catch (...) {}
        }
    }
}

} // namespace m1core::scripts
