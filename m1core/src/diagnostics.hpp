#pragma once

#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace m1core
{
enum class LogLevel { Debug = 0, Info = 1, Warning = 2, Error = 3 };

struct LogEvent
{
    std::string timestamp;
    LogLevel level;
    std::string source;
    std::string message;
};

// Thread-safe journal. The bounded in-memory panel is independent of its
// on-disk diagnostic trail. Does not register exception or graphics hooks.
class Diagnostics final
{
public:
    static Diagnostics& Get();
    void Initialize(const std::filesystem::path& directory);
    bool Write(LogLevel level, std::string_view source, std::string_view message);
    std::vector<LogEvent> Snapshot() const;
    void ClearView();

private:
    mutable std::mutex mutex_;
    std::filesystem::path file_;
    std::deque<LogEvent> events_;
    bool initialized_ = false;
};

const char* LogLevelName(LogLevel level);
} // namespace m1core
