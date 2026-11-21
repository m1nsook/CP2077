// m1core native RED4ext settings frontend using the user's AFTER.html layout.
// ImGui remains responsible for controls, keyboard focus, mouse and mod state;
// only the small title drag region uses the standard ImDrawList text primitive.
// No web view, CET runtime dependency or duplicate input hooks.
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
char keybindSearch[128]{};
std::string selectedKeybindMod = "m1core";
std::string activeProfile = "default";
enum class Page { Settings, Keybinds, Profiles, Logs, About };
Page currentPage = Page::Settings;

ImVec4 Color(int r, int g, int b, float a = 1.0f)
{
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a);
}
constexpr float kNavWidth = 240.0f;
constexpr float kTitleHeight = 48.0f;
constexpr float kPageHeaderHeight = 74.0f;


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
    if (auto* font = m1core::KarlaMedium()) ImGui::PushFont(font);
    ImGui::TextUnformatted(setting.label.c_str());
    if (m1core::KarlaMedium()) ImGui::PopFont();
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
    if (auto* font = m1core::KarlaSemiBold()) ImGui::PushFont(font);
    ImGui::Text("%s (%zu settings)", mod.title.c_str(), mod.settings.size());
    if (m1core::KarlaSemiBold()) ImGui::PopFont();
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
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##settings-search",
        m1core::HasIcons() ? M1CORE_ICON_SEARCH "  Search mods or settings..." :
                             "Search mods or settings...", search, sizeof(search));
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Values save automatically to m1core/settings.json.");
    ImGui::Spacing();

    const auto mods = Settings().Snapshot();
    if (selectedMod.empty() && !mods.empty())
        selectedMod = mods.front().id;
    const float remainingWidth = ImGui::GetContentRegionAvail().x;
    const float modListWidth = std::min(240.0f, std::max(170.0f, remainingWidth * 0.29f));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(30,31,35));
    if (ImGui::BeginChild("##settings-mod-list", ImVec2(modListWidth,0),
                          ImGuiChildFlags_Borders))
    {
        if (auto* font = m1core::KarlaSemiBold()) ImGui::PushFont(font);
        ImGui::TextDisabled("REGISTERED MODS (%zu)", mods.size());
        if (m1core::KarlaSemiBold()) ImGui::PopFont();
        ImGui::Separator();
        bool anyVisible = false;
        for (const auto& mod : mods)
        {
            if (!Matches(mod, search)) continue;
            anyVisible = true;
            ImGui::PushID(mod.id.c_str());
            if (ImGui::Selectable(mod.title.c_str(), selectedMod == mod.id))
            {
                selectedMod = mod.id;
                confirmModReset = false;
            }
            ImGui::PopID();
        }
        if (!anyVisible)
            ImGui::TextDisabled("No matching mods");
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, 12.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(30,31,35));
    if (ImGui::BeginChild("##settings-detail", ImVec2(0,0),
                          ImGuiChildFlags_Borders))
    {
        const auto selected = std::find_if(mods.begin(), mods.end(),
            [](const ModView& m) { return m.id == selectedMod; });
        if (selected != mods.end() && Matches(*selected, search))
            DrawSettingsFor(*selected);
        else
            ImGui::TextDisabled("Select a mod to edit its settings.");
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}



