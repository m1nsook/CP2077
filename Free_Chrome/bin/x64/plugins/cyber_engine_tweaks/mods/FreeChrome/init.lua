-- Free Chrome | CP2077 2.5 + Phantom Liberty | CET 1.38+
-- Persistent setting -> optional NSUI / Mod Settings bridge -> reversible TweakDB patch.
-- Never ship db.json: player preferences are generated locally.
local MOD, TITLE = "FreeChrome", "Free Chrome"
local FACT = "miso_free_chrome_disabled"
local FILE = "db.json"
local state = { schema = 1, settings = { enabled = true } }
local ns, nsSwitch, nsReady = nil, nil, false
local playerInSession, lastFact = false, nil
local syncClock, scanClock, uiClock = 0, 0, 0
local patch = nil

local function log(message) print("[" .. MOD .. "] " .. tostring(message)) end

local function load()
  local f = io.open(FILE, "rb")
  if not f then return end
  local raw = f:read("*a"); f:close()
  local ok, data = pcall(json.decode, raw)
  if ok and type(data) == "table" and type(data.settings) == "table"
      and type(data.settings.enabled) == "boolean" then
    state.settings.enabled = data.settings.enabled
  else
    log("Ignoring malformed db.json; default enabled")
  end
end

local function save()
  local ok, result = pcall(json.encode, state)
  if not ok then log("Could not serialize settings"); return end
  local f, err = io.open(FILE, "wb")
  if not f then log("Could not write settings: " .. tostring(err)); return end
  f:write(result); f:close()
end

load()

local function questSystem()
  local ok, qs = pcall(function()
    if not Game.GetPlayer() then return nil end
    return Game.GetQuestsSystem()
  end)
  return ok and qs or nil
end

local function setFact()
  local qs = questSystem()
  if not qs then return end
  local value = state.settings.enabled and 0 or 1
  local ok = pcall(function() qs:SetFactStr(FACT, value) end)
  if ok then lastFact = value end
end

local function setEnabled(enabled)
  if type(enabled) ~= "boolean" then return end
  if state.settings.enabled ~= enabled then
    state.settings.enabled = enabled
    save()
    if patch then
      local ok, err = pcall(function() patch:setEnabled(enabled) end)
      if not ok then log("Patch toggle failed: " .. tostring(err)) end
    end
  end
  setFact()
  if ns and nsSwitch then
    pcall(function() ns.setOption(nsSwitch, enabled) end)
  end
  log(enabled and "Enabled: requested zero Humanity modifiers (see scan results)"
    or "Disabled: requested restoration of original Humanity modifiers (see restore results)")
end

local function registerNSUI()
  if nsReady then return end
  local ok, api = pcall(GetMod, "nativeSettings")
  if not ok or not api then return end
  local success, err = pcall(function()
    api.addTab("/" .. MOD, TITLE)
    api.addSubcategory("/" .. MOD .. "/general", "General")
    nsSwitch = api.addSwitch("/" .. MOD .. "/general",
      "Zero Humanity cost",
      "Set cyberware's Humanity/Capacity cost to 0. Does not alter prices, maximum Capacity, or stats.",
      state.settings.enabled, true,
      function(value) setEnabled(value) end)
  end)
  if not success then
    log("Native Settings UI registration failed: " .. tostring(err))
    return
  end
  ns, nsReady = api, true
  log("Native Settings UI registered")
end

registerHotkey(MOD .. "_Toggle", "Free Chrome: enable/disable zero Humanity cost", function()
  setEnabled(not state.settings.enabled)
end)

registerForEvent("onInit", function()
  -- CET RTTI and TweakDB may only be accessed after the game initializes.
  local ok, obj = pcall(dofile, "patch.lua")
  if not ok or type(obj) ~= "table" or type(obj.new) ~= "function" then
    log("Patch module failed to load: " .. tostring(obj))
    return
  end
  patch = obj.new(log)
  local applied, failure = pcall(function() patch:setEnabled(state.settings.enabled) end)
  if not applied then log("TweakDB patch failed: " .. tostring(failure)) end
  save()
  registerNSUI()
end)

-- NSUI and Mod Settings share one persisted switch. The Mod Settings
-- REDscript system sends changes via the fact; CET writes db.json and applies
-- the TweakDB changes immediately when the fact changes.
registerForEvent("onUpdate", function(delta)
  delta = delta or 0
  uiClock = uiClock + delta
  if not nsReady and uiClock >= 4 then
    uiClock = 0
    registerNSUI()
  end

  syncClock = syncClock + delta
  if syncClock >= 0.5 then
    syncClock = 0
    local qs = questSystem()
    if not qs then
      if playerInSession then lastFact = nil end
      playerInSession = false
    elseif not playerInSession then
      playerInSession = true
      lastFact = nil
      setFact() -- db.json wins when a save is loaded
      if patch and state.settings.enabled then
        local ok, err = pcall(function() patch:scan() end)
        if not ok then log("Session scan failed: " .. tostring(err)) end
      end
    else
      local ok, value = pcall(function() return qs:GetFactStr(FACT) end)
      if ok and (value == 0 or value == 1) then
        if lastFact ~= nil and value ~= lastFact then
          local newEnabled = value == 0
          if state.settings.enabled ~= newEnabled then
            setEnabled(newEnabled)
          end
        end
        lastFact = value
      end
    end
  end

  -- Refresh at a low frequency for cyberware registered by other mods after
  -- initialization. Costly record enumeration is NOT done every frame.
  scanClock = scanClock + delta
  if scanClock >= 30 then
    scanClock = 0
    if patch and state.settings.enabled then
      local ok, err = pcall(function() patch:scan() end)
      if not ok then log("Refresh scan failed: " .. tostring(err)) end
    end
  end
end)

registerForEvent("onShutdown", function()
  -- No TweakDB persistence between full game runs; don't modify live
  -- records during the shutdown phase. Preserve only the on/off preference.
  save()
end)
