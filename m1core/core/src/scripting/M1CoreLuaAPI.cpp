#include "M1CoreLuaAPI.hpp"
#include "registry.hpp"

#include <nlohmann/json.hpp>
#include <string>

namespace m1core {
void BindLuaRegistryAPI(sol::state& lua, sol::table& globals)
{
    auto api = lua.create_table();

    api.set_function("registerJson", [](const std::string& modId,
                                        const std::string& title,
                                        const std::string& schemaJson) -> bool {
        try {
            return Settings().Register(modId, title, nlohmann::json::parse(schemaJson));
        } catch (const nlohmann::json::exception&) {
            return false;
        }
    });
    api.set_function("getJson", [](const std::string& modId,
                                   const std::string& settingId) -> std::string {
        const auto value = Settings().Get(modId, settingId);
        return value ? value->dump() : "null";
    });
    api.set_function("setJson", [](const std::string& modId,
                                   const std::string& settingId,
                                   const std::string& valueJson) -> bool {
        try {
            return Settings().Set(modId, settingId, nlohmann::json::parse(valueJson));
        } catch (const nlohmann::json::exception&) {
            return false;
        }
    });
    api.set_function("resetSetting", [](const std::string& modId,
                                         const std::string& settingId) -> bool {
        return Settings().Reset(modId, settingId);
    });
    globals["m1core"] = api;
}
} // namespace m1core
