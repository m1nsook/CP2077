#include "ui.hpp"
#include "settings.hpp"
#include "diagnostics.hpp"
#include "ui_theme.hpp"
#include "ui_components.hpp"
#include "ui_icons.hpp"
#include "workspace_layout.hpp"
#include "cursor.hpp"
#include "keybind_conflicts.hpp"

#include <RED4ext/CName.hpp>
#include <RED4ext/CNamePool.hpp>
#include <imgui.h>
#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <map>
#include <vector>
#include <optional>
#include <string>
#include <string_view>

namespace m1core {
namespace {
enum class Page { Mods, Keybinds, Profiles, Logs, About };

struct BindCapture {
    bool active = false;
    Id mod = 0;
    Id setting = 0;
    std::array<bool, 256> wasDown{};
};

BindCapture g_capture;
std::atomic_bool g_bindCaptureActive{false};
bool g_dirty = false;
char g_newProfileName[80]{};
char g_renameProfileName[80]{};
std::string g_selectedProfile;
std::string g_profileMessage;
bool g_profileError = false;
bool g_confirmOverwrite = false;
bool g_confirmDelete = false;
bool g_confirmGroupReset = false;
Id g_pendingGroupMod = 0;
Id g_pendingGroup = 0;
Id g_pendingModReset = 0;

std::string Label(Id id)
{
    const auto* text = RED4ext::CNamePool::Get(RED4ext::CName(id));
    if (text && *text)
        return text;
    char fallback[25]{};
    std::snprintf(fallback, sizeof(fallback), "0x%016llx", static_cast<unsigned long long>(id));
    return fallback;
}

bool Matches(std::string_view text, std::string_view query)
{
    if (query.empty()) return true;
    return std::search(text.begin(), text.end(), query.begin(), query.end(),
        [](unsigned char lhs, unsigned char rhs) {
            return std::tolower(lhs) == std::tolower(rhs);
        }) != text.end();
}

bool MatchesSetting(const SettingSnapshot& setting, std::string_view query)
{
    return Matches(Label(setting.id), query) ||
           Matches(setting.displayName, query) ||
           Matches(setting.description, query) ||
           (setting.group != 0 && Matches(Label(setting.group), query)) ||
           (setting.group == 0 && Matches("General", query));
}

std::string KeyLabel(int key)
{
    if (key <= 0) return "Unbound";
    if (key >= 'A' && key <= 'Z') return std::string(1, static_cast<char>(key));
    if (key >= '0' && key <= '9') return std::string(1, static_cast<char>(key));
    if (key >= VK_F1 && key <= VK_F24) return "F" + std::to_string(key - VK_F1 + 1);
    if (key == VK_SPACE) return "Space";
    if (key == VK_SHIFT) return "Shift";
    if (key == VK_CONTROL) return "Ctrl";
    if (key == VK_MENU) return "Alt";

    char label[80]{};
    const auto scan = MapVirtualKeyA(static_cast<UINT>(key), MAPVK_VK_TO_VSC);
    if (scan && GetKeyNameTextA(static_cast<LONG>(scan << 16), label, sizeof(label)) > 0)
        return label;
    return "VK " + std::to_string(key);
}

void StartCapture(Id mod, Id id)
{
    g_capture.active = true;
    g_bindCaptureActive.store(true);
    g_capture.mod = mod;
    g_capture.setting = id;
    for (int key = 0; key < 256; ++key)
        g_capture.wasDown[key] = (GetAsyncKeyState(key) & 0x8000) != 0;
}

bool PollCapture(Registry& registry)
{
    if (!g_capture.active) return false;
    for (int key = 7; key < 256; ++key) {
        const bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
        const bool rising = down && !g_capture.wasDown[key];
        g_capture.wasDown[key] = down;
        if (!rising) continue;
        // A key is chosen by the player, never reserved at compile time.
        // Escape cancels ordinary mod key capture, but can be selected as
        // m1core's own menu key when deliberately configuring that control.
        const bool coreMenuCapture =
            g_capture.mod == RED4ext::CNamePool::Add("m1core").hash &&
            g_capture.setting == RED4ext::CNamePool::Add("Overlay Toggle").hash;
        if (key == VK_ESCAPE && !coreMenuCapture) {
            g_capture.active = false;
            g_bindCaptureActive.store(false);
            return false;
        }
        // Windows/meta and standalone modifiers interfere with desktop
        // focus or the emergency chord. All regular VK keys are selectable.
        if (key == VK_LWIN || key == VK_RWIN || key == VK_CONTROL ||
            key == VK_LCONTROL || key == VK_RCONTROL ||
            key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT ||
            key == VK_MENU || key == VK_LMENU || key == VK_RMENU)
            continue;
        const bool saved = registry.SetKeybind(g_capture.mod, g_capture.setting, key);
        g_capture.active = false;
        g_bindCaptureActive.store(false);
        if (saved && coreMenuCapture)
            return !registry.Save(); // The first binding persists immediately; only failed saves are dirty.
        return saved;
    }
    return false;
}

bool DrawSetting(Registry& registry, Id mod, const SettingSnapshot& setting)
{
    const auto stableId = std::to_string(mod) + ":" + std::to_string(setting.id);
    ImGui::PushID(stableId.c_str());
    ImGui::TableNextRow(ImGuiTableRowFlags_None, 64.0F);
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    const auto name = setting.displayName.empty() ? Label(setting.id) : setting.displayName;
    ImGui::TextWrapped("%s", name.c_str());
    if (!setting.description.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.63F, 0.63F, 0.67F, 1.0F));
        ImGui::TextWrapped("%s", setting.description.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::TableSetColumnIndex(1);
    bool changed = false;
    const float available = ImGui::GetContentRegionAvail().x;
    const float controlWidth = (std::min)(235.0F, (std::max)(160.0F, available - 48.0F));

    switch (setting.kind) {
    case Kind::Boolean: {
        const bool value = std::get<bool>(setting.value);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
            (std::max)(0.0F, available - 80.0F));
        if (ui::Switch("##switch", value))
            changed = registry.SetBool(mod, setting.id, !value);
        break;
    }
    case Kind::Integer: {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
            (std::max)(0.0F, available - controlWidth - 40.0F));
        int value = std::get<std::int32_t>(setting.value);
        double number = static_cast<double>(value);
        if (ui::Slider("##value", number,
                static_cast<double>(std::get<std::int32_t>(setting.minimum)),
                static_cast<double>(std::get<std::int32_t>(setting.maximum)),
                1.0, 0, controlWidth))
            changed = registry.SetInt(mod, setting.id, static_cast<int>(number));
        break;
    }
    case Kind::Float: {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
            (std::max)(0.0F, available - controlWidth - 40.0F));
        float value = std::get<float>(setting.value);
        double number = static_cast<double>(value);
        if (ui::Slider("##value", number,
                static_cast<double>(std::get<float>(setting.minimum)),
                static_cast<double>(std::get<float>(setting.maximum)),
                0.01, 2, controlWidth))
            changed = registry.SetFloat(mod, setting.id, static_cast<float>(number));
        break;
    }
    case Kind::Enum: {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
            (std::max)(0.0F, available - 190.0F));
        int index = std::get<std::int32_t>(setting.value);
        if (ui::Select("##value", index, setting.choices, 145.0F))
            changed = registry.SetEnum(mod, setting.id, index);
        break;
    }
    case Kind::Keybind: {
        const bool capturing = g_capture.active &&
            g_capture.mod == mod && g_capture.setting == setting.id;
        const bool coreMenuBind =
            mod == RED4ext::CNamePool::Add("m1core").hash &&
            setting.id == RED4ext::CNamePool::Add("Overlay Toggle").hash;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
            (std::max)(0.0F, available - 230.0F));
        const auto label = capturing ? std::string(coreMenuBind ? "Press your menu key..." :
                                                  "Press key... (Esc cancels)")
                                    : KeyLabel(std::get<std::int32_t>(setting.value));
        if (MonoFont()) ImGui::PushFont(MonoFont());
        if (ui::Button(label.c_str(), ui::Tone::Secondary, ImVec2(140.0F, 32.0F)))
            StartCapture(mod, setting.id);
        if (MonoFont()) ImGui::PopFont();
        ImGui::SameLine();
        if (ui::Button("Clear", ui::Tone::Ghost, ImVec2(42.0F, 32.0F))) {
            if (capturing) {
                g_capture.active = false;
                g_bindCaptureActive.store(false);
            }
            changed = registry.SetKeybind(mod, setting.id, 0);
        }
        break;
    }
    }

    ImGui::SameLine();
    if (ui::Button(icons::rotate, ui::Tone::Ghost, ImVec2(30.0F, 32.0F)))
        changed = registry.Reset(mod, setting.id);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Reset to default");
    ImGui::PopID();
    return changed;
}

