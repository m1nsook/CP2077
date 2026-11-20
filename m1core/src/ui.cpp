#include "ui.hpp"
#include "settings.hpp"
#include "diagnostics.hpp"

#include <RED4ext/CName.hpp>
#include <RED4ext/CNamePool.hpp>
#include <imgui.h>
#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <cctype>
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
            registry.Save(); // The very first menu key survives restart immediately.
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
                ImGui::PushID(static_cast<int>(group));
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
    constexpr const char* levels[] = {"All", "Debug", "Info", "Warning", "Error"};

    ImGui::BeginChild("diagnostics_panel", ImVec2(0.0F, -66.0F), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("M1CORE DEBUG LOG");
    ImGui::SameLine();
    ImGui::TextDisabled("In-game events and native mod diagnostics");
    ImGui::Separator();
    ImGui::SetNextItemWidth(155.0F);
    ImGui::Combo("Minimum severity", &minimum, levels, IM_ARRAYSIZE(levels));
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
    if (ImGui::SmallButton("Copy visible")) {
        std::string copied;
        for (const auto& line : lines) {
            copied += line;
            copied += '\n';
        }
        ImGui::SetClipboardText(copied.c_str());
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear panel"))
        Diagnostics::Get().ClearView();
    ImGui::TextDisabled("Disk log: red4ext/plugins/m1core/m1core.log (retained when panel clears)");
    ImGui::Separator();

    ImGui::BeginChild("log_lines", ImVec2(0.0F, 0.0F), ImGuiChildFlags_Borders);
    if (lines.empty()) {
        ImGui::TextDisabled("No events match the filter.");
    } else {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(lines.size()));
        while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                ImGui::TextUnformatted(lines[static_cast<std::size_t>(i)].c_str());
    }
    ImGui::EndChild();
    ImGui::EndChild();
}

