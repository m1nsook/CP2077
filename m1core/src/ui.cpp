#include "ui.hpp"
#include "settings.hpp"

#include <RED4ext/CName.hpp>
#include <RED4ext/CNamePool.hpp>
#include <imgui.h>
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

namespace m1core {
namespace {
enum class Page { Mods, Keybinds, About };

struct BindCapture {
    bool active = false;
    Id mod = 0;
    Id setting = 0;
    std::array<bool, 256> wasDown{};
};

BindCapture g_capture;

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
    g_capture.mod = mod;
    g_capture.setting = id;
    for (int key = 0; key < 256; ++key)
        g_capture.wasDown[key] = (GetAsyncKeyState(key) & 0x8000) != 0;
}

void PollCapture(Registry& registry)
{
    if (!g_capture.active) return;
    for (int key = 7; key < 256; ++key) {
        const bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
        const bool rising = down && !g_capture.wasDown[key];
        g_capture.wasDown[key] = down;
        if (!rising) continue;
        // Never steal the overlay's reserved open/close key or a system
        // shortcut; Escape only cancels *keybind capture*, never m1core itself.
        if (key == VK_ESCAPE) {
            g_capture.active = false;
            return;
        }
        if (key == VK_F11 || key == VK_LWIN || key == VK_RWIN ||
            key == VK_MENU || key == VK_LMENU || key == VK_RMENU)
            continue;
        registry.SetKeybind(g_capture.mod, g_capture.setting, key);
        g_capture.active = false;
        return;
    }
}

bool DrawSetting(Registry& registry, Id mod, const SettingSnapshot& setting)
{
    const auto stableId = std::to_string(mod) + ":" + std::to_string(setting.id);
    ImGui::PushID(stableId.c_str());
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    const auto name = Label(setting.id);
    ImGui::TextUnformatted(name.c_str());
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
        const auto label = capturing ? std::string("Press key... (Esc cancels)")
                                    : KeyLabel(std::get<std::int32_t>(setting.value));
        if (ImGui::Button(label.c_str(), ImVec2(width, 0.0F)))
            StartCapture(mod, setting.id);
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            if (capturing) g_capture.active = false;
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
    if (ImGui::BeginTable("settings_table", 2, ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerH)) {
        ImGui::TableSetupColumn("Setting", ImGuiTableColumnFlags_WidthStretch, 0.45F);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.55F);
        ImGui::TableHeadersRow();
        for (const auto& setting : settings) {
            if (onlyKeybinds && setting.kind != Kind::Keybind) continue;
            if (!Matches(Label(setting.id), query)) continue;
            changed |= DrawSetting(registry, mod, setting);
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
        if (Matches(Label(setting.id), query)) return true;
    }
    return hasMatchingType && Matches(Label(mod.id), query);
}
} // namespace

void RenderSettings(Registry& registry, bool& open)
{
    // Keep the functional layout from the supplied prototype, not its colors.
    static char search[160]{};
    static Id selected = 0;
    static Page page = Page::Mods;
    static bool saveFailed = false;
    static bool dirty = false;

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

    ImGui::SetNextItemWidth((std::max)(150.0F, ImGui::GetContentRegionAvail().x * 0.42F));
    ImGui::InputTextWithHint("##search", "Search mods and settings", search, sizeof(search));
    ImGui::SameLine();
    auto tab = [&page](const char* name, Page value) {
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
    tab("About", Page::About);
    ImGui::Separator();

    const bool keybindsOnly = page == Page::Keybinds;
    if (page != Page::About) {
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
            ImGui::Separator();
            ImGui::PushID(std::to_string(current->id).c_str());
            // If the module name is what matched, show its full setting list.
            const std::string_view filter = Matches(name, search) ? std::string_view{} : search;
            dirty |= DrawSettingsTable(registry, current->id, current->settings, filter, keybindsOnly);
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
        ImGui::TextUnformatted("Open / close overlay: F11");
        ImGui::TextUnformatted("Emergency disable: Ctrl + Shift + F11");
        ImGui::TextUnformatted("Settings file: red4ext/plugins/m1core/settings.json");
        ImGui::Spacing();
        ImGui::TextDisabled("Development preview. The overlay visuals and additional setting types are still evolving.");
        ImGui::EndChild();
    }

    // Footer is a functional action area. Apply = immediate; Save persists to
    // JSON. The normal F11 close is handled by the overlay input gate.
    ImGui::Separator();
    if (dirty) ImGui::TextUnformatted("Unsaved changes");
    else ImGui::TextDisabled("Settings applied in memory");
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        saveFailed = !registry.Save();
        if (!saveFailed) dirty = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save & Close")) {
        saveFailed = !registry.Save();
        if (!saveFailed) { dirty = false; open = false; }
    }
    ImGui::SameLine();
    if (ImGui::Button("Close")) open = false;
    if (saveFailed) ImGui::TextUnformatted("Failed saving settings.json");
    PollCapture(registry);
    ImGui::End();
}
} // namespace m1core