bool DrawSettingsTable(Registry& registry, Id mod, const std::vector<SettingSnapshot>& settings,
                       std::string_view query, bool onlyKeybinds)
{
    // The user's final.html and main.cpp draw ONE continuous settings card
    // with group headers, not separate nested ImGui tables/cards per group.
    std::map<Id, std::vector<const SettingSnapshot*>> grouped;
    for (const auto& setting : settings) {
        if (onlyKeybinds && setting.kind != Kind::Keybind) continue;
        if (!MatchesSetting(setting, query)) continue;
        grouped[setting.group].push_back(&setting);
    }
    if (grouped.empty()) {
        ImGui::TextDisabled("No matching settings found.");
        return false;
    }
    std::vector<Id> order;
    for (const auto& [group, _] : grouped) order.push_back(group);
    // Group order mirrors the user's final.html, not a lexical hash order.
    const auto priority = [](Id group) {
        if (group == 0) return 0;
        const auto name = Label(group);
        if (name == "General") return 0;
        if (name == "Gameplay") return 1;
        if (name == "Controls" || name == "Hotkeys") return 2;
        return 3;
    };
    std::stable_sort(order.begin(), order.end(), [&](Id lhs, Id rhs) {
        const int a = priority(lhs), b = priority(rhs);
        return a != b ? a < b : Label(lhs) < Label(rhs);
    });
    bool changed = false;
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0F);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(17/255.0F,17/255.0F,20/255.0F,1));
    if (ImGui::BeginChild("##settingsCard", ImVec2(0,0),
             ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY,
             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        for (Id group : order) {
            ImGui::PushID(std::to_string(group).c_str());
            const auto& members = grouped.at(group);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(24/255.0F,24/255.0F,27/255.0F,1));
            ImGui::BeginChild("##groupHeader", ImVec2(0,42),
                              ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            const float right = ImGui::GetWindowContentRegionMax().x;
            ImGui::SetCursorPos(ImVec2(16,10));
            if (MediumFont()) ImGui::PushFont(MediumFont());
            const auto groupName = group == 0 ? std::string("General") : Label(group);
            ImGui::TextUnformatted(groupName.c_str());
            if (MediumFont()) ImGui::PopFont();
            ImGui::SetCursorPos(ImVec2((std::max)(130.0F,right-195.0F),8));
            ImGui::TextDisabled("%zu settings", members.size());
            ImGui::SameLine(0,9);
            const bool pending = g_confirmGroupReset &&
                g_pendingGroupMod == mod && g_pendingGroup == group;
            if (ui::Button(pending ? "Confirm reset" : "Reset group",
                           pending ? ui::Tone::Danger : ui::Tone::Secondary,
                           ImVec2(99,26))) {
                if (pending) {
                    changed |= registry.ResetGroup(mod,group);
                    g_confirmGroupReset = false;
                } else {
                    g_confirmGroupReset = true;
                    g_pendingGroupMod = mod;
                    g_pendingGroup = group;
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            if (ImGui::BeginTable("##settingRows",2,
                    ImGuiTableFlags_SizingStretchProp |
                    ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_PadOuterX)) {
                ImGui::TableSetupColumn("Setting",ImGuiTableColumnFlags_WidthStretch,0.66F);
                ImGui::TableSetupColumn("Control",ImGuiTableColumnFlags_WidthStretch,0.34F);
                for (const auto* setting : members)
                    changed |= DrawSetting(registry,mod,*setting);
                ImGui::EndTable();
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    return changed;
}

void DrawKeybindConflicts(const std::vector<ModSnapshot>& mods)
{
    const auto conflicts = FindKeybindConflicts(mods);
    if (conflicts.empty()) return;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12F, 0.086F, 0.03F, 1.0F));
    const bool visible = ImGui::BeginChild("keybind_conflicts", ImVec2(0, 0),
        ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (visible) {
        ImGui::TextColored(ImVec4(0.96F, 0.62F, 0.04F, 1.0F),
            "Keybind conflicts - %zu", conflicts.size());
        ImGui::TextWrapped("These actions share a key. The selected m1core menu key takes priority.");
        for (const auto& conflict : conflicts) {
            std::string owners;
            for (const auto& owner : conflict.owners) {
                if (!owners.empty()) owners += ", ";
                owners += Label(owner.mod) + " / " + Label(owner.setting);
            }
            ImGui::TextWrapped("%s: %s", KeyLabel(conflict.virtualKey).c_str(), owners.c_str());
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

bool ModMatches(const ModSnapshot& mod, std::string_view query, bool keybindsOnly)
{
    if (query.empty() && !keybindsOnly) return true;
    bool hasMatchingType = false;
    for (const auto& setting : mod.settings) {
        if (keybindsOnly && setting.kind != Kind::Keybind) continue;
        hasMatchingType = true;
        if (MatchesSetting(setting, query)) return true;
    }
    return hasMatchingType && Matches(Label(mod.id), query);
}

void DrawLogs()
{
    static int minimum = 0;
    static char search[128]{};
    static const std::vector<std::string> levels{"All", "Debug", "Info", "Warning", "Error"};

    ImGui::BeginChild("diagnostics_panel", ImVec2(0.0F, -66.0F), ImGuiChildFlags_Borders);
    if (HeadingFont()) ImGui::PushFont(HeadingFont());
    ImGui::TextUnformatted("Debug Log");
    if (HeadingFont()) ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextDisabled("In-game events and native mod diagnostics");
    ImGui::Separator();
    ImGui::SetNextItemWidth(155.0F);
    ui::Select("##logseverity", minimum, levels, 155.0F);
    ImGui::SameLine();
    ImGui::SetNextItemWidth((std::max)(140.0F, ImGui::GetContentRegionAvail().x - 170.0F));
    ImGui::InputTextWithHint("##logsearch", "Search source or message", search, sizeof(search));

    const auto entries = Diagnostics::Get().Snapshot();
    std::vector<std::string> lines;
    for (const auto& event : entries) {
        if (minimum && static_cast<int>(event.level) < minimum - 1) continue;
        if (!Matches(event.source, search) && !Matches(event.message, search)) continue;
        lines.push_back(event.timestamp + " [" + LogLevelName(event.level) + "] [" +
                        event.source + "] " + event.message);
    }
    ImGui::TextDisabled("Showing %zu / %zu recent events", lines.size(), entries.size());
    ImGui::SameLine();
    if (ui::Button("Copy visible", ui::Tone::Secondary)) {
        std::string copied;
        for (const auto& line : lines) {
            copied += line;
            copied += '\n';
        }
        ImGui::SetClipboardText(copied.c_str());
    }
    ImGui::SameLine();
    if (ui::Button("Clear panel", ui::Tone::Ghost))
        Diagnostics::Get().ClearView();
    ImGui::TextDisabled("Disk log: red4ext/plugins/m1core/m1core.log (retained when panel clears)");
    ImGui::Separator();

    ImGui::BeginChild("log_lines", ImVec2(0.0F, 0.0F), ImGuiChildFlags_Borders);
    if (MonoFont()) ImGui::PushFont(MonoFont());
    if (lines.empty()) {
        ImGui::TextDisabled("No events match the filter.");
    } else {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(lines.size()));
        while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                ImGui::TextUnformatted(lines[static_cast<std::size_t>(i)].c_str());
    }
    if (MonoFont()) ImGui::PopFont();
    ImGui::EndChild();
    ImGui::EndChild();
}

void DrawProfiles(Registry& registry)
{
    const auto names = registry.ListProfiles();
    if (!g_selectedProfile.empty() &&
        std::find(names.begin(), names.end(), g_selectedProfile) == names.end())
        g_selectedProfile.clear();
    if (g_selectedProfile.empty() && !names.empty()) {
        g_selectedProfile = names.front();
        std::snprintf(g_renameProfileName, sizeof(g_renameProfileName),
                      "%s", g_selectedProfile.c_str());
    }

    ImGui::BeginChild("profiles_panel", ImVec2(0.0F, -66.0F), ImGuiChildFlags_Borders);
    if (HeadingFont()) ImGui::PushFont(HeadingFont());
    ImGui::TextUnformatted("Settings Profiles");
    if (HeadingFont()) ImGui::PopFont();
    ImGui::Separator();
    ImGui::TextWrapped("Save different combinations of mod settings and keybinds, then restore them with one click.");
    ImGui::Spacing();

    ImGui::Text("Saved profiles: %zu / 24", names.size());
    ImGui::SetNextItemWidth((std::max)(220.0F, ImGui::GetContentRegionAvail().x * 0.60F));
    int profileIndex = -1;
    for (std::size_t i = 0; i < names.size(); ++i)
        if (names[i] == g_selectedProfile) profileIndex = static_cast<int>(i);
    if (ui::Select("##profiles", profileIndex, names,
                   (std::max)(220.0F, ImGui::GetContentRegionAvail().x * 0.60F))) {
        if (profileIndex >= 0 && static_cast<std::size_t>(profileIndex) < names.size()) {
            g_selectedProfile = names[static_cast<std::size_t>(profileIndex)];
            std::snprintf(g_renameProfileName, sizeof(g_renameProfileName),
                          "%s", g_selectedProfile.c_str());
            g_confirmOverwrite = false;
            g_confirmDelete = false;
            g_profileMessage.clear();
        }
    }

    ImGui::SetNextItemWidth((std::max)(220.0F, ImGui::GetContentRegionAvail().x * 0.60F));
    ImGui::InputTextWithHint("New profile name", "e.g. Stealth, Combat, Exploration",
                             g_newProfileName, sizeof(g_newProfileName));

    if (ui::Button("Create from current settings", ui::Tone::Primary)) {
        const std::string name(g_newProfileName);
        if (std::find(names.begin(), names.end(), name) != names.end()) {
            g_profileError = true;
            g_profileMessage = "A profile already has that name. Select it to overwrite.";
        } else if (registry.SaveProfile(name)) {
            g_selectedProfile = name;
            std::snprintf(g_renameProfileName, sizeof(g_renameProfileName), "%s", name.c_str());
            g_newProfileName[0] = '\0';
            g_profileError = false;
            g_profileMessage = "Profile created. Your current settings were saved.";
        } else {
            g_profileError = true;
            g_profileMessage = "Could not create profile. Check name, available slots and disk permissions.";
        }
        g_confirmOverwrite = false;
        g_confirmDelete = false;
    }

    ImGui::Spacing();
    if (g_selectedProfile.empty())
        ImGui::BeginDisabled();

    if (ui::Button("Load selected", ui::Tone::Primary)) {
        if (!registry.ActivateProfile(g_selectedProfile)) {
            g_profileError = true;
            g_profileMessage = "Could not load the selected profile.";
        } else {
            const bool saved = registry.Save();
            g_dirty = !saved;
            g_profileError = !saved;
            g_profileMessage = saved ? "Profile applied and settings.json saved."
                                      : "Profile applied in memory, but settings.json could not be saved.";
        }
        g_capture.active = false;
        g_bindCaptureActive.store(false); // Don't leave the menu shortcut disabled after loading a profile.
        g_confirmOverwrite = false;
        g_confirmDelete = false;
    }
    ImGui::SameLine();

    if (ui::Button(g_confirmOverwrite ? "Confirm overwrite" : "Overwrite selected",
                   g_confirmOverwrite ? ui::Tone::Danger : ui::Tone::Secondary)) {
        if (g_confirmOverwrite) {
            const bool ok = registry.SaveProfile(g_selectedProfile);
            g_profileError = !ok;
            g_profileMessage = ok ? "Profile updated from current settings."
                                  : "Unable to overwrite profile.";
            g_confirmOverwrite = false;
        } else {
            g_confirmOverwrite = true;
            g_confirmDelete = false;
            g_profileMessage = "Click Confirm overwrite to replace this profile's saved values.";
            g_profileError = false;
        }
    }
    ImGui::SameLine();

    if (ui::Button(g_confirmDelete ? "Confirm delete" : "Delete selected",
                   g_confirmDelete ? ui::Tone::Danger : ui::Tone::Secondary)) {
        if (g_confirmDelete) {
            const bool ok = registry.DeleteProfile(g_selectedProfile);
            g_profileError = !ok;
            g_profileMessage = ok ? "Profile deleted. Current game settings are unchanged."
                                  : "Unable to delete profile.";
            if (ok) {
                g_selectedProfile.clear();
                g_renameProfileName[0] = '\0';
            }
            g_confirmDelete = false;
        } else {
            g_confirmDelete = true;
            g_confirmOverwrite = false;
            g_profileMessage = "Click Confirm delete to permanently remove this profile.";
            g_profileError = false;
        }
    }

    ImGui::Spacing();
    ImGui::SetNextItemWidth((std::max)(200.0F, ImGui::GetContentRegionAvail().x * 0.55F));
    ImGui::InputTextWithHint("##renameProfile", "New name for selected profile",
                             g_renameProfileName, sizeof(g_renameProfileName));
    ImGui::SameLine();
    if (ui::Button("Rename selected", ui::Tone::Secondary)) {
        const std::string name(g_renameProfileName);
        if (registry.RenameProfile(g_selectedProfile, name)) {
            g_selectedProfile = name;
            g_profileError = false;
            g_profileMessage = "Profile renamed. Saved values and current settings are unchanged.";
            g_confirmOverwrite = false;
            g_confirmDelete = false;
        } else {
            g_profileError = true;
            g_profileMessage = "Rename failed. Enter a unique valid name (up to 64 bytes).";
        }
    }

    if (g_selectedProfile.empty())
        ImGui::EndDisabled();

    ImGui::Spacing();
    if (!g_profileMessage.empty()) {
        if (g_profileError)
            ImGui::TextWrapped("Error: %s", g_profileMessage.c_str());
        else
            ImGui::TextWrapped("%s", g_profileMessage.c_str());
    }
    ImGui::Spacing();
    ImGui::TextDisabled("Profiles: red4ext/plugins/m1core/profiles.json");
    ImGui::TextWrapped("Profiles include values for registered and previously saved mods. "
                       "They do not replace or delete settings.json. A loaded profile is "
                       "automatically written to settings.json for the next game start.");
    ImGui::EndChild();
}

} // namespace

void RenderSettings(Registry& registry, bool& open, bool interactive)
{
    // Fullscreen workspace follows the user's m1core-skeleton/main.cpp and
    // NCREC fixed-region composition. No floating/resizeable cheat-menu
    // window or stock ImGui titlebar. Registry remains the live data source.
    static char search[160]{};
    static Id selected = 0;
    static Page page = Page::Mods;
    static bool saveFailed = false;
    static bool firstRunSelected = false;
    const auto coreMod = RED4ext::CNamePool::Add("m1core");
    const auto coreToggle = RED4ext::CNamePool::Add("Overlay Toggle");
    const int menuKey = registry.GetKeybind(coreMod.hash,coreToggle.hash).value_or(0);
    if (menuKey == 0 && !firstRunSelected) {
        selected = RED4ext::CNamePool::Add("m1core.preview").hash;
        page = Page::Mods; // screenshot: action banner on All Mods
        firstRunSelected = true;
    }

    auto mods = registry.Snapshot();
    // Present mods in an understandable order; hashed IDs aren't a UI order.
    std::stable_sort(mods.begin(), mods.end(), [](const ModSnapshot& a, const ModSnapshot& b) {
        return Label(a.id) < Label(b.id);
    });
    if (mods.empty()) selected = 0;
    else if (std::none_of(mods.begin(),mods.end(),
        [](const ModSnapshot& mod) { return mod.id == selected; }))
        selected = mods.front().id;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const bool sidebarOn = page == Page::Mods || page == Page::Keybinds;
    const auto layout = workspace::Calculate(viewport->Size.x,viewport->Size.y,sidebarOn);

    ImGui::SetNextWindowPos(viewport->Pos,ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->Size,ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0.0F);
    const ImGuiWindowFlags fullFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;
    const bool visible = ImGui::Begin("##m1coreFullscreenWorkspace",nullptr,fullFlags);
    ImGui::PopStyleVar(3);
    if (!visible) { ImGui::End(); return; }

    // Child widgets are explicitly positioned within the viewport. Dear
    // ImGui's ItemSpacing after BeginChild otherwise adds 9px per region,
    // pushing the bottom footer off-screen even at 1920x1080.
    ImGui::SetCursorPos(ImVec2(0.0F, layout.header.y));
    // Header: the fixed 56px dark toolbar from the user's source.
    ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(7/255.0F,7/255.0F,9/255.0F,1));
    ImGui::BeginChild("##m1coreHeader",ImVec2(0,layout.header.height),
        ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::SetCursorPos(ImVec2(22,16));
    ImGui::TextUnformatted(icons::cube);
    ImGui::SameLine(0,9);
    if (MediumFont()) ImGui::PushFont(MediumFont());
    ImGui::TextUnformatted("m1core");
    if (MediumFont()) ImGui::PopFont();
    ImGui::SameLine(0,10);
    ImGui::TextDisabled("/");
    ImGui::SameLine(0,10);
    ImGui::TextDisabled("Mod Settings");
    ImGui::SameLine(0,12);
    ui::CountBadge("v0.5.0-dev");

    if (layout.header.width >= 780) {
        const float searchX = (std::max)(330.0F, layout.header.width - 445.0F);
        ImGui::SetCursorPos(ImVec2(searchX,12));
        ImGui::SetNextItemWidth(290.0F);
        ImGui::InputTextWithHint("##m1coreGlobalSearch",
            "Search mods, settings, keybinds...",search,sizeof(search));
        ImGui::SetCursorPos(ImVec2(layout.header.width-132.0F,18.0F));
        auto* draw = ImGui::GetWindowDrawList();
        const auto marker = ImGui::GetCursorScreenPos();
        draw->AddCircleFilled(ImVec2(marker.x+6.0F,marker.y+6.0F),
                              3.5F,IM_COL32(16,185,129,255));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+14.0F);
        ImGui::TextDisabled("RED4ext");
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Navigation is a separate fixed 44px strip, never inside a scrolling pane.
    ImGui::SetCursorPos(ImVec2(0.0F, layout.tabs.y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(9/255.0F,9/255.0F,11/255.0F,1));
    ImGui::BeginChild("##m1coreTabs",ImVec2(0,layout.tabs.height),
        ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::SetCursorPos(ImVec2(14,0));
    const auto switchPage = [&](Page dest) {
        if (page != dest) {
            page = dest;
            g_capture.active = false;
            g_bindCaptureActive.store(false);
            g_confirmGroupReset = false;
            g_pendingModReset = 0;
        }
    };
    std::size_t settingsCount = 0, keybindCount = 0;
    for (const auto& mod : mods) for (const auto& setting : mod.settings) {
        ++settingsCount;
        if (setting.kind == Kind::Keybind) ++keybindCount;
    }
    const std::string settingsText = std::to_string(settingsCount);
    const std::string keybindText = std::to_string(keybindCount);
    const std::string profileText = std::to_string(registry.ListProfiles().size());
    const std::string logText = std::to_string(Diagnostics::Get().Snapshot().size());
    const std::string tabs[] = {
        std::string(icons::sliders)+"  All Mods",
        std::string(icons::keyboard)+"  Keybinds",
        std::string(icons::layers)+"  Profiles",
        std::string(icons::terminal)+"  Logs",
        std::string(icons::info)+"  About"
    };
    if (ui::NavTab(tabs[0].c_str(),settingsText.c_str(),page==Page::Mods)) switchPage(Page::Mods);
    ImGui::SameLine(0,4);
    if (ui::NavTab(tabs[1].c_str(),keybindText.c_str(),page==Page::Keybinds)) switchPage(Page::Keybinds);
    ImGui::SameLine(0,4);
    if (ui::NavTab(tabs[2].c_str(),profileText.c_str(),page==Page::Profiles)) switchPage(Page::Profiles);
    ImGui::SameLine(0,4);
    if (ui::NavTab(tabs[3].c_str(),logText.c_str(),page==Page::Logs)) switchPage(Page::Logs);
    ImGui::SameLine(0,4);
    if (ui::NavTab(tabs[4].c_str(),nullptr,page==Page::About)) switchPage(Page::About);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SetCursorPos(ImVec2(0.0F, layout.body.y));
    ImGui::BeginChild("##m1coreBody",ImVec2(0,layout.body.height),
        ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const bool keybindsOnly = page == Page::Keybinds;
    if (layout.showSidebar && sidebarOn) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(10/255.0F,10/255.0F,13/255.0F,1));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0F);
        ImGui::BeginChild("##m1coreSidebar",ImVec2(layout.sidebar.width,0),
            ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();
        // Only a straight 1px separator, as in the user's flat sidebar.
        const ImVec2 sideMin = ImGui::GetWindowPos();
        const ImVec2 sideSize = ImGui::GetWindowSize();
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(sideMin.x+sideSize.x-1.0F,sideMin.y),
            ImVec2(sideMin.x+sideSize.x-1.0F,sideMin.y+sideSize.y),
            IM_COL32(39,39,42,255));
        ImGui::SetCursorPos(ImVec2(14,14));
        ImGui::TextDisabled("REGISTERED MODS");
        ImGui::SameLine();
        ImGui::SetCursorPosX(layout.sidebar.width-30.0F);
        ImGui::TextDisabled("%zu",mods.size());
        ImGui::SetCursorPos(ImVec2(8,47));
        ImGui::BeginChild("##m1coreModList",ImVec2(layout.sidebar.width-16.0F,-112.0F),
            ImGuiChildFlags_None);
        for (const auto& mod : mods) {
            if (!ModMatches(mod,search,keybindsOnly)) continue;
            std::size_t count = 0;
            for (const auto& setting : mod.settings)
                if (!keybindsOnly || setting.kind == Kind::Keybind) ++count;
            ImGui::PushID(std::to_string(mod.id).c_str());
            const auto name = Label(mod.id);
            const auto countText = "(" + std::to_string(count) + ")";
            if (ui::SidebarItem(name.c_str(),countText.c_str(),
                               selected==mod.id,ImGui::GetContentRegionAvail().x))
                selected = mod.id;
            ImGui::PopID();
            ImGui::Dummy(ImVec2(0,2));
        }
        ImGui::EndChild();
        ImGui::SetCursorPos(ImVec2(12,layout.body.height-102));
        ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(24/255.0F,24/255.0F,27/255.0F,1));
        ImGui::BeginChild("##revisionStatus",ImVec2(layout.sidebar.width-24,86),
                          ImGuiChildFlags_Borders);
        ImGui::TextColored(ImVec4(0.98F,0.75F,0.14F,1.0F),
            "%s  Live Revision Sync",icons::bolt);
        ImGui::TextWrapped("Changes update mod revision tokens immediately in memory.");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::SameLine(0,0);
    }

    // The center pane is capped at the same practical width as the HTML.
    // Only this pane scrolls; header, nav, sidebar and footer remain fixed.
    ImGui::BeginChild("##m1coreContentHost",ImVec2(0,0),
        ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float centerWidth = (std::max)(1.0F,
        (std::min)(940.0F, availableWidth - (availableWidth > 360.0F ? 48.0F : 16.0F)));
    const float offset = (std::max)(0.0F,(availableWidth-centerWidth)*0.5F);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+offset);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::BeginChild("##m1coreContentScroll",ImVec2(centerWidth,0),ImGuiChildFlags_None);
    ImGui::PopStyleVar();
    ImGui::Dummy(ImVec2(0,22));

    // The action banner exists only until the player sets their own key.
    if (menuKey == 0) {
        const auto pos = ImGui::GetCursorScreenPos();
        const float w = ImGui::GetContentRegionAvail().x;
        const float h = 88;
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(pos,ImVec2(pos.x+w,pos.y+h),
                            IM_COL32(48,35,13,255),8.0F);
        draw->AddRect(pos,ImVec2(pos.x+w,pos.y+h),
                      IM_COL32(118,80,12,255),8.0F);
        ImGui::SetCursorScreenPos(ImVec2(pos.x+18,pos.y+15));
        ImGui::TextColored(ImVec4(0.98F,0.75F,0.14F,1.0F),
            "%s  Action Required - No Menu Hotkey Assigned",icons::warning);
        ImGui::SetCursorScreenPos(ImVec2(pos.x+18,pos.y+43));
        ImGui::PushTextWrapPos(pos.x+w-200);
        ImGui::TextDisabled("No menu key is assigned. Choose one via Keybinds > m1core > Open / Close Menu.");
        ImGui::PopTextWrapPos();
        ImGui::SetCursorScreenPos(ImVec2(pos.x+w-175,pos.y+26));
        if (ui::Button("Bind my menu key",ui::Tone::Primary,ImVec2(158,34))) {
            switchPage(Page::Keybinds);
            selected = coreMod.hash;
            StartCapture(coreMod.hash,coreToggle.hash);
        }
        ImGui::SetCursorScreenPos(ImVec2(pos.x,pos.y+h+20));
    }

    if (page == Page::Profiles) {
        DrawProfiles(registry);
    } else if (page == Page::Logs) {
        DrawLogs();
    } else if (page == Page::About) {
        if (HeadingFont()) ImGui::PushFont(HeadingFont());
        ImGui::TextUnformatted("About m1core");
        if (HeadingFont()) ImGui::PopFont();
        ImGui::TextWrapped("A native RED4ext settings framework for compatible Cyberpunk 2077 mods.");
        ui::Separator();
        ImGui::Text("Registered mods: %zu",mods.size());
        const std::string keyText = KeyLabel(menuKey);
        ImGui::Text("Open / close menu: %s",keyText.c_str());
        if (menuKey > 0)
            ImGui::Text("Emergency disable: Ctrl + Shift + %s",keyText.c_str());
        ImGui::Text("Mouse path: %s",CursorDetoursAvailable() ?
            "Windows system pointer" : "RAWINPUT fallback");
        ImGui::Spacing();
        ImGui::TextDisabled("Settings: red4ext/plugins/m1core/settings.json");
        ImGui::TextDisabled("Profiles: red4ext/plugins/m1core/profiles.json");
        ImGui::TextDisabled("Logs: red4ext/plugins/m1core/m1core.log");
        ImGui::Spacing();
        ImGui::TextWrapped("Recovery: delete red4ext/plugins/m1core/overlay.enable before launching to disable graphics and input hooks.");
    } else {
        if (keybindsOnly) DrawKeybindConflicts(mods);
        auto current = std::find_if(mods.begin(),mods.end(),
            [](const ModSnapshot& mod){return mod.id==selected;});
        if (current==mods.end() || !ModMatches(*current,search,keybindsOnly)) {
            ImGui::TextDisabled("Choose a mod in the sidebar to view its settings.");
        } else {
            const auto name = Label(current->id);
            const float titleY = ImGui::GetCursorPosY();
            if (HeadingFont()) ImGui::PushFont(HeadingFont());
            ImGui::TextUnformatted(name.c_str());
            if (HeadingFont()) ImGui::PopFont();
            ImGui::SameLine(0,10);
            ImGui::TextDisabled("(%zu settings)",current->settings.size());
            if (name=="m1core.preview")
                ImGui::TextDisabled("Reference sample plugin demonstrating native controls, sliders, and revision tokens.");
            else
                ImGui::TextDisabled("Live configuration for %s",name.c_str());
            const float afterDescription = ImGui::GetCursorPosY();
            const float resetX = ImGui::GetWindowContentRegionMax().x-185.0F;
            ImGui::SetCursorPos(ImVec2((std::max)(0.0F,resetX),titleY));
            const bool pending = g_pendingModReset==current->id;
            if (ui::Button(pending ? "Confirm reset mod" : "Reset mod to defaults",
                           pending ? ui::Tone::Danger : ui::Tone::Secondary,
                           ImVec2(180,32))) {
                if (pending) {
                    g_dirty |= registry.ResetMod(current->id);
                    g_pendingModReset = 0;
                    g_confirmGroupReset = false;
                } else {
                    g_pendingModReset = current->id;
                    g_confirmGroupReset = false;
                }
            }
            ImGui::SetCursorPosY((std::max)(afterDescription,titleY+48.0F));
            ImGui::Dummy(ImVec2(0,12));
            ImGui::PushID(std::to_string(current->id).c_str());
            const std::string_view filter = Matches(name,search) ?
                std::string_view{} : search;
            g_dirty |= DrawSettingsTable(registry,current->id,
                                         current->settings,filter,keybindsOnly);
            ImGui::PopID();
        }
    }
    ImGui::Dummy(ImVec2(0,24));
    ImGui::EndChild(); // center pane
    ImGui::EndChild(); // content host
    ImGui::EndChild(); // body

    // Fixed bottom action bar. All actions connect to real Registry methods.
    ImGui::SetCursorPos(ImVec2(0.0F, layout.footer.y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(7/255.0F,7/255.0F,9/255.0F,1));
    ImGui::BeginChild("##m1coreFooter",ImVec2(0,layout.footer.height),
        ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::SetCursorPos(ImVec2(22,20));
    const ImVec2 statusPos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddCircleFilled(
        ImVec2(statusPos.x+4,statusPos.y+7),4,
        g_dirty ? IM_COL32(251,191,36,255) : IM_COL32(16,185,129,255));
    ImGui::SetCursorPosX(35);
    if (g_dirty)
        ImGui::TextColored(ImVec4(0.98F,0.75F,0.14F,1),"Unsaved changes");
    else
        ImGui::TextDisabled("All changes saved");
    const float right = layout.footer.width-22.0F;
    float x = right;
    const auto footerButton = [&](const char* label,float w,ui::Tone tone) {
        x-=w;
        ImGui::SetCursorPos(ImVec2(x,12));
        const bool clicked = ui::Button(label,tone,ImVec2(w,32));
        x-=8;
        return clicked;
    };
    if (menuKey==0) ImGui::BeginDisabled();
    if (footerButton("Close",86.0F,ui::Tone::Ghost)) open=false;
    if (footerButton("Save & Close",128.0F,ui::Tone::Secondary)) {
        saveFailed = !registry.Save();
        if (!saveFailed) {g_dirty=false;open=false;}
    }
    if (menuKey==0) ImGui::EndDisabled();
    if (footerButton("Save",88.0F,ui::Tone::Primary)) {
        saveFailed = !registry.Save();
        if (!saveFailed) g_dirty=false;
    }
    if (footerButton("Discard",88.0F,ui::Tone::Ghost)) {
        if (registry.Load()) { g_dirty=false; saveFailed=false; }
        else saveFailed=true;
    }
    if (saveFailed) {
        ImGui::SetCursorPos(ImVec2(35,2));
        ImGui::TextColored(ImVec4(1,0.3F,0.3F,1),"Unable to save/reload settings.json");
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (interactive)
        g_dirty |= PollCapture(registry);
    ImGui::End();
}

void OnOverlayFocusLost()
{
    g_capture.active = false;
    g_bindCaptureActive.store(false);
}

void OnOverlayHidden(bool persisted)
{
    g_capture.active = false;
    g_bindCaptureActive.store(false);
    g_confirmOverwrite = false;
    g_confirmDelete = false;
    g_confirmGroupReset = false;
    g_pendingModReset = 0;
    if (persisted) g_dirty = false;
}

bool IsCapturingKeybind()
{
    return g_bindCaptureActive.load();
}
} // namespace m1core
