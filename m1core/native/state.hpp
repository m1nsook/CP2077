#pragma once
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <nlohmann/json.hpp>

namespace m1core {
// DLL-owned durable settings state. The CET-facing UI is deliberately a
// separate Lua frontend so CET (not this plugin) owns ImGui and input.
class StateStore {
public:
    StateStore() = default;
    void Configure(std::filesystem::path configPath);
    [[nodiscard]] std::string Snapshot() const;
    [[nodiscard]] bool Replace(const std::string& document);
    [[nodiscard]] std::int32_t Revision() const;
    [[nodiscard]] bool Ready() const;
    [[nodiscard]] std::string LastError() const;
private:
    [[nodiscard]] bool PersistLocked(const nlohmann::json& document);
    mutable std::mutex mutex_;
    nlohmann::json data_ = {
        {"schema", 1}, {"values", nlohmann::json::object()},
        {"profiles", nlohmann::json::object()}
    };
    std::filesystem::path path_;
    std::int32_t revision_ = 0;
    bool ready_ = false;
    bool frozen_ = false;
    std::string lastError_;
};
StateStore& Store();
} // namespace m1core
