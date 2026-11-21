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
    ImGui::InputTextWithHint("##search", m1core::HasIcons() ? M1CORE_ICON_SEARCH " Search mods or settings..." : "Search mods or settings...",
                             search, sizeof(search));
    ImGui::SameLine();
    ImGui::TextDisabled("Auto-saved to settings.json");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Changes are written immediately; no Save button required.");
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
    bool closeRequested = false;
    // Use a compact header composed only of stock Dear ImGui widgets.
    // The native close glyph is hardcoded in ImGui::CloseButton and cannot
    // be swapped for Phosphor via a font. With NoTitleBar, a regular
    // Phosphor SmallButton provides a properly sized clickable close.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(7.0f, 6.0f));
    const bool visible = ImGui::Begin("M1CORE - MOD SETTINGS", nullptr,
                                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleVar();
    if (visible) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.95f, 0.96f, 0.98f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(9.0f, 4.0f));
        const bool headerVisible = ImGui::BeginChild(
            "##m1core-titlebar", ImVec2(0.0f, 29.0f),
            ImGuiChildFlags_None,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        if (headerVisible) {
            ImGui::PushStyleColor(ImGuiCol_Text, m1core::ui::TitleText());
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.95f, 0.96f, 0.98f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.83f, 0.86f, 0.90f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.79f, 0.83f, 1.0f));
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("M1CORE - MOD SETTINGS");
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 31.0f);
            closeRequested = ImGui::SmallButton(
                m1core::HasIcons() ? M1CORE_ICON_CLOSE "##m1core-close" : "X##m1core-close");
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("Close m1core");
            ImGui::PopStyleColor(4);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
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
    }
    ImGui::End();
    if (closeRequested) M1CoreRuntime::Get().GetOverlay().Toggle();
}
} // namespace m1core
