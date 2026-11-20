#include "state.hpp"

#include <fstream>
#include <limits>
#include <system_error>
#ifdef _WIN32
#include <Windows.h>
#endif

namespace m1core {
namespace {
bool Valid(const nlohmann::json& document)
{
    return document.is_object() && document.value("schema", 0) == 1 &&
        document.contains("values") && document["values"].is_object() &&
        document.contains("profiles") && document["profiles"].is_object();
}
}

StateStore& Store()
{
    static StateStore store;
    return store;
}

void StateStore::Configure(std::filesystem::path configPath)
{
    std::lock_guard lock(mutex_);
    path_ = std::move(configPath);
    lastError_.clear();
    ready_ = false;
    frozen_ = false;
    revision_ = 0;
    data_ = {{"schema", 1}, {"values", nlohmann::json::object()},
             {"profiles", nlohmann::json::object()}};
    std::error_code ec;
    if (!std::filesystem::exists(path_, ec)) {
        if (ec) { lastError_ = ec.message(); frozen_ = true; return; }
        ready_ = true;
        return;
    }
    try {
        std::ifstream file(path_, std::ios::binary);
        if (!file.good()) throw std::runtime_error("Cannot read settings file");
        auto document = nlohmann::json::parse(file, nullptr, true);
        if (!Valid(document))
            throw std::runtime_error("Unsupported or malformed settings schema");
        data_ = std::move(document);
        ready_ = true;
    } catch (const std::exception& ex) {
        lastError_ = ex.what();
        // A malformed or unreadable file must not be silently overwritten.
        frozen_ = true;
    }
}

std::string StateStore::Snapshot() const
{
    std::lock_guard lock(mutex_);
    return data_.dump();
}

bool StateStore::PersistLocked(const nlohmann::json& document)
{
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);
    if (ec) { lastError_ = ec.message(); return false; }
    const auto temp = std::filesystem::path(path_.wstring() + L".tmp");
    {
        std::ofstream stream(temp, std::ios::binary | std::ios::trunc);
        if (!stream.good()) { lastError_ = "Failed opening temporary settings file"; return false; }
        stream << document.dump(2);
        stream.flush();
        if (!stream.good()) {
            lastError_ = "Failed writing temporary settings file";
            stream.close();
            std::filesystem::remove(temp, ec);
            return false;
        }
    }
#ifdef _WIN32
    const bool moved = MoveFileExW(temp.c_str(), path_.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::filesystem::rename(temp, path_, ec);
    const bool moved = !ec;
#endif
    if (!moved) {
        lastError_ = "Failed replacing settings.json (previous data preserved)";
        std::filesystem::remove(temp, ec);
        return false;
    }
    lastError_.clear();
    return true;
}

bool StateStore::Replace(const std::string& document)
{
    if (document.size() > 1024 * 1024) return false;
    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(document);
        if (!Valid(parsed)) return false;
    } catch (...) { return false; }
    std::lock_guard lock(mutex_);
    if (!ready_ || frozen_) return false;
    if (parsed == data_) return true;
    if (!PersistLocked(parsed)) return false;
    data_ = std::move(parsed);
    if (revision_ == std::numeric_limits<std::int32_t>::max())
        revision_ = 1;
    else
        ++revision_;
    return true;
}

std::int32_t StateStore::Revision() const
{
    std::lock_guard lock(mutex_);
    return revision_;
}

bool StateStore::Ready() const
{
    std::lock_guard lock(mutex_);
    return ready_ && !frozen_;
}

std::string StateStore::LastError() const
{
    std::lock_guard lock(mutex_);
    return lastError_;
}
} // namespace m1core