void DrawKeybindsPage()
{
    auto& runtime = M1CoreRuntime::Get();
    const auto& bindings = runtime.GetBindings();
    const auto& registered = runtime.GetVM().GetAllBinds();

    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##keybind-search",
        m1core::HasIcons() ? M1CORE_ICON_SEARCH "  Search mods or keybinds..." :
                             "Search mods or keybinds...", keybindSearch, sizeof(keybindSearch));
    ImGui::Spacing();

    std::vector<std::string> visibleMods;
    if (ContainsIgnoreCase("m1core", keybindSearch) ||
        ContainsIgnoreCase("Toggle Menu", keybindSearch))
        visibleMods.emplace_back("m1core");
    for (const auto& [id, modBinds] : registered)
    {
        bool matched = ContainsIgnoreCase(id, keybindSearch);
        for (const auto& bind : modBinds.get())
            matched = matched || ContainsIgnoreCase(bind.DisplayName, keybindSearch);
        if (matched && !modBinds.get().empty() && id != "m1core")
            visibleMods.push_back(id);
    }
    std::sort(visibleMods.begin(), visibleMods.end());
    const float avail = ImGui::GetContentRegionAvail().x;
    const float panelWidth = std::min(240.0f, std::max(170.0f, avail * 0.29f));
    if (selectedKeybindMod.empty() || std::find(visibleMods.begin(), visibleMods.end(),
                                                selectedKeybindMod) == visibleMods.end())
        selectedKeybindMod = visibleMods.empty() ? "" : visibleMods.front();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(30,31,35));
    if (ImGui::BeginChild("##keybind-mod-list", ImVec2(panelWidth,0),
                          ImGuiChildFlags_Borders))
    {
        if (auto* font = m1core::KarlaSemiBold()) ImGui::PushFont(font);
        ImGui::TextDisabled("REGISTERED MODS (%zu)", visibleMods.size());
        if (m1core::KarlaSemiBold()) ImGui::PopFont();
        ImGui::Separator();
        for (const auto& modId : visibleMods)
        {
            if (ImGui::Selectable(modId == "m1core" ? "M1CORE" : modId.c_str(),
                                  selectedKeybindMod == modId))
                selectedKeybindMod = modId;
        }
        if (visibleMods.empty()) ImGui::TextDisabled("No matching mods");
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine(0,12);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(30,31,35));
    if (ImGui::BeginChild("##keybind-detail", ImVec2(0,0),
                          ImGuiChildFlags_Borders))
    {
        if (selectedKeybindMod.empty())
            ImGui::TextDisabled("No keybinds match your search.");
        else
        {
            if (auto* font = m1core::KarlaSemiBold()) ImGui::PushFont(font);
            ImGui::TextUnformatted(selectedKeybindMod == "m1core" ?
                                   "M1CORE" : selectedKeybindMod.c_str());
            if (m1core::KarlaSemiBold()) ImGui::PopFont();
            ImGui::SameLine();
            const float buttonWidth = 178.0f;
            const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),right-buttonWidth));
            if (ImGui::Button("Configure keybinds", ImVec2(buttonWidth,0)))
                runtime.GetOverlay().GetBindings().Toggle();
            ImGui::Separator();
            const ImGuiTableFlags flags =
                ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_SizingStretchProp;
            if (ImGui::BeginTable("##keybind-actions",2,flags))
            {
                ImGui::TableSetupColumn("ACTION",ImGuiTableColumnFlags_WidthStretch,0.70f);
                ImGui::TableSetupColumn("KEYBIND",ImGuiTableColumnFlags_WidthStretch,0.30f);
                ImGui::TableHeadersRow();
                if (selectedKeybindMod == "m1core")
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Toggle Menu");
                    ImGui::TextDisabled("Opens and closes the M1CORE overlay");
                    ImGui::TableSetColumnIndex(1);
                    const auto shortcut = bindings.GetBindString(Bindings::GetOverlayToggleModBind());
                    if (ImGui::Button(shortcut.c_str(),ImVec2(-FLT_MIN,0)))
                        runtime.GetOverlay().GetBindings().Toggle();
                }
                else
                {
                    auto it = registered.find(selectedKeybindMod);
                    if (it != registered.end())
                    {
                        for (const auto& bind : it->second.get())
                        {
                            if (!ContainsIgnoreCase(selectedKeybindMod,keybindSearch) &&
                                !ContainsIgnoreCase(bind.DisplayName,keybindSearch))
                                continue;
                            ImGui::PushID(bind.ID.c_str());
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextUnformatted(bind.DisplayName.c_str());
                            if (bind.HasSimpleDescription())
                                ImGui::TextDisabled("%s",std::get<std::string>(bind.Description).c_str());
                            ImGui::TableSetColumnIndex(1);
                            const auto key = bindings.GetBindString({selectedKeybindMod,bind.ID});
                            if (ImGui::Button(key.c_str(),ImVec2(-FLT_MIN,0)))
                                runtime.GetOverlay().GetBindings().Toggle();
                            ImGui::PopID();
                        }
                    }
                }
                ImGui::EndTable();
            }
            ImGui::Spacing();
            ImGui::TextDisabled("Select a keybind to open the native recorder. Esc cancels.");
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}


