#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <nlohmann/json.hpp>

namespace m1core {
// DLL-owned durable settings state for the standalone RED4ext plugin.
// No external scripting frontend is required.
class StateStore {
public:
    StateStore() = default;
    void Configure(std::filesystem::path configPath);
    [[nodiscard]] std::string Snapshot() const;
    [[nodiscard]] bool Replace(const std::string& document);
    // Transactional read-modify-write across all native callers. The updater
    // runs under the store mutex; failures never alter the on-disk state.
    [[nodiscard]] bool Update(const std::function<void(nlohmann::json&)>& updater);
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
