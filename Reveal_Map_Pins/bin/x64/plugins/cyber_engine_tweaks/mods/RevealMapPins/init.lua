-- Reveal Map Pins | Cyberpunk 2077 2.5 | 0.3.2-cet1381-sandbox
-- CET is the settings/database bridge. Native Settings UI is optional.
local MOD = "RevealMapPins"
local TITLE = "Reveal Map Pins"
local OPTIONS = {
  { key = "enabled", label = "Reveal map pins", desc = "Master switch for selected vanilla categories.", fact = "miso_rmp_all_disabled", default = true },
  { key = "catalog", label = "Use classified map catalog (experimental)", desc = "Display missing verified vanilla POIs using their native icon variants; disable anytime.", fact = "miso_rmp_catalog_enabled", default = false },
  { key = "travel", label = "Fast travel locations", desc = "Display only; does NOT unlock destinations.", fact = "miso_rmp_travel_disabled", default = true },
  { key = "ncpd", label = "NCPD scanner hustles", desc = "Assault in Progress, Organized Crime and Reported Crime.", fact = "miso_rmp_ncpd_disabled", default = true },
  { key = "psycho", label = "Cyberpsycho sightings", desc = "Cyberpsycho locations, not quest stages.", fact = "miso_rmp_psycho_disabled", default = true },
  { key = "tarot", label = "Tarot cards", desc = "Tarot graffiti markers.", fact = "miso_rmp_tarot_disabled", default = true },
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
  local number
  if item.key == "catalog" then
    number = db.settings.catalog and 1 or 0
  else
    number = db.settings[item.key] and 0 or 1
  end
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

-- Separate CET owner for the 212 journal-classified game-derived POIs.
-- This mode starts disabled until the player explicitly opts in.
-- Bindings must always register, even if a missing/corrupt catalog fails.
-- CET exposes RTTI/Game types only after onInit: do not load the catalog
-- module at top level, before the hotkeys and onInit event are registered.
local catalogRuntime = {
  reset = function() end,
  tick = function() end,
  stats = function() return "catalog=not-loaded" end,
}
local catalogReady = false
local function initializeCatalog()
  local ok, instance = pcall(function()
    local mod = dofile("catalog_runtime.lua")
    assert(type(mod) == "table" and type(mod.new) == "function", "catalog module missing constructor")
    return mod.new(
      "",
      function(key) return db.settings[key] == true end,
      function(message) print("[" .. MOD .. "] " .. message) end
    )
  end)
  if ok and type(instance) == "table" and type(instance.tick) == "function"
    and type(instance.reset) == "function" and type(instance.stats) == "function" then
    catalogRuntime = instance
    catalogReady = true
    print("[" .. MOD .. "] Classified catalog loaded (" .. catalogRuntime:stats() .. ")")
  else
    catalogReady = false
    print("[" .. MOD .. "] Classified catalog startup FAILED: " .. tostring(instance))
    print("[" .. MOD .. "] CET hotkeys remain available; check catalog_runtime.lua and catalog.tsv")
  end
end

-- REDscript ScriptableSystem classes are accessed through the system
-- container. Explicitly resolve them when a game session is available: if
-- the scanner was never instantiated, toggles alone cannot reveal anything.
local scannerReady = false
local settingsReady = false
local bootPoll = 0
local bootAttempts = 0
local bootWarningIssued = false

local function bootstrapSystems()
  if not Game.GetPlayer() then return end
  local function getSystem(name)
    local ok, result = pcall(function()
      local container = Game.GetScriptableSystemsContainer()
      if not container then return nil end
      return container:Get(CName.new("MisoRevealMapPins." .. name))
    end)
    return ok and result ~= nil
  end
  if not scannerReady then
    scannerReady = getSystem("RMPSystem")
    if scannerReady then
      print("[" .. MOD .. "] World-map marker scanner initialized")
    end
  end
  if not settingsReady then
    settingsReady = getSystem("RMPSettings")
    if settingsReady then
      print("[" .. MOD .. "] REDscript settings bridge initialized")
    end
  end
  if scannerReady and settingsReady then return end
  bootAttempts = bootAttempts + 1
  if bootAttempts >= 4 and not bootWarningIssued then
    bootWarningIssued = true
    print("[" .. MOD .. "] WARNING: REDscript system missing (scanner=" .. tostring(scannerReady) ..
      ", settings=" .. tostring(settingsReady) .. "). Check r6/logs/redscript_rCURRENT.log")
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
  initializeCatalog()
  print("[" .. MOD .. "] CET initialized; four Reveal Map Pins bindings registered")
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

registerHotkey(MOD .. "_Catalog", TITLE .. ": toggle classified catalog", function()
  updateOption("catalog", not db.settings.catalog)
  print("[" .. MOD .. "] Classified 212-location catalog " .. (db.settings.catalog and "ON" or "OFF"))
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
  print("[" .. MOD .. "] scanner=" .. (scannerReady and "initialized" or "MISSING")
    .. " | settings=" .. (settingsReady and "initialized" or "MISSING")
    .. " | " .. table.concat(values, " | ") .. " | " .. catalogRuntime:stats())
end)

-- Independent map-rendering probe, inspired by standard CET MappinSystem usage.
-- This does NOT discover any locations. It is an opt-in, temporary marker
-- at a user-verified vanilla coordinate, for isolating map UI from REDscript.
local previewMappinId = nil

local function clearPreviewMappin()
  if not previewMappinId then return end
  local id = previewMappinId
  previewMappinId = nil
  local ok, err = pcall(function()
    local ms = Game.GetMappinSystem()
    if ms then ms:UnregisterMappin(id) end
  end)
  if not ok then
    print("[" .. MOD .. "] Preview cleanup warning: " .. tostring(err))
  end
end

registerHotkey(MOD .. "_Preview", TITLE .. ": toggle temporary marker test", function()
  if previewMappinId then
    clearPreviewMappin()
    print("[" .. MOD .. "] Temporary preview marker removed")
    return
  end
  if not Game.GetPlayer() then
    print("[" .. MOD .. "] Preview unavailable: load a save first")
    return
  end
  local ok, result = pcall(function()
    local mappin = MappinData.new()
    mappin.mappinType = TweakDBID.new("Mappins.QuestStaticMappinDefinition")
    mappin.variant = gamedataMappinVariant.ExclamationMarkVariant
    mappin.active = true
    mappin.visibleThroughWalls = false
    return Game.GetMappinSystem():RegisterMappin(
      mappin, Vector4.new(-1382.2, 1270.6, 123.08, 1)
    )
  end)
  if not ok or not result then
    print("[" .. MOD .. "] Preview registration FAILED: " .. tostring(result))
    return
  end
  local valid, value = pcall(function() return result.value end)
  if not valid or value == nil or tostring(value) == "0" then
    print("[" .. MOD .. "] Preview registration returned invalid marker ID: " .. tostring(value))
    return
  end
  previewMappinId = result
  print("[" .. MOD .. "] Temporary test marker added near H10 / V's Apartment. Open the world map; press the same binding to remove it.")
end)

-- CET has no onSessionStart/onSessionEnd events; use onUpdate instead.

registerForEvent("onUpdate", function(delta)
  catalogRuntime:tick(delta)
  if not scannerReady or not settingsReady then
    bootPoll = bootPoll + (delta or 0)
    if bootPoll >= 3 then
      bootPoll = 0
      bootstrapSystems()
    end
  end
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
    if inSession then
      catalogRuntime:reset()
      clearPreviewMappin()
      scannerReady, settingsReady = false, false
      bootPoll, bootAttempts = 0, 0
      bootWarningIssued = false
      lastWritten = {}
    end
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
        -- The experimental catalog fact is positive (1=ON), unlike the
        -- legacy *_disabled facts (0=ON).
        local enabled = item.key == "catalog" and value == 1 or item.key ~= "catalog" and value == 0
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
registerForEvent("onShutdown", function()
  catalogRuntime:reset()
  clearPreviewMappin()
  saveDB()
end)
