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

// Console has its own page. Logs stays a read-only output viewer.
char luaCommand[4096]{};
std::vector<std::string> luaCommandHistory;
std::size_t luaHistoryIndex = 0;
std::string luaConsoleStatus;
std::string consoleSnapshot;
bool consoleLoaded = false;
bool consoleShouldScroll = false;
char keybindSearch[128]{};
std::string selectedKeybindMod = "m1core";
std::string activeProfile = "default";
enum class Page { Settings, Keybinds, Profiles, Console, Logs, About };
Page currentPage = Page::Settings;

ImVec4 Color(int r, int g, int b, float a = 1.0f)
{
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a);
}
constexpr float kNavWidth = 240.0f;
constexpr float kTitleHeight = 48.0f;
constexpr float kPageHeaderHeight = 82.0f;
void MoveDown(float px) { ImGui::SetCursorPosY(ImGui::GetCursorPosY()+px); }
void PushLayoutFont(ImFont* f) { if (f) ImGui::PushFont(f); }
void PopLayoutFont(ImFont* f) { if (f) ImGui::PopFont(); }
std::string ToUppercaseASCII(std::string name) {
    for (char& c : name)
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return name;
}



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

// Up/down uses the in-session command history. Commands are never executed
// by arrow-key navigation; only Enter or the explicit Run button can do so.
int LuaCommandHistoryCallback(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag != ImGuiInputTextFlags_CallbackHistory ||
        luaCommandHistory.empty())
        return 0;

    if (data->EventKey == ImGuiKey_UpArrow)
    {
        if (luaHistoryIndex > 0) --luaHistoryIndex;
    }
    else if (data->EventKey == ImGuiKey_DownArrow)
    {
        if (luaHistoryIndex < luaCommandHistory.size()) ++luaHistoryIndex;
    }
    else return 0;

    const char* selected = luaHistoryIndex == luaCommandHistory.size()
        ? "" : luaCommandHistory[luaHistoryIndex].c_str();
    data->DeleteChars(0, data->BufTextLen);
    data->InsertChars(0, selected);
    return 0;
}

void ExecuteLuaConsoleCommand()
{
    std::string command(luaCommand);
    const auto first = command.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return;
    const auto last = command.find_last_not_of(" \t\r\n");
    command = command.substr(first, last - first + 1);

    // Only user-submitted commands execute. Nothing runs automatically on
    // startup, reload, frame update, or text edit.
    auto& vm = M1CoreRuntime::Get().GetVM();
    if (!vm.IsInitialized())
    {
        luaConsoleStatus = "Game scripting is not ready yet.";
        return;
    }

    if (const auto logger = spdlog::get("scripting"))
        logger->info("> {}", command);

    const bool success = vm.ExecuteLua(command);
    // Release DLL logging buffers informational print() messages. Flush so
    // the file-backed Logs page shows the command result in this frame.
    if (const auto logger = spdlog::get("scripting"))
        logger->flush();
    luaConsoleStatus = success ? "Lua command executed."
                               : "Lua command failed. Read scripting.log below.";

    if (luaCommandHistory.empty() || luaCommandHistory.back() != command)
    {
        if (luaCommandHistory.size() >= 50)
            luaCommandHistory.erase(luaCommandHistory.begin());
        luaCommandHistory.push_back(command);
    }
    luaHistoryIndex = luaCommandHistory.size();
    luaCommand[0] = '\0';

    // Console uses its own log snapshot, without changing Logs selection.
    consoleLoaded = false;
    consoleShouldScroll = true;
}

