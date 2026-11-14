-- Reveal Map Pins | Cyberpunk 2077 2.5 | 0.1.1-compat
-- CET is the settings/database bridge. Native Settings UI is optional.
local MOD = "RevealMapPins"
local TITLE = "Reveal Map Pins"
local OPTIONS = {
  { key = "enabled", label = "Reveal map pins", desc = "Master switch for all category overrides.", fact = "miso_rmp_all_disabled", default = true },
  { key = "jobs", label = "Jobs", desc = "Discoverable main and side jobs.", fact = "miso_rmp_jobs_disabled", default = true },
  { key = "gigs", label = "Gigs", desc = "Gig and fixer pins already registered by the game.", fact = "miso_rmp_gigs_disabled", default = true },
  { key = "ncpd", label = "NCPD scanners", desc = "NCPD scanner hustles and events.", fact = "miso_rmp_ncpd_disabled", default = true },
  { key = "psycho", label = "Cyberpsychos", desc = "Cyberpsycho sightings.", fact = "miso_rmp_psycho_disabled", default = true },
  { key = "travel", label = "Fast travel / NCART", desc = "Markers only; does NOT unlock destinations.", fact = "miso_rmp_travel_disabled", default = true },
  { key = "ripperdoc", label = "Ripperdocs", desc = "Ripperdoc markers.", fact = "miso_rmp_ripperdoc_disabled", default = true },
  { key = "vendor", label = "Vendors", desc = "Vendor and market markers.", fact = "miso_rmp_vendor_disabled", default = true },
  { key = "autofixer", label = "Autofixer", desc = "Car vendor markers.", fact = "miso_rmp_autofixer_disabled", default = true },
  { key = "drop", label = "Drop points", desc = "Drop point markers.", fact = "miso_rmp_drop_disabled", default = true },
  { key = "rent", label = "Apartments for rent", desc = "Apartments available for purchase.", fact = "miso_rmp_rent_disabled", default = true },
  { key = "tarot", label = "Tarot", desc = "Tarot graffiti markers.", fact = "miso_rmp_tarot_disabled", default = true },
  { key = "property", label = "Owned property", desc = "Your apartments and owned vehicles.", fact = "miso_rmp_property_disabled", default = true },
}

local function scriptDir()
  local s = debug.getinfo(1, "S").source or ""
  return s:sub(1, 1) == "@" and (s:sub(2):match("^(.*[\\/])") or "") or ""
end
local DATA_FILE = scriptDir() .. "db.json"
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

local nativeUIReady = false
local nativeUIPoll = 0

-- NSUI may initialize after this CET mod. Retry its registration until the
-- API becomes available; neither the game logic nor Mod Settings depends on it.
local function tryRegisterNativeSettings()
  if nativeUIReady then return end
  local ok, module = pcall(GetMod, "nativeSettings")
  if not ok or not module then return end
  local success, err = pcall(function()
    local path = "/" .. MOD
    module.addTab(path, TITLE)
    module.addSubcategory(path .. "/general", "Options")
    for _, item in ipairs(OPTIONS) do
      local opt = item
      uiOptions[opt.key] = module.addSwitch(path .. "/general", opt.label, opt.desc,
        db.settings[opt.key], opt.default,
        function(value) updateOption(opt.key, value) end)
    end
  end)
  if not success then
    print("[" .. MOD .. "] Native Settings UI registration failed: " .. tostring(err))
    return
  end
  ns = module
  nativeUIReady = true
  print("[" .. MOD .. "] Native Settings UI tab registered")
end

registerForEvent("onInit", function()
  tryRegisterNativeSettings()
  if not nativeUIReady then
    print("[" .. MOD .. "] Native Settings UI not detected yet; will retry in game")
  end
end)

-- CET's configurable Bindings page provides a working in-game master toggle
-- even without either of the two optional graphical settings mods.
registerHotkey(MOD .. "_Toggle", TITLE .. ": master on/off", function()
  updateOption("enabled", not db.settings.enabled)
  print("[" .. MOD .. "] " .. (db.settings.enabled and "ON" or "OFF"))
end)

-- CET Bindings -> diagnostic hotkey. This reports whether the game has
-- hidden eligible sources and whether our own proxy registration succeeded.
registerHotkey(MOD .. "_Status", TITLE .. ": print map scan diagnostics", function()
  local qs = questSystem()
  if not qs then
    print("[" .. MOD .. "] Diagnostics unavailable: no active game session")
    return
  end
  local values = {}
  for _, name in ipairs({"seen", "hidden", "eligible", "active", "failed"}) do
    local ok, value = pcall(function() return qs:GetFactStr("miso_rmp_diag_" .. name) end)
    values[#values + 1] = name .. "=" .. tostring(ok and value or "?")
  end
  print("[" .. MOD .. "] " .. table.concat(values, " | "))
end)

registerForEvent("onSessionStart", function() inSession = false end)
registerForEvent("onSessionEnd", function() inSession = false end)

registerForEvent("onUpdate", function(delta)
  if not nativeUIReady then
    nativeUIPoll = nativeUIPoll + (delta or 0)
    if nativeUIPoll >= 4 then
      nativeUIPoll = 0
      tryRegisterNativeSettings()
    end
  end
  pollTime = pollTime + (delta or 0)
  if pollTime < 0.6 then return end
  pollTime = 0
  local qs = questSystem()
  if not qs then
    inSession = false
    return
  end
  if not inSession then
    -- The JSON database is the source of truth whenever a save is loaded.
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
