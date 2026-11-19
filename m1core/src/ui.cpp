#include "ui.hpp"
#include "settings.hpp"

#include <RED4ext/CName.hpp>
#include <RED4ext/CNamePool.hpp>
#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>

namespace m1core
{
namespace
{
std::string Label(Id id)
{
    const auto* text = RED4ext::CNamePool::Get(RED4ext::CName(id));
    if (text && *text)
        return text;
    char fallback[25]{};
    std::snprintf(fallback, sizeof(fallback), "0x%016llx", static_cast<unsigned long long>(id));
    return fallback;
}

bool Matches(std::string_view text, std::string_view search)
{
    if (search.empty())
        return true;
    const auto at = std::search(text.begin(), text.end(), search.begin(), search.end(),
        [](unsigned char lhs, unsigned char rhs) {
            return std::tolower(lhs) == std::tolower(rhs);
        });
    return at != text.end();
}

void DrawSetting(Registry& registry, Id modId, const SettingSnapshot& setting)
{
    const auto label = Label(setting.id);
    const auto stableId = std::to_string(setting.id);
    ImGui::PushID(stableId.c_str());
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label.c_str());
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-56.0F);

    switch (setting.kind)
    {
    case Kind::Boolean:
    {
        bool value = std::get<bool>(setting.value);
        if (ImGui::Checkbox("##value", &value))
            registry.SetBool(modId, setting.id, value);
        break;
    }
    case Kind::Integer:
    {
        int value = std::get<std::int32_t>(setting.value);
        if (ImGui::SliderInt("##value", &value, std::get<std::int32_t>(setting.minimum),
                             std::get<std::int32_t>(setting.maximum)))
            registry.SetInt(modId, setting.id, value);
        break;
    }
    case Kind::Float:
    {
        float value = std::get<float>(setting.value);
        if (ImGui::SliderFloat("##value", &value, std::get<float>(setting.minimum),
                               std::get<float>(setting.maximum), "%.2f"))
            registry.SetFloat(modId, setting.id, value);
        break;
    }
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("Reset"))
        registry.Reset(modId, setting.id);
    ImGui::PopID();
}
} // namespace

void RenderSettings(Registry& registry, bool& open)
{
    // Layout prototype only. Final colors/fonts/visual appearance are undecided.
    // The D3D12 presentation and safe focus/input layer will call this later.
    static char query[160]{};
    static Id selected = 0;
    static bool saveFailed = false;

    const auto mods = registry.Snapshot();
    if (mods.empty())
        selected = 0;
    else if (std::none_of(mods.begin(), mods.end(),
        [](const ModSnapshot& mod) { return mod.id == selected; }))
        selected = mods.front().id;

    ImGui::SetNextWindowSize(ImVec2(900.0F, 580.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("m1core - Mod Settings", &open, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputTextWithHint("##search", "Search mods and settings", query, sizeof(query));
    ImGui::Separator();

    ImGui::BeginChild("mods_sidebar", ImVec2(220.0F, -46.0F), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("Registered mods");
    ImGui::Separator();
    for (const auto& mod : mods)
    {
        const auto name = Label(mod.id);
        bool visible = Matches(name, query);
        if (!visible)
            for (const auto& setting : mod.settings)
                if (Matches(Label(setting.id), query))
                    visible = true;
        if (!visible)
            continue;

        const auto stableId = std::to_string(mod.id);
        ImGui::PushID(stableId.c_str());
        if (ImGui::Selectable(name.c_str(), selected == mod.id))
            selected = mod.id;
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("mod_settings", ImVec2(0.0F, -46.0F), ImGuiChildFlags_Borders);
    const auto active = std::find_if(mods.begin(), mods.end(),
        [](const ModSnapshot& item) { return item.id == selected; });
    if (active == mods.end())
        ImGui::TextDisabled("No mods have registered settings yet.");
    else
    {
        const auto name = Label(active->id);
        ImGui::TextUnformatted(name.c_str());
        ImGui::Separator();
        if (ImGui::BeginTable("settings_table", 2,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerH))
        {
            ImGui::TableSetupColumn("Setting", ImGuiTableColumnFlags_WidthStretch, 0.45F);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.55F);
            ImGui::TableHeadersRow();
            for (const auto& setting : active->settings)
                if (Matches(name, query) || Matches(Label(setting.id), query))
                    DrawSetting(registry, active->id, setting);
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    ImGui::Separator();
    if (ImGui::Button("Save settings"))
        saveFailed = !registry.Save();
    ImGui::SameLine();
    if (ImGui::Button("Close"))
        open = false;
    ImGui::SameLine();
    if (saveFailed)
        ImGui::TextUnformatted("Save failed. Check plugin directory permissions.");
    else
        ImGui::TextDisabled("Apply values immediately, save to settings.json");
    ImGui::End();
}
} // namespace m1core
