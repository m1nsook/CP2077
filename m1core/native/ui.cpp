#include "ui.hpp"
#include "registry.hpp"
#include "input.hpp"

#include <imgui.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

namespace m1core {
namespace {
enum class Page { Settings, Keybinds, Profiles, Logs, About };
Page activePage = Page::Settings;
std::string activeMod;
char searchText[128]{};
char profileName[96] = "default";
std::string activeProfile;
bool confirmReset = false;

bool ColorTab(const char* text, Page page,
              ImVec4 background, ImVec4 foreground, float width)
{
    ImGui::PushStyleColor(ImGuiCol_Button, background);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
         ImVec4(std::min(1.0F,background.x+0.10F),
                std::min(1.0F,background.y+0.10F),
                std::min(1.0F,background.z+0.10F),1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, background);
    ImGui::PushStyleColor(ImGuiCol_Text, foreground);
    const bool pressed = ImGui::Button(text,ImVec2(width,34));
    ImGui::PopStyleColor(4);
    if (pressed) {
        activePage = page;
        confirmReset = false;
    }
    if (activePage == page) {
        const auto a = ImGui::GetItemRectMin();
        const auto b = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(a.x,b.y+2),ImVec2(b.x,b.y+2),IM_COL32(253,190,45,255),2);
    }
    return pressed;
}

void DrawSettingRow(const ModView& mod,const SettingView& setting)
{
    const std::string id = mod.id + "/" + setting.id;
    ImGui::PushID(id.c_str());
    ImGui::TableNextRow(ImGuiTableRowFlags_None,
                        setting.description.empty()?52.0F:70.0F);
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(setting.label.c_str());
    if (!setting.description.empty())
        ImGui::TextDisabled("%s",setting.description.c_str());

    ImGui::TableSetColumnIndex(1);
    const float controlWidth = std::max(1.0F,ImGui::GetContentRegionAvail().x);
    ImGui::SetNextItemWidth(controlWidth);
    switch (setting.kind) {
    case SettingKind::Boolean: {
        bool value = setting.value.get<bool>();
        if (ImGui::Checkbox("##value",&value))
            Settings().Set(mod.id,setting.id,value);
        break;
    }
    case SettingKind::Integer: {
        auto value = setting.value.get<int>();
        if (ImGui::SliderInt("##value",&value,
                static_cast<int>(setting.minimum),
                static_cast<int>(setting.maximum)))
            Settings().Set(mod.id,setting.id,value);
        break;
    }
    case SettingKind::Float: {
        float value = setting.value.get<float>();
        if (ImGui::SliderFloat("##value",&value,
                static_cast<float>(setting.minimum),
                static_cast<float>(setting.maximum),"%.2f"))
            Settings().Set(mod.id,setting.id,value);
        break;
    }
    case SettingKind::Choice: {
        const auto value = setting.value.get<std::string>();
        if (ImGui::BeginCombo("##value",value.c_str())) {
            for (const auto& choice : setting.choices) {
                if (ImGui::Selectable(choice.c_str(),value==choice))
                    Settings().Set(mod.id,setting.id,choice);
            }
            ImGui::EndCombo();
        }
        break;
    }
    }
    ImGui::TableSetColumnIndex(2);
    if (ImGui::Button("Reset",ImVec2(std::max(1.0F,ImGui::GetContentRegionAvail().x),32)))
        Settings().Reset(mod.id,setting.id);
    ImGui::PopID();
}

void DrawModSettings(const std::vector<ModView>& mods)
{
    const auto found = std::find_if(mods.begin(),mods.end(),
         [](const auto& mod) { return mod.id==activeMod; });
    if (found==mods.end()) {
        ImGui::TextDisabled("Select a mod from the sidebar.");
        return;
    }
    ImGui::TextUnformatted(found->title.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu settings)",found->settings.size());
    const float width = ImGui::GetContentRegionAvail().x;
    if (width >= 210) {
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x-190.0F);
        if (ImGui::Button(confirmReset?"Confirm mod reset":"Reset mod",ImVec2(180,32))) {
            if (confirmReset) {
                Settings().ResetMod(found->id);
                confirmReset=false;
            } else confirmReset=true;
        }
    }
    ImGui::Separator();
    constexpr ImGuiTableFlags flags=ImGuiTableFlags_BordersInnerH |
        ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame;
    if (ImGui::BeginTable("##settings",3,flags)) {
        ImGui::TableSetupColumn("Setting",ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthFixed,210);
        ImGui::TableSetupColumn("Reset",ImGuiTableColumnFlags_WidthFixed,80);
        for (const auto& setting : found->settings) {
            const std::string query=searchText;
            const std::string haystack = setting.label+" "+setting.description;
            if (!query.empty() && haystack.find(query)==std::string::npos &&
                found->title.find(query)==std::string::npos) continue;
            DrawSettingRow(*found,setting);
        }
        ImGui::EndTable();
    }
}

void DrawProfiles()
{
    ImGui::TextUnformatted("Profiles");
    ImGui::Separator();
    ImGui::SetNextItemWidth(260);
    ImGui::InputTextWithHint("##profile","Profile name",profileName,sizeof(profileName));
    ImGui::SameLine();
    if (ImGui::Button("Save profile",ImVec2(125,33))) {
        if (Settings().SaveProfile(profileName)) activeProfile=profileName;
    }
    for (const auto& name : Settings().Profiles()) {
        if (ImGui::Selectable(name.c_str(),activeProfile==name))
            activeProfile=name;
    }
    if (!activeProfile.empty()) {
        if (ImGui::Button("Load",ImVec2(90,32)))
            Settings().LoadProfile(activeProfile);
        ImGui::SameLine();
        if (ImGui::Button("Delete",ImVec2(90,32))) {
            Settings().DeleteProfile(activeProfile);
            activeProfile.clear();
        }
    }
}

