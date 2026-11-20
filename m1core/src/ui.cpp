#include "ui.hpp"
#include "settings.hpp"
#include "diagnostics.hpp"
#include "ui_theme.hpp"
#include "ui_components.hpp"
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
    ImGui::TableNextRow();
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
    const float controlWidth = (std::max)(95.0F, available - 75.0F);

    switch (setting.kind) {
    case Kind::Boolean: {
        const bool value = std::get<bool>(setting.value);
        if (ui::Switch("##switch", value))
            changed = registry.SetBool(mod, setting.id, !value);
        ImGui::SameLine();
        ImGui::TextDisabled(value ? "On" : "Off");
        break;
    }
    case Kind::Integer: {
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
        int index = std::get<std::int32_t>(setting.value);
        if (ui::Select("##value", index, setting.choices, controlWidth))
            changed = registry.SetEnum(mod, setting.id, index);
        break;
    }
    case Kind::Keybind: {
        const bool capturing = g_capture.active &&
            g_capture.mod == mod && g_capture.setting == setting.id;
        const bool coreMenuBind =
            mod == RED4ext::CNamePool::Add("m1core").hash &&
            setting.id == RED4ext::CNamePool::Add("Overlay Toggle").hash;
        const auto label = capturing ? std::string(coreMenuBind ? "Press your menu key..." :
                                                  "Press key... (Esc cancels)")
                                    : KeyLabel(std::get<std::int32_t>(setting.value));
        if (MonoFont()) ImGui::PushFont(MonoFont());
        if (ui::Button(label.c_str(), ui::Tone::Secondary,
                       ImVec2((std::max)(108.0F, available - 126.0F), 34.0F)))
            StartCapture(mod, setting.id);
        if (MonoFont()) ImGui::PopFont();
        ImGui::SameLine();
        if (ui::Button("Clear", ui::Tone::Ghost, ImVec2(53.0F, 34.0F))) {
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
    if (ui::Button("Reset", ui::Tone::Ghost, ImVec2(54.0F, 34.0F)))
        changed = registry.Reset(mod, setting.id);
    ImGui::PopID();
    return changed;
}

bool DrawSettingsTable(Registry& registry, Id mod, const std::vector<SettingSnapshot>& settings,
                       std::string_view query, bool onlyKeybinds)
{
    bool changed = false;
    // Render one elevated native ImGui card per group, with readable
    // descriptions always visible. No browser/webview or schema changes.
    std::map<Id, std::vector<const SettingSnapshot*>> grouped;
    for (const auto& setting : settings) {
        if (onlyKeybinds && setting.kind != Kind::Keybind) continue;
        if (!MatchesSetting(setting, query)) continue;
        grouped[setting.group].push_back(&setting);
    }
    if (grouped.empty()) {
        ImGui::TextDisabled("No matching settings found for this mod.");
        return false;
    }

    std::vector<Id> order;
    for (const auto& [group, _] : grouped) order.push_back(group);
    std::sort(order.begin(), order.end(), [](Id lhs, Id rhs) {
        if (lhs == 0) return true;
        if (rhs == 0) return false;
        return Label(lhs) < Label(rhs);
    });

    for (Id group : order) {
        const auto& members = grouped.at(group);
        ImGui::PushID(std::to_string(group).c_str());
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.094F, 0.094F, 0.106F, 1.0F));
        const bool visible = ImGui::BeginChild("group_card", ImVec2(0, 0),
            ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        if (visible) {
            const auto groupName = group == 0 ? std::string("General") : Label(group);
            ImGui::TextUnformatted(groupName.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%zu settings", members.size());
            const bool pending = g_confirmGroupReset &&
                                 g_pendingGroupMod == mod && g_pendingGroup == group;
            ImGui::SameLine();
            if (ui::Button(pending ? "Confirm reset group" : "Reset group",
                           pending ? ui::Tone::Danger : ui::Tone::Ghost)) {
                if (pending) {
                    changed |= registry.ResetGroup(mod, group);
                    g_confirmGroupReset = false;
                } else {
                    g_confirmGroupReset = true;
                    g_pendingGroupMod = mod;
                    g_pendingGroup = group;
                }
            }
            ui::Separator(5.0F);
            if (ImGui::BeginTable("settings_rows", 2,
                    ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerH |
                    ImGuiTableFlags_PadOuterX)) {
                ImGui::TableSetupColumn("Setting", ImGuiTableColumnFlags_WidthStretch, 0.54F);
                ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch, 0.46F);
                for (const auto* setting : members)
                    changed |= DrawSetting(registry, mod, *setting);
                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopID();
        ImGui::Spacing();
    }
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

void RenderSettings(Registry& registry, bool& open)
{
    // Fully native ImGui layout and colors based on the supplied HTML reference.
    static char search[160]{};
    static Id selected = 0;
    static Page page = Page::Mods;
    static bool saveFailed = false;
    static bool firstRunSelected = false;
    const auto coreMod = RED4ext::CNamePool::Add("m1core");
    const auto coreToggle = RED4ext::CNamePool::Add("Overlay Toggle");
    const int menuKey = registry.GetKeybind(coreMod.hash, coreToggle.hash).value_or(0);
    if (menuKey == 0 && !firstRunSelected) {
        // First run needs no preassigned key: menu auto-opens and selects
        // the binding control. A user can choose a key without editing files.
        selected = coreMod.hash;
        page = Page::Keybinds;
        firstRunSelected = true;
    }

    const auto mods = registry.Snapshot();
    if (mods.empty()) selected = 0;
    else if (std::none_of(mods.begin(), mods.end(),
        [](const ModSnapshot& mod) { return mod.id == selected; }))
        selected = mods.front().id;

    const ImVec2 display = ImGui::GetMainViewport()->Size;
    ImGui::SetNextWindowSize(
        ImVec2((std::min)(1080.0F, display.x * 0.92F),
               (std::min)(730.0F, display.y * 0.88F)),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(720.0F, 470.0F),
                                         ImVec2(FLT_MAX, FLT_MAX));
    // Never strand the player behind a closed, unbound menu. The X and
    // footer Close actions become available after a menu key is assigned.
    if (!ImGui::Begin("m1core - Mod Settings", nullptr,
                      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    if (HeadingFont()) ImGui::PushFont(HeadingFont());
    ImGui::TextUnformatted("m1core");
    if (HeadingFont()) ImGui::PopFont();
    ImGui::SameLine(0.0F, 11.0F);
    ImGui::TextDisabled("/ Mod Settings");
    ImGui::SameLine();
    ImGui::TextDisabled("v0.5.0-dev");
    if (menuKey > 0) {
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - 33.0F);
        if (ui::Button("X", ui::Tone::Ghost, ImVec2(32.0F, 30.0F)))
            open = false;
    }
    ui::Separator(9.0F);
    if (menuKey == 0) {
        ImGui::TextWrapped("No m1core menu key is assigned. Choose one now: "
                           "Keybinds > m1core > Open / Close Menu. "
                           "The menu cannot be closed until a key is assigned, "
                           "so you can always reopen it.");
        if (ui::Button("Bind my menu key", ui::Tone::Primary))
            StartCapture(coreMod.hash, coreToggle.hash);
        if (g_capture.active && g_capture.mod == coreMod.hash &&
            g_capture.setting == coreToggle.hash) {
            ImGui::SameLine();
            if (ui::Button("Cancel key selection", ui::Tone::Ghost)) {
                g_capture.active = false;
                g_bindCaptureActive.store(false);
            }
        }
        ui::Separator(7.0F);
    }

    ImGui::SetNextItemWidth((std::max)(150.0F, ImGui::GetContentRegionAvail().x * 0.42F));
    ImGui::InputTextWithHint("##search", "Search mods and settings", search, sizeof(search));
    if (search[0] != '\0') {
        ImGui::SameLine();
        if (ui::Button("Clear search", ui::Tone::Ghost)) search[0] = '\0';
    }
    ui::Separator(4.0F);
    if (ui::Tab("All Mods", page == Page::Mods)) page = Page::Mods;
    ImGui::SameLine(0.0F, 4.0F);
    if (ui::Tab("Keybinds", page == Page::Keybinds)) page = Page::Keybinds;
    ImGui::SameLine(0.0F, 4.0F);
    if (ui::Tab("Profiles", page == Page::Profiles)) page = Page::Profiles;
    ImGui::SameLine(0.0F, 4.0F);
    if (ui::Tab("Logs", page == Page::Logs)) page = Page::Logs;
    ImGui::SameLine(0.0F, 4.0F);
    if (ui::Tab("About", page == Page::About)) page = Page::About;
    ui::Separator(5.0F);

    const bool keybindsOnly = page == Page::Keybinds;
    if (page == Page::Profiles) {
        DrawProfiles(registry);
    } else if (page == Page::Logs) {
        DrawLogs();
    } else if (page != Page::About) {
        const float sidebarWidth = (std::clamp)(ImGui::GetContentRegionAvail().x * 0.28F, 155.0F, 220.0F);
        ImGui::BeginChild("mods_sidebar", ImVec2(sidebarWidth, -72.0F), ImGuiChildFlags_Borders);
        ImGui::TextDisabled("REGISTERED MODS");
        ImGui::Separator();
        for (const auto& mod : mods) {
            if (!ModMatches(mod, search, keybindsOnly)) continue;
            std::size_t count = 0;
            for (const auto& setting : mod.settings)
                if (!keybindsOnly || setting.kind == Kind::Keybind) ++count;
            const auto name = Label(mod.id);
            const auto countText = std::to_string(count);
            ImGui::PushID(std::to_string(mod.id).c_str());
            if (ui::SidebarItem(name.c_str(), countText.c_str(),
                                selected == mod.id, ImGui::GetContentRegionAvail().x))
                selected = mod.id;
            ImGui::PopID();
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("mod_settings", ImVec2(0.0F, -72.0F), ImGuiChildFlags_Borders);
        if (keybindsOnly) DrawKeybindConflicts(mods);
        auto current = std::find_if(mods.begin(), mods.end(),
            [](const ModSnapshot& mod) { return mod.id == selected; });
        if (current == mods.end() || !ModMatches(*current, search, keybindsOnly)) {
            ImGui::TextDisabled("Choose a mod from the sidebar.");
        } else {
            const auto name = Label(current->id);
            ImGui::TextUnformatted(name.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(%zu settings)", current->settings.size());
            ImGui::SameLine();
            const bool pendingReset = g_pendingModReset == current->id;
            if (ui::Button(pendingReset ? "Confirm reset mod" : "Reset mod to defaults",
                            pendingReset ? ui::Tone::Danger : ui::Tone::Ghost)) {
                if (pendingReset) {
                    g_dirty |= registry.ResetMod(current->id);
                    g_pendingModReset = 0;
                    g_confirmGroupReset = false;
                } else {
                    g_pendingModReset = current->id;
                    g_confirmGroupReset = false;
                }
            }
            ui::Separator(7.0F);
            ImGui::PushID(std::to_string(current->id).c_str());
            // If the module name is what matched, show its full setting list.
            const std::string_view filter = Matches(name, search) ? std::string_view{} : search;
            g_dirty |= DrawSettingsTable(registry, current->id, current->settings, filter, keybindsOnly);
            ImGui::PopID();
        }
        ImGui::EndChild();
    } else {
        ImGui::BeginChild("about", ImVec2(0.0F, -72.0F), ImGuiChildFlags_Borders);
        if (HeadingFont()) ImGui::PushFont(HeadingFont());
        ImGui::TextUnformatted("About m1core");
        if (HeadingFont()) ImGui::PopFont();
        ImGui::Separator();
        ImGui::TextWrapped("RED4ext + Dear ImGui, independent of CET. Compatible mods register their own live settings and keybinds.");
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::TextDisabled("Registered mods");
        ImGui::SameLine(220.0F);
        ImGui::Text("%zu", mods.size());
        const auto shortcut = KeyLabel(menuKey);
        ImGui::TextDisabled("Menu key");
        ImGui::SameLine(220.0F);
        ImGui::TextUnformatted(shortcut.c_str());
        if (menuKey > 0) ImGui::Text("Emergency disable: Ctrl + Shift + %s", shortcut.c_str());
        else ImGui::TextDisabled("Emergency shortcut: assign a menu key first");
        ImGui::TextDisabled("Mouse path");
        ImGui::SameLine(220.0F);
        ImGui::TextUnformatted(CursorDetoursAvailable() ? "Windows system pointer" : "RAWINPUT fallback");
        ImGui::Separator();
        ImGui::TextDisabled("No default menu key is assigned. Choose one in Keybinds > m1core.");
        ImGui::TextUnformatted("Settings: red4ext/plugins/m1core/settings.json");
        ImGui::TextUnformatted("Profiles: red4ext/plugins/m1core/profiles.json");
        ImGui::TextUnformatted("Logs: red4ext/plugins/m1core/m1core.log");
        ImGui::Spacing();
        ImGui::TextWrapped("Emergency recovery: close the game and delete red4ext/plugins/m1core/overlay.enable to disable m1core graphics and input hooks.");
        ImGui::TextDisabled("Development preview - native controls and typography are evolving.");
        ImGui::EndChild();
    }

    // Footer is a functional action area. Apply = immediate; Save persists to
    // JSON. Closing with the player-selected key uses the overlay input gate.
    ui::Separator(5.0F);
    if (g_dirty)
        ImGui::TextColored(ImVec4(0.96F, 0.62F, 0.04F, 1.0F), "Unsaved changes");
    else
        ImGui::TextDisabled("Changes applied - no pending save");
    ImGui::SameLine();
    const float actionsStart = ImGui::GetWindowContentRegionMax().x - 302.0F;
    if (ImGui::GetCursorPosX() < actionsStart)
        ImGui::SetCursorPosX(actionsStart);
    if (ui::Button("Save", ui::Tone::Primary, ImVec2(76.0F, 34.0F))) {
        saveFailed = !registry.Save();
        if (!saveFailed) g_dirty = false;
    }
    ImGui::SameLine(0.0F, 7.0F);
    if (menuKey == 0) ImGui::BeginDisabled();
    if (ui::Button("Save & Close", ui::Tone::Secondary, ImVec2(125.0F, 34.0F))) {
        saveFailed = !registry.Save();
        if (!saveFailed) { g_dirty = false; open = false; }
    }
    ImGui::SameLine(0.0F, 7.0F);
    if (ui::Button("Close", ui::Tone::Ghost, ImVec2(78.0F, 34.0F))) open = false;
    if (menuKey == 0) ImGui::EndDisabled();
    if (saveFailed) ImGui::TextColored(ImVec4(0.96F, 0.44F, 0.44F, 1.0F),
                                       "Could not save settings.json");
    g_dirty |= PollCapture(registry);
    ImGui::End();
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