void DrawProfiles(Registry& registry)
{
    const auto names = registry.ListProfiles();
    if (!g_selectedProfile.empty() &&
        std::find(names.begin(), names.end(), g_selectedProfile) == names.end())
        g_selectedProfile.clear();

    ImGui::BeginChild("profiles_panel", ImVec2(0.0F, -66.0F), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("SETTINGS PROFILES");
    ImGui::Separator();
    ImGui::TextWrapped("Save different combinations of mod settings and keybinds, then restore them with one click.");
    ImGui::Spacing();

    ImGui::Text("Saved profiles: %zu / 24", names.size());
    ImGui::SetNextItemWidth((std::max)(220.0F, ImGui::GetContentRegionAvail().x * 0.60F));
    const auto selectedLabel = g_selectedProfile.empty() ? "Select a profile..." : g_selectedProfile.c_str();
    if (ImGui::BeginCombo("Saved profile", selectedLabel)) {
        for (const auto& name : names) {
            if (ImGui::Selectable(name.c_str(), g_selectedProfile == name)) {
                g_selectedProfile = name;
                std::snprintf(g_renameProfileName, sizeof(g_renameProfileName), "%s", name.c_str());
                g_confirmOverwrite = false;
                g_confirmDelete = false;
                g_profileMessage.clear();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SetNextItemWidth((std::max)(220.0F, ImGui::GetContentRegionAvail().x * 0.60F));
    ImGui::InputTextWithHint("New profile name", "e.g. Stealth, Combat, Exploration",
                             g_newProfileName, sizeof(g_newProfileName));

    if (ImGui::Button("Create from current settings")) {
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

    if (ImGui::Button("Load selected")) {
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
        g_confirmOverwrite = false;
        g_confirmDelete = false;
    }
    ImGui::SameLine();

    if (ImGui::Button(g_confirmOverwrite ? "Confirm overwrite" : "Overwrite selected")) {
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

    if (ImGui::Button(g_confirmDelete ? "Confirm delete" : "Delete selected")) {
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
    if (ImGui::Button("Rename selected")) {
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
    // Keep the functional layout from the supplied prototype, not its colors.
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

    ImGui::SetNextWindowSize(ImVec2(940.0F, 600.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("m1core - Mod Settings", &open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted("m1core");
    ImGui::SameLine();
    ImGui::TextDisabled("Mod Settings");
    ImGui::Separator();
    if (menuKey == 0) {
        ImGui::TextWrapped("No m1core menu key is assigned. Choose one now: "
                           "Keybinds > m1core > Open / Close Menu. "
                           "The menu opens automatically when no key has ever been assigned.");
        if (ImGui::Button("Bind my menu key"))
            StartCapture(coreMod.hash, coreToggle.hash);
        if (g_capture.active && g_capture.mod == coreMod.hash &&
            g_capture.setting == coreToggle.hash) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Cancel key selection")) {
                g_capture.active = false;
                g_bindCaptureActive.store(false);
            }
        }
        ImGui::Separator();
    }

    ImGui::SetNextItemWidth((std::max)(150.0F, ImGui::GetContentRegionAvail().x * 0.42F));
    ImGui::InputTextWithHint("##search", "Search mods and settings", search, sizeof(search));
    ImGui::SameLine();
    auto tab = [](const char* name, Page value) {
        if (ImGui::Button(name)) page = value;
        if (page == value) {
            const auto a = ImGui::GetItemRectMin();
            const auto b = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddLine(ImVec2(a.x, b.y), b, IM_COL32(220, 220, 220, 200), 2.0F);
        }
    };
    tab("All Mods", Page::Mods);
    ImGui::SameLine();
    tab("Keybinds", Page::Keybinds);
    ImGui::SameLine();
    tab("Profiles", Page::Profiles);
    ImGui::SameLine();
    tab("Logs", Page::Logs);
    ImGui::SameLine();
    tab("About", Page::About);
    ImGui::Separator();

    const bool keybindsOnly = page == Page::Keybinds;
    if (page == Page::Profiles) {
        DrawProfiles(registry);
    } else if (page == Page::Logs) {
        DrawLogs();
    } else if (page != Page::About) {
        ImGui::BeginChild("mods_sidebar", ImVec2(220.0F, -66.0F), ImGuiChildFlags_Borders);
        ImGui::TextDisabled("REGISTERED MODS");
        ImGui::Separator();
        for (const auto& mod : mods) {
            if (!ModMatches(mod, search, keybindsOnly)) continue;
            std::size_t count = 0;
            for (const auto& setting : mod.settings)
                if (!keybindsOnly || setting.kind == Kind::Keybind) ++count;
            const auto name = Label(mod.id) + " (" + std::to_string(count) + ")";
            ImGui::PushID(std::to_string(mod.id).c_str());
            if (ImGui::Selectable(name.c_str(), selected == mod.id))
                selected = mod.id;
            ImGui::PopID();
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("mod_settings", ImVec2(0.0F, -66.0F), ImGuiChildFlags_Borders);
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
            if (ImGui::SmallButton(pendingReset ? "Confirm reset mod" : "Reset mod to defaults")) {
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
            // If the module name is what matched, show its full setting list.
            const std::string_view filter = Matches(name, search) ? std::string_view{} : search;
            g_dirty |= DrawSettingsTable(registry, current->id, current->settings, filter, keybindsOnly);
            ImGui::PopID();
        }
        ImGui::EndChild();
    } else {
        ImGui::BeginChild("about", ImVec2(0.0F, -66.0F), ImGuiChildFlags_Borders);
        ImGui::TextUnformatted("m1core native settings framework");
        ImGui::Separator();
        ImGui::TextWrapped("A single RED4ext plugin hosts the ImGui settings interface and persistent configuration for compatible Cyberpunk 2077 mods.");
        ImGui::Spacing();
        ImGui::Text("Registered mods: %zu", mods.size());
        const auto shortcut = KeyLabel(menuKey);
        ImGui::Text("Open / close overlay: %s", shortcut.c_str());
        ImGui::Text("Emergency disable: Ctrl + Shift + %s", shortcut.c_str());
        ImGui::TextDisabled("No menu key is preset; assign it in Keybinds > m1core.");
        ImGui::TextUnformatted("Settings file: red4ext/plugins/m1core/settings.json");
        ImGui::Spacing();
        ImGui::TextDisabled("Development preview. The overlay visuals and additional setting types are still evolving.");
        ImGui::EndChild();
    }

    // Footer is a functional action area. Apply = immediate; Save persists to
    // JSON. Closing with the player-selected key uses the overlay input gate.
    ImGui::Separator();
    if (g_dirty) ImGui::TextUnformatted("Unsaved changes");
    else ImGui::TextDisabled("Settings applied in memory");
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        saveFailed = !registry.Save();
        if (!saveFailed) g_dirty = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save & Close")) {
        saveFailed = !registry.Save();
        if (!saveFailed) { g_dirty = false; open = false; }
    }
    ImGui::SameLine();
    if (ImGui::Button("Close")) open = false;
    if (saveFailed) ImGui::TextUnformatted("Failed saving settings.json");
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
