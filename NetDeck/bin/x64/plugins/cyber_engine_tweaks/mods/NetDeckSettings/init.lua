-- NETWORM / NetWatch Sigma Mk.1 live test controls (Cyber Engine Tweaks).
-- Does not hook NPC/vehicle spawning, teleporting, or global game HUD events.
-- Writes only the settings.json next to this init.lua and calls the public
-- NetDeckConfigure method added by NetDeck.reds. Safe if no save is loaded.
local PREFIX = "[NetDeck/SETTINGS]"
local DEFAULT = {
  uploadSeconds = 2.0,
  scanRadius = 105.0,
  hopRadius = 75.0,
  spreadInterval = 0.25,
  maxJumps = 100,
  maxTargets = 120,
  fanout = 10
}
local LIMITS = {
  uploadSeconds = {0.7, 4.5}, scanRadius = {30, 180},
  hopRadius = {15, 130}, spreadInterval = {0.10, 2.50},
  maxJumps = {4, 150}, maxTargets = {8, 160}, fanout = {1, 16}
}
local INTEGER = {maxJumps = true, maxTargets = true, fanout = true}
local KEYS = {"uploadSeconds", "scanRadius", "hopRadius", "spreadInterval", "maxJumps", "maxTargets", "fanout"}
local settings = {}
local loaded = false
local syncSeconds = 0
local lastError = ""
local script = (debug and debug.getinfo and debug.getinfo(1, "S").source) or ""
local dir = script:gsub("^@", ""):match("^(.*[/\\])") or ""
local filePath = dir .. "settings.json"

local function normalize(key, value)
  local range = LIMITS[key]
  if not range then return nil end
  value = tonumber(value) or DEFAULT[key]
  if value ~= value then value = DEFAULT[key] end -- NaN
  value = math.max(range[1], math.min(range[2], value))
  return INTEGER[key] and math.floor(value + 0.5) or value
end

local function setAll(values)
  for _, key in ipairs(KEYS) do
    settings[key] = normalize(key, values and values[key])
  end
end

local function load()
  local fh = io.open(filePath, "r")
  if fh then
    local raw = fh:read("*a")
    fh:close()
    local ok, parsed = pcall(function() return json.decode(raw) end)
    if ok and type(parsed) == "table" then
      setAll(parsed)
      loaded = true
      return
    end
    print(PREFIX .. " invalid JSON; using safe defaults")
  end
  setAll(DEFAULT)
  loaded = true
end

local function save()
  local ok, err = pcall(function()
    local fh = assert(io.open(filePath, "w"))
    fh:write(json.encode(settings))
    fh:close()
  end)
  if not ok then print(PREFIX .. " cannot save settings: " .. tostring(err)) end
end

local function apply()
  if not loaded then return end
  local ok, err = pcall(function()
    local player = Game.GetPlayer()
    if not player then return end
    player:NetDeckConfigure(
      settings.uploadSeconds, settings.scanRadius, settings.hopRadius,
      settings.spreadInterval, settings.maxJumps,
      settings.maxTargets, settings.fanout
    )
  end)
  if not ok then
    local message = tostring(err)
    if message ~= lastError then
      print(PREFIX .. " runtime settings unavailable (check REDscript compile): " .. message)
    end
    lastError = message
  else
    if lastError ~= "" then print(PREFIX .. " runtime settings reconnected") end
    lastError = ""
  end
end

local function changed(key, value)
  if type(value) ~= "number" then return end
  settings[key] = normalize(key, value)
  save()
  apply()
end

local function sliderFloat(key, label, fmt)
  local range = LIMITS[key]
  local yes, value = ImGui.SliderFloat(label, settings[key], range[1], range[2], fmt or "%.2f")
  if yes then changed(key, value) end
end

local function sliderInt(key, label)
  local range = LIMITS[key]
  local yes, value = ImGui.SliderInt(label, settings[key], range[1], range[2])
  if yes then changed(key, value) end
end

local function preset(values)
  setAll(values)
  save()
  apply()
end

registerForEvent("onInit", function()
  load()
  apply()
  print(PREFIX .. " controls ready in CET overlay; settings.json persists between launches")
end)

registerForEvent("onUpdate", function(dt)
  if not loaded or type(dt) ~= "number" then return end
  syncSeconds = syncSeconds + math.max(dt, 0)
  if syncSeconds >= 2.0 then
    syncSeconds = 0
    apply() -- reapply after save load or changing player instances
  end
end)

registerForEvent("onDraw", function()
  if not loaded then return end
  if ImGui.Begin("NETWORM - Test Settings") then
    ImGui.Text("Changes apply live and are saved automatically.")
    ImGui.Separator()
    sliderFloat("uploadSeconds", "HUD upload seconds", "%.2f s")
    sliderFloat("scanRadius", "Network scan radius (m)", "%.0f m")
    sliderFloat("hopRadius", "Worm jump radius (m)", "%.0f m")
    sliderFloat("spreadInterval", "Delay between infections (s)", "%.2f s")
    sliderInt("maxJumps", "Maximum infection steps")
    sliderInt("maxTargets", "Maximum affected targets")
    sliderInt("fanout", "Max relay fanout per host")
    ImGui.Separator()
    if ImGui.Button("Club takeover") then preset({
      uploadSeconds=1.6, scanRadius=145, hopRadius=115,
      spreadInterval=0.14, maxJumps=140, maxTargets=150, fanout=14
    }) end
    ImGui.SameLine()
    if ImGui.Button("Balanced") then preset(DEFAULT) end
    ImGui.SameLine()
    if ImGui.Button("Performance safe") then preset({
      uploadSeconds=2.0, scanRadius=65, hopRadius=45,
      spreadInterval=0.50, maxJumps=60, maxTargets=60, fanout=6
    }) end
    ImGui.Separator()
    ImGui.TextWrapped("HUD upload time is separate from the spread delay. Scan range only finds loaded, eligible targets that the game exposes to its targeting system.")
    ImGui.TextWrapped("Quest targets, companions, children and crowd civilians stay excluded. Large scans can increase CPU load; use Performance safe if needed.")
    if lastError ~= "" then
      ImGui.TextWrapped("Settings could not reach REDscript. See CET scripting.log.")
    end
  end
  ImGui.End()
end)
