#include "diagnostics.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <system_error>

namespace m1core
{
namespace
{
constexpr std::size_t kMemoryLimit = 512;
constexpr std::uintmax_t kFileLimit = 2 * 1024 * 1024;

std::string Sanitize(std::string_view text, std::size_t limit)
{
    std::string clean;
    clean.reserve((std::min)(text.size(), limit));
    for (unsigned char c : text)
    {
        if (clean.size() >= limit)
            break;
        // One physical line per event; preserve non-control UTF-8 bytes.
        clean.push_back(c < 0x20 || c == 0x7F ? ' ' : static_cast<char>(c));
    }
    return clean;
}

std::string Timestamp()
{
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    std::tm time{};
#ifdef _WIN32
    gmtime_s(&time, &seconds);
#else
    gmtime_r(&seconds, &time);
#endif
    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &time);
    return buffer;
}
} // namespace

const char* LogLevelName(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warning: return "WARN";
    case LogLevel::Error: return "ERROR";
    }
    return "UNKNOWN";
}

Diagnostics& Diagnostics::Get()
{
    static Diagnostics journal;
    return journal;
}

void Diagnostics::Initialize(const std::filesystem::path& directory)
{
    std::lock_guard lock(mutex_);
    file_ = directory / "m1core.log";
    events_.clear();
    initialized_ = !directory.empty();
}

bool Diagnostics::Write(LogLevel level, std::string_view source, std::string_view message)
{
    if (level < LogLevel::Debug || level > LogLevel::Error ||
        source.empty() || message.empty())
        return false;

    LogEvent event{Timestamp(), level, Sanitize(source, 80), Sanitize(message, 2048)};
    std::lock_guard lock(mutex_);
    if (!initialized_)
        return false;
    if (events_.size() == kMemoryLimit)
        events_.pop_front();
    events_.push_back(event);

    // If disk is unwritable, keep the event in memory without crashing.
    std::error_code ec;
    std::filesystem::create_directories(file_.parent_path(), ec);
    if (ec)
        return false;
    const auto size = std::filesystem::file_size(file_, ec);
    if (!ec && size >= kFileLimit)
    {
        auto previous = file_;
        previous += ".previous";
        std::filesystem::remove(previous, ec);
        ec.clear();
        std::filesystem::rename(file_, previous, ec);
        // Rotation failure is nonfatal: append to the original file.
    }
    std::ofstream output(file_, std::ios::binary | std::ios::app);
    if (!output)
        return false;
    output << event.timestamp << " [" << LogLevelName(level) << "] ["
           << event.source << "] " << event.message << '\n';
    output.flush();
    return static_cast<bool>(output);
}

std::vector<LogEvent> Diagnostics::Snapshot() const
{
    std::lock_guard lock(mutex_);
    return {events_.begin(), events_.end()};
}

void Diagnostics::ClearView()
{
    std::lock_guard lock(mutex_);
    events_.clear();
}
} // namespace m1core
