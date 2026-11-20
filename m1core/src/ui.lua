-- M1CORE settings view, drawn entirely inside CET's ImGui frame.
-- This module never creates, moves, or hides an OS cursor.
local View = {}
local palette = {
  blue = {0.03, 0.37, 0.86}, white = {0.97,0.97,0.97},
  green = {0.07, 0.50, 0.27}, amber = {0.98,0.70,0.02},
  orange = {0.90,0.39,0.18}, black = {0.04,0.04,0.04}
}

function View.new()
  return {
    visible = true, tab = "SETTINGS", selected = nil,
    filter = "", profileName = "", selectedProfile = nil,
    notice = "CET handles input, cursor and keybinds."
  }
end

local function coloredButton(label, tint, darkText)
  ImGui.PushStyleColor(ImGuiCol.Button, tint[1], tint[2], tint[3], 1.0)
  ImGui.PushStyleColor(ImGuiCol.ButtonHovered,
    math.min(1,tint[1]+0.10), math.min(1,tint[2]+0.10), math.min(1,tint[3]+0.10), 1)
  ImGui.PushStyleColor(ImGuiCol.Text,
    darkText and 0.05 or 1, darkText and 0.05 or 1,
    darkText and 0.05 or 1, 1)
  local clicked = ImGui.Button(label, 115, 34)
  ImGui.PopStyleColor(3)
  return clicked
end

local function renderSetting(registry, mod, setting)
  ImGui.PushID(mod.id .. "." .. setting.id)
  ImGui.TableNextRow()
  ImGui.TableSetColumnIndex(0)
  ImGui.TextUnformatted(setting.label)
  if setting.description ~= "" then ImGui.TextDisabled(setting.description) end
  ImGui.TableSetColumnIndex(1)
  ImGui.SetNextItemWidth(-1)
  local value = setting.value
  local nextValue, changed = nil, false
  if setting.type == "boolean" then
    nextValue, changed = ImGui.Checkbox("##value", value)
  elseif setting.type == "integer" then
    nextValue, changed = ImGui.SliderInt("##value", value,
      setting.min or 0, setting.max or 100)
  elseif setting.type == "float" then
    nextValue, changed = ImGui.SliderFloat("##value", value,
      setting.min or 0, setting.max or 100, "%.2f")
  elseif setting.type == "choice" then
    if ImGui.BeginCombo("##value", tostring(value)) then
      for _,choice in ipairs(setting.choices or {}) do
        if ImGui.Selectable(choice, value == choice) then
          nextValue, changed = choice, true
        end
      end
      ImGui.EndCombo()
    end
  end
  if changed then registry:set(mod.id, setting.id, nextValue) end
  ImGui.TableSetColumnIndex(2)
  if ImGui.Button("Reset", 62, 30) then
    registry:reset(mod.id, setting.id)
  end
  ImGui.PopID()
end