void DrawProfilesPage()
{
    if (auto* font = m1core::KarlaSemiBold()) ImGui::PushFont(font);
    ImGui::TextUnformatted("Profiles");
    if (m1core::KarlaSemiBold()) ImGui::PopFont();
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
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(30,31,35));
    if (ImGui::BeginChild("##profile-list", ImVec2(0,220),ImGuiChildFlags_Borders)) {
        for (const auto& name : Settings().Profiles()) {
            if (ImGui::Selectable(name.c_str(), selectedProfile == name))
                selectedProfile = name;
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    if (!selectedProfile.empty()) {
        if (ImGui::Button(m1core::HasIcons() ? M1CORE_ICON_REFRESH " Load selected" : "Load selected")) {
            if (Settings().LoadProfile(selectedProfile)) {
                activeProfile = selectedProfile;
                notice = "Profile loaded.";
            } else notice = "Could not load profile.";
        }
        ImGui::SameLine();
        if (ImGui::Button(m1core::HasIcons() ? M1CORE_ICON_DELETE " Delete selected" : "Delete selected")) {
            if (Settings().DeleteProfile(selectedProfile)) {
                if (activeProfile == selectedProfile) activeProfile = "default";
                selectedProfile.clear();
                notice = "Profile deleted.";
            } else notice = "Could not delete profile.";
        }
    }
}


void DrawAboutPage()
{
    if (auto* font = m1core::KarlaBold()) ImGui::PushFont(font);
    ImGui::TextUnformatted("M1CORE");
    if (m1core::KarlaBold()) ImGui::PopFont();
    ImGui::Separator();
    ImGui::TextWrapped("Native RED4ext mod settings framework.");
    ImGui::TextWrapped("Settings, keybindings and profiles with Dear ImGui.");
    ImGui::TextWrapped("Native RED4ext DLL. No additional overlay installation required.");
}

const char* PageTitle(Page p)
{
    switch (p) {
    case Page::Settings: return "Settings";
    case Page::Keybinds: return "Keybinds";
    case Page::Profiles: return "Profiles";
    case Page::Logs: return "System Logs";
    case Page::About: return "About M1CORE";
    }
    return "Settings";
}

const char* PageDescription(Page p)
{
    switch (p) {
    case Page::Settings: return "Manage mod settings and configurations.";
    case Page::Keybinds: return "Configure shortcuts and hotkeys for each mod.";
    case Page::Profiles: return "Save and load your settings configurations.";
    case Page::Logs: return "View real-time output from m1core.";
    case Page::About: return "Information about the framework.";
    }
    return "";
}

void SidebarNav(const char* icon, const char* title, Page target)
{
    ImGui::PushID(static_cast<int>(target));
    const bool active = currentPage == target;
    ImGui::PushStyleColor(ImGuiCol_Button, active ? Color(38,56,86) : Color(24,25,28));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,Color(42,44,49));
    ImGui::PushStyleColor(ImGuiCol_Text,active ? Color(96,165,250) : Color(148,163,184));
    const std::string label = (m1core::HasIcons() ? std::string(icon)+"  " : "") + title;
    if (ImGui::Button(label.c_str(),ImVec2(-FLT_MIN,36.0f)))
        currentPage = target;
    ImGui::PopStyleColor(3);
    ImGui::PopID();
}

