-- NETWORM diagnostics for Cyber Engine Tweaks 1.38.x
-- Optional, no gameplay changes, no native resource or entity lifecycle hooks.
-- Advanced Crash Reporter 1.6 already captures CET scripting.log.
-- Output: [NetDeck/DIAG] ... in bin/x64/plugins/cyber_engine_tweaks/scripting.log
-- Remove this folder after diagnosing crashes.
local PREFIX = "[NetDeck/DIAG v0.6.5]"
local seconds = 0
local waves = 0
local infections = 0
local observerErrors = 0

local function log(event, details)
  print(PREFIX .. " " .. event .. (details and (" | " .. tostring(details)) or ""))
end

local function targetInfo(target)
  if target == nil then return "nil" end
  local ok, id = pcall(function() return target:GetEntityID() end)
  if ok and id ~= nil then return tostring(id) end
  return "<target>"
end

-- CET observes our own REDscript wave/HUD methods ONLY. It does not wrap the
-- game's globally invoked NPC/vehicle OnGameAttached / OnDetach methods.
-- If CET cannot observe an added method, log that and keep the mod running.
local function observe(className, methodName, callback)
  local ok, err = pcall(function()
    ObserveAfter(className, methodName, function(...)
      local worked, failure = pcall(callback, ...)
      if not worked and observerErrors < 5 then
        observerErrors = observerErrors + 1
        log("observer_error", methodName .. ": " .. tostring(failure))
      end
    end)
  end)
  if ok then
    log("observer_ready", className .. "." .. methodName)
  else
    log("observer_unavailable", className .. "." .. methodName .. ": " .. tostring(err))
  end
end

-- Observe() runs BEFORE the NetDeck method and therefore leaves evidence
-- even if the engine crashes *inside* the upload or HUD method.
local function observeBefore(className, methodName, callback)
  local ok, err = pcall(function()
    Observe(className, methodName, function(...)
      local worked, failure = pcall(callback, ...)
      if not worked and observerErrors < 5 then
        observerErrors = observerErrors + 1
        log("observer_error", methodName .. ": " .. tostring(failure))
      end
    end)
  end)
  if ok then
    log("observer_ready_before", className .. "." .. methodName)
  else
    log("observer_unavailable_before", className .. "." .. methodName .. ": " .. tostring(err))
  end
end

registerForEvent("onInit", function()
  log("session_start", "CET logging active; this file only observes NetDeck methods")
  log("startup_probe", "if no wave_request appears, the crash was not preceded by a detected NETWORM upload")

  observeBefore("PlayerPuppet", "NetDeckWaveStart", function(_, target, fromDevice)
    log("wave_request", "target=" .. targetInfo(target)
      .. " fromDevice=" .. tostring(fromDevice))
  end)
  observeBefore("PlayerPuppet", "NetDeckHudStart", function()
    log("hud_request", "wave=" .. waves)
  end)

  observe("PlayerPuppet", "NetDeckWaveStart", function(_, target, fromDevice)
    waves = waves + 1
    infections = 0
    log("wave_start", "wave=" .. waves .. " target=" .. targetInfo(target)
      .. " fromDevice=" .. tostring(fromDevice))
  end)

  observe("PlayerPuppet", "NetDeckWaveInfect", function(_, target, depth)
    infections = infections + 1
    if infections <= 16 then
      log("wave_infect", "wave=" .. waves .. " #=" .. infections ..
        " depth=" .. tostring(depth) .. " target=" .. targetInfo(target))
    elseif infections == 17 then
      log("wave_infect", "further infected-target messages suppressed for this wave")
    end
  end)

  observe("PlayerPuppet", "NetDeckWaveFinish", function()
    log("wave_finish", "wave=" .. waves .. " infections_logged=" .. math.min(infections, 16))
  end)
  observe("PlayerPuppet", "NetDeckHudStart", function()
    log("hud_start", "wave=" .. waves)
  end)
  observe("PlayerPuppet", "NetDeckHudFinish", function()
    log("hud_finish", "wave=" .. waves)
  end)
end)

-- Safe heartbeat without scanning NPCs, vehicles, game objects, files, or UI.
-- It distinguishes a loading/streaming crash from an upload-driven crash.
registerForEvent("onUpdate", function(delta)
  if type(delta) ~= "number" or delta <= 0 then return end
  seconds = seconds + delta
  if seconds >= 15 then
    seconds = seconds - 15
    log("heartbeat", "waves=" .. waves .. " observed_infections=" .. infections)
  end
end)

registerForEvent("onShutdown", function()
  log("session_shutdown", "waves=" .. waves)
end)
