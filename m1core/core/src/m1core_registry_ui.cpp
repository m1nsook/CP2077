// m1core standard Dear ImGui settings frontend on yamashi's native M1CoreRuntime-derived backend.
// Preserves the original m1core registry and profile API; M1CoreRuntime owns keyboard,
// mouse, focus, D3D12 and hotkey lifecycle. No duplicate input hooks.
#include <stdafx.h>
#include "m1core_registry_ui.hpp"
#include "registry.hpp"
#include <M1CoreRuntime.h>
#include <overlay/widgets/Bindings.h>
#include <overlay/FontPalette.h>
#include <overlay/PhosphorIcons.h>
#include <overlay/M1CoreUIColors.h>
#include <scripting/LuaVM.h>

#include <filesystem>
#include <fstream>

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
std::string logSnapshot;
int logSource = 0;
bool logLoaded = false;

std::string ReadLogTail(const std::filesystem::path& file)
{
    // Read only the most recent 64 KiB and only when Logs is first opened
    // or Refresh is pressed. No per-frame disk I/O while ImGui renders.
    std::ifstream input(file, std::ios::binary | std::ios::ate);
    if (!input)
        return "Log file not created yet.";
    const auto size = input.tellg();
    if (size <= 0)
        return "Log is empty.";
    const auto length = static_cast<std::streamoff>(size);
    const std::streamoff start = std::max<std::streamoff>(0, length - 65536);
    input.seekg(start);
    std::string text(static_cast<size_t>(length - start), '\0');
    input.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<size_t>(input.gcount()));
    if (start != 0)
    {
        const auto firstLineEnd = text.find('\n');
        if (firstLineEnd != std::string::npos)
            text.erase(0, firstLineEnd + 1);
    }
    return text;
}

void DrawLogsPage()
{
    const char* sources[] = {"m1core.log", "scripting.log"};
    ImGui::SetNextItemWidth(190.0f);
    if (ImGui::Combo("Log", &logSource, sources, IM_ARRAYSIZE(sources)))
        logLoaded = false;
    ImGui::SameLine();
    if (ImGui::Button(m1core::HasIcons() ? M1CORE_ICON_REFRESH " Refresh" : "Refresh"))
        logLoaded = false;
    ImGui::SameLine();
    if (ImGui::Button("Reload scripts"))
    {
        // Defer to LuaVM::Update rather than unloading script contexts
        // during the current ImGui frame.
        M1CoreRuntime::Get().GetVM().ReloadAllMods();
        notice = "Script reload queued.";
        logLoaded = false;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Unload all Lua mods (onShutdown), then reload scripts (onInit).");
    if (!logLoaded)
    {
        logSnapshot = ReadLogTail(M1CoreRuntime::Get().GetPaths().CoreRoot() /
                                  (logSource == 0 ? L"m1core.log" : L"scripting.log"));
        logLoaded = true;
    }
    const auto error = Settings().LastError();
    if (!error.empty())
        ImGui::TextWrapped("Settings: %s", error.c_str());
    ImGui::Separator();

    if (ImGui::BeginChild("##m1core-log-output", ImVec2(0, 0),
                          ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_HorizontalScrollbar))
    {
        if (ImFont* font = m1core::LogFont())
            ImGui::PushFont(font);
        ImGui::TextUnformatted(logSnapshot.c_str());
        if (m1core::LogFont())
            ImGui::PopFont();
    }
    ImGui::EndChild();
}

// Settings changes are durable immediately. Report filesystem failures at
// the control that triggered them rather than silently discarding the input.
void ApplySetting(const ModView& mod, const SettingView& setting, const nlohmann::json& value)
{
    if (Settings().Set(mod.id, setting.id, value))
    {
        notice.clear();
        return;
    }
    const auto detail = Settings().LastError();
    notice = detail.empty() ? "Could not save setting; check Logs."
                            : "Could not save setting: " + detail;
}

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
            ApplySetting(mod, setting, value);
        break;
    }
    case SettingKind::Integer: {
        int value = setting.value.get<int>();
        if (ImGui::SliderInt("##value", &value,
            static_cast<int>(setting.minimum),
            static_cast<int>(setting.maximum)))
            ApplySetting(mod, setting, value);
        break;
    }
    case SettingKind::Float: {
        float value = setting.value.get<float>();
        if (ImGui::SliderFloat("##value", &value,
            static_cast<float>(setting.minimum),
            static_cast<float>(setting.maximum), "%.2f"))
            ApplySetting(mod, setting, value);
        break;
    }
    case SettingKind::Choice: {
        const auto current = setting.value.get<std::string>();
        if (ImGui::BeginCombo("##value", current.c_str())) {
            for (const auto& option : setting.choices) {
                const bool selected = option == current;
                if (ImGui::Selectable(option.c_str(), selected))
                    ApplySetting(mod, setting, option);
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        break;
    }
    }
    ImGui::TableSetColumnIndex(2);
    // Full-size action, exactly like the Logs page buttons. SmallButton()
    // removes vertical FramePadding and caused the tiny Reset controls.
    if (ImGui::Button("Reset", ImVec2(-FLT_MIN, 0.0f)))
    {
        if (!Settings().Reset(mod.id, setting.id))
        {
            const auto detail = Settings().LastError();
            notice = detail.empty() ? "Could not reset setting." : "Could not reset setting: " + detail;
        }
        else
            notice.clear();
    }
    ImGui::PopID();
}