void DrawSidebar()
{
    if (auto* font = m1core::KarlaSemiBold()) ImGui::PushFont(font);
    ImGui::TextDisabled("CONFIGURATION");
    if (m1core::KarlaSemiBold()) ImGui::PopFont();
    SidebarNav(M1CORE_ICON_SLIDERS_HORIZONTAL,"Settings",Page::Settings);
    SidebarNav(M1CORE_ICON_KEYBINDS,"Keybinds",Page::Keybinds);
    SidebarNav(M1CORE_ICON_USERS_THREE,"Profiles",Page::Profiles);
    ImGui::Dummy(ImVec2(0,18));
    if (auto* font = m1core::KarlaSemiBold()) ImGui::PushFont(font);
    ImGui::TextDisabled("SYSTEM");
    if (m1core::KarlaSemiBold()) ImGui::PopFont();
    SidebarNav(M1CORE_ICON_TERMINAL,"Logs",Page::Logs);
    SidebarNav(M1CORE_ICON_ABOUT,"About",Page::About);

    // The profile selector follows the HTML layout by staying at the
    // bottom of the sidebar. It navigates to Profiles; it does not silently
    // load a profile or overwrite persistent settings.
    const float desiredY = ImGui::GetWindowHeight()-104.0f;
    if (ImGui::GetCursorPosY() < desiredY)
        ImGui::SetCursorPosY(desiredY);
    ImGui::Separator();
    ImGui::TextDisabled("ACTIVE PROFILE");
    ImGui::PushStyleColor(ImGuiCol_Button,Color(30,31,35));
    std::string profileLabel = (m1core::HasIcons() ? std::string(M1CORE_ICON_CHECK_CIRCLE) + "  " : "") +
                               activeProfile + "  " +
        (m1core::HasIcons() ? M1CORE_ICON_CARET : "v");
    if (ImGui::Button(profileLabel.c_str(),ImVec2(-FLT_MIN,32)))
        currentPage = Page::Profiles;
    ImGui::PopStyleColor();
}

void DrawPageHeader()
{
    if (auto* font = m1core::KarlaBold()) ImGui::PushFont(font);
    ImGui::TextUnformatted(PageTitle(currentPage));
    if (m1core::KarlaBold()) ImGui::PopFont();
    ImGui::TextDisabled("%s",PageDescription(currentPage));
}

void DrawPageBody()
{
    if (!Store().Ready())
    {
        const auto error = Store().LastError();
        ImGui::TextColored(m1core::ui::WarningText(),
            "Settings unavailable: %s",error.empty() ? "Store not initialized" : error.c_str());
    }
    else if (const auto error = Settings().LastError(); !error.empty())
        ImGui::TextColored(m1core::ui::WarningText(),"Save failed: %s",error.c_str());
    if (!notice.empty())
        ImGui::TextColored(m1core::ui::WarningText(),"%s",notice.c_str());

    switch (currentPage)
    {
    case Page::Settings: DrawSettingsPage(); break;
    case Page::Keybinds: DrawKeybindsPage(); break;
    case Page::Profiles: DrawProfilesPage(); break;
    case Page::Logs: DrawLogsPage(); break;
    case Page::About: DrawAboutPage(); break;
    }
}

} // namespace