void DrawLogsPage()
{
    const char* sources[] = {"m1core.log", "scripting.log"};
    ImGui::SetNextItemWidth(190.0f);
    if (ImGui::Combo("##log-file", &logSource, sources, IM_ARRAYSIZE(sources)))
        logLoaded = false;
    ImGui::SameLine();
    if (ImGui::Button(m1core::HasIcons() ? M1CORE_ICON_REFRESH " Refresh" : "Refresh"))
        logLoaded = false;
    ImGui::SameLine();
    if (ImGui::Button(m1core::HasIcons() ? M1CORE_ICON_REFRESH " Reload Scripts" : "Reload Scripts"))
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
    MoveDown(10);
    ImGui::PushStyleColor(ImGuiCol_ChildBg,Color(10,10,12));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(16,16));
    if (ImGui::BeginChild("##m1core-log-output",ImVec2(0,0),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                          ImGuiWindowFlags_HorizontalScrollbar))
    {
        if (ImFont* font = m1core::LogFont())
            ImGui::PushFont(font);
        ImGui::TextUnformatted(logSnapshot.c_str());
        if (m1core::LogFont())
            ImGui::PopFont();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void DrawConsolePage()
{
    if (ImGui::Button(m1core::HasIcons() ? M1CORE_ICON_REFRESH " Refresh output" : "Refresh output"))
    {
        consoleLoaded = false;
        consoleShouldScroll = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Live Lua console  |  Up/Down recalls commands");

    if (!consoleLoaded)
    {
        consoleSnapshot = ReadLogTail(
            M1CoreRuntime::Get().GetPaths().CoreRoot() / L"scripting.log");
        consoleLoaded = true;
    }

    // A dedicated scrolling output region above the command input. This
    // reuses the approved ImGui styling and JetBrains Mono log font.
    const float footer = ImGui::GetFrameHeight() +
        ImGui::GetTextLineHeightWithSpacing() +
        ImGui::GetStyle().ItemSpacing.y * 3.0f;
    MoveDown(5);
    ImGui::PushStyleColor(ImGuiCol_ChildBg,Color(10,10,12));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(16,16));
    if (ImGui::BeginChild("##m1core-console-output",
                          ImVec2(0, -footer),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                          ImGuiWindowFlags_HorizontalScrollbar))
    {
        if (ImFont* font = m1core::LogFont())
            ImGui::PushFont(font);
        ImGui::TextUnformatted(consoleSnapshot.c_str());
        if (m1core::LogFont())
            ImGui::PopFont();
        if (consoleShouldScroll)
        {
            ImGui::SetScrollHereY(1.0f);
            consoleShouldScroll = false;
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    auto& vm = M1CoreRuntime::Get().GetVM();
    const bool luaReady = vm.IsInitialized();
    ImGui::BeginDisabled(!luaReady);
    ImGui::SetNextItemWidth(-110.0f);
    const bool pressedEnter = ImGui::InputTextWithHint(
        "##m1core-lua-command", "Enter Lua command...",
        luaCommand, sizeof(luaCommand),
        ImGuiInputTextFlags_EnterReturnsTrue |
        ImGuiInputTextFlags_CallbackHistory,
        &LuaCommandHistoryCallback);
    ImGui::SameLine();
    const bool clickedRun = ImGui::Button("Run Lua", ImVec2(98, 0));
    ImGui::EndDisabled();

    if (pressedEnter || clickedRun)
    {
        ExecuteLuaConsoleCommand();
        // The executor flushed the logger. Show result on this frame,
        // rather than requiring a separate read-only Logs page refresh.
        consoleSnapshot = ReadLogTail(
            M1CoreRuntime::Get().GetPaths().CoreRoot() / L"scripting.log");
        consoleLoaded = true;
    }

    if (!luaReady)
        ImGui::TextDisabled("Game scripting is not initialized.");
    else if (!luaConsoleStatus.empty())
        ImGui::TextDisabled("%s", luaConsoleStatus.c_str());
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
    const char* label = m1core::HasIcons() ? M1CORE_ICON_RESET "##reset-setting" : "Reset##reset-setting";
    if (ImGui::Button(label, ImVec2(-FLT_MIN, 0.0f)))
    {
        if (!Settings().Reset(mod.id, setting.id))
        {
            const auto detail = Settings().LastError();
            notice = detail.empty() ? "Could not reset setting." : "Could not reset setting: " + detail;
        }
        else
            notice.clear();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Reset %s to default",setting.label.c_str());
    ImGui::PopID();
}

void DrawSettingsFor(const ModView& mod)
{
    // Keep the mod title aligned with a normal-height action at the
    // right edge, instead of squeezing the button against the title.
    const char* resetLabel = confirmModReset ? "Confirm Reset" :
        (m1core::HasIcons() ? M1CORE_ICON_RESET "  Reset Mod" : "Reset Mod");
    const float actionWidth = ImGui::CalcTextSize(resetLabel).x +
                              ImGui::GetStyle().FramePadding.x * 2.0f;
    const float rightEdge = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
    ImGui::AlignTextToFramePadding();
    auto* headingFont = m1core::KarlaSectionTitle() ?
        m1core::KarlaSectionTitle() : m1core::KarlaSemiBold();
    PushLayoutFont(headingFont);
    ImGui::Text("%s (%zu settings)", mod.title.c_str(), mod.settings.size());
    PopLayoutFont(headingFont);
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
    MoveDown(16);

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
        ImGui::PushStyleColor(ImGuiCol_Text,Color(100,116,139));
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextAlign,ImVec2(0,0.5f));
        PushLayoutFont(m1core::KarlaCaption());
        const auto heading = ToUppercaseASCII(group);
        ImGui::SeparatorText(heading.c_str());
        PopLayoutFont(m1core::KarlaCaption());
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        MoveDown(12);
        ImGui::PushID(group.c_str());
        const ImGuiTableFlags flags =
            ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_SizingStretchProp;
        // The global theme uses 10px cell padding. Narrow that only
        // for settings rows: full-height buttons shouldn't sit inside
        // unnecessarily tall empty table cells.
        const auto cellPadding = ImGui::GetStyle().CellPadding;
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(cellPadding.x, 10.0f));
        if (ImGui::BeginTable("settings", 3, flags)) {
            const float available = ImGui::GetContentRegionAvail().x;
            ImGui::TableSetupColumn("Setting",ImGuiTableColumnFlags_WidthStretch,1.0f);
            ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthFixed,
                                    std::min(240.0f,std::max(110.0f,available*0.43f)));
            ImGui::TableSetupColumn("Action",ImGuiTableColumnFlags_WidthFixed,80.0f);
            for (const auto* setting : rows) DrawSetting(mod, *setting);
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
        ImGui::PopID();
    }
}


void DrawSettingsPage()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(12,10));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1.0f);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##settings-search",
        m1core::HasIcons() ? M1CORE_ICON_SEARCH "  Search mods or settings..." :
                             "Search mods or settings...", search, sizeof(search));
    ImGui::PopStyleVar(2);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Values save automatically to m1core/settings.json.");
    MoveDown(12);

    const auto mods = Settings().Snapshot();
    if (selectedMod.empty() && !mods.empty())
        selectedMod = mods.front().id;
    const float remainingWidth = ImGui::GetContentRegionAvail().x;
    const float modListWidth = std::min(240.0f, std::max(170.0f, remainingWidth * 0.29f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(16,14));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,Color(30,31,35));
    if (ImGui::BeginChild("##settings-mod-list",ImVec2(modListWidth,0),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding))
    {
        PushLayoutFont(m1core::KarlaCaption());
        ImGui::TextColored(Color(100,116,139),"REGISTERED MODS (%zu)",mods.size());
        PopLayoutFont(m1core::KarlaCaption());
        MoveDown(7);
        ImGui::Separator();
        MoveDown(7);
        bool anyVisible = false;
        for (const auto& mod : mods)
        {
            if (!Matches(mod, search)) continue;
            anyVisible = true;
            ImGui::PushID(mod.id.c_str());
            const bool selected = selectedMod == mod.id;
            ImGui::PushStyleColor(ImGuiCol_Header,Color(35,48,72));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered,Color(42,44,49));
            ImGui::PushStyleColor(ImGuiCol_Text,selected ? Color(96,165,250) : Color(148,163,184));
            PushLayoutFont(m1core::KarlaMedium());
            if (ImGui::Selectable(mod.title.c_str(),selected,0,ImVec2(0,35))) {
                selectedMod = mod.id;
                confirmModReset = false;
            }
            PopLayoutFont(m1core::KarlaMedium());
            ImGui::PopStyleColor(3);
            ImGui::PopID();
        }
        if (!anyVisible)
            ImGui::TextDisabled("No matching mods");
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    ImGui::SameLine(0.0f,20.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(24,20));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,Color(30,31,35));
    if (ImGui::BeginChild("##settings-detail",ImVec2(0,0),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding))
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
    ImGui::PopStyleVar();
}