void DrawSettingsFor(const ModView& mod)
{
    // Keep the mod title aligned with a normal-height action at the
    // right edge, instead of squeezing the button against the title.
    const char* resetLabel = confirmModReset ? "Confirm reset" : "Reset mod";
    const float actionWidth = ImGui::CalcTextSize(resetLabel).x +
                              ImGui::GetStyle().FramePadding.x * 2.0f;
    const float rightEdge = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s (%zu settings)", mod.title.c_str(), mod.settings.size());
    ImGui::SameLine();
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), rightEdge - actionWidth));
    if (ImGui::Button(resetLabel, ImVec2(actionWidth, 0.0f))) {
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
        // The global theme uses 10px cell padding. Narrow that only
        // for settings rows: full-height buttons shouldn't sit inside
        // unnecessarily tall empty table cells.
        const auto cellPadding = ImGui::GetStyle().CellPadding;
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(cellPadding.x, 5.0f));
        if (ImGui::BeginTable("settings", 3, flags)) {
            ImGui::TableSetupColumn("Setting", ImGuiTableColumnFlags_WidthStretch, 0.55f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.35f);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            for (const auto* setting : rows) DrawSetting(mod, *setting);
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
        ImGui::PopID();
    }
}

void DrawSettingsPage()
{
    ImGui::SetNextItemWidth(320);
    ImGui::InputTextWithHint("##search", m1core::HasIcons() ? M1CORE_ICON_SEARCH " Search mods or settings..." : "Search mods or settings...",
                             search, sizeof(search));
    // Autosave remains active, but the long status message does not
    // belong on the same line as the search control.
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Changes save automatically to m1core/settings.json.");
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
    if (ImGui::Button(m1core::HasIcons() ? M1CORE_ICON_EDIT " Change shortcut" : "Change shortcut")) {
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
    if (ImGui::Button(m1core::HasIcons() ? M1CORE_ICON_SAVE " Save profile" : "Save profile")) {
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
        if (ImGui::Button(m1core::HasIcons() ? M1CORE_ICON_REFRESH " Load selected" : "Load selected")) {
            notice = Settings().LoadProfile(selectedProfile) ?
                     "Profile loaded." : "Could not load profile.";
        }
        ImGui::SameLine();
        if (ImGui::Button(m1core::HasIcons() ? M1CORE_ICON_DELETE " Delete selected" : "Delete selected")) {
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
    // Stock Dear ImGui title bar + close button. No fake child title bar
    // and no hand-built X. Keep the user's light title colors and uppercase.
    // Compact only this title's vertical padding; controls inside the
    // window continue to use the user's normal FramePadding.
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
        ImVec2(ImGui::GetStyle().FramePadding.x, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.91f, 0.92f, 0.94f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.97f, 0.98f, 0.99f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ImVec4(0.91f, 0.92f, 0.94f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_Text, m1core::ui::TitleText());
    const bool visible = ImGui::Begin("M1CORE - MOD SETTINGS", &open,
                                     ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    if (visible) {
        if (!Store().Ready()) {
            const auto error = Store().LastError();
            ImGui::TextColored(m1core::ui::WarningText(),
                "Settings cannot be saved: %s", error.empty() ? "store not initialized" : error.c_str());
        } else {
            const auto error = Settings().LastError();
            if (!error.empty())
                ImGui::TextColored(m1core::ui::WarningText(), "Save failed: %s", error.c_str());
        }
        if (!notice.empty())
            ImGui::TextColored(m1core::ui::WarningText(), "%s", notice.c_str());
        // Standard tabs, with the same vertical frame padding used by
        // regular ImGui::Button() on the Logs page.
        const auto framePadding = ImGui::GetStyle().FramePadding;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                            ImVec2(framePadding.x + 3.0f, framePadding.y));
        if (ImGui::BeginTabBar("m1core-tabs")) {
            if (ImGui::BeginTabItem(m1core::HasIcons() ? M1CORE_ICON_SETTINGS "  Settings" : "Settings")) {
                DrawSettingsPage();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(m1core::HasIcons() ? M1CORE_ICON_KEYBINDS "  Keybinds" : "Keybinds")) {
                DrawKeybindsPage();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(m1core::HasIcons() ? M1CORE_ICON_PROFILES "  Profiles" : "Profiles")) {
                DrawProfilesPage();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(m1core::HasIcons() ? M1CORE_ICON_LOGS "  Logs" : "Logs")) {
                DrawLogsPage();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(m1core::HasIcons() ? M1CORE_ICON_ABOUT "  About" : "About")) {
                DrawAboutPage();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::PopStyleVar();
    }
    ImGui::End();
    if (!open) M1CoreRuntime::Get().GetOverlay().Toggle();
}
} // namespace m1core