void DrawNativeRegistryUI()
{
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(),ImGuiCond_FirstUseEver,ImVec2(0.5f,0.5f));
    ImGui::SetNextWindowSize(ImVec2(1150,680),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(760,490),viewport->Size);

    // All AFTER.html-inspired colors/spacing are scoped to this one
    // m1core window. Do not alter the user's native input layer,
    // other ImGui windows or third-party mod windows.
    int colors = 0;
    const auto color = [&](ImGuiCol idx, ImVec4 value) {
        ImGui::PushStyleColor(idx,value);
        ++colors;
    };
    color(ImGuiCol_WindowBg,Color(17,18,20));
    color(ImGuiCol_ChildBg,Color(17,18,20));
    color(ImGuiCol_Text,Color(248,250,252));
    color(ImGuiCol_TextDisabled,Color(148,163,184));
    color(ImGuiCol_Border,Color(42,44,49));
    color(ImGuiCol_Separator,Color(42,44,49));
    color(ImGuiCol_Button,Color(42,44,49));
    color(ImGuiCol_ButtonHovered,Color(63,66,74));
    color(ImGuiCol_ButtonActive,Color(38,56,86));
    color(ImGuiCol_Header,Color(38,56,86));
    color(ImGuiCol_HeaderHovered,Color(42,44,49));
    color(ImGuiCol_HeaderActive,Color(38,56,86));
    color(ImGuiCol_FrameBg,Color(12,13,15));
    color(ImGuiCol_FrameBgHovered,Color(42,44,49));
    color(ImGuiCol_FrameBgActive,Color(42,44,49));
    color(ImGuiCol_CheckMark,Color(59,130,246));
    color(ImGuiCol_SliderGrab,Color(59,130,246));
    color(ImGuiCol_SliderGrabActive,Color(96,165,250));
    color(ImGuiCol_PopupBg,Color(30,31,35));
    color(ImGuiCol_TableHeaderBg,Color(30,31,35));
    color(ImGuiCol_TableRowBgAlt,Color(42,44,49,0.18f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(10,7));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(9,9));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,ImVec2(8,6));

    bool hideOverlay = false;
    const bool visible = ImGui::Begin("M1CORE##native-overlay",nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
    if (visible)
    {
        const ImVec2 windowPos = ImGui::GetWindowPos();
        bool draggingTitle = false;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(8,8));
        if (ImGui::BeginChild("##app-titlebar",ImVec2(0,kTitleHeight),
                ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar))
        {
            const float dragWidth = std::max(100.0f,ImGui::GetContentRegionAvail().x-84.0f);
            ImGui::InvisibleButton("##move-m1core",ImVec2(dragWidth,32.0f));
            draggingTitle = ImGui::IsItemActive() &&
                            ImGui::IsMouseDragging(ImGuiMouseButton_Left);
            const ImVec2 top = ImGui::GetItemRectMin();
            auto* draw = ImGui::GetWindowDrawList();
            if (m1core::HasIcons())
                draw->AddText(ImVec2(top.x+7,top.y+8),IM_COL32(59,130,246,255),M1CORE_ICON_CUBE);
            draw->AddText(ImVec2(top.x+(m1core::HasIcons()?34.0f:7.0f),top.y+8),
                          IM_COL32(248,250,252,255),"M1CORE");
            ImGui::SameLine(0,5);
            if (ImGui::Button(m1core::HasIcons() ? M1CORE_ICON_MINUS "##minimize" : "-##minimize",
                              ImVec2(32,32)))
                hideOverlay = true;
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("Hide m1core overlay");
            ImGui::SameLine(0,4);
            if (ImGui::Button(m1core::HasIcons() ? M1CORE_ICON_CLOSE "##close" : "X##close",
                              ImVec2(32,32)))
                hideOverlay = true;
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("Close m1core overlay");
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        if (draggingTitle)
        {
            const auto delta = ImGui::GetIO().MouseDelta;
            ImGui::SetWindowPos(ImVec2(windowPos.x+delta.x,windowPos.y+delta.y));
        }
        ImGui::Separator();

        // Two children form the 240px navigation and flexible content
        // columns. Each page gets standard native widgets and scrollbars.
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(12,16));
        ImGui::PushStyleColor(ImGuiCol_ChildBg,Color(24,25,28));
        if (ImGui::BeginChild("##app-sidebar",ImVec2(kNavWidth,0),
                ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar))
            DrawSidebar();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::SameLine(0,0);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
        if (ImGui::BeginChild("##app-content",ImVec2(0,0),
                ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(24,14));
            if (ImGui::BeginChild("##app-page-header",ImVec2(0,kPageHeaderHeight),
                    ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar))
                DrawPageHeader();
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::Separator();
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(24,18));
            if (ImGui::BeginChild("##app-page-body",ImVec2(0,0),
                    ImGuiChildFlags_None,ImGuiWindowFlags_None))
                DrawPageBody();
            ImGui::EndChild();
            ImGui::PopStyleVar();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleVar();
    }
    ImGui::End();
    ImGui::PopStyleVar(7);
    ImGui::PopStyleColor(colors);
    if (hideOverlay)
        M1CoreRuntime::Get().GetOverlay().Toggle();
}
} // namespace m1core
