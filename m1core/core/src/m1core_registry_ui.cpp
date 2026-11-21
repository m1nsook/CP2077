// m1core standard Dear ImGui settings frontend on yamashi's native M1CoreRuntime-derived backend.
// Preserves the original m1core registry and profile API; M1CoreRuntime owns keyboard,
// mouse, focus, D3D12 and hotkey lifecycle. No duplicate input hooks.
#include <stdafx.h>
#include "m1core_registry_ui.hpp"
#include "registry.hpp"
#include <M1CoreRuntime.h>
#include <overlay/widgets/Bindings.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace m1core {
namespace {
std::string selectedMod;
std::string selectedProfile;
char search[128]{};
char profileName[96] = "default";
bool confirmModReset = false;
std::string notice;

bool ContainsIgnoreCase(std::string_view value, std::string_view query)
{
    if (query.empty()) return true;
    return std::search(value.begin(), value.end(), query.begin(), query.end(),
        [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) ==
                   std::tolower(static_cast<unsigned char>(b));
        }) != value.end();
}

bool Matches(const ModView& mod, std::string_view query)
{
    if (ContainsIgnoreCase(mod.title, query) ||
        ContainsIgnoreCase(mod.id, query)) return true;
    return std::any_of(mod.settings.begin(), mod.settings.end(),
        [&](const SettingView& s) {
            return ContainsIgnoreCase(s.label, query) ||
                   ContainsIgnoreCase(s.description, query) ||
                   ContainsIgnoreCase(s.group, query);
        });
}

