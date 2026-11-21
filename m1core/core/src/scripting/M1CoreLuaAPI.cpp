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
    // These are the same operations exposed by the native REDscript/RTTI API.
    // Keep profile persistence in the single native settings registry.
    api.set_function("resetMod", [](const std::string& modId) -> bool {
        return Settings().ResetMod(modId);
    });
    api.set_function("saveProfile", [](const std::string& name) -> bool {
        return Settings().SaveProfile(name);
    });
    api.set_function("loadProfile", [](const std::string& name) -> bool {
        return Settings().LoadProfile(name);
    });
    api.set_function("deleteProfile", [](const std::string& name) -> bool {
        return Settings().DeleteProfile(name);
    });
    api.set_function("profiles", [state = lua.lua_state()]() -> sol::table {
        sol::state_view vm(state);
        sol::table names = vm.create_table();
        std::size_t index = 1;
        for (const auto& name : Settings().Profiles())
            names[index++] = name;
        return names;
    });
    api.set_function("lastError", []() -> std::string {
        return Settings().LastError();
    });
    globals["m1core"] = api;
}
} // namespace m1core
