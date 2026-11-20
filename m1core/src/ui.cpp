#include "ui.hpp"
#include "settings.hpp"
#include "diagnostics.hpp"
#include "ui_theme.hpp"
#include "ui_components.hpp"
#include "ui_icons.hpp"
#include "workspace_layout.hpp"
#include "overlay_ui_policy.hpp"
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
    ImGui::TextUnformatted(name.c_str());
    if (!setting.description.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 38.0F);
        ImGui::TextUnformatted(setting.description.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    ImGui::TableSetColumnIndex(1);

    bool changed = false;
    const auto width = (std::max)(90.0F, ImGui::GetContentRegionAvail().x - 63.0F);
    ImGui::SetNextItemWidth(width);
    switch (setting.kind) {
    case Kind::Boolean: {
        bool value = std::get<bool>(setting.value);
        if (ImGui::Checkbox("##value", &value))
            changed = registry.SetBool(mod, setting.id, value);
        break;
    }
    case Kind::Integer: {
        int value = std::get<std::int32_t>(setting.value);
        if (ImGui::SliderInt("##value", &value, std::get<std::int32_t>(setting.minimum),
                             std::get<std::int32_t>(setting.maximum)))
            changed = registry.SetInt(mod, setting.id, value);
        break;
    }
    case Kind::Float: {
        float value = std::get<float>(setting.value);
        if (ImGui::SliderFloat("##value", &value, std::get<float>(setting.minimum),
                               std::get<float>(setting.maximum), "%.2f"))
            changed = registry.SetFloat(mod, setting.id, value);
        break;
    }
    case Kind::Enum: {
        const auto index = std::get<std::int32_t>(setting.value);
        const bool valid = index >= 0 && static_cast<std::size_t>(index) < setting.choices.size();
        const auto preview = valid ? setting.choices[static_cast<std::size_t>(index)].c_str() : "Select...";
        if (ImGui::BeginCombo("##value", preview)) {
            for (std::size_t i = 0; i < setting.choices.size(); ++i) {
                const auto selected = index == static_cast<std::int32_t>(i);
                if (ImGui::Selectable(setting.choices[i].c_str(), selected))
                    changed = registry.SetEnum(mod, setting.id, static_cast<std::int32_t>(i));
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
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
        if (ImGui::Button(label.c_str(), ImVec2(width, 0.0F)))
            StartCapture(mod, setting.id);
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            if (capturing) {
                g_capture.active = false;
                g_bindCaptureActive.store(false);
            }
            changed = registry.SetKeybind(mod, setting.id, 0);
        }
        break;
    }
    }
    if (setting.kind != Kind::Keybind)
        ImGui::SameLine();
    if (ImGui::SmallButton("Reset"))
        changed = registry.Reset(mod, setting.id);
    ImGui::PopID();
    return changed;
}

bool DrawSettingsTable(Registry& registry, Id mod, const std::vector<SettingSnapshot>& settings,
                       std::string_view query, bool onlyKeybinds)
{
    bool changed = false;
    // Group settings by the owning mod's optional category metadata. The
    // underlying values, stable IDs and schema remain unchanged.
    std::map<Id, std::vector<const SettingSnapshot*>> grouped;
    for (const auto& setting : settings) {
        if (onlyKeybinds && setting.kind != Kind::Keybind) continue;
        if (!MatchesSetting(setting, query)) continue;
        grouped[setting.group].push_back(&setting);
    }
    std::vector<Id> order;
    order.reserve(grouped.size());
    for (const auto& [group, _] : grouped) order.push_back(group);
    std::sort(order.begin(), order.end(), [](Id lhs, Id rhs) {
        if (lhs == 0) return true;       // Ungrouped first.
        if (rhs == 0) return false;
        return Label(lhs) < Label(rhs);  // Actual category names, not hashes.
    });

    if (ImGui::BeginTable("settings_table", 2, ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerH)) {
        ImGui::TableSetupColumn("Setting", ImGuiTableColumnFlags_WidthStretch, 0.45F);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.55F);
        ImGui::TableHeadersRow();

        for (Id group : order) {
            const auto& members = grouped.at(group);
            if (group != 0 || order.size() > 1) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                const auto groupName = group == 0 ? std::string("General") : Label(group);
                ImGui::TextDisabled("%s", groupName.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("%zu settings", members.size());
                ImGui::SameLine();
                ImGui::PushID(std::to_string(group).c_str());
                const bool pending = g_confirmGroupReset &&
                                     g_pendingGroupMod == mod && g_pendingGroup == group;
                if (ImGui::SmallButton(pending ? "Confirm reset group" : "Reset group")) {
                    if (pending) {
                        changed |= registry.ResetGroup(mod, group);
                        g_confirmGroupReset = false;
                    } else {
                        g_confirmGroupReset = true;
                        g_pendingGroupMod = mod;
                        g_pendingGroup = group;
                    }
                }
                ImGui::PopID();
            }
            for (const auto* setting : members)
                changed |= DrawSetting(registry, mod, *setting);
        }
        ImGui::EndTable();
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

void RenderSettings(Registry& registry, bool& open, bool interactive)
{
    // Keep the functional layout from the supplied prototype, not its colors.
    static char search[160]{};
    static Id selected = 0;
    static Page page = Page::Mods;
    static bool saveFailed = false;
    const auto coreMod = RED4ext::CNamePool::Add("m1core");
    const auto coreToggle = RED4ext::CNamePool::Add("Overlay Toggle");
    const int menuKey = registry.GetKeybind(coreMod.hash,coreToggle.hash).value_or(0);
    if (NeedsCompactSetup(menuKey)) {
        // CET-like compact first-run window: never open the massive settings
        // workspace before the player has picked their own menu shortcut.
        const auto* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
        ImGui::SetNextWindowSize(ImVec2(470.0F, 170.0F), ImGuiCond_Always);
        const ImGuiWindowFlags setupFlags = ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("m1core - First Time Setup", nullptr, setupFlags)) {
            ImGui::TextWrapped("Choose a key to open and close m1core.");
            ImGui::TextDisabled("No default key is assigned. The binding is saved automatically.");
            ImGui::Spacing();
            const bool recording = g_capture.active &&
                g_capture.mod == coreMod.hash && g_capture.setting == coreToggle.hash;
            if (recording) {
                ImGui::TextUnformatted("Press your preferred menu key...");
            } else if (ImGui::Button("Bind overlay key", ImVec2(-1.0F, 34.0F))) {
                StartCapture(coreMod.hash, coreToggle.hash);
            }
        }
        ImGui::End();
        if (interactive) {
            g_dirty |= PollCapture(registry);
            if (registry.GetKeybind(coreMod.hash, coreToggle.hash).value_or(0) > 0)
                open = false; // Return to game; the chosen key opens full UI.
        }
        return;
    }


    const auto mods = registry.Snapshot();
    if (mods.empty()) selected = 0;
    else if (std::none_of(mods.begin(), mods.end(),
        [](const ModSnapshot& mod) { return mod.id == selected; }))
        selected = mods.front().id;

    // Reference: player's sharp-edged black/white M1CORE image. This is a
    // conventional ImGui menu with native Buttons, InputText, Selectable,
    // Combo, Slider and Checkbox. NO blurred game background, browser UI
    // or custom input capture code is needed for the visual shell.
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float width = (std::min)(1100.0F, (std::max)(1.0F, viewport->Size.x - 48.0F));
    const float height = (std::min)(625.0F, (std::max)(1.0F, viewport->Size.y - 48.0F));
    constexpr float headerHeight = 46.0F;
    constexpr float navHeight = 53.0F;
    constexpr float footerHeight = 48.0F;
    const float contentHeight = (std::max)(1.0F, height-headerHeight-navHeight-footerHeight);
    const float sidebarWidth = (std::min)(224.0F, (std::max)(155.0F, width*0.25F));
    const ImGuiWindowFlags noScroll =
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always,
                            ImVec2(0.5F, 0.5F));
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0F, 0.0F));
    const ImGuiWindowFlags shellFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings | noScroll;
    const bool shown = ImGui::Begin("##m1coreSettingsShell", nullptr, shellFlags);
    ImGui::PopStyleVar(4);
    if (!shown) { ImGui::End(); return; }

    // Bright titlebar, as in the reference. The actual development version is
    // displayed rather than borrowing the 1.4.1 mockup version number.
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1, 1, 1, 1));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.06F, 0.06F, 0.06F, 1));
    ImGui::BeginChild("##titlebar", ImVec2(width, headerHeight),
                      ImGuiChildFlags_None, noScroll);
    ImGui::SetCursorPos(ImVec2(18, 9));
    if (HeadingFont()) ImGui::PushFont(HeadingFont());
    ImGui::TextUnformatted("M1CORE");
    if (HeadingFont()) ImGui::PopFont();
    ImGui::SameLine(0, 12);
    ImGui::TextColored(ImVec4(0.58F, 0.58F, 0.58F, 1), "v0.5.0-dev");
    ImGui::SetCursorPos(ImVec2(width-92.0F, 7.0F));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.02F, 0.02F, 0.02F, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18F, 0.18F, 0.18F, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.28F, 0.28F, 0.28F, 1));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    if (ImGui::Button("CLOSE", ImVec2(82, 32))) open = false;
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    ImGui::EndChild();
    ImGui::PopStyleColor(2);

    // Toolbar: black, sharp edged, exact tab color roles from the screenshot.
    ImGui::SetCursorPos(ImVec2(0, headerHeight));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.035F, 0.035F, 0.035F, 1));
    ImGui::BeginChild("##toolbar", ImVec2(width, navHeight),
                      ImGuiChildFlags_None, noScroll);
    ImGui::SetCursorPos(ImVec2(12, 16));
    ImGui::TextUnformatted(icons::layers);
    ImGui::SameLine(0, 6);
    ImGui::TextDisabled("REGISTERED MODS");
    ImGui::SameLine(0, 10);
    ImGui::TextColored(ImVec4(0.16F, 0.61F, 1.0F, 1.0F), "%zu", mods.size());

    ImGui::SetCursorPos(ImVec2(sidebarWidth+9, 9));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.03F,0.03F,0.03F,1));
    ImGui::SetNextItemWidth((std::min)(255.0F, (std::max)(112.0F, width*0.233F)));
    ImGui::InputTextWithHint("##shellSearch", "Search mods or settings...",
                             search, sizeof(search));
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 11);
    const auto drawTab = [&](const char* name, Page destination,
                             ImVec4 fill, ImVec4 foreground, float tabWidth) {
        const bool active = page == destination;
        ImGui::PushStyleColor(ImGuiCol_Button, fill);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4((std::min)(fill.x+0.10F, 1.0F),
                   (std::min)(fill.y+0.10F, 1.0F),
                   (std::min)(fill.z+0.10F, 1.0F), 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, fill);
        ImGui::PushStyleColor(ImGuiCol_Text, foreground);
        if (ImGui::Button(name, ImVec2(tabWidth, 33)) && !active) {
            page = destination;
            g_capture.active = false;
            g_bindCaptureActive.store(false);
            g_confirmGroupReset = false;
            g_pendingModReset = 0;
        }
        if (active) {
            const ImVec2 left = ImGui::GetItemRectMin();
            const ImVec2 right = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(left.x, right.y+3.0F),
                ImVec2(right.x, right.y+3.0F), fill.x+fill.y+fill.z > 2.5F ?
                IM_COL32(255,255,255,255) : IM_COL32(245,188,42,255), 2.0F);
        }
        ImGui::PopStyleColor(4);
    };
    const std::string settingsTab = std::string(icons::sliders) + "  SETTINGS";
    const std::string keybindsTab = std::string(icons::keyboard) + "  KEYBINDS";
    const std::string profilesTab = std::string(icons::layers) + "  PROFILES";
    const std::string logsTab = std::string(icons::warning) + "  LOGS";
    const std::string aboutTab = std::string(icons::info) + "  ABOUT";
    drawTab(settingsTab.c_str(), Page::Mods,
            ImVec4(0.025F,0.37F,0.86F,1), ImVec4(1,1,1,1), 113);
    ImGui::SameLine(0, 10);
    drawTab(keybindsTab.c_str(), Page::Keybinds,
            ImVec4(0.98F,0.98F,0.98F,1), ImVec4(0.05F,0.05F,0.05F,1), 118);
    ImGui::SameLine(0, 10);
    drawTab(profilesTab.c_str(), Page::Profiles,
            ImVec4(0.06F,0.50F,0.28F,1), ImVec4(1,1,1,1), 115);
    ImGui::SameLine(0, 10);
    drawTab(logsTab.c_str(), Page::Logs,
            ImVec4(0.99F,0.71F,0.02F,1), ImVec4(0.07F,0.06F,0.01F,1), 82);
    ImGui::SameLine(0, 10);
    drawTab(aboutTab.c_str(), Page::About,
            ImVec4(0.90F,0.40F,0.20F,1), ImVec4(1,1,1,1), 93);
    ImGui::PopStyleVar();
    // Vertical rule separates the registered-mods column from the toolbar.
    const ImVec2 toolbarOrigin = ImGui::GetWindowPos();
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(toolbarOrigin.x+sidebarWidth, toolbarOrigin.y),
        ImVec2(toolbarOrigin.x+sidebarWidth, toolbarOrigin.y+navHeight),
        IM_COL32(75,75,75,255));
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Sidebar and settings content occupy separate fixed regions. The ImGui
    // children own scrolling, clipping and all input; no hand-drawn controls.
    ImGui::SetCursorPos(ImVec2(0, headerHeight+navHeight));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 5));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02F,0.02F,0.02F,1));
    ImGui::BeginChild("##modSidebar", ImVec2(sidebarWidth, contentHeight),
                      ImGuiChildFlags_Borders);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    for (const auto& mod : mods) {
        if (!ModMatches(mod, search, page == Page::Keybinds)) continue;
        const auto idText = std::to_string(mod.id);
        const std::string name = Label(mod.id);
        ImGui::PushID(idText.c_str());
        const bool isSelected = selected == mod.id;
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.13F,0.13F,0.13F,1));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.17F,0.17F,0.17F,1));
        if (!isSelected)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.52F,0.52F,0.52F,1));
        if (ImGui::Selectable(name.c_str(), isSelected,
                              ImGuiSelectableFlags_None, ImVec2(0, 49))) {
            selected = mod.id;
            if (page != Page::Mods && page != Page::Keybinds)
                page = Page::Mods;
            g_pendingModReset = 0;
            g_confirmGroupReset = false;
        }
        if (!isSelected) ImGui::PopStyleColor();
        if (isSelected) {
            // The yellow arrow is decoration only. The row remains an
            // ordinary focusable ImGui::Selectable, never a custom hitbox.
            const ImVec2 a = ImGui::GetItemRectMin();
            const ImVec2 b = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(b.x-24.0F, a.y+15.0F), IM_COL32(255,193,7,255), "<");
        }
        ImGui::PopStyleColor(2);
        ImGui::PopID();
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
    ImGui::SameLine(0, 0);

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(19, 18));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 9));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.012F,0.012F,0.012F,1));
    ImGui::BeginChild("##settingsContent", ImVec2(0, contentHeight),
                      ImGuiChildFlags_Borders);
    if (page == Page::Profiles) {
        DrawProfiles(registry);
    } else if (page == Page::Logs) {
        DrawLogs();
    } else if (page == Page::About) {
        if (HeadingFont()) ImGui::PushFont(HeadingFont());
        ImGui::TextUnformatted("About m1core");
        if (HeadingFont()) ImGui::PopFont();
        ImGui::Separator();
        ImGui::TextWrapped("Native RED4ext mod settings for Cyberpunk 2077.");
        ImGui::Text("Registered mods: %zu", mods.size());
        const auto shortcut = KeyLabel(menuKey);
        ImGui::Text("Open / close menu: %s", shortcut.c_str());
        ImGui::Text("Emergency disable: Ctrl + Shift + %s", shortcut.c_str());
        ImGui::Text("Mouse path: %s", CursorDetoursAvailable() ?
                    "Cyberpunk engine cursor" : "Engine cursor unavailable");
        ImGui::Spacing();
        ImGui::TextDisabled("Settings: red4ext/plugins/m1core/settings.json");
        ImGui::TextDisabled("Profiles: red4ext/plugins/m1core/profiles.json");
        ImGui::TextDisabled("Logs: red4ext/plugins/m1core/m1core.log");
    } else {
        if (page == Page::Keybinds) DrawKeybindConflicts(mods);
        const auto current = std::find_if(mods.begin(), mods.end(),
            [](const ModSnapshot& mod) { return mod.id == selected; });
        if (current == mods.end() ||
            !ModMatches(*current, search, page == Page::Keybinds)) {
            ImGui::TextDisabled("Select a registered mod on the left.");
        } else {
            if (HeadingFont()) ImGui::PushFont(HeadingFont());
            const auto name = Label(current->id);
            ImGui::TextUnformatted(name.c_str());
            if (HeadingFont()) ImGui::PopFont();
            ImGui::SameLine();
            ImGui::TextDisabled("(%zu settings)", current->settings.size());
            const bool pendingReset = g_pendingModReset == current->id;
            if (ImGui::Button(pendingReset ? "Confirm reset mod" :
                              "Reset mod to defaults")) {
                if (pendingReset) {
                    g_dirty |= registry.ResetMod(current->id);
                    g_pendingModReset = 0;
                    g_confirmGroupReset = false;
                } else {
                    g_pendingModReset = current->id;
                    g_confirmGroupReset = false;
                }
            }
            ImGui::Separator();
            ImGui::PushID(std::to_string(current->id).c_str());
            const std::string_view query = Matches(name, search) ?
                std::string_view{} : std::string_view(search);
            g_dirty |= DrawSettingsTable(registry, current->id,
                current->settings, query, page == Page::Keybinds);
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    // Compact functional footer, kept from the working framework. The
    // screenshot's empty bottom edge must not remove persistence controls.
    ImGui::SetCursorPos(ImVec2(0, height-footerHeight));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.035F,0.035F,0.035F,1));
    ImGui::BeginChild("##shellFooter", ImVec2(width, footerHeight),
                      ImGuiChildFlags_None, noScroll);
    ImGui::SetCursorPos(ImVec2(14, 16));
    if (g_dirty)
        ImGui::TextColored(ImVec4(1.0F,0.72F,0.05F,1), "Unsaved changes");
    else
        ImGui::TextDisabled("All changes saved");
    ImGui::SetCursorPos(ImVec2((std::max)(14.0F,width-402.0F), 8));
    if (ImGui::Button("Discard", ImVec2(86, 32))) {
        if (registry.Load()) { g_dirty=false; saveFailed=false; }
        else saveFailed=true;
    }
    ImGui::SameLine(0, 8);
    if (ui::Button("Save", ui::Tone::Primary, ImVec2(78,32))) {
        saveFailed = !registry.Save();
        if (!saveFailed) g_dirty=false;
    }
    ImGui::SameLine(0, 8);
    if (ImGui::Button("Save & Close", ImVec2(115,32))) {
        saveFailed = !registry.Save();
        if (!saveFailed) { g_dirty=false; open=false; }
    }
    ImGui::SameLine(0, 8);
    if (ImGui::Button("Close", ImVec2(80,32))) open=false;
    if (saveFailed) {
        ImGui::SetCursorPos(ImVec2(14, 2));
        ImGui::TextColored(ImVec4(1,0.35F,0.35F,1),
                           "Unable to save or reload settings.json");
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (interactive) g_dirty |= PollCapture(registry);
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