void DrawKeybindsPage()
{
    auto& runtime = M1CoreRuntime::Get();
    const auto& bindings = runtime.GetBindings();
    const auto& registered = runtime.GetVM().GetAllBinds();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(12,10));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1.0f);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##keybind-search",
        m1core::HasIcons() ? M1CORE_ICON_SEARCH "  Search mods or keybinds..." :
                             "Search mods or keybinds...", keybindSearch, sizeof(keybindSearch));
    ImGui::PopStyleVar(2);
    MoveDown(12);

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

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(16,14));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,Color(30,31,35));
    if (ImGui::BeginChild("##keybind-mod-list",ImVec2(panelWidth,0),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding))
    {
        PushLayoutFont(m1core::KarlaCaption());
        ImGui::TextColored(Color(100,116,139),"REGISTERED MODS (%zu)",visibleMods.size());
        PopLayoutFont(m1core::KarlaCaption());
        MoveDown(7);
        ImGui::Separator();
        MoveDown(7);
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
    ImGui::PopStyleVar();

    ImGui::SameLine(0,20);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(24,20));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,Color(30,31,35));
    if (ImGui::BeginChild("##keybind-detail",ImVec2(0,0),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding))
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
    ImGui::PopStyleVar();
}


void DrawProfilesPage()
{
    // PageHeader already supplies the title; keep this area for actions.
    MoveDown(6);
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(12,10));
    if (ImGui::BeginChild("##profile-list", ImVec2(0,220),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding)) {
        for (const auto& name : Settings().Profiles()) {
            if (ImGui::Selectable(name.c_str(), selectedProfile == name))
                selectedProfile = name;
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    MoveDown(14);
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
    case Page::Console: return "Lua Console";
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
    case Page::Console: return "Execute Lua commands using Cyberpunk's game APIs.";
    case Page::Logs: return "Read-only m1core and scripting log output.";
    case Page::About: return "Information about the framework.";
    }
    return "";
}