void DrawSetting(const ModView& mod, const SettingView& setting)
{
    ImGui::PushID(setting.id.c_str());
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(setting.label.c_str());
    if (!setting.description.empty() &&
        ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", setting.description.c_str());

    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
    switch (setting.kind) {
    case SettingKind::Boolean: {
        bool value = setting.value.get<bool>();
        if (ImGui::Checkbox("##value", &value))
            Settings().Set(mod.id, setting.id, value);
        break;
    }
    case SettingKind::Integer: {
        int value = setting.value.get<int>();
        if (ImGui::SliderInt("##value", &value,
            static_cast<int>(setting.minimum),
            static_cast<int>(setting.maximum)))
            Settings().Set(mod.id, setting.id, value);
        break;
    }
    case SettingKind::Float: {
        float value = setting.value.get<float>();
        if (ImGui::SliderFloat("##value", &value,
            static_cast<float>(setting.minimum),
            static_cast<float>(setting.maximum), "%.2f"))
            Settings().Set(mod.id, setting.id, value);
        break;
    }
    case SettingKind::Choice: {
        const auto current = setting.value.get<std::string>();
        if (ImGui::BeginCombo("##value", current.c_str())) {
            for (const auto& option : setting.choices) {
                const bool selected = option == current;
                if (ImGui::Selectable(option.c_str(), selected))
                    Settings().Set(mod.id, setting.id, option);
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        break;
    }
    }
    ImGui::TableSetColumnIndex(2);
    if (ImGui::SmallButton("Reset"))
        Settings().Reset(mod.id, setting.id);
    ImGui::PopID();
}

void DrawSettingsFor(const ModView& mod)
{
    ImGui::Text("%s (%zu settings)", mod.title.c_str(), mod.settings.size());
    ImGui::SameLine();
    if (ImGui::Button(confirmModReset ? "Confirm reset" : "Reset mod")) {
        if (confirmModReset) {
            if (!Settings().ResetMod(mod.id))
                notice = "Could not reset settings.";
            confirmModReset = false;
        } else {
            confirmModReset = true;
        }
    }
    ImGui::Separator();

    std::map<std::string, std::vector<const SettingView*>> groups;
    for (const auto& setting : mod.settings) {
        if (!ContainsIgnoreCase(mod.title, search) &&
            !ContainsIgnoreCase(mod.id, search) &&
            !ContainsIgnoreCase(setting.label, search) &&
            !ContainsIgnoreCase(setting.group, search) &&
            !ContainsIgnoreCase(setting.description, search))
            continue;
        groups[setting.group].push_back(&setting);
    }
    if (groups.empty()) {
        ImGui::TextDisabled("No matching settings.");
        return;
    }

    for (const auto& [group, rows] : groups) {
        ImGui::SeparatorText(group.c_str());
        ImGui::PushID(group.c_str());
        const ImGuiTableFlags flags =
            ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_SizingStretchProp;
        if (ImGui::BeginTable("settings", 3, flags)) {
            ImGui::TableSetupColumn("Setting", ImGuiTableColumnFlags_WidthStretch, 0.55f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.35f);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 65.0f);
            for (const auto* setting : rows) DrawSetting(mod, *setting);
            ImGui::EndTable();
        }
        ImGui::PopID();
    }
}

void DrawSettingsPage()
{
    ImGui::SetNextItemWidth(320);
    ImGui::InputTextWithHint("##search", "Search mods or settings...",
                             search, sizeof(search));
    ImGui::Separator();

    const auto mods = Settings().Snapshot();
    if (selectedMod.empty() && !mods.empty()) selectedMod = mods.front().id;

    const float available = ImGui::GetContentRegionAvail().x;
    const float sidebarWidth = available >= 600.0f ? 220.0f :
                               std::max(140.0f, available * 0.30f);
    if (ImGui::BeginChild("Registered mods", ImVec2(sidebarWidth, 0),
                          ImGuiChildFlags_Borders)) {
        ImGui::TextDisabled("Registered mods (%zu)", mods.size());
        ImGui::Separator();
        for (const auto& mod : mods) {
            if (!Matches(mod, search)) continue;
            if (ImGui::Selectable(mod.title.c_str(), selectedMod == mod.id)) {
                selectedMod = mod.id;
                confirmModReset = false;
            }
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();
    if (ImGui::BeginChild("Mod settings", ImVec2(0, 0),
                          ImGuiChildFlags_Borders)) {
        const auto found = std::find_if(mods.begin(), mods.end(),
            [](const ModView& mod) { return mod.id == selectedMod; });
        if (found != mods.end()) DrawSettingsFor(*found);
        else ImGui::TextDisabled("Select a mod on the left.");
    }
    ImGui::EndChild();
}


void DrawKeybindsPage()
{
    ImGui::TextUnformatted("m1core menu shortcut");
    const auto& bindings = M1CoreRuntime::Get().GetBindings();
    const auto key = bindings.GetBindString(Bindings::GetOverlayToggleModBind());
    ImGui::Text("Current: %s", key.empty() ? "Not assigned" : key.c_str());
    if (ImGui::Button("Change shortcut")) {
        // Use the donated key capture engine instead of introducing another
        // raw/WM_KEYDOWN listener that can regress game input.
        M1CoreRuntime::Get().GetOverlay().GetBindings().Toggle();
    }
    ImGui::Separator();
    ImGui::TextWrapped("The same shortcut opens and closes m1core. "
                       "All input capture is handled by the native game window backend.");
}

void DrawProfilesPage()
{
    ImGui::TextUnformatted("Profiles");
    ImGui::SetNextItemWidth(240);
    ImGui::InputTextWithHint("##profile", "Profile name",
                             profileName, sizeof(profileName));
    ImGui::SameLine();
    if (ImGui::Button("Save profile")) {
        if (Settings().SaveProfile(profileName)) {
            selectedProfile = profileName;
            notice = "Profile saved.";
        } else notice = "Could not save profile.";
    }
    ImGui::Separator();
    for (const auto& name : Settings().Profiles()) {
        if (ImGui::Selectable(name.c_str(), selectedProfile == name))
            selectedProfile = name;
    }
    if (!selectedProfile.empty()) {
        if (ImGui::Button("Load selected")) {
            notice = Settings().LoadProfile(selectedProfile) ?
                     "Profile loaded." : "Could not load profile.";
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete selected")) {
            if (Settings().DeleteProfile(selectedProfile)) {
                selectedProfile.clear();
                notice = "Profile deleted.";
            } else notice = "Could not delete profile.";
        }
    }
}


void DrawAboutPage()
{
    ImGui::TextUnformatted("m1core");
    ImGui::Separator();
    ImGui::TextWrapped("Native RED4ext mod settings framework.");
    ImGui::TextWrapped("Settings, keybindings and profiles with Dear ImGui.");
    ImGui::TextWrapped("Native RED4ext DLL. No additional overlay installation required.");
}
} // namespace

void DrawNativeRegistryUI()
{
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver,
                            ImVec2(0.5f,0.5f));
    ImGui::SetNextWindowSize(ImVec2(940,600), ImGuiCond_FirstUseEver);
    bool open = true;
    if (ImGui::Begin("m1core - Mod Settings", &open,
                     ImGuiWindowFlags_NoCollapse)) {
        if (!notice.empty())
            ImGui::TextDisabled("%s",notice.c_str());
        if (ImGui::BeginTabBar("m1core-tabs")) {
            if (ImGui::BeginTabItem("Settings")) {
                DrawSettingsPage();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Keybinds")) {
                DrawKeybindsPage();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Profiles")) {
                DrawProfilesPage();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Logs")) {
                const auto error=Settings().LastError();
                if (!error.empty()) ImGui::TextWrapped("%s",error.c_str());
                else ImGui::TextDisabled("No settings errors.");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("About")) {
                DrawAboutPage();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    if (!open) M1CoreRuntime::Get().GetOverlay().Toggle();
}
} // namespace m1core
