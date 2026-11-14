-- No Tracking Please | Cyberpunk 2077 2.5 | v0.1.1-cet1381
-- CET is the settings/database bridge. Native Settings UI is optional.
local MOD = "NoTrackingPlease"
local TITLE = "No Tracking Please"
local OPTIONS = {
  { key = "enabled", label = "No enemy position trace", desc = "Stop reveal-position hacks. Other enemy attacks still work.", fact = "miso_ntp_disabled", default = true },
}

-- CET 1.38.1 sandbox excludes the Lua debug standard library.
-- Relative file paths resolve inside the mod sandbox directory.
local DATA_FILE = "db.json"
local db = { schema = 1, settings = {} }
local byKey, byFact, uiOptions = {}, {}, {}
for _, item in ipairs(OPTIONS) do
  db.settings[item.key] = item.default
  byKey[item.key] = item
  byFact[item.fact] = item
end

local function loadDB()
  local f = io.open(DATA_FILE, "rb")
  if not f then return end
  local raw = f:read("*a"); f:close()
  local ok, saved = pcall(json.decode, raw)
  if not ok or type(saved) ~= "table" or type(saved.settings) ~= "table" then
    print("[" .. MOD .. "] Ignoring malformed db.json")
    return
  end
  for key, item in pairs(byKey) do
    if type(saved.settings[key]) == "boolean" then db.settings[key] = saved.settings[key] end
  end
end

local function saveDB()
  local ok, data = pcall(json.encode, db)
  if not ok then print("[" .. MOD .. "] JSON serialization failed"); return end
  local f, err = io.open(DATA_FILE, "wb")
  if not f then print("[" .. MOD .. "] DB write failed: " .. tostring(err)); return end
  f:write(data); f:close()
end
loadDB()
saveDB() -- create or normalize the persistent database on the first launch

local ns = nil
local inSession = false
local pollTime = 0
local lastWritten = {}

local function questSystem()
  local ok, qs = pcall(function()
    if not Game.GetPlayer() then return nil end
    return Game.GetQuestsSystem()
  end)
  return ok and qs or nil
end

local function writeFact(item)
  local qs = questSystem()
  if not qs then return false end
  local number = db.settings[item.key] and 0 or 1
  local ok = pcall(function() qs:SetFactStr(item.fact, number) end)
  if ok then lastWritten[item.fact] = number end
  return ok
end

local function updateOption(key, value)
  if not byKey[key] or type(value) ~= "boolean" then return end
  db.settings[key] = value
  saveDB()
  writeFact(byKey[key])
  if ns and uiOptions[key] then
    pcall(function() ns.setOption(uiOptions[key], value) end)
  end
end

registerForEvent("onInit", function()
  ns = GetMod("nativeSettings")
  if not ns then
    print("[" .. MOD .. "] Native Settings UI not installed; CET hotkey and db.json remain available")
    return
  end
  local path = "/" .. MOD
  ns.addTab(path, TITLE)
  ns.addSubcategory(path .. "/general", "Options")
  for _, item in ipairs(OPTIONS) do
    local opt = item
    uiOptions[opt.key] = ns.addSwitch(path .. "/general", opt.label, opt.desc,
      db.settings[opt.key], opt.default, function(value) updateOption(opt.key, value) end)
  end
end)

-- CET's configurable Bindings page provides a working in-game master toggle
-- even without either of the two optional graphical settings mods.
registerHotkey(MOD .. "_Toggle", TITLE .. ": master on/off", function()
  updateOption("enabled", not db.settings.enabled)
  print("[" .. MOD .. "] " .. (db.settings.enabled and "ON" or "OFF"))
end)

-- CET session transitions are handled by onUpdate instead.

registerForEvent("onUpdate", function(delta)
  pollTime = pollTime + (delta or 0)
  if pollTime < 0.6 then return end
  pollTime = 0
  local qs = questSystem()
  if not qs then
    if inSession then lastWritten = {} end
    inSession = false
    return
  end
  if not inSession then
    -- The JSON database is the source of truth whenever a save is loaded.
    lastWritten = {}
    for _, item in ipairs(OPTIONS) do writeFact(item) end
    inSession = true
    return
  end
  local changed = false
  for _, item in ipairs(OPTIONS) do
    local ok, value = pcall(function() return qs:GetFactStr(item.fact) end)
    if ok and type(value) == "number" and (value == 0 or value == 1) then
      if lastWritten[item.fact] ~= nil and value ~= lastWritten[item.fact] then
        local enabled = value == 0
        if db.settings[item.key] ~= enabled then
          db.settings[item.key] = enabled
          changed = true
          if ns and uiOptions[item.key] then
            pcall(function() ns.setOption(uiOptions[item.key], enabled) end)
          end
        end
      end
      lastWritten[item.fact] = value
    end
  end
  if changed then saveDB() end
end)

-- Optional debugging convenience; no additional dependency.
registerForEvent("onShutdown", function() saveDB() end)