local function settingsPane(registry, state, mods)
  local chosen
  for _,mod in ipairs(mods) do
    if mod.id == state.selected then chosen = mod; break end
  end
  if not chosen then
    ImGui.TextDisabled("Select a mod in the left sidebar.")
    return
  end
  ImGui.TextUnformatted(chosen.name)
  ImGui.SameLine()
  ImGui.TextDisabled("(" .. #chosen.order .. " settings)")
  if ImGui.Button("Reset this mod") then registry:reset(chosen.id) end
  ImGui.Separator()
  local groups, order = {}, {}
  for _,id in ipairs(chosen.order) do
    local setting = chosen.settings[id]
    local text = (setting.label .. " " .. setting.description):lower()
    if state.filter == "" or text:find(state.filter:lower(),1,true) or
       chosen.name:lower():find(state.filter:lower(),1,true) then
      local group = setting.group
      if not groups[group] then groups[group] = {}; order[#order+1] = group end
      groups[group][#groups[group]+1] = setting
    end
  end
  table.sort(order)
  if #order == 0 then ImGui.TextDisabled("No matching settings."); return end
  if ImGui.BeginTable("##m1coreSettings", 3,
      ImGuiTableFlags.BordersInnerH) then
    ImGui.TableSetupColumn("Setting", ImGuiTableColumnFlags.WidthStretch, 0.53)
    ImGui.TableSetupColumn("Control", ImGuiTableColumnFlags.WidthStretch, 0.35)
    ImGui.TableSetupColumn("Reset", ImGuiTableColumnFlags.WidthFixed, 74)
    for _,group in ipairs(order) do
      ImGui.TableNextRow()
      ImGui.TableSetColumnIndex(0)
      ImGui.TextColored(0.84,0.84,0.86,1,group)
      for _,setting in ipairs(groups[group]) do
        renderSetting(registry, chosen, setting)
      end
    end
    ImGui.EndTable()
  end
end

local function profilesPane(registry, state)
  ImGui.TextUnformatted("Profiles")
  ImGui.TextWrapped("Save or restore settings for all registered mods.")
  state.profileName = select(1, ImGui.InputText("Profile name", state.profileName, 64))
  if ImGui.Button("Save profile", 115, 32) then
    local ok,err = registry:saveProfile(state.profileName)
    state.notice = ok and "Profile saved." or tostring(err)
  end
  ImGui.Separator()
  for _,name in ipairs(registry:profileNames()) do
    ImGui.PushID(name)
    if ImGui.Selectable(name, state.selectedProfile == name) then
      state.selectedProfile = name
    end
    ImGui.PopID()
  end
  if state.selectedProfile then
    if ImGui.Button("Load profile", 115, 32) then
      local ok,err = registry:loadProfile(state.selectedProfile)
      state.notice = ok and "Profile loaded." or tostring(err)
    end
    ImGui.SameLine()
    if ImGui.Button("Delete profile", 115, 32) then
      registry:deleteProfile(state.selectedProfile)
      state.selectedProfile = nil
      state.notice = "Profile deleted."
    end
  end
end

function View.draw(registry, state)
  -- Deliberately allow CET's font, input, mouse speed and overlay rendering
  -- to control the frame. No hooks, raw mouse packets or software pointers.
  ImGui.SetNextWindowSize(1050, 620, ImGuiCond.FirstUseEver)
  ImGui.PushStyleVar(ImGuiStyleVar.WindowRounding, 0)
  -- CET's Begin(name, open) returns (open, shouldDraw), in that order.
  local stillOpen, show = ImGui.Begin("M1CORE##CET", state.visible,
                                   ImGuiWindowFlags.NoCollapse)
  state.visible = stillOpen
  if show then
    ImGui.PushStyleColor(ImGuiCol.ChildBg, 1,1,1,1)
    ImGui.PushStyleColor(ImGuiCol.Text, 0.05,0.05,0.05,1)
    ImGui.BeginChild("##header",0,48,false)
    ImGui.TextUnformatted("M1CORE  /  Settings")
    ImGui.EndChild()
    ImGui.PopStyleColor(2)

    if coloredButton("SETTINGS",palette.blue,false) then state.tab="SETTINGS" end
    ImGui.SameLine()
    if coloredButton("KEYBINDS",palette.white,true) then state.tab="KEYBINDS" end
    ImGui.SameLine()
    if coloredButton("PROFILES",palette.green,false) then state.tab="PROFILES" end
    ImGui.SameLine()
    if coloredButton("LOGS",palette.amber,true) then state.tab="LOGS" end
    ImGui.SameLine()
    if coloredButton("ABOUT",palette.orange,false) then state.tab="ABOUT" end
    ImGui.Separator()
    state.filter = select(1,ImGui.InputText("Search settings",state.filter,128))

    local availW, availH = ImGui.GetContentRegionAvail()
    local panelH = math.max(160, availH-48)
    local sideW = math.min(220, math.max(130, availW*0.27))
    ImGui.BeginChild("##m1coreMods",sideW,panelH,true)
    ImGui.TextDisabled("REGISTERED MODS")
    ImGui.Separator()
    local mods = registry:modsList()
    for _,mod in ipairs(mods) do
      if state.filter == "" or mod.name:lower():find(state.filter:lower(),1,true) or
         state.tab ~= "SETTINGS" then
        if ImGui.Selectable(mod.name .. "##" .. mod.id,
           state.selected == mod.id) then
          state.selected = mod.id
          if state.tab ~= "SETTINGS" then state.tab="SETTINGS" end
        end
      end
    end
    ImGui.EndChild()
    ImGui.SameLine()
    ImGui.BeginChild("##m1coreContent",0,panelH,true)
    if state.tab == "SETTINGS" then
      settingsPane(registry, state, mods)
    elseif state.tab == "PROFILES" then
      profilesPane(registry, state)
    elseif state.tab == "KEYBINDS" then
      ImGui.TextUnformatted("Keyboard shortcuts")
      ImGui.TextWrapped("Use CET > Bindings to select a key for 'M1CORE - Show / hide settings'.")
      ImGui.TextWrapped("CET itself owns all key events, focus and cursor visibility.")
    elseif state.tab == "LOGS" then
      for _,entry in ipairs(registry.logs) do ImGui.TextWrapped(entry) end
    elseif state.tab == "ABOUT" then
      ImGui.TextUnformatted("M1CORE 1.0.0-dev.1")
      ImGui.TextWrapped("Native-feeling settings UI hosted by Cyber Engine Tweaks.")
      ImGui.TextWrapped("MIT-licensed Lua mod. CET supplies proven overlay, input and ImGui APIs.")
      ImGui.TextDisabled("Other CET mods can use GetMod('m1core').")
    end
    ImGui.EndChild()

    ImGui.Separator()
    if ImGui.Button("Save",100,32) then
      local ok,err = registry:save()
      state.notice = ok and "Settings saved." or tostring(err)
    end
    ImGui.SameLine()
    if ImGui.Button("Close",100,32) then state.visible = false end
    ImGui.SameLine()
    ImGui.TextDisabled(registry.error or state.notice)
  end
  ImGui.End()
  ImGui.PopStyleVar()
end

return View