void DrawKeybindSetup()
{
    ImGui::SetNextWindowSize(ImVec2(465,174),ImGuiCond_Always);
    const auto screen=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(screen->GetCenter(),ImGuiCond_Always,ImVec2(0.5F,0.5F));
    const ImGuiWindowFlags flags=ImGuiWindowFlags_NoCollapse |
         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
         ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("M1CORE - Set menu key",nullptr,flags)) {
        ImGui::TextWrapped("Assign a key to open and close M1CORE.");
        ImGui::TextDisabled("The game retains its own inputs until you activate setup.");
        ImGui::Spacing();
        if (InputRouter::Get().Recording())
            ImGui::TextUnformatted("Press a key (Esc to cancel)...");
        else if (ImGui::Button("Assign menu key",ImVec2(-1,34)))
            InputRouter::Get().BeginKeyCapture();
    }
    ImGui::End();
}

void DrawWindow()
{
    auto& input=InputRouter::Get();
    if (!input.KeyAssigned()) {
        DrawKeybindSetup();
        return;
    }
    const ImGuiViewport* screen=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(screen->GetCenter(),ImGuiCond_Always,ImVec2(0.5F,0.5F));
    ImGui::SetNextWindowSize(ImVec2(std::min(screen->Size.x-40.0F,1080.0F),
                                   std::min(screen->Size.y-40.0F,640.0F)),ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    const ImGuiWindowFlags flags=ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoSavedSettings;
    const bool shown=ImGui::Begin("##M1CORE",nullptr,flags);
    ImGui::PopStyleVar(2);
    if (!shown) { ImGui::End(); return; }

    const float width=ImGui::GetWindowSize().x;
    const float height=ImGui::GetWindowSize().y;
    ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(1,1,1,1));
    ImGui::BeginChild("##banner",ImVec2(width,48),ImGuiChildFlags_None);
    ImGui::SetCursorPos(ImVec2(20,13));
    ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.04F,0.04F,0.04F,1));
    ImGui::TextUnformatted("M1CORE");
    ImGui::PopStyleColor();
    ImGui::SetCursorPos(ImVec2(width-96,8));
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0.04F,0.04F,0.04F,1));
    if (ImGui::Button("CLOSE",ImVec2(86,32)))
        input.Close();
    ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    const auto mods=Settings().Snapshot();
    ImGui::SetCursorPos(ImVec2(0,48));
    ImGui::BeginChild("##tabs",ImVec2(width,50),ImGuiChildFlags_None);
    ImGui::SetCursorPos(ImVec2(14,9));
    ColorTab("SETTINGS",Page::Settings,{0.03F,0.37F,0.86F,1},{1,1,1,1},114);
    ImGui::SameLine();
    ColorTab("KEYBINDS",Page::Keybinds,{0.98F,0.98F,0.98F,1},{0.05F,0.05F,0.05F,1},114);
    ImGui::SameLine();
    ColorTab("PROFILES",Page::Profiles,{0.04F,0.50F,0.27F,1},{1,1,1,1},114);
    ImGui::SameLine();
    ColorTab("LOGS",Page::Logs,{0.98F,0.71F,0.02F,1},{0.05F,0.05F,0.05F,1},90);
    ImGui::SameLine();
    ColorTab("ABOUT",Page::About,{0.90F,0.41F,0.18F,1},{1,1,1,1},90);
    ImGui::EndChild();

    const float body=height-98.0F;
    ImGui::SetCursorPos(ImVec2(0,98));
    ImGui::BeginChild("##sidebar",ImVec2(220,body),ImGuiChildFlags_Borders);
    ImGui::TextDisabled("REGISTERED MODS (%zu)",mods.size());
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##filter","Search...",searchText,sizeof(searchText));
    ImGui::Separator();
    for (const auto& mod : mods) {
        if (ImGui::Selectable(mod.title.c_str(),activeMod==mod.id))
            activeMod=mod.id;
    }
    ImGui::EndChild();
    ImGui::SameLine(0,0);
    ImGui::BeginChild("##content",ImVec2(0,body),ImGuiChildFlags_Borders);
    ImGui::Spacing();
    if (activePage==Page::Settings) DrawModSettings(mods);
    else if (activePage==Page::Profiles) DrawProfiles();
    else if (activePage==Page::Keybinds) {
        ImGui::TextUnformatted("Hotkeys");
        ImGui::Separator();
        ImGui::Text("Current menu key: %s",input.KeyDescription().c_str());
        if (ImGui::Button("Rebind menu key",ImVec2(180,34))) input.BeginKeyCapture();
        if (input.Recording()) ImGui::TextDisabled("Press a key (Esc to cancel)");
    } else if (activePage==Page::Logs) {
        ImGui::TextDisabled("Standalone RED4ext backend. No CET runtime.");
        if (!Settings().LastError().empty())
            ImGui::TextWrapped("%s",Settings().LastError().c_str());
    } else {
        ImGui::TextUnformatted("M1CORE (development)");
        ImGui::TextWrapped("Native RED4ext framework using Dear ImGui.");
        ImGui::TextDisabled("No Cyber Engine Tweaks required.");
    }
    ImGui::EndChild();
    ImGui::End();
}
} // namespace

void DrawNativeSettingsUI()
{
    if (InputRouter::Get().ShouldDraw()) DrawWindow();
}
} // namespace m1core