void SidebarNav(const char* icon, const char* title, Page target)
{
    // The native Selectable owns mouse, keyboard and navigation state.
    // Small draw-list decorations left-align the icon/text and round
    // the active background exactly as in the supplied AFTER.html.
    ImGui::PushID(static_cast<int>(target));
    const bool active = currentPage == target;
    ImGui::PushStyleColor(ImGuiCol_Header,Color(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,Color(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,Color(0,0,0,0));
    if (ImGui::Selectable("##sidebar-nav",active,0,ImVec2(0,38)))
        currentPage = target;
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 top = ImGui::GetItemRectMin(), bottom = ImGui::GetItemRectMax();
    auto* draw = ImGui::GetWindowDrawList();
    if (active || hovered)
        draw->AddRectFilled(top,bottom,active ? IM_COL32(33,47,70,255) :
                                               IM_COL32(42,44,49,255),6.0f);
    const ImU32 tint = active ? IM_COL32(96,165,250,255) :
                            (hovered ? IM_COL32(248,250,252,255) : IM_COL32(148,163,184,255));
    const bool hasIcon = m1core::HasIcons();
    if (hasIcon)
        draw->AddText(ImGui::GetFont(),ImGui::GetFontSize()*1.12f,
                      ImVec2(top.x+12,top.y+10),tint,icon);
    auto* font = m1core::KarlaSemiBold() ? m1core::KarlaSemiBold() : ImGui::GetFont();
    draw->AddText(font,ImGui::GetFontSize(),
                  ImVec2(top.x+(hasIcon?40.0f:12.0f),top.y+10),tint,title);
    ImGui::PopStyleColor(3);
    ImGui::PopID();
}

void SidebarGroup(const char* label)
{
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+8);
    PushLayoutFont(m1core::KarlaCaption());
    ImGui::TextColored(Color(100,116,139),"%s",label);
    PopLayoutFont(m1core::KarlaCaption());
    MoveDown(5);
}

void DrawSidebar()
{
    SidebarGroup("CONFIGURATION");
    SidebarNav(M1CORE_ICON_SLIDERS_HORIZONTAL,"Settings",Page::Settings);
    SidebarNav(M1CORE_ICON_KEYBINDS,"Keybinds",Page::Keybinds);
    SidebarNav(M1CORE_ICON_USERS_THREE,"Profiles",Page::Profiles);
    MoveDown(20);
    SidebarGroup("SYSTEM");
    SidebarNav(M1CORE_ICON_TERMINAL,"Console",Page::Console);
    SidebarNav(M1CORE_ICON_TERMINAL,"Logs",Page::Logs);
    SidebarNav(M1CORE_ICON_ABOUT,"About",Page::About);

    // AFTER.html's 76px card is anchored above the bottom padding.
    const float cardHeight = 76.0f;
    const float footerY = ImGui::GetWindowHeight() -
                          ImGui::GetStyle().WindowPadding.y - cardHeight;
    if (ImGui::GetCursorPosY() < footerY)
        ImGui::SetCursorPosY(footerY);
    ImGui::PushStyleColor(ImGuiCol_ChildBg,Color(30,31,35));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(12,9));
    if (ImGui::BeginChild("##active-profile-card",ImVec2(0,cardHeight),
        ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
        ImGuiWindowFlags_NoScrollbar))
    {
        PushLayoutFont(m1core::KarlaCaption());
        ImGui::TextColored(Color(100,116,139),"Active Profile");
        PopLayoutFont(m1core::KarlaCaption());
        MoveDown(5);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(10,5));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1.0f);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##active-profile",activeProfile.c_str()))
        {
            for (const auto& name : Settings().Profiles())
            {
                const bool selected = activeProfile == name;
                if (ImGui::Selectable(name.c_str(),selected))
                {
                    if (Settings().LoadProfile(name))
                    {
                        activeProfile = name;
                        selectedProfile = name;
                        notice = "Profile loaded.";
                    }
                    else notice = "Could not load profile.";
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            if (ImGui::Selectable("Manage profiles...##manage-profiles"))
                currentPage = Page::Profiles;
            ImGui::EndCombo();
        }
        ImGui::PopStyleVar(2);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void DrawPageHeader()
{
    auto* heading = m1core::KarlaPageTitle() ?
        m1core::KarlaPageTitle() : m1core::KarlaBold();
    PushLayoutFont(heading);
    ImGui::TextUnformatted(PageTitle(currentPage));
    PopLayoutFont(heading);
    MoveDown(3);
    ImGui::TextColored(Color(148,163,184),"%s",PageDescription(currentPage));
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
    case Page::Console: DrawConsolePage(); break;
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

    // AFTER.html styling is scoped to the registry window only.
    // Runtime-owned input and overlay visibility are never rehooked.
    int colors = 0;
    const auto color = [&](ImGuiCol idx,ImVec4 value) {
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
    color(ImGuiCol_ButtonActive,Color(33,47,70));
    color(ImGuiCol_Header,Color(33,47,70));
    color(ImGuiCol_HeaderHovered,Color(42,44,49));
    color(ImGuiCol_HeaderActive,Color(33,47,70));
    color(ImGuiCol_FrameBg,Color(12,13,15));
    color(ImGuiCol_FrameBgHovered,Color(42,44,49));
    color(ImGuiCol_FrameBgActive,Color(42,44,49));
    color(ImGuiCol_CheckMark,Color(59,130,246));
    color(ImGuiCol_SliderGrab,Color(59,130,246));
    color(ImGuiCol_SliderGrabActive,Color(96,165,250));
    color(ImGuiCol_PopupBg,Color(30,31,35));
    color(ImGuiCol_TableHeaderBg,Color(30,31,35));
    color(ImGuiCol_TableRowBgAlt,Color(42,44,49,0.12f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(10,7));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,ImVec2(8,8));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize,1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0.0f);

    bool hideOverlay = false;
    const bool visible = ImGui::Begin("M1CORE##native-overlay",nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
    if (visible)
    {
        const auto windowPos = ImGui::GetWindowPos();
        bool dragging = false;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(16,8));
        if (ImGui::BeginChild("##app-titlebar",ImVec2(0,kTitleHeight),
            ImGuiChildFlags_AlwaysUseWindowPadding,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            const float dragWidth = std::max(90.0f,ImGui::GetContentRegionAvail().x-72.0f);
            ImGui::InvisibleButton("##drag-titlebar",ImVec2(dragWidth,32));
            dragging = ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left);
            const auto top = ImGui::GetItemRectMin();
            auto* draw = ImGui::GetWindowDrawList();
            auto* defaultFont = ImGui::GetFont();
            const float size = ImGui::GetFontSize();
            if (m1core::HasIcons())
                draw->AddText(defaultFont,size*1.18f,ImVec2(top.x+2,top.y+7),
                              IM_COL32(59,130,246,255),M1CORE_ICON_CUBE);
            auto* brand = m1core::KarlaBold() ? m1core::KarlaBold() : defaultFont;
            draw->AddText(brand,size,ImVec2(top.x+(m1core::HasIcons()?34.0f:2.0f),top.y+9),
                          IM_COL32(248,250,252,255),"M1CORE");
            ImGui::SameLine(0,4);
            ImGui::PushStyleColor(ImGuiCol_Button,Color(17,18,20));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,Color(42,44,49));
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
                ImGui::SetTooltip("Close m1core");
            ImGui::PopStyleColor(2);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        if (dragging)
        {
            const auto delta = ImGui::GetIO().MouseDelta;
            ImGui::SetWindowPos(ImVec2(windowPos.x+delta.x,windowPos.y+delta.y));
        }
        ImGui::Separator();

        // Borderless ImGui child windows ignore WindowPadding unless the
        // AlwaysUseWindowPadding flag is explicitly passed. This caused
        // the missing margins seen in the first user screenshot.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(12,16));
        ImGui::PushStyleColor(ImGuiCol_ChildBg,Color(24,25,28));
        // AFTER.html: the sidebar is one square, flush panel, NOT a
        // rounded bordered card. Override rounding only for BeginChild;
        // immediately restore it so the nav selection and profile card
        // keep their own intended rounded corners.
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,0.0f);
        const bool sidebarVisible = ImGui::BeginChild("##app-sidebar",ImVec2(kNavWidth,0),
            ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();
        if (sidebarVisible)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(4,4));
            DrawSidebar();
            ImGui::PopStyleVar();
        }
        ImGui::EndChild();
        // A single straight divider matches the mockup and avoids the
        // partial rounded border segments in the in-game screenshots.
        const ImVec2 sidebarMin = ImGui::GetItemRectMin();
        const ImVec2 sidebarMax = ImGui::GetItemRectMax();
        const float dividerX = sidebarMax.x - 0.5f;
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(dividerX,sidebarMin.y),ImVec2(dividerX,sidebarMax.y),
            IM_COL32(42,44,49,255),1.0f);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::SameLine(0,0);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
        if (ImGui::BeginChild("##app-content",ImVec2(0,0),
            ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoScrollbar))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(24,18));
            if (ImGui::BeginChild("##app-page-header",ImVec2(0,kPageHeaderHeight),
                ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoScrollbar))
                DrawPageHeader();
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::Separator();

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(24,20));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(8,8));
            if (ImGui::BeginChild("##app-page-body",ImVec2(0,0),
                ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_None))
                DrawPageBody();
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }
    ImGui::End();
    ImGui::PopStyleVar(9);
    ImGui::PopStyleColor(colors);
    if (hideOverlay)
        M1CoreRuntime::Get().GetOverlay().Toggle();
}
} // namespace m1core
