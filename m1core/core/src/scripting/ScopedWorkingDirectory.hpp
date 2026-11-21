#pragma once
#include <filesystem>
#include <system_error>

namespace m1core::scripts
{
// The Lua filesystem APIs temporarily operate relative to a mod's root.
// Restore the process-wide working directory on every exit, including
// exceptions and early returns, without changing the Lua lock policy.
class ScopedWorkingDirectory final
{
public:
    explicit ScopedWorkingDirectory(const std::filesystem::path& root)
        : m_previous(std::filesystem::current_path())
    {
        std::filesystem::current_path(root);
    }

    ~ScopedWorkingDirectory() noexcept
    {
        std::error_code ignored;
        std::filesystem::current_path(m_previous, ignored);
    }

    ScopedWorkingDirectory(const ScopedWorkingDirectory&) = delete;
    ScopedWorkingDirectory& operator=(const ScopedWorkingDirectory&) = delete;
    ScopedWorkingDirectory(ScopedWorkingDirectory&&) = delete;
    ScopedWorkingDirectory& operator=(ScopedWorkingDirectory&&) = delete;

private:
    std::filesystem::path m_previous;
};
} // namespace m1core::scripts
